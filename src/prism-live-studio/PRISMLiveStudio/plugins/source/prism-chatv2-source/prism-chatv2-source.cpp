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

#define S_CT_SOURCE_TEMPLATE "CTSource.Template"
#define S_CT_SOURCE_TEMPLATE_DESC "CTSource.Template.Desc"
#define S_CT_SOURCE_FONT_SIZE "CTSource.FontSize"
#define S_URL "url"
#define S_WIDTH "width"
#define S_HEIGHT "height"

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
};
static QMap<uint, ChatSourceAttr> s_ChatSourceAttrs;
static QStringList s_defaultFontList;
static uint s_defaultId = 0;
static bool isFirstShow = true;
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
	else if (0 == style.compare("bibid", Qt::CaseInsensitive))
		return 1;
	return -1;
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
		chatAttr.selectPlatformList = cdp.value("platform_type").toObject().contains("platform_list") ? tmpPlatformList.split(';') : getChannelWithChatList();
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

static void getDefaultChatSourceAttrs()
{
	if (!s_ChatSourceAttrs.isEmpty())
		return;

	QString chatSourceJsonPath = pls_get_app_data_dir(QStringLiteral("PRISMLiveStudio/library/Library_Policy_PC/chatv2source/")) + QStringLiteral("chatv2source.json");
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

	if (is_default) {
		obs_data_set_default_string(ct_display_data_obj, "selectPlatformList", attrs.selectPlatformList.join(';').toUtf8().constData());
		obs_data_set_default_bool(ct_display_data_obj, "isEnablePlatformType", attrs.isEnablePlatformType);
		obs_data_set_default_bool(ct_display_data_obj, "isEnablePlatformIcon", attrs.isEnablePlatformIcon);
		obs_data_set_default_bool(ct_display_data_obj, "isCheckPlatformIcon", attrs.isCheckPlatformIcon);
		obs_data_set_default_bool(ct_display_data_obj, "isEnableLevelIcon", attrs.isEnableLevelIcon);
		obs_data_set_default_bool(ct_display_data_obj, "isCheckLevelIcon", attrs.isCheckLevelIcon);
		obs_data_set_default_bool(ct_display_data_obj, "isEnableIdIcon", attrs.isEnableIdIcon);
		obs_data_set_default_bool(ct_display_data_obj, "isCheckIdIcon", attrs.isCheckIdIcon);
		obs_data_set_default_bool(ct_display_data_obj, "isEnablePlatformIconDisplay", attrs.isEnablePlatformIconDisplay);
		obs_data_set_default_bool(ct_display_data_obj, "isEnableChatDisplay", attrs.isEnableChatDisplay);
		obs_data_set_default_obj(settings, S_CT_SOURCE_DISPLAY, ct_display_data_obj);
	} else {
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
		obs_data_set_obj(settings, S_CT_SOURCE_DISPLAY, ct_display_data_obj);
	}

	obs_data_release(ct_display_data_obj);
}
static void set_ct_options_data(obs_data_t *settings, const uint &id, bool is_default)
{
	obs_data_t *ct_options_data_obj = obs_data_create();
	auto attrs = s_ChatSourceAttrs.value(id);

	if (is_default) {
		obs_data_set_default_bool(ct_options_data_obj, "isEnableChatWrap", attrs.isEnableChatWrap);
		obs_data_set_default_bool(ct_options_data_obj, "isCheckChatWrap", attrs.isCheckChatWrap);
		obs_data_set_default_bool(ct_options_data_obj, "isCheckLeftChatAlign", attrs.isCheckLeftChatAlign);
		obs_data_set_default_bool(ct_options_data_obj, "isEnableChatAlign", attrs.isEnableChatAlign);
		obs_data_set_default_bool(ct_options_data_obj, "isCheckChatDisapperEffect", attrs.isCheckChatDisapperEffect);
		obs_data_set_default_bool(ct_options_data_obj, "isEnableChatDisapperEffect", attrs.isEnableChatDisapperEffect);
		obs_data_set_default_int(ct_options_data_obj, "chatWidth", attrs.chatWidth);
		obs_data_set_default_int(ct_options_data_obj, "chatHeight", attrs.chatHeight);
		obs_data_set_default_bool(ct_options_data_obj, "isEnableChatBoxSize", attrs.isEnableChatBoxSize);
		obs_data_set_default_bool(ct_options_data_obj, "isEnableChatOption", attrs.isEnableChatOption);
		obs_data_set_default_obj(settings, S_CT_SOURCE_OPTIONS, ct_options_data_obj);

	} else {
		obs_data_set_bool(ct_options_data_obj, "isEnableChatWrap", attrs.isEnableChatWrap);
		obs_data_set_bool(ct_options_data_obj, "isCheckChatWrap", attrs.isCheckChatWrap);
		obs_data_set_bool(ct_options_data_obj, "isCheckLeftChatAlign", attrs.isCheckLeftChatAlign);
		obs_data_set_bool(ct_options_data_obj, "isEnableChatAlign", attrs.isEnableChatAlign);
		obs_data_set_bool(ct_options_data_obj, "isCheckChatDisapperEffect", attrs.isCheckChatDisapperEffect);
		obs_data_set_bool(ct_options_data_obj, "isEnableChatDisapperEffect", attrs.isEnableChatDisapperEffect);
		obs_data_set_int(ct_options_data_obj, "chatWidth", attrs.chatWidth);
		obs_data_set_int(ct_options_data_obj, "chatHeight", attrs.chatHeight);
		obs_data_set_bool(ct_options_data_obj, "isEnableChatBoxSize", attrs.isEnableChatBoxSize);
		obs_data_set_bool(ct_options_data_obj, "isEnableChatOption", attrs.isEnableChatOption);
		obs_data_set_obj(settings, S_CT_SOURCE_OPTIONS, ct_options_data_obj);
	}

	obs_data_release(ct_options_data_obj);
}
static void set_ct_motion_data(obs_data_t *settings, const uint &id, bool is_default)
{
	obs_data_t *ct_motion_data_obj = obs_data_create();
	auto attrs = s_ChatSourceAttrs.value(id);
	if (is_default) {
		obs_data_set_default_bool(ct_motion_data_obj, "isEnableChatMotion", attrs.isEnableChatMotion);
		obs_data_set_default_bool(ct_motion_data_obj, "isCheckChatMotion", attrs.isCheckChatMotion);
		obs_data_set_default_int(ct_motion_data_obj, "chatMotionStyle", attrs.chatMotionStyle);
		obs_data_set_default_obj(settings, S_CT_SOURCE_MOTION, ct_motion_data_obj);
	} else {
		obs_data_set_bool(ct_motion_data_obj, "isEnableChatMotion", attrs.isEnableChatMotion);
		obs_data_set_bool(ct_motion_data_obj, "isCheckChatMotion", attrs.isCheckChatMotion);
		obs_data_set_int(ct_motion_data_obj, "chatMotionStyle", attrs.chatMotionStyle);
		obs_data_set_obj(settings, S_CT_SOURCE_MOTION, ct_motion_data_obj);
	}

	obs_data_release(ct_motion_data_obj);
}
static void set_ct_font_data(obs_data_t *settings, const uint &id, bool is_default)
{
	obs_data_t *ct_font_data_obj = obs_data_create();
	auto attrs = s_ChatSourceAttrs.value(id);
	if (is_default) {
		obs_data_set_default_string(ct_font_data_obj, "chatFontFamily", attrs.chatFontFamily.toUtf8());
		obs_data_set_default_string(ct_font_data_obj, "chatFontStyle", attrs.chatFontStyle.toUtf8());
		obs_data_set_default_bool(ct_font_data_obj, "isEnableChatCommonFont", attrs.isEnableChatCommonFont);
		obs_data_set_default_int(ct_font_data_obj, "chatFontSize", attrs.chatFontSize);
		obs_data_set_default_bool(ct_font_data_obj, "isEnableChatFontSize", attrs.isEnableChatFontSize);
		obs_data_set_default_int(ct_font_data_obj, "chatFontOutlineSize", attrs.chatFontOutlineSize);
		obs_data_set_default_int(ct_font_data_obj, "chatFontOutlineColor", attrs.chatFontOutlineColor);
		obs_data_set_default_bool(ct_font_data_obj, "isEnableChatFontOutlineColor", attrs.isEnableChatFontOutlineColor);
		obs_data_set_default_bool(ct_font_data_obj, "isEnableChatFontOutlineSize", attrs.isEnableChatFontOutlineSize);
		obs_data_set_default_bool(ct_font_data_obj, "isEnableChatFont", attrs.isEnableChatFont);
		obs_data_set_default_obj(settings, S_CT_SOURCE_FONT, ct_font_data_obj);

	} else {
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
		obs_data_set_obj(settings, S_CT_SOURCE_FONT, ct_font_data_obj);
	}

	obs_data_release(ct_font_data_obj);
}
static void set_ct_text_color_data(obs_data_t *settings, const uint &id, bool is_default)
{
	obs_data_t *ct_text_color_data_obj = obs_data_create();
	auto attrs = s_ChatSourceAttrs.value(id);
	if (is_default) {
		obs_data_set_default_bool(ct_text_color_data_obj, "isCheckNickTextSingleColor", attrs.isCheckNickTextSingleColor);
		obs_data_set_default_int(ct_text_color_data_obj, "nickTextDefaultColor", attrs.nickTextDefaultColor);
		obs_data_set_default_bool(ct_text_color_data_obj, "isEnableSingleNickTextColor", attrs.isEnableSingleNickTextColor);
		obs_data_set_default_bool(ct_text_color_data_obj, "isEnableRadomNickTextColor", attrs.isEnableRadomNickTextColor);
		obs_data_set_default_int(ct_text_color_data_obj, "mgrNickTextColor", attrs.mgrNickTextColor);
		obs_data_set_default_bool(ct_text_color_data_obj, "isEnableMgrNickTextColor", attrs.isEnableMgrNickTextColor);
		obs_data_set_default_int(ct_text_color_data_obj, "subcribeTextColor", attrs.subcribeTextColor);
		obs_data_set_default_bool(ct_text_color_data_obj, "isEnableSubcribeTextColor", attrs.isEnableSubcribeTextColor);
		obs_data_set_default_int(ct_text_color_data_obj, "messageTextColor", attrs.messageTextColor);
		obs_data_set_default_bool(ct_text_color_data_obj, "isEnableMessageTextColor", attrs.isEnableMessageTextColor);
		obs_data_set_default_bool(ct_text_color_data_obj, "isEnableChatTextColor", attrs.isEnableChatTextColor);
		obs_data_set_default_obj(settings, S_CT_SOURCE_TEXT_COLOR, ct_text_color_data_obj);

	} else {
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
		obs_data_set_obj(settings, S_CT_SOURCE_TEXT_COLOR, ct_text_color_data_obj);
	}

	obs_data_release(ct_text_color_data_obj);
}
static void set_ct_bk_color_data(obs_data_t *settings, const uint &id, bool is_default)
{
	obs_data_t *ct_bk_color_data_obj = obs_data_create();
	auto attrs = s_ChatSourceAttrs.value(id);
	if (is_default) {
		obs_data_set_default_int(ct_bk_color_data_obj, "chatSingleBkColor", attrs.chatSingleBkColor);
		obs_data_set_default_int(ct_bk_color_data_obj, "chatSingleBkAlpha", attrs.chatSingleBkAlpha);
		obs_data_set_default_int(ct_bk_color_data_obj, "chatSingleColorStyle", attrs.chatSingleColorStyle);
		obs_data_set_default_bool(ct_bk_color_data_obj, "isCheckChatSingleBkColor", attrs.isCheckChatSingleBkColor);
		obs_data_set_default_bool(ct_bk_color_data_obj, "isEnableChatSingleBkColor", attrs.isEnableChatSingleBkColor);
		obs_data_set_default_int(ct_bk_color_data_obj, "chatTotalBkColor", attrs.chatTotalBkColor);
		obs_data_set_default_int(ct_bk_color_data_obj, "chatTotalBkAlpha", attrs.chatTotalBkAlpha);
		obs_data_set_default_bool(ct_bk_color_data_obj, "isCheckChatTotalBkColor", attrs.isCheckChatTotalBkColor);
		obs_data_set_default_bool(ct_bk_color_data_obj, "isEnableChatTotalBkColor", attrs.isEnableChatTotalBkColor);
		obs_data_set_default_int(ct_bk_color_data_obj, "chatWindowAlpha", attrs.chatWindowAlpha);
		obs_data_set_default_bool(ct_bk_color_data_obj, "isEnableChatBackgroundColor", attrs.isEnableChatBackgroundColor);
		obs_data_set_default_obj(settings, S_CT_SOURCE_BK_COLOR, ct_bk_color_data_obj);
	} else {
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
		obs_data_set_obj(settings, S_CT_SOURCE_BK_COLOR, ct_bk_color_data_obj);
	}

	obs_data_release(ct_bk_color_data_obj);
}

