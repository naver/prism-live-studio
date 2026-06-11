#include "libresource.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>

#include <qurl.h>
#include <qdir.h>
#include <liblog.h>

#if defined(Q_OS_WIN)
#include <Windows.h>
#include "WindowsUnzip/unzip.h"
#include "WindowsUnzip/zip.h"
#endif

namespace pls {
namespace rsm {

constexpr auto LIBRESOURCE_MODULE = "libresource";
const QString CATEGORY_DOT_JSON = "category.json";

const QString DOWNLOAD_ALL = "downloadAll";
const QString DOWNLOAD_CATEGORY = "downloadCategory/";
const QString DOWNLOAD_CATEGORY_GROUP = "downloadCategoryGroup/";
const QString DOWNLOAD_CATEGORY_ITEM = "downloadCategoryItem/";

const QByteArray PRISM_PC_HMAC_KEY_DEV = "";
const QByteArray PRISM_PC_HMAC_KEY_REAL = "";

const QString PRISM_HOST_DEV = "";
const QString PRISM_HOST_REAL = "";

const QString PRISM_SYNC_GATEWAY_DEV = "";
const QString PRISM_SYNC_GATEWAY_REAL = "";

#define CB_Get_UrlAndHowSave_FilePath [](const UrlAndHowSave &v) { return v.savedFilePath(); }
#define CB_IsEqual_UrlAndHowSave_Names [names](const UrlAndHowSave &v) { return pls_is_equal(v.names(), names); }
#define optionalSetValueRet(variable, newValue) \
	variable = newValue;                    \
	return *this
#define optionalSetValueRet2(variable1, newValue1, variable2, newValue2) \
	variable1 = newValue1;                                           \
	variable2 = newValue2;                                           \
	return *this
#define optionalGetValueChkRet(variable, defaultValue) \
	if (variable)                                  \
		return variable.value();               \
	return defaultValue
#define mapAddValue(variable, name, value) \
	variable.insert(name, value);      \
	return *this
#define CallMgrMethod(return, method, ...) \
	auto mgr = getResourceManager();   \
	return mgr->method(this->categoryId(mgr) __VA_ARGS__)
#define PRISM_PC_HMAC_KEY (pls_prism_is_dev() ? PRISM_PC_HMAC_KEY_DEV : PRISM_PC_HMAC_KEY_REAL)
#define PRISM_HOST (pls_prism_is_dev() ? PRISM_HOST_DEV : PRISM_HOST_REAL)
#define PRISM_SYNC_GATEWAY (pls_prism_is_dev() ? PRISM_SYNC_GATEWAY_DEV : PRISM_SYNC_GATEWAY_REAL)
#define CATEGORY_URL (PRISM_HOST + PRISM_SYNC_GATEWAY + QStringLiteral(""))

State loadState(State state)
{
	return (state != State::Downloading) ? state : State::Failed;
}
bool setState(State &state1, State &state2, State value, bool retval)
{
	state2 = state2 = value;
	return retval;
}
std::vector<std::pair<const char *, const char *>> &neloFields(std::vector<std::pair<const char *, const char *>> &fields)
{
	fields.push_back({"type", "RSMError"});
	return fields;
}
std::vector<std::pair<const char *, const char *>> neloFields(std::vector<std::pair<const char *, const char *>> &&fields)
{
	std::vector<std::pair<const char *, const char *>> result = std::move(fields);
	return neloFields(result);
}

struct UrlAndHowSaveImpl {
	bool m_noCache = false;
	bool m_forceDownload = false;
	bool m_needDecompress = false;
	bool m_defaultFileIsEncrypted = false;
	bool m_encryptJson = false;
	int m_timeout = 30000; // 30s
	State m_emptyUrlState = State::Failed;
	QString m_keyPrefix;
	std::optional<QStringList> m_names = std::nullopt;
	std::optional<QUrl> m_url = std::nullopt;
	std::optional<QByteArray> m_hmacKey = std::nullopt;
	std::optional<QString> m_saveDir = std::nullopt;
	std::optional<FileName> m_fileName = std::nullopt;
	std::optional<QString> m_specFileName = std::nullopt;
	std::optional<QString> m_filePath = std::nullopt;
	std::optional<QString> m_defaultFilePath = std::nullopt;
	std::optional<QString> m_savedFilePath = std::nullopt;
	std::optional<std::chrono::seconds> m_expired = std::nullopt;
	QVariantHash m_customAttrs;
	QMap<QByteArray, QByteArray> m_neloFields;
	FnCheck m_check = nullptr;
	FnDecompress m_decompress = nullptr;
	FnDone m_done = nullptr;

	QString key() const
	{
		if (m_names.has_value())
			return m_keyPrefix + m_names.value().join('/');
		return m_keyPrefix;
	}
	bool hasUrl() const { return m_url ? m_url.value().isValid() : false; }
	QString url() const { return m_url ? m_url.value().toString() : QString(); }
	QString saveDir() const
	{
		if (m_saveDir.has_value())
			return m_saveDir.value();
		return rsm::getAppDataPath(QStringLiteral("cache"));
	}
	QString namesDir() const
	{
		if (!m_names)
			return {};
		else if (auto dir = m_names.value().join('/'); !dir.isEmpty())
			return '/' + dir;
		return {};
	}
	std::optional<QString> fileName() const
	{
		switch (m_fileName.value_or(FileName::Uuid)) {
		case FileName::Uuid:
		default:
			return std::nullopt;
		case FileName::FromUrl:
			if (m_url.has_value())
				return m_url.value().fileName();
			return std::nullopt;
		case FileName::Spec:
			if (m_specFileName.has_value())
				return m_specFileName.value();
			return std::nullopt;
		}
	}
	qint64 expired() const
	{
		if (m_expired)
			return m_expired.value().count();
		return -1;
	}
	std::vector<std::pair<const char *, const char *>> nfields(const std::vector<std::pair<QByteArray, QByteArray>> &extra = {}) const
	{
		std::vector<std::pair<const char *, const char *>> result;
		for (auto iter = m_neloFields.begin(), end = m_neloFields.end(); iter != end; ++iter)
			result.push_back({iter.key().constData(), iter.value().constData()});
		pls_to_fields(result, extra);
		return neloFields(result);
	}

	bool check(UrlAndHowSave urlAndHowSave, const QString &filePath) const { return pls_invoke_safe(true, m_check, urlAndHowSave, filePath); }
	bool decompress(UrlAndHowSave urlAndHowSave) const
	{
		if (!m_needDecompress || !m_decompress || !m_savedFilePath)
			return false;
		else if (auto fileNameUtf8 = pls_get_path_file_name(m_savedFilePath.value()).toUtf8(); m_decompress(urlAndHowSave, m_savedFilePath.value())) {
			PLS_INFO(LIBRESOURCE_MODULE, "decompress file %s ok.", fileNameUtf8.constData());
			return true;
		} else {
			PLS_LOGEX(PLS_LOG_ERROR, LIBRESOURCE_MODULE, nfields({{"fileName", fileNameUtf8}}), "decompress file %s failed.", fileNameUtf8.constData());
			return false;
		}
	}
	void done(UrlAndHowSave urlAndHowSave, bool ok, const QString &filePath, PathFrom pathFrom, bool decompressOk) const
	{
		pls_invoke_safe(m_done, urlAndHowSave, ok, filePath, pathFrom, decompressOk);
	}
};

UrlAndHowSave::UrlAndHowSave() : UrlAndHowSave(std::make_shared<UrlAndHowSaveImpl>()) {}
UrlAndHowSave::UrlAndHowSave(UrlAndHowSaveImplPtr urlAndHowSaveImpl) : m_impl(urlAndHowSaveImpl) {}

UrlAndHowSaveImpl *UrlAndHowSave::operator->() const
{
	return m_impl.operator->();
}

QString UrlAndHowSave::keyPrefix() const
{
	return m_impl->m_keyPrefix;
}
UrlAndHowSave &UrlAndHowSave::keyPrefix(const QString &keyPrefix)
{
	optionalSetValueRet(m_impl->m_keyPrefix, keyPrefix);
}

QStringList UrlAndHowSave::names() const
{
	optionalGetValueChkRet(m_impl->m_names, {});
}
UrlAndHowSave &UrlAndHowSave::names(const QStringList &names)
{
	optionalSetValueRet(m_impl->m_names, names);
}

QUrl UrlAndHowSave::url() const
{
	optionalGetValueChkRet(m_impl->m_url, {});
}
UrlAndHowSave &UrlAndHowSave::url(const QUrl &url)
{
	optionalSetValueRet(m_impl->m_url, url);
}

QByteArray UrlAndHowSave::hmacKey() const
{
	optionalGetValueChkRet(m_impl->m_hmacKey, {});
}
UrlAndHowSave &UrlAndHowSave::hmacKey(const QByteArray &hmacKey)
{
	optionalSetValueRet(m_impl->m_hmacKey, hmacKey);
}

State UrlAndHowSave::emptyUrlState() const
{
	return m_impl->m_emptyUrlState;
}
UrlAndHowSave &UrlAndHowSave::emptyUrlState(State emptyUrlState)
{
	optionalSetValueRet(m_impl->m_emptyUrlState, emptyUrlState);
}

bool UrlAndHowSave::noCache() const
{
	return m_impl->m_noCache;
}
UrlAndHowSave &UrlAndHowSave::noCache(bool noCache)
{
	optionalSetValueRet(m_impl->m_noCache, noCache);
}

bool UrlAndHowSave::forceDownload() const
{
	return m_impl->m_forceDownload;
}
UrlAndHowSave &UrlAndHowSave::forceDownload(bool forceDownload)
{
	optionalSetValueRet(m_impl->m_forceDownload, forceDownload);
}

QString UrlAndHowSave::saveDir() const
{
	optionalGetValueChkRet(m_impl->m_saveDir, {});
}
UrlAndHowSave &UrlAndHowSave::saveDir(const QString &saveDir)
{
	optionalSetValueRet(m_impl->m_saveDir, saveDir);
}

QString UrlAndHowSave::fileName() const
{
	optionalGetValueChkRet(m_impl->m_specFileName, {});
}
UrlAndHowSave &UrlAndHowSave::fileName(const QString &fileName)
{
	optionalSetValueRet2(m_impl->m_fileName, FileName::Spec, m_impl->m_specFileName, fileName);
}
UrlAndHowSave &UrlAndHowSave::fileName(FileName fileName)
{
	optionalSetValueRet2(m_impl->m_fileName, fileName, m_impl->m_specFileName, std::nullopt);
}

QString UrlAndHowSave::filePath() const
{
	optionalGetValueChkRet(m_impl->m_filePath, {});
}
UrlAndHowSave &UrlAndHowSave::filePath(const QString &filePath)
{
	optionalSetValueRet(m_impl->m_filePath, filePath);
}

QString UrlAndHowSave::defaultFilePath() const
{
	optionalGetValueChkRet(m_impl->m_defaultFilePath, {});
}
UrlAndHowSave &UrlAndHowSave::defaultFilePath(const QString &defaultFilePath, bool encrypted)
{
	optionalSetValueRet2(m_impl->m_defaultFilePath, defaultFilePath, m_impl->m_defaultFileIsEncrypted, encrypted);
}

bool UrlAndHowSave::encryptJson() const
{
	return m_impl->m_encryptJson;
}
UrlAndHowSave &UrlAndHowSave::encryptJson(bool encryptJson)
{
	optionalSetValueRet(m_impl->m_encryptJson, encryptJson);
}

QString UrlAndHowSave::savedFilePath() const
{
	optionalGetValueChkRet(m_impl->m_savedFilePath, {});
}

bool UrlAndHowSave::needDecompress() const
{
	return m_impl->m_needDecompress;
}
UrlAndHowSave &UrlAndHowSave::needDecompress(bool needDecompress)
{
	optionalSetValueRet(m_impl->m_needDecompress, needDecompress);
}

int UrlAndHowSave::timeout() const
{
	return m_impl->m_timeout;
}
UrlAndHowSave &UrlAndHowSave::timeout(int timeout)
{
	optionalSetValueRet(m_impl->m_timeout, timeout);
}

// seconds default not expired
UrlAndHowSave &UrlAndHowSave::expired(std::chrono::seconds expired)
{
	optionalSetValueRet(m_impl->m_expired, expired);
}
UrlAndHowSave &UrlAndHowSave::expired(std::chrono::minutes expired)
{
	optionalSetValueRet(m_impl->m_expired, std::chrono::duration_cast<std::chrono::seconds>(expired));
}
UrlAndHowSave &UrlAndHowSave::expired(std::chrono::hours expired)
{
	optionalSetValueRet(m_impl->m_expired, std::chrono::duration_cast<std::chrono::seconds>(expired));
}

QVariant UrlAndHowSave::customAttr(const QString &name, const QVariant &defval) const
{
	return m_impl->m_customAttrs.value(name, defval);
}
UrlAndHowSave &UrlAndHowSave::customAttr(const QString &name, const QVariant &value)
{
	mapAddValue(m_impl->m_customAttrs, name, value);
}
QByteArray UrlAndHowSave::neloField(const QByteArray &name) const
{
	return m_impl->m_neloFields.value(name);
}
UrlAndHowSave &UrlAndHowSave::neloField(const QByteArray &name, const QByteArray &value)
{
	mapAddValue(m_impl->m_neloFields, name, value);
}

UrlAndHowSave &UrlAndHowSave::check(FnCheck &&check)
{
	optionalSetValueRet(m_impl->m_check, std::move(check));
}
UrlAndHowSave &UrlAndHowSave::decompress(FnDecompress &&decompress)
{
	optionalSetValueRet(m_impl->m_decompress, std::move(decompress));
}
UrlAndHowSave &UrlAndHowSave::done(FnDone &&done)
{
	optionalSetValueRet(m_impl->m_done, std::move(done));
}

struct CategoryImpl {
	int m_version;
	QString m_categoryId;
	QStringList m_groups;
	QStringList m_items;
	QByteArray m_categoryIdUtf8;
	QJsonObject m_json;

	std::map<QString, GroupImplPtr> m_allGroups;
	std::map<QString, ItemImplPtr> m_allItems;

	explicit CategoryImpl(int version, const QString &categoryId, const QJsonObject &json) : m_version(version), m_categoryId(categoryId), m_categoryIdUtf8(categoryId.toUtf8()), m_json(json) {}

	std::list<Group> groups() const { return groups(m_groups); }
	std::list<Group> groups(const QStringList &groupIds) const
	{
		return pls_map<StdList>(groupIds, [this](const QString &groupId) -> Group { return findGroup(groupId); });
	}
	std::list<Item> items() const { return items(m_items); }
	std::list<Item> items(const QStringList &itemIds) const
	{
		return pls_map<StdList>(itemIds, [this](const QString &itemId) -> Item { return findItem(itemId); });
	}

	GroupImplPtr findGroup(const QString &groupId) const { return pls_get_value(m_allGroups, groupId, nullptr); }
	ItemImplPtr findItem(const QString &itemId) const { return pls_get_value(m_allItems, itemId, nullptr); }
	GroupImplPtr findGroup(const UniqueId &uniqueId) const
	{
		for (const auto &[groupId, groupImpl] : m_allGroups)
			if (Group group = groupImpl; group.isUniqueId(uniqueId))
				return groupImpl;
		return nullptr;
	}
	ItemImplPtr findItem(const UniqueId &uniqueId) const
	{
		for (const auto &[itemId, itemImpl] : m_allItems)
			if (Item item = itemImpl; item.isUniqueId(uniqueId))
				return itemImpl;
		return nullptr;
	}
	void insertGroup(qsizetype pos, const QString &groupId)
	{
		if (m_groups.contains(groupId))
			return;
		else if (pos >= 0 && pos < m_groups.size())
			m_groups.insert(pos, groupId);
		else
			m_groups.append(groupId);
	}
};

Category::Category() : Category(std::make_shared<CategoryImpl>(0, QString(), QJsonObject())) {}

Category::Category(std::nullptr_t) : Category(std::make_shared<CategoryImpl>(0, QString(), QJsonObject())) {}

////////////////////////////////////////////////////////////////////////////////
// struct Category
Category::Category(CategoryImplPtr categoryImpl) : m_impl(categoryImpl) {}

Category::operator bool() const
{
	return m_impl.operator bool();
}
Category &Category::operator=(std::nullptr_t)
{
	optionalSetValueRet(m_impl, nullptr);
}
Category &Category::operator=(CategoryImplPtr categoryImpl)
{
	optionalSetValueRet(m_impl, categoryImpl);
}
Category &Category::operator=(Category category)
{
	optionalSetValueRet(m_impl, category.m_impl);
}
CategoryImpl *Category::operator->() const
{
	return m_impl.operator->();
}

const QJsonObject& Category::json() const
{
	return m_impl->m_json;
}
int Category::version() const
{
	return m_impl->m_version;
}
QString Category::categoryId() const
{
	return m_impl->m_categoryId;
}
std::list<Group> Category::groups() const
{
	return m_impl->groups();
}
std::list<Item> Category::items() const
{
	return m_impl->items();
}

Group Category::getGroup(const QString &groupId) const
{
	return m_impl->findGroup(groupId);
}
Group Category::getGroup(const UniqueId &uniqueId) const
{
	return m_impl->findGroup(uniqueId);
}
Item Category::getItem(const QString &itemId) const
{
	return m_impl->findItem(itemId);
}
Item Category::getItem(const UniqueId &uniqueId) const
{
	return m_impl->findItem(uniqueId);
}

struct GroupImpl {
	bool m_custom = false;
	bool m_archive = false;
	QString m_uniqueId;
	QString m_groupId;
	QString m_homeDir;
	QVariantHash m_attrs;
	QVariantHash m_customAttrs;
	std::list<UrlAndHowSave> m_urlAndHowSaves;
	QStringList m_items;
	CategoryImplWeakPtr m_category;
	QByteArray m_groupIdUtf8;

	GroupImpl(const QString &groupId, const QVariantHash &attrs, bool custom = false) : m_custom(custom), m_groupId(groupId), m_attrs(attrs), m_groupIdUtf8(groupId.toUtf8()) {}

	const QString &id() const { return m_groupId; }
	const QByteArray &idUtf8() const { return m_groupIdUtf8; }
	std::optional<QUrl> url(const QStringList &names) const { return pls_get_attr(m_attrs, names, PLS_GET_ATTR_STRING_CB(QUrl)); }

	QVariant attr(const QString &name, const QVariant &defval) const { return m_attrs.value(name, defval); }
	QVariant attr(const QStringList &names, const QVariant &defval) const { return pls_get_attr(m_attrs, names).value_or(defval); }

