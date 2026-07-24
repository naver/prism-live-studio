/******************************************************************************
//PRISM/ChengBing/2024.6.1/#for chat source v2
 ******************************************************************************/

#include "prism-chatv2-source.hpp"
#include <util/threading.h>
#include <QApplication>
#include <util/dstr.h>
#include <functional>
#include <thread>
#include <mutex>
#include "obs-frontend-api.h"
#include <qjsondocument.h>
#include <QThread>
#include <libutils-api.h>
#include "log.h"
#include <pls/pls-obs-api.h>
#include <pls/pls-properties.h>
#include <pls/pls-source.h>
#include <libui.h>
#include "pls-net-url.hpp"
#include "frontend-api.h"
#include "network-state.h"
#include <qurlquery.h>
#ifdef USE_QT_LOOP
#include <QEventLoop>
#include <QThread>
#endif

using namespace std;
#define S_CT_SOURCE_TAB "Tab"
#define S_CT_SOURCE_TEMPLATE_TAB "Chat.Template.Tab"
#define S_CT_SOURCE_TEMPLATE_LIST "Chat.Template.List"
#define S_CT_SOURCE_DISPLAY "Chat.Display"
#define S_CT_SOURCE_OPTIONS "Chat.Options"
#define S_CT_SOURCE_MOTION "Chat.Motion"
#define S_CT_SOURCE_FONT "Chat.Font"
#define S_CT_SOURCE_TEXT_COLOR "Chat.Text.Color"
#define S_CT_SOURCE_BK_COLOR "Chat.Bk.Color"
#define S_CT_SOURCE_BK_CONTROL "Chat.Bk.Control"
#define S_CT_SOURCE_BK_ENABLE "Chat.Bk.Enable"
#define S_CT_SOURCE_BK_DISABLE "Chat.Bk.Disable"
#define S_CT_SOURCE_BK_TEMPLATE "Chat.Bk.Template"
#define S_CT_SOURCE_BK_TEMPLATE_LIST "Chat.Bk.Template.List"
#define S_CT_SOURCE_BK_COLOR_MODE "Chat.Bk.Color.Mode"
#define S_CT_SOURCE_BK_COLOR_MODE_DEFAULT "Chat.Bk.Color.Mode.Default"
#define S_CT_SOURCE_BK_COLOR_MODE_DEFAULT_VISABLE_LABEL "Chat.Bk.Color.Mode.Default.Label"
#define S_CT_SOURCE_BK_COLOR_MODE_CUSTOM "Chat.Bk.Color.Mode.Custom"
#define S_CT_SOURCE_BK_TAB_NEW "Chat.BK.Tab.New"
const auto S_CT_SOURCE_BK_H_LINE_1 = "Chat.Bk.Hline1";
const auto S_CT_SOURCE_BK_H_LINE_2 = "Chat.Bk.Hline2";
const auto S_CT_SOURCE_BK_H_LINE_3 = "Chat.Bk.Hline3";
const auto S_CT_SOURCE_BK_H_LINE_4 = "Chat.Bk.Hline4";
#define S_CT_SOURCE_TEMPLATE "CTSource.Template"
#define S_CT_SOURCE_TEMPLATE_DESC "CTSource.Template.Desc"
#define S_CT_SOURCE_FONT_SIZE "CTSource.FontSize"
#define S_URL "url"
#define S_WIDTH "width"
#define S_HEIGHT "height"

#define CUSTOMID 1100
struct chatBackgroundAttr {
	QString chatBackgroundWebType;
	QString chatBackgroundName;
	QString chatBackgroundDisplayName;
	bool isPaid = false;
	int chatBackgroundIndex = -1;
	bool isCustomChatBackgroundColor = false;
	bool isEnableChatBackgroundTemplateColor = false;
	QStringList chatBackgroundDefaultColorList;
	QStringList chatBackgroundCustomColorList;
	bool isVisableColorMode = true;
	bool isVisableColorDefaultMode = false;
	bool isVisableColorCustomMode = true;
	bool isEnableColorMode = true;
	bool isEnableColorDefaultMode = true;
	bool isEnableColorCustomMode = true;
};
struct ChatSourceAttr {
	QString title;
	QString webId;
	QStringList selectPlatformList;
	bool isEnablePlatformType = false;
	bool isEnablePlatformIcon = false;
	bool isCheckPlatformIcon = false;
	bool isEnableLevelIcon = false;
	bool isCheckLevelIcon = false;
	bool isEnableIdIcon = false;
	bool isCheckIdIcon = false;
	bool isEnablePlatformIconDisplay = false;
	bool isEnableChatDisplay = false;

	bool isEnableChatWrap = false;
	bool isCheckChatWrap = false;
	bool isCheckLeftChatAlign = false;
	bool isEnableChatAlign = false;
	bool isCheckChatDisapperEffect = false;
	bool isEnableChatDisapperEffect = false;
	int chatWidth = 0;
	int chatHeight = 0;
	bool isEnableChatBoxSize = false;
	bool isEnableChatOption = false;

	int chatMotionStyle = 0;
	bool isCheckChatMotion = 0;
	bool isEnableChatMotion = false;

	QString chatFontFamily;
	QString chatFontStyle;
	bool isEnableChatCommonFont = false;
	int chatFontSize = 0;
	bool isEnableChatFontSize = false;
	int chatFontOutlineSize = 0;
	qint64 chatFontOutlineColor = 0;
	bool isEnableChatFontOutlineColor = false;
	bool isEnableChatFontOutlineSize = false;
	bool isEnableChatFont = false;

	bool isCheckNickTextSingleColor = false;
	qint64 nickTextDefaultColor = 0;
	bool isEnableSingleNickTextColor = false;
	bool isEnableRadomNickTextColor = false;
	qint64 mgrNickTextColor = 0;
	bool isEnableMgrNickTextColor = false;
	qint64 subcribeTextColor = 0;
	bool isEnableSubcribeTextColor = false;
	qint64 messageTextColor = 0;
	bool isEnableMessageTextColor = false;
	bool isEnableChatTextColor = false;

	qint64 chatSingleBkColor = 0;
	int chatSingleBkAlpha = 0;
	qint64 chatSingleColorStyle = 0;
	bool isCheckChatSingleBkColor = false;
	bool isEnableChatSingleBkColor = false;
	qint64 chatTotalBkColor = 0;
	int chatTotalBkAlpha = 0;
	bool isCheckChatTotalBkColor = false;
	bool isEnableChatTotalBkColor = false;
	int chatWindowAlpha = 0;
	bool isEnableChatBackgroundColor = false;

	int ctBkControl = -1;
	int ctBkTemplateIndex = -1;
	int ctBkMode = -1;
	int ctBkModeDefault = -1;
	QString ctBkModeCustom;
};
static QSet<uint> s_paidIds;
static QMap<uint, ChatSourceAttr> s_ChatSourceAttrs;
static QMap<uint, chatBackgroundAttr> s_chatBackgroundAttrs;
static QStringList s_defaultFontList;
static uint s_defaultId = 0;
static bool isFirstShow = true;
static bool isChatBkFirstShow = true;

