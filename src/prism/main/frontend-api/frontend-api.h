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
#include "cancel.hpp"

class PLSLoginInfo;

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
using pls_result_checking_callback_t = PLSResultCheckingResult (*)(QJsonObject &result, const QString &url, const QMap<QString, QString> &cookies);


FRONTEND_API void pls_delete_specific_url_cookie(const QString &url);


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
FRONTEND_API bool pls_browser_view(QJsonObject &result, const QUrl &url, const std::map<std::string, std::string> &headers, pls_result_checking_callback_t callback, QWidget *parent = nullptr);
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
FRONTEND_API QString pls_get_youtube_code_from_url(const QString &url_str);
/*
* channel login 
*/
FRONTEND_API bool pls_channel_login(QJsonObject &result, const QString &accountName, QWidget *parent);




/*
* get bulid Date
*/
FRONTEND_API long long pls_get_gpop_bulid_data();
/*
* get log level
*/
FRONTEND_API QString pls_get_gpop_log_level();


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
FRONTEND_API void pls_flush_style_recursive(QWidget *widget);

/*
* flush widget style with property
*/
FRONTEND_API void pls_flush_style(QWidget *widget, const char *propertyName, const QVariant &propertyValue);

/*
* flush widget style recursively with property
*/
FRONTEND_API void pls_flush_style_recursive(QWidget *widget, const char *propertyName, const QVariant &propertyValue);
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