	void addItem(const QString &itemId)
	{
		if (!m_items.contains(itemId))
			m_items.push_back(itemId);
	}
	void insertItem(qsizetype pos, const QString &itemId)
	{
		if (m_items.contains(itemId))
			return;
		else if (pos >= 0 && pos < m_items.size())
			m_items.insert(pos, itemId);
		else
			m_items.append(itemId);
	}
};

Group::Group() : Group(std::make_shared<GroupImpl>(QString(), QVariantHash())) {}

Group::Group(std::nullptr_t) : Group(std::make_shared<GroupImpl>(QString(), QVariantHash())) {}

Group::Group(GroupImplPtr groupImpl) : m_impl(groupImpl) {}

Group::operator bool() const
{
	return m_impl.operator bool();
}
Group &Group::operator=(std::nullptr_t)
{
	optionalSetValueRet(m_impl, nullptr);
}
Group &Group::operator=(GroupImplPtr groupImpl)
{
	optionalSetValueRet(m_impl, groupImpl);
}
Group &Group::operator=(Group group)
{
	optionalSetValueRet(m_impl, group.m_impl);
}
GroupImpl *Group::operator->() const
{
	return m_impl.operator->();
}

QString Group::groupId() const
{
	return m_impl->m_groupId;
}

bool Group::isCustom() const
{
	return m_impl->m_custom;
}
State Group::state() const
{
	if (auto category = m_impl->m_category.lock(); category)
		return getResourceManager()->getGroupState(category->m_categoryId, groupId());
	return State::Initialized;
}

QString Group::dir() const
{
	return m_impl->m_homeDir;
}
QString Group::filePath(const QString &subpath) const
{
	if (subpath.isEmpty())
		return m_impl->m_homeDir;
	else if (auto front = subpath.front(); front == '/' || front == '\\')
		return m_impl->m_homeDir + subpath;
	else
		return m_impl->m_homeDir + '/' + subpath;
}

int Group::itemCount() const
{
	return (int)m_impl->m_items.count();
}
Item Group::item(const QString &itemId) const
{
	if (!m_impl->m_items.contains(itemId))
		return {};
	else if (auto category = m_impl->m_category.lock(); category)
		return category->findItem(itemId);
	return {};
}
std::list<Item> Group::items() const
{
	if (auto category = m_impl->m_category.lock(); category)
		return category->items(m_impl->m_items);
	return {};
}

QVariant Group::attr(const QString &name, const QVariant &defval) const
{
	return m_impl->attr(name, defval);
}
QVariant Group::attr(const QStringList &names, const QVariant &defval) const
{
	return m_impl->attr(names, defval);
}

bool Group::isUniqueId(const UniqueId &uniqueId) const
{
	return m_impl->m_uniqueId == uniqueId.m_id;
}
UniqueId Group::uniqueId() const
{
	return UniqueId(m_impl->m_uniqueId);
}
void Group::setUniqueId(const QString &uniqueId)
{
	m_impl->m_uniqueId = uniqueId;
}
void Group::setUniqueId(const UniqueId &uniqueId)
{
	setUniqueId(uniqueId.m_id);
}

QVariant Group::customAttr(const QString &name, const QVariant &defval) const
{
	return m_impl->m_customAttrs.value(name, defval);
}
void Group::customAttr(const QString &name, const QVariant &value)
{
	m_impl->m_customAttrs.insert(name, value);
}

std::list<UrlAndHowSave> Group::urlAndHowSaves() const
{
	return m_impl->m_urlAndHowSaves;
}
size_t Group::fileCount() const
{
	return m_impl->m_urlAndHowSaves.size();
}
QString Group::file(size_t index) const
{
	return pls_get_value<QString>(m_impl->m_urlAndHowSaves, index, CB_Get_UrlAndHowSave_FilePath);
}
QString Group::file(const QString &name) const
{
	return file(name.split('/', Qt::SkipEmptyParts));
}
QString Group::file(const QStringList &names) const
{
	return pls_get_value<QString>(m_impl->m_urlAndHowSaves, CB_IsEqual_UrlAndHowSave_Names, CB_Get_UrlAndHowSave_FilePath);
}

struct ItemImpl {
	bool m_custom = false;
	bool m_archive = false;
	int m_version = 0;
	QString m_uniqueId;
	QString m_itemId;
	QString m_homeDir;
	QVariantHash m_attrs;
	QVariantHash m_customAttrs;
	std::list<UrlAndHowSave> m_urlAndHowSaves;
	QStringList m_groups;
	CategoryImplWeakPtr m_category;
	QByteArray m_itemIdUtf8;

	ItemImpl(int version, const QString &itemId, const QVariantHash &attrs, bool custom = false)
		: m_custom(custom), m_version(version), m_itemId(itemId), m_attrs(attrs), m_itemIdUtf8(itemId.toUtf8())
	{
	}

	const QString &id() const { return m_itemId; }
	const QByteArray &idUtf8() const { return m_itemIdUtf8; }
	std::optional<QUrl> url(const QStringList &names) const { return pls_get_attr(m_attrs, names, PLS_GET_ATTR_STRING_CB(QUrl)); }

	QVariant attr(const QString &name, const QVariant &defval) const { return m_attrs.value(name, defval); }
	QVariant attr(const QStringList &names, const QVariant &defval) const { return pls_get_attr(m_attrs, names).value_or(defval); }
};

Item::Item() : Item(std::make_shared<ItemImpl>(0, QString(), QVariantHash())) {}

Item::Item(std::nullptr_t) : Item(std::make_shared<ItemImpl>(0, QString(), QVariantHash())) {}

Item::Item(ItemImplPtr itemImpl) : m_impl(itemImpl) {}

Item::operator bool() const
{
	if (m_impl)
		return true;
	return false;
}
bool Item::operator==(Item item) const
{
	return m_impl == item.m_impl;
}
Item &Item::operator=(std::nullptr_t)
{
	optionalSetValueRet(m_impl, nullptr);
}
Item &Item::operator=(ItemImplPtr itemImpl)
{
	optionalSetValueRet(m_impl, itemImpl);
}
Item &Item::operator=(Item item)
{
	optionalSetValueRet(m_impl, item.m_impl);
}
ItemImpl *Item::operator->() const
{
	return m_impl.operator->();
}

QString Item::itemId() const
{
	return m_impl->m_itemId;
}
int Item::version() const
{
	return m_impl->m_version;
}

bool Item::isCustom() const
{
	return m_impl->m_custom;
}
State Item::state() const
{
	if (auto category = m_impl->m_category.lock(); category)
		return getResourceManager()->getItemState(category->m_categoryId, itemId());
	return State::Initialized;
}

QString Item::dir() const
{
	return m_impl->m_homeDir;
}
QString Item::filePath(const QString &subpath) const
{
	if (subpath.isEmpty())
		return m_impl->m_homeDir;
	else if (auto front = subpath.front(); front == '/' || front == '\\')
		return m_impl->m_homeDir + subpath;
	else
		return m_impl->m_homeDir + '/' + subpath;
}

std::list<Group> Item::groups() const
{
	if (auto category = m_impl->m_category.lock(); category)
		return category->groups(m_impl->m_groups);
	return {};
}

QVariant Item::attr(const QString &name, const QVariant &defval) const
{
	return m_impl->attr(name, defval);
}
QVariant Item::attr(const QStringList &names, const QVariant &defval) const
{
	return m_impl->attr(names, defval);
}

bool Item::isUniqueId(const UniqueId &uniqueId) const
{
	return m_impl->m_uniqueId == uniqueId.m_id;
}
UniqueId Item::uniqueId() const
{
	return UniqueId(m_impl->m_uniqueId);
}
void Item::setUniqueId(const QString &uniqueId)
{
	m_impl->m_uniqueId = uniqueId;
}
void Item::setUniqueId(const UniqueId &uniqueId)
{
	setUniqueId(uniqueId.m_id);
}

QVariant Item::customAttr(const QString &name, const QVariant &defval) const
{
	return m_impl->m_customAttrs.value(name, defval);
}
void Item::customAttr(const QString &name, const QVariant &value)
{
	m_impl->m_customAttrs.insert(name, value);
}

std::list<UrlAndHowSave> Item::urlAndHowSaves() const
{
	return m_impl->m_urlAndHowSaves;
}

size_t Item::fileCount() const
{
	return m_impl->m_urlAndHowSaves.size();
}
QString Item::file(size_t index) const
{
	return pls_get_value<QString>(m_impl->m_urlAndHowSaves, index, CB_Get_UrlAndHowSave_FilePath);
}
QString Item::file(const QString &name) const
{
	return file(name.split('/', Qt::SkipEmptyParts));
}
QString Item::file(const QStringList &names) const
{
	return pls_get_value<QString>(m_impl->m_urlAndHowSaves, CB_IsEqual_UrlAndHowSave_Names, CB_Get_UrlAndHowSave_FilePath);
}

const char *ICategory::moduleName() const
{
	CallMgrMethod(return, moduleName, .toUtf8());
}

void ICategory::download() const // download category resource
{
	CallMgrMethod(, downloadCategory);
}
void ICategory::downloadGroup(const QString &groupId) const // download category group resource
{
	CallMgrMethod(, downloadCategoryGroup, , groupId);
}
void ICategory::downloadItem(const QString &itemId) const // download category item resource
{
	CallMgrMethod(, downloadCategoryItem, , itemId);
}

State ICategory::getJsonState() const
{
	CallMgrMethod(return, getJsonState);
}
State ICategory::getState() const
{
	CallMgrMethod(return, getCategoryState);
}
State ICategory::getGroupState(const QString &groupId) const
{
	CallMgrMethod(return, getGroupState, , groupId);
}
State ICategory::getItemState(const QString &itemId) const
{
	CallMgrMethod(return, getItemState, , itemId);
}
Category ICategory::getCategory() const
{
	CallMgrMethod(return, getCategory);
}
void ICategory::getCategory(std::function<void(Category category)> &&result) const
{
	CallMgrMethod(, getCategory, , std::move(result));
}
void ICategory::getCategory(pls::QObjectPtr<QObject> receiver, std::function<void(Category category)> &&result) const
{
	CallMgrMethod(, getCategory, , receiver, std::move(result));
}

std::list<Group> ICategory::getGroups() const
{
	CallMgrMethod(return, getGroups);
}
void ICategory::getGroups(std::function<void(const std::list<Group> &groups)> &&result) const
{
	CallMgrMethod(, getGroups, , std::move(result));
}
void ICategory::getGroups(pls::QObjectPtr<QObject> receiver, std::function<void(const std::list<Group> &groups)> &&result) const
{
	CallMgrMethod(, getGroups, , receiver, std::move(result));
}
std::list<Item> ICategory::getItems() const
{
	CallMgrMethod(return, getItems);
}
void ICategory::getItems(std::function<void(const std::list<Item> &items)> &&result) const
{
	CallMgrMethod(, getItems, , std::move(result));
}
void ICategory::getItems(pls::QObjectPtr<QObject> receiver, std::function<void(const std::list<Item> &items)> &&result) const
{
	CallMgrMethod(, getItems, , receiver, std::move(result));
}

Group ICategory::getGroup(const QString &groupId) const
{
	CallMgrMethod(return, getGroup, , groupId);
}
void ICategory::getGroup(const QString &groupId, std::function<void(Group group)> &&result) const
{
	CallMgrMethod(, getGroup, , groupId, std::move(result));
}
void ICategory::getGroup(const QString &groupId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group)> &&result) const
{
	CallMgrMethod(, getGroup, , groupId, receiver, std::move(result));
}

std::pair<Group, Item> ICategory::getGroupItem(const QString &groupId, const QString &itemId) const
{
	CallMgrMethod(return, getGroupItem, , groupId, itemId);
}
void ICategory::getGroupItem(const QString &groupId, const QString &itemId, std::function<void(Group group, Item item)> &&result) const
{
	CallMgrMethod(, getGroupItem, , groupId, itemId, std::move(result));
}
void ICategory::getGroupItem(const QString &groupId, const QString &itemId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) const
{
	CallMgrMethod(, getGroupItem, , groupId, itemId, receiver, std::move(result));
}
Item ICategory::getItem(const QString &itemId) const
{
	CallMgrMethod(return, getItem, , itemId);
}
void ICategory::getItem(const QString &itemId, std::function<void(Item item)> &&result) const
{
	CallMgrMethod(, getItem, , itemId, std::move(result));
}
void ICategory::getItem(const QString &itemId, pls::QObjectPtr<QObject> receiver, std::function<void(Item item)> &&result) const
{
	CallMgrMethod(, getItem, , itemId, receiver, std::move(result));
}

Group ICategory::getGroup(const UniqueId &uniqueId) const
{
	CallMgrMethod(return, getGroup, , uniqueId);
}
void ICategory::getGroup(const UniqueId &uniqueId, std::function<void(Group group)> &&result) const
{
	CallMgrMethod(, getGroup, , uniqueId, std::move(result));
}
void ICategory::getGroup(const UniqueId &uniqueId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group)> &&result) const
{
	CallMgrMethod(, getGroup, , uniqueId, receiver, std::move(result));
}

Item ICategory::getItem(const UniqueId &uniqueId) const
{
	CallMgrMethod(return, getItem, , uniqueId);
}
void ICategory::getItem(const UniqueId &uniqueId, std::function<void(Item item)> &&result) const
{
	CallMgrMethod(, getItem, , uniqueId, std::move(result));
}
void ICategory::getItem(const UniqueId &uniqueId, pls::QObjectPtr<QObject> receiver, std::function<void(Item item)> &&result) const
{
	CallMgrMethod(, getItem, , uniqueId, receiver, std::move(result));
}

std::pair<Category, bool> ICategory::check() const
{
	CallMgrMethod(return, checkCategory);
}
void ICategory::check(std::function<void(Category category, bool ok)> &&result) const
{
	CallMgrMethod(, checkCategory, , std::move(result));
}
void ICategory::check(pls::QObjectPtr<QObject> receiver, std::function<void(Category category, bool ok)> &&result) const
{
	CallMgrMethod(, checkCategory, , receiver, std::move(result));
}
std::pair<Group, bool> ICategory::checkGroup(const QString &groupId) const
{
	CallMgrMethod(return, checkGroup, , groupId);
}
void ICategory::checkGroup(const QString &groupId, std::function<void(Group group, bool ok)> &&result) const
{
	CallMgrMethod(, checkGroup, , groupId, std::move(result));
}
void ICategory::checkGroup(const QString &groupId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, bool ok)> &&result) const
{
	CallMgrMethod(, checkGroup, , groupId, receiver, std::move(result));
}
std::pair<Item, bool> ICategory::checkItem(const QString &itemId) const
{
	CallMgrMethod(return, checkItem, , itemId);
}
void ICategory::checkItem(const QString &itemId, std::function<void(Item item, bool ok)> &&result) const
{
	CallMgrMethod(, checkItem, , itemId, std::move(result));
}
void ICategory::checkItem(const QString &itemId, pls::QObjectPtr<QObject> receiver, std::function<void(Item item, bool ok)> &&result) const
{
	CallMgrMethod(, checkItem, , itemId, receiver, std::move(result));
}

// custom group/items
std::pair<Group, Item> ICategory::addCustomItem(const QString &groupId, Item item) const
{
	CallMgrMethod(return, addCustomItem, , groupId, item);
}
void ICategory::addCustomItem(const QString &groupId, Item item, std::function<void(Group group, Item item)> &&result) const
{
	CallMgrMethod(, addCustomItem, , groupId, item, std::move(result));
}
void ICategory::addCustomItem(const QString &groupId, Item item, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) const
{
	CallMgrMethod(, addCustomItem, , groupId, item, receiver, std::move(result));
}
std::pair<Group, Item> ICategory::addCustomItem(const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item) const
{
	CallMgrMethod(return, addCustomItem, , groupId, groupAttrs, groupCustomAttrs, item);
}
void ICategory::addCustomItem(const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item, std::function<void(Group group, Item item)> &&result) const
{
	CallMgrMethod(, addCustomItem, , groupId, groupAttrs, groupCustomAttrs, item, std::move(result));
}
void ICategory::addCustomItem(const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item, pls::QObjectPtr<QObject> receiver,
			      std::function<void(Group group, Item item)> &&result) const
{
	CallMgrMethod(, addCustomItem, , groupId, groupAttrs, groupCustomAttrs, item, receiver, std::move(result));
}
std::pair<Group, Item> ICategory::addCustomItem(const QString &groupId, const QString &itemId, const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs) const
{
	CallMgrMethod(return, addCustomItem, , groupId, itemId, itemAttrs, itemCustomAttrs);
}
void ICategory::addCustomItem(const QString &groupId, const QString &itemId, const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs,
			      std::function<void(Group group, Item item)> &&result) const
{
	CallMgrMethod(, addCustomItem, , groupId, itemId, itemAttrs, itemCustomAttrs, std::move(result));
}
void ICategory::addCustomItem(const QString &groupId, const QString &itemId, const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs, pls::QObjectPtr<QObject> receiver,
			      std::function<void(Group group, Item item)> &&result) const
{
	CallMgrMethod(, addCustomItem, , groupId, itemId, itemAttrs, itemCustomAttrs, receiver, std::move(result));
}
std::pair<Group, Item> ICategory::addCustomItem(const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId, const QVariantHash &itemAttrs,
						const QVariantHash &itemCustomAttrs) const
{
	CallMgrMethod(return, addCustomItem, , groupId, groupAttrs, groupCustomAttrs, itemId, itemAttrs, itemCustomAttrs);
}
void ICategory::addCustomItem(const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId, const QVariantHash &itemAttrs,
			      const QVariantHash &itemCustomAttrs, std::function<void(Group group, Item item)> &&result) const
{
	CallMgrMethod(, addCustomItem, , groupId, groupAttrs, groupCustomAttrs, itemId, itemAttrs, itemCustomAttrs, std::move(result));
}
void ICategory::addCustomItem(const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId, const QVariantHash &itemAttrs,
			      const QVariantHash &itemCustomAttrs, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) const
{
	CallMgrMethod(, addCustomItem, , groupId, groupAttrs, groupCustomAttrs, itemId, itemAttrs, itemCustomAttrs, receiver, std::move(result));
}
void ICategory::removeCustomItems(const QString &groupId, bool async) const
{
	CallMgrMethod(, removeCustomItems, , groupId, async);
}
void ICategory::removeCustomItem(const QString &groupId, const QString &itemId, bool async) const
{
	CallMgrMethod(, removeCustomItem, , groupId, itemId, async);
}
void ICategory::removeCustomItem(const QString &itemId, bool async) const
{
	CallMgrMethod(, removeCustomItem, , itemId, async);
}

// recent items
void ICategory::useItem(const QString &group, Item item, bool async) const
{
	CallMgrMethod(, useItem, , group, item, async);
}
std::list<Item> ICategory::getUsedItems(const QString &group) const
{
	CallMgrMethod(return, getUsedItems, , group);
}
void ICategory::getUsedItems(const QString &group, std::function<void(const std::list<Item> &items)> &&result) const
{
	CallMgrMethod(, getUsedItems, , group, std::move(result));
}
void ICategory::getUsedItems(const QString &group, pls::QObjectPtr<QObject> receiver, std::function<void(const std::list<Item> &items)> &&result) const
{
	CallMgrMethod(, getUsedItems, , group, receiver, std::move(result));
}
void ICategory::listenUsedItems(const QString &group, std::function<void(const QString &group, const std::list<Item> &items)> &&result) const
{
	CallMgrMethod(, listenUsedItems, , group, std::move(result));
}
void ICategory::removeUsedItem(const QString &group, Item item, bool async) const
{
	CallMgrMethod(, removeUsedItem, , group, item, async);
}
void ICategory::removeAllUsedItems(const QString &group, bool async) const
{
	CallMgrMethod(, removeAllUsedItems, , group, async);
}
void ICategory::removeAllUsedItems(bool async) const
{
	CallMgrMethod(, removeAllUsedItems, , async);
}

QString ICategory::defaultJsonPath(IResourceManager *mgr) const
{
	return QStringLiteral(":/Configs/resource/DefaultResources/%1.json").arg(categoryId(mgr));
}

size_t ICategory::useMaxCount(IResourceManager *mgr) const
{
	return 30;
}

// json downloaded
void ICategory::jsonDownloaded(IResourceManager *mgr, const DownloadResult &result) {}

bool ICategory::groupNeedLoad(IResourceManager *mgr, Group group) const
{
	return true;
}
bool ICategory::itemNeedLoad(IResourceManager *mgr, Item item) const
{
	return true;
}

// download ok, load json
// download failed, load default json
void ICategory::jsonLoaded(IResourceManager *mgr, Category category) {}

// check manual download
bool ICategory::groupManualDownload(IResourceManager *mgr, Group group) const
{
	return false;
}
bool ICategory::itemManualDownload(IResourceManager *mgr, Item item) const
{
	return false;
}