QStringList JsonArrayToStringList(const QJsonArray &array)
{
	QStringList list;
	for (const auto &data : array) {
		list.append(data.toString());
	}
	return list;
}
int getMotionStyle(const QString &style)
{
	if (0 == style.compare("Shaking", Qt::CaseInsensitive))
		return 0;
	else if (0 == style.compare("Random", Qt::CaseInsensitive))
		return 1;
	else if (0 == style.compare("Wave", Qt::CaseInsensitive))
		return 2;
	return -1;
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
int getSingleColorStyle(const QString &style)
{
	if (0 == style.compare("pastel", Qt::CaseInsensitive))
		return 0;
	else if (0 == style.compare("Vivid", Qt::CaseInsensitive))
		return 1;
	return -1;
}
QString getSingleColorStr(const int &style)
{
	QString singleColorStr;
	if (style == 0) {
		singleColorStr = "Pastel";
	} else if (style == 1) {
		singleColorStr = "Vivid";
	}
	return singleColorStr;
}
static inline long long color_to_int(const QString &colorStr)
{
	QString colorStr_(colorStr.trimmed());
	if (colorStr_.isEmpty() || colorStr_ == "#000000") {
		return 0;
	}
	QColor color(colorStr_);
	auto shift = [&](unsigned val, int shift) { return ((val & 0xff) << shift); };

	return shift(color.red(), 0) | shift(color.green(), 8) | shift(color.blue(), 16) | shift(color.alpha(), 24);
}

static inline void color_to_int_list(QStringList &colorList, const QStringList &colorsData)
{
	for (auto colorStr : colorsData) {
		auto tmpStr = colorStr.trimmed();
		if (tmpStr.isEmpty())
			continue;
		if (tmpStr.contains('/')) {
			auto colors = tmpStr.split('/');
			QStringList tmpList;
			for (auto color : colors) {
				tmpList.append(QString::number(color_to_int(color)));
			}
			colorList.append(tmpList.join('/'));

		} else {
			colorList.append(QString::number(color_to_int(tmpStr)));
		}
	}
}

static void initsChatSourceAttrs(const QJsonArray &array)
{
	auto lang = pls_get_current_language_short_str();
	if (lang != "ko") {
		lang = "en";
	}

	for (const auto &chatTemplate : array) {
		auto ct = chatTemplate.toObject();
		ChatSourceAttr chatAttr;
		auto id = ct.value("itemId").toInt();
		auto title = ct.value("title").toString();
		auto webId = ct.value("webId").toString();
		auto properties = ct.value("properties").toObject();
		auto cdp = properties.value("chat_display_properties").toObject();
		chatAttr.webId = webId;
		auto tmpPlatformList = cdp.value("platform_type").toObject().value("platform_list").toString();
		chatAttr.selectPlatformList = cdp.value("platform_type").toObject().contains("platform_list") ? tmpPlatformList.split(';') : getChannelWithChatList(true);
		chatAttr.isEnablePlatformType = cdp.value("platform_type").toObject().value("is_enable").toBool();
		chatAttr.isEnablePlatformIcon = cdp.value("platform_icon_display").toObject().value("platform_icon").toObject().value("is_enable").toBool();
		chatAttr.isCheckPlatformIcon = cdp.value("platform_icon_display").toObject().value("platform_icon").toObject().value("is_check").toBool();
		chatAttr.isEnableLevelIcon = cdp.value("platform_icon_display").toObject().value("level_icon").toObject().value("is_enable").toBool();
		chatAttr.isCheckLevelIcon = cdp.value("platform_icon_display").toObject().value("level_icon").toObject().value("is_check").toBool();
		chatAttr.isEnableIdIcon = cdp.value("platform_icon_display").toObject().value("id_icon").toObject().value("is_enable").toBool();
		chatAttr.isCheckIdIcon = cdp.value("platform_icon_display").toObject().value("id_icon").toObject().value("is_check").toBool();
		chatAttr.isEnablePlatformIconDisplay = cdp.value("platform_icon_display").toObject().value("is_enable").toBool();
		chatAttr.isEnableChatDisplay = cdp.value("is_enable").toBool();

		auto cop = properties.value("chat_options_properties").toObject();
		chatAttr.isEnableChatWrap = cop.value("chat_sort").toObject().value("chat_wrap").toObject().value("is_enable").toBool();
		chatAttr.isCheckChatWrap = cop.value("chat_sort").toObject().value("chat_wrap").toObject().value("is_check").toBool();
		chatAttr.isCheckLeftChatAlign = cop.value("chat_sort").toObject().value("chat_align").toObject().value("is_left").toBool();
		chatAttr.isEnableChatAlign = cop.value("chat_sort").toObject().value("chat_align").toObject().value("is_enable").toBool();
		chatAttr.isCheckChatDisapperEffect = cop.value("chat_disapper_effect").toObject().value("is_on").toBool();
		chatAttr.isEnableChatDisapperEffect = cop.value("chat_disapper_effect").toObject().value("is_enable").toBool();
		chatAttr.chatWidth = cop.value("chat_box_size").toObject().value("width").toInt();
		chatAttr.chatHeight = cop.value("chat_box_size").toObject().value("height").toInt();
		chatAttr.isEnableChatBoxSize = cop.value("chat_box_size").toObject().value("is_enable").toBool();
		chatAttr.isEnableChatOption = cop.value("is_enable").toBool();

		auto cmp = properties.value("chat_motion_properties").toObject();
		chatAttr.chatMotionStyle = getMotionStyle(cmp.value("motion_style").toString());
		chatAttr.isCheckChatMotion = cmp.value("is_check").toBool();
		chatAttr.isEnableChatMotion = cmp.value("is_enable").toBool();

		auto cfp = properties.value("chat_font_properties").toObject();
		chatAttr.chatFontFamily = cfp.value("common_font").toObject().value("selected_font").toObject().value("family").toObject().value(lang).toString();
		chatAttr.chatFontStyle = cfp.value("common_font").toObject().value("selected_font").toObject().value("style").toObject().value(lang).toString();
		chatAttr.isEnableChatCommonFont = cfp.value("common_font").toObject().value("is_enable").toBool();
		chatAttr.chatFontSize = cfp.value("font_size").toObject().value("size_pt").toInt();
		chatAttr.isEnableChatFontSize = cfp.value("font_size").toObject().value("is_enable").toBool();
		chatAttr.chatFontOutlineSize = cfp.value("font_outLine_color").toObject().value("outLine_size_pt").toInt();
		chatAttr.chatFontOutlineColor = color_to_int(cfp.value("font_outLine_color").toObject().value("default_color_rgb_code").toString());
		chatAttr.isEnableChatFontOutlineColor = cfp.value("font_outLine_color").toObject().value("is_color_enable").toBool();
		chatAttr.isEnableChatFontOutlineSize = cfp.value("font_outLine_color").toObject().value("is_size_enable").toBool();
		chatAttr.isEnableChatFont = cfp.value("is_enable").toBool();

		auto ctop = properties.value("chat_text_color_properties").toObject();
		chatAttr.isCheckNickTextSingleColor = ctop.value("common_nick_text_color").toObject().value("is_single_color").toBool();
		chatAttr.nickTextDefaultColor = color_to_int(ctop.value("common_nick_text_color").toObject().value("default_color_rgb_code").toString());
		chatAttr.isEnableSingleNickTextColor = ctop.value("common_nick_text_color").toObject().value("is_single_enable").toBool();
		chatAttr.isEnableRadomNickTextColor = ctop.value("common_nick_text_color").toObject().value("is_radom_enable").toBool();
		chatAttr.mgrNickTextColor = color_to_int(ctop.value("manager_nick_text_color").toObject().value("default_color_rgb_code").toString());
		chatAttr.isEnableMgrNickTextColor = ctop.value("manager_nick_text_color").toObject().value("is_enable").toBool();
		chatAttr.subcribeTextColor = color_to_int(ctop.value("subcribe_text_color").toObject().value("default_color_rgb_code").toString());
		chatAttr.isEnableSubcribeTextColor = ctop.value("subcribe_text_color").toObject().value("is_enable").toBool();
		chatAttr.messageTextColor = color_to_int(ctop.value("message_text_color").toObject().value("default_color_rgb_code").toString());
		chatAttr.isEnableMessageTextColor = ctop.value("message_text_color").toObject().value("is_enable").toBool();
		chatAttr.isEnableChatTextColor = ctop.value("is_enable").toBool();

		auto cbcp = properties.value("chat_background_color_properties").toObject();
		chatAttr.chatSingleBkColor = color_to_int(cbcp.value("single_background_color").toObject().value("default_color_rgb_code").toString());
		chatAttr.chatSingleBkAlpha = cbcp.value("single_background_color").toObject().value("color_alpha").toInt();
		chatAttr.chatSingleColorStyle = getSingleColorStyle(cbcp.value("single_background_color").toObject().value("color_style").toString());
		chatAttr.isCheckChatSingleBkColor = cbcp.value("single_background_color").toObject().value("is_check").toBool();
		chatAttr.isEnableChatSingleBkColor = cbcp.value("single_background_color").toObject().value("is_enable").toBool();
		chatAttr.chatTotalBkColor = color_to_int(cbcp.value("total_background_color").toObject().value("default_color_rgb_code").toString());
		chatAttr.chatTotalBkAlpha = cbcp.value("total_background_color").toObject().value("color_alpha").toInt();
		chatAttr.isCheckChatTotalBkColor = cbcp.value("total_background_color").toObject().value("is_check").toBool();
		chatAttr.isEnableChatTotalBkColor = cbcp.value("total_background_color").toObject().value("is_enable").toBool();
		chatAttr.chatWindowAlpha = cbcp.value("chat_window_alpha").toInt();
		chatAttr.isEnableChatBackgroundColor = cbcp.value("is_enable").toBool();

		auto cbtp = properties.value("chat_background_template_properties").toObject();
		if (cbtp.isEmpty()) {
			auto chatBackgroundAttr = s_chatBackgroundAttrs.value(0);
			chatAttr.ctBkControl = chatBackgroundAttr.isEnableChatBackgroundTemplateColor ? 0 : 1;
			chatAttr.ctBkTemplateIndex = chatBackgroundAttr.chatBackgroundIndex;
			chatAttr.ctBkMode = chatBackgroundAttr.isCustomChatBackgroundColor;
			chatAttr.ctBkModeDefault = 0;
			chatAttr.ctBkModeCustom = chatBackgroundAttr.chatBackgroundCustomColorList.join(',');
		} else {
			chatAttr.ctBkTemplateIndex = cbtp.value(S_CT_SOURCE_BK_TEMPLATE_LIST).toInt();
			chatAttr.ctBkControl = cbtp.value(S_CT_SOURCE_BK_CONTROL).toInt();
			chatAttr.ctBkMode = cbtp.value(S_CT_SOURCE_BK_COLOR_MODE).toInt();
			chatAttr.ctBkModeDefault = cbtp.value(S_CT_SOURCE_BK_COLOR_MODE_DEFAULT).toInt();
			chatAttr.ctBkModeCustom = cbtp.value(S_CT_SOURCE_BK_COLOR_MODE_CUSTOM).toString();
		}
		s_ChatSourceAttrs.insert(id, chatAttr);
	}
}

static void updateMyChatSourceAttrs()
{
	if (!pls_get_chat_template_helper_instance())
		return;
	auto chatTemplateArray = pls_get_chat_template_helper_instance()->getSaveTemplate();
	initsChatSourceAttrs(chatTemplateArray);
}
static bool isCustomBKColor(const QString &colorType)
{
	return colorType.compare("custom", Qt::CaseInsensitive) == 0;
}
static void getDefaultChatBackgroundAttrs()
{
	if (!s_chatBackgroundAttrs.isEmpty())
		return;
	auto lang = pls_get_current_language_short_str();
	if (lang != "ko") {
		lang = "en";
	}
	auto items = pls::rsm::getResourceManager()->getItems(PLS_RSM_CID_CHAT_BG);
	pls::rsm::Category category;
	if (items.empty()) {
		category = pls::rsm::getResourceManager()->getDefaultCategory(PLS_RSM_CID_CHAT_BG);
		items = category.items();
	}
	PLS_DEBUG("chatv2source", "chat v2 source items is %d", items.size());

	for (auto item : items) {
		chatBackgroundAttr attr;
		attr.chatBackgroundDisplayName = item.attr({"properties", "item_name", lang}).toString();
		attr.chatBackgroundName = item.attr({"properties", "item_name", "en"}).toString();
		attr.chatBackgroundWebType = item.attr({"properties", "web_type"}).toString();
		attr.isPaid = false;
		auto colorHash = item.attr({"properties", "color"}).toHash();
		attr.isCustomChatBackgroundColor = isCustomBKColor(colorHash.value("color_mode").toString());
		attr.isEnableChatBackgroundTemplateColor = false;
		attr.chatBackgroundIndex = item.attr({"properties", "itemNo"}).toInt();
		color_to_int_list(attr.chatBackgroundDefaultColorList, colorHash.value("default_color").toStringList());
		color_to_int_list(attr.chatBackgroundCustomColorList, colorHash.value("custom_color").toStringList());
		attr.isVisableColorMode = colorHash.value("is_visable_color_mode").toBool();
		attr.isVisableColorCustomMode = colorHash.value("is_visiable_color_custom").toBool();
		attr.isVisableColorDefaultMode = colorHash.value("is_visable_color_default").toBool();
		attr.isEnableColorCustomMode = colorHash.value("is_enable_color_mode").toBool();
		attr.isEnableColorCustomMode = colorHash.value("is_enable__color_custom").toBool();
		attr.isEnableColorDefaultMode = colorHash.value("is_enable_color_default").toBool();
		s_chatBackgroundAttrs.insert(attr.chatBackgroundIndex, attr);
		if (attr.isPaid) {
			s_paidIds.insert(attr.chatBackgroundIndex);
		}
	}
}
static void getDefaultChatSourceAttrs()
{
	if (!s_ChatSourceAttrs.isEmpty())
		return;

	QString chatSourceJsonPath = pls_get_app_user_data_file_path_pn(QStringLiteral("/resources/library/Library_Policy_PC/chatv2source/chatv2source.json"), false);
	QString chatSourceLocalJsonPath = ":/Configs/resource/DefaultResources/chatv2source.json";
	QJsonObject chatSourceObj, chatSourceLocalObj;
	pls_read_json(chatSourceObj, chatSourceJsonPath);
	pls_read_json(chatSourceLocalObj, chatSourceLocalJsonPath);
	auto currentChatSourceObj = chatSourceObj.value("version").toInt() >= chatSourceLocalObj.value("version").toInt() ? chatSourceObj : chatSourceLocalObj;
	s_defaultFontList = JsonArrayToStringList(currentChatSourceObj.value("default_font_list").toArray());
	auto templates = currentChatSourceObj.value("default_template").toArray();
	s_defaultId = templates.first().toObject().value("itemId").toInt();
	initsChatSourceAttrs(templates);
}

static void process_url_language(QString &url)
{
	auto lang = pls_get_current_language_short_str();
	if (lang != "ko") {
		lang = "en";
	}
	url += "?lang=";
	url += lang;
}

static void set_ct_display_data(obs_data_t *settings, const uint &id, bool is_default)
{
	obs_data_t *ct_display_data_obj = obs_data_create();
	auto attrs = s_ChatSourceAttrs.value(id);
	obs_data_set_string(ct_display_data_obj, "selectPlatformList", attrs.selectPlatformList.join(';').toUtf8().constData());
	obs_data_set_bool(ct_display_data_obj, "isEnablePlatformType", attrs.isEnablePlatformType);
	obs_data_set_bool(ct_display_data_obj, "isEnablePlatformIcon", attrs.isEnablePlatformIcon);
	obs_data_set_bool(ct_display_data_obj, "isCheckPlatformIcon", attrs.isCheckPlatformIcon);
	obs_data_set_bool(ct_display_data_obj, "isEnableLevelIcon", attrs.isEnableLevelIcon);
	obs_data_set_bool(ct_display_data_obj, "isCheckLevelIcon", attrs.isCheckLevelIcon);
	obs_data_set_bool(ct_display_data_obj, "isEnableIdIcon", attrs.isEnableIdIcon);
	obs_data_set_bool(ct_display_data_obj, "isCheckIdIcon", attrs.isCheckIdIcon);
	obs_data_set_bool(ct_display_data_obj, "isEnablePlatformIconDisplay", attrs.isEnablePlatformIconDisplay);
	obs_data_set_bool(ct_display_data_obj, "isEnableChatDisplay", attrs.isEnableChatDisplay);
	if (is_default) {
		obs_data_set_default_obj(settings, S_CT_SOURCE_DISPLAY, ct_display_data_obj);
	} else {
		obs_data_set_obj(settings, S_CT_SOURCE_DISPLAY, ct_display_data_obj);
	}
	obs_data_release(ct_display_data_obj);
}
static void set_ct_options_data(obs_data_t *settings, const uint &id, bool is_default)
{
	obs_data_t *ct_options_data_obj = obs_data_create();
	auto attrs = s_ChatSourceAttrs.value(id);

	obs_data_set_bool(ct_options_data_obj, "isEnableChatWrap", attrs.isEnableChatWrap);
	obs_data_set_bool(ct_options_data_obj, "isCheckChatWrap", attrs.isCheckChatWrap);
	obs_data_set_bool(ct_options_data_obj, "isCheckLeftChatAlign", attrs.isCheckLeftChatAlign);
	obs_data_set_bool(ct_options_data_obj, "isEnableChatAlign", attrs.isEnableChatAlign);
	obs_data_set_bool(ct_options_data_obj, "isCheckChatDisapperEffect", attrs.isCheckChatDisapperEffect);
	obs_data_set_bool(ct_options_data_obj, "isEnableChatDisapperEffect", attrs.isEnableChatDisapperEffect);
	obs_data_set_bool(ct_options_data_obj, "isEnableChatBoxSize", attrs.isEnableChatBoxSize);
	obs_data_set_bool(ct_options_data_obj, "isEnableChatOption", attrs.isEnableChatOption);

	if (is_default) {
		obs_data_set_default_int(settings, "chatWidth", attrs.chatWidth);
		obs_data_set_default_int(settings, "chatHeight", attrs.chatHeight);
		obs_data_set_default_obj(settings, S_CT_SOURCE_OPTIONS, ct_options_data_obj);
	} else {
		obs_data_set_obj(settings, S_CT_SOURCE_OPTIONS, ct_options_data_obj);
		obs_data_set_int(settings, "chatWidth", attrs.chatWidth);
		obs_data_set_int(settings, "chatHeight", attrs.chatHeight);
	}
	obs_data_release(ct_options_data_obj);
}
static void set_ct_motion_data(obs_data_t *settings, const uint &id, bool is_default)
{
	obs_data_t *ct_motion_data_obj = obs_data_create();
	auto attrs = s_ChatSourceAttrs.value(id);

	obs_data_set_bool(ct_motion_data_obj, "isEnableChatMotion", attrs.isEnableChatMotion);
	obs_data_set_bool(ct_motion_data_obj, "isCheckChatMotion", attrs.isCheckChatMotion);
	obs_data_set_int(ct_motion_data_obj, "chatMotionStyle", attrs.chatMotionStyle);

	if (is_default) {
		obs_data_set_default_obj(settings, S_CT_SOURCE_MOTION, ct_motion_data_obj);
	} else {
		obs_data_set_obj(settings, S_CT_SOURCE_MOTION, ct_motion_data_obj);
	}

	obs_data_release(ct_motion_data_obj);
}
static void set_ct_font_data(obs_data_t *settings, const uint &id, bool is_default)
{
	obs_data_t *ct_font_data_obj = obs_data_create();
	auto attrs = s_ChatSourceAttrs.value(id);

	obs_data_set_string(ct_font_data_obj, "chatFontFamily", attrs.chatFontFamily.toUtf8());
	obs_data_set_string(ct_font_data_obj, "chatFontStyle", attrs.chatFontStyle.toUtf8());
	obs_data_set_bool(ct_font_data_obj, "isEnableChatCommonFont", attrs.isEnableChatCommonFont);
	obs_data_set_int(ct_font_data_obj, "chatFontSize", attrs.chatFontSize);
	obs_data_set_bool(ct_font_data_obj, "isEnableChatFontSize", attrs.isEnableChatFontSize);
	obs_data_set_int(ct_font_data_obj, "chatFontOutlineSize", attrs.chatFontOutlineSize);
	obs_data_set_int(ct_font_data_obj, "chatFontOutlineColor", attrs.chatFontOutlineColor);
	obs_data_set_bool(ct_font_data_obj, "isEnableChatFontOutlineColor", attrs.isEnableChatFontOutlineColor);
	obs_data_set_bool(ct_font_data_obj, "isEnableChatFontOutlineSize", attrs.isEnableChatFontOutlineSize);
	obs_data_set_bool(ct_font_data_obj, "isEnableChatFont", attrs.isEnableChatFont);

	if (is_default) {
		obs_data_set_default_obj(settings, S_CT_SOURCE_FONT, ct_font_data_obj);
	} else {
		obs_data_set_obj(settings, S_CT_SOURCE_FONT, ct_font_data_obj);
	}
	obs_data_release(ct_font_data_obj);
}
static void set_ct_text_color_data(obs_data_t *settings, const uint &id, bool is_default)
{
	obs_data_t *ct_text_color_data_obj = obs_data_create();
	auto attrs = s_ChatSourceAttrs.value(id);

	obs_data_set_bool(ct_text_color_data_obj, "isCheckNickTextSingleColor", attrs.isCheckNickTextSingleColor);
	obs_data_set_int(ct_text_color_data_obj, "nickTextDefaultColor", attrs.nickTextDefaultColor);
	obs_data_set_bool(ct_text_color_data_obj, "isEnableSingleNickTextColor", attrs.isEnableSingleNickTextColor);
	obs_data_set_bool(ct_text_color_data_obj, "isEnableRadomNickTextColor", attrs.isEnableRadomNickTextColor);
	obs_data_set_int(ct_text_color_data_obj, "mgrNickTextColor", attrs.mgrNickTextColor);
	obs_data_set_bool(ct_text_color_data_obj, "isEnableMgrNickTextColor", attrs.isEnableMgrNickTextColor);
	obs_data_set_int(ct_text_color_data_obj, "subcribeTextColor", attrs.subcribeTextColor);
	obs_data_set_bool(ct_text_color_data_obj, "isEnableSubcribeTextColor", attrs.isEnableSubcribeTextColor);
	obs_data_set_int(ct_text_color_data_obj, "messageTextColor", attrs.messageTextColor);
	obs_data_set_bool(ct_text_color_data_obj, "isEnableMessageTextColor", attrs.isEnableMessageTextColor);
	obs_data_set_bool(ct_text_color_data_obj, "isEnableChatTextColor", attrs.isEnableChatTextColor);
	if (is_default) {
		obs_data_set_default_obj(settings, S_CT_SOURCE_TEXT_COLOR, ct_text_color_data_obj);
	} else {
		obs_data_set_obj(settings, S_CT_SOURCE_TEXT_COLOR, ct_text_color_data_obj);
	}
	obs_data_release(ct_text_color_data_obj);
}
static void set_ct_bk_color_data(obs_data_t *settings, const uint &id, bool is_default)
{
	obs_data_t *ct_bk_color_data_obj = obs_data_create();
	auto attrs = s_ChatSourceAttrs.value(id);
	obs_data_set_int(ct_bk_color_data_obj, "chatSingleBkColor", attrs.chatSingleBkColor);
	obs_data_set_int(ct_bk_color_data_obj, "chatSingleBkAlpha", attrs.chatSingleBkAlpha);
	obs_data_set_int(ct_bk_color_data_obj, "chatSingleColorStyle", attrs.chatSingleColorStyle);
	obs_data_set_bool(ct_bk_color_data_obj, "isCheckChatSingleBkColor", attrs.isCheckChatSingleBkColor);
	obs_data_set_bool(ct_bk_color_data_obj, "isEnableChatSingleBkColor", attrs.isEnableChatSingleBkColor);
	obs_data_set_int(ct_bk_color_data_obj, "chatTotalBkColor", attrs.chatTotalBkColor);
	obs_data_set_int(ct_bk_color_data_obj, "chatTotalBkAlpha", attrs.chatTotalBkAlpha);
	obs_data_set_bool(ct_bk_color_data_obj, "isCheckChatTotalBkColor", attrs.isCheckChatTotalBkColor);
	obs_data_set_bool(ct_bk_color_data_obj, "isEnableChatTotalBkColor", attrs.isEnableChatTotalBkColor);
	obs_data_set_int(ct_bk_color_data_obj, "chatWindowAlpha", attrs.chatWindowAlpha);
	obs_data_set_bool(ct_bk_color_data_obj, "isEnableChatBackgroundColor", attrs.isEnableChatBackgroundColor);

	if (is_default) {
		obs_data_set_default_obj(settings, S_CT_SOURCE_BK_COLOR, ct_bk_color_data_obj);
	} else {
		obs_data_set_obj(settings, S_CT_SOURCE_BK_COLOR, ct_bk_color_data_obj);
	}
	obs_data_release(ct_bk_color_data_obj);
}
static void ct_bk_default_color_view_changed(obs_properties_t *props, const int index)
{
	for (auto constInter = s_chatBackgroundAttrs.constBegin(); constInter != s_chatBackgroundAttrs.constEnd(); ++constInter) {
		obs_property_t *ct_bk_color_default = obs_properties_get(props, constInter->chatBackgroundName.toUtf8().constData());
		obs_property_set_visible(ct_bk_color_default, constInter->chatBackgroundIndex == index);
	}
}
static void ct_bk_default_color_view_reset(obs_properties_t *props, obs_data_t *settings)
{
	for (auto constInter = s_chatBackgroundAttrs.constBegin(); constInter != s_chatBackgroundAttrs.constEnd(); ++constInter) {
		obs_property_t *ct_bk_color_default = obs_properties_get(props, constInter->chatBackgroundName.toUtf8().constData());
		obs_data_set_int(settings, constInter->chatBackgroundName.toUtf8().constData(), constInter->chatBackgroundIndex == 2 ? 3 : 0);
	}
}
static void ct_bk_sub_view_changed(obs_properties_t *props, obs_data_t *settings, const int index)
{

	obs_property_t *ct_bk_color_mode = obs_properties_get(props, S_CT_SOURCE_BK_COLOR_MODE);
	obs_property_set_visible(ct_bk_color_mode, s_chatBackgroundAttrs.value(index).isVisableColorMode);
	obs_property_t *ct_bk_color_mode_custom = obs_properties_get(props, S_CT_SOURCE_BK_COLOR_MODE_CUSTOM);
	obs_property_set_visible(ct_bk_color_mode_custom, s_chatBackgroundAttrs.value(index).isVisableColorCustomMode);
	ct_bk_default_color_view_changed(props, index);
}

static bool ct_bk_mode_changed(void *data, obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	auto isCustom = (obs_data_get_int(settings, S_CT_SOURCE_BK_COLOR_MODE)) == 1;
	auto index = obs_data_get_int(settings, S_CT_SOURCE_BK_TEMPLATE_LIST);

	obs_property_t *ct_bk_color_mode_custom = obs_properties_get(props, S_CT_SOURCE_BK_COLOR_MODE_CUSTOM);
	obs_property_set_visible(ct_bk_color_mode_custom, isCustom);
	ct_bk_default_color_view_changed(props, !isCustom ? index : -1);

	return true;
}
static bool ct_tab_changed(void *data, obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	if (!data)
		return false;

	updateMyChatSourceAttrs();

	auto index = obs_data_get_int(settings, S_CT_SOURCE_TAB);
	obs_property_t *ct_template_tab = obs_properties_get(props, S_CT_SOURCE_TEMPLATE);
	obs_property_t *ct_template_list = obs_properties_get(props, S_CT_SOURCE_TEMPLATE_LIST);
	obs_property_t *ct_display = obs_properties_get(props, S_CT_SOURCE_DISPLAY);
	obs_property_t *ct_options = obs_properties_get(props, S_CT_SOURCE_OPTIONS);
	obs_property_t *ct_motion = obs_properties_get(props, S_CT_SOURCE_MOTION);
	obs_property_t *ct_font = obs_properties_get(props, S_CT_SOURCE_FONT);
	obs_property_t *ct_text_color = obs_properties_get(props, S_CT_SOURCE_TEXT_COLOR);
	obs_property_t *ct_bk_color = obs_properties_get(props, S_CT_SOURCE_BK_COLOR);
	obs_property_t *ct_bk_control = obs_properties_get(props, S_CT_SOURCE_BK_CONTROL);
	obs_property_t *ct_bk_template_list = obs_properties_get(props, S_CT_SOURCE_BK_TEMPLATE_LIST);
	obs_property_t *ct_bk_color_mode = obs_properties_get(props, S_CT_SOURCE_BK_COLOR_MODE);
	obs_property_t *ct_bk_color_mode_custom = obs_properties_get(props, S_CT_SOURCE_BK_COLOR_MODE_CUSTOM);
	obs_property_t *ct_bk_hline1 = obs_properties_get(props, S_CT_SOURCE_BK_H_LINE_1);
	obs_property_t *ct_bk_hline2 = obs_properties_get(props, S_CT_SOURCE_BK_H_LINE_2);
	obs_property_t *ct_bk_hline3 = obs_properties_get(props, S_CT_SOURCE_BK_H_LINE_3);

	obs_property_set_visible(ct_template_tab, index == 0);
	obs_property_set_visible(ct_template_list, index == 0);
	obs_property_set_visible(ct_display, index == 1);
	obs_property_set_visible(ct_options, index == 1);
	obs_property_set_visible(ct_motion, index == 1);
	obs_property_set_visible(ct_font, index == 2);
	obs_property_set_visible(ct_text_color, index == 2);
	obs_property_set_visible(ct_bk_color, index == 2);
	obs_property_set_visible(ct_bk_control, index == 3);
	obs_property_set_visible(ct_bk_template_list, index == 3);
	obs_property_set_visible(ct_bk_color_mode, index == 3);
	obs_property_set_visible(ct_bk_color_mode_custom, index == 3);
	obs_property_set_visible(ct_bk_hline1, index == 3);
	obs_property_set_visible(ct_bk_hline2, index == 3);
	obs_property_set_visible(ct_bk_hline3, index == 3);

	ct_bk_default_color_view_changed(props, -1);
	if (index == 3) {
		ct_bk_mode_changed(data, props, property, settings);
		obs_property_t *ct_bk_color_mode = obs_properties_get(props, S_CT_SOURCE_BK_COLOR_MODE);
		auto colorBkIndex = obs_data_get_int(settings, S_CT_SOURCE_BK_TEMPLATE_LIST);
		obs_property_set_visible(ct_bk_color_mode, s_chatBackgroundAttrs.value(colorBkIndex).isVisableColorMode);
		obs_data_set_bool(settings, S_CT_SOURCE_BK_COLOR_MODE_DEFAULT_VISABLE_LABEL, true);
	}
	return true;
}

static bool ct_bk_enable_changed(void *data, obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	auto isEnable = (obs_data_get_int(settings, S_CT_SOURCE_BK_CONTROL)) == 0;
	obs_property_t *ct_bk_template_list = obs_properties_get(props, S_CT_SOURCE_BK_TEMPLATE_LIST);
	obs_property_t *ct_bk_color_mode = obs_properties_get(props, S_CT_SOURCE_BK_COLOR_MODE);
	obs_property_t *ct_bk_color_mode_default = obs_properties_get(props, S_CT_SOURCE_BK_COLOR_MODE_DEFAULT);
	obs_property_t *ct_bk_color_mode_custom = obs_properties_get(props, S_CT_SOURCE_BK_COLOR_MODE_CUSTOM);
	obs_property_set_enabled(ct_bk_template_list, isEnable);
	obs_property_set_enabled(ct_bk_color_mode, isEnable);
	obs_property_set_enabled(ct_bk_color_mode_default, isEnable);
	obs_property_set_enabled(ct_bk_color_mode_custom, isEnable);

	for (auto constInter = s_chatBackgroundAttrs.constBegin(); constInter != s_chatBackgroundAttrs.constEnd(); ++constInter) {
		obs_property_t *ct_bk_color_default = obs_properties_get(props, constInter->chatBackgroundName.toUtf8().constData());
		obs_property_set_enabled(ct_bk_color_default, isEnable);
	}
	return true;
}
static void ct_bk_render_data(obs_data_t *settings, int index, bool isCustom)
{
	if (isCustom && index < CUSTOMID) {
		return;
	}

	if (auto itew = s_ChatSourceAttrs.find(index); itew != s_ChatSourceAttrs.end()) {
		ChatSourceAttr attrs = itew.value();
		obs_data_set_int(settings, S_CT_SOURCE_BK_CONTROL, attrs.ctBkControl);
		obs_data_set_int(settings, S_CT_SOURCE_BK_TEMPLATE_LIST, attrs.ctBkTemplateIndex);
		obs_data_set_int(settings, S_CT_SOURCE_BK_COLOR_MODE, attrs.ctBkMode);
		obs_data_set_int(settings, s_chatBackgroundAttrs.value(attrs.ctBkTemplateIndex).chatBackgroundName.toUtf8().constData(), attrs.ctBkModeDefault);
		obs_data_set_int(settings, S_CT_SOURCE_BK_COLOR_MODE_DEFAULT, attrs.ctBkModeDefault);
		obs_data_set_string(settings, S_CT_SOURCE_BK_COLOR_MODE_CUSTOM, attrs.ctBkModeCustom.toUtf8().constData());
	}
}

static bool ct_template_list_changed(void *data, obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	auto index = obs_data_get_int(settings, S_CT_SOURCE_TEMPLATE_LIST);
	auto context = (chat_template_source *)(data);
	if (!context)
		return false;

	if (index % 10 == 4 && context->currentTemplateId % 10 != 4 || (index % 10 != 4 && context->currentTemplateId % 10 == 4))
		context->template5Changed = true;

	// On first show (open properties), keep source's saved settings (e.g. custom chat background color).
	// Only overwrite with template data when user actually changes the template selection (PRISM_PC-4769).
	if (isFirstShow) {
		isFirstShow = false;
		ct_bk_render_data(settings, index, true);
		ct_bk_enable_changed(data, props, property, settings);
	} else if (context->currentTemplateId != index) {
		context->currentBKTemplateId = -1;
		obs_data_set_bool(settings, "ctParamChanged", false);
		set_ct_display_data(settings, index, false);
		set_ct_options_data(settings, index, false);
		set_ct_motion_data(settings, index, false);
		set_ct_font_data(settings, index, false);
		set_ct_text_color_data(settings, index, false);
		set_ct_bk_color_data(settings, index, false);

		auto browser_settings = obs_source_get_settings(context->m_browser);
		auto browserWidth = obs_data_get_int(settings, "chatWidth");
		auto browserHeight = obs_data_get_int(settings, "chatHeight");
		obs_data_set_int(browser_settings, S_WIDTH, browserWidth);
		obs_data_set_int(browser_settings, S_HEIGHT, browserHeight);
		obs_source_update(context->m_browser, browser_settings);
		obs_data_release(browser_settings);

		ct_bk_render_data(settings, index, false);
		ct_bk_enable_changed(data, props, property, settings);
	}

	context->currentTemplateId = index;
	return true;
}

static bool ct_bk_template_list_changed(void *data, obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	auto index = obs_data_get_int(settings, S_CT_SOURCE_BK_TEMPLATE_LIST);
	auto context = (chat_template_source *)(data);
	if (!context)
		return false;
	if (index == context->currentBKTemplateId || index == -1 || isChatBkFirstShow) {
		isChatBkFirstShow = false;
	} else {
		ct_bk_sub_view_changed(props, settings, index);
		obs_data_set_int(settings, S_CT_SOURCE_BK_COLOR_MODE, s_chatBackgroundAttrs.value(index).isCustomChatBackgroundColor);
		obs_data_set_int(settings, S_CT_SOURCE_BK_COLOR_MODE_DEFAULT, 0);
		obs_data_set_string(settings, S_CT_SOURCE_BK_COLOR_MODE_CUSTOM, s_chatBackgroundAttrs.value(index).chatBackgroundCustomColorList.join(',').toUtf8().constData());
		auto name = s_chatBackgroundAttrs.value(index).chatBackgroundName;
		obs_data_set_bool(settings, S_CT_SOURCE_BK_COLOR_MODE_DEFAULT_VISABLE_LABEL, pls_is_equal(name, "HUD", Qt::CaseInsensitive));
		ct_bk_default_color_view_changed(props, index);
		ct_bk_default_color_view_reset(props, settings);
	}
	context->currentBKTemplateId = index;
	return true;
}

static obs_properties_t *chat_source_get_properties(void *data)
{
	isFirstShow = true;
	obs_properties_t *properties = obs_properties_create();
	auto ct_tab_prop = pls_properties_tm_add_tab(properties, S_CT_SOURCE_TAB);
	obs_property_set_modified_callback2(ct_tab_prop, ct_tab_changed, data);

	auto ct_template_tab_prop = pls_properties_tm_add_template_tab(properties, S_CT_SOURCE_TEMPLATE);
	obs_property_set_long_description(ct_template_tab_prop, obs_module_text(S_CT_SOURCE_TEMPLATE_DESC));

	auto chat_template_list = pls_properties_tm_add_template_list(properties, S_CT_SOURCE_TEMPLATE_LIST);
	obs_property_set_long_description(chat_template_list, obs_module_text(S_CT_SOURCE_TEMPLATE_LIST));
	obs_property_set_modified_callback2(chat_template_list, ct_template_list_changed, data);

	pls_properties_add_display(properties, S_CT_SOURCE_DISPLAY, obs_module_text(S_CT_SOURCE_DISPLAY));
	pls_properties_add_options(properties, S_CT_SOURCE_OPTIONS, obs_module_text(S_CT_SOURCE_OPTIONS));
	pls_properties_add_motion(properties, S_CT_SOURCE_MOTION, obs_module_text(S_CT_SOURCE_MOTION));
	pls_properties_add_font(properties, S_CT_SOURCE_FONT, obs_module_text(S_CT_SOURCE_FONT));
	auto text_color_prop = pls_properties_add_text_color(properties, S_CT_SOURCE_TEXT_COLOR, obs_module_text(S_CT_SOURCE_TEXT_COLOR));
	obs_property_set_long_description(text_color_prop, obs_module_text(S_CT_SOURCE_TEMPLATE_DESC));

	pls_properties_add_bk_color(properties, S_CT_SOURCE_BK_COLOR, obs_module_text(S_CT_SOURCE_BK_COLOR));

	obs_property_t *chat_background_enable_prop = pls_properties_add_bool_group(properties, S_CT_SOURCE_BK_CONTROL, obs_module_text(S_CT_SOURCE_BK_CONTROL));
	//obs_property_set_long_description(chat_background_enable_prop, obs_module_text(S_CT_SOURCE_BK_CONTROL));
	pls_property_bool_group_add_item(chat_background_enable_prop, nullptr, obs_module_text(S_CT_SOURCE_BK_ENABLE), nullptr, nullptr);
	pls_property_bool_group_add_item(chat_background_enable_prop, nullptr, obs_module_text(S_CT_SOURCE_BK_DISABLE), nullptr, nullptr);
	obs_property_set_modified_callback2(chat_background_enable_prop, ct_bk_enable_changed, data);

	pls_properties_add_line(properties, S_CT_SOURCE_BK_H_LINE_1, "");

	auto chat_bk_template_list = pls_properties_add_bk_template_list(properties, S_CT_SOURCE_BK_TEMPLATE_LIST, obs_module_text(S_CT_SOURCE_BK_TEMPLATE_LIST));
	obs_property_set_long_description(chat_bk_template_list, obs_module_text(S_CT_SOURCE_BK_TEMPLATE_LIST));
	obs_property_set_modified_callback2(chat_bk_template_list, ct_bk_template_list_changed, data);

	pls_properties_add_line(properties, S_CT_SOURCE_BK_H_LINE_2, "");

	obs_property_t *chat_background_color_mode_prop = pls_properties_add_bool_group(properties, S_CT_SOURCE_BK_COLOR_MODE, obs_module_text(S_CT_SOURCE_BK_COLOR_MODE));
	//obs_property_set_long_description(chat_background_color_mode_prop, obs_module_text(S_CT_SOURCE_BK_COLOR_MODE));
	pls_property_bool_group_add_item(chat_background_color_mode_prop, nullptr, obs_module_text(S_CT_SOURCE_BK_COLOR_MODE_DEFAULT), nullptr, nullptr);
	pls_property_bool_group_add_item(chat_background_color_mode_prop, nullptr, obs_module_text(S_CT_SOURCE_BK_COLOR_MODE_CUSTOM), nullptr, nullptr);
	obs_property_set_modified_callback2(chat_background_color_mode_prop, ct_bk_mode_changed, data);

	pls_properties_add_color_toolbtn(properties, S_CT_SOURCE_BK_COLOR_MODE_CUSTOM, obs_module_text(S_CT_SOURCE_BK_COLOR_MODE_CUSTOM));

	auto keys = s_chatBackgroundAttrs.keys();
	for (auto key : keys) {
		auto p = pls_properties_add_image_group(properties, s_chatBackgroundAttrs.value(key).chatBackgroundName.toUtf8().constData(), obs_module_text(S_CT_SOURCE_BK_COLOR_MODE), 1, 13,
							PLS_IMAGE_STYLE_BORDER_PAINT_BUTTON);
		auto colors = s_chatBackgroundAttrs.value(key).chatBackgroundDefaultColorList;
		auto colorSize = colors.size();
		for (int i = 0; i < colorSize; ++i) {
			pls_property_image_group_add_item(p, "", colors.at(i).toUtf8().constData(), i, nullptr);
		}
	}

	pls_properties_add_line(properties, S_CT_SOURCE_BK_H_LINE_3, "");

	return properties;
}

static void chat_source_get_defaults(obs_data_t *settings)
{
	getDefaultChatBackgroundAttrs();
	getDefaultChatSourceAttrs();
	obs_data_set_default_int(settings, S_CT_SOURCE_TAB, 0);
	obs_data_set_default_int(settings, S_CT_SOURCE_TEMPLATE, 0);
	obs_data_set_default_int(settings, S_CT_SOURCE_TEMPLATE_LIST, s_defaultId);
	set_ct_display_data(settings, s_defaultId, true);
	set_ct_options_data(settings, s_defaultId, true);
	set_ct_motion_data(settings, s_defaultId, true);
	set_ct_font_data(settings, s_defaultId, true);
	set_ct_text_color_data(settings, s_defaultId, true);
	set_ct_bk_color_data(settings, s_defaultId, true);
	obs_data_set_default_int(settings, S_CT_SOURCE_BK_CONTROL, 1);
	obs_data_set_default_int(settings, S_CT_SOURCE_BK_TEMPLATE_LIST, 0);
	obs_data_set_default_int(settings, S_CT_SOURCE_BK_COLOR_MODE, 0);
	obs_data_set_default_int(settings, S_CT_SOURCE_BK_COLOR_MODE_DEFAULT, 0);
	obs_data_set_default_string(settings, S_CT_SOURCE_BK_COLOR_MODE_CUSTOM, s_chatBackgroundAttrs.value(0).chatBackgroundCustomColorList.join(',').toUtf8().constData());
	obs_data_set_default_bool(settings, S_CT_SOURCE_BK_COLOR_MODE_DEFAULT_VISABLE_LABEL, false);
	obs_data_set_default_int(settings, S_CT_SOURCE_BK_TAB_NEW, 3);
}

static void source_notified(void *data, calldata_t *calldata)
{
	pls_used(calldata);
	auto context = static_cast<struct chat_template_source *>(data);
	auto source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (!source || (source != context->m_browser) || !data)
		return;

	auto type = (int)calldata_int(calldata, "message");
	switch (type) {
	case OBS_SOURCE_BROWSER_LOADED:
		context->sendNotifyAsync(OBS_SOURCE_CHAT_UPDATE_PARAMS, OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_LOADED);
		break;
	default:
		break;
	}
}
static void init_browser_source(struct chat_template_source *context)
{
	if (!context || context->m_browser) {
		return;
	}
	QString serviceName;
	auto settings = pls_get_source_setting(context->m_source);
	if (pls_is_ncp_first_login(serviceName)) {
		auto displayObj = obs_data_get_obj(settings, S_CT_SOURCE_DISPLAY);
		QString selectPlatforms = obs_data_get_string(displayObj, "selectPlatformList");
		auto selectPlatformList = selectPlatforms.split(';');
		auto ncpName = "NCP_" + serviceName;
		if (!selectPlatformList.contains(ncpName)) {
			auto newSelect = ncpName + ";" + selectPlatforms;
			obs_data_set_string(displayObj, "selectPlatformList", newSelect.toUtf8().constData());
			obs_data_set_obj(settings, S_CT_SOURCE_DISPLAY, displayObj);
		}
		obs_data_release(displayObj);
	}
	// init browser source

	QUrl URL = QUrl(CHATV2_SOURCE_URL);
	auto data = context->getData();
	QUrlQuery query;
	auto lang = pls_get_current_language_short_str();
	if (lang != "ko") {
		lang = "en";
	}
	query.addQueryItem(QStringLiteral("lang"), QUrl::toPercentEncoding(lang));
	QString strData = QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));
	query.addQueryItem(QStringLiteral("config"), QUrl::toPercentEncoding(strData));
	URL.setQuery(query);
	obs_data_t *browser_settings = obs_data_create();
	obs_data_set_string(browser_settings, "url", URL.toString(QUrl::FullyEncoded).toUtf8().constData());
	obs_data_set_bool(browser_settings, "is_local_file", false);
	obs_data_set_bool(browser_settings, "reroute_audio", true);
	obs_data_set_bool(browser_settings, "ignore_reload", true);
	context->m_browserWidth = obs_data_get_int(settings, "chatWidth");
	context->m_browserHeight = obs_data_get_int(settings, "chatHeight");
	context->m_chatWidth = context->m_browserWidth + 2 * CT_X_MARGIN;
	context->m_chatHeight = context->m_browserHeight + 2 * CT_Y_MARGIN;
	obs_data_set_int(browser_settings, S_WIDTH, context->m_browserWidth);
	obs_data_set_int(browser_settings, S_HEIGHT, context->m_browserHeight);

	context->m_browser = obs_source_create_private("browser_source", "prism_chat_browser_source", browser_settings);
	obs_data_release(browser_settings);

	signal_handler_connect_ref(obs_get_signal_handler(), "source_notify", source_notified, context);

	obs_source_inc_active(context->m_browser);
	obs_source_inc_showing(context->m_browser);
}
static void source_created(void *data, calldata_t *calldata)
{
	auto context = static_cast<struct chat_template_source *>(data);
	auto source = (uint64_t)calldata_int(calldata, "source_address");
	if (!source || (source != (uint64_t)context->m_source) || !data)
		return;
	init_browser_source(context);
}