static bool ct_tab_changed(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
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

	obs_property_set_visible(ct_template_tab, index == 0);
	obs_property_set_visible(ct_template_list, index == 0);
	obs_property_set_visible(ct_display, index == 1);
	obs_property_set_visible(ct_options, index == 1);
	obs_property_set_visible(ct_motion, index == 1);
	obs_property_set_visible(ct_font, index == 2);
	obs_property_set_visible(ct_text_color, index == 2);
	obs_property_set_visible(ct_bk_color, index == 2);
	return true;
}
static bool ct_template_list_changed(void *data, obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	obs_data_set_bool(settings, "ctParamChanged", false);
	auto index = obs_data_get_int(settings, S_CT_SOURCE_TEMPLATE_LIST);
	auto context = (chat_template_source *)(data);
	if (!context)
		return false;
	if (context->currentTemplateId == index || s_ChatSourceAttrs.find(index) == s_ChatSourceAttrs.end() || isFirstShow) {
		isFirstShow = false;
	} else {
		set_ct_display_data(settings, index, false);
		set_ct_options_data(settings, index, false);
		set_ct_motion_data(settings, index, false);
		set_ct_font_data(settings, index, false);
		set_ct_text_color_data(settings, index, false);
		set_ct_bk_color_data(settings, index, false);
	}
	context->currentTemplateId = index;

	return true;
}

