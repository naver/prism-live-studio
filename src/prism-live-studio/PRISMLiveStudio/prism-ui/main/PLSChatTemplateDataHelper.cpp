#include "PLSChatTemplateDataHelper.h"
#include "pls-common-define.hpp"
#include "frontend-api.h"
#include "liblog.h"
#include "log/log.h"
#include <qbuttongroup.h>
#include <qdir.h>
#include "utils-api.h"
#include "PLSApp.h"
#include "PLSPropertiesExtraUI.hpp"
#include "window-basic-main.hpp"
#include <qjsonobject.h>
#include <QRandomGenerator>
#include "PLSNameDialog.hpp"
#include "qt-wrappers.hpp"
#include "PLSBasic.h"

#define CUSTOMTHEME_MIN_ID 1100
constexpr auto PRISM_CHAT_WIDGET = "chat-widget";

using namespace common;

static QStringList colorList{"#755074", "#336E67", "#363E4B", "#475A5E", "#725B5A", "#474566", "#986E96", "#4B9A90", "#5C6A7E",
			     "#647E85", "#937675", "#656193", "#523751", "#224D48", "#232932", "#313F42", "#513F3F", "#2F2D43"};
static QString getCurrentShortLocale()
{
	auto lang = pls_get_current_language_short_str();
	if (lang != "ko")
		lang = "en";
	return lang;
}
struct CategoryChatWidget : public pls::rsm::ICategory {
	PLS_RSM_CATEGORY(CategoryChatWidget)
	QString categoryId(pls::rsm::IResourceManager *mgr) const override { return PLS_RSM_CID_CHAT_BG; }

	void getItemDownloadUrlAndHowSaves(pls::rsm::IResourceManager *mgr, std::list<pls::rsm::UrlAndHowSave> &urlAndHowSaves, pls::rsm::Item item) const override
	{
		PLS_INFO(moduleName(), "getItemDownloadUrlAndHowSaves chat widget background %s", item.itemId().toUtf8().constData());
		auto lang = pls_prism_get_locale() != "ko-KR" ? "en" : "ko";
		urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave()
						 .names({
							 QStringLiteral("properties"),
							 QStringLiteral("thumbnail"),
							 lang,
							 QStringLiteral("url"),
						 })
						 .fileName(pls::rsm::FileName::FromUrl));
	}
	void allDownload(pls::rsm::IResourceManager *mgr, bool ok) override
	{
		PLS_INFO(moduleName(), "allDownload chat widget background");
		pls_async_call_mt([this, ok, mgr]() {
			pls_check_app_exiting();
			if (PLSBasic::instance() && PLSBasic::instance()->GetPropertiesWindow()) {
				static_cast<PLSChatTemplateDataHelper *>(PLSChatTemplateDataHelper::instance())->allDownloadedChatBKRes(mgr->getCategory(PRISM_CHAT_WIDGET), ok);
			}
		});
	}
};
PLSChatTemplateDataHelper::PLSChatTemplateDataHelper()
{
	initPLSChatTemplateData();
}

