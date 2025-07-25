#if !defined(_PRISM_COMMON_LIBRESOURCE_LIBRESOURCE_H)
#define _PRISM_COMMON_LIBRESOURCE_LIBRESOURCE_H

#include <chrono>
#include <list>
#include <memory>
#include <qvariant.h>
#include <libhttp-client.h>
#include <libutils-api.h>

#include <qglobal.h>

#if defined(LIRESOURCE_LIB)
#define LIRESOURCE_API Q_DECL_EXPORT
#else
#define LIRESOURCE_API Q_DECL_IMPORT
#endif

namespace pls {
namespace rsm {

enum class FileName {
	Uuid = 1, // default
	FromUrl,
	Spec
};
enum class State { Initialized, Downloading, Ok, Failed };
enum class PathFrom { Invalid, Downloaded, UseCache, UseDefault };

struct UrlAndHowSave;
struct Category;
struct Group;
struct Item;

struct UrlAndHowSaveImpl;
struct CategoryImpl;
struct GroupImpl;
struct ItemImpl;

struct IResourceManager;

using UrlAndHowSaveImplPtr = std::shared_ptr<UrlAndHowSaveImpl>;
using CategoryImplPtr = std::shared_ptr<CategoryImpl>;
using CategoryImplWeakPtr = std::weak_ptr<CategoryImpl>;
using GroupImplPtr = std::shared_ptr<GroupImpl>;
using ItemImplPtr = std::shared_ptr<ItemImpl>;

using FnCheck = std::function<bool(const UrlAndHowSave &urlAndHowSave, const QString &filePath)>;
using FnDecompress = std::function<bool(const UrlAndHowSave &urlAndHowSave, const QString &filePath)>;
using FnDone = std::function<void(const UrlAndHowSave &urlAndHowSave, bool ok, const QString &filePath, PathFrom pathFrom, bool decompressOk)>;

// clang-format off
#define PLS_RSM_CATEGORY(Category) \
	static Category *instance() \
	{ return &pls::Initializer<pls::rsm::ICategoryImplRegister<Category>>::s_initializer.m_impl; }

#define PLS_RSM_CID_LIBRARY			QStringLiteral("library")
#define PLS_RSM_CID_MUSIC			QStringLiteral("music")
#define PLS_RSM_CID_REACTION		QStringLiteral("reaction")
#define PLS_RSM_CID_SCENE_TEMPLATES QStringLiteral("scene_templates")
#define PLS_RSM_CID_TEXT_TEMPLATE	QStringLiteral("textTemplatePC")
#define PLS_RSM_CID_VIRTUAL_BG		QStringLiteral("virtual_bg")
#define PLS_RSM_CID_CHAT_BG			QStringLiteral("chatBackground")
#define PLS_RSM_UID_LIBRARY_POLICY_PC pls::rsm::UniqueId(QStringLiteral("library_Policy_PC"))

#define PLS_RSM_getLibraryPolicyPC_Path(subpath) pls::rsm::getItemPath(PLS_RSM_CID_LIBRARY, PLS_RSM_UID_LIBRARY_POLICY_PC, subpath)
// clang-format on

struct UniqueId {
	QString m_id;
	UniqueId() = default;
	explicit UniqueId(const QString &id) : m_id(id) {}
};

struct LIRESOURCE_API UrlAndHowSave {
	UrlAndHowSaveImplPtr m_impl;

	UrlAndHowSave();
	UrlAndHowSave(UrlAndHowSaveImplPtr urlAndHowSaveImpl);

	UrlAndHowSaveImpl *operator->() const;

	QString keyPrefix() const;
	UrlAndHowSave &keyPrefix(const QString &keyPrefix);

	QStringList names() const;
	UrlAndHowSave &names(const QStringList &names);

	QUrl url() const;
	UrlAndHowSave &url(const QUrl &url);

	QByteArray hmacKey() const;
	UrlAndHowSave &hmacKey(const QByteArray &hmacKey);

	State emptyUrlState() const;
	UrlAndHowSave &emptyUrlState(State emptyUrlState);

	bool noCache() const;
	UrlAndHowSave &noCache(bool noCache);

	bool forceDownload() const;
	UrlAndHowSave &forceDownload(bool forceDownload);

