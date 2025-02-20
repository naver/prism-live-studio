#pragma once

#include "frontend-api-global.h"

#include <obs-frontend-api.h>

#include <functional>
#include <QUrl>
#include <QJsonObject>
#include <QMap>
#include <qlist.h>
#include <QVariant>
#include <QFileInfo>
#include <QObject>
#include <qsystemtrayicon.h>
#include "pls-net-url.hpp"
#include "cancel.hpp"
#include "obs.hpp"
#include <vector>
#include "utils-api.h"
#include <qlocale.h>
#include <qdialogbuttonbox.h>
#include <qpair.h>
#include "../prism-ui/main/audio-meter-wrapper.h"
#include "../prism-ui/main/pls-gpop-data.hpp"
#include "../prism-ui/scene-templates/PLSSceneTemplateModel.h"
#include "PLSErrorHandler.h"

class PLSLoginInfo;
class QWidget;
class QNetworkCookie;

#define PRISM_SSL pls_get_gpop_connection().ssl

enum class PLSResultCheckingResult { Ok, Continue, Close };

/**
  * browser result check
  * param:
  *     [out] result: project name
  *     [in] url: url
  *     [in] cookies: cookies
  * return:
  *     true for success
  */
using pls_result_checking_callback_t = std::function<PLSResultCheckingResult(QVariantHash &result, const QString &url, const QMap<QString, QString> &cookies)>;

using pls_translate_callback_t = const char *(*)(const char *lookup);
FRONTEND_API void pls_set_translate_cb(pls_translate_callback_t translate_cb);
FRONTEND_API const char *pls_translate(const char *lookup);
FRONTEND_API QString pls_translate_qstr(const char *lookup);

/**
  * register login module info
  * param:
  *     [in] login_info: login info
  * return:
  *     true for success, false for failed
  */
FRONTEND_API bool pls_register_login_info(PLSLoginInfo *login_info);
/**
  * unregister login module info
  * param:
  *     [in] login_info: login info
  */
FRONTEND_API void pls_unregister_login_info(PLSLoginInfo *login_info);
/**
  * get registered login module info count
  * return:
  *     count
  */
FRONTEND_API int pls_get_login_info_count();
/**
  * get registered login module info count
  * param:
  *     [in] index: index (0 ~ pls_get_login_info_count() - 1)
  * return:
  *     login info
  */
FRONTEND_API PLSLoginInfo *pls_get_login_info(int index);

FRONTEND_API void del_pannel_cookies(const QString &pannelName);
FRONTEND_API void pls_set_manual_cookies(const QString &pannelName);

/**
  * del special url cookie cache
  */
FRONTEND_API void pls_delete_specific_url_cookie(const QString &url, const QString &cookieName = QString());
/**
  * transfrom map to json
  * param:
  *     [in] ssmap: string map
  * return:
  *     json object
  */
FRONTEND_API QJsonObject pls_ssmap_to_json(const QMap<QString, QString> &ssmap);

/**
  * popup browser view
  * param:
  *     [out] result: result data, for example: token, cookies, ...
  *     [in] url: url
  *     [in] callback: browser result check callback
  *     [in-opt] parent: parent widget
  * return:
  *     json object
  */
FRONTEND_API bool pls_browser_view(QVariantHash &result, const QUrl &url, const pls_result_checking_callback_t &callback, QWidget *parent = nullptr, bool readCookies = true);
/**
  * popup browser view
  * param:
  *     [out] result: result data, for example: token, cookies, ...
  *     [in] url: url
  *     [in] headers: request headers
  *     [in] callback: browser result check callback
  *     [in-opt] parent: parent widget
  * return:
  *     json object
  */
FRONTEND_API bool pls_browser_view(QVariantHash &result, const QUrl &url, const std_map<std::string, std::string> &headers, const QString &pannelName, const pls_result_checking_callback_t &callback,
				   QWidget *parent = nullptr, bool readCookies = true);
/**
  * popup browser view
  * param:
  *     [out] result: result data, for example: token, cookies, ...
  *     [in] url: url
  *     [in] headers: request headers
  *     [in] script: javascript script
  *     [in] callback: browser result check callback
  *     [in-opt] parent: parent widget
  * return:
  *     json object
  */
FRONTEND_API bool pls_browser_view(QVariantHash &result, const QUrl &url, const std_map<std::string, std::string> &headers, const QString &pannelName, const std::string &script,
				   const pls_result_checking_callback_t &callback, QWidget *parent = nullptr, bool readCookies = true);

/**
  * popup rtmp view
  * param:
  *     [out] result: result data, for example: token, cookies, ...
  *     [in] login_info: login info
  *     [in-opt] parent: parent widget
  * return:
  *     true for success, false for failed
  */
FRONTEND_API bool pls_rtmp_view(QJsonObject &result, PLSLoginInfo *login_info, QWidget *parent = nullptr);

