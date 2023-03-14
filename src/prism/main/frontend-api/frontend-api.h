#pragma once

#include "frontend-api-global.h"

#include <obs-frontend-api.h>

#include <functional>
#include <QUrl>
#include <QJsonObject>
#include <QMap>
#include <QWidget>
#include <qlist.h>
#include <QNetworkCookie>
#include <QNetworkCookie>
#include <QVariant>
#include <QFileInfo>
#include <QObject>
#include <QDialogButtonBox>
#include "../../prism/main/pls-gpop-data.hpp"
#include "cancel.hpp"
#include "obs.hpp"
#include <vector>

class PLSLoginInfo;

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
using pls_result_checking_callback_t = std::function<PLSResultCheckingResult(QJsonObject &result, const QString &url, const QMap<QString, QString> &cookies)>;

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
/**
  * del all cookie cache
  */

//FRONTEND_API void pls_del_all_cookie();
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
FRONTEND_API bool pls_browser_view(QJsonObject &result, const QUrl &url, pls_result_checking_callback_t callback, QWidget *parent = nullptr);
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
FRONTEND_API bool pls_browser_view(QJsonObject &result, const QUrl &url, const std::map<std::string, std::string> &headers, const QString &pannelName, pls_result_checking_callback_t callback,
				   QWidget *parent = nullptr);
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
FRONTEND_API bool pls_browser_view(QJsonObject &result, const QUrl &url, const std::map<std::string, std::string> &headers, const QString &pannelName, const std::string &script,
				   pls_result_checking_callback_t callback, QWidget *parent = nullptr);

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

/**
  * generate url hmac
  * param:
  *     [out] out_url: output url
  *     [in] in_url: input url
  *     [in] mac_key: key
  * return:
  *     true for success, false for failed
  */
FRONTEND_API bool pls_get_encrypt_url(QUrl &out_url, const QString &in_url, const QString &mac_key);
/**
  * generate url hmac
  * param:
  *     [in] in_url: input url
  *     [in] mac_key: key
  * return:
  *     output url
  */
FRONTEND_API QUrl pls_get_encrypt_url(const QString &url, const QString &mac_key);
/**
  * generate networkcookie list
  * param:
  *     [in] QJsonObject: cookies
  * return:
  *     output networkCookieList
  */
FRONTEND_API QList<QNetworkCookie> pls_get_cookies(const QJsonObject &cookies);
/**
  * get sns user info
  * param:
  *     [out] QJsonObject: result
  *     [in]QList<QNetworkCookie>: cookies
  *     [in] urlStr: url
  * return:
  *     true for success, false for failed
  */
FRONTEND_API bool pls_sns_user_info(QJsonObject &result, const QList<QNetworkCookie> &cookies, const QString &urlStr);
FRONTEND_API bool pls_google_user_info(std::function<void(bool ok, const QJsonObject &)> callback, const QString &redirect_uri, const QString &code, QObject *receiver);
/**
  * close mainwindow and reset config show login view

  */
FRONTEND_API void pls_prism_change_over_login_view();
/**
  * network environment check
  * return:
  *     true for success, false for failed
  */
FRONTEND_API bool pls_network_environment_reachable();
/** token vaild check
*return:
	true for success false for faild
*/
FRONTEND_API bool pls_prism_token_is_vaild(const QString &urlStr);
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
FRONTEND_API void pls_channel_login_async(std::function<void(bool ok, const QJsonObject &result)> &&callback, const QString &accountName, QWidget *parent);

FRONTEND_API Common pls_get_gpop_common();
FRONTEND_API VliveNotice pls_get_vlive_notice();

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

/**
  * get main view widget
  * return:
  *     main view widget
  */
FRONTEND_API QWidget *pls_get_main_view();
/**
  * get top level widget
  * param:
  *     [in] widget: child widget
  * return:
  *     top level widget
  */
FRONTEND_API QWidget *pls_get_toplevel_view(QWidget *widget);

