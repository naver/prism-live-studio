#include "frontend-internal.hpp"
#include "IMACManager.h"
#include <QUrl>
#include <QStyle>
#include <QDebug>
#include "login-info.hpp"
#include <QScreen>
#include <QGuiApplication>
#include "../pls-net-url.hpp"
#include "ui-config.h"
#include "../pls-app.hpp"
#include <util/windows/win-version.h>
#include <QNetworkInterface>
#include <Windows.h>
#include <util/windows/WinHandle.hpp>
#include <util/windows/win-version.h>
#include <qdir.h>

static pls_frontend_callbacks *fc = nullptr;
static PfnGetConfigPath getConfigPath = nullptr;

static QString getLocalIPAddr()
{
	for (auto &addr : QNetworkInterface::allAddresses()) {
		if (!addr.isLoopback() && (addr.protocol() == QAbstractSocket::IPv4Protocol)) {
			return addr.toString();
		}
	}
	return QString();
}
static QString getOsVersion(const win_version_info &wvi)
{
	return QString("Windows %1.%2.%3.%4").arg(wvi.major).arg(wvi.minor).arg(wvi.build).arg(wvi.revis);
}
static QString getUserAgent(const win_version_info &wvi)
{
	LANGID langId = GetUserDefaultUILanguage();
#ifdef Q_OS_WIN64
	return QString("PRISM Live Studio/" PLS_VERSION " (Windows %1 Build %2 Architecture x64 Language %3)").arg(wvi.major).arg(wvi.build).arg(langId);
#else
	return QString("PRISM Live Studio/" PLS_VERSION " (Windows %1 Build %2 Architecture x86 Language %3)").arg(wvi.major).arg(wvi.build).arg(langId);
#endif
}
FRONTEND_API void pls_frontend_set_callbacks_internal(pls_frontend_callbacks *callbacks)
{
	obs_frontend_set_callbacks_internal(callbacks);
	fc = callbacks;
}

static inline bool callbacks_valid_(const char *func_name)
{
	if (!fc) {
		blog(LOG_WARNING, "Tried to call %s with no callbacks!", func_name);
		return false;
	}

	return true;
}

#define callbacks_valid() callbacks_valid_(__FUNCTION__)



FRONTEND_API void pls_delete_specific_url_cookie(const QString &url)
{
	if (callbacks_valid()) {
		fc->pls_del_specific_url_cookie(url);
	}
}
FRONTEND_API QJsonObject pls_ssmap_to_json(const QMap<QString, QString> &ssmap)
{
	if (callbacks_valid()) {
		//return fc->pls_ssmap_to_json(ssmap);
	}
	return QJsonObject();
}

FRONTEND_API bool pls_browser_view(QJsonObject &result, const QUrl &url, pls_result_checking_callback_t callback, QWidget *parent)
{
	if (callbacks_valid()) {
		return fc->pls_browser_view(result, url, callback, parent);
	}
	return false;
}
FRONTEND_API bool pls_browser_view(QJsonObject &result, const QUrl &url, const std::map<std::string, std::string> &headers, pls_result_checking_callback_t callback, QWidget *parent)
{
	if (callbacks_valid()) {
		return fc->pls_browser_view(result, url, headers, callback, parent);
	}
	return false;
}

FRONTEND_API bool pls_rtmp_view(QJsonObject &result, PLSLoginInfo *login_info, QWidget *parent)
{
	if (callbacks_valid()) {
		//return fc->pls_rtmp_view(result, login_info, parent);
	}
	return false;
}

FRONTEND_API bool pls_channel_login(QJsonObject &result, QWidget *parent)
{
	if (callbacks_valid()) {
		//return fc->pls_channel_login(result, parent);
	}
	return false;
}

FRONTEND_API bool pls_get_encrypt_url(QUrl &url, const QString &urlStr, const QString &hMackey)
{
	return true;
}

FRONTEND_API QList<QNetworkCookie> pls_get_cookies(const QJsonObject &cookies)
{
	QList<QNetworkCookie> cookieList;
	for (auto it = cookies.constBegin(); it != cookies.constEnd(); ++it) {
		QNetworkCookie cookie;
		cookie.setName(it.key().toUtf8());
		cookie.setValue(it.value().toString().toUtf8());
		cookieList.push_back(cookie);
	}
	return cookieList;
}

FRONTEND_API bool pls_sns_user_info(QJsonObject &result, const QList<QNetworkCookie> &cookies, const QString &urlStr)
{
	if (callbacks_valid()) {
		//return fc->pls_sns_user_info(result, cookies, urlStr);
	}
	return false;
}