bool ICategory::groupNeedDownload(IResourceManager *mgr, Group group) const
{
	return true;
}
bool ICategory::checkGroup(IResourceManager *mgr, Group group) const
{
	return true;
}
UniqueId ICategory::getGroupUniqueId(IResourceManager *mgr, Group group) const
{
	return {};
}
QString ICategory::getGroupHomeDir(IResourceManager *mgr, Group group) const
{
	if (auto category = group->m_category.lock(); category)
		return rsm::getAppDataPath(category->m_categoryId + '/' + group->id());
	return {};
}
void ICategory::getGroupDownloadUrlAndHowSaves(IResourceManager *mgr, std::list<UrlAndHowSave> &urlAndHowSaves, Group group) const {}
void ICategory::groupDownloaded(IResourceManager *mgr, Group group, bool ok, const std::list<DownloadResult> &results) const {}
void ICategory::getCustomGroupExtras(qsizetype &pos, bool &archive, IResourceManager *mgr, Group group) const
{
	pos = -1;
	archive = false;
}

bool ICategory::itemNeedDownload(IResourceManager *mgr, Item item) const
{
	return true;
}
bool ICategory::checkItem(IResourceManager *mgr, Item item) const
{
	return true;
}
UniqueId ICategory::getItemUniqueId(IResourceManager *mgr, Item item) const
{
	return {};
}
QString ICategory::getItemHomeDir(IResourceManager *mgr, Item item) const
{
	if (auto category = item->m_category.lock(); category)
		return rsm::getAppDataPath(category->m_categoryId + '/' + item->id());
	return {};
}
void ICategory::getItemDownloadUrlAndHowSaves(IResourceManager *mgr, std::list<UrlAndHowSave> &urlAndHowSaves, Item item) const {}
void ICategory::itemDownloaded(IResourceManager *mgr, Item item, bool ok, const std::list<DownloadResult> &results) const {}
void ICategory::getCustomItemExtras(qsizetype &pos, bool &archive, IResourceManager *mgr, Item item) const
{
	pos = -1;
	archive = false;
}

void ICategory::allDownload(IResourceManager *mgr, bool ok) {}

DownloadResult::DownloadResult(const UrlAndHowSave &urlAndHowSave, State state) //
	: m_urlAndHowSave(urlAndHowSave), m_state(state)
{
}
DownloadResult::DownloadResult(const UrlAndHowSave &urlAndHowSave, State state, PathFrom pathFrom, bool decompressOk)
	: m_urlAndHowSave(urlAndHowSave), m_state(state), m_pathFrom(pathFrom), m_decompressOk(decompressOk)
{
}
DownloadResult::DownloadResult(const UrlAndHowSave &urlAndHowSave, State state, bool timeout, int statusCode)
	: m_urlAndHowSave(urlAndHowSave), m_state(state), m_timeout(timeout), m_statusCode(statusCode)
{
}

static bool readJson(QJsonDocument &doc, bool encrypted, const QString &filePath, QString *error = nullptr)
{
	return encrypted ? pls_decrypt_json(doc, filePath, error) : pls_read_json(doc, filePath, error);
}
static bool readJson(QJsonObject &object, bool encrypted, const QString &filePath, QString *error = nullptr)
{
	if (QJsonDocument doc; !readJson(doc, encrypted, filePath, error)) {
		return false;
	} else if (doc.isObject()) {
		object = doc.object();
		return true;
	}
	return false;
}

bool DownloadResult::json(QJsonDocument &doc, QString *error) const
{
	switch (m_pathFrom) {
	case PathFrom::Downloaded:
		return readJson(doc, m_urlAndHowSave->m_encryptJson, filePath(), error);
	case PathFrom::UseCache:
		if (readJson(doc, m_urlAndHowSave->m_encryptJson, filePath(), error))
			return true;
		else if (m_urlAndHowSave->m_defaultFilePath)
			return readJson(doc, m_urlAndHowSave->m_defaultFileIsEncrypted, m_urlAndHowSave->m_defaultFilePath.value(), error);
		return false;
	case PathFrom::UseDefault:
		return readJson(doc, m_urlAndHowSave->m_defaultFileIsEncrypted, filePath(), error);
	case PathFrom::Invalid:
	default:
		pls_set_value(error, QStringLiteral("invalid path"));
		return false;
	}
}
bool DownloadResult::json(QJsonArray &array, QString *error) const
{
	if (QJsonDocument doc; !json(doc, error)) {
		return false;
	} else if (doc.isArray()) {
		array = doc.array();
		return true;
	}
	return false;
}
bool DownloadResult::json(QVariantList &list, QString *error) const
{
	if (QJsonArray array; json(array, error)) {
		list = array.toVariantList();
		return true;
	}
	return false;
}
bool DownloadResult::json(QJsonObject &object, QString *error) const
{
	if (QJsonDocument doc; !json(doc, error)) {
		return false;
	} else if (doc.isObject()) {
		object = doc.object();
		return true;
	}
	return false;
}
bool DownloadResult::json(QVariantMap &map, QString *error) const
{
	if (QJsonObject object; json(object, error)) {
		map = object.toVariantMap();
		return true;
	}
	return false;
}
bool DownloadResult::json(QVariantHash &hash, QString *error) const
{
	if (QJsonObject object; json(object, error)) {
		hash = object.toVariantHash();
		return true;
	}
	return false;
}

class Downloader : public IDownloader {
public:
	struct _FileState {
		mutable std::mutex m_mutex;
		State m_curState = State::Initialized;
		State m_state = State::Initialized;
		QString m_key;
		QString m_url;
		QString m_filePath;
		qint64 m_fileSize = -1;
		qint64 m_birthTime = -1;
		qint64 m_lastModified = -1;
		qint64 m_expired = -1;
		qint64 m_lastUsed = -1;

		_FileState() = default;
		explicit _FileState(const QString &key, const QString &url) : m_key(key), m_url(url) {}
		explicit _FileState(const QJsonObject &obj) { load(obj); }

		bool isOk(const QString &url, UrlAndHowSave urlAndHowSave)
		{
			std::lock_guard guard(m_mutex);
			if (m_state != State::Ok || m_filePath.isEmpty())
				return false;
			else if (url != m_url)
				return rsm::setState(m_curState, m_state, State::Initialized, false);
			else if (QFileInfo fi(m_filePath); !fi.isFile())
				return rsm::setState(m_curState, m_state, State::Initialized, false);
			else if ((m_fileSize <= 0) || (m_fileSize != fi.size()))
				return rsm::setState(m_curState, m_state, State::Initialized, false);
			else if ((m_birthTime <= 0) || (m_birthTime != fi.birthTime().toMSecsSinceEpoch()))
				return rsm::setState(m_curState, m_state, State::Initialized, false);
			else if ((m_lastModified <= 0) || (m_lastModified != fi.lastModified().toMSecsSinceEpoch()))
				return rsm::setState(m_curState, m_state, State::Initialized, false);
			else if (urlAndHowSave->check(urlAndHowSave, m_filePath))
				return true;
			return rsm::setState(m_curState, m_state, State::Initialized, false);
		}
		bool expired() const
		{
			std::lock_guard guard(m_mutex);
			if ((m_expired <= 0) || (m_lastUsed <= 0))
				return false;
			else if (auto used = QDateTime::currentSecsSinceEpoch() - m_lastUsed; used <= m_expired)
				return false;
			return true;
		}

		void load(const QJsonObject &obj)
		{
			std::lock_guard guard(m_mutex);
			m_curState = m_state = loadState(static_cast<State>(obj[QStringLiteral("state")].toInt()));
			m_key = obj[QStringLiteral("key")].toString();
			m_url = obj[QStringLiteral("url")].toString();
			m_filePath = obj[QStringLiteral("filePath")].toString();
			m_fileSize = static_cast<qint64>(obj[QStringLiteral("fileSize")].toDouble());
			m_birthTime = static_cast<qint64>(obj[QStringLiteral("birthTime")].toDouble());
			m_lastModified = static_cast<qint64>(obj[QStringLiteral("lastModified")].toDouble());
			m_expired = static_cast<qint64>(obj[QStringLiteral("expired")].toDouble(-1));
			m_lastUsed = static_cast<qint64>(obj[QStringLiteral("lastUsed")].toDouble(-1));
		}
		QJsonObject save() const
		{
			std::lock_guard guard(m_mutex);
			QJsonObject obj;
			obj[QStringLiteral("state")] = static_cast<int>(m_state);
			obj[QStringLiteral("key")] = m_key;
			obj[QStringLiteral("url")] = m_url;
			obj[QStringLiteral("filePath")] = m_filePath;
			obj[QStringLiteral("fileSize")] = m_fileSize;
			obj[QStringLiteral("birthTime")] = m_birthTime;
			obj[QStringLiteral("lastModified")] = m_lastModified;
			obj[QStringLiteral("expired")] = m_expired;
			obj[QStringLiteral("lastUsed")] = m_lastUsed;
			return obj;
		}

		void clear(qint64 expired = -1, State state = State::Failed)
		{
			std::lock_guard guard(m_mutex);
			if (!m_filePath.isEmpty())
				pls_remove_file(m_filePath);

			m_curState = m_state = state;
			m_url.clear();
			m_filePath.clear();
			m_fileSize = -1;
			m_birthTime = -1;
			m_lastModified = -1;
			m_expired = expired;
			m_lastUsed = -1;
		}
	};
	struct _CacheFile {
		State m_state;
		QString m_filePath;

		explicit _CacheFile(State state) : m_state(state) {}
		explicit _CacheFile(State state, const QString &filePath) : m_state(state), m_filePath(filePath) {}
	};

	using FileState = std::shared_ptr<_FileState>;

	mutable std::mutex m_statesMutex;
	std::map<QString, FileState> m_states;

	static Downloader *instance()
	{
		static Downloader s_instance;
		return &s_instance;
	}

	FileState getState(const QString &key, const QString &url)
	{
		std::lock_guard guard(m_statesMutex);
		if (auto iter = m_states.find(key); iter != m_states.end())
			return iter->second;

		auto fs = std::make_shared<_FileState>(key, url);
		m_states[key] = fs;
		return fs;
	}
	void setState(const QString &key, FileState fs)
	{
		std::lock_guard guard(m_statesMutex);
		m_states[key] = fs;
	}
	QJsonArray getStateJsonArray() const
	{
		QJsonArray arr;
		std::lock_guard guard(m_statesMutex);
		for (const auto &[_, fs] : m_states)
			arr.append(fs->save());
		return arr;
	}

	void initialize()
	{
		if (QJsonArray arr; pls_read_json(arr, rsm::getAppDataPath(QStringLiteral("cache/downloadStates.json")))) {
			bool saved = true;
			for (auto v : arr) {
				if (auto fs = std::make_shared<_FileState>(v.toObject()); !fs->expired()) {
					setState(fs->m_key, fs);
				} else {
					saved = false;
					fs->clear();
				}
			}

			if (!saved) {
				saveDownloadStates();
			}
		}
	}
	void cleanup()
	{
		saveDownloadStates();

		std::lock_guard guard(m_statesMutex);
		m_states.clear();
	}

	void saveDownloadStates() const
	{
		auto arr = getStateJsonArray();
		if (auto filePath = rsm::getAppDataPath(QStringLiteral("cache/downloadStates.json")); arr.isEmpty()) {
			pls_remove_file(filePath);
		} else if (QString error; !pls_write_json(filePath, arr, &error)) {
			PLS_LOGEX(PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({{"error", error.toUtf8().constData()}}), "save download states failed.");
		}
	}

	_CacheFile startDownload(const QString &key, const QString &url, UrlAndHowSave urlAndHowSave)
	{
		if (urlAndHowSave->m_noCache || key.isEmpty()) {
			return _CacheFile{State::Initialized};
		} else if (auto fs = getState(key, url); fs->isOk(url, urlAndHowSave)) {
			return _CacheFile{State::Ok, fs->m_filePath};
		} else {
			auto state = fs->m_state;
			fs->m_curState = State::Downloading;
			return _CacheFile{state != State::Ok ? state : State::Failed};
		}
	}
	void endDownload(const UrlAndHowSave &urlAndHowSave, const QString &key, const QString &url, bool ok, const QString &filePath)
	{
		if (urlAndHowSave->m_noCache || key.isEmpty()) {
			return;
		} else if (auto fs = getState(key, url); ok) {
			QFileInfo fi(filePath);
			qint64 fileSize = fi.size();
			qint64 birthTime = fi.birthTime().toMSecsSinceEpoch();
			qint64 lastModified = fi.lastModified().toMSecsSinceEpoch();

			std::lock_guard guard(fs->m_mutex);
			fs->m_curState = fs->m_state = State::Ok;
			fs->m_url = url;
			fs->m_filePath = filePath;
			fs->m_fileSize = fileSize;
			fs->m_birthTime = birthTime;
			fs->m_lastModified = lastModified;
			fs->m_expired = urlAndHowSave->expired();
			fs->m_lastUsed = QDateTime::currentSecsSinceEpoch();
		} else if (!fs->isOk(url, urlAndHowSave)) {
			fs->clear(urlAndHowSave->expired());
		} else {
			fs->m_curState = fs->m_state;
			return;
		}

		saveDownloadStates();
	}
	void useCache(const QString &key, const QString &url, qint64 expired)
	{
		if (auto fs = getState(key, url); fs) {
			std::lock_guard guard(fs->m_mutex);
			fs->m_expired = expired;
			fs->m_lastUsed = QDateTime::currentSecsSinceEpoch();
		}

		saveDownloadStates();
	}
	void asyncCall(pls::QObjectPtr<QObject> receiver, const ResultsCb &resultsCb, std::shared_ptr<std::list<DownloadResult>> results) const
	{
		if (!pls::get_object(receiver))
			pls_invoke_safe(resultsCb, *results);
		else if (receiver.valid())
			pls_async_call(receiver, [resultsCb, results]() { pls_invoke_safe(resultsCb, *results); });
	}
	void done(std::shared_ptr<std::list<DownloadResult>> results, UrlAndHowSave urlAndHowSave, State state) const
	{
		results->emplace_back(urlAndHowSave, state);
		urlAndHowSave->done(urlAndHowSave, false, QString(), PathFrom::Invalid, false);
	}
	void done(std::shared_ptr<std::list<DownloadResult>> results, UrlAndHowSave urlAndHowSave, const pls::http::Reply &reply) const
	{
		results->emplace_back(urlAndHowSave, State::Failed, reply.isTimeout(), reply.statusCode());
		urlAndHowSave->done(urlAndHowSave, false, QString(), PathFrom::Invalid, false);
	}
	void done(std::shared_ptr<std::list<DownloadResult>> results, UrlAndHowSave urlAndHowSave, const QString &filePath, State state, PathFrom pathFrom) const
	{
		urlAndHowSave->m_savedFilePath = filePath;
		auto decompressOk = urlAndHowSave->decompress(urlAndHowSave);
		results->emplace_back(urlAndHowSave, state, pathFrom, decompressOk);
		urlAndHowSave->done(urlAndHowSave, state == State::Ok, filePath, pathFrom, decompressOk);
	}
	void encryptJson(const QString &contentType, UrlAndHowSave urlAndHowSave, const QString &filePath) const
	{
		if (!urlAndHowSave->m_encryptJson)
			return;
		else if (!contentType.contains(QStringLiteral("application/json"))) {
			PLS_ERROR(LIBRESOURCE_MODULE, "Encrypt JSON failed, invalid content type: %s", contentType.toUtf8().constData());
			return;
		}

		QJsonDocument doc;
		if (QString error; !pls_read_json(doc, filePath, &error)) {
			PLS_ERROR(LIBRESOURCE_MODULE, "Encrypt JSON failed, parse JSON failed, error: %s", error.toUtf8().constData());
		} else if (!pls_remove_file(filePath, &error)) {
			PLS_ERROR(LIBRESOURCE_MODULE, "Encrypt JSON failed, can't remove oringal file, error: %s", error.toUtf8().constData());
		} else if (!pls_encrypt_json(filePath, doc, &error)) {
			PLS_ERROR(LIBRESOURCE_MODULE, "Encrypt JSON failed, error: %s", error.toUtf8().constData());
		}
	}

	QString userAgent() const
	{
#ifdef Q_OS_WIN
		auto wv = pls_get_win_ver();
		LANGID langId = GetUserDefaultUILanguage();
		return QString("PRISM Live Studio/%1 (Windows %2 Build %3 Architecture x64 Language %4)").arg(pls_get_prism_version_string()).arg(wv.major).arg(wv.build).arg(langId);
#else
		pls_mac_ver_t wvi = pls_get_mac_systerm_ver();
		QString langId = pls_get_current_system_language_id();
		return QString("PRISM Live Studio/%1 (MacOS %2 Build %3 Architecture arm Language %4)").arg(pls_get_prism_version_string()).arg(wvi.major).arg(wvi.buildNum.c_str()).arg(langId);
#endif
	}
	QVariantMap headers() const
	{
		QVariantMap headers;
#if defined(Q_OS_WIN)
		headers[QStringLiteral("X-prism-device")] = QStringLiteral("Windows OS");
#elif defined(Q_OS_MACOS)
		headers[QStringLiteral("X-prism-device")] = QStringLiteral("Mac OS");
#endif
		headers[QStringLiteral("Accept-Language")] = pls_prism_get_locale();
		headers[QStringLiteral("X-prism-appversion")] = pls_get_prism_version_string();
		headers[QStringLiteral("X-prism-ip")] = pls_get_local_ip();
		headers[QStringLiteral("X-prism-os")] = pls_get_os_ver_string();
		headers[QStringLiteral("User-Agent")] = userAgent();
		return headers;
	}

	void download(UrlAndHowSave urlAndHowSave, ResultCb &&resultCb) override { download(urlAndHowSave, nullptr, std::move(resultCb)); }
	void download(UrlAndHowSave urlAndHowSave, pls::QObjectPtr<QObject> receiver, ResultCb &&resultCb) override
	{
		download(std::list<UrlAndHowSave>{urlAndHowSave}, receiver, [resultCb](const auto &results) { pls_invoke_safe(resultCb, results.front()); });
	}
	void download(const std::list<UrlAndHowSave> &urlAndHowSaves, ResultsCb &&resultsCb) override { download(urlAndHowSaves, nullptr, std::move(resultsCb)); }
	void download(const std::list<UrlAndHowSave> &urlAndHowSaves, pls::QObjectPtr<QObject> receiver, ResultsCb &&resultsCb) override
	{
		pls::http::Requests requests;

		auto results = std::make_shared<std::list<DownloadResult>>();
		for (const auto &urlAndHowSave : urlAndHowSaves) {
			if (!urlAndHowSave->hasUrl()) {
				done(results, urlAndHowSave, urlAndHowSave->m_emptyUrlState);
				continue;
			}

			QString cacheFilePath;
			auto key = urlAndHowSave->key();
			auto url = urlAndHowSave->url();
			if (auto state = startDownload(key, url, urlAndHowSave); state.m_state == State::Downloading) {
				done(results, urlAndHowSave, State::Downloading);
				continue;
			} else if (state.m_state == State::Ok) {
				cacheFilePath = state.m_filePath;
				if (!urlAndHowSave->m_forceDownload) {
					useCache(key, url, urlAndHowSave->expired());
					done(results, urlAndHowSave, state.m_filePath, State::Ok, PathFrom::UseCache);
					continue;
				}
			}

			pls::http::Request request;

			if (urlAndHowSave->m_filePath)
				request.saveFilePath(urlAndHowSave->m_filePath.value());
			else if (auto fileName = urlAndHowSave->fileName(); fileName)
				request.saveFileName(fileName.value());

			if (urlAndHowSave->m_hmacKey)
				request.hmacUrl(urlAndHowSave->m_url.value(), urlAndHowSave->m_hmacKey.value());
			else
				request.url(urlAndHowSave->m_url.value());

			requests.add(request                                                                                     //
					     .method(pls::http::Method::Get)                                                     //
					     .rawHeaders(headers())                                                              //
					     .forDownload(true)                                                                  //
					     .saveDir(urlAndHowSave->saveDir())                                                  //
					     .withLog()                                                                          //
					     .timeout(urlAndHowSave->m_timeout)                                                  //
					     .okResult([this, results, urlAndHowSave, key, url](const pls::http::Reply &reply) { //
						     auto filePath = reply.downloadFilePath();
						     endDownload(urlAndHowSave, key, url, true, filePath);
						     encryptJson(reply.contentType(), urlAndHowSave, filePath);
						     done(results, urlAndHowSave, filePath, State::Ok, PathFrom::Downloaded);
					     })
					     .failResult([this, results, urlAndHowSave, key, url, cacheFilePath](const pls::http::Reply &reply) {
						     auto statusCode = reply.statusCode();
						     auto error = reply.errors().toUtf8();
						     PLS_LOGEX(PLS_LOG_ERROR, LIBRESOURCE_MODULE, urlAndHowSave->nfields({{"statusCode", QByteArray::number(statusCode)}, {"error", error}}),
							       "download resource failed, status code: %d, error: %s", statusCode, error.constData());
						     endDownload(urlAndHowSave, key, url, false, QString());
						     if (!urlAndHowSave->m_noCache && !cacheFilePath.isEmpty()) {
							     done(results, urlAndHowSave, cacheFilePath, State::Failed, PathFrom::UseCache);
						     } else if (urlAndHowSave->m_defaultFilePath) {
							     done(results, urlAndHowSave, urlAndHowSave->m_defaultFilePath.value(), State::Failed, PathFrom::UseDefault);
						     } else {
							     done(results, urlAndHowSave, reply);
						     }
					     }));
		}

		if (requests.count() <= 0) {
			asyncCall(receiver, resultsCb, results);
			return;
		}

		if (receiver.valid())
			requests.receiver(receiver.object());

		pls::http::requests(requests                                                                            //
					    .workInNewThread()                                                          //
					    .results([this, receiver, resultsCb, results](const pls::http::Replies &) { //
						    asyncCall(receiver, resultsCb, results);
					    }));
	}
};