void PLSChatTemplateDataHelper::initPLSChatTemplateData()
{
	auto lang = getCurrentShortLocale();

	auto parseTemplateJson = [this, lang](const QString groupId, const QJsonArray &templateJson) {
		m_templateInfos.insert(groupId, {});
		QVector<PLSChatTemplateData> datas;
		for (auto templateData : templateJson) {
			PLSChatTemplateData chatTemplate;
			chatTemplate.id = templateData.toObject().value("itemId").toInt();
			chatTemplate.name = templateData.toObject().value("title").toObject().value(lang).toString();
			QString iconName = QString("%1_%2.png").arg(lang).arg(chatTemplate.id);
			chatTemplate.resourcePath = PLS_RSM_getLibraryPolicyPC_Path(QStringLiteral("Library_Policy_PC/")) + QString("images/chat_source/%1").arg(iconName);
			chatTemplate.resourceBackupPath = ":/resource/images/chat-template-source/" + iconName;
			datas.append(chatTemplate);
			m_chatTemplateObjs.insert(chatTemplate.id, templateData.toObject());
			m_chatTemplateNames.insert(chatTemplate.name);
		}
		m_templateInfos.insert(groupId, datas);
	};
	QString chatSourceJsonPath = PLS_RSM_getLibraryPolicyPC_Path(QStringLiteral("Library_Policy_PC/chatv2source/")) + QStringLiteral("chatv2source.json");
	QString chatSourceLocalJsonPath = ":/Configs/resource/DefaultResources/chatv2source.json";
	QJsonObject chatSourceObj, chatSourceLocalObj;
	pls_read_json(chatSourceObj, chatSourceJsonPath);
	pls_read_json(chatSourceLocalObj, chatSourceLocalJsonPath);
	auto currentChatSourceObj = chatSourceObj.value("version").toInt() >= chatSourceLocalObj.value("version").toInt() ? chatSourceObj : chatSourceLocalObj;
	auto defaultTemplates = currentChatSourceObj.value("default_template").toArray();
	m_defaultId = defaultTemplates.first().toObject().value("itemId").toInt();
	parseTemplateJson("theme", defaultTemplates);
	m_templateTabs.insert(0, "theme");
	m_templateTabs.insert(1, "my");

	initChatDefaultFamilyData(currentChatSourceObj.value("default_font_list").toArray(), lang);
}
void PLSChatTemplateDataHelper::initChatDefaultFamilyData(const QJsonArray &fontArray, const QString &langShort)
{
	auto insertDefaultFamily = [this](int index, const PLSChatDefaultFamily &newChatFamilyData) {
		auto size = m_chatDefaultfamilies.size();
		int position = -1;
		for (int i = 0; i < size; ++i) {
			auto maxIndex = m_chatDefaultfamilies[i].index;
			if (index < maxIndex) {
				position = i;
				break;
			} else {
				continue;
			}
		}
		if (position == -1) {
			m_chatDefaultfamilies.append(newChatFamilyData);
		} else {
			m_chatDefaultfamilies.insert(position, newChatFamilyData);
		}
	};
	for (auto jsonValue : fontArray) {
		PLSChatDefaultFamily defaultFamily;
		auto fontObj = jsonValue.toObject();
		auto familyObj = fontObj.value("web_qt_family_name").toObject();
		defaultFamily.qtFamilyText = familyObj.value("qt_family").toString();
		defaultFamily.webFamilyText = familyObj.value("web_family").toString();
		defaultFamily.fontWeight = familyObj.value("weight").toInt();
		defaultFamily.fontSize = familyObj.value(langShort).toObject().value("font_size").toInt();
		auto uiJsonObj = fontObj.value("ui_name").toObject().value(langShort).toObject();
		defaultFamily.uiFamilyText = uiJsonObj.value("name").toString();
		auto index = uiJsonObj.value("index").toInt();
		defaultFamily.index = index;
		defaultFamily.buttonWidth = uiJsonObj.value("width").toInt();
		defaultFamily.buttonResourceStr = uiJsonObj.value("resource_name").toString();
		insertDefaultFamily(index, defaultFamily);
	}
}
void PLSChatTemplateDataHelper::initTemplateGroup()
{
	m_templateButtons.clear();

	for (auto templateStr : m_templateInfos.keys()) {
		auto buttonGroup = pls_new<QButtonGroup>();
		buttonGroup->setExclusive(true);
		m_templateButtons.insert(templateStr, buttonGroup);
	}
}

QString PLSChatTemplateDataHelper::getDefaultTitle()
{
	auto customTemplateSize = m_chatTemplateNames.size();
	auto name = QString("My Theme - %1").arg(1);
	for (int index = 1; index <= customTemplateSize; ++index) {
		name = QString("My Theme - %1").arg(index);
		if (m_chatTemplateNames.find(name) == m_chatTemplateNames.end()) {
			return name;
		}
	}
	return name;
}