static void chat_browser_render_proc(void *data, calldata_t *cd)
{
	auto context = (chat_template_source *)(data);
	if (!context) {
		return;
	}
	if (context->m_browser)
		obs_source_video_render(context->m_browser);
	UNUSED_PARAMETER(cd);
	return;
}

static void get_browser_size_proc(void *data, calldata_t *cd)
{
	auto context = (chat_template_source *)(data);
	if (!context) {
		return;
	}
	calldata_set_int(cd, "width", context->m_browserWidth);
	calldata_set_int(cd, "height", context->m_browserHeight);
	return;
}

static void *chat_source_create(obs_data_t *settings, obs_source_t *source)
{
	//obs_source_set_capture_valid(source, true, OBS_SOURCE_ERROR_OK);

	auto context = pls_new_nothrow<chat_template_source>();
	if (!context) {
		PLS_PLUGIN_ERROR("viewer count source create failed, because out of memory.");
		//obs_source_set_capture_valid(source, false, OBS_SOURCE_ERROR_UNKNOWN);
		return nullptr;
	}
	signal_handler_connect_ref(obs_get_signal_handler(), "source_create_finished", source_created, context);

	proc_handler_t *ph = obs_source_get_proc_handler(source);
	proc_handler_add(ph, "void chat_browser_render()", chat_browser_render_proc, context);
	proc_handler_add(ph, "void get_browser_size(out int width, out int height)", get_browser_size_proc, context);

	context->m_source = source;
	context->update(settings);
	return context;
}
static void chat_source_destroy(void *data)
{
	auto context = (chat_template_source *)(data);
	signal_handler_disconnect(obs_get_signal_handler(), "source_create_finished", source_created, context);

	if (context->m_browser) {
		signal_handler_disconnect(obs_get_signal_handler(), "source_notify", source_notified, context);
		obs_source_dec_active(context->m_browser);
		obs_source_release(context->m_browser);
	}

	if (context->m_source_texture) {
		obs_enter_graphics();
		gs_texture_destroy(context->m_source_texture);
		context->m_source_texture = nullptr;
		obs_leave_graphics();
	}

	pls_delete(context);
}
static void chat_source_activate(void *data)
{
	if (auto context = (chat_template_source *)(data); context->m_browser) {
		obs_source_inc_active(context->m_browser);
	}
}
static void chat_source_deactivate(void *data)
{
	if (auto context = (chat_template_source *)(data); context->m_browser) {
		obs_source_dec_active(context->m_browser);
	}
}
static void chat_source_clear_texture(gs_texture_t *tex)
{
	if (!tex) {
		return;
	}
	obs_enter_graphics();
	gs_texture_t *pre_rt = gs_get_render_target();
	gs_projection_push();
	gs_set_render_target(tex, nullptr);
	struct vec4 clear_color = {0};
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);
	gs_set_render_target(pre_rt, nullptr);
	gs_projection_pop();
	obs_leave_graphics();
}
static void chat_source_video_render(void *data, gs_effect_t *effect)
{
	auto context = (chat_template_source *)(data);

	const bool srgb = gs_get_color_space() == GS_CS_SRGB;
	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(!srgb);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	gs_eparam_t *const param = gs_effect_get_param_by_name(effect, "image");
	if (srgb)
		gs_effect_set_texture(param, context->m_source_texture);
	else
		gs_effect_set_texture_srgb(param, context->m_source_texture);

	gs_draw_sprite(context->m_source_texture, 0, 0, 0);

	gs_blend_state_pop();
	gs_enable_framebuffer_srgb(previous);
}
static void chat_source_render(void *data, obs_source_t *source)
{
	if (!data)
		return;

	auto vc_source = (chat_template_source *)(data);
	uint32_t source_width = obs_source_get_width(vc_source->m_source);
	uint32_t source_height = obs_source_get_height(vc_source->m_source);

	// To avoid the web screen flashing the previous template screen when switching templates. Especially when switching between template 5 and other templates, it is easy to reproduce
	// Therefore, when Template 5 is switched, the texture will be cleared and a black frame will be displayed at about 250 milliseconds during rendering to avoid the appearance of the previous template's image.
	struct obs_video_info ovi;
	obs_get_video_info(&ovi);
	float fps = ovi.fps_den ? ovi.fps_num / ovi.fps_den : 30.0f;
	int count = 250 / fps; //250ms

	if (source_width <= 0 || source_height <= 0 || (vc_source->template5Changed && vc_source->renderCount <= count)) {
		chat_source_clear_texture(vc_source->m_source_texture);
		if (vc_source->template5Changed) {
			vc_source->renderCount++;
			if (vc_source->renderCount == count) {
				vc_source->template5Changed = false;
				vc_source->renderCount = 0;
			}
		}
		return;
	}
	obs_enter_graphics();
	if (vc_source->m_source_texture) {
		uint32_t tex_width = gs_texture_get_width(vc_source->m_source_texture);
		uint32_t tex_height = gs_texture_get_height(vc_source->m_source_texture);
		if (tex_width != source_width || tex_height != source_height) {
			gs_texture_destroy(vc_source->m_source_texture);
			vc_source->m_source_texture = nullptr;
		}
	}

	if (!vc_source->m_source_texture) {
		vc_source->m_source_texture = gs_texture_create(source_width, source_height, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
	}

	gs_texture_t *pre_rt = gs_get_render_target();
	gs_viewport_push();
	gs_projection_push();
	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);

	gs_set_render_target(vc_source->m_source_texture, nullptr);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);
	gs_set_viewport(CT_X_MARGIN, CT_Y_MARGIN, source_width - (CT_X_MARGIN * 2), source_height - (CT_Y_MARGIN * 2));

	gs_ortho(0.0f, float(vc_source->m_browserWidth), 0.0f, float(vc_source->m_browserHeight), -100.0f, 100.0f);
	obs_source_video_render(source);

	gs_set_render_target(pre_rt, nullptr);
	gs_viewport_pop();
	gs_projection_pop();

	obs_leave_graphics();
}