	QString saveDir() const;
	UrlAndHowSave &saveDir(const QString &saveDir);

	QString fileName() const;
	UrlAndHowSave &fileName(const QString &fileName);
	UrlAndHowSave &fileName(FileName fileName);

	QString filePath() const;
	UrlAndHowSave &filePath(const QString &filePath);

	QString defaultFilePath() const;
	UrlAndHowSave &defaultFilePath(const QString &defaultFilePath, bool encrypted = false);

	bool encryptJson() const;
	UrlAndHowSave &encryptJson(bool encryptJson);

	QString savedFilePath() const;

	bool needDecompress() const;
	UrlAndHowSave &needDecompress(bool needDecompress);

	// default 30000 30s
	int timeout() const;
	UrlAndHowSave &timeout(int timeout);

	// seconds default not expired
	UrlAndHowSave &expired(std::chrono::seconds expired);
	UrlAndHowSave &expired(std::chrono::minutes expired);
	UrlAndHowSave &expired(std::chrono::hours expired);

	QVariant customAttr(const QString &name, const QVariant &defval = QVariant()) const;
	UrlAndHowSave &customAttr(const QString &name, const QVariant &value);

	QByteArray neloField(const QByteArray &name) const;
	UrlAndHowSave &neloField(const QByteArray &name, const QByteArray &value);

	UrlAndHowSave &check(FnCheck &&check);
	UrlAndHowSave &decompress(FnDecompress &&decompress);
	UrlAndHowSave &done(FnDone &&done);
};

struct LIRESOURCE_API Category {
	CategoryImplPtr m_impl;

	Category();
	Category(std::nullptr_t);
	Category(CategoryImplPtr categoryImpl);

	explicit operator bool() const;
	Category &operator=(std::nullptr_t);
	Category &operator=(CategoryImplPtr categoryImpl);
	Category &operator=(Category category);
	CategoryImpl *operator->() const;

	const QJsonObject &json() const;
	int version() const;
	QString categoryId() const;
	std::list<Group> groups() const;
	std::list<Item> items() const;

	Group getGroup(const QString &groupId) const;
	Group getGroup(const UniqueId &uniqueId) const;
	Item getItem(const QString &itemId) const;
	Item getItem(const UniqueId &uniqueId) const;
};
struct LIRESOURCE_API Group {
	GroupImplPtr m_impl;

	Group();
	Group(std::nullptr_t);
	Group(GroupImplPtr groupImpl);

	explicit operator bool() const;
	Group &operator=(std::nullptr_t);
	Group &operator=(GroupImplPtr impl);
	Group &operator=(Group group);
	GroupImpl *operator->() const;

	QString groupId() const;

	bool isCustom() const;
	State state() const;

	QString dir() const;
	QString filePath(const QString &subpath) const;

	int itemCount() const;
	Item item(const QString &itemId) const;
	std::list<Item> items() const;

	QVariant attr(const QString &name, const QVariant &defval = QVariant()) const;
	QVariant attr(const QStringList &names, const QVariant &defval = QVariant()) const;

	bool isUniqueId(const UniqueId &uniqueId) const;
	UniqueId uniqueId() const;
	void setUniqueId(const QString &uniqueId);
	void setUniqueId(const UniqueId &uniqueId);

	QVariant customAttr(const QString &name, const QVariant &defval = QVariant()) const;
	void customAttr(const QString &name, const QVariant &value);

	std::list<UrlAndHowSave> urlAndHowSaves() const;

	size_t fileCount() const;
	QString file(size_t index) const;
	QString file(const QString &name) const;
	QString file(const QStringList &names) const;
};
struct LIRESOURCE_API Item {
	ItemImplPtr m_impl;

	Item();
	Item(std::nullptr_t);
	Item(ItemImplPtr itemImpl);

	explicit operator bool() const;
	bool operator==(Item item) const;
	Item &operator=(std::nullptr_t);
	Item &operator=(ItemImplPtr itemImpl);
	Item &operator=(Item item);
	ItemImpl *operator->() const;

	QString itemId() const;
	int version() const;

	bool isCustom() const;
	State state() const;

	QString dir() const;
	QString filePath(const QString &subpath) const;