FRONTEND_API bool pls_network_environment_reachable()
{
	if (callbacks_valid()) {
		return fc->pls_network_environment_reachable();
	}
	return false;
}
FRONTEND_API QString pls_get_oauth_token_from_url(const QString &url_str)
{
	QUrl qurl(url_str);
	QString token = qurl.fragment().split("&")[0];
	QString tokenStr = token.mid(token.indexOf('=') + 1);
	if ("access_denied" == tokenStr) {
		tokenStr = "";
	}
	return tokenStr;
}
FRONTEND_API QString pls_get_youtube_code_from_url(const QString &url_str)
{
	QUrl qurl(url_str);
	QString token = qurl.query().split("&")[0];
	QString codeStr = token.mid(token.indexOf('=') + 1);
	if ("access_denied" == codeStr) {
		codeStr = "";
	}
	return codeStr;
}
FRONTEND_API bool pls_prism_token_is_vaild(const QString &urlStr)
{

	return false;
}
FRONTEND_API QString pls_prism_user_thumbnail_path()
{

	return QString();
}
FRONTEND_API void pls_prism_change_over_login_view()
{

}
FRONTEND_API bool pls_channel_login(QJsonObject &result, const QString &accountName, QWidget *parent)
{

	return false;
}

FRONTEND_API void pls_prism_logout()
{

}

FRONTEND_API void pls_prism_signout()

{

}
FRONTEND_API QUrl pls_get_encrypt_url(const QString &url_str, const QString &mac_key)
{
	QUrl url;
	pls_get_encrypt_url(url, url_str, mac_key);
	return url;
}


FRONTEND_API long long pls_get_gpop_bulid_data()
{

	return -1;
}
FRONTEND_API QString pls_get_gpop_log_level()
{

	return QString();
}

FRONTEND_API QString pls_get_gcc_data()
{

	return QString();
}

FRONTEND_API QString pls_get_prism_token()
{

	return QString();
}

FRONTEND_API QString pls_get_prism_email()
{

	return QString("abc@cd.com");
}

FRONTEND_API QString pls_get_prism_thmbanilurl()
{

	return QString();
}

FRONTEND_API QString pls_get_prism_nickname()
{

	return QString("******");
}

FRONTEND_API QString pls_get_prism_usercode()
{

	return QString("******");
}

FRONTEND_API QWidget *pls_get_main_view()
{
	if (callbacks_valid()) {
		return fc->pls_get_main_view();
	}
	return nullptr;
}
FRONTEND_API QWidget *pls_get_toplevel_view(QWidget *widget)
{
	if (callbacks_valid()) {
		return fc->pls_get_toplevel_view(widget);
	}
	return nullptr;
}

FRONTEND_API QByteArray pls_get_prism_cookie()
{

	return QByteArray();
}

FRONTEND_API void pls_frontend_add_event_callback(pls_frontend_event_cb callback, void *context)
{
	if (callbacks_valid()) {
		return fc->pls_frontend_add_event_callback(callback, context);
	}
}
FRONTEND_API void pls_frontend_add_event_callback(pls_frontend_event event, pls_frontend_event_cb callback, void *context)
{
	if (callbacks_valid()) {
		return fc->pls_frontend_add_event_callback(event, callback, context);
	}
}
FRONTEND_API void pls_frontend_add_event_callback(QList<pls_frontend_event> events, pls_frontend_event_cb callback, void *context)
{
	if (callbacks_valid()) {
		return fc->pls_frontend_add_event_callback(events, callback, context);
	}
}
FRONTEND_API void pls_frontend_remove_event_callback(pls_frontend_event_cb callback, void *context)
{
	if (callbacks_valid()) {
		return fc->pls_frontend_remove_event_callback(callback, context);
	}
}
FRONTEND_API void pls_frontend_remove_event_callback(pls_frontend_event event, pls_frontend_event_cb callback, void *context)
{
	if (callbacks_valid()) {
		return fc->pls_frontend_remove_event_callback(event, callback, context);
	}
}
FRONTEND_API void pls_frontend_remove_event_callback(QList<pls_frontend_event> events, pls_frontend_event_cb callback, void *context)
{
	if (callbacks_valid()) {
		return fc->pls_frontend_remove_event_callback(events, callback, context);
	}
}

FRONTEND_API QString pls_get_theme_dir_path()
{
	if (callbacks_valid()) {
		return fc->pls_get_theme_dir_path();
	}
	return QString();
}

FRONTEND_API QString pls_get_color_filter_dir_path()
{
	if (callbacks_valid()) {
		return fc->pls_get_color_filter_dir_path();
	}
	return QString();
}