static void chat_source_video_tick(void *data, float /*seconds*/)
{
	if (auto context = (chat_template_source *)(data); context->m_browser) {
		chat_source_render(data, context->m_browser);
	}
}
static void chat_cef_dispatch_js(void *data, const char *event_name, const char *json_data)
{
	if (auto context = (chat_template_source *)(data); context->m_browser) {
		pls_source_dispatch_cef_js(context->m_browser, event_name, json_data);
	}
}
static uint32_t chat_width(void *data)
{
	if (!data) {
		return CT_CHAT_WIDTH;
	}
	auto source = static_cast<chat_template_source *>(data);
	return source->m_chatWidth;
}
static uint32_t chat_height(void *data)
{
	if (!data) {
		return CT_CHAT_HEIGHT;
	}
	auto source = static_cast<chat_template_source *>(data);
	return source->m_chatHeight;
}

static void chat_template_set_private_data(void *data, obs_data_t *private_data)
{
	if (!data || !private_data) {
		return;
	}

	auto source = static_cast<chat_template_source *>(data);
	auto width = obs_data_get_int(private_data, S_WIDTH);
	auto height = obs_data_get_int(private_data, S_HEIGHT);

	auto browser_settings = obs_source_get_settings(source->m_browser);
	obs_data_set_int(browser_settings, S_WIDTH, width);
	obs_data_set_int(browser_settings, S_HEIGHT, height);
	obs_source_update(source->m_browser, browser_settings);
	obs_data_release(browser_settings);
	source->m_browserWidth = width;
	source->m_browserHeight = height;
	source->m_chatWidth = width + 2 * CT_X_MARGIN;
	source->m_chatHeight = height + 2 * CT_Y_MARGIN;
	auto source_settings = obs_source_get_settings(source->m_source);
	obs_data_set_bool(source_settings, "ctParamChanged", true);
	obs_data_set_int(source_settings, "chatWidth", width);
	obs_data_set_int(source_settings, "chatHeight", height);
	obs_source_update(source->m_source, source_settings);
	obs_data_release(source_settings);
	return;
}