/**
  * popup channel login view
  * param:
  *     [out] result: result data, for example: token, cookies, ...
  *     [in-opt] parent: parent widget
  * return:
  *     true for success, false for failed
  */
FRONTEND_API bool pls_channel_login(QJsonObject &result, QWidget *parent = nullptr);

FRONTEND_API QList<QNetworkCookie> pls_get_cookies(const QJsonObject &cookies);

/**
  * close mainwindow and reset config show login view

  */
FRONTEND_API void pls_prism_change_over_login_view();

FRONTEND_API QString pls_prism_user_thumbnail_path();
FRONTEND_API void pls_prism_logout();
FRONTEND_API void pls_prism_signout();
/*
* get gpop common data
* return:
*	Common include stage,regionCode,deviceType,rtmpDestination datas.See Common struct for details
*/
FRONTEND_API QString pls_get_oauth_token_from_url(const QString &url_str);
/**
* get youtube code info from url
*/
FRONTEND_API QString pls_get_code_from_url(const QString &url_str);
/*
* channel login
*/
FRONTEND_API void pls_channel_login_async(const std::function<void(bool ok, const QVariantHash &result)> &callback, const QString &accountName, QWidget *parent);

//FRONTEND_API Common pls_get_gpop_common();
/*
* get sns login callback url
*/
FRONTEND_API QMap<QString, SnsCallbackUrl> pls_get_gpop_snscallback_urls();

/*
* get bulid Date
*/
FRONTEND_API long long pls_get_gpop_bulid_data();
/*
* get log level
*/
FRONTEND_API QString pls_get_gpop_log_level();

/*
* get connection data include apis and fallback .Connection struct for details.
*/
FRONTEND_API Connection pls_get_gpop_connection();

FRONTEND_API QMap<int, RtmpDestination> pls_get_rtmpDestination();
/*
* get gcc data
*/
FRONTEND_API QString pls_get_gcc_data();
/*
* get prism login token data
*/
FRONTEND_API QString pls_get_prism_token();
/*
* get prism login email data
*/
FRONTEND_API QString pls_get_prism_email();
/*
* get prism getprofileThumbanilUrl data
*/
FRONTEND_API QString pls_get_prism_thmbanilurl();
/*
* get prism nick name data
*/
FRONTEND_API QString pls_get_prism_nickname();
/*
* get prism nick name data
*/
FRONTEND_API QString pls_get_prism_usercode();

FRONTEND_API QByteArray pls_get_prism_cookie();

FRONTEND_API QString pls_get_b2b_auth_url();

FRONTEND_API bool pls_get_b2b_acctoken(const QString &url);

/**
  * get cpu, gpu, memory data
  * param:
  * return:
  *     e.g.	{"result": "OK", "data": {"CPU": 0.3, "GPU": 0.3, "memory": 200.3}}
  *				{"result": "fail", reason: ""}
  */
FRONTEND_API QJsonObject pls_get_resource_statistics_data();

/**
* alert window
*/
FRONTEND_API bool pls_click_alert_message();

FRONTEND_API bool pls_alert_message_visible();

FRONTEND_API int pls_alert_message_count();

/// <summary>
/// return all active channel information
/// </summary>
/// <returns>first is platform, second is share url</returns>
FRONTEND_API QList<std::tuple<QString, QString>> pls_get_user_active_channles_info();

FRONTEND_API bool pls_is_rehearsal_info_display();

FRONTEND_API QString pls_get_remote_control_mobile_name(const QString &platformName);

enum class pls_frontend_event {
	PLS_FRONTEND_EVENT_TRANSITION_DURATION_CHANGED, // transition duration changed
							// params:
							//     0: int duration
	PLS_FRONTEND_EVENT_PRISM_LOGOUT,                // user logout
	PLS_FRONTEND_EVENT_PRISM_SIGNOUT,               //user sign out
	PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY,          //volume monitor type
	PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY_BACK,
	PLS_FRONTEND_EVENT_PRISM_LOGIN,
	PLS_FRONTEND_EVENT_LIVE_OR_RECORD_START,
	PLS_FRONTEND_EVENT_LIVE_OR_RECORD_END,
	//PRISM/Jimbo/20220914/#/for Youtube rehearsal switch to live.
	PLS_FRONTEND_EVENT_REHEARSAL_SWITCH_TO_LIVE,