class ResourceManager : public IResourceManager {
	using FileState = Downloader::FileState;

	class DownloadingContext {
		using FinishedCb = std::function<void(bool ok, const std::list<DownloadResult> &results)>;

		QByteArray m_type;
		QStringList m_categories;
		QStringList m_groups;
		QStringList m_items;

		bool m_ok = true;
		bool m_invoked = false;
		std::list<DownloadResult> m_results;
		FinishedCb m_finishedCb;

		static std::list<std::shared_ptr<DownloadingContext>> &all()
		{
			static std::list<std::shared_ptr<DownloadingContext>> s_all;
			return s_all;
		}

	public:
		static std::shared_ptr<DownloadingContext> create(const QByteArray &type, const QStringList &categories, const QStringList &groups, const QStringList &items, FinishedCb &&finishedCb)
		{
			auto context = std::make_shared<DownloadingContext>(type, categories, groups, items, std::move(finishedCb));
			all().push_back(context);
			return context;
		}
		static std::shared_ptr<DownloadingContext> create(const QStringList &categories, FinishedCb &&finishedCb)
		{
			return create("categories", categories, QStringList(), QStringList(), std::move(finishedCb));
		}
		static std::shared_ptr<DownloadingContext> create(CategoryImplPtr categoryImpl, FinishedCb &&finishedCb)
		{
			return create(categoryImpl->m_categoryIdUtf8, QStringList(), categoryImpl->m_groups, categoryImpl->m_items, std::move(finishedCb));
		}
		static std::shared_ptr<DownloadingContext> create(GroupImplPtr groupImpl, FinishedCb &&finishedCb)
		{
			return create(groupImpl->m_groupIdUtf8, QStringList(), {groupImpl->m_groupId}, groupImpl->m_items, std::move(finishedCb));
		}
		static std::shared_ptr<DownloadingContext> create(ItemImplPtr itemImpl, FinishedCb &&finishedCb)
		{
			return create(itemImpl->m_itemIdUtf8, QStringList(), QStringList(), {itemImpl->m_itemId}, std::move(finishedCb));
		}

		void removeCategory(const QString &categoryId, bool ok) { remove(m_categories, categoryId, ok, {}); }
		void removeGroup(const QString &groupId, bool ok, const std::list<DownloadResult> &results = {}) { remove(m_groups, groupId, ok, results); }
		void removeItem(const QString &itemId, bool ok, const std::list<DownloadResult> &results = {}) { remove(m_items, itemId, ok, results); }
		static void removeCategoryAll(const QString &categoryId, bool ok)
		{
			remove(categoryId, ok, {}, [](auto ctx) { return &ctx->m_categories; });
		}
		static void removeGroupAll(const QString &groupId, bool ok, const std::list<DownloadResult> &results)
		{
			remove(groupId, ok, results, [](auto ctx) { return &ctx->m_groups; });
		}
		static void removeItemAll(const QString &itemId, bool ok, const std::list<DownloadResult> &results)
		{
			remove(itemId, ok, results, [](auto ctx) { return &ctx->m_items; });
		}

		DownloadingContext(const QByteArray &type, const QStringList &categories, const QStringList &groups, const QStringList &items, FinishedCb &&finishedCb) //
			: m_type(type), m_categories(categories), m_groups(groups), m_items(items), m_finishedCb(std::move(finishedCb))
		{
		}

		template<typename GetList> static void remove(const QString &id, bool ok, const std::list<DownloadResult> &results, GetList getList)
		{
			auto &dcs = all();
			dcs.remove_if([](auto dc) { return dc->m_invoked; });
			for (auto dc : pls_copy(dcs))
				dc->remove(*getList(dc), id, ok, results);
		}
		void remove(QStringList &list, const QString &id, bool ok, const std::list<DownloadResult> &results)
		{
			if (list.removeOne(id)) {
				if (m_ok && !ok)
					m_ok = false;
				if (!ok && m_results.empty() && !results.empty())
					m_results = results;
			}

			if (m_invoked || !m_categories.isEmpty() || !m_groups.isEmpty() || !m_items.isEmpty())
				return;

			PLS_LOGEX(m_ok ? PLS_LOG_INFO : PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({{"rsmType", m_type.constData()}}), "download context %s finished %s", m_type.constData(),
				  m_ok ? "ok" : "failed");
			m_invoked = true;
			pls_invoke_safe(m_finishedCb, m_ok, m_results);
		}
	};
	class DownloadAllContext {
		using FinishedCb = std::function<void()>;

		QStringList m_categories;

		bool m_invoked = false;
		FinishedCb m_finishedCb;

	public:
		DownloadAllContext(const QStringList &categories, FinishedCb &&finishedCb) //
			: m_categories(categories), m_finishedCb(std::move(finishedCb))
		{
		}

		void remove(const QString &categoryId)
		{
			m_categories.removeOne(categoryId);
			if (m_invoked || !m_categories.isEmpty())
				return;

			m_invoked = true;
			pls_invoke_safe(m_finishedCb);
		}
	};
	struct CategoryItem {
		struct _CigiState {
			State m_state = State::Initialized;
			QString m_id;
			QVariantHash m_attrs;
			std::map<QString, FileState> m_fileStates;

			_CigiState() = default;
			explicit _CigiState(const QString &id, const QVariantHash &attrs) : m_id(id), m_attrs(attrs) {}
			explicit _CigiState(const QJsonObject &obj) { load(obj); }

			void load(const QJsonObject &obj)
			{
				m_state = loadState(static_cast<State>(obj[QStringLiteral("state")].toInt()));
				m_id = obj[QStringLiteral("id")].toString();
				m_attrs = obj[QStringLiteral("attrs")].toObject().toVariantHash();
				auto fss = obj[QStringLiteral("fileStates")].toArray();
				for (auto fsv : fss) {
					auto fs = std::make_shared<Downloader::_FileState>(fsv.toObject());
					m_fileStates[fs->m_key] = fs;
				}
			}
			QJsonObject save() const
			{
				QJsonObject obj;
				obj[QStringLiteral("state")] = static_cast<int>(m_state);
				obj[QStringLiteral("id")] = m_id;
				obj[QStringLiteral("attrs")] = QJsonObject::fromVariantHash(m_attrs);
				QJsonArray fss;
				for (const auto &[_, fs] : m_fileStates)
					fss.append(fs->save());
				obj[QStringLiteral("fileStates")] = fss;
				return obj;
			}

			bool checkAttrs(const QVariantHash &attrs) const { return pls_is_equal(m_attrs, attrs); }
		};
		using CigiState = std::shared_ptr<_CigiState>;

		int m_version = 0;
		QString m_categoryId;
		QString m_categoryUniqueKey;
		QString m_resourceUrl;
		QString m_fallbackUrl;
		CategoryImplPtr m_category;
		QByteArray m_categoryIdUtf8;

		std::map<QString, CigiState> m_groupStates;
		std::map<QString, CigiState> m_itemStates;
		std::map<QString, std::list<Item>> m_usedItems; // group -> items

		explicit CategoryItem(const QJsonObject &obj)
		{
			m_categoryId = obj[QStringLiteral("categoryId")].toString();
			m_version = obj[QStringLiteral("version")].toInt();
			m_categoryUniqueKey = obj[QStringLiteral("categoryUniqueKey")].toString();
			m_resourceUrl = obj[QStringLiteral("resourceUrl")].toString();
			m_fallbackUrl = obj[QStringLiteral("fallbackUrl")].toString();
			m_categoryIdUtf8 = m_categoryId.toUtf8();
		}

		bool isLoaded() const { return m_category ? true : false; }
		CategoryImplPtr category() const { return m_category; }
		void setCategory(ResourceManager *rm, ICategory *icategory, CategoryImplPtr category)
		{
			m_category = category;
			loadCustomItems(rm, icategory);
			loadCustomGroups(rm, icategory);
			loadGroupStates();
			loadItemStates();
			loadUsedItems();
		}

		std::list<Group> groups() const
		{
			if (m_category)
				return m_category->groups();
			return {};
		}
		std::list<Item> items() const
		{
			if (m_category)
				return m_category->items();
			return {};
		}

		GroupImplPtr findGroup(const QString &groupId) const { return m_category ? m_category->findGroup(groupId) : nullptr; }
		GroupImplPtr findGroup(const UniqueId &uniqueId) const { return m_category ? m_category->findGroup(uniqueId) : nullptr; }
		ItemImplPtr findItem(const QString &itemId) const { return m_category ? m_category->findItem(itemId) : nullptr; }
		ItemImplPtr findItem(const UniqueId &uniqueId) const { return m_category ? m_category->findItem(uniqueId) : nullptr; }

		static CigiState getState(std::map<QString, CigiState> &states, const QString &key, const QVariantHash &attrs)
		{
			if (auto iter = states.find(key); iter != states.end()) {
				auto cs = iter->second;
				cs->m_attrs = attrs;
				return cs;
			}

			auto cs = std::make_shared<_CigiState>(key, attrs);
			states[key] = cs;
			return cs;
		}
		static FileState getState(std::map<QString, FileState> &states, const QString &key, const QString &url)
		{
			if (auto iter = states.find(key); iter != states.end())
				return iter->second;

			auto fs = std::make_shared<Downloader::_FileState>(key, url);
			states[key] = fs;
			return fs;
		}

		State groupState(const QString &groupId) const
		{
			if (auto cs = pls_get_value(m_groupStates, groupId, nullptr); cs)
				return cs->m_state;
			return State::Failed;
		}
		State groupItemsState(const QString &groupId) const
		{
			if (Group group = findGroup(groupId); group) {
				bool ok = true;
				for (auto item : group.items()) {
					if (auto is = itemState(item->m_itemId); is == State::Downloading) {
						return State::Downloading;
					} else if (ok && is != State::Ok) {
						ok = false;
					}
				}
				return ok ? State::Ok : State::Failed;
			}
			return State::Failed;
		}
		State check(ResourceManager *mgr, const ICategory *icategory, bool forDownload)
		{
			for (auto group : groups()) {
				if (auto state = checkGroup(mgr, icategory, group.m_impl, forDownload); state != State::Ok)
					return state;
				for (auto item : group.items())
					if (auto state = checkItem(mgr, icategory, item.m_impl, forDownload); state != State::Ok)
						return state;
			}
			for (auto item : items())
				if (auto state = checkItem(mgr, icategory, item.m_impl, forDownload); state != State::Ok)
					return state;
			return State::Ok;
		}
		State checkGroup(ResourceManager *mgr, const ICategory *icategory, GroupImplPtr groupImpl, bool forDownload)
		{
			auto cs = getState(m_groupStates, groupImpl->m_groupId, groupImpl->m_attrs);
			if (forDownload && cs->m_state == State::Downloading) {
				PLS_INFO(LIBRESOURCE_MODULE, "download category [%s] group [%s] is in progress", m_categoryIdUtf8.constData(), groupImpl->m_groupIdUtf8.constData());
				return State::Downloading;
			}

			bool fssOk = true;
			for (auto urlAndHowSave : groupImpl->m_urlAndHowSaves) {
				auto names = urlAndHowSave->m_names.value().join('/');
				auto url = urlAndHowSave->url();
				if (!urlAndHowSave->hasUrl()) {
					PLS_WARN(LIBRESOURCE_MODULE, "category [%s] group [%s] file [%s] url invalid", m_categoryIdUtf8.constData(), groupImpl->m_groupIdUtf8.constData(),
						 names.toUtf8().constData());
				} else if (auto fs = getState(cs->m_fileStates, names, url); fs->isOk(url, urlAndHowSave)) {
					PLS_INFO(LIBRESOURCE_MODULE, "category [%s] group [%s] file [%s] check ok", m_categoryIdUtf8.constData(), groupImpl->m_groupIdUtf8.constData(),
						 fs->m_key.toUtf8().constData());
					urlAndHowSave->m_savedFilePath = fs->m_filePath;
				} else {
					if (!fs->m_filePath.isEmpty())
						PLS_WARN(LIBRESOURCE_MODULE, "category [%s] group [%s] file [%s] check failed", m_categoryIdUtf8.constData(), groupImpl->m_groupIdUtf8.constData(),
							 fs->m_key.toUtf8().constData());
					fssOk = false;
				}
			}

			if (fssOk && (cs->m_state == State::Ok) && cs->checkAttrs(groupImpl->m_attrs) && icategory->checkGroup(mgr, groupImpl)) {
				PLS_INFO(LIBRESOURCE_MODULE, "category [%s] group [%s] check ok", m_categoryIdUtf8.constData(), groupImpl->m_groupIdUtf8.constData());
				return State::Ok;
			} else if (!forDownload) {
				PLS_WARN(LIBRESOURCE_MODULE, "category [%s] group [%s] check failed", m_categoryIdUtf8.constData(), groupImpl->m_groupIdUtf8.constData());
				return State::Failed;
			}

			PLS_INFO(LIBRESOURCE_MODULE, "downloading category [%s] group [%s]", m_categoryIdUtf8.constData(), groupImpl->m_groupIdUtf8.constData());
			cs->m_state = State::Downloading;
			for (const auto &[_, fs] : cs->m_fileStates) {
				fs->m_state = State::Initialized;
				if (!fs->m_filePath.isEmpty())
					pls_remove_file(fs->m_filePath);
			}
			return State::Failed;
		}
		void endDownloadGroup(GroupImplPtr groupImpl, bool ok, const std::list<DownloadResult> &results)
		{
			PLS_LOGEX(ok ? PLS_LOG_INFO : PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({{"categoryId", m_categoryIdUtf8}, {"groupId", groupImpl->m_groupIdUtf8}}),
				  "downloaded category [%s] group [%s] %s", m_categoryIdUtf8.constData(), groupImpl->m_groupIdUtf8.constData(), ok ? "ok" : "failed");

			auto cs = getState(m_groupStates, groupImpl->m_groupId, groupImpl->m_attrs);

			for (const auto &result : results) {
				auto isOk = result.isOk();
				auto names = result.m_urlAndHowSave->m_names.value().join('/');
				auto namesUtf8 = names.toUtf8();
				auto url = result.m_urlAndHowSave->url();
				PLS_LOGEX(isOk ? PLS_LOG_INFO : PLS_LOG_ERROR, LIBRESOURCE_MODULE, result.m_urlAndHowSave->nfields({{"name", namesUtf8}}), "downloaded category [%s] group [%s] %s %s",
					  m_categoryIdUtf8.constData(), groupImpl->m_groupIdUtf8.constData(), namesUtf8.constData(), isOk ? "ok" : "failed");
				if (auto fs = getState(cs->m_fileStates, names, url); isOk) {
					auto filePath = result.filePath();
					QFileInfo fi(filePath);
					fs->m_curState = fs->m_state = State::Ok;
					fs->m_url = url;
					fs->m_filePath = filePath;
					fs->m_fileSize = fi.size();
					fs->m_birthTime = fi.birthTime().toMSecsSinceEpoch();
					fs->m_lastModified = fi.lastModified().toMSecsSinceEpoch();
				} else if (!fs->isOk(url, result.m_urlAndHowSave)) {
					fs->clear();
				}
			}

			cs->m_attrs = groupImpl->m_attrs;
			cs->m_state = ok ? State::Ok : State::Failed;
			saveGroupStates();
		}
		void loadGroupStates()
		{
			PLS_INFO(LIBRESOURCE_MODULE, "load category [%s] group state", m_categoryIdUtf8.constData());

			m_groupStates.clear();

			QJsonArray arr;
			if (pls_read_json(arr, rsm::getAppDataPath(m_categoryId + QStringLiteral("/groupStates.json")))) {
				for (auto v : arr) {
					auto cs = std::make_shared<_CigiState>(v.toObject());
					m_groupStates[cs->m_id] = cs;
				}
			}
		}
		void saveGroupStates() const
		{
			QJsonArray arr;
			for (const auto &[_, cs] : m_groupStates)
				arr.append(cs->save());
			if (auto filePath = rsm::getAppDataPath(m_categoryId + QStringLiteral("/groupStates.json")); arr.isEmpty()) {
				pls_remove_file(filePath);
			} else if (QString error; !pls_write_json(filePath, arr, &error)) {
				PLS_LOGEX(PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({{"categoryId", m_categoryIdUtf8.constData()}, {"error", error.toUtf8().constData()}}),
					  "save [%s] group states failed.", m_categoryIdUtf8.constData());
			}
		}
		State itemState(const QString &itemId) const
		{
			if (auto cs = pls_get_value(m_itemStates, itemId, nullptr); cs)
				return cs->m_state;
			return State::Failed;
		}
		State checkItem(ResourceManager *mgr, const ICategory *icategory, ItemImplPtr itemImpl, bool forDownload)
		{
			auto cs = getState(m_itemStates, itemImpl->m_itemId, itemImpl->m_attrs);
			if (forDownload && cs->m_state == State::Downloading) {
				PLS_INFO(LIBRESOURCE_MODULE, "download category [%s] item [%s] is in progress", m_categoryIdUtf8.constData(), itemImpl->m_itemIdUtf8.constData());
				return State::Downloading;
			}

			bool fssOk = true;
			for (auto urlAndHowSave : itemImpl->m_urlAndHowSaves) {
				auto names = urlAndHowSave->m_names.value().join('/');
				auto url = urlAndHowSave->url();
				if (!urlAndHowSave->hasUrl()) {
					PLS_WARN(LIBRESOURCE_MODULE, "category [%s] item [%s] file [%s] url invalid", m_categoryIdUtf8.constData(), itemImpl->m_itemIdUtf8.constData(),
						 names.toUtf8().constData());
				} else if (auto fs = getState(cs->m_fileStates, names, url); fs->isOk(url, urlAndHowSave)) {
					PLS_INFO(LIBRESOURCE_MODULE, "category [%s] item [%s] file [%s] check ok", m_categoryIdUtf8.constData(), itemImpl->m_itemIdUtf8.constData(),
						 names.toUtf8().constData());
					urlAndHowSave->m_savedFilePath = fs->m_filePath;
				} else {
					if (!fs->m_filePath.isEmpty())
						PLS_WARN(LIBRESOURCE_MODULE, "category [%s] item [%s] file [%s] check failed", m_categoryIdUtf8.constData(), itemImpl->m_itemIdUtf8.constData(),
							 fs->m_key.toUtf8().constData());
					fssOk = false;
				}
			}

			if (fssOk && (cs->m_state == State::Ok) && cs->checkAttrs(itemImpl->m_attrs) && icategory->checkItem(mgr, itemImpl)) {
				PLS_INFO(LIBRESOURCE_MODULE, "category [%s] item [%s] check ok", m_categoryIdUtf8.constData(), itemImpl->m_itemIdUtf8.constData());
				return State::Ok;
			} else if (!forDownload) {
				PLS_WARN(LIBRESOURCE_MODULE, "category [%s] item [%s] check failed", m_categoryIdUtf8.constData(), itemImpl->m_itemIdUtf8.constData());
				return State::Failed;
			}

			PLS_INFO(LIBRESOURCE_MODULE, "downloading category [%s] item [%s]", m_categoryIdUtf8.constData(), itemImpl->m_itemIdUtf8.constData());
			cs->m_state = State::Downloading;
			for (const auto &[_, fs] : cs->m_fileStates) {
				fs->m_state = State::Initialized;
				if (!fs->m_filePath.isEmpty())
					pls_remove_file(fs->m_filePath);
			}
			return State::Failed;
		}
		void endDownloadItem(ItemImplPtr itemImpl, bool ok, const std::list<DownloadResult> &results)
		{
			PLS_LOGEX(ok ? PLS_LOG_INFO : PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({{"categoryId", m_categoryIdUtf8}, {"itemId", itemImpl->m_itemIdUtf8}}),
				  "downloaded category [%s] item [%s] %s", m_categoryIdUtf8.constData(), itemImpl->m_itemIdUtf8.constData(), ok ? "ok" : "failed");

			auto cs = getState(m_itemStates, itemImpl->m_itemId, itemImpl->m_attrs);

			for (const auto &result : results) {
				auto isOk = result.isOk();
				auto names = result.m_urlAndHowSave->m_names.value().join('/');
				auto namesUtf8 = names.toUtf8();
				auto url = result.m_urlAndHowSave->url();
				PLS_LOGEX(isOk ? PLS_LOG_INFO : PLS_LOG_ERROR, LIBRESOURCE_MODULE, result.m_urlAndHowSave->nfields({{"name", namesUtf8}}), "downloaded category [%s] item [%s] %s %s",
					  m_categoryIdUtf8.constData(), itemImpl->m_itemIdUtf8.constData(), namesUtf8.constData(), isOk ? "ok" : "failed");
				if (auto fs = getState(cs->m_fileStates, names, url); isOk) {
					auto filePath = result.filePath();
					QFileInfo fi(filePath);
					fs->m_curState = fs->m_state = State::Ok;
					fs->m_url = url;
					fs->m_filePath = filePath;
					fs->m_fileSize = fi.size();
					fs->m_birthTime = fi.birthTime().toMSecsSinceEpoch();
					fs->m_lastModified = fi.lastModified().toMSecsSinceEpoch();
				} else if (!fs->isOk(url, result.m_urlAndHowSave)) {
					fs->clear();
				}
			}

			cs->m_attrs = itemImpl->m_attrs;
			cs->m_state = ok ? State::Ok : State::Failed;
			saveItemStates();
		}
		void loadItemStates()
		{
			PLS_INFO(LIBRESOURCE_MODULE, "load category [%s] item state", m_categoryIdUtf8.constData());

			m_itemStates.clear();
			if (QJsonArray arr; pls_read_json(arr, rsm::getAppDataPath(m_categoryId + QStringLiteral("/itemStates.json")))) {
				for (auto v : arr) {
					auto cs = std::make_shared<_CigiState>(v.toObject());
					m_itemStates[cs->m_id] = cs;
				}
			}
		}
		void saveItemStates() const
		{
			QJsonArray arr;
			for (const auto &[_, cs] : m_itemStates)
				arr.append(cs->save());
			if (auto filePath = rsm::getAppDataPath(m_categoryId + QStringLiteral("/itemStates.json")); arr.isEmpty()) {
				pls_remove_file(filePath);
			} else if (QString error; !pls_write_json(filePath, arr, &error)) {
				PLS_LOGEX(PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({{"categoryId", m_categoryIdUtf8.constData()}, {"error", error.toUtf8().constData()}}),
					  "save [%s] item states failed.", m_categoryIdUtf8.constData());
			}
		}