static obs_properties_t *chat_source_get_properties(void *data)
{
	isFirstShow = true;
	obs_properties_t *properties = obs_properties_create();
	auto ct_tab_prop = pls_properties_tm_add_tab(properties, S_CT_SOURCE_TAB);
	obs_property_set_modified_callback(ct_tab_prop, ct_tab_changed);

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
	return properties;
}

static void chat_source_get_defaults(obs_data_t *settings)
{
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
	if (pls_is_ncp_first_login(serviceName)) {
		auto settings = pls_get_source_setting(context->m_source);
		auto displayObj = obs_data_get_obj(settings, S_CT_SOURCE_DISPLAY);
		QString selectPlatforms = obs_data_get_string(displayObj, "selectPlatformList");
		auto selectPlatformList = selectPlatforms.split(';');
		if (!selectPlatformList.contains(serviceName)) {
			auto newSelect = serviceName + ";" + selectPlatforms;
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
	obs_data_set_int(browser_settings, S_WIDTH, CT_WIDTH);
	obs_data_set_int(browser_settings, S_HEIGHT, CT_HEIGHT);
	context->m_width = CT_WIDTH;
	context->m_height = CT_HEIGHT;
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
	calldata_set_int(cd, "width", context->m_width);
	calldata_set_int(cd, "height", context->m_height);
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
	signal_handler_add(obs_source_get_signal_handler(source), "void source_notify(ptr source, int message, int sub_code)");

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
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), context->m_source_texture);
	gs_draw_sprite(context->m_source_texture, 0, 0, 0);
	pls_used(effect);
}
static void chat_source_render(void *data, obs_source_t *source)
{
	if (!data)
		return;

	auto vc_source = (chat_template_source *)(data);
	uint32_t source_width = obs_source_get_width(vc_source->m_source);
	uint32_t source_height = obs_source_get_height(vc_source->m_source);
	if (source_width <= 0 || source_height <= 0) {
		chat_source_clear_texture(vc_source->m_source_texture);
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

	gs_ortho(0.0f, float(vc_source->m_width), 0.0f, float(vc_source->m_height), -100.0f, 100.0f);
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
	return CT_CHAT_WIDTH;
}
static uint32_t chat_height(void *data)
{
	return CT_CHAT_HEIGHT;
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
	source->m_width = width;
	source->m_height = height;
	auto source_settings = obs_source_get_settings(source->m_source);
	auto optionsObj = obs_data_get_obj(source_settings, S_CT_SOURCE_OPTIONS);
	obs_data_set_int(optionsObj, "chatWidth", width);
	obs_data_set_int(optionsObj, "chatHeight", height);
	obs_source_update(source->m_source, source_settings);
	obs_data_release(source_settings);
	obs_data_release(optionsObj);
	return;
}

static void chat_template_get_private_data(void *data, obs_data_t *private_data)
{
	if (!data || !private_data) {
		return;
	}
	auto source = static_cast<chat_template_source *>(data);

	obs_data_set_int(private_data, S_WIDTH, source->m_width);
	obs_data_set_int(private_data, S_HEIGHT, source->m_height);
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
	auto optionsObj = obs_data_get_obj(settings, S_CT_SOURCE_OPTIONS);
	auto browser_settings = obs_source_get_settings(m_browser);
	int width = obs_data_get_int(optionsObj, "chatWidth");
	int height = obs_data_get_int(optionsObj, "chatHeight");
	if (width != m_width || height != m_height) {
		m_width = width;
		m_height = height;
		obs_data_set_int(browser_settings, S_WIDTH, m_width);
		obs_data_set_int(browser_settings, S_HEIGHT, m_height);
		obs_source_update(m_browser, browser_settings);
		sendNotifyAsync(OBS_SOURCE_CHAT_UPDATE_PARAMS, OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_RESIZE_VIEW);
	}
	obs_data_release(browser_settings);
	obs_data_release(optionsObj);

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
	if (m_browser) {
		pls_source_dispatch_cef_js(m_browser, "chatEvent", json.constData());
	}
}

QByteArray chat_template_source::toJson(const char *cjson) const
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
		dispatchJSEvent(toJson(cjson));
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_EDIT_START:
		dispatchJSEvent(toJson(cjson));
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_LOADED:
		dispatchJSEvent(toJson(cjson));
		sendNotifyAsync(OBS_SOURCE_CHAT_UPDATE_PARAMS, OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_CHECK_LIVE);
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_CHECK_LIVE:
		dispatchJSEvent(toJson(cjson));
		break;
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
	data.insert("templateID", s_ChatSourceAttrs.value(obs_data_get_int(settings, S_CT_SOURCE_TEMPLATE_LIST)).webId);
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
	data.insert("fontOutlineColor", textLineColor);
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
	data.insert("nickTextDefaultColor", subcribeTextColorStr);
	auto messageTextColor = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(textObj, "messageTextColor")));
	QString messageTextColorStr = QString("%1").arg(QColor(messageTextColor).name(QColor::HexRgb));
	data.insert("messageTextColor", messageTextColorStr);

	auto bkObj = obs_data_get_obj(settings, S_CT_SOURCE_BK_COLOR);
	QString chatSingleColorStyle = obs_data_get_string(bkObj, "chatSingleColorStyle");
	data.insert("singleMsgStyle", chatSingleColorStyle);
	auto bkSingleColorInt = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(bkObj, "chatSingleBkColor")));
	auto bkSingleColorAlaph = obs_data_get_int(bkObj, "chatSingleBkAlpha") * 0.01 * 255;
	QString bkSingleColor = QString("%1%2").arg(QColor(bkSingleColorInt).name(QColor::HexRgb)).arg(QString::asprintf("%02x", static_cast<uint32_t>(bkSingleColorAlaph)));
	data.insert("singleMsgBackgroundColor", bkSingleColor);

	auto totalBkColorInt = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(bkObj, "chatTotalBkColor")));
	auto totalBkAlpha = obs_data_get_int(bkObj, "chatTotalBkAlpha") * 0.01 * 255;
	QString bkColor = QString("%1%2").arg(QColor(totalBkColorInt).name(QColor::HexRgb)).arg(QString::asprintf("%02x", static_cast<uint32_t>(totalBkAlpha)));
	data.insert("totalMsgBackgroundColor", bkColor);

	auto chatWindowAlpha = obs_data_get_int(bkObj, "chatWindowAlpha") * 0.01;
	data.insert("chatWindowAlpha", chatWindowAlpha);

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
		if (pls_is_ncp(name)) {
			name = "ncp";
		}
	}
	return selectPlatformList.join(';');
}