static void chat_template_get_private_data(void *data, obs_data_t *private_data)
{
	if (!data || !private_data) {
		return;
	}
	auto source = static_cast<chat_template_source *>(data);

	obs_data_set_int(private_data, S_WIDTH, source->m_browserWidth);
	obs_data_set_int(private_data, S_HEIGHT, source->m_browserHeight);
	obs_data_set_int(private_data, "TemplateId", source->currentTemplateId);
}
static void check_source_is_paid(void *data, obs_data_t *private_data)
{

	if (!data || !private_data) {
		return;
	}
	auto source = static_cast<chat_template_source *>(data);
	auto source_settings = obs_source_get_settings(source->m_source);

	bool isPaid = s_paidIds.find(obs_data_get_int(source_settings, S_CT_SOURCE_BK_TEMPLATE_LIST)) != s_paidIds.end() && (obs_data_get_int(source_settings, S_CT_SOURCE_BK_CONTROL)) == 0;
	obs_data_set_bool(private_data, API_PAID_KEY_NAME, isPaid);
	obs_data_release(source_settings);
}
void register_prism_template_chat_source()
{
	struct obs_source_info info = {0};
	info.id = "prism_chatv2_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_properties = chat_source_get_properties;
	info.get_defaults = chat_source_get_defaults;
	info.activate = chat_source_activate;
	info.deactivate = chat_source_deactivate;

	info.get_name = [](void *) { return obs_module_text("ChatSource"); };
	info.create = chat_source_create;
	info.destroy = chat_source_destroy;
	info.update = [](void *data, obs_data_t *settings) { static_cast<chat_template_source *>(data)->update(settings); };
	info.get_width = chat_width;
	info.get_height = chat_height;
	info.video_tick = chat_source_video_tick;
	info.video_render = chat_source_video_render;
	info.icon_type = static_cast<obs_icon_type>(PLS_ICON_TYPE_CHAT_TEMPLATE);

	pls_source_info psi = {0};
	psi.properties_edit_start = [](void *data, obs_data_t *settings) { static_cast<chat_template_source *>(data)->propertiesEditStart(settings); };
	psi.properties_edit_end = [](void *data, obs_data_t *settings, bool) { static_cast<chat_template_source *>(data)->propertiesEditEnd(settings); };
	psi.update_extern_params = [](void *data, const calldata_t *extern_params) { static_cast<chat_template_source *>(data)->updateExternParamsAsync(extern_params); };
	psi.get_private_data = chat_template_get_private_data;
	psi.set_private_data = chat_template_set_private_data;
	psi.check_obs_source_settings = check_source_is_paid;
	register_pls_source_info(&info, &psi);
	obs_register_source(&info);

	if (!chat_template_source::asyncThread) {
		chat_template_source::asyncThread = new QThread();
		chat_template_source::asyncThread->start();
	}
}