	//PRISM/Xiewei/20210113/#/add events for stream deck
	PLS_FRONTEND_EVENT_ALL_MUTE,
	PLS_FRONTEND_EVENT_SIDE_WINDOW_VISIBLE_CHANGED,
	PLS_FRONTEND_EVENT_SIDE_WINDOW_ALL_REGISTERD,
	PLS_FRONTEND_EVENT_PRISM_UPDATE_CPU_USAGE,
	PLS_FRONTEND_EVENT_PRISM_UPDATE_FRAME_DROP,
	PLS_FRONTEND_EVENT_PRISM_UPDATE_BITRATE,
	PLS_FRONTEND_EVENT_PRISM_UPDATE_NOTICE_MESSAGE,
	PLS_FRONTEND_EVENT_PRISM_STREAM_STATE_CHANGED,
	PLS_FRONTEND_EVENT_PRISM_RECORD_STATE_CHANGED,

	//PRISM/Zhangdewen/20210713/#8257/chat source record contains preview messages
	PLS_FRONTEND_EVENT_STREAMING_START_FAILED,
	PLS_FRONTEND_EVENT_SCENE_COLLECTION_ABOUT_TO_CHANGED,

	//PRISM/Zhongling/20220906/#/add events for remote control
	PLS_FRONTEND_EVENT_REMOTE_CONTROL_CLICK_CLOSE_CONNECT,
	PLS_FRONTEND_EVENT_REMOTE_CONTROL_CONNECTION_CHANGED,
	PLS_FRONTEND_EVENT_REMOTE_CONTROL_RNNOISE_CHANGED,

	//PRISM/Liuying/20240118/#4076/add events for streaming time ready
	PLS_FRONTEND_EVENT_STREAMING_TIME_READY,

	//PRISM/FanZirong/20240402/#4948/add events for reset spout ouptut
	PLS_FRONTEND_EVENT_RESET_VIDEO,

	//PRISM/Zhongling/20240416/#5096 add shutting down event
	PLS_FRONTEND_EVENT_PRISM_SHUTTING_DOWN,

	//PRISM/WuLongyue/20241111/for dual output
	PLS_FRONTEND_EVENT_DUAL_OUTPUT_ON,
	PLS_FRONTEND_EVENT_DUAL_OUTPUT_OFF
};
/**
  * frontend event callback
  * param:
  *     [in] event   : frontend event
  *     [in] params  : event params
  *     [in] context : user context
  */
using pls_frontend_event_cb = void (*)(pls_frontend_event event, const QVariantList &params, void *context);

/**
  * add frontend event callback
  * param:
  *     [in] callback : frontend event callback
  *     [in] context  : user context
  */
FRONTEND_API void pls_frontend_add_event_callback(pls_frontend_event_cb callback, void *context);
/**
  * remove frontend event callback
  * param:
  *     [in] callback : frontend event callback
  *     [in] context  : user context
  */
FRONTEND_API void pls_frontend_remove_event_callback(pls_frontend_event_cb callback, void *context);

FRONTEND_API QString pls_get_theme_dir_path();

FRONTEND_API QString pls_get_color_filter_dir_path();

FRONTEND_API QString pls_get_user_path(const QString &path);

enum class pls_toast_info_type { PLS_TOAST_ERROR = 1, PLS_TOAST_REPLY_BUFFER = 2, PLS_TOAST_NOTICE = 3 };
/**
  * get top level widget
  * param:
  *     [in] type: message type
  *     [in] message: message content
  *     [in] auto_close: auto close timeout(millisecond)
  */
FRONTEND_API void pls_toast_message(pls_toast_info_type type, const QString &message, int auto_close = 5000);
/**
  * get top level widget
  * param:
  *     [in] type: message type
  *     [in] message: message content
  *     [in] url: text url
  *     [in] replaceStr: replace string
  *     [in] auto_close: auto close timeout(millisecond)
  */
FRONTEND_API void pls_toast_message(pls_toast_info_type type, const QString &message, const QString &url, const QString &replaceStr, int auto_close = 5000);
/**
  * get top level widget
  * param:
  *     [in] widget: child widget
  * return:
  *     top level widget
  */
FRONTEND_API void pls_toast_clear();

/**
  * set main view side bar user button icon
  * param:
  *     [in] icon: user button icon
  */
FRONTEND_API void pls_set_main_view_side_bar_user_button_icon(const QIcon &icon);

/**
* Remove the right dock window
* param:
	[in] objectName: an unique platform name
**/
FRONTEND_API void pls_unload_chat_dock(const QString &objectName);
FRONTEND_API bool pls_get_prism_user_thumbnail(QPixmap &pixMap);

/**
  * get setting config data
  */
FRONTEND_API const char *pls_basic_config_get_string(const char *section, const char *name, const char *defaultValue = "");
FRONTEND_API int64_t pls_basic_config_get_int(const char *section, const char *name, int64_t defaultValue = 0);
FRONTEND_API uint64_t pls_basic_config_get_uint(const char *section, const char *name, uint64_t defaultValue = 0);
FRONTEND_API bool pls_basic_config_get_bool(const char *section, const char *name, bool defaultValue = false);
FRONTEND_API double pls_basic_config_get_double(const char *section, const char *name, double defaultValue = 0.0);