FRONTEND_API QByteArray pls_get_prism_cookie();

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

	//PRISM/Xiewei/20210113/#/add events for stream deck
	PLS_FRONTEND_EVENT_ALL_MUTE,
	PLS_FRONTEND_EVENT_SIDE_WINDOW_VISIBLE_CHANGED,
	PLS_FRONTEND_EVENT_SIDE_WINDOW_ALL_REGISTERD,
	PLS_FRONTEND_EVENT_PRISM_UPDATE_CPU_USAGE,
	PLS_FRONTEND_EVENT_PRISM_UPDATE_FRAME_DROP,
	PLS_FRONTEND_EVENT_PRISM_UPDATE_BITRATE,
	PLS_FRONTEND_EVENT_PRISM_UPDATE_NOTICE_MESSAGE,
	PLS_FRONTEND_EVENT_PRISM_LOGIN_STATE_CHANGED,
	PLS_FRONTEND_EVENT_PRISM_STREAM_STATE_CHANGED,
	PLS_FRONTEND_EVENT_PRISM_RECORD_STATE_CHANGED,

	//PRISM/Zhangdewen/20210713/#8257/chat source record contains preview messages
	PLS_FRONTEND_EVENT_STREAMING_START_FAILED,
	PLS_FRONTEND_EVENT_SCENE_COLLECTION_ABOUT_TO_CHANGED
};
/**
  * frontend event callback
  * param:
  *     [in] event   : frontend event
  *     [in] params  : event params
  *     [in] context : user context
  */
typedef void (*pls_frontend_event_cb)(pls_frontend_event event, const QVariantList &params, void *context);

/**
  * add frontend event callback
  * param:
  *     [in] callback : frontend event callback
  *     [in] context  : user context
  */
FRONTEND_API void pls_frontend_add_event_callback(pls_frontend_event_cb callback, void *context);
/**
  * add frontend event callback
  * param:
  *     [in] event    : event for notify
  *     [in] callback : frontend event callback
  *     [in] context  : user context
  */
FRONTEND_API void pls_frontend_add_event_callback(pls_frontend_event event, pls_frontend_event_cb callback, void *context);
/**
  * add frontend event callback
  * param:
  *     [in] events   : events for notify
  *     [in] callback : frontend event callback
  *     [in] context  : user context
  */
FRONTEND_API void pls_frontend_add_event_callback(QList<pls_frontend_event> events, pls_frontend_event_cb callback, void *context);
/**
  * remove frontend event callback
  * param:
  *     [in] callback : frontend event callback
  *     [in] context  : user context
  */
FRONTEND_API void pls_frontend_remove_event_callback(pls_frontend_event_cb callback, void *context);
/**
  * remove frontend event callback
  * param:
  *     [in] event    : event for notify
  *     [in] callback : frontend event callback
  *     [in] context  : user context
  */
FRONTEND_API void pls_frontend_remove_event_callback(pls_frontend_event event, pls_frontend_event_cb callback, void *context);
/**
  * remove frontend event callback
  * param:
  *     [in] events   : events for notify
  *     [in] callback : frontend event callback
  *     [in] context  : user context
  */
FRONTEND_API void pls_frontend_remove_event_callback(QList<pls_frontend_event> events, pls_frontend_event_cb callback, void *context);

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
* Use cef to load a chat url, then dock the window on the right
* param:
	[in] objectName: an unique platform name
	[in] url: chat url
	[in] white_popup_url: white url list to allow popup window
	[in] startup_script: injected a startup script
**/
FRONTEND_API void pls_load_chat_dock(const QString &objectName, const std::string &url, const std::string &white_popup_url = std::string(), const std::string &startup_script = std::string());