		void loadCustomGroups(ResourceManager *rm, ICategory *icategory)
		{
			PLS_INFO(LIBRESOURCE_MODULE, "load category [%s] custom groups", m_categoryIdUtf8.constData());

			if (QJsonArray arr; pls_read_json(arr, rsm::getAppDataPath(m_categoryId + QStringLiteral("/customGroups.json")))) {
				for (auto v : arr) {
					auto obj = v.toObject();
					auto groupId = obj[QStringLiteral("id")].toString();
					auto custom = obj[QStringLiteral("custom")].toBool();
					auto groupImpl = findGroup(groupId);
					if (!custom && groupImpl) {
						groupImpl->m_customAttrs = obj[QStringLiteral("customAttrs")].toObject().toVariantHash();
						groupImpl->m_items = pls_to_string_list(obj[QStringLiteral("items")].toArray());
						groupImpl->m_archive = true;
					} else if (custom && !groupImpl) {
						groupImpl = std::make_shared<GroupImpl>(groupId, obj[QStringLiteral("attrs")].toObject().toVariantHash(), true);
						groupImpl->m_customAttrs = obj[QStringLiteral("customAttrs")].toObject().toVariantHash();
						groupImpl->m_items = pls_to_string_list(obj[QStringLiteral("items")].toArray());
						groupImpl->m_archive = true;
						groupImpl->m_category = m_category;
						m_category->m_allGroups[groupId] = groupImpl;

						qsizetype pos = -1;
						bool archive = false;
						icategory->getCustomGroupExtras(pos, archive, rm, groupImpl);
						m_category->insertGroup(pos, groupId);
						groupImpl->m_uniqueId = icategory->getGroupUniqueId(rm, groupImpl).m_id;
						groupImpl->m_homeDir = icategory->getGroupHomeDir(rm, groupImpl);
					}
				}
			}
		}
		void saveCustomGroups()
		{
			QJsonArray arr;
			for (const auto &[groupId, groupImpl] : m_category->m_allGroups) {
				if (groupImpl->m_archive) {
					QJsonObject obj;
					obj[QStringLiteral("id")] = groupId;
					obj[QStringLiteral("custom")] = groupImpl->m_custom;
					if (groupImpl->m_custom)
						obj[QStringLiteral("attrs")] = QJsonObject::fromVariantHash(groupImpl->m_attrs);
					obj[QStringLiteral("customAttrs")] = QJsonObject::fromVariantHash(groupImpl->m_customAttrs);
					obj[QStringLiteral("items")] = QJsonArray::fromStringList(groupImpl->m_items);
					arr.append(obj);
				}
			}

			if (auto filePath = rsm::getAppDataPath(m_categoryId + QStringLiteral("/customGroups.json")); arr.isEmpty()) {
				pls_remove_file(filePath);
			} else if (QString error; !pls_write_json(filePath, arr, &error)) {
				PLS_LOGEX(PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({{"categoryId", m_categoryIdUtf8.constData()}, {"error", error.toUtf8().constData()}}),
					  "save [%s] custom groups failed.", m_categoryIdUtf8.constData());
			}
		}
		void loadCustomItems(ResourceManager *rm, ICategory *icategory)
		{
			PLS_INFO(LIBRESOURCE_MODULE, "load category [%s] custom items", m_categoryIdUtf8.constData());

			QJsonArray arr;
			if (pls_read_json(arr, rsm::getAppDataPath(m_categoryId + QStringLiteral("/customItems.json")))) {
				for (auto v : arr) {
					auto obj = v.toObject();
					auto itemId = obj[QStringLiteral("id")].toString();
					auto custom = obj[QStringLiteral("custom")].toBool();
					if (auto itemImpl = findItem(itemId); !custom && itemImpl) {
						itemImpl->m_customAttrs = obj[QStringLiteral("customAttrs")].toObject().toVariantHash();
						itemImpl->m_groups = pls_to_string_list(obj[QStringLiteral("groups")].toArray());
						itemImpl->m_archive = true;
					} else if (custom && !itemImpl) {
						itemImpl = std::make_shared<ItemImpl>(0, itemId, obj[QStringLiteral("attrs")].toObject().toVariantHash(), true);
						itemImpl->m_customAttrs = obj[QStringLiteral("customAttrs")].toObject().toVariantHash();
						itemImpl->m_groups = pls_to_string_list(obj[QStringLiteral("groups")].toArray());
						itemImpl->m_archive = true;
						itemImpl->m_category = m_category;
						m_category->m_allItems[itemId] = itemImpl;
						itemImpl->m_uniqueId = icategory->getItemUniqueId(rm, itemImpl).m_id;
						itemImpl->m_homeDir = icategory->getItemHomeDir(rm, itemImpl);
					}
				}
			}
		}
		void saveCustomItems()
		{
			QJsonArray arr;
			for (const auto &[itemId, itemImpl] : m_category->m_allItems) {
				if (itemImpl->m_archive) {
					QJsonObject obj;
					obj[QStringLiteral("id")] = itemId;
					obj[QStringLiteral("custom")] = itemImpl->m_custom;
					if (itemImpl->m_custom)
						obj[QStringLiteral("attrs")] = QJsonObject::fromVariantHash(itemImpl->m_attrs);
					obj[QStringLiteral("customAttrs")] = QJsonObject::fromVariantHash(itemImpl->m_customAttrs);
					obj[QStringLiteral("groups")] = QJsonArray::fromStringList(itemImpl->m_groups);
					arr.append(obj);
				}
			}

			if (auto filePath = rsm::getAppDataPath(m_categoryId + QStringLiteral("/customItems.json")); arr.isEmpty()) {
				pls_remove_file(filePath);
			} else if (QString error; !pls_write_json(filePath, arr, &error)) {
				PLS_LOGEX(PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({{"categoryId", m_categoryIdUtf8.constData()}, {"error", error.toUtf8().constData()}}),
					  "save [%s] custom items failed.", m_categoryIdUtf8.constData());
			}
		}

		std::list<Item> usedItems(const QString &group) const
		{
			if (auto iter = m_usedItems.find(group); iter != m_usedItems.end())
				return iter->second;
			return {};
		}
		void useItem(const QString &group, Item item, size_t useMaxCount)
		{
			if (!useMaxCount) {
				return;
			}

			PLS_INFO(LIBRESOURCE_MODULE, "[%s] add used item [%s][%s]", m_categoryIdUtf8.constData(), group.toUtf8().constData(), item->m_itemIdUtf8.constData());

			auto &usedItems = m_usedItems[group];
			usedItems.remove(item);
			usedItems.push_front(item);
			if (usedItems.size() > useMaxCount)
				usedItems.pop_back();
			saveUsedItems();
		}
		void removeUsedItem(const QString &group, Item item)
		{
			PLS_INFO(LIBRESOURCE_MODULE, "[%s] remove used item [%s][%s]", m_categoryIdUtf8.constData(), group.toUtf8().constData(), item->m_itemIdUtf8.constData());

			auto &usedItems = m_usedItems[group];
			auto size = usedItems.size();
			usedItems.remove(item);
			if (size != usedItems.size()) {
				saveUsedItems();
			}
		}
		void removeUsedItem(const QString &itemId)
		{
			PLS_INFO(LIBRESOURCE_MODULE, "[%s] remove used item [%s]", m_categoryIdUtf8.constData(), itemId.toUtf8().constData());

			bool changed = false;
			for (auto &[group, usedItems] : m_usedItems) {
				auto size = usedItems.size();
				usedItems.remove_if([itemId](Item item) { return item->m_itemId == itemId; });
				if (!changed && size != usedItems.size()) {
					changed = true;
				}
			}

			if (changed) {
				saveUsedItems();
			}
		}
		void removeUsedItems(const QString &group)
		{
			PLS_INFO(LIBRESOURCE_MODULE, "[%s] remove used item group [%s]", m_categoryIdUtf8.constData(), group.toUtf8().constData());

			if (auto &usedItems = m_usedItems[group]; !usedItems.empty()) {
				usedItems.clear();
				saveUsedItems();
			}
		}
		void removeUsedItems()
		{
			PLS_INFO(LIBRESOURCE_MODULE, "[%s] remove all used items", m_categoryIdUtf8.constData());

			if (!m_usedItems.empty()) {
				m_usedItems.clear();
				saveUsedItems();
			}
		}
		void loadUsedItems()
		{
			PLS_INFO(LIBRESOURCE_MODULE, "load category %s used items", m_categoryIdUtf8.constData());

			m_usedItems.clear();

			QJsonObject obj;
			if (!pls_read_json(obj, rsm::getAppDataPath(m_categoryId + QStringLiteral("/usedItems.json")))) {
				return;
			}

			bool changed = false;
			for (const auto &group : obj.keys()) {
				for (auto id : obj[group].toArray()) {
					if (auto item = m_category->findItem(id.toString()); item) {
						m_usedItems[group].push_back(item);
					} else if (!changed) {
						changed = true;
					}
				}
			}

			if (changed) {
				saveUsedItems();
			}
		}
		void saveUsedItems() const
		{
			QJsonObject obj;
			for (const auto &[group, items] : m_usedItems) {
				QJsonArray arr;
				for (const auto &item : items)
					arr.append(item->id());
				obj[group] = arr;
			}
			if (auto filePath = rsm::getAppDataPath(m_categoryId + QStringLiteral("/usedItems.json")); obj.isEmpty()) {
				pls_remove_file(filePath);
			} else if (QString error; !pls_write_json(filePath, obj, &error)) {
				PLS_LOGEX(PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({{"categoryId", m_categoryIdUtf8.constData()}, {"error", error.toUtf8().constData()}}),
					  "save [%s] used items failed.", m_categoryIdUtf8.constData());
			}
		}
	};

	using CategoryItemPtr = std::shared_ptr<CategoryItem>;

	mutable std::shared_mutex m_moduleNamesMutex;
	mutable std::map<QByteArray, QByteArray> m_moduleNames;

	pls::http::ExclusiveWorker *m_worker = nullptr;
	std::map<QString, ICategory *> m_icategories;
	std::map<QString, CategoryItemPtr> m_categoryItems;
	std::map<QString, State> m_states;

public:
	static ResourceManager *instance()
	{
		static ResourceManager s_instance;
		return &s_instance;
	}

public:
	ResourceManager() = default;
	~ResourceManager() override = default;

	bool hasICategories() const { return !m_icategories.empty(); }
	ICategory *getICategory(const QString &categoryId) const { return pls_get_value(m_icategories, categoryId, nullptr); }

	bool hasCategoryItems() const { return !m_categoryItems.empty(); }
	CategoryItemPtr getCategoryItem(const QString &categoryId) const { return pls_get_value(m_categoryItems, categoryId, nullptr); }
	void addCategoryItem(CategoryItemPtr categoryItem)
	{
		PLS_INFO(LIBRESOURCE_MODULE, "add category: %s", categoryItem->m_categoryIdUtf8.constData());
		m_categoryItems[categoryItem->m_categoryId] = categoryItem;
	}

	template<typename Fn, typename... Args> void syncCall(Fn fn, Args &&...args) const
	{
		if (QThread::currentThread() == m_worker) {
			fn(std::forward<Args>(args)...);
		} else if (m_worker) {
			m_worker->syncCall([fn, args...]() { fn(args...); });
		}
	}
	template<typename R, typename Fn, typename... Args> void syncCall(R &rv, Fn fn, Args &&...args) const
	{
		if (QThread::currentThread() == m_worker) {
			rv = fn(std::forward<Args>(args)...);
		} else if (m_worker) {
			m_worker->syncCall([&rv, fn, args...]() { rv = fn(args...); });
		}
	}
	template<typename Fn, typename... Args> void asyncCall(Fn fn, Args &&...args) const
	{
		if (QThread::currentThread() == m_worker) {
			fn(std::forward<Args>(args)...);
		} else if (m_worker) {
			m_worker->asyncCall([fn, args...]() { fn(args...); });
		}
	}

	bool startDownload(const QString &key)
	{
		if (pls_get_value(m_states, key, State::Initialized) == State::Downloading)
			return false;
		m_states[key] = State::Downloading;
		return true;
	}
	template<typename Fail, typename... Args> bool startDownload(const QString &key, Fail fail, Args &&...args)
	{
		if (startDownload(key))
			return true;
		fail(std::forward<Args>(args)...);
		return false;
	}
	void setState(const QString &key, State state)
	{
		asyncCall([this, key, state]() { m_states[key] = state; });
	}
	template<typename Fn, typename... Args> void setState(const QString &key, State state, Fn fn, Args &&...args)
	{
		setState(key, state);
		asyncCall(fn, std::forward<Args>(args)...);
	}

	const char *moduleName(const QByteArray &submodule) const override
	{
		{
			std::shared_lock lock(m_moduleNamesMutex);
			if (auto iter = m_moduleNames.find(submodule); iter != m_moduleNames.end())
				return iter->second.constData();
		}

		std::unique_lock lock(m_moduleNamesMutex);
		if (auto iter = m_moduleNames.find(submodule); iter != m_moduleNames.end())
			return iter->second.constData();

		auto &moduleName = m_moduleNames[submodule];
		moduleName = "libresource-" + submodule;
		return moduleName.constData();
	}

	void registerCategory(ICategory *icategory) override { m_icategories[icategory->categoryId(this)] = icategory; }
	void unregisterCategory(ICategory *icategory) override { m_icategories.erase(icategory->categoryId(this)); }

	void initialize() { m_worker = pls_new<pls::http::ExclusiveWorker>(); }
	void cleanup()
	{
		if (m_worker) {
			m_worker->quitAndWait();
			pls_delete(m_worker, nullptr);
		}

		for (const auto &[_, categoryItem] : m_categoryItems) {
			if (categoryItem->m_category) {
				categoryItem->saveGroupStates();
				categoryItem->saveItemStates();
				categoryItem->saveUsedItems();
			}
		}

		m_icategories.clear();
		m_categoryItems.clear();
		m_states.clear();
	}