/**
* the geometry is inside the visible screen area.
* param:
	[in] geometry
**/
FRONTEND_API bool pls_inside_visible_screen_area(QRect geometry);

enum class pls_check_update_result_t {
	Failed = 0x01,
	HmacExceedTime = 0x02,
	HasUpdate = 0x03,
	NoUpdate = 0x04,
};

enum class pls_upload_file_result_t {
	Ok = 0x01,
	NetworkError = 0x02,
	EmailFormatError = 0x03,
	FileFormatError = 0x04,
	AttachUpToMaxFile = 0x05,
};

/**
  * check update
  * param:
  *     [out] is_force: is force update
  *     [out] version: new version
  *     [out] file_url: new file url
  *     [out] update_info_url: update info url
  * return:
  *     check result
  */
FRONTEND_API pls_check_update_result_t pls_check_app_update(bool &is_force, QString &version, QString &file_url, QString &update_info_url, PLSErrorHandler::RetData &retData);

/**
  * check update
  * param:
  *     [out] file_url: new file url
  * return:
  *     check result
  */

enum class PLS_CONTACTUS_QUESTION_TYPE { Error, Advice, Consult, Other };
FRONTEND_API pls_upload_file_result_t pls_upload_contactus_files(PLS_CONTACTUS_QUESTION_TYPE iType, const QString &email, const QString &question, const QList<QFileInfo> files);

/**
  * show update info
  * param:
  *     [in] is_force: is force update
  *     [in] version: new version
  *     [in] file_url: new file url
  *     [in] update_info_url: update info url
  *     [in] is_manual: is manual update
  * return:
  *     true is download update, false other else
  */
FRONTEND_API bool pls_show_update_info_view(bool is_force, const QString &version, const QString &file_url, const QString &update_info_url, bool is_manual = false, QWidget *parent = nullptr);

/**
  * get new notice url
  * return:
  *     url
  */
FRONTEND_API void pls_get_new_notice_Info(const std::function<void(const QVariantMap &noticeInfo)> &noticeCallback);
/**
  * get windows version
  * return:
  *     windows version
  */
FRONTEND_API QString pls_get_win_os_version();

/*
*pls_is_living_or_recording
* return isliving or recording result
*
*/
FRONTEND_API bool pls_is_living_or_recording();

/*
* pls_is_output_actived
* return if output actived
*/
FRONTEND_API bool pls_is_output_actived();

/**
  * add tools menu seperator
  */
FRONTEND_API void pls_add_tools_menu_seperator();

/*
* is streaming
*
*/
FRONTEND_API bool pls_is_streaming();

/*
* is rehearsaling
*/
FRONTEND_API bool pls_is_rehearsaling();

/*
* is recording
*/
FRONTEND_API bool pls_is_recording();

/*
* if the window Right Margin is out of the scrren, then move it to screen right border.
*/

FRONTEND_API void pls_window_right_margin_fit(QWidget *widget);
/*
* widget style sheet
*/
FRONTEND_API void pls_load_stylesheet(QWidget *widget, const QStringList &filePaths);

FRONTEND_API void pls_load_dev_server();

using PfnGetConfigPath = int(__cdecl *)(char *path, size_t size, const char *name);
FRONTEND_API PfnGetConfigPath pls_get_config_path(void);
FRONTEND_API void pls_set_config_path(PfnGetConfigPath getConfigPath);
FRONTEND_API QString pls_get_config_dir(PfnGetConfigPath getConfigPath, const char *name);
FRONTEND_API QString pls_get_config_dir();
FRONTEND_API QString pls_get_relative_config_path(const QString &config_path);
FRONTEND_API QString pls_get_absolute_config_path(const QString &config_path);

FRONTEND_API QVariantMap pls_http_request_default_headers(bool hasGcc = true);

FRONTEND_API QString pls_get_md5(const QString &originStr, const QString &prefix = "");

enum class ControlSrcType { None, RemoteControl, OutPutTimer, StreamDeck };
FRONTEND_API ControlSrcType pls_previous_broadcast_control_by();
FRONTEND_API void pls_set_broadcast_control(const ControlSrcType &control = ControlSrcType::None);
FRONTEND_API ControlSrcType pls_previous_record_control_by();
FRONTEND_API ControlSrcType pls_get_current_live_control_type();

FRONTEND_API void pls_start_broadcast(bool toStart = true, const ControlSrcType &control = ControlSrcType::None);
FRONTEND_API void pls_start_broadcast_in_info(bool toStart = true);
FRONTEND_API void pls_start_rehearsal(bool toStart = true);
FRONTEND_API void pls_start_record(bool toStart = true, const ControlSrcType &control = ControlSrcType::None);