	std::list<Group> groups() const;

	QVariant attr(const QString &name, const QVariant &defval = QVariant()) const;
	QVariant attr(const QStringList &names, const QVariant &defval = QVariant()) const;

	bool isUniqueId(const UniqueId &uniqueId) const;
	UniqueId uniqueId() const;
	void setUniqueId(const QString &uniqueId);
	void setUniqueId(const UniqueId &uniqueId);

	QVariant customAttr(const QString &name, const QVariant &defval = QVariant()) const;
	void customAttr(const QString &name, const QVariant &value);

	std::list<UrlAndHowSave> urlAndHowSaves() const;

	size_t fileCount() const;
	QString file(size_t index) const;
	QString file(const QString &name) const;
	QString file(const QStringList &names) const;
};

struct LIRESOURCE_API DownloadResult {
	UrlAndHowSave m_urlAndHowSave;
	State m_state = State::Initialized;
	PathFrom m_pathFrom = PathFrom::Invalid;
	bool m_decompressOk = false;
	bool m_timeout = false;
	int m_statusCode = 0;

	DownloadResult(const UrlAndHowSave &urlAndHowSave, State state);
	DownloadResult(const UrlAndHowSave &urlAndHowSave, State state, PathFrom pathFrom, bool decompressOk);
	DownloadResult(const UrlAndHowSave &urlAndHowSave, State state, bool timeout, int statusCode);

	bool isOk() const { return m_state == State::Ok; }
	bool hasFilePath() const { return m_pathFrom != PathFrom::Invalid; }
	bool decompressOk() const { return m_decompressOk; }
	bool timeout() const { return m_timeout; }
	int statusCode() const { return m_statusCode; }
	QString filePath() const { return m_urlAndHowSave.savedFilePath(); }

	bool json(QJsonDocument &doc, QString *error = nullptr) const;
	bool json(QJsonArray &array, QString *error = nullptr) const;
	bool json(QVariantList &list, QString *error = nullptr) const;
	bool json(QJsonObject &object, QString *error = nullptr) const;
	bool json(QVariantMap &map, QString *error = nullptr) const;
	bool json(QVariantHash &hash, QString *error = nullptr) const;
};
struct LIRESOURCE_API IDownloader {
	using ResultCb = std::function<void(const DownloadResult &result)>;
	using ResultsCb = std::function<void(const std::list<DownloadResult> &results)>;

	IDownloader() = default;
	virtual ~IDownloader() = default;

	virtual void download(UrlAndHowSave urlAndHowSave, ResultCb &&result) = 0;
	virtual void download(UrlAndHowSave urlAndHowSave, pls::QObjectPtr<QObject> receiver, ResultCb &&result) = 0;
	virtual void download(const std::list<UrlAndHowSave> &urlAndHowSaves, ResultsCb &&results) = 0;
	virtual void download(const std::list<UrlAndHowSave> &urlAndHowSaves, pls::QObjectPtr<QObject> receiver, ResultsCb &&results) = 0;
};

class LIRESOURCE_API ICategory {
protected:
	ICategory() = default;
	virtual ~ICategory() = default;

public:
	const char *moduleName() const;

	void download() const;                            // download category resource
	void downloadGroup(const QString &groupId) const; // download category group resource
	void downloadItem(const QString &itemId) const;   // download category item resource

	State getJsonState() const;
	State getState() const;
	State getGroupState(const QString &groupId) const;
	State getItemState(const QString &itemId) const;

	Category getCategory() const;
	void getCategory(std::function<void(Category category)> &&result) const;
	void getCategory(pls::QObjectPtr<QObject> receiver, std::function<void(Category category)> &&result) const;

	std::list<Group> getGroups() const;
	void getGroups(std::function<void(const std::list<Group> &groups)> &&result) const;
	void getGroups(pls::QObjectPtr<QObject> receiver, std::function<void(const std::list<Group> &groups)> &&result) const;
	std::list<Item> getItems() const;
	void getItems(std::function<void(const std::list<Item> &items)> &&result) const;
	void getItems(pls::QObjectPtr<QObject> receiver, std::function<void(const std::list<Item> &items)> &&result) const;