bool PLSChatTemplateDataHelper::isPaidBkTemplate(int id)
{

	auto items = CategoryChatWidget::instance()->getItems();
	for (auto item : items) {
		auto itemId = item.attr({"properties", "itemNo"}).toInt();
		if (id == itemId)
			return item.attr("paidFlag").toBool();
	}

	return false;
}
void PLSChatTemplateDataHelper::initchatBKTemplateButtons()
{
	if (!m_chatBKTemplateGroup) {
		m_chatBKTemplateGroup = pls_new<QButtonGroup>();
	}
	auto findExistButton = [this](int id) -> bool { return m_chatBKTemplateGroup->button(id) != nullptr; };
	auto items = CategoryChatWidget::instance()->getItems();
	for (auto item : items) {
		if (auto versionLimit = item.attr({"properties", "versionLimit"}).toString(); !versionLimit.isEmpty()) {
			if (auto matched = pls_check_version(versionLimit.toUtf8(), QVersionNumber(pls_get_prism_version_major(), pls_get_prism_version_minor(), pls_get_prism_version_patch()));
			    !matched) {
				PLS_WARN(PRISM_CHAT_WIDGET, "item: %s, version: %s is a wrong format", qUtf8Printable(item.itemId()), qUtf8Printable(versionLimit));
				continue;
			} else if (!matched.value()) {
				PLS_INFO(PRISM_CHAT_WIDGET, "item: %s, version: %s cannot be supported", qUtf8Printable(item.itemId()), qUtf8Printable(versionLimit));
				continue;
			}
		}
		auto id = item.attr({"properties", "itemNo"}).toInt();
		if (!findExistButton(id)) {
			auto name = item.attr({"properties", "item_name", IS_KR() ? "ko" : "en"}).toString();
			auto isPaid = item.attr("paidFlag").toBool();
			auto *button = pls_new<ChatTemplate>(m_chatBKTemplateGroup, id, name, item.file(0), false, "", isPaid);
			m_chatBKTemplateGroup->addButton(button, id);
		} else {
			auto button = dynamic_cast<ChatTemplate *>(m_chatBKTemplateGroup->button(id));
			if (button) {
				button->updateTemplateRes(item.file(0));
			}
		}
	}
}

void PLSChatTemplateDataHelper::allDownloadedChatBKRes(pls::rsm::Category category, bool ok)
{
	if (ok) {
		PLS_INFO(PRISM_CHAT_WIDGET, "chat widget background update success");
		loadechatWidgetBKRes(ok, true);
	} else {
		PLSChatTemplateDataHelper::raisePropertyView();
		auto ret = pls_show_download_failed_alert(nullptr);
		if (ret == PLSAlertView::Button::Ok) {
			PLS_INFO(PRISM_CHAT_WIDGET, "chat widget background: User select retry download.");
			if (!category) {
				pls_async_call_mt([this, category]() { allDownloadedChatBKRes(category, false); });
			} else {
				pls_async_call_mt([this]() { CategoryChatWidget::instance()->download(); });
			}
		} else {
			PLS_INFO(PRISM_CHAT_WIDGET, "chat widget background: User select quit download.");
			loadechatWidgetBKRes(true, false);
		}
	}
}

void PLSChatTemplateDataHelper::raisePropertyView()
{
#ifdef Q_OS_MACOS
	if (auto propertiesView = OBSBasic::Get()->GetPropertiesWindow(); propertiesView)
		pls_bring_mac_window_to_front(propertiesView->winId());
#endif // Q_OS_MACOS
}

void PLSChatTemplateDataHelper::getChatTemplateFromsceneCollection(const QJsonArray &array)
{
	m_needSaveChatTemplates = array;
}