	void downloadAll(const std::function<void()> &complete) // download all resource
	{
		asyncCall([this, complete]() {
			PLS_INFO(LIBRESOURCE_MODULE, "download all");

			if (!startDownload(DOWNLOAD_ALL)) {
				PLS_WARN(LIBRESOURCE_MODULE, "download all is in progress");
				pls_invoke_safe(complete);
				return;
			}

			downloadCategoryDotJson([this, complete](bool ok) {
				if (ok) {
					auto dc = DownloadingContext::create(pls_get_keys<QList>(m_categoryItems), [this](bool ok1, const std::list<DownloadResult> &) {
						PLS_LOGEX(ok1 ? PLS_LOG_INFO : PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({}), "download all finished %s", ok1 ? "ok" : "failed");
						setState(DOWNLOAD_ALL, ok1 ? State::Ok : State::Failed);
					});
					auto dac = std::make_shared<DownloadAllContext>(pls_get_keys<QList>(m_categoryItems), [complete]() {
						PLS_LOGEX(PLS_LOG_INFO, LIBRESOURCE_MODULE, neloFields({}), "download all json download finished");
						pls_invoke_safe(complete);
					});
					for (const auto &[_, categoryItem] : m_categoryItems) {
						if (auto icategory = getICategory(categoryItem->m_categoryId); icategory) {
							downloadCategoryItem(
								categoryItem, icategory, false, [categoryItem, dc](bool ok2) { dc->removeCategory(categoryItem->m_categoryId, ok2); }, dac);
						} else {
							dc->removeCategory(categoryItem->m_categoryId, true);
							dac->remove(categoryItem->m_categoryId);
						}
					}
				} else {
					setState(DOWNLOAD_ALL, State::Failed);
					pls_invoke_safe(complete);
				}
			});
		});
	}
	void downloadCategory(const QString &categoryId) override // download category resource
	{
		if (categoryId.isEmpty())
			return;
		else if (auto icategory = getICategory(categoryId); !icategory)
			PLS_WARN(LIBRESOURCE_MODULE, "ICategory [%s] is't registered", categoryId.toUtf8().constData());
		else {
			asyncCall([this, categoryId, icategory]() {
				PLS_INFO(LIBRESOURCE_MODULE, "download category %s", categoryId.toUtf8().constData());

				auto key = DOWNLOAD_CATEGORY + categoryId;
				if (!startDownload(key)) {
					PLS_INFO(LIBRESOURCE_MODULE, "download category %s is in progress", categoryId.toUtf8().constData());
					return;
				}

				downloadCategoryDotJson([this, categoryId, icategory, key](bool ok) {
					if (ok) {
						downloadCategoryItem(categoryId, icategory, true, [this, key](bool ok1) { setState(key, ok1 ? State::Ok : State::Failed); });
					} else {
						setState(key, State::Failed);
					}
				});
			});
		}
	}
	void downloadCategoryGroup(const QString &categoryId, const QString &groupId) override // download category group resource
	{
		if (categoryId.isEmpty() || groupId.isEmpty())
			return;

		asyncCall([this, categoryId, groupId]() {
			PLS_INFO(LIBRESOURCE_MODULE, "download category %s group %s", categoryId.toUtf8().constData(), groupId.toUtf8().constData());

			auto key = DOWNLOAD_CATEGORY_GROUP + categoryId + '/' + groupId;
			if (!startDownload(key)) {
				PLS_WARN(LIBRESOURCE_MODULE, "download category %s group %s is in progress", categoryId.toUtf8().constData(), groupId.toUtf8().constData());
				return;
			}

			downloadCategoryDotJson([this, categoryId, groupId, key](bool ok) {
				if (!ok) {
					setState(key, State::Failed);
				} else if (auto icategory = getICategory(categoryId); !icategory) {
					setState(key, State::Failed);
				} else if (auto categoryItem = getCategoryItem(categoryId); !categoryItem) {
					setState(key, State::Failed);
				} else if (auto group = categoryItem->findGroup(groupId); group) {
					downloadCategoryItemGroup(categoryItem, icategory, group, true, [this, key](bool ok1) { setState(key, ok1 ? State::Ok : State::Failed); });
				}
			});
		});
	}
	void downloadCategoryItem(const QString &categoryId, const QString &itemId) override // download category item resource
	{
		if (categoryId.isEmpty() || itemId.isEmpty())
			return;

		asyncCall([this, categoryId, itemId]() {
			PLS_INFO(LIBRESOURCE_MODULE, "download category %s item %s", categoryId.toUtf8().constData(), itemId.toUtf8().constData());

			auto key = DOWNLOAD_CATEGORY_ITEM + categoryId + '/' + itemId;
			if (!startDownload(key)) {
				PLS_WARN(LIBRESOURCE_MODULE, "download category %s item %s is in progress", categoryId.toUtf8().constData(), itemId.toUtf8().constData());
				return;
			}

			downloadCategoryDotJson([this, categoryId, itemId, key](bool ok) {
				if (!ok) {
					setState(key, State::Failed);
				} else if (auto icategory = getICategory(categoryId); !icategory) {
					setState(key, State::Failed);
				} else if (auto categoryItem = getCategoryItem(categoryId); !categoryItem) {
					setState(key, State::Failed);
				} else if (auto item = categoryItem->findItem(itemId); item) {
					downloadCategoryItemItem(categoryItem, icategory, item, true, [this, key](bool ok1) { setState(key, ok1 ? State::Ok : State::Failed); });
				}
			});
		});
	}

	State getJsonState(const QString &categoryId) const override
	{
		State state = State::Failed;
		syncCall([this, categoryId, &state]() {
			if (auto categoryItem = getCategoryItem(categoryId); categoryItem) {
				state = categoryItem->isLoaded() ? State::Ok : State::Downloading;
			}
		});
		return state;
	}
	State getCategoryState(const QString &categoryId) const override
	{
		State state = State::Failed;
		syncCall([this, categoryId, &state]() {
			if (auto categoryItem = getCategoryItem(categoryId); categoryItem) {
				state = pls_get_value(m_states, categoryItem->m_categoryId, State::Failed);
			}
		});
		return state;
	}
	State getGroupState(const QString &categoryId, const QString &groupId) const override
	{
		State state = State::Failed;
		syncCall([this, categoryId, groupId, &state]() {
			if (auto categoryItem = getCategoryItem(categoryId); !categoryItem) {
				state = State::Failed;
			} else if (auto gs = categoryItem->groupState(groupId); gs == State::Initialized || gs == State::Downloading) {
				state = gs;
			} else if (auto giss = categoryItem->groupItemsState(groupId); giss == State::Downloading) {
				state = State::Downloading;
			} else if (gs == State::Ok && giss == State::Ok) {
				state = State::Ok;
			} else {
				state = State::Failed;
			}
		});
		return state;
	}
	State getItemState(const QString &categoryId, const QString &itemId) const override
	{
		State state = State::Failed;
		syncCall([this, categoryId, itemId, &state]() {
			if (auto categoryItem = getCategoryItem(categoryId); categoryItem) {
				state = categoryItem->itemState(itemId);
			}
		});
		return state;
	}

	void syncFindCategory(const QString &categoryId, std::function<void(ICategory *icategory, CategoryItemPtr categoryItem)> &&result) const
	{
		syncCall([this, categoryId, result]() { pls_invoke_safe(result, getICategory(categoryId), getCategoryItem(categoryId)); });
	}
	void asyncFindCategory(const QString &categoryId, std::function<void(ICategory *icategory, CategoryItemPtr categoryItem)> &&result) const
	{
		asyncCall([this, categoryId, result]() { pls_invoke_safe(result, getICategory(categoryId), getCategoryItem(categoryId)); });
	}

	void syncFindGroup(const QString &categoryId, const QString &groupId, std::function<void(ICategory *icategory, CategoryItemPtr categoryItem, Group group)> &&result) const
	{
		syncFindCategory(categoryId, [this, groupId, result](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (icategory && categoryItem) {
				pls_invoke_safe(result, icategory, categoryItem, categoryItem->findGroup(groupId));
			} else {
				pls_invoke_safe(result, icategory, categoryItem, nullptr);
			}
		});
	}
	void asyncFindGroup(const QString &categoryId, const QString &groupId, std::function<void(ICategory *icategory, CategoryItemPtr categoryItem, Group group)> &&result) const
	{
		asyncFindCategory(categoryId, [this, groupId, result](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (icategory && categoryItem) {
				pls_invoke_safe(result, icategory, categoryItem, categoryItem->findGroup(groupId));
			} else {
				pls_invoke_safe(result, icategory, categoryItem, nullptr);
			}
		});
	}

	Category getCategory(const QString &categoryId) const override
	{
		CategoryItemPtr categoryItem = nullptr;
		syncCall(categoryItem, [this, categoryId]() { return getCategoryItem(categoryId); });
		return categoryItem ? categoryItem->category() : nullptr;
	}
	void getCategory(const QString &categoryId, std::function<void(Category category)> &&result) const override
	{
		asyncCall([this, categoryId, result]() {
			auto categoryItem = getCategoryItem(categoryId);
			pls_invoke_safe(result, categoryItem ? categoryItem->category() : nullptr);
		});
	}
	void getCategory(const QString &categoryId, pls::QObjectPtr<QObject> receiver, std::function<void(Category category)> &&result) const override
	{
		getCategory(categoryId, [this, categoryId, receiver, result](Category category) { pls_async_call(receiver, [category, result]() { pls_invoke_safe(result, category); }); });
	}

	void getGroups(bool async, const QString &categoryId, std::function<void(const std::list<Group> &groups)> &&result) const
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [this, result](ICategory *icategory, CategoryItemPtr categoryItem) {
			std::list<Group> groups;
			if (icategory && categoryItem)
				groups = categoryItem->groups();
			pls_invoke_safe(result, groups);
		});
	}
	std::list<Group> getGroups(const QString &categoryId) const override
	{
		std::list<Group> result;
		getGroups(false, categoryId, [&result](const std::list<Group> &groups) { result = groups; });
		return result;
	}
	void getGroups(const QString &categoryId, std::function<void(const std::list<Group> &groups)> &&result) const override { getGroups(true, categoryId, std::move(result)); }
	void getGroups(const QString &categoryId, pls::QObjectPtr<QObject> receiver, std::function<void(const std::list<Group> &groups)> &&result) const override
	{
		getGroups(categoryId, [receiver, result](const std::list<Group> &groups) { pls_async_call(receiver, [groups, result]() { pls_invoke_safe(result, groups); }); });
	}

	void getItems(bool async, const QString &categoryId, std::function<void(const std::list<Item> &items)> &&result) const
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [this, result](ICategory *icategory, CategoryItemPtr categoryItem) {
			std::list<Item> items;
			if (icategory && categoryItem)
				items = categoryItem->items();
			pls_invoke_safe(result, items);
		});
	}
	std::list<Item> getItems(const QString &categoryId) const override
	{
		std::list<Item> result;
		getItems(false, categoryId, [&result](const std::list<Item> &items) { result = items; });
		return result;
	}
	void getItems(const QString &categoryId, std::function<void(const std::list<Item> &items)> &&result) const override { getItems(true, categoryId, std::move(result)); }
	void getItems(const QString &categoryId, pls::QObjectPtr<QObject> receiver, std::function<void(const std::list<Item> &items)> &&result) const override
	{
		getItems(categoryId, [receiver, result](const std::list<Item> &items) { pls_async_call(receiver, [items, result]() { pls_invoke_safe(result, items); }); });
	}

	void getGroup(bool async, const QString &categoryId, const QString &groupId, std::function<void(Group group)> &&result) const
	{
		auto findGroup = async ? &ResourceManager::asyncFindGroup : &ResourceManager::syncFindGroup;
		(this->*findGroup)(categoryId, groupId, [result](ICategory *icategory, CategoryItemPtr categoryItem, Group group) {
			if (icategory && categoryItem && group) {
				pls_invoke_safe(result, group);
			} else {
				pls_invoke_safe(result, nullptr);
			}
		});
	}
	Group getGroup(const QString &categoryId, const QString &groupId) const override
	{
		Group result;
		getGroup(false, categoryId, groupId, [&result](Group group) { result = group; });
		return result;
	}
	void getGroup(const QString &categoryId, const QString &groupId, std::function<void(Group group)> &&result) const override { getGroup(true, categoryId, groupId, std::move(result)); }
	void getGroup(const QString &categoryId, const QString &groupId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group)> &&result) const override
	{
		getGroup(categoryId, groupId, [receiver, result](Group group) { pls_async_call(receiver, [group, result]() { pls_invoke_safe(result, group); }); });
	}

	void getGroupItem(bool async, const QString &categoryId, const QString &groupId, const QString &itemId, std::function<void(Group group, Item item)> &&result) const
	{
		auto findGroup = async ? &ResourceManager::asyncFindGroup : &ResourceManager::syncFindGroup;
		(this->*findGroup)(categoryId, groupId, [this, itemId, result](ICategory *icategory, CategoryItemPtr categoryItem, Group group) {
			if (!icategory || !categoryItem || !group) {
				pls_invoke_safe(result, nullptr, nullptr);
			} else if (group->m_items.contains(itemId)) {
				pls_invoke_safe(result, group, categoryItem->findItem(itemId));
			} else {
				pls_invoke_safe(result, group, nullptr);
			}
		});
	}
	std::pair<Group, Item> getGroupItem(const QString &categoryId, const QString &groupId, const QString &itemId) const
	{
		std::pair<Group, Item> result{nullptr, nullptr};
		getGroupItem(false, categoryId, groupId, itemId, [&result](Group group, Item item) { result = {group, item}; });
		return result;
	}
	void getGroupItem(const QString &categoryId, const QString &groupId, const QString &itemId, std::function<void(Group group, Item item)> &&result) const override
	{
		getGroupItem(true, categoryId, groupId, itemId, std::move(result));
	}
	void getGroupItem(const QString &categoryId, const QString &groupId, const QString &itemId, pls::QObjectPtr<QObject> receiver,
			  std::function<void(Group group, Item item)> &&result) const override
	{
		getGroupItem(categoryId, groupId, itemId, [receiver, result](Group group, Item item) { pls_async_call(receiver, [group, item, result]() { pls_invoke_safe(result, group, item); }); });
	}

	void getItem(bool async, const QString &categoryId, const QString &itemId, std::function<void(Item item)> &&result) const
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [this, itemId, result](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (icategory && categoryItem) {
				pls_invoke_safe(result, categoryItem->findItem(itemId));
			} else {
				pls_invoke_safe(result, nullptr);
			}
		});
	}
	Item getItem(const QString &categoryId, const QString &itemId) const override
	{
		Item result = nullptr;
		getItem(false, categoryId, itemId, [&result](Item item) { result = item; });
		return result;
	}
	void getItem(const QString &categoryId, const QString &itemId, std::function<void(Item item)> &&result) const override { getItem(true, categoryId, itemId, std::move(result)); }
	void getItem(const QString &categoryId, const QString &itemId, pls::QObjectPtr<QObject> receiver, std::function<void(Item item)> &&result) const override
	{
		getItem(categoryId, itemId, [receiver, result](Item item) { pls_async_call(receiver, [item, result]() { pls_invoke_safe(result, item); }); });
	}

	void getGroup(bool async, const QString &categoryId, const UniqueId &uniqueId, std::function<void(Group group)> &&result) const
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [this, uniqueId, result](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (icategory && categoryItem) {
				pls_invoke_safe(result, categoryItem->findGroup(uniqueId));
			} else {
				pls_invoke_safe(result, nullptr);
			}
		});
	}
	Group getGroup(const QString &categoryId, const UniqueId &uniqueId) const override
	{
		Group result;
		getGroup(false, categoryId, uniqueId, [&result](Group group) { result = group; });
		return result;
	}
	void getGroup(const QString &categoryId, const UniqueId &uniqueId, std::function<void(Group group)> &&result) const override { getGroup(true, categoryId, uniqueId, std::move(result)); }
	void getGroup(const QString &categoryId, const UniqueId &uniqueId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group)> &&result) const override
	{
		getGroup(categoryId, uniqueId, [receiver, result](Group group) { pls_async_call(receiver, [group, result]() { pls_invoke_safe(result, group); }); });
	}

	void getItem(bool async, const QString &categoryId, const UniqueId &uniqueId, std::function<void(Item item)> &&result) const
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [this, uniqueId, result](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (icategory && categoryItem) {
				pls_invoke_safe(result, categoryItem->findItem(uniqueId));
			} else {
				pls_invoke_safe(result, nullptr);
			}
		});
	}
	Item getItem(const QString &categoryId, const UniqueId &uniqueId) const override
	{
		Item result = nullptr;
		getItem(false, categoryId, uniqueId, [&result](Item item) { result = item; });
		return result;
	}
	void getItem(const QString &categoryId, const UniqueId &uniqueId, std::function<void(Item item)> &&result) const override { getItem(true, categoryId, uniqueId, std::move(result)); }
	void getItem(const QString &categoryId, const UniqueId &uniqueId, pls::QObjectPtr<QObject> receiver, std::function<void(Item item)> &&result) const override
	{
		getItem(categoryId, uniqueId, [receiver, result](Item item) { pls_async_call(receiver, [item, result]() { pls_invoke_safe(result, item); }); });
	}

	void checkCategory(bool async, const QString &categoryId, std::function<void(Category category, bool ok)> &&result) const
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [this, result](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (icategory && categoryItem) {
				pls_invoke_safe(result, categoryItem->m_category, categoryItem->check(pls_ptr(this), icategory, false) == State::Ok);
			} else {
				pls_invoke_safe(result, nullptr, false);
			}
		});
	}
	std::pair<Category, bool> checkCategory(const QString &categoryId) const override
	{
		std::pair<Category, bool> result{nullptr, false};
		checkCategory(false, categoryId, [&result](Category category, bool ok) { result = {category, ok}; });
		return result;
	}
	void checkCategory(const QString &categoryId, std::function<void(Category category, bool ok)> &&result) const override { checkCategory(true, categoryId, std::move(result)); }
	void checkCategory(const QString &categoryId, pls::QObjectPtr<QObject> receiver, std::function<void(Category category, bool ok)> &&result) const override
	{
		checkCategory(categoryId, [receiver, result](Category category, bool ok) { pls_async_call(receiver, [category, ok, result]() { pls_invoke_safe(result, category, ok); }); });
	}

	void checkGroup(bool async, const QString &categoryId, const QString &groupId, std::function<void(Group group, bool ok)> &&result) const
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [this, groupId, result](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (!icategory || !categoryItem) {
				pls_invoke_safe(result, nullptr, false);
			} else if (auto group = categoryItem->findGroup(groupId); group) {
				pls_invoke_safe(result, group, categoryItem->checkGroup(pls_ptr(this), icategory, group, false) == State::Ok);
			} else {
				pls_invoke_safe(result, nullptr, false);
			}
		});
	}
	std::pair<Group, bool> checkGroup(const QString &categoryId, const QString &groupId) const override
	{
		std::pair<Group, bool> result{nullptr, false};
		checkGroup(false, categoryId, groupId, [&result](Group group, bool ok) { result = {group, ok}; });
		return result;
	}
	void checkGroup(const QString &categoryId, const QString &groupId, std::function<void(Group group, bool ok)> &&result) const override
	{
		checkGroup(true, categoryId, groupId, std::move(result));
	}
	void checkGroup(const QString &categoryId, const QString &groupId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, bool ok)> &&result) const override
	{
		checkGroup(categoryId, groupId, [receiver, result](Group group, bool ok) { pls_async_call(receiver, [group, ok, result]() { pls_invoke_safe(result, group, ok); }); });
	}

	void checkItem(bool async, const QString &categoryId, const QString &itemId, std::function<void(Item item, bool ok)> &&result) const
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [this, itemId, result](const ICategory *icategory, CategoryItemPtr categoryItem) {
			if (!icategory || !categoryItem) {
				pls_invoke_safe(result, nullptr, false);
			} else if (auto item = categoryItem->findItem(itemId); item) {
				pls_invoke_safe(result, item, categoryItem->checkItem(pls_ptr(this), icategory, item, false) == State::Ok);
			} else {
				pls_invoke_safe(result, nullptr, false);
			}
		});
	}
	std::pair<Item, bool> checkItem(const QString &categoryId, const QString &itemId) const override
	{
		std::pair<Item, bool> result{nullptr, false};
		checkItem(false, categoryId, itemId, [&result](Item item, bool ok) { result = {item, ok}; });
		return result;
	}
	void checkItem(const QString &categoryId, const QString &itemId, std::function<void(Item item, bool ok)> &&result) const override { checkItem(true, categoryId, itemId, std::move(result)); }
	void checkItem(const QString &categoryId, const QString &itemId, pls::QObjectPtr<QObject> receiver, std::function<void(Item item, bool ok)> &&result) const override
	{
		checkItem(categoryId, itemId, [receiver, result](Item item, bool ok) { pls_async_call(receiver, [item, ok, result]() { pls_invoke_safe(result, item, ok); }); });
	}

	// custom group/items
	GroupImplPtr addCustomGroup(ICategory *icategory, CategoryItemPtr categoryItem, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs) const
	{
		auto groupImpl = std::make_shared<GroupImpl>(groupId, groupAttrs, true);
		groupImpl->m_customAttrs = groupCustomAttrs;
		groupImpl->m_category = categoryItem->m_category;

		qsizetype pos = -1;
		icategory->getCustomGroupExtras(pos, groupImpl->m_archive, pls_ptr(this), groupImpl);
		categoryItem->m_category->m_allGroups[groupId] = groupImpl;
		categoryItem->m_category->insertGroup(pos, groupId);
		groupImpl->m_uniqueId = icategory->getGroupUniqueId(pls_ptr(this), groupImpl).m_id;
		groupImpl->m_homeDir = icategory->getGroupHomeDir(pls_ptr(this), groupImpl);
		categoryItem->saveCustomGroups();
		return groupImpl;
	}
	std::pair<Group, Item> addCustomItem(const QString &categoryId, const QString &groupId, Item item) override { return addCustomItem(categoryId, groupId, QVariantHash(), QVariantHash(), item); }
	void addCustomItem(const QString &categoryId, const QString &groupId, Item item, std::function<void(Group group, Item item)> &&result) override
	{
		addCustomItem(categoryId, groupId, QVariantHash(), QVariantHash(), item, std::move(result));
	}
	void addCustomItem(const QString &categoryId, const QString &groupId, Item item, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) override
	{
		addCustomItem(categoryId, groupId, QVariantHash(), QVariantHash(), item, receiver, std::move(result));
	}

	void addCustomItem(bool async, const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item,
			   std::function<void(Group group, Item item)> &&result)
	{
		auto findGroup = async ? &ResourceManager::asyncFindGroup : &ResourceManager::syncFindGroup;
		(this->*findGroup)(categoryId, groupId, [this, groupId, groupAttrs, groupCustomAttrs, item, result](ICategory *icategory, CategoryItemPtr categoryItem, Group group) {
			if (!icategory || !categoryItem || !categoryItem->m_category) {
				pls_invoke_safe(result, nullptr, nullptr);
				return;
			} else if (group && !group->m_custom) {
				pls_invoke_safe(result, nullptr, nullptr);
				return;
			} else if (!group) {
				group = addCustomGroup(icategory, categoryItem, groupId, groupAttrs, groupCustomAttrs);
			}

			qsizetype pos = -1;
			bool archive = false;
			icategory->getCustomItemExtras(pos, archive, pls_ptr(this), item);
			if (!item->m_archive && archive)
				item->m_archive = true;
			item->m_groups.append(group->m_groupId);
			group->insertItem(pos, item->id());

			categoryItem->saveCustomItems();
			categoryItem->saveCustomGroups();

			pls_invoke_safe(result, group, item);
		});
	}
	std::pair<Group, Item> addCustomItem(const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item) override
	{
		std::pair<Group, Item> result;
		addCustomItem(false, categoryId, groupId, groupAttrs, groupCustomAttrs, item, [&result](Group group, Item item) { result = {group, item}; });
		return result;
	}
	void addCustomItem(const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item,
			   std::function<void(Group group, Item item)> &&result) override
	{
		addCustomItem(true, categoryId, groupId, groupAttrs, groupCustomAttrs, item, std::move(result));
	}
	void addCustomItem(const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item, pls::QObjectPtr<QObject> receiver,
			   std::function<void(Group group, Item item)> &&result) override
	{
		addCustomItem(categoryId, groupId, groupAttrs, groupCustomAttrs, item,
			      [receiver, result](Group group, Item item) { pls_async_call(receiver, [group, item, result]() { pls_invoke_safe(result, group, item); }); });
	}
	std::pair<Group, Item> addCustomItem(const QString &categoryId, const QString &groupId, const QString &itemId, const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs) override
	{
		return addCustomItem(categoryId, groupId, QVariantHash(), QVariantHash(), itemId, itemAttrs, itemCustomAttrs);
	}
	void addCustomItem(const QString &categoryId, const QString &groupId, const QString &itemId, const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs,
			   std::function<void(Group group, Item item)> &&result) override
	{
		addCustomItem(categoryId, groupId, QVariantHash(), QVariantHash(), itemId, itemAttrs, itemCustomAttrs, std::move(result));
	}
	void addCustomItem(const QString &categoryId, const QString &groupId, const QString &itemId, const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs,
			   pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) override
	{
		addCustomItem(categoryId, groupId, itemId, itemAttrs, itemCustomAttrs,
			      [receiver, result](Group group, Item item) { pls_async_call(receiver, [group, item, result]() { pls_invoke_safe(result, group, item); }); });
	}
	void addCustomItem(bool async, const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId,
			   const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs, std::function<void(Group group, Item item)> &&result)
	{
		auto findGroup = async ? &ResourceManager::asyncFindGroup : &ResourceManager::syncFindGroup;
		(this->*findGroup)(categoryId, groupId,
				   [this, groupId, groupAttrs, groupCustomAttrs, itemId, itemAttrs, itemCustomAttrs, result](ICategory *icategory, CategoryItemPtr categoryItem, Group group) {
					   if (!icategory || !categoryItem || !categoryItem->m_category) {
						   pls_invoke_safe(result, nullptr, nullptr);
						   return;
					   } else if (group && !group->m_custom) {
						   pls_invoke_safe(result, nullptr, nullptr);
						   return;
					   } else if (!group) {
						   group = addCustomGroup(icategory, categoryItem, groupId, groupAttrs, groupCustomAttrs);
					   }

					   auto item = std::make_shared<ItemImpl>(0, itemId, itemAttrs, true);
					   item->m_customAttrs = itemCustomAttrs;
					   item->m_category = categoryItem->m_category;

					   qsizetype pos = -1;
					   icategory->getCustomItemExtras(pos, item->m_archive, pls_ptr(this), item);
					   categoryItem->m_category->m_allItems[itemId] = item;
					   item->m_groups.append(group->m_groupId);
					   group->insertItem(pos, itemId);
					   item->m_uniqueId = icategory->getItemUniqueId(this, item).m_id;
					   item->m_homeDir = icategory->getItemHomeDir(this, item);

					   categoryItem->saveCustomItems();
					   categoryItem->saveCustomGroups();

					   pls_invoke_safe(result, group, item);
				   });
	}
	std::pair<Group, Item> addCustomItem(const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId,
					     const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs) override
	{
		std::pair<Group, Item> result;
		addCustomItem(false, categoryId, groupId, groupAttrs, groupCustomAttrs, itemId, itemAttrs, itemCustomAttrs, [&result](Group group, Item item) { result = {group, item}; });
		return result;
	}
	void addCustomItem(const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId,
			   const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs, std::function<void(Group group, Item item)> &&result) override
	{
		addCustomItem(true, categoryId, groupId, groupAttrs, groupCustomAttrs, itemId, itemAttrs, itemCustomAttrs, std::move(result));
	}
	void addCustomItem(const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId,
			   const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) override
	{
		addCustomItem(categoryId, groupId, groupAttrs, groupCustomAttrs, itemId, itemAttrs, itemCustomAttrs,
			      [receiver, result](Group group, Item item) { pls_async_call(receiver, [group, item, result]() { pls_invoke_safe(result, group, item); }); });
	}
	void removeCustomItems(const QString &categoryId, const QString &groupId, bool async = true) override
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [this, groupId](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (!icategory || !categoryItem || !categoryItem->m_category) {
				return;
			} else if (auto groupImpl = categoryItem->findGroup(groupId); groupImpl && groupImpl->m_custom) {
				for (auto itemId : groupImpl->m_items) {
					if (auto itemImpl = categoryItem->m_category->findItem(itemId); itemImpl) {
						itemImpl->m_groups.removeOne(groupImpl->m_groupId);
						if (itemImpl->m_groups.isEmpty()) {
							categoryItem->m_category->m_allItems.erase(itemId);
							categoryItem->removeUsedItem(itemId);
						}
					}
				}

				groupImpl->m_items.clear();

				categoryItem->saveCustomItems();
				categoryItem->saveCustomGroups();
			}
		});
	}
	void removeCustomItem(const QString &categoryId, const QString &groupId, const QString &itemId, bool async = true) override
	{
		auto findGroup = async ? &ResourceManager::asyncFindGroup : &ResourceManager::syncFindGroup;
		(this->*findGroup)(categoryId, groupId, [this, groupId, itemId](ICategory *icategory, CategoryItemPtr categoryItem, Group group) {
			if (!icategory || !categoryItem || !categoryItem->m_category || !group || !group->m_items.contains(itemId)) {
				return;
			} else if (auto itemImpl = categoryItem->findItem(itemId); itemImpl) {
				group->m_items.removeOne(itemId);
				itemImpl->m_groups.removeOne(group->m_groupId);
				if (itemImpl->m_groups.isEmpty()) {
					categoryItem->m_category->m_allItems.erase(itemId);
					categoryItem->removeUsedItem(itemId);
				}

				categoryItem->saveCustomItems();
				categoryItem->saveCustomGroups();
			}
		});
	}
	void removeCustomItem(const QString &categoryId, const QString &itemId, bool async = true) override
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [this, itemId](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (!icategory || !categoryItem || !categoryItem->m_category) {
				return;
			} else if (auto itemImpl = categoryItem->findItem(itemId); itemImpl && itemImpl->m_custom) {
				categoryItem->m_category->m_allItems.erase(itemId);
				categoryItem->removeUsedItem(itemId);
				for (const auto &groupId : itemImpl->m_groups)
					if (auto groupImpl = categoryItem->m_category->findGroup(groupId); groupImpl)
						groupImpl->m_items.removeOne(itemId);

				categoryItem->saveCustomItems();
				categoryItem->saveCustomGroups();
			}
		});
	}

	// recent items
	void useItem(const QString &categoryId, const QString &group, Item item, bool async = true) override
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [this, group, item](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (icategory && categoryItem) {
				categoryItem->useItem(group, item, icategory->useMaxCount(this));
			}
		});
	}
	std::list<Item> getUsedItems(const QString &categoryId, const QString &group) const override
	{
		std::list<Item> result;
		syncFindCategory(categoryId, [group, &result](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (icategory && categoryItem) {
				result = categoryItem->usedItems(group);
			}
		});
		return result;
	}
	void getUsedItems(const QString &categoryId, const QString &group, std::function<void(const std::list<Item> &items)> &&result) const override
	{
		asyncFindCategory(categoryId, [group, result](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (icategory && categoryItem) {
				pls_invoke_safe(result, categoryItem->usedItems(group));
			}
		});
	}
	void getUsedItems(const QString &categoryId, const QString &group, pls::QObjectPtr<QObject> receiver, std::function<void(const std::list<Item> &items)> &&result) const override
	{
		getUsedItems(categoryId, group, [receiver, result](const std::list<Item> &items) { pls_async_call(receiver, [items, result]() { pls_invoke_safe(result, items); }); });
	}
	void listenUsedItems(const QString &categoryId, const QString &group, std::function<void(const QString &group, const std::list<Item> &items)> &&result) const override {}
	void removeUsedItem(const QString &categoryId, const QString &group, Item item, bool async = true) override
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [group, item](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (icategory && categoryItem) {
				categoryItem->removeUsedItem(group, item);
			}
		});
	}
	void removeAllUsedItems(const QString &categoryId, const QString &group, bool async = true) override
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [group](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (icategory && categoryItem) {
				categoryItem->removeUsedItems(group);
			}
		});
	}
	void removeAllUsedItems(const QString &categoryId, bool async = true) override
	{
		auto findCategory = async ? &ResourceManager::asyncFindCategory : &ResourceManager::syncFindCategory;
		(this->*findCategory)(categoryId, [](ICategory *icategory, CategoryItemPtr categoryItem) {
			if (icategory && categoryItem) {
				categoryItem->removeUsedItems();
			}
		});
	}