	Group getGroup(const QString &groupId) const;
	void getGroup(const QString &groupId, std::function<void(Group group)> &&result) const;
	void getGroup(const QString &groupId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group)> &&result) const;

	std::pair<Group, Item> getGroupItem(const QString &groupId, const QString &itemId) const;
	void getGroupItem(const QString &groupId, const QString &itemId, std::function<void(Group group, Item item)> &&result) const;
	void getGroupItem(const QString &groupId, const QString &itemId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) const;

	Item getItem(const QString &itemId) const;
	void getItem(const QString &itemId, std::function<void(Item item)> &&result) const;
	void getItem(const QString &itemId, pls::QObjectPtr<QObject> receiver, std::function<void(Item item)> &&result) const;

	Group getGroup(const UniqueId &uniqueId) const;
	void getGroup(const UniqueId &uniqueId, std::function<void(Group group)> &&result) const;
	void getGroup(const UniqueId &uniqueId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group)> &&result) const;

	Item getItem(const UniqueId &uniqueId) const;
	void getItem(const UniqueId &uniqueId, std::function<void(Item item)> &&result) const;
	void getItem(const UniqueId &uniqueId, pls::QObjectPtr<QObject> receiver, std::function<void(Item item)> &&result) const;

	std::pair<Category, bool> check() const;
	void check(std::function<void(Category category, bool ok)> &&result) const;
	void check(pls::QObjectPtr<QObject> receiver, std::function<void(Category category, bool ok)> &&result) const;
	std::pair<Group, bool> checkGroup(const QString &groupId) const;
	void checkGroup(const QString &groupId, std::function<void(Group group, bool ok)> &&result) const;
	void checkGroup(const QString &groupId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, bool ok)> &&result) const;
	std::pair<Item, bool> checkItem(const QString &itemId) const;
	void checkItem(const QString &itemId, std::function<void(Item item, bool ok)> &&result) const;
	void checkItem(const QString &itemId, pls::QObjectPtr<QObject> receiver, std::function<void(Item item, bool ok)> &&result) const;

	// custom group/items
	std::pair<Group, Item> addCustomItem(const QString &groupId, Item item) const;
	void addCustomItem(const QString &groupId, Item item, std::function<void(Group group, Item item)> &&result) const;
	void addCustomItem(const QString &groupId, Item item, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) const;
	std::pair<Group, Item> addCustomItem(const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item) const;
	void addCustomItem(const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item, std::function<void(Group group, Item item)> &&result) const;
	void addCustomItem(const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item, pls::QObjectPtr<QObject> receiver,
			   std::function<void(Group group, Item item)> &&result) const;
	std::pair<Group, Item> addCustomItem(const QString &groupId, const QString &itemId, const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs) const;
	void addCustomItem(const QString &groupId, const QString &itemId, const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs,
			   std::function<void(Group group, Item item)> &&result) const;
	void addCustomItem(const QString &groupId, const QString &itemId, const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs, pls::QObjectPtr<QObject> receiver,
			   std::function<void(Group group, Item item)> &&result) const;
	std::pair<Group, Item> addCustomItem(const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId, const QVariantHash &itemAttrs,
					     const QVariantHash &itemCustomAttrs) const;
	void addCustomItem(const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId, const QVariantHash &itemAttrs,
			   const QVariantHash &itemCustomAttrs, std::function<void(Group group, Item item)> &&result) const;
	void addCustomItem(const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId, const QVariantHash &itemAttrs,
			   const QVariantHash &itemCustomAttrs, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) const;
	void removeCustomItems(const QString &groupId, bool async = true) const;
	void removeCustomItem(const QString &groupId, const QString &itemId, bool async = true) const;
	void removeCustomItem(const QString &itemId, bool async = true) const;

	// recent items
	void useItem(const QString &group, Item item, bool async = true) const;
	std::list<Item> getUsedItems(const QString &group) const;
	void getUsedItems(const QString &group, std::function<void(const std::list<Item> &items)> &&result) const;
	void getUsedItems(const QString &group, pls::QObjectPtr<QObject> receiver, std::function<void(const std::list<Item> &items)> &&result) const;
	void listenUsedItems(const QString &group, std::function<void(const QString &group, const std::list<Item> &items)> &&result) const;
	void removeUsedItem(const QString &group, Item item, bool async = true) const;
	void removeAllUsedItems(const QString &group, bool async = true) const;
	void removeAllUsedItems(bool async = true) const;

	virtual QString categoryId(IResourceManager *mgr) const = 0;
	virtual QString defaultJsonPath(IResourceManager *mgr) const;

	virtual size_t useMaxCount(IResourceManager *mgr) const;

	// json downloaded
	virtual void jsonDownloaded(IResourceManager *mgr, const DownloadResult &result);

	// check
	virtual bool groupNeedLoad(IResourceManager *mgr, Group group) const;
	virtual bool itemNeedLoad(IResourceManager *mgr, Item item) const;

	// download ok, load json
	// download failed, load default json
	virtual void jsonLoaded(IResourceManager *mgr, Category category);

	// check manual download
	virtual bool groupManualDownload(IResourceManager *mgr, Group group) const;
	virtual bool itemManualDownload(IResourceManager *mgr, Item item) const;

	virtual bool groupNeedDownload(IResourceManager *mgr, Group group) const;
	virtual bool checkGroup(IResourceManager *mgr, Group group) const;
	virtual UniqueId getGroupUniqueId(IResourceManager *mgr, Group group) const;
	virtual QString getGroupHomeDir(IResourceManager *mgr, Group group) const;
	virtual void getGroupDownloadUrlAndHowSaves(IResourceManager *mgr, std::list<UrlAndHowSave> &urlAndHowSaves, Group group) const;
	virtual void groupDownloaded(IResourceManager *mgr, Group group, bool ok, const std::list<DownloadResult> &results) const;
	virtual void getCustomGroupExtras(qsizetype &pos, bool &archive, IResourceManager *mgr, Group group) const;

	virtual bool itemNeedDownload(IResourceManager *mgr, Item item) const;
	virtual bool checkItem(IResourceManager *mgr, Item item) const;
	virtual UniqueId getItemUniqueId(IResourceManager *mgr, Item item) const;
	virtual QString getItemHomeDir(IResourceManager *mgr, Item item) const;
	virtual void getItemDownloadUrlAndHowSaves(IResourceManager *mgr, std::list<UrlAndHowSave> &urlAndHowSaves, Item item) const;
	virtual void itemDownloaded(IResourceManager *mgr, Item item, bool ok, const std::list<DownloadResult> &results) const;
	virtual void getCustomItemExtras(qsizetype &pos, bool &archive, IResourceManager *mgr, Item item) const;

	virtual void allDownload(IResourceManager *mgr, bool ok);
};
struct LIRESOURCE_API IResourceManager {
	IResourceManager() = default;
	virtual ~IResourceManager() = default;