void PLSChatTemplateDataHelper::initTemplateButtons()
{
	auto checkGroup = [this](const QString &key) -> QPointer<QButtonGroup> {
		auto group = m_templateButtons.value(key);
		return group;
	};
	readCutsomPLSChatTemplateData();
	if (m_templateButtons.isEmpty()) {
		initTemplateGroup();
	}

	for (auto templateKey : m_templateInfos.keys()) {
		auto group = checkGroup(templateKey);

		const QVector<PLSChatTemplateData> templateDatas = m_templateInfos.value(templateKey);
		for (int index = 0; index != templateDatas.count(); ++index) {
			auto data = templateDatas.value(index);
			auto name = data.name;
			if (findTemplateGroupStr(data.id).isEmpty())
				auto *button = pls_new<ChatTemplate>(group, data.id, name, data.resourceBackupPath, data.id >= CUSTOMTHEME_MIN_ID, data.backgroundColor, data.isPaid);
		}
	}
	initchatBKTemplateButtons();
}

QMap<int, QString> PLSChatTemplateDataHelper::getTemplateNames()
{
	return m_templateTabs;
}

QButtonGroup *PLSChatTemplateDataHelper::getTemplateButtons(const QString &templateName)
{
	if (m_templateButtons.find(templateName.toLower()) != m_templateButtons.end()) {
		return m_templateButtons.value(templateName);
	}
	return nullptr;
}

void PLSChatTemplateDataHelper::resetButtonStyle()
{
	for (auto templateKey : m_templateButtons.keys()) {
		auto buttonGroup = m_templateButtons.value(templateKey);
		buttonGroup->setExclusive(false);
		for (auto _button : buttonGroup->buttons()) {
			_button->setChecked(false);
		}
		buttonGroup->setExclusive(true);
	}
	if (m_chatBKTemplateGroup) {
		for (auto _button : m_chatBKTemplateGroup->buttons()) {
			_button->setChecked(false);
		}
	}
}
QStringList PLSChatTemplateDataHelper::getTemplateNameList()
{
	return m_templateTabs.values();
}

QString PLSChatTemplateDataHelper::findTemplateGroupStr(const int &templateId)
{
	for (auto templateKey : m_templateButtons.keys()) {
		auto buttonGroup = m_templateButtons.value(templateKey)->buttons();
		for (auto _button : buttonGroup) {
			if (templateId == _button->property("ID").toInt()) {
				return templateKey;
			}
		}
	}
	return QString();
}

int PLSChatTemplateDataHelper::getDefaultTemplateId()
{
	return m_defaultId;
}

void PLSChatTemplateDataHelper::removeParent()
{
	for (auto templateKey : m_templateButtons.keys()) {
		auto buttonGroup = m_templateButtons.value(templateKey)->buttons();
		for (auto _button : buttonGroup) {
			_button->setParent(nullptr);
		}
	}
	if (m_chatBKTemplateGroup) {
		for (auto _button : m_chatBKTemplateGroup->buttons()) {
			_button->setParent(nullptr);
		}
	}
}

QJsonObject PLSChatTemplateDataHelper::defaultTemplateObj(const int itemId)
{
	return m_chatTemplateObjs.value(itemId);
}
void PLSChatTemplateDataHelper::readCutsomPLSChatTemplateData()
{
	m_templateInfos.remove("my");
	auto lang = pls_get_current_language_short_str();
	if (lang != "ko")
		lang = "en";
	QVector<PLSChatTemplateData> datas;
	for (auto templateData : m_needSaveChatTemplates) {
		PLSChatTemplateData chatTemplate;
		chatTemplate.id = templateData.toObject().value("itemId").toInt();
		chatTemplate.name = templateData.toObject().value("title").toString();
		chatTemplate.backgroundColor = templateData.toObject().value("backgroundColor").toString();
		QString iconName = QString("ic_chat_mytheme_%1.svg").arg(chatTemplate.id % 10 + 1);
		chatTemplate.resourcePath = PLS_RSM_getLibraryPolicyPC_Path(QStringLiteral("Library_Policy_PC/")) + QString("images/chat_source/%1").arg(iconName);
		chatTemplate.resourceBackupPath = ":/resource/images/chat-template-source/" + iconName;
		chatTemplate.isPaid = pls_get_attr<bool>(templateData.toObject(), {"properties", "chat_background_template_properties", "Chat.Bk.Color.Template.Paid"});
		datas.append(chatTemplate);
		m_chatTemplateObjs.insert(chatTemplate.id, templateData.toObject());
		m_chatTemplateNames.insert(chatTemplate.name);
	}
	m_templateInfos.insert("my", datas);
	OBSBasic::Get()->SaveProject();
}
QString getMotionStyleString(int style)
{
	QString styleStr;
	if (style == 0) {
		styleStr = "shaking";
	} else if (style == 1) {
		styleStr = "random";
	} else if (style == 2) {
		styleStr = "wave";
	}
	return styleStr;
}
QString getSingleColorStyleStr(const int &style)
{
	switch (style) {
	case 0:
		return "pastel";
	case 1:
		return "bibid";
	default:
		return "pastel";
	}
}