void release_prism_chat_source()
{
	if (chat_template_source::asyncThread) {
		QThread *asyncThread = chat_template_source::asyncThread;
		chat_template_source::asyncThread = nullptr;

		asyncThread->quit();
		asyncThread->wait();
		delete asyncThread;
	}
}

ChatSourceAsynInvoke::ChatSourceAsynInvoke(chat_template_source *chatSource_) : chatSource(chatSource_)
{
	moveToThread(chat_template_source::asyncThread);
}

ChatSourceAsynInvoke::~ChatSourceAsynInvoke() {}

void ChatSourceAsynInvoke::setChatSource(chat_template_source *chatSource)
{
	QWriteLocker locker(&chatSourceLock);
	this->chatSource = chatSource;
}

void ChatSourceAsynInvoke::sendNotify(int type, int sub_code)
{
	QReadLocker locker(&chatSourceLock);
	if (chatSource) {
		chatSource->sendNotify(type, sub_code);
	}
}

void ChatSourceAsynInvoke::updateExternParams(const QByteArray &cjson, int sub_code)
{
	QReadLocker locker(&chatSourceLock);
	if (chatSource) {
		chatSource->updateExternParams(cjson, sub_code);
	}
}

QThread *chat_template_source::asyncThread = nullptr;

chat_template_source::chat_template_source()
{
	getDefaultChatSourceAttrs();
	getDefaultChatBackgroundAttrs();
	updateMyChatSourceAttrs();

	asynInvoke = pls_new_nothrow<ChatSourceAsynInvoke>(this);
	m_netConnection = QObject::connect(pls::NetworkState::instance(), &pls::NetworkState::stateChanged, std::bind(&chat_template_source::networkStateCallbackFunc, this, std::placeholders::_1));
}