//add by xiewei
FRONTEND_API OBSSource pls_get_source_by_name(const char *name);
FRONTEND_API OBSData pls_get_source_setting(const obs_source_t *source);
FRONTEND_API OBSData pls_get_source_private_setting(obs_source_t *source);
FRONTEND_API OBSSource pls_get_source_by_pointer_address(const void *pointerAddress);
FRONTEND_API OBSScene pls_get_scene_by_pointer_address(const void *pointerAddress);
FRONTEND_API OBSSceneItem pls_get_sceneitem_by_pointer_address(OBSScene destScene, void *sceneitemAddress);
FRONTEND_API OBSSceneItem pls_get_sceneitem_by_pointer_address(void *sceneitemAddress);
FRONTEND_API const char *pls_source_get_display_name(const char *id);

FRONTEND_API void pls_get_all_source(std::vector<OBSSource> &vecSources);
FRONTEND_API void pls_get_all_source(std::vector<OBSSource> &sources, const char *source_id, const char *name, const std::function<bool(const char *value)> &value);

FRONTEND_API bool pls_source_support_rnnoise(const char *id);
FRONTEND_API bool pls_get_chat_info(QString &id, int &seqHorizontal, int &seqVertical, QString &cookie, bool &isSinglePlatform);
FRONTEND_API int pls_get_current_selected_channel_count();

FRONTEND_API void pls_add_custom_font(const QString &fontPath);
FRONTEND_API bool pls_install_scene_template(const SceneTemplateItem &item);
FRONTEND_API bool pls_get_output_stream_dealy_active();

class QButtonGroup;

struct ITextMotionTemplateHelper {

	struct PLSChatDefaultFamily {
		QString webFamilyText;
		QString qtFamilyText;
		QString uiFamilyText;
		int fontSize = 0;
		int fontWeight = 0;
		int index = -1;
		int buttonWidth = 0;
		QString buttonResourceStr;
	};
	virtual ~ITextMotionTemplateHelper() = default;
	virtual void initTemplateButtons() = 0;
	virtual QMap<int, QString> getTemplateNames() = 0;
	virtual QButtonGroup *getTemplateButtons(const QString &templateName) = 0;
	virtual void resetButtonStyle() = 0;
	virtual QString findTemplateGroupStr(const int &templateId) = 0;
	virtual int getDefaultTemplateId() = 0;
	virtual QStringList getTemplateNameList() = 0;
	virtual void removeParent() = 0;
	virtual QJsonObject defaultTemplateObj(const int itemId) { return QJsonObject(); }
	virtual bool saveCustomObj(const OBSData &settings, const int itemId) { return false; }
	virtual QJsonArray getSaveTemplate() const { return QJsonArray(); }
	virtual QSet<QString> getChatTemplateName() const { return QSet<QString>(); }
	virtual void updateCustomTemplateName(const QString &name, const int id) {}
	virtual void removeCustomTemplate(const int id) {}
	virtual QList<PLSChatDefaultFamily> getChatCustomDefaultFamily() { return {}; }
	virtual void clearChatTemplateButton() {}
};

FRONTEND_API ITextMotionTemplateHelper *pls_get_text_motion_template_helper_instance();
FRONTEND_API ITextMotionTemplateHelper *pls_get_chat_template_helper_instance();

/**
  * get current application language
  */
FRONTEND_API QString pls_get_current_language();

FRONTEND_API QLocale::Language pls_get_current_language_enum();

/**
  * pt-BR -> pt
  */
FRONTEND_API QString pls_get_current_language_short_str();

/**
  * pt-BR -> BR
  */
FRONTEND_API QString pls_get_current_country_short_str();
/*
* check if current app language  is languge x
* et. QLocale::English ->pls_is_match_current_language(QLocale::English)
*/
FRONTEND_API bool pls_is_match_current_language(QLocale::Language xlanguage);

#define IS_ENGLISH() pls_is_match_current_language(QLocale::English)

#define IS_KR() pls_is_match_current_language(QLocale::Korean)

/**
  * get current accept language
  */
FRONTEND_API QString pls_get_current_accept_language();

FRONTEND_API bool pls_is_match_current_language(const QString &lang);

/**
  * get actived chat channel count
  */
FRONTEND_API int pls_get_actived_chat_channel_count();

/**
  * get prism liveSeq
  */
FRONTEND_API void pls_get_prism_live_seq(int &seqHorizontal, int &seqVertical);

/**
  * get prism login cookie value
  */
FRONTEND_API QByteArray pls_get_prism_cookie_value();

/**
  * check create source is in app start loading
  */
FRONTEND_API bool pls_is_create_souce_in_loading();

/**
  * monitor network state
  */
FRONTEND_API void pls_network_state_monitor(const std::function<void(bool)> &callback);

/**
  * get network state
  */
FRONTEND_API bool pls_get_network_state();