static inline QColor color_from_int(long long val)
{
	return QColor(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff, (val >> 24) & 0xff);
}

void insertJsonValue(QJsonObject &jsonObject, const QStringList &keys, const QVariant &value, int index = 0)
{
	if (index >= keys.size() || keys.isEmpty()) {
		return;
	}
	QString currentKey = keys.at(index);
	if (index == keys.size() - 1) {
		jsonObject[currentKey] = QJsonValue::fromVariant(value);
	} else {
		QJsonObject nestedObject = jsonObject[currentKey].toObject();
		insertJsonValue(nestedObject, keys, value, index + 1);
		jsonObject[currentKey] = nestedObject;
	}
}
bool PLSChatTemplateDataHelper::saveCustomObj(const OBSData &settings, const int itemId)
{
	if (m_needSaveChatTemplates.size() >= 10) {
		PLSAlertView::information(nullptr, QObject::tr("Alert.Title"), QObject::tr("ChatTemplate.Over.Number.Ten"));
		return false;
	}
	auto defaultCustomTitle = getDefaultTitle();
	bool accepted = false;

	for (;;) {
		raisePropertyView();
		accepted = PLSNameDialog::AskForName(nullptr, QObject::tr("ChatTemplate.Rename.Title"), QObject::tr("ChatTemplate.Rename.Content"), m_currentTemplateTitle,
						     QT_UTF8(defaultCustomTitle.toUtf8().constData()));
		if (!accepted)
			break;
		if (m_currentTemplateTitle.empty()) {
			OBSMessageBox::warning(nullptr, QObject::tr("Alert.Title"), QObject::tr("NoNameEntered.Text"));
			continue;
		}
		bool isExist = m_chatTemplateNames.find(m_currentTemplateTitle.c_str()) != m_chatTemplateNames.end();

		if (isExist) {
			OBSMessageBox::warning(nullptr, QObject::tr("Alert.Title"), QObject::tr("ChatTemplate.Rename.Exist"));
			continue;
		}
		break;
	}
	raisePropertyView();
	if (!accepted) {
		return false;
	}
	auto lang = pls_get_current_language_short_str();
	if (lang != "ko") {
		lang = "en";
	}
	auto jsonObj = m_chatTemplateObjs.value(itemId);
	OBSDataAutoRelease ctFontObj = obs_data_get_obj(settings, "Chat.Font");
	OBSDataAutoRelease ctTextColorObj = obs_data_get_obj(settings, "Chat.Text.Color");
	OBSDataAutoRelease ctBkColorObj = obs_data_get_obj(settings, "Chat.Bk.Color");
	OBSDataAutoRelease ctDisplayObj = obs_data_get_obj(settings, "Chat.Display");
	OBSDataAutoRelease ctOptionsObj = obs_data_get_obj(settings, "Chat.Options");
	OBSDataAutoRelease ctMotionObj = obs_data_get_obj(settings, "Chat.Motion");
	auto maxKey = m_chatTemplateObjs.lastKey();
	auto customTemplateCount = maxKey / 100;
	jsonObj["itemId"] = itemId + (++customTemplateCount) * 100;
	jsonObj["title"] = m_currentTemplateTitle.c_str();

	int randomNumber = QRandomGenerator::global()->bounded(0, colorList.size());

	jsonObj["backgroundColor"] = colorList.at(randomNumber);

	insertJsonValue(jsonObj, {"properties", "chat_display_properties", "platform_type", "platform_list"}, obs_data_get_string(ctDisplayObj, "selectPlatformList"));

	insertJsonValue(jsonObj, {"properties", "chat_display_properties", "platform_icon_display", "platform_icon", "is_check"}, obs_data_get_bool(ctDisplayObj, "isCheckPlatformIcon"));

	insertJsonValue(jsonObj, {"properties", "chat_display_properties", "platform_icon_display", "level_icon", "is_check"}, obs_data_get_bool(ctDisplayObj, "isCheckLevelIcon"));
	insertJsonValue(jsonObj, {"properties", "chat_display_properties", "platform_icon_display", "id_icon", "is_check"}, obs_data_get_bool(ctDisplayObj, "isCheckIdIcon"));

	insertJsonValue(jsonObj, {"properties", "chat_options_properties", "chat_sort", "chat_wrap", "is_check"}, obs_data_get_bool(ctOptionsObj, "isCheckChatWrap"));
	insertJsonValue(jsonObj, {"properties", "chat_options_properties", "chat_sort", "chat_wrap", "is_enable"}, obs_data_get_bool(ctOptionsObj, "isEnableChatWrap"));
	insertJsonValue(jsonObj, {"properties", "chat_options_properties", "chat_sort", "chat_align", "is_left"}, obs_data_get_bool(ctOptionsObj, "isCheckLeftChatAlign"));
	insertJsonValue(jsonObj, {"properties", "chat_options_properties", "chat_sort", "chat_align", "is_enable"}, obs_data_get_bool(ctOptionsObj, "isEnableChatAlign"));
	insertJsonValue(jsonObj, {"properties", "chat_options_properties", "chat_disapper_effect", "is_on"}, obs_data_get_bool(ctOptionsObj, "isCheckChatDisapperEffect"));
	insertJsonValue(jsonObj, {"properties", "chat_options_properties", "chat_box_size", "width"}, obs_data_get_int(settings, "chatWidth"));
	insertJsonValue(jsonObj, {"properties", "chat_options_properties", "chat_box_size", "height"}, obs_data_get_int(settings, "chatHeight"));

	insertJsonValue(jsonObj, {"properties", "chat_motion_properties", "motion_style"}, getMotionStyleString(obs_data_get_int(ctMotionObj, "chatMotionStyle")));
	insertJsonValue(jsonObj, {"properties", "chat_motion_properties", "is_check"}, obs_data_get_bool(ctMotionObj, "isCheckChatMotion"));
	insertJsonValue(jsonObj, {"properties", "chat_font_properties", "common_font", "selected_font", "family", lang}, obs_data_get_string(ctFontObj, "chatFontFamily"));
	insertJsonValue(jsonObj, {"properties", "chat_font_properties", "common_font", "selected_font", "style", lang}, obs_data_get_string(ctFontObj, "chatFontStyle"));
	insertJsonValue(jsonObj, {"properties", "chat_font_properties", "font_size", "size_pt"}, obs_data_get_int(ctFontObj, "chatFontSize"));
	insertJsonValue(jsonObj, {"properties", "chat_font_properties", "font_outLine_color", "outLine_size_pt"}, obs_data_get_int(ctFontObj, "chatFontOutlineSize"));
	insertJsonValue(jsonObj, {"properties", "chat_font_properties", "font_outLine_color", "default_color_rgb_code"},
			pls_qint64_to_qcolor(obs_data_get_int(ctFontObj, "chatFontOutlineColor")).name(QColor::HexRgb));

	insertJsonValue(jsonObj, {"properties", "chat_text_color_properties", "common_nick_text_color", "is_single_color"}, obs_data_get_bool(ctTextColorObj, "isCheckNickTextSingleColor"));
	insertJsonValue(jsonObj, {"properties", "chat_text_color_properties", "common_nick_text_color", "default_color_rgb_code"},
			pls_qint64_to_qcolor(obs_data_get_int(ctTextColorObj, "nickTextDefaultColor")).name(QColor::HexRgb));
	insertJsonValue(jsonObj, {"properties", "chat_text_color_properties", "manager_nick_text_color", "default_color_rgb_code"},
			pls_qint64_to_qcolor(obs_data_get_int(ctTextColorObj, "mgrNickTextColor")).name(QColor::HexRgb));
	insertJsonValue(jsonObj, {"properties", "chat_text_color_properties", "subcribe_text_color", "default_color_rgb_code"},
			pls_qint64_to_qcolor(obs_data_get_int(ctTextColorObj, "subcribeTextColor")).name(QColor::HexRgb));
	insertJsonValue(jsonObj, {"properties", "chat_text_color_properties", "message_text_color", "default_color_rgb_code"},
			pls_qint64_to_qcolor(obs_data_get_int(ctTextColorObj, "messageTextColor")).name(QColor::HexRgb));

	insertJsonValue(jsonObj, {"properties", "chat_background_color_properties", "single_background_color", "default_color_rgb_code"},
			pls_qint64_to_qcolor(obs_data_get_int(ctBkColorObj, "chatSingleBkColor")).name(QColor::HexRgb));
	insertJsonValue(jsonObj, {"properties", "chat_background_color_properties", "single_background_color", "color_alpha"}, obs_data_get_int(ctBkColorObj, "chatSingleBkAlpha"));
	insertJsonValue(jsonObj, {"properties", "chat_background_color_properties", "single_background_color", "color_style"},
			getSingleColorStyleStr(obs_data_get_int(ctBkColorObj, "chatSingleColorStyle")));
	insertJsonValue(jsonObj, {"properties", "chat_background_color_properties", "single_background_color", "is_check"}, obs_data_get_bool(ctBkColorObj, "isCheckChatSingleBkColor"));
	insertJsonValue(jsonObj, {"properties", "chat_background_color_properties", "total_background_color", "default_color_rgb_code"},
			pls_qint64_to_qcolor(obs_data_get_int(ctBkColorObj, "chatTotalBkColor")).name(QColor::HexRgb));
	insertJsonValue(jsonObj, {"properties", "chat_background_color_properties", "total_background_color", "color_alpha"}, obs_data_get_int(ctBkColorObj, "chatTotalBkAlpha"));
	insertJsonValue(jsonObj, {"properties", "chat_background_color_properties", "total_background_color", "is_check"}, obs_data_get_bool(ctBkColorObj, "isCheckChatTotalBkColor"));
	insertJsonValue(jsonObj, {"properties", "chat_background_color_properties", "chat_window_alpha"}, obs_data_get_int(ctBkColorObj, "chatWindowAlpha"));

	//insert chat background template json value
	insertJsonValue(jsonObj, {"properties", "chat_background_template_properties", "Chat.Bk.Control"}, obs_data_get_int(settings, "Chat.Bk.Control"));
	insertJsonValue(jsonObj, {"properties", "chat_background_template_properties", "Chat.Bk.Template.List"}, obs_data_get_int(settings, "Chat.Bk.Template.List"));
	insertJsonValue(jsonObj, {"properties", "chat_background_template_properties", "Chat.Bk.Color.Mode"}, obs_data_get_int(settings, "Chat.Bk.Color.Mode"));
	insertJsonValue(jsonObj, {"properties", "chat_background_template_properties", "Chat.Bk.Color.Mode.Default"}, obs_data_get_int(settings, "Chat.Bk.Color.Mode.Default"));
	insertJsonValue(jsonObj, {"properties", "chat_background_template_properties", "Chat.Bk.Color.Mode.Custom"}, obs_data_get_string(settings, "Chat.Bk.Color.Mode.Custom"));
	insertJsonValue(jsonObj, {"properties", "chat_background_template_properties", "Chat.Bk.Color.Template.Paid"}, isPaidBkTemplate(obs_data_get_int(settings, "Chat.Bk.Template.List")));

	m_needSaveChatTemplates.append(jsonObj);

	readCutsomPLSChatTemplateData();
	return true;
}