FRONTEND_API QString pls_get_user_path(const QString &path)
{
	if (getConfigPath) {
		char configPath[512];
		int ret = getConfigPath(configPath, sizeof(configPath), path.toUtf8());
		return QString(configPath);
	}
	return QString();
}
FRONTEND_API void pls_toast_message(pls_toast_info_type type, const QString &message, int auto_close)
{
	if (callbacks_valid()) {
		return fc->pls_toast_message(type, message, auto_close);
	}
}

FRONTEND_API void pls_toast_clear()
{
	if (callbacks_valid()) {
		return fc->pls_toast_clear();
	}
}
FRONTEND_API void pls_set_main_view_side_bar_user_button_icon(const QIcon &icon)
{
	if (callbacks_valid()) {
		return fc->pls_set_main_view_side_bar_user_button_icon(icon);
	}
}

FRONTEND_API void pls_load_chat_dock(const QString &objectName, const std::string &url, const std::string &white_popup_url, const std::string &startup_script)
{
	if (callbacks_valid()) {
		return fc->pls_load_chat_dock(objectName, url, white_popup_url, startup_script);
	}
}

FRONTEND_API void pls_unload_chat_dock(const QString &objectName)
{
	if (callbacks_valid()) {
		return fc->pls_unload_chat_dock(objectName);
	}
}

FRONTEND_API const char *pls_basic_config_get_string(const char *section, const char *name, const char *defaultValue)
{
	if (callbacks_valid()) {
		return fc->pls_basic_config_get_string(section, name, defaultValue);
	}
	return defaultValue;
}

FRONTEND_API int64_t pls_basic_config_get_int(const char *section, const char *name, int64_t defaultValue)
{
	if (callbacks_valid()) {
		return fc->pls_basic_config_get_int(section, name, defaultValue);
	}
	return defaultValue;
}

FRONTEND_API uint64_t pls_basic_config_get_uint(const char *section, const char *name, uint64_t defaultValue)
{
	if (callbacks_valid()) {
		return fc->pls_basic_config_get_uint(section, name, defaultValue);
	}
	return defaultValue;
}

FRONTEND_API bool pls_basic_config_get_bool(const char *section, const char *name, bool defaultValue)
{
	if (callbacks_valid()) {
		return fc->pls_basic_config_get_bool(section, name, defaultValue);
	}
	return defaultValue;
}

FRONTEND_API double pls_basic_config_get_double(const char *section, const char *name, double defaultValue)
{
	if (callbacks_valid()) {
		return fc->pls_basic_config_get_double(section, name, defaultValue);
	}
	return defaultValue;
}

FRONTEND_API QVariantMap pls_get_new_notice_Info()
{

	return QVariantMap();
}
FRONTEND_API QString pls_get_win_os_version()
{
	if (callbacks_valid()) {
		return fc->pls_get_win_os_version();
	}
	return QString();
}

FRONTEND_API bool pls_is_living_or_recording()
{
	if (callbacks_valid()) {
		return fc->pls_is_living_or_recording();
	}
	return false;
}

// add tools menu seperator
FRONTEND_API void pls_add_tools_menu_seperator()
{
	if (callbacks_valid()) {
		fc->pls_add_tools_menu_seperator();
	}
}

FRONTEND_API bool pls_is_streaming()
{
	if (obs_output_t *streaming = obs_frontend_get_streaming_output(); streaming) {
		bool active = obs_output_active(streaming);
		obs_output_release(streaming);
		return active;
	}
	return false;
}

FRONTEND_API bool pls_is_recording()
{
	if (obs_output_t *recording = obs_frontend_get_recording_output(); recording) {
		bool active = obs_output_active(recording);
		obs_output_release(recording);
		return active;
	}
	return false;
}

FRONTEND_API void pls_flush_style(QWidget *widget)
{
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
}

FRONTEND_API void pls_flush_style_recursive(QWidget *widget)
{
	pls_flush_style(widget);

	for (QObject *child : widget->children()) {
		if (child->isWidgetType()) {
			pls_flush_style_recursive(dynamic_cast<QWidget *>(child));
		}
	}
}

FRONTEND_API void pls_flush_style(QWidget *widget, const char *propertyName, const QVariant &propertyValue)
{
	widget->setProperty(propertyName, propertyValue);
	pls_flush_style(widget);
}

FRONTEND_API void pls_flush_style_recursive(QWidget *widget, const char *propertyName, const QVariant &propertyValue)
{
	widget->setProperty(propertyName, propertyValue);
	pls_flush_style_recursive(widget);
}