/**
  * show virtual background
  */
FRONTEND_API void pls_show_virtual_background();

/**
  * create virtual background resource list widget
  */
FRONTEND_API QWidget *pls_create_virtual_background_resource_widget(QWidget *parent, const std::function<void(QWidget *)> &init, bool forProperty = false, const QString &itemId = QString(),
								    bool checkBoxState = false, bool switchToPrismFirst = false);

/**
  * get media size
  */
FRONTEND_API bool pls_get_media_size(QSize &size, const char *path);

FRONTEND_API QPixmap pls_load_svg(const QString &path, const QSize &size);

FRONTEND_API void pls_set_bgm_visible(bool visible);

//PRISM/Xiewei/20210113/#/add apis for stream deck
FRONTEND_API bool pls_set_side_window_visible(int key, bool visible);
FRONTEND_API void pls_mixer_mute_all(bool mute);
FRONTEND_API bool pls_mixer_is_all_mute();
FRONTEND_API QString pls_get_stream_state();
FRONTEND_API QString pls_get_record_state();
FRONTEND_API int pls_get_record_duration();
FRONTEND_API bool pls_get_hotkey_enable();

/**
  * enum for window config key
  */
class FRONTEND_API WindowConfigEnum : public QObject {
	Q_OBJECT
public:
	enum FeatureId {
		None = -1,
		BeautyConfig = 1,
		GiphyStickersConfig,
		BgmConfig,
		LivingMsgView,
		ChatConfig,
		WiFiConfig,
		VirtualbackgroundConfig,
		PrismStickerConfig,
		VirtualCameraConfig,
		DrawPenConfig,
		LaboratoryConfig,
		RemoteControlConfig,
		CamStudioConfig,
		SceneTemplateConfig,
		Ncb2bBrowserSettings,
		DualOutputConfig
	};
	Q_ENUM(FeatureId)
};
using ConfigId = WindowConfigEnum::FeatureId;

struct SideWindowInfo {
	QString windowName;
	WindowConfigEnum::FeatureId id;
	bool visible = false;
};

FRONTEND_API QList<SideWindowInfo> pls_get_side_windows_info();

struct PrismStatus {
	double cpuUsage = 0.0;
	double totalCpu = 0.0;
	double kbitsPerSec = 0.0;
	int totalDrop = 0;
	double dropPercent = 0.0;
};
//Q_DECLARE_METATYPE(PrismStatus)

FRONTEND_API int pls_get_toast_message_count();

FRONTEND_API void pls_config_set_string(config_t *config, ConfigId id, const char *name, const char *value);
FRONTEND_API void pls_config_set_int(config_t *config, ConfigId id, const char *name, int64_t value);
FRONTEND_API void pls_config_set_uint(config_t *config, ConfigId id, const char *name, uint64_t value);
FRONTEND_API void pls_config_set_bool(config_t *config, ConfigId id, const char *name, bool value);
FRONTEND_API void pls_config_set_double(config_t *config, ConfigId id, const char *name, double value);

FRONTEND_API const char *pls_config_get_string(config_t *config, ConfigId id, const char *name);
FRONTEND_API int64_t pls_config_get_int(config_t *config, ConfigId id, const char *name);
FRONTEND_API uint64_t pls_config_get_uint(config_t *config, ConfigId id, const char *name);
FRONTEND_API bool pls_config_get_bool(config_t *config, ConfigId id, const char *name);
FRONTEND_API double pls_config_get_double(config_t *config, ConfigId id, const char *name);

FRONTEND_API bool pls_config_remove_value(config_t *config, ConfigId id, const char *name);

enum class pls_blacklist_type {
	None = 0,
	GPUModel = (1 << 0),
	GraphicsDrivers = (1 << 1),
	DeviceDrivers = (1 << 2),
	ThirdPlugins = (1 << 3),
	VSTPlugins = (1 << 4),
	ThirdPrograms = (1 << 5),
	OtherType = (1 << 6),
	ExceptionType = (1 << 7)
};

FRONTEND_API bool pls_is_dev_server();
FRONTEND_API QString pls_get_navershopping_deviceId();

FRONTEND_API bool pls_run_http_server(const char *path, QString &addr, const std::function<void(const QString &, const QJsonObject &)> &callback);

FRONTEND_API int pls_alert_warning(const char *title, const char *message);

FRONTEND_API void pls_singleton_wakeup();

FRONTEND_API bool pls_is_test_mode();

FRONTEND_API bool pls_is_immersive_audio();
FRONTEND_API uint pls_get_live_start_time();
/*
* user indentify number
* 900101-1*****
*/
FRONTEND_API QString pls_masking_identify_info(const QString &str);
/*
* datetime
* 1990.***
*/
FRONTEND_API QString pls_masking_datetime_info(const QString &str);
/*
* passport
* M488*****
*/
FRONTEND_API QString pls_masking_passport_info(const QString &str);