private:
	template<typename ResultCb> void downloadCategoryDotJson(ResultCb resultCb)
	{
		if (!hasICategories()) {
			PLS_LOGEX(PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({}), "No category id registered.");
			resultCb(false);
		} else if (hasCategoryItems()) {
			PLS_INFO(LIBRESOURCE_MODULE, "category.json loaded");
			resultCb(true);
		} else if (startDownload(CATEGORY_DOT_JSON, resultCb, false)) {
			PLS_INFO(LIBRESOURCE_MODULE, "downloading category.json");
			getDownloader()->download(UrlAndHowSave()
							  .keyPrefix(QStringLiteral("libresource/resources/"))                                  //
							  .names({CATEGORY_DOT_JSON})                                                           //
							  .url(CATEGORY_URL)                                                                    //
							  .hmacKey(PRISM_PC_HMAC_KEY)                                                           //
							  .forceDownload(true)                                                                  //
							  .defaultFilePath(QStringLiteral(":/Configs/resource/DefaultResources/category.json")) //
							  .saveDir(rsm::getAppDataPath())                                                       //
							  .fileName(CATEGORY_DOT_JSON)
							  .encryptJson(true),
						  [this, resultCb](const DownloadResult &result) {
							  auto ok = result.isOk();
							  PLS_LOGEX(ok ? PLS_LOG_INFO : PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({}), "download category.json %s", ok ? "ok" : "failed");
							  setState(CATEGORY_DOT_JSON, ok ? State::Ok : State::Failed);
							  if (!result.hasFilePath()) {
								  asyncCall(resultCb, false);
								  return;
							  }

							  PLS_INFO(LIBRESOURCE_MODULE, "load category.json");
							  if (QJsonArray categories; result.json(categories)) {
								  PLS_INFO(LIBRESOURCE_MODULE, "load category.json ok");
								  dynamicAddCategoryItem(categories); // use for add local category item
								  for (auto cv : categories)
									  addCategoryItem(std::make_shared<CategoryItem>(cv.toObject()));
								  asyncCall(resultCb, true);
							  } else {
								  PLS_LOGEX(PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({}), "load category.json failed");
								  asyncCall(resultCb, false);
							  }
						  });
		}
	}
	template<typename ResultCb> void downloadCategoryItemDotJson(CategoryItemPtr categoryItem, ICategory *icategory, ResultCb resultCb)
	{
		if (categoryItem->isLoaded()) {
			PLS_INFO(LIBRESOURCE_MODULE, "%s.json is loaded", categoryItem->m_categoryIdUtf8.constData());
			resultCb(true);
		} else if (QString fileName = categoryItem->m_categoryId + QStringLiteral(".json"); startDownload(fileName, resultCb, false)) {
			PLS_INFO(LIBRESOURCE_MODULE, "downloading %s", fileName.toUtf8().constData());

			UrlAndHowSave urlAndHowSave;
			if (auto defaultJsonPath = icategory->defaultJsonPath(this); !defaultJsonPath.isEmpty())
				urlAndHowSave.defaultFilePath(defaultJsonPath);

			getDownloader()->download(urlAndHowSave //
							  .keyPrefix(QStringLiteral("libresource/resources/") + categoryItem->m_categoryId + '/')
							  .names({fileName})
							  .url(categoryItem->m_resourceUrl)
							  .forceDownload(true)
							  .saveDir(rsm::getAppDataPath(categoryItem->m_categoryId))
							  .neloField("categoryId", categoryItem->m_categoryIdUtf8)
							  .fileName(fileName)
							  .encryptJson(true),
						  [this, icategory, categoryItem, fileName, resultCb, urlAndHowSave](const DownloadResult &result) {
							  auto ok = result.isOk();
							  auto nfields = urlAndHowSave->nfields();
							  PLS_LOGEX(ok ? PLS_LOG_INFO : PLS_LOG_ERROR, LIBRESOURCE_MODULE, nfields, "download categoryId json %s %s", fileName.toUtf8().constData(),
								    ok ? "ok" : "failed");
							  setState(fileName, ok ? State::Ok : State::Failed);
							  asyncCall([this, icategory, categoryItem, result]() { icategory->jsonDownloaded(this, result); });
							  if (!result.hasFilePath()) {
								  PLS_LOGEX(PLS_LOG_ERROR, LIBRESOURCE_MODULE, nfields, "categoryId json %s not found", fileName.toUtf8().constData());
								  asyncCall(resultCb, false);
								  return;
							  }

							  PLS_INFO(LIBRESOURCE_MODULE, "load %s", fileName.toUtf8().constData());
							  if (QJsonObject obj; loadCategoryItemDotJson(obj, result, fileName)) {
								  PLS_INFO(LIBRESOURCE_MODULE, "load %s ok", fileName.toUtf8().constData());
								  asyncCall([this, icategory, categoryItem, category = from(icategory, obj, categoryItem->m_categoryId), resultCb]() {
									  categoryItem->setCategory(this, icategory, category);
									  icategory->jsonLoaded(this, category);
									  resultCb(true);
								  });
							  } else {
								  PLS_LOGEX(PLS_LOG_ERROR, LIBRESOURCE_MODULE, nfields, "load categoryId json %s failed", fileName.toUtf8().constData());
								  asyncCall(resultCb, false);
							  }
						  });
		}
	}

	template<typename ResultCb> void downloadCategoryItem(const QString &categoryId, ICategory *icategory, bool manualDownload, ResultCb resultCb)
	{
		if (auto categoryItem = pls_get_value(m_categoryItems, categoryId, nullptr); categoryItem) {
			downloadCategoryItem(categoryItem, icategory, manualDownload, resultCb);
		} else {
			resultCb(false);
		}
	}
	template<typename ResultCb>
	void downloadCategoryItem(CategoryItemPtr categoryItem, ICategory *icategory, bool manualDownload, ResultCb resultCb, std::shared_ptr<DownloadAllContext> dac = nullptr)
	{
		if (startDownload(categoryItem->m_categoryId, resultCb, false)) {
			downloadCategoryItemDotJson(categoryItem, icategory, [this, icategory, categoryItem, manualDownload, resultCb, dac](bool ok) { //
				if (dac) {
					dac->remove(categoryItem->m_categoryId);
				}

				if (!ok) {
					setState(categoryItem->m_categoryId, State::Failed, resultCb, false);
					return;
				}

				PLS_INFO(LIBRESOURCE_MODULE, "downloading category [%s]'s groups and items", categoryItem->m_categoryIdUtf8.constData());
				auto context = DownloadingContext::create(categoryItem->m_category, [this, categoryItem, icategory, resultCb](bool ok1, const std::list<DownloadResult> &) {
					PLS_LOGEX(ok1 ? PLS_LOG_INFO : PLS_LOG_ERROR, LIBRESOURCE_MODULE, neloFields({{"categoryId", categoryItem->m_categoryIdUtf8.constData()}}),
						  "category [%s] downloaded %s", categoryItem->m_categoryIdUtf8.constData(), ok1 ? "ok" : "failed");
					setState(categoryItem->m_categoryId, ok1 ? State::Ok : State::Failed);
					icategory->allDownload(this, ok1);
					resultCb(ok1);
					DownloadingContext::removeCategoryAll(categoryItem->m_categoryId, ok1);
				});
				pls_for_each(categoryItem->m_category->m_groups, [this, categoryItem, icategory, manualDownload, context](const auto &groupId) {
					downloadCategoryItemGroup(categoryItem, icategory, groupId, manualDownload, [context, groupId](bool ok2) { context->removeGroup(groupId, ok2); });
				});
				pls_for_each(categoryItem->m_category->m_items, [this, categoryItem, icategory, manualDownload, context](const auto &itemId) {
					downloadCategoryItemItem(categoryItem, icategory, itemId, manualDownload, [context, itemId](bool ok3) { context->removeItem(itemId, ok3); });
				});
			});
		} else if (dac) {
			dac->remove(categoryItem->m_categoryId);
		}
	}
	template<typename ResultCb> void downloadCategoryItemGroup(CategoryItemPtr categoryItem, ICategory *icategory, const QString &groupId, bool manualDownload, ResultCb resultCb)
	{
		if (auto group = categoryItem->findGroup(groupId); group) {
			downloadCategoryItemGroup(categoryItem, icategory, group, manualDownload, resultCb);
		} else {
			resultCb(false);
		}
	}
	template<typename ResultCb> void downloadCategoryItemGroup(CategoryItemPtr categoryItem, ICategory *icategory, GroupImplPtr groupImpl, bool manualDownload, ResultCb resultCb)
	{
		auto key = categoryItem->m_categoryId + '/' + groupImpl->m_groupId;

		if (!icategory->groupNeedDownload(this, groupImpl)) {
			PLS_INFO(LIBRESOURCE_MODULE, "category [%s] group [%s] does not need to be downloaded", categoryItem->m_categoryIdUtf8.constData(), groupImpl->m_groupIdUtf8.constData());
			setState(key, State::Initialized);
			resultCb(true);
			return;
		} else if (!manualDownload && icategory->groupManualDownload(this, groupImpl)) {
			PLS_INFO(LIBRESOURCE_MODULE, "category [%s] group [%s] need to be downloaded manually", categoryItem->m_categoryIdUtf8.constData(), groupImpl->m_groupIdUtf8.constData());
			setState(key, State::Failed);
			resultCb(false);
			return;
		}

		PLS_INFO(LIBRESOURCE_MODULE, "downloading category [%s] group [%s]", categoryItem->m_categoryIdUtf8.constData(), groupImpl->m_groupIdUtf8.constData());

		auto context = DownloadingContext::create(groupImpl, [this, icategory, key, groupImpl, resultCb](bool ok, const std::list<DownloadResult> &results) {
			setState(key, ok ? State::Ok : State::Failed);
			icategory->groupDownloaded(this, groupImpl, ok, results);
			resultCb(ok);
			DownloadingContext::removeGroupAll(groupImpl->m_groupId, ok, results);
		});

		std::list<UrlAndHowSave> urlAndHowSaves;
		icategory->getGroupDownloadUrlAndHowSaves(this, urlAndHowSaves, groupImpl);
		initUrlAndHowSaves(urlAndHowSaves, categoryItem, "groupId", groupImpl);
		groupImpl->m_urlAndHowSaves = urlAndHowSaves;

		pls_for_each(groupImpl->m_items, [this, categoryItem, icategory, manualDownload, context](const auto &itemId) {
			downloadCategoryItemItem(categoryItem, icategory, itemId, manualDownload, [context, itemId](bool ok) { context->removeItem(itemId, ok); });
		});

		if (auto state = categoryItem->checkGroup(this, icategory, groupImpl, true); state == State::Ok) {
			context->removeGroup(groupImpl->m_groupId, true, {});
			return;
		} else if (state == State::Downloading) {
			return;
		}

		getDownloader()->download(groupImpl->m_urlAndHowSaves, [this, categoryItem, groupImpl, context](const std::list<DownloadResult> &results) {
			asyncCall([categoryItem, groupImpl, results, context]() {
				auto ok = std::ranges::all_of(results, [](const DownloadResult &result) { return result.isOk(); });
				categoryItem->endDownloadGroup(groupImpl, ok, results);
				context->removeGroup(groupImpl->m_groupId, ok, results);
			});
		});
	}
	template<typename ResultCb> void downloadCategoryItemItem(CategoryItemPtr categoryItem, ICategory *icategory, const QString &itemId, bool manualDownload, ResultCb resultCb)
	{
		if (auto item = categoryItem->findItem(itemId); item) {
			downloadCategoryItemItem(categoryItem, icategory, item, manualDownload, resultCb);
		} else {
			resultCb(false);
		}
	}
	template<typename ResultCb> void downloadCategoryItemItem(CategoryItemPtr categoryItem, ICategory *icategory, ItemImplPtr itemImpl, bool manualDownload, ResultCb resultCb)
	{
		auto key = categoryItem->m_categoryId + '/' + itemImpl->m_itemId;

		if (!icategory->itemNeedDownload(this, itemImpl)) {
			PLS_INFO(LIBRESOURCE_MODULE, "category [%s] item [%s] does not need to be downloaded", categoryItem->m_categoryIdUtf8.constData(), itemImpl->m_itemIdUtf8.constData());
			setState(key, State::Initialized);
			resultCb(true);
			return;
		} else if (!manualDownload && icategory->itemManualDownload(this, itemImpl)) {
			PLS_INFO(LIBRESOURCE_MODULE, "category [%s] item [%s] need to be downloaded manually", categoryItem->m_categoryIdUtf8.constData(), itemImpl->m_itemIdUtf8.constData());
			setState(key, State::Failed);
			resultCb(false);
			return;
		}

		PLS_INFO(LIBRESOURCE_MODULE, "downloading category [%s] item [%s]", categoryItem->m_categoryIdUtf8.constData(), itemImpl->m_itemIdUtf8.constData());

		auto context = DownloadingContext::create(itemImpl, [this, icategory, key, itemImpl, resultCb](bool ok, const std::list<DownloadResult> &results) {
			setState(key, ok ? State::Ok : State::Failed);
			icategory->itemDownloaded(this, itemImpl, ok, results);
			resultCb(ok);
			DownloadingContext::removeItemAll(itemImpl->m_itemId, ok, results);
		});

		std::list<UrlAndHowSave> urlAndHowSaves;
		icategory->getItemDownloadUrlAndHowSaves(this, urlAndHowSaves, itemImpl);
		initUrlAndHowSaves(urlAndHowSaves, categoryItem, "itemId", itemImpl);
		itemImpl->m_urlAndHowSaves = urlAndHowSaves;

		if (auto state = categoryItem->checkItem(this, icategory, itemImpl, true); state == State::Ok) {
			context->removeItem(itemImpl->m_itemId, true, {});
			return;
		} else if (state == State::Downloading) {
			return;
		}

		getDownloader()->download(itemImpl->m_urlAndHowSaves, nullptr, [this, icategory, categoryItem, itemImpl, context](const std::list<DownloadResult> &results) {
			asyncCall([this, icategory, categoryItem, itemImpl, results, context]() {
				auto ok = std::ranges::all_of(results, [](const DownloadResult &result) { return result.isOk(); });
				categoryItem->endDownloadItem(itemImpl, ok, results);
				context->removeItem(itemImpl->m_itemId, ok, results);
			});
		});
	}

	CategoryImplPtr from(ICategory *icategory, const QJsonObject &obj, const QString &categoryId) const
	{
		auto category = std::make_shared<CategoryImpl>(obj[QStringLiteral("version")].toInt(), categoryId, obj);
		parseGroups(icategory, category, obj[QStringLiteral("group")].toArray());
		parseItems(icategory, category, obj[QStringLiteral("items")].toArray());
		return category;
	}
	void parseGroups(ICategory *icategory, CategoryImplPtr category, const QJsonArray &groups) const
	{
		for (auto gv : groups) {
			auto go = gv.toObject();
			auto groupId = go[QStringLiteral("groupId")].toString();
			auto items = go.take(QStringLiteral("items")).toArray();
			auto group = std::make_shared<GroupImpl>(groupId, go.toVariantHash());
			if (!icategory->groupNeedLoad(pls_ptr(this), group)) {
				PLS_INFO(LIBRESOURCE_MODULE, "category [%s] group [%s] does not need to be loaded", category->m_categoryIdUtf8.constData(), groupId.toUtf8().constData());
				continue;
			}

			category->m_groups.append(groupId);
			group->m_category = category;
			category->m_allGroups[groupId] = group;
			group->m_uniqueId = icategory->getGroupUniqueId(pls_ptr(this), group).m_id;
			group->m_homeDir = icategory->getGroupHomeDir(pls_ptr(this), group);

			for (auto iv : items) {
				auto io = iv.toObject();
				auto itemId = io[QStringLiteral("itemId")].toString();
				if (auto item = category->findItem(itemId); item) {
					item->m_groups.append(groupId);
					group->addItem(itemId);
					continue;
				}

				auto item = std::make_shared<ItemImpl>(io[QStringLiteral("version")].toInt(), itemId, io.toVariantHash());
				if (!icategory->itemNeedLoad(pls_ptr(this), item)) {
					PLS_INFO(LIBRESOURCE_MODULE, "category [%s] item [%s] does not need to be loaded", category->m_categoryIdUtf8.constData(), itemId.toUtf8().constData());
					continue;
				}

				item->m_category = category;
				item->m_groups.append(groupId);
				category->m_allItems[itemId] = item;
				group->addItem(itemId);
				item->m_uniqueId = icategory->getItemUniqueId(pls_ptr(this), item).m_id;
				item->m_homeDir = icategory->getItemHomeDir(pls_ptr(this), item);
			}
		}
	}
	void parseItems(ICategory *icategory, CategoryImplPtr category, const QJsonArray &items) const
	{
		for (auto iv : items) {
			auto io = iv.toObject();
			auto itemId = io[QStringLiteral("itemId")].toString();
			category->m_items.append(itemId);
			if (category->m_allItems.contains(itemId))
				continue;

			auto item = std::make_shared<ItemImpl>(io[QStringLiteral("version")].toInt(), itemId, io.toVariantHash());
			if (!icategory->itemNeedLoad(pls_ptr(this), item)) {
				PLS_INFO(LIBRESOURCE_MODULE, "category [%s] item [%s] does not need to be loaded", category->m_categoryIdUtf8.constData(), itemId.toUtf8().constData());
				continue;
			}

			item->m_category = category;
			category->m_allItems[itemId] = item;
			item->m_uniqueId = icategory->getItemUniqueId(pls_ptr(this), item).m_id;
			item->m_homeDir = icategory->getItemHomeDir(pls_ptr(this), item);
		}
	}

	template<typename ImplPtr> void initUrlAndHowSaves(std::list<UrlAndHowSave> &urlAndHowSaves, CategoryItemPtr categoryItem, const QByteArray &idName, ImplPtr impl) const
	{
		for (auto &urlAndHowSave : urlAndHowSaves) {
			urlAndHowSave
				.keyPrefix(QStringLiteral("libresource/resources/") + categoryItem->m_categoryId + '/' + impl->id() + '/') //
				.emptyUrlState(State::Ok)
				.neloField("categoryId", categoryItem->m_categoryIdUtf8)
				.neloField(idName, impl->idUtf8());
			if (urlAndHowSave->m_names && !urlAndHowSave->m_url)
				urlAndHowSave->m_url = impl->url(urlAndHowSave->m_names.value());
			if (!urlAndHowSave->m_saveDir)
				urlAndHowSave->m_saveDir = getHomeDir(categoryItem->m_categoryId, impl);
		}
	}
	template<typename ImplPtr> QString getHomeDir(const QString &categoryId, ImplPtr impl) const
	{
		if (impl->m_homeDir.isEmpty())
			return rsm::getAppDataPath(categoryId + '/' + impl->id());
		return impl->m_homeDir;
	}
	bool loadCategoryItemDotJson(QJsonObject &obj, const DownloadResult &result, const QString &fileName) const
	{
		if (result.m_pathFrom == PathFrom::UseDefault) {
			return result.json(obj);
		} else if (auto defaultFilePath = result.m_urlAndHowSave.defaultFilePath(); defaultFilePath.isEmpty()) {
			return result.json(obj);
		} else if (QJsonObject defobj; !readJson(defobj, result.m_urlAndHowSave->m_defaultFileIsEncrypted, defaultFilePath)) {
			return result.json(obj);
		} else if (QJsonObject dlobj; result.json(dlobj) && (dlobj[QStringLiteral("version")].toInt() >= defobj[QStringLiteral("version")].toInt())) {
			obj = dlobj;
			return true;
		} else {
			PLS_WARN(LIBRESOURCE_MODULE, "%s json version less default version, use default json, file name: %s", (result.m_pathFrom == PathFrom::Downloaded) ? "downloaded" : "cached",
				 fileName.toUtf8().constData());
			obj = defobj;
			return true;
		}
	}
	bool isValidCategoryItem(const QJsonObject &obj) const
	{
		return (obj.contains(QStringLiteral("categoryId")) && obj.contains(QStringLiteral("categoryUniqueKey")) && obj.contains(QStringLiteral("fallbackUrl")) &&
			obj.contains(QStringLiteral("resourceUrl")) && obj.contains(QStringLiteral("version")));
	}
	void dynamicAddCategoryItem(QJsonArray &categories) const
	{
		if (auto json = pls_prism_get_qsetting_value(QStringLiteral("ExtraCategorieItems")).toString(); json.isEmpty()) {
			return;
		} else if (QJsonDocument doc; !pls_parse_json(doc, json.toUtf8())) {
			return;
		} else if (doc.isArray()) {
			for (auto i : doc.array()) {
				if (auto obj = i.toObject(); isValidCategoryItem(obj)) {
					categories.append(obj);
				}
			}
		} else if (doc.isObject()) {
			if (auto obj = doc.object(); isValidCategoryItem(obj)) {
				categories.append(obj);
			}
		}
	}
};