/**
* Remove the right dock window
* param:
	[in] objectName: an unique platform name
**/
FRONTEND_API void pls_unload_chat_dock(const QString &objectName);

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
	Ok = 0x01,
	Failed = 0x02,
	HasUpdate = 0x10,
	OkNoUpdate = Ok,
	OkHasUpdate = Ok | HasUpdate,
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
  *     [out] gcc: gcc
  *     [out] is_force: is force update
  *     [out] version: new version
  *     [out] file_url: new file url
  *     [out] update_info_url: update info url
  * return:
  *     check result
  */
FRONTEND_API pls_check_update_result_t pls_check_update(QString &gcc, bool &is_force, QString &version, QString &file_url, QString &update_info_url);

/**
  * check update
  * param:
  *     [out] file_url: new file url
  * return:
  *     check result
  */
FRONTEND_API bool pls_check_lastest_version(QString &update_info_url);

/**
  * show update info
  * param:
  *     [in] email: email address
  *     [in] question: question
  *     [in] files: file list
  */
FRONTEND_API pls_upload_file_result_t pls_upload_contactus_files(const QString &email, const QString &question, const QList<QFileInfo> files);

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
  * download update
  * param:
  *     [out] local_file_path: saved local file path
  *     [in] file_url: new file url
  *     [in] cancel: true for cancel download
  *     [in] progress: progress callback
  * return:
  *     true download success
  */
FRONTEND_API bool pls_download_update(QString &local_file_path, const QString &file_url, PLSCancel &cancel, const std::function<void(qint64 download_bytes, qint64 total_bytes)> &progress);
/**
  * install update
  * param:
  *     [in] file_path: update file path
  */
FRONTEND_API bool pls_install_update(const QString &file_path);
/**
  * get new notice url
  * return:
  *     url
  */
FRONTEND_API QVariantMap pls_get_new_notice_Info();
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
* is recording
*/
FRONTEND_API bool pls_is_recording();

/*
* flush widget style
*/
FRONTEND_API void pls_flush_style(QWidget *widget);

/*
* flush widget style recursively
*/
FRONTEND_API void pls_flush_style_recursive(QWidget *widget, int recursiveDeep = -1);

/*
* flush widget style with property
*/
FRONTEND_API void pls_flush_style(QWidget *widget, const char *propertyName, const QVariant &propertyValue);

/*
* flush widget style recursively with property
* recursiveDeep < 0, mean not limit the recursive deep.
*/
FRONTEND_API void pls_flush_style_recursive(QWidget *widget, const char *propertyName, const QVariant &propertyValue, int recursiveDeep = -1);
/*
* if the window Right Margin is out of the scrren, then move it to screen right border.
*/

FRONTEND_API void pls_window_right_margin_fit(QWidget *widget);
/*
* widget style sheet
*/
FRONTEND_API void pls_load_stylesheet(QWidget *widget, const QStringList &filePaths);

FRONTEND_API void pls_load_dev_server();

typedef int(__cdecl *PfnGetConfigPath)(char *path, size_t size, const char *name);
FRONTEND_API PfnGetConfigPath pls_get_config_path(void);
FRONTEND_API void pls_set_config_path(PfnGetConfigPath getConfigPath);

FRONTEND_API void pls_http_request_head(QVariantMap &headMap, bool hasGacc = true);

FRONTEND_API std::string pls_get_offline_javaScript();
FRONTEND_API QString pls_get_md5(const QString &originStr, const QString &prefix = "");

FRONTEND_API void pls_start_broadcast(bool toStart = true);
FRONTEND_API void pls_start_record(bool toStart = true);

//add by xiewei
FRONTEND_API OBSSource pls_get_source_by_name(const char *name);
FRONTEND_API OBSData pls_get_source_setting(const obs_source_t *source);
FRONTEND_API OBSData pls_get_source_private_setting(const obs_source_t *source);
FRONTEND_API OBSSource pls_get_source_by_pointer_address(void *pointerAddress);
FRONTEND_API OBSSceneItem pls_get_sceneitem_by_pointer_address(OBSScene destScene, void *sceneitemAddress);
FRONTEND_API OBSSceneItem pls_get_sceneitem_by_pointer_address(void *sceneitemAddress);

