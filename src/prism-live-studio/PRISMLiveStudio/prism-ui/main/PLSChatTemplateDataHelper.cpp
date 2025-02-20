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
using namespace common;

static QStringList colorList{"#755074", "#336E67", "#363E4B", "#475A5E", "#725B5A", "#474566", "#986E96", "#4B9A90", "#5C6A7E",
			     "#647E85", "#937675", "#656193", "#523751", "#224D48", "#232932", "#313F42", "#513F3F", "#2F2D43"};

PLSChatTemplateDataHelper::PLSChatTemplateDataHelper()
{
	initPLSChatTemplateData();
}

void PLSChatTemplateDataHelper::initPLSChatTemplateData()
{
	auto lang = pls_get_current_language_short_str();
	if (lang != "ko")
		lang = "en";
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
				auto *button = pls_new<ChatTemplate>(group, data.id, name, data.resourceBackupPath, data.id >= CUSTOMTHEME_MIN_ID, data.backgroundColor);
		}
	}
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
	auto raiseWidget = []() {
#ifdef Q_OS_MACOS
		if (auto propertiesView = QPointer<QWidget>(PLSBasic::instance()->GetPropertiesWindow()); propertiesView)
			pls_bring_mac_window_to_front(propertiesView->winId());
#endif // Q_OS_MACOS
	};
	for (;;) {
		raiseWidget();
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
	raiseWidget();
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
			_button->setParent(nullptr);
			delete _button;
		}
		delete buttonGroup;
	}
	m_templateButtons.clear();
}