/*
* Region
*  00-123***
*/
FRONTEND_API QString pls_masking_Region_info(const QString &str);
/*
* Bank Card
* 예시: 1234-****-****-5678 / 1234-****-****-567
*/
FRONTEND_API QString pls_masking_bank_card_info(const QString &str);

/*
* user name or other information
* LA***IM, SH***UB, LU***EI
*/
FRONTEND_API QString pls_masking_name_info(const QString &str);
/*
* ipv4/ipv6
* 192.168.***.***
*/
FRONTEND_API QString pls_masking_ip_info(const QString &str);
/*
* address
* 
*/
FRONTEND_API QString pls_masking_address_info(const QString &str);

/*
* email
* pr****@naver.com
*/
FRONTEND_API QString pls_masking_email_info(const QString &str);
/*
* userid
* priv***
*/
FRONTEND_API QString pls_masking_user_id_info(const QString &str);
/*
* other person info
* P12345*****
* Personal information without specific examples (personal customs clearance code, overseas account number, overseas ID card number, etc.)
*/
FRONTEND_API QString pls_masking_person_info(const QString &str);

/*
* long long 
* 1234568*
* mask long long data
*/
FRONTEND_API QString pls_masking_int_info(const qint64 &intData);
/*
* double data
* 145.34***
* mask double data
*/
FRONTEND_API QString pls_masking_double_info(const double &douData);

/*
* Click the Lab Open button
* 145.34***
* laboratoryId laboratory item id
* targetStatus false means closed true means open
*/
FRONTEND_API void pls_laboratory_click_open_button(const QString &laboratoryId, bool targetStatus);

/*
* send the usage status of the current item to the laboratory, which needs to be called after calling the open method of each item
* 145.34***
* laboratoryId laboratory item id
* on whether the current function is turned on in the laboratory
*/
FRONTEND_API void pls_set_laboratory_status(const QString &laboratoryId, bool on);

/*
* js events on the lab details page
* 145.34***
* page The page where the event occurred
* action type of event
* info information about the incident
*/
FRONTEND_API void pls_laboratory_detail_page_js_event(const QString &page, const QString &action, const QString &info);

/*
* whether the current laboratory item is in use
* 145.34***
*/
FRONTEND_API bool pls_get_laboratory_status(const QString &laboratoryId);

FRONTEND_API void pls_navershopping_get_store_login_url(QWidget *widget, const std::function<void(const QString &storeLoginUrl)> &ok, const std::function<void(const QByteArray &data)> &fail);

FRONTEND_API void pls_navershopping_get_error_code_message(const QByteArray &data, QString &errorCode, QString &errorMessage);

FRONTEND_API void pls_enum_sources(const std::function<bool(obs_source_t *)> &callback);

/**
 * overwrite: true to overwrite if newName file is exist. true by defaualt.
 * sendSre: true to send SRE log if copy failed. false by default.
 */

FRONTEND_API void pls_on_frontend_event(pls_frontend_event event, const QVariantList &params = QVariantList());

enum class AnalogType {
	ANALOG_VIRTUAL_BG_TEMPLATE,
	ANALOG_VIRTUAL_BG,
	ANALOG_BEAUTY,
	ANALOG_DRAWPEN,
	ANALOG_ADD_SOURCE,
	ANALOG_ADD_FILTER,
	ANALOG_PLAY_BGM,
	ANALOG_VIRTUAL_CAM,
	ANALOG_PLATFORM_OUTPUTGUIDE,
	ANALOG_NCB2B_LOGIN,
	ANALOG_ERROR_CODE
};

FRONTEND_API void pls_send_analog(AnalogType logType, const QVariantMap &info);

FRONTEND_API QString pls_get_analog_filter_id(const char *id);

FRONTEND_API void pls_get_scene_source_count(int &sceneCount, int &sourceCount);

FRONTEND_API int pls_show_download_failed_alert(QWidget *parent);

FRONTEND_API QVector<QString> pls_get_scene_collections();
#ifdef ENABLE_CAPTURE_IMAGE
/**
 * compress an directory to a zip file.
 * sdel: if delete source files after finish compress.
 */
FRONTEND_API bool pls_compress_directory(const char *src_dir, const char *zip_file, bool sdel = true);
#endif

namespace pls {
using Button = QDialogButtonBox::StandardButton;
using Buttons = QDialogButtonBox::StandardButtons;
}
FRONTEND_API pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, pls::Buttons buttons = pls::Button::Ok, pls::Button defaultButton = pls::Button::Ok,
						 const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
FRONTEND_API pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, const QMap<pls::Button, QString> &buttons,
						 pls::Button defaultButton = pls::Button::NoButton, const std::optional<int> &timeout = std::optional<int>(),
						 const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