chat_template_source::~chat_template_source()
{
	QObject::disconnect(m_netConnection);
	asynInvoke->setChatSource(nullptr);
	asynInvoke->deleteLater();
}

void chat_template_source::update(obs_data_t *settings)
{
	auto browser_settings = obs_source_get_settings(m_browser);
	int width = obs_data_get_int(settings, "chatWidth");
	int height = obs_data_get_int(settings, "chatHeight");
	if (width != m_browserWidth || height != m_browserHeight) {
		m_browserWidth = width;
		m_browserHeight = height;
		m_chatWidth = m_browserWidth + 2 * CT_X_MARGIN;
		m_chatHeight = m_browserHeight + 2 * CT_Y_MARGIN;
		obs_data_set_int(browser_settings, S_WIDTH, m_browserWidth);
		obs_data_set_int(browser_settings, S_HEIGHT, m_browserHeight);
		obs_source_update(m_browser, browser_settings);
		sendNotifyAsync(OBS_SOURCE_CHAT_UPDATE_PARAMS, OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_RESIZE_VIEW);
	}
	obs_data_release(browser_settings);
	sendNotifyAsync(OBS_SOURCE_CHAT_UPDATE_PARAMS, OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_UPDATE);
}

void chat_template_source::propertiesEditStart(obs_data_t *settings)
{
	sendNotifyAsync(OBS_SOURCE_CHAT_UPDATE_PARAMS, OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_EDIT_START);
}

void chat_template_source::propertiesEditEnd(obs_data_t *settings) {}

void chat_template_source::dispatchJSEvent(const QByteArray &json)
{
	PLS_PLUGIN_INFO("chatEvent: %s", json.constData());

	//send event to Web
	if (m_browser && !json.isEmpty()) {
		pls_source_dispatch_cef_js(m_browser, "chatEvent", json.constData());
	}
}

QByteArray chat_template_source::toJson(const char *cjson, bool isForce)
{
	QJsonParseError error;
	QJsonObject setting = QJsonDocument::fromJson(QByteArray(cjson), &error).object();
	if (error.error != QJsonParseError::NoError) {
		return QByteArray();
	}

	QJsonObject data = setting.value("data").toObject();
	auto otherData = getData();
	for (auto it = otherData.constBegin(); it != otherData.constEnd(); ++it) {
		data.insert(it.key(), it.value());
	}
	setting.insert("data", data);

	QByteArray json = QJsonDocument(setting).toJson(QJsonDocument::Compact);
	if (cacheData == json && !isForce) {
		return QByteArray();
	}
	cacheData = json;
	return json;
}