	virtual const char *moduleName(const QByteArray &submodule) const = 0;

	virtual void registerCategory(ICategory *icategory) = 0;   // don't call
	virtual void unregisterCategory(ICategory *icategory) = 0; // don't call

	virtual void downloadCategory(const QString &categoryId) = 0;                              // download category resource
	virtual void downloadCategoryGroup(const QString &categoryId, const QString &groupId) = 0; // download category group resource
	virtual void downloadCategoryItem(const QString &categoryId, const QString &itemId) = 0;   // download category item resource

	virtual State getJsonState(const QString &categoryId) const = 0;
	virtual State getCategoryState(const QString &categoryId) const = 0;
	virtual State getGroupState(const QString &categoryId, const QString &groupId) const = 0;
	virtual State getItemState(const QString &categoryId, const QString &itemId) const = 0;

	virtual Category getCategory(const QString &categoryId) const = 0;
	virtual void getCategory(const QString &categoryId, std::function<void(Category category)> &&result) const = 0;
	virtual void getCategory(const QString &categoryId, pls::QObjectPtr<QObject> receiver, std::function<void(Category category)> &&result) const = 0;

	virtual std::list<Group> getGroups(const QString &categoryId) const = 0;
	virtual void getGroups(const QString &categoryId, std::function<void(const std::list<Group> &groups)> &&result) const = 0;
	virtual void getGroups(const QString &categoryId, pls::QObjectPtr<QObject> receiver, std::function<void(const std::list<Group> &groups)> &&result) const = 0;
	virtual std::list<Item> getItems(const QString &categoryId) const = 0;
	virtual void getItems(const QString &categoryId, std::function<void(const std::list<Item> &items)> &&result) const = 0;
	virtual void getItems(const QString &categoryId, pls::QObjectPtr<QObject> receiver, std::function<void(const std::list<Item> &items)> &&result) const = 0;