FRONTEND_API void pls_load_stylesheet(QWidget *widget, const QStringList &filePaths)
{
	if (widget) {
		QString qssStr;
		for (auto filePath : filePaths) {
			QFile f(filePath);
			f.open(QIODevice::ReadOnly);
			qssStr += f.readAll();
			f.close();
		}
		widget->setStyleSheet(qssStr);
	}
}

FRONTEND_API void pls_window_right_margin_fit(QWidget *widget)
{
	QRect windowGeometry = widget->normalGeometry();
	int mostRightPostion = INT_MIN;

	for (QScreen *screen : QGuiApplication::screens()) {
		if (screen->availableGeometry().right() > mostRightPostion) {
			mostRightPostion = screen->availableGeometry().right();
		}
	}

	if (mostRightPostion < windowGeometry.right()) {
		widget->move(mostRightPostion - windowGeometry.width(), windowGeometry.y());
		widget->repaint();
	}
}

#define LOAD_DEV_SERVER(x) x = x##_DEV
FRONTEND_API void pls_load_dev_server()
{
	LOAD_DEV_SERVER(PLS_FACEBOOK_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_GOOGLE_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_LINE_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_NAVER_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_TWITTER_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_TWITCH_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_SNS_LOGIN_SIGNUP_URL);
	LOAD_DEV_SERVER(PLS_EMAIL_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_EMAIL_SIGNUP_URL);
	LOAD_DEV_SERVER(PLS_EMAIL_FOGETTON_URL);
	LOAD_DEV_SERVER(PLS_TERM_OF_USE_URL);
	LOAD_DEV_SERVER(PLS_PRIVACY_URL);
	LOAD_DEV_SERVER(PLS_HMAC_KEY);
	LOAD_DEV_SERVER(PLS_LOGOUT_URL);
	LOAD_DEV_SERVER(PLS_TOKEN_SESSION_URL);
	LOAD_DEV_SERVER(PLS_NOTICE_URL);
	LOAD_DEV_SERVER(PLS_SIGNOUT_URL);
	LOAD_DEV_SERVER(PLS_CHANGE_PASSWORD);

	// Twitch
	LOAD_DEV_SERVER(TWITCH_CLIENT_ID);
	LOAD_DEV_SERVER(TWITCH_REDIRECT_URI);

	// Youtube
	LOAD_DEV_SERVER(YOUTUBE_CLIENT_ID);
	LOAD_DEV_SERVER(YOUTUBE_CLIENT_KEY);
	LOAD_DEV_SERVER(YOUTUBE_CLIENT_URL);

	LOAD_DEV_SERVER(PLS_CATEGORY);
	// gpop
	LOAD_DEV_SERVER(PLS_GPOP);
	// rtmp
	LOAD_DEV_SERVER(PLS_RTMP_ADD);
	LOAD_DEV_SERVER(PLS_RTMP_MODIFY);
	LOAD_DEV_SERVER(PLS_RTMP_DELETE);
	LOAD_DEV_SERVER(PLS_RTMP_LIST);

	LOAD_DEV_SERVER(g_streamKeyPrismHelperEn);
	LOAD_DEV_SERVER(g_streamKeyPrismHelperKr);

	LOAD_DEV_SERVER(LIBRARY_POLICY_PC_ID);

	LOAD_DEV_SERVER(MQTT_SERVER);
	LOAD_DEV_SERVER(MQTT_SERVER_PW);
	LOAD_DEV_SERVER(MQTT_SERVER_WEB);

	LOAD_DEV_SERVER(CONTACT_SEND_EMAIL_URL);

	LOAD_DEV_SERVER(PRISM_API_BASE);
	LOAD_DEV_SERVER(PRISM_AUTH_API_BASE);
}

FRONTEND_API PfnGetConfigPath pls_get_config_path(void)
{
	return getConfigPath;
}

FRONTEND_API void pls_set_config_path(PfnGetConfigPath getConfigPath)
{
	::getConfigPath = getConfigPath;
}

FRONTEND_API void pls_http_request_head(QVariantMap &headMap, bool hasGacc)
{
}
FRONTEND_API std::string pls_get_offline_javaScript()
{
	QDir appDir(qApp->applicationDirPath());
	QString path = appDir.absoluteFilePath(QString("data/prism-studio/webpage/pls_offline_page.js"));
	QFile file(path);
	file.open(QIODevice::ReadOnly | QIODevice::Text);
	QByteArray byteArray = file.readAll();
	file.close();
	return byteArray.toStdString();
}