FRONTEND_API void pls_get_all_source(std::vector<OBSSource> &vecSources);
FRONTEND_API void pls_get_all_source(std::vector<OBSSource> &sources, const char *source_id, const char *name, std::function<bool(const char *value)> value);

class QButtonGroup;

struct ITextMotionTemplateHelper {

	virtual ~ITextMotionTemplateHelper() {}
	virtual void initTemplateButtons() = 0;
	virtual QMap<int, QString> getTemplateNames() = 0;
	virtual QButtonGroup *getTemplateButtons(const QString &templateName) = 0;
	virtual void resetButtonStyle() = 0;
	virtual QString findTemplateGroupStr(const int &templateId) = 0;
};

FRONTEND_API ITextMotionTemplateHelper *pls_get_text_motion_template_helper_instance();

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
* et. QLocale::English ->pls_is_match_current_language(QLocale::English);
*/
FRONTEND_API bool pls_is_match_current_language(QLocale::Language xlanguage);

#define IS_ENGLISH() pls_is_match_current_language(QLocale::English)

#define IS_KR() pls_is_match_current_language(QLocale::Korean)

/**
  * get current accept language
  * return "qt;q=0.9, en;q=0.8, ko;q=0.7, *;q=0.5";
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
FRONTEND_API int pls_get_prism_live_seq();

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
FRONTEND_API void pls_network_state_monitor(std::function<void(bool)> &&callback);

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
FRONTEND_API QWidget *pls_create_virtual_background_resource_widget(QWidget *parent, std::function<void(QWidget *)> &&init, bool forProperty = false, const QString &itemId = QString(),
								    bool checkBoxState = false, bool switchToPrismFirst = false);

/**
  * get media size
  */
FRONTEND_API bool pls_get_media_size(QSize &size, const char *path);

FRONTEND_API QPixmap pls_load_svg(const QString &path, const QSize &size);

FRONTEND_API void pls_show_mobile_source_help();

//PRISM/Xiewei/20210113/#/add apis for stream deck
FRONTEND_API void pls_set_side_window_visible(int key, bool visible);
FRONTEND_API void pls_mixer_mute_all(bool mute);
FRONTEND_API bool pls_mixer_is_all_mute();
FRONTEND_API QString pls_get_login_state();
FRONTEND_API QString pls_get_stream_state();
FRONTEND_API QString pls_get_record_state();
FRONTEND_API bool pls_get_live_record_available();

/**
  * enum for window config key
  */
class FRONTEND_API WindowConfigEnum : public QObject {
	Q_OBJECT
public:
	enum FeatureId { None = -1, BeautyConfig = 1, GiphyStickersConfig, BgmConfig, LivingMsgView, ChatConfig, WiFiConfig, VirtualbackgroundConfig, PrismStickerConfig };
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
Q_DECLARE_METATYPE(PrismStatus)

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

FRONTEND_API pls_blacklist_type pls_is_gpop_blacklist(QString value, pls_blacklist_type type);
FRONTEND_API void pls_alert_third_party_plugins(QString pluginName, QWidget *parent = nullptr);

FRONTEND_API bool pls_is_dev_server();
FRONTEND_API QString pls_get_navershopping_deviceId();

FRONTEND_API bool pls_run_http_server(const char *path, QString &addr, std::function<void(const QString &, const QJsonObject &)> callback);

FRONTEND_API QDialogButtonBox::StandardButton pls_alert_warning(const char *title, const char *message);

FRONTEND_API bool pls_get_app_exiting();
FRONTEND_API void pls_set_app_exiting(bool);

FRONTEND_API void pls_singletonWakeup();

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

class QDialog;
FRONTEND_API void pls_push_dialog_view(QDialog *dialog);
FRONTEND_API void pls_pop_dialog_view(QDialog *dialog);
FRONTEND_API void pls_notify_close_dialog_views();