FRONTEND_API pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, pls::Buttons buttons = pls::Button::Ok,
						 pls::Button defaultButton = pls::Button::Ok, const std::optional<int> &timeout = std::optional<int>(),
						 const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
FRONTEND_API pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, const QMap<pls::Button, QString> &buttons,
						 pls::Button defaultButton = pls::Button::NoButton, const std::optional<int> &timeout = std::optional<int>(),
						 const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());

class QPushButton;
namespace pls {

struct IObject {
	virtual ~IObject() = default;

	virtual void release() = 0;
};
struct IPropertyModel : public IObject {
	~IPropertyModel() override = default;
};
struct ITemplateListPropertyModel : public IPropertyModel {
	~ITemplateListPropertyModel() override = default;

	struct IButton;
	struct IButtonGroup;

	virtual void getButtons(QObject *receiver, const std::function<void()> &waiting, const std::function<void(bool ok, IButtonGroup *group)> &result) = 0;
};
struct ITemplateListPropertyModel::IButton : public IObject {
	~IButton() override = default;
	virtual IButtonGroup *buttonGroup() = 0;
	virtual QPushButton *button() = 0;
	virtual int value() const = 0;
};
struct ITemplateListPropertyModel::IButtonGroup : public IObject {
	~IButtonGroup() override = default;

	virtual QWidget *widget() = 0;

	virtual IButton *selectedButton() = 0;
	virtual void selectButton(IButton *button) = 0;
	virtual void selectButton(int value) = 0;
	virtual void selectFirstButton() = 0;

	virtual int buttonCount() const = 0;
	virtual IButton *button(int index) = 0;
	virtual IButton *fromValue(int value) = 0;

	virtual void connectSlot(QObject *receiver, const std::function<void(IButton *current, IButton *previous)> &slot) = 0;
};
}
FRONTEND_API pls::IPropertyModel *pls_get_property_model(obs_source_t *source, obs_data_t *settings, obs_properties_t *props, obs_property_t *prop);
class QAbstractButton;
FRONTEND_API void pls_template_button_refresh_gif_geometry(QAbstractButton *button);

struct ServiceRecord {
	const char *id;
	const char *name;
	const char *deviceType;
	const char *connectType;
	int version;
};

#if _WIN32
FRONTEND_API bool pls_register_mdns_service(const char *pszName, unsigned short wPort, const ServiceRecord &record, bool bRegister = true);
#else
FRONTEND_API void pls_register_mdns_service_ex(const char *pszName, unsigned short wPort, const ServiceRecord &record, bool bRegister, void *context, void (*completion)(void *context, bool success));
#endif

FRONTEND_API void pls_set_remote_control_server_info(quint16 port);
FRONTEND_API void pls_get_remote_control_server_info(quint16 &port);

FRONTEND_API void pls_set_remote_control_client_info(const QString &peerName, bool connected);
FRONTEND_API void pls_get_remote_control_client_info(QString &peerName, bool &connected);

FRONTEND_API bool pls_set_remote_control_log_file(const QString &logFile);
FRONTEND_API void pls_get_remote_control_log_file(QString &logFile);

FRONTEND_API void pls_sys_tray_notify(const QString &text, QSystemTrayIcon::MessageIcon n, bool usePrismLogo = true);

FRONTEND_API config_t *pls_get_global_cookie_config(void);

const int NdiSuccess = 0;
const int NoNdiRuntimeFound = 1;
const int NDIInitializeFail = 2;
using pls_load_ndi_runtime_pfn = int (*)();
FRONTEND_API pls_load_ndi_runtime_pfn pls_get_load_ndi_runtime();
FRONTEND_API void pls_set_load_ndi_runtime(pls_load_ndi_runtime_pfn load_ndi_runtime);

FRONTEND_API QWidget *pls_get_banner_widget();
FRONTEND_API void pls_open_cam_studio(QStringList arguments, QWidget *parent);
FRONTEND_API bool pls_is_install_cam_studio(QString &program);
FRONTEND_API void pls_show_cam_studio_uninstall(QWidget *parent, QString title, QString content, QString okTip, QString cancelTip);

FRONTEND_API bool pls_is_always_on_top(const QWidget *widget);

FRONTEND_API QStringList getChannelWithChatList(bool bAddNCPPrefix);

FRONTEND_API bool pls_is_ncp(QString &channlName);

//The pop-up login box is judged to be true, and the skip login is judged to be false.
FRONTEND_API bool pls_is_ncp_first_login(QString &serviceName);
FRONTEND_API QString get_channel_cookie_path(const QString &channelLoginName);

FRONTEND_API bool pls_get_random_bool();

FRONTEND_API bool pls_is_chzzk_checked(bool forHorizontal = true);

FRONTEND_API obs_output_t *pls_frontend_get_streaming_output_v(void);