	virtual Group getGroup(const QString &categoryId, const QString &groupId) const = 0;
	virtual void getGroup(const QString &categoryId, const QString &groupId, std::function<void(Group group)> &&result) const = 0;
	virtual void getGroup(const QString &categoryId, const QString &groupId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group)> &&result) const = 0;

	virtual std::pair<Group, Item> getGroupItem(const QString &categoryId, const QString &groupId, const QString &itemId) const = 0;
	virtual void getGroupItem(const QString &categoryId, const QString &groupId, const QString &itemId, std::function<void(Group group, Item item)> &&result) const = 0;
	virtual void getGroupItem(const QString &categoryId, const QString &groupId, const QString &itemId, pls::QObjectPtr<QObject> receiver,
				  std::function<void(Group group, Item item)> &&result) const = 0;

	virtual Item getItem(const QString &categoryId, const QString &itemId) const = 0;
	virtual void getItem(const QString &categoryId, const QString &itemId, std::function<void(Item item)> &&result) const = 0;
	virtual void getItem(const QString &categoryId, const QString &itemId, pls::QObjectPtr<QObject> receiver, std::function<void(Item item)> &&result) const = 0;

	virtual Group getGroup(const QString &categoryId, const UniqueId &uniqueId) const = 0;
	virtual void getGroup(const QString &categoryId, const UniqueId &uniqueId, std::function<void(Group group)> &&result) const = 0;
	virtual void getGroup(const QString &categoryId, const UniqueId &uniqueId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group)> &&result) const = 0;

	virtual Item getItem(const QString &categoryId, const UniqueId &uniqueId) const = 0;
	virtual void getItem(const QString &categoryId, const UniqueId &uniqueId, std::function<void(Item item)> &&result) const = 0;
	virtual void getItem(const QString &categoryId, const UniqueId &uniqueId, pls::QObjectPtr<QObject> receiver, std::function<void(Item item)> &&result) const = 0;

	virtual std::pair<Category, bool> checkCategory(const QString &categoryId) const = 0;
	virtual void checkCategory(const QString &categoryId, std::function<void(Category category, bool ok)> &&result) const = 0;
	virtual void checkCategory(const QString &categoryId, pls::QObjectPtr<QObject> receiver, std::function<void(Category category, bool ok)> &&result) const = 0;
	virtual std::pair<Group, bool> checkGroup(const QString &categoryId, const QString &groupId) const = 0;
	virtual void checkGroup(const QString &categoryId, const QString &groupId, std::function<void(Group group, bool ok)> &&result) const = 0;
	virtual void checkGroup(const QString &categoryId, const QString &groupId, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, bool ok)> &&result) const = 0;
	virtual std::pair<Item, bool> checkItem(const QString &categoryId, const QString &itemId) const = 0;
	virtual void checkItem(const QString &categoryId, const QString &itemId, std::function<void(Item item, bool ok)> &&result) const = 0;
	virtual void checkItem(const QString &categoryId, const QString &itemId, pls::QObjectPtr<QObject> receiver, std::function<void(Item item, bool ok)> &&result) const = 0;

	// custom group/items
	virtual std::pair<Group, Item> addCustomItem(const QString &categoryId, const QString &groupId, Item item) = 0;
	virtual void addCustomItem(const QString &categoryId, const QString &groupId, Item item, std::function<void(Group group, Item item)> &&result) = 0;
	virtual void addCustomItem(const QString &categoryId, const QString &groupId, Item item, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) = 0;
	virtual std::pair<Group, Item> addCustomItem(const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item) = 0;
	virtual void addCustomItem(const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item,
				   std::function<void(Group group, Item item)> &&result) = 0;
	virtual void addCustomItem(const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, Item item,
				   pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) = 0;
	virtual std::pair<Group, Item> addCustomItem(const QString &categoryId, const QString &groupId, const QString &itemId, const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs) = 0;
	virtual void addCustomItem(const QString &categoryId, const QString &groupId, const QString &itemId, const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs,
				   std::function<void(Group group, Item item)> &&result) = 0;
	virtual void addCustomItem(const QString &categoryId, const QString &groupId, const QString &itemId, const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs,
				   pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) = 0;
	virtual std::pair<Group, Item> addCustomItem(const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId,
						     const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs) = 0;
	virtual void addCustomItem(const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId,
				   const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs, std::function<void(Group group, Item item)> &&result) = 0;
	virtual void addCustomItem(const QString &categoryId, const QString &groupId, const QVariantHash &groupAttrs, const QVariantHash &groupCustomAttrs, const QString &itemId,
				   const QVariantHash &itemAttrs, const QVariantHash &itemCustomAttrs, pls::QObjectPtr<QObject> receiver, std::function<void(Group group, Item item)> &&result) = 0;
	virtual void removeCustomItems(const QString &categoryId, const QString &groupId, bool async = true) = 0;
	virtual void removeCustomItem(const QString &categoryId, const QString &groupId, const QString &itemId, bool async = true) = 0;
	virtual void removeCustomItem(const QString &categoryId, const QString &itemId, bool async = true) = 0;

	// recent items
	virtual void useItem(const QString &categoryId, const QString &group, Item item, bool async = true) = 0;
	virtual std::list<Item> getUsedItems(const QString &categoryId, const QString &group) const = 0;
	virtual void getUsedItems(const QString &categoryId, const QString &group, std::function<void(const std::list<Item> &items)> &&result) const = 0;
	virtual void getUsedItems(const QString &categoryId, const QString &group, pls::QObjectPtr<QObject> receiver, std::function<void(const std::list<Item> &items)> &&result) const = 0;
	virtual void listenUsedItems(const QString &categoryId, const QString &group, std::function<void(const QString &group, const std::list<Item> &items)> &&result) const = 0;
	virtual void removeUsedItem(const QString &categoryId, const QString &group, Item item, bool async = true) = 0;
	virtual void removeAllUsedItems(const QString &categoryId, const QString &group, bool async = true) = 0;
	virtual void removeAllUsedItems(const QString &categoryId, bool async = true) = 0;
};