void chat_template_source::sendNotifyAsync(int type, int subCode)
{
	QMetaObject::invokeMethod(asynInvoke, "sendNotify", Qt::QueuedConnection, Q_ARG(int, type), Q_ARG(int, subCode));
}

void chat_template_source::updateExternParamsAsync(const calldata_t *extern_params)
{
	const char *cjson = calldata_string(extern_params, "cjson");
	auto sub_code = calldata_int(extern_params, "sub_code");
	QMetaObject::invokeMethod(asynInvoke, "updateExternParams", Qt::QueuedConnection, Q_ARG(QByteArray, QByteArray(cjson)), Q_ARG(int, sub_code));
}

void chat_template_source::sendNotify(int type, int sub_code)
{
	pls_source_send_notify(m_source, static_cast<obs_source_event_type>(type), sub_code);
}

void chat_template_source::updateExternParams(const QByteArray &cjson, int sub_code)
{
	switch (sub_code) {
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_UPDATE:
		dispatchJSEvent(toJson(cjson, false));
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_EDIT_START:
		dispatchJSEvent(toJson(cjson, true));
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_LOADED:
		dispatchJSEvent(toJson(cjson, true));
		sendNotifyAsync(OBS_SOURCE_CHAT_UPDATE_PARAMS, OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_CHECK_LIVE);
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_CHECK_LIVE:
		dispatchJSEvent(toJson(cjson, true));
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_JSONLOADED: {
		OBSDataAutoRelease settings = obs_source_get_settings(m_source);
		update(settings);
	} break;
	}
}

void chat_template_source::networkStateCallbackFunc(bool accessible)
{
	if (!accessible || !m_browser) {
		return;
	}
	if (!pls_is_streaming() && !pls_is_rehearsaling()) {
		pls_source_invoke_method(m_browser, METHOD_REFRESH_BROWSER);
	}
}

static inline uint32_t rgb_to_bgr(uint32_t rgb)
{
	return ((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb & 0xFF0000) >> 16);
}

QJsonObject chat_template_source::getData() const
{
	auto settings = pls_get_source_setting(m_source);
	auto displayObj = obs_data_get_obj(settings, S_CT_SOURCE_DISPLAY);
	auto optionsObj = obs_data_get_obj(settings, S_CT_SOURCE_OPTIONS);
	auto motionObj = obs_data_get_obj(settings, S_CT_SOURCE_MOTION);
	QJsonObject data;
	auto webId = s_ChatSourceAttrs.value(obs_data_get_int(settings, S_CT_SOURCE_TEMPLATE_LIST)).webId;
	data.insert("templateID", webId);
	QString selectPlatforms = obs_data_get_string(displayObj, "selectPlatformList");
	selectPlatforms = modifyNCB2BService2Ncp(selectPlatforms);
	data.insert("showPlatforms", selectPlatforms);
	bool isCheckPlatformIcon = obs_data_get_bool(displayObj, "isCheckPlatformIcon");
	data.insert("isShowPlatformIcon", isCheckPlatformIcon);
	bool isCheckLevelIcon = obs_data_get_bool(displayObj, "isCheckLevelIcon");
	data.insert("isShowLevelIcon", isCheckLevelIcon);
	bool isCheckIdIcon = obs_data_get_bool(displayObj, "isCheckIdIcon");
	data.insert("isShowID", isCheckIdIcon);

	bool isCheckChatWrap = obs_data_get_bool(optionsObj, "isCheckChatWrap");
	data.insert("isChatWrap", isCheckChatWrap);
	bool isCheckLeftChatAlign = obs_data_get_bool(optionsObj, "isCheckLeftChatAlign");
	data.insert("isChatLeft", isCheckLeftChatAlign);
	bool isCheckChatDisapperEffect = obs_data_get_bool(optionsObj, "isCheckChatDisapperEffect");
	data.insert("isChatDisapperEffect", isCheckChatDisapperEffect);

	int chatMotionStyle = obs_data_get_int(motionObj, "chatMotionStyle");
	QString style = getMotionStyleString(chatMotionStyle);
	data.insert("chatMotion", style);
	bool isCheckChatMotion = obs_data_get_bool(motionObj, "isCheckChatMotion");
	data.insert("isCheckChatMotion", isCheckChatMotion);

	auto fontObj = obs_data_get_obj(settings, S_CT_SOURCE_FONT);
	//todo fontFamily fontStyle
	data.insert("fontFamily", obs_data_get_string(fontObj, "chatFontFamily"));
	data.insert("fontStyle", obs_data_get_string(fontObj, "chatFontStyle"));

	int fontSize = (int)obs_data_get_int(fontObj, "chatFontSize");
	data.insert("fontSize", fontSize);
	auto outlineColorInt = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(fontObj, "chatFontOutlineColor")));
	QString textLineColor = QString("%1").arg(QColor(outlineColorInt).name(QColor::HexRgb));
	webId.toInt() == 4 ? data.insert("fontOutlineColor", "") : data.insert("fontOutlineColor", textLineColor);

	int textLineSize = obs_data_get_int(fontObj, "chatFontOutlineSize");
	data.insert("fontOutlineSize", textLineSize);

	auto textObj = obs_data_get_obj(settings, S_CT_SOURCE_TEXT_COLOR);
	bool bSingleColor = obs_data_get_bool(textObj, "isCheckNickTextSingleColor");
	if (bSingleColor) {
		auto nickTextDefaultColor = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(textObj, "nickTextDefaultColor")));
		QString nickTextColor = QString("%1").arg(QColor(nickTextDefaultColor).name(QColor::HexRgb));
		data.insert("nickTextColor", nickTextColor);
	} else {
		data.insert("nickTextColor", "");
	}
	auto mgrNickTextColor = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(textObj, "mgrNickTextColor")));
	QString MgrTextColor = QString("%1").arg(QColor(mgrNickTextColor).name(QColor::HexRgb));
	data.insert("MgrTextColor", MgrTextColor);
	auto subcribeTextColor = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(textObj, "subcribeTextColor")));
	QString subcribeTextColorStr = QString("%1").arg(QColor(subcribeTextColor).name(QColor::HexRgb));
	data.insert("SubcribeTextColor", subcribeTextColorStr);
	auto messageTextColor = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(textObj, "messageTextColor")));
	QString messageTextColorStr = QString("%1").arg(QColor(messageTextColor).name(QColor::HexRgb));
	data.insert("messageTextColor", messageTextColorStr);

	auto bkObj = obs_data_get_obj(settings, S_CT_SOURCE_BK_COLOR);
	QString chatSingleColorStyle = getSingleColorStr(obs_data_get_int(bkObj, "chatSingleColorStyle"));
	data.insert("singleMsgStyle", chatSingleColorStyle);
	auto bkSingleColorInt = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(bkObj, "chatSingleBkColor")));
	auto bkSingleColorAlaph = obs_data_get_int(bkObj, "chatSingleBkAlpha") * 0.01 * 255;
	QString bkSingleColor = QString("%1%2").arg(QColor(bkSingleColorInt).name(QColor::HexRgb)).arg(QString::asprintf("%02x", static_cast<uint32_t>(bkSingleColorAlaph)));
	data.insert("singleMsgBackgroundColor", bkSingleColor);

	bool isCheckBkColor = obs_data_get_bool(bkObj, "isCheckChatTotalBkColor");
	QString bkColor;
	if (isCheckBkColor) {
		auto totalBkColorInt = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(bkObj, "chatTotalBkColor")));
		auto totalBkAlpha = obs_data_get_int(bkObj, "chatTotalBkAlpha") * 0.01 * 255;
		bkColor = QString("%1%2").arg(QColor(totalBkColorInt).name(QColor::HexRgb)).arg(QString::asprintf("%02x", static_cast<uint32_t>(totalBkAlpha)));
	}
	data.insert("totalMsgBackgroundColor", bkColor);

	auto chatWindowAlpha = obs_data_get_int(bkObj, "chatWindowAlpha") * 0.01;
	data.insert("chatWindowAlpha", chatWindowAlpha);

	auto bkControl = (obs_data_get_int(settings, S_CT_SOURCE_BK_CONTROL)) == 0;
	auto bkTemplateId = obs_data_get_int(settings, S_CT_SOURCE_BK_TEMPLATE_LIST);
	data.insert(QStringLiteral("borderType"), bkControl ? s_chatBackgroundAttrs.value(bkTemplateId).chatBackgroundWebType.toLower() : "");
	QJsonArray colorArray;
	auto isCustom = (obs_data_get_int(settings, S_CT_SOURCE_BK_COLOR_MODE)) == 1;
	auto appendColor = [&colorArray](int id, const QStringList &colorList) {
		for (auto color : colorList) {
			auto colorInt = rgb_to_bgr(static_cast<uint32_t>(color.toLongLong()));
			auto colorHex = QColor(colorInt).name(QColor::HexRgb);
			colorArray.append(colorHex);
		}
	};
	QStringList colorList;
	if (isCustom) {
		colorList = QString(obs_data_get_string(settings, S_CT_SOURCE_BK_COLOR_MODE_CUSTOM)).split(',');
	} else {
		colorList = s_chatBackgroundAttrs.value(bkTemplateId).chatBackgroundDefaultColorList;
		auto index = obs_data_get_int(settings, s_chatBackgroundAttrs.value(bkTemplateId).chatBackgroundName.toUtf8().constData());
		if (index < colorList.size()) {
			colorList = QString(colorList.value(index)).split('/');
		}
	}
	appendColor(bkTemplateId, colorList);

	data.insert("borderColor", colorArray);

	obs_data_release(displayObj);
	obs_data_release(optionsObj);
	obs_data_release(motionObj);
	obs_data_release(fontObj);
	obs_data_release(textObj);
	obs_data_release(bkObj);
	return data;
}

QString chat_template_source::modifyNCB2BService2Ncp(QString &selectPlatform) const
{
	auto selectPlatformList = selectPlatform.split(';');
	for (auto &name : selectPlatformList) {
		if (name.startsWith("NCP_", Qt::CaseInsensitive)) {
			name = "ncp";
		}
	}
	return selectPlatformList.join(';');
}