QJsonArray PLSChatTemplateDataHelper::getSaveTemplate() const
{
	return m_needSaveChatTemplates;
}

QSet<QString> PLSChatTemplateDataHelper::getChatTemplateName() const
{
	return m_chatTemplateNames;
}

void PLSChatTemplateDataHelper::updateCustomTemplateName(const QString &name, const int id)
{
	auto customCout = m_needSaveChatTemplates.size();
	QJsonObject editNameTemplate;
	int selectindex = 0;
	for (; selectindex != customCout; ++selectindex) {
		if (m_needSaveChatTemplates[selectindex].toObject().value("itemId").toInt() == id) {
			editNameTemplate = m_needSaveChatTemplates[selectindex].toObject();
			m_needSaveChatTemplates.removeAt(selectindex);
			auto oldTemplateName = editNameTemplate.value("title").toString();
			m_chatTemplateNames.remove(oldTemplateName);
			break;
		}
	}
	editNameTemplate.insert("title", name);

	m_needSaveChatTemplates.insert(selectindex, editNameTemplate);
	m_chatTemplateNames.insert(name);
}

void PLSChatTemplateDataHelper::removeCustomTemplate(const int id)
{
	auto customCout = m_needSaveChatTemplates.size();
	int selectindex = 0;
	for (; selectindex != customCout; ++selectindex) {
		if (m_needSaveChatTemplates[selectindex].toObject().value("itemId").toInt() == id) {
			auto name = m_needSaveChatTemplates[selectindex].toObject().value("title").toString();
			m_needSaveChatTemplates.removeAt(selectindex);
			m_chatTemplateNames.remove(name);
			break;
		}
	}
	auto buttonGroup = m_templateButtons.value("my");
	auto button = buttonGroup->button(id);
	if (button) {
		buttonGroup->removeButton(button);
		button->setParent(nullptr);
		delete button;
	}
}