class Initializer {
public:
	Initializer()
	{
		pls_qapp_construct_add_cb([]() {
			Downloader::instance()->initialize();
			ResourceManager::instance()->initialize();
		});
		pls_qapp_deconstruct_add_cb([]() {
			Downloader::instance()->cleanup();
			ResourceManager::instance()->cleanup();
		});
	}

	static Initializer *initializer() { return &pls::Initializer<Initializer>::s_initializer; }
};

LIRESOURCE_API IDownloader *getDownloader()
{
	return Downloader::instance();
}
LIRESOURCE_API IResourceManager *getResourceManager()
{
	return ResourceManager::instance();
}
LIRESOURCE_API void downloadAll(const std::function<void()> &complete)
{
	ResourceManager::instance()->downloadAll(complete);
}
LIRESOURCE_API QString getDataPath(const QString &subpath)
{
#if defined(Q_OS_WIN)
	if (!subpath.isEmpty())
		return pls_get_app_dir() + QStringLiteral("/../../data/prism-studio/resources/") + subpath;
	return pls_get_app_dir() + QStringLiteral("/../../data/prism-studio/resources");
#elif defined(Q_OS_MACOS)
	if (!subpath.isEmpty())
		return pls_get_app_resource_dir() + QStringLiteral("/data/prism-studio/resources/") + subpath;
	return pls_get_app_resource_dir() + QStringLiteral("/data/prism-studio/resources");
#endif
}
LIRESOURCE_API QString getAppDataPath(const QString &subpath)
{
	if (!subpath.isEmpty())
		return pls_get_app_data_dir_pn(QStringLiteral("resources/")) + subpath;
	return pls_get_app_data_dir_pn(QStringLiteral("resources"));
}
// base dir PRISMLiveStudio/resources/{categoryId}/xxx
LIRESOURCE_API QString getItemPath(const QString &categoryId, const UniqueId &uniqueId, const QString &subpath)
{
	if (auto item = getResourceManager()->getItem(categoryId, uniqueId); item)
		return item.filePath(subpath);
	return {};
}

#if defined(Q_OS_WIN)
static bool closeZip(HZIP zip)
{
	if (zip)
		return CloseZip(zip) == ZR_OK;
	return false;
}
#endif
static QString getDstDir(QFileInfo &src, const QString &dstdir)
{
	return dstdir.isEmpty() ? src.path() : dstdir;
}
static QString getDstFile(QFileInfo &src, const QString &dstfile)
{
	return dstfile.isEmpty() ? src.completeBaseName() : dstfile;
}
static bool _zip(const QString &srcpath, const QString &dstdir, const QString &dstfile, QString *error)
{
	if (QFileInfo src(srcpath); !src.exists()) {
		pls_set_value(error, QStringLiteral("source file or directory does not exist"));
		return false;
	} else if (QString _dstdir = getDstDir(src, dstdir); !pls_mkdir(_dstdir)) {
		pls_set_value(error, QStringLiteral("failed to create the destination file's directory"));
		return false;
	} else if (QString dstpath = _dstdir + '/' + getDstFile(src, dstfile), rmerror; !pls_remove_file(dstpath, &rmerror)) {
		pls_set_value(error, QStringLiteral("failed to remove the destination file, reason: ") + rmerror);
		return false;
	}
#if defined(Q_OS_WIN)
	else if (pls::AutoHandle<HZIP, decltype(&closeZip)> zip(CreateZip(dstpath.toStdWString().c_str(), nullptr), closeZip); !zip.handle()) {
		pls_set_value(error, QStringLiteral("failed to create the destination zip file"));
		return false;
	} else if (src.isDir())
		return pls_enum_dir(srcpath, [zip = zip.handle()](const QString &reldir, const QFileInfo &fi) {
			if (QString relpath = reldir.isEmpty() ? fi.fileName() : (reldir + '/' + fi.fileName()); fi.isDir())
				return ZR_OK == ZipAddFolder(zip, relpath.toStdWString().c_str());
			else
				return ZR_OK == ZipAdd(zip, relpath.toStdWString().c_str(), fi.filePath().toStdWString().c_str());
		});
	else
		return ZR_OK == ZipAdd(zip.handle(), src.fileName().toStdWString().c_str(), srcpath.toStdWString().c_str());
#elif defined(Q_OS_MACOS)
	else
		return pls_zipFile(dstpath, src.fileName(), src.path());
#else
	return false;
#endif
}

// srcpath: source file or directory path
// dstpath: dest zip file path
LIRESOURCE_API bool zip(const QString &srcpath, const QString &dstpath, QString *error)
{
	QFileInfo fi(dstpath);
	return _zip(srcpath, fi.path(), fi.fileName(), error);
}
// srcpath: source file or directory path
// dstdir: dest zip file dir
// dstfile: dest zip file name, empty to use srcpath file name
LIRESOURCE_API bool zip(const QString &srcpath, const QString &dstdir, const QString &dstfile, QString *error)
{
	return _zip(srcpath, dstdir, dstfile, error);
}

static bool _unzip(const QString &srcpath, const QString &dstdir, bool removeZip, QStringList *files, QString *error)
{
	if (QFileInfo src(srcpath); !src.isFile()) {
		pls_set_value(error, QStringLiteral("source zip file not exist"));
		return false;
	} else if (QString _dstdir = getDstDir(src, dstdir); !pls_mkdir(_dstdir)) {
		pls_set_value(error, QStringLiteral("failed to create the destination directory"));
		return false;
	}
#if defined(Q_OS_WIN)
	else if (pls::AutoHandle<HZIP, decltype(&closeZip)> zip(OpenZip(srcpath.toStdWString().c_str(), nullptr), closeZip); !zip.handle()) {
		pls_set_value(error, QStringLiteral("failed to open the source zip file"));
		return false;
	} else if (ZR_OK != SetUnzipBaseDir(zip.handle(), _dstdir.toStdWString().c_str()))
		return false;
	else if (ZIPENTRY zipEntry; ZR_OK != GetZipItem(zip.handle(), -1, &zipEntry))
		return false;
	else {
		for (int i = 0, count = zipEntry.index; i < count; ++i) {
			if (ZR_OK != GetZipItem(zip.handle(), i, &zipEntry))
				return false;
			else if (!wcsncmp(zipEntry.name, L"__MACOSX", 8))
				continue;
			UnzipItem(zip.handle(), i, zipEntry.name);
			if (files)
				files->append(_dstdir + '/' + (QString::fromWCharArray(zipEntry.name)));
		}
	}
#elif defined(Q_OS_MACOS)
	else if (!pls_unZipFile(_dstdir, srcpath)) {
		return false;
	}
#endif

	if (removeZip) {
		return pls_remove_file(srcpath);
	}
	return true;
}

// srcpath: zip file path
// dstdir: dest file dir
// removeZip: remove zip file?
LIRESOURCE_API bool unzip(const QString &srcpath, const QString &dstdir, bool removeZip, QString *error)
{
	return _unzip(srcpath, dstdir, removeZip, nullptr, error);
}
// files: output zip file list
// srcpath: zip file path
// dstdir: dest file dir
// removeZip: remove zip file?
LIRESOURCE_API bool unzip(QStringList &files, const QString &srcpath, const QString &dstdir, bool removeZip, QString *error)
{
	return _unzip(srcpath, dstdir, removeZip, &files, error);
}

}
}