LIRESOURCE_API IDownloader *getDownloader();
LIRESOURCE_API IResourceManager *getResourceManager();
LIRESOURCE_API void downloadAll(const std::function<void()> &complete = nullptr);
// base dir data/prism-studio/resources/
LIRESOURCE_API QString getDataPath(const QString &subpath = QString());
// base dir PRISMLiveStudio/resources/
LIRESOURCE_API QString getAppDataPath(const QString &subpath = QString());
// base dir PRISMLiveStudio/resources/{categoryId}/xxx
LIRESOURCE_API QString getItemPath(const QString &categoryId, const UniqueId &uniqueId, const QString &subpath = QString());
// srcpath: source file or directory path
// dstpath: dest zip file path
LIRESOURCE_API bool zip(const QString &srcpath, const QString &dstpath, QString *error = nullptr);
// srcpath: source file or directory path
// dstdir: dest zip file dir, empty to use srcpath file dir
// dstfile: dest zip file name, empty to use srcpath file name
LIRESOURCE_API bool zip(const QString &srcpath, const QString &dstdir, const QString &dstfile, QString *error = nullptr);
// srcpath: zip file path
// dstdir: dest file dir
// removeZip: remove zip file?
LIRESOURCE_API bool unzip(const QString &srcpath, const QString &dstdir, bool removeZip = false, QString *error = nullptr);
// files: output zip file list
// srcpath: zip file path
// dstdir: dest file dir
// removeZip: remove zip file?
LIRESOURCE_API bool unzip(QStringList &files, const QString &srcpath, const QString &dstdir, bool removeZip = false, QString *error = nullptr);

template<typename ICategoryImpl> struct ICategoryImplRegister {
	ICategoryImpl m_impl;
	ICategoryImplRegister() { getResourceManager()->registerCategory(&m_impl); }
};
}
}

#endif // _PRISM_COMMON_LIBRESOURCE_LIBRESOURCE_H