QList<ITextMotionTemplateHelper::PLSChatDefaultFamily> PLSChatTemplateDataHelper::getChatCustomDefaultFamily()
{
	return m_chatDefaultfamilies;
}

void PLSChatTemplateDataHelper::clearChatTemplateButton()
{
	for (auto templateKey : m_templateButtons.keys()) {
		auto buttonGroup = m_templateButtons.value(templateKey);
		auto buttons = buttonGroup->buttons();
		for (auto _button : buttons) {
			buttonGroup->removeButton(_button);
			_button->setParent(nullptr);
			delete _button;
		}
		buttonGroup->setParent(nullptr);
		delete buttonGroup;
	}
	m_templateButtons.clear();

	auto bkButtons = m_chatBKTemplateGroup->buttons();
	for (auto _button : bkButtons) {
		m_chatBKTemplateGroup->removeButton(_button);
		_button->setParent(nullptr);
		delete _button;
	}
}

QButtonGroup *PLSChatTemplateDataHelper::getBKTemplateButtons()
{
	return m_chatBKTemplateGroup;
}

void PLSChatTemplateDataHelper::checkChatBkRes(const std::function<void(bool)> &callback)
{
	pls::rsm::getResourceManager()->checkCategory(PLS_RSM_CID_CHAT_BG, [callback, this](pls::rsm::Category category, bool ok) {
		pls_async_call_mt([ok, callback, category, this]() {
			if (!ok) {
				if (!category) {
					allDownloadedChatBKRes(category, false);
				} else {
					CategoryChatWidget::instance()->download();
				}
			}
			callback(ok);
		});
	});
}
