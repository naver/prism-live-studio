#include "frontend-internal.hpp"
#include "IMACManager.h"
#include <optional>
#include <QUrl>
#include <QStyle>
#include <QDebug>
#include <QMetaEnum>
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
#include "alert-view.hpp"
#include "pls/media-info.h"
#include "log/log.h"
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>
#include <atomic>
#include <QUuid>
#include <QPointer>
#include <QDialog>

#define VERSION_COMPARE_COUNT 3
#define HEADER_USER_AGENT_KEY QStringLiteral("User-Agent")
#define HEADER_PRISM_LANGUAGE QStringLiteral("Accept-Language")
#define HEADER_PRISM_GCC QStringLiteral("X-prism-cc")
#define HEADER_PRISM_USERCODE QStringLiteral("X-prism-usercode")
#define HEADER_PRISM_OS QStringLiteral("X-prism-os")
#define HEADER_PRISM_IP QStringLiteral("X-prism-ip")
#define HEADER_PRISM_DEVICE QStringLiteral("X-prism-device")
#define HEADER_PRISM_APPVERSION QStringLiteral("X-prism-appversion")

static pls_frontend_callbacks *fc = nullptr;
static PfnGetConfigPath getConfigPath = nullptr;
static std::atomic<bool> g_bMainWindowClosing = false;
static QList<QPointer<QDialog>> g_dialog_views;

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

extern "C" FRONTEND_API PfnGetConfigPath pls_get_config_path_c(void)
{
	return pls_get_config_path();
}

// zhangdewen for obs-browser chat source use
extern "C" FRONTEND_API void pls_get_chat_source_url_c(QString *chat_source_url)
{
	if (chat_source_url) {
		*chat_source_url = CHAT_SOURCE_URL;
	}
}

// zhangdewen for obs-browser chat source use
extern "C" FRONTEND_API void pls_get_prism_cookie_value_c(QString *prism_cookie_value)
{
	if (prism_cookie_value) {
		*prism_cookie_value = QString::fromUtf8(pls_get_prism_cookie_value());
	}
}

// zhangdewen for obs-browser chat source use
extern "C" FRONTEND_API int pls_get_actived_chat_channel_count_c()
{
	return pls_get_actived_chat_channel_count();
}

// zhangdewen for obs-browser chat source use
extern "C" FRONTEND_API int pls_get_prism_live_seq_c()
{
	return pls_get_prism_live_seq();
}

// zhangdewen for obs-browser chat source use
extern "C" FRONTEND_API int pls_is_create_souce_in_loading_c()
{
	return pls_is_create_souce_in_loading() ? 1 : 0;
}

// zhangdewen for obs-browser chat source use
extern "C" FRONTEND_API void pls_network_state_monitor_c(void (*callback)(int))
{
	pls_network_state_monitor([=](bool state) { callback(state); });
}

// zhangdewen for obs-browser chat source use
extern "C" FRONTEND_API int pls_get_network_state_c()
{
	return pls_get_network_state() ? 1 : 0;
}

extern "C" FRONTEND_API QWidget *pls_get_toplevel_view_c(QWidget *widget)
{
	return pls_get_toplevel_view(widget);
}

extern "C" FRONTEND_API void pls_get_prism_usercode_c(QString *prism_usercode)
{
	if (prism_usercode) {
		*prism_usercode = pls_get_prism_usercode();
	}
}

FRONTEND_API void pls_frontend_set_callbacks_internal(pls_frontend_callbacks *callbacks)
{
	obs_frontend_set_callbacks_internal(callbacks);
	fc = callbacks;
}

static inline bool callbacks_valid_(const char *func_name)
{
	if (!fc) {
		PLS_WARN(MAIN_FRONTEND_API, "Tried to call %s with no callbacks!", func_name);
		return false;
	}

	return true;
}

#define callbacks_valid() callbacks_valid_(__FUNCTION__)

FRONTEND_API bool pls_register_login_info(PLSLoginInfo *login_info)
{
	if (callbacks_valid()) {
		return fc->pls_register_login_info(login_info);
	}
	return false;
}

FRONTEND_API void pls_unregister_login_info(PLSLoginInfo *login_info)
{
	if (callbacks_valid()) {
		fc->pls_unregister_login_info(login_info);
	}
}

FRONTEND_API int pls_get_login_info_count()
{
	if (callbacks_valid()) {
		return fc->pls_get_login_info_count();
	}
	return 0;
}

FRONTEND_API PLSLoginInfo *pls_get_login_info(int index)
{
	if (callbacks_valid()) {
		return fc->pls_get_login_info(index);
	}
	return nullptr;
}

FRONTEND_API void del_pannel_cookies(const QString &pannelName)
{
	if (callbacks_valid()) {
		return fc->del_pannel_cookies(pannelName);
	}
}

FRONTEND_API void pls_delete_specific_url_cookie(const QString &url, const QString &cookieName)
{
	if (callbacks_valid()) {
		fc->pls_del_specific_url_cookie(url, cookieName);
	}
}

FRONTEND_API QJsonObject pls_ssmap_to_json(const QMap<QString, QString> &ssmap)
{
	if (callbacks_valid()) {
		return fc->pls_ssmap_to_json(ssmap);
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
FRONTEND_API bool pls_browser_view(QJsonObject &result, const QUrl &url, const std::map<std::string, std::string> &headers, const QString &pannelName, pls_result_checking_callback_t callback,
				   QWidget *parent)
{
	if (callbacks_valid()) {
		return fc->pls_browser_view(result, url, headers, pannelName, std::string(), callback, parent);
	}
	return false;
}

FRONTEND_API bool pls_browser_view(QJsonObject &result, const QUrl &url, const std::map<std::string, std::string> &headers, const QString &pannelName, const std::string &script,
				   pls_result_checking_callback_t callback, QWidget *parent)
{
	if (callbacks_valid()) {
		return fc->pls_browser_view(result, url, headers, pannelName, script, callback, parent);
	}
	return false;
}

FRONTEND_API bool pls_rtmp_view(QJsonObject &result, PLSLoginInfo *login_info, QWidget *parent)
{
	if (callbacks_valid()) {
		return fc->pls_rtmp_view(result, login_info, parent);
	}
	return false;
}

FRONTEND_API bool pls_channel_login(QJsonObject &result, QWidget *parent)
{
	if (callbacks_valid()) {
		return fc->pls_channel_login(result, parent);
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
		return fc->pls_sns_user_info(result, cookies, urlStr);
	}
	return false;
}
FRONTEND_API bool pls_google_user_info(std::function<void(bool ok, const QJsonObject &)> callback, const QString &redirect_uri, const QString &code, QObject *receiver)
{
	if (callbacks_valid()) {
		return fc->pls_google_user_info(callback, redirect_uri, code, receiver);
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
	for (const auto &str : qurl.fragment(QUrl::FullyDecoded).split("&")) {
		if (str.startsWith("access_token", Qt::CaseInsensitive)) {
			QString tokenStr = str.mid(str.indexOf('=') + 1);
			return tokenStr.remove('#');
		}
	}
	return QString();
}
FRONTEND_API QString pls_get_code_from_url(const QString &url_str)
{
	QUrl qurl(url_str);
	for (const auto &str : qurl.query(QUrl::FullyDecoded).split("&")) {
		if (str.startsWith("code", Qt::CaseInsensitive)) {
			QString codeStr = str.mid(str.indexOf('=') + 1);
			return codeStr.remove('#');
		}
	}
	return QString();
}
FRONTEND_API bool pls_prism_token_is_vaild(const QString &urlStr)
{
	if (callbacks_valid()) {
		return fc->pls_prism_token_is_vaild(urlStr);
	}
	return false;
}
FRONTEND_API QString pls_prism_user_thumbnail_path()
{
	if (callbacks_valid()) {
		return fc->pls_prism_user_thumbnail_path();
	}
	return QString();
}
FRONTEND_API void pls_prism_change_over_login_view()
{
	if (callbacks_valid()) {
		return fc->pls_prism_change_over_login_view();
	}
}

FRONTEND_API void pls_login_async(std::function<void(bool ok, const QJsonObject &result)> &&callback, const QString &platformName, QWidget *parent, PLSLoginInfo::UseFor useFor)
{
	if (!pls_get_network_state()) {
		PLSAlertView::warning(parent, QTStr("Alert.Title"), QTStr("login.check.note.network"));
	} else {
		for (int index = 0; index != pls_get_login_info_count(); ++index) {
			PLSLoginInfo *channelInfo = pls_get_login_info(index);
			if (0 == platformName.compare(channelInfo->name(), Qt::CaseInsensitive)) {
				if (channelInfo->loginWithAccountImplementType() == PLSLoginInfo::ImplementType::Synchronous) {
					QJsonObject result;
					bool ok = channelInfo->loginWithAccount(result, useFor, parent);
					callback(ok, result);
				} else {
					channelInfo->loginWithAccountAsync(callback, useFor, parent);
				}
			}
		}
	}
}

FRONTEND_API void pls_channel_login_async(std::function<void(bool ok, const QJsonObject &result)> &&callback, const QString &accountName, QWidget *parent)
{
	pls_login_async(std::forward<std::function<void(bool ok, const QJsonObject &result)>>(callback), accountName, parent, PLSLoginInfo::UseFor::Channel);
}

FRONTEND_API void pls_prism_logout()
{
	if (callbacks_valid()) {
		fc->pls_prism_logout(PLS_LOGOUT_URL.arg(PRISM_SSL));
	}
}

FRONTEND_API void pls_prism_signout()

{
	if (callbacks_valid()) {
		fc->pls_prism_logout(PLS_SIGNOUT_URL.arg(PRISM_SSL), LoginInfoType::PrismSignoutInfo);
	}
}
FRONTEND_API QUrl pls_get_encrypt_url(const QString &url_str, const QString &mac_key)
{
	QUrl url;
	pls_get_encrypt_url(url, url_str, mac_key);
	return url;
}
FRONTEND_API Common pls_get_gpop_common()
{
	if (callbacks_valid()) {
		return fc->pls_get_common();
	}
	return Common();
}

FRONTEND_API VliveNotice pls_get_vlive_notice()
{
	if (callbacks_valid()) {
		return fc->pls_get_vlive_notice();
	}
	return VliveNotice();
}

FRONTEND_API QMap<QString, SnsCallbackUrl> pls_get_gpop_snscallback_urls()
{
	if (callbacks_valid()) {
		return fc->pls_get_snscallback_urls();
	}
	return QMap<QString, SnsCallbackUrl>();
}

FRONTEND_API Connection pls_get_gpop_connection()
{
	if (callbacks_valid()) {
		return fc->pls_get_connection();
	}
	return Connection();
}

FRONTEND_API QMap<int, RtmpDestination> pls_get_rtmpDestination()
{
	if (callbacks_valid()) {
		return fc->pls_get_rtmpDestination();
	}
	return QMap<int, RtmpDestination>();
}

FRONTEND_API QString pls_get_gcc_data()
{
	if (callbacks_valid()) {
		return fc->pls_get_gcc_data();
	}
	return QString();
}

FRONTEND_API QString pls_get_prism_token()
{
	if (callbacks_valid()) {
		return fc->pls_get_prism_token();
	}
	return QString();
}

FRONTEND_API QString pls_get_prism_email()
{
	if (callbacks_valid()) {
		return fc->pls_get_prism_email();
	}
	return QString();
}

FRONTEND_API QString pls_get_prism_thmbanilurl()
{
	if (callbacks_valid()) {
		return fc->pls_get_prism_thmbanilurl();
	}
	return QString();
}

FRONTEND_API QString pls_get_prism_nickname()
{
	if (callbacks_valid()) {
		return fc->pls_get_prism_nickname();
	}
	return QString();
}

FRONTEND_API QString pls_get_prism_usercode()
{
	if (callbacks_valid()) {
		return fc->pls_get_prism_usercode();
	}
	return QString();
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
	if (callbacks_valid()) {
		return fc->pls_get_prism_cookie();
	}
	return nullptr;
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
		getConfigPath(configPath, sizeof(configPath), path.toUtf8());
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

FRONTEND_API void pls_toast_message(pls_toast_info_type type, const QString &message, const QString &url, const QString &replaceStr, int auto_close)
{
	if (callbacks_valid()) {
		return fc->pls_toast_message(type, message, url, replaceStr, auto_close);
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

FRONTEND_API bool pls_inside_visible_screen_area(QRect geometry)
{
	QList<QScreen *> screens = QApplication::screens();
	for (int i = 0; i < screens.size(); i++) {
		QScreen *screen = screens[i];
		if (!screen) {
			continue;
		}
		QRect rect = screen->availableGeometry();
		if (rect.intersects(geometry)) {
			return true;
		}
	}
	return false;
}

FRONTEND_API pls_check_update_result_t pls_check_update(QString &gcc, bool &is_force, QString &version, QString &file_url, QString &update_info_url)
{
	if (callbacks_valid()) {
		return fc->pls_check_update(gcc, is_force, version, file_url, update_info_url);
	}
	return pls_check_update_result_t::Failed;
}

FRONTEND_API bool pls_check_lastest_version(QString &update_info_url)
{
	if (callbacks_valid()) {
		return fc->pls_check_lastest_version(update_info_url);
	}
	return false;
}

FRONTEND_API pls_upload_file_result_t pls_upload_contactus_files(const QString &email, const QString &question, const QList<QFileInfo> files)
{
	if (callbacks_valid()) {
		return fc->pls_upload_contactus_files(email, question, files);
	}
	return pls_upload_file_result_t::Ok;
}

FRONTEND_API bool pls_show_update_info_view(bool is_force, const QString &version, const QString &file_url, const QString &update_info_url, bool is_manual, QWidget *parent)
{
	if (callbacks_valid()) {
		return fc->pls_show_update_info_view(is_force, version, file_url, update_info_url, is_manual, parent);
	}
	return false;
}
FRONTEND_API bool pls_download_update(QString &local_file_path, const QString &file_url, PLSCancel &cancel, const std::function<void(qint64 download_bytes, qint64 total_bytes)> &progress)
{
	if (callbacks_valid()) {
		return fc->pls_download_update(local_file_path, file_url, cancel, progress);
	}
	return false;
}
FRONTEND_API bool pls_install_update(const QString &file_path)
{
	if (callbacks_valid()) {
		return fc->pls_install_update(file_path);
	}
	return false;
}

FRONTEND_API QVariantMap pls_get_new_notice_Info()
{
	if (callbacks_valid()) {
		return fc->pls_get_new_notice_Info();
	}
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

FRONTEND_API void pls_flush_style_recursive(QWidget *widget, int recursiveDeep)
{
	pls_flush_style(widget);

	for (QObject *child : widget->children()) {
		if (child->isWidgetType()) {
			if (recursiveDeep != 0) {
				pls_flush_style_recursive(dynamic_cast<QWidget *>(child), recursiveDeep - 1);
			} else {
				pls_flush_style(dynamic_cast<QWidget *>(child));
			}
		}
	}
}

FRONTEND_API void pls_flush_style(QWidget *widget, const char *propertyName, const QVariant &propertyValue)
{
	widget->setProperty(propertyName, propertyValue);
	pls_flush_style(widget);
}

FRONTEND_API void pls_flush_style_recursive(QWidget *widget, const char *propertyName, const QVariant &propertyValue, int recursiveDeep)
{
	widget->setProperty(propertyName, propertyValue);
	pls_flush_style_recursive(widget, recursiveDeep);
}

FRONTEND_API void pls_load_stylesheet(QWidget *widget, const QStringList &filePaths)
{
	if (widget) {
		QString qssStr;
		for (const auto &filePath : filePaths) {
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
	LOAD_DEV_SERVER(PLS_GOOGLE_LOGIN_URL_TOKEN);
	LOAD_DEV_SERVER(PLS_LINE_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_NAVER_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_TWITTER_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_TWITCH_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_WHALESPACE_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_SNS_LOGIN_SIGNUP_URL);
	LOAD_DEV_SERVER(PLS_EMAIL_LOGIN_URL);
	LOAD_DEV_SERVER(PLS_EMAIL_SIGNUP_URL);
	LOAD_DEV_SERVER(PLS_EMAIL_FOGETTON_URL);
	LOAD_DEV_SERVER(PLS_TERM_OF_USE_URL);
	LOAD_DEV_SERVER(PLS_PRIVACY_URL);
	LOAD_DEV_SERVER(PLS_PC_HMAC_KEY);
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
	LOAD_DEV_SERVER(PLS_GPOP_CATEGORY);

	// rtmp
	LOAD_DEV_SERVER(PLS_RTMP_ADD);
	LOAD_DEV_SERVER(PLS_RTMP_MODIFY);
	LOAD_DEV_SERVER(PLS_RTMP_DELETE);
	LOAD_DEV_SERVER(PLS_RTMP_LIST);

	LOAD_DEV_SERVER(g_streamKeyPrismHelperEn);
	LOAD_DEV_SERVER(g_streamKeyPrismHelperKr);

	LOAD_DEV_SERVER(LIBRARY_POLICY_PC_ID);
	LOAD_DEV_SERVER(LIBRARY_SENSETIME_PC_ID);

	LOAD_DEV_SERVER(MQTT_SERVER);
	LOAD_DEV_SERVER(MQTT_SERVER_PW);
	LOAD_DEV_SERVER(MQTT_SERVER_WEB);

	LOAD_DEV_SERVER(UPDATE_URL);
	LOAD_DEV_SERVER(LASTEST_UPDATE_URL);
	LOAD_DEV_SERVER(CONTACT_SEND_EMAIL_URL);

	LOAD_DEV_SERVER(PRISM_AUTH_API_BASE);

	// NaverTV
	LOAD_DEV_SERVER(CHANNEL_NAVERTV_LOGIN);
	LOAD_DEV_SERVER(CHANNEL_NAVERTV_GET_AUTH);
	LOAD_DEV_SERVER(CHANNEL_NAVERTV_COMMENT_OPTIONS);
	LOAD_DEV_SERVER(CHANNEL_NAVERTV_UPLOAD_IMAGE);
	LOAD_DEV_SERVER(CHANNEL_NAVERTV_AUTHORIZE);
	LOAD_DEV_SERVER(CHANNEL_NAVERTV_AUTHORIZE_REDIRECT);
	LOAD_DEV_SERVER(CHANNEL_NAVERTV_TOKEN);

	// VLive
	LOAD_DEV_SERVER(CHANNEL_VLIVE_LOGIN);
	LOAD_DEV_SERVER(CHANNEL_VLIVE_LOGIN_JUMP);
	LOAD_DEV_SERVER(CHANNEL_VLIVE_LOGIN_JUMP_1);
	LOAD_DEV_SERVER(CHANNEL_VLIVE_SHARE);

	//Band
	LOAD_DEV_SERVER(CHANNEL_BAND_LOGIN);
	LOAD_DEV_SERVER(CHANNEL_BAND_ID);
	LOAD_DEV_SERVER(CHANNEL_BAND_REDIRECTURL);
	LOAD_DEV_SERVER(CHANNEL_BAND_SECRET);
	LOAD_DEV_SERVER(CHANNEL_BAND_AUTH);
	LOAD_DEV_SERVER(CHANNEL_BAND_REFRESH_TOKEN);

	LOAD_DEV_SERVER(CHANNEL_BAND_USER_PROFILE);
	LOAD_DEV_SERVER(CHANNEL_BAND_CATEGORY);
	LOAD_DEV_SERVER(CHANNEL_BAND_LIVE_CREATE);
	LOAD_DEV_SERVER(CHANNEL_BAND_LIVE_OFF);

	//Facebook
	LOAD_DEV_SERVER(CHANNEL_FACEBOOK_CLIENT_ID);
	LOAD_DEV_SERVER(CHANNEL_FACEBOOK_SECRET);

	// Chat Widget
	LOAD_DEV_SERVER(CHAT_SOURCE_URL);

	//Naver Shopping Live
	LOAD_DEV_SERVER(CHANNEL_NAVER_SHOPPING_LIVE_LOGIN);
	LOAD_DEV_SERVER(CHANNEL_NAVER_SHOPPING_LIVE_SMART_STORE_LOGIN);
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
	return;
}
FRONTEND_API std::string pls_get_offline_javaScript()
{
	QDir appDir(qApp->applicationDirPath());
	QString path = appDir.absoluteFilePath(QString("data/prism-studio/webpage/pls_offline_page.js"));
	QFile file(path);
	file.open(QIODevice::ReadOnly | QIODevice::Text);
	QByteArray byteArray = file.readAll();
	file.close();
	QString str = byteArray;
	QString _strNet = QTStr("login.check.note.network").replace("\n", "\\n");
	QString _strRetry = QTStr("Retry").replace("\n", "\\n");
	str = str.replace("net_err_key", _strNet).replace("retry_key", _strRetry);
	return str.toStdString();
}

FRONTEND_API QString pls_get_md5(const QString &originStr, const QString &prefix)
{
	QString fileText = originStr;
	QString fileCut = fileText.remove(256, fileText.count() - 256);
	QByteArray ba;
	ba = QCryptographicHash::hash(fileCut.toUtf8(), QCryptographicHash::Md5);
	QString md5Str(ba.toHex());
	if (!prefix.isEmpty()) {
		md5Str = prefix + "-" + md5Str;
	}
	return md5Str;
}

FRONTEND_API void pls_start_broadcast(bool toStart)
{
	if (callbacks_valid()) {
		fc->pls_start_broadcast(toStart);
	}
}

FRONTEND_API void pls_start_record(bool toStart)
{
	if (callbacks_valid()) {
		fc->pls_start_record(toStart);
	}
}

FRONTEND_API OBSSource pls_get_source_by_name(const char *name)
{
	if (nullptr == name || 0 == strlen(name)) {
		return OBSSource();
	}

	OBSSource source = obs_get_source_by_name(name);
	if (source)
		obs_source_release(source);
	return source;
}

FRONTEND_API OBSData pls_get_source_setting(const obs_source_t *source)
{
	OBSData sourceSettings = obs_source_get_settings(source);
	if (sourceSettings)
		obs_data_release(sourceSettings);
	return sourceSettings;
}

FRONTEND_API OBSData pls_get_source_private_setting(const obs_source_t *source)
{
	OBSData sourceSettings = obs_source_get_private_settings((obs_source_t *)source);
	if (sourceSettings)
		obs_data_release(sourceSettings);
	return sourceSettings;
}

FRONTEND_API OBSSource pls_get_source_by_pointer_address(void *pointerAddress)
{
	OBSSource source_output = nullptr;

	if (pointerAddress <= 0)
		return source_output;

	auto searchForSource = [pointerAddress, &source_output](obs_source_t *source) {
		if (!source)
			return true;

		if (pointerAddress == source) {
			source_output = source;
			return false;
		}

		return true;
	};

	using SearchForSource_t = decltype(searchForSource);
	auto enumSource = [](void *param, obs_source_t *source) { return (*reinterpret_cast<SearchForSource_t *>(param))(source); };
	obs_enum_all_sources(enumSource, &searchForSource);

	return source_output;
}

struct FindSceneitemHelper {
	void *address;
	OBSSceneItem output;
};

static bool FindSceneItemCb(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	FindSceneitemHelper *helper = (FindSceneitemHelper *)ptr;

	if (item == helper->address) {
		helper->output = item;
		return false;
	}

	if (obs_sceneitem_is_group(item)) {
		obs_sceneitem_group_enum_items(item, FindSceneItemCb, ptr);
	}

	return true;
}

OBSSceneItem get_sceneitem_by_pointer_address_internal(OBSScene destScene, FindSceneitemHelper *helper)
{
	obs_scene_enum_items(destScene, FindSceneItemCb, helper);
	return helper->output;
}

FRONTEND_API OBSSceneItem pls_get_sceneitem_by_pointer_address(OBSScene destScene, void *sceneitemAddress)
{
	FindSceneitemHelper helper;
	helper.address = sceneitemAddress;
	helper.output = NULL;

	return get_sceneitem_by_pointer_address_internal(destScene, &helper);
}

FRONTEND_API OBSSceneItem pls_get_sceneitem_by_pointer_address(void *sceneitemAddress)
{
	if (!sceneitemAddress) {
		return OBSSceneItem();
	}

	auto cb = [](void *helper, obs_source_t *src) {
		obs_scene_t *scene = obs_scene_from_source(src);
		if (scene) {
			OBSSceneItem item = get_sceneitem_by_pointer_address_internal(scene, (FindSceneitemHelper *)helper);
			if (item) {
				return false;
			}
		}
		return true;
	};

	FindSceneitemHelper helper;
	helper.address = sceneitemAddress;
	helper.output = NULL;

	obs_enum_scenes(cb, &helper);

	return helper.output;
}

FRONTEND_API void pls_get_all_source(std::vector<OBSSource> &vecSources)
{
	std::vector<OBSSource> *_vec = new std::vector<OBSSource>{};
	auto enumSource = [](void *param, obs_source_t *source) {
		std::vector<OBSSource> *_data = static_cast<std::vector<OBSSource> *>(param);
		if (_data == nullptr) {
			return false;
		}
		_data->push_back(source);
		return true;
	};
	obs_enum_sources(enumSource, _vec);

	vecSources.assign(_vec->begin(), _vec->end());
	delete _vec;
	_vec = nullptr;
}

static bool is_string_empty(const char *s)
{
	return !s || !s[0];
}

FRONTEND_API void pls_get_all_source(std::vector<OBSSource> &sources, const char *source_id, const char *name, std::function<bool(const char *value)> value)
{
	struct Context {
		std::vector<OBSSource> &sources;
		const char *source_id;
		const char *name;
		std::function<bool(const char *value)> value;
		bool source_id_is_empty;
		bool name_is_empty;
		bool value_is_empty;
	} context = {sources, source_id, name, value, is_string_empty(source_id), is_string_empty(name), !value};

	auto enumSource = [](void *param, obs_source_t *source) {
		Context *context = static_cast<Context *>(param);

		if (context->source_id_is_empty) {
			context->sources.push_back(source);
			return true;
		}

		const char *id = obs_source_get_id(source);
		if (is_string_empty(id) || strcmp(id, context->source_id)) {
			return true;
		}

		if (context->name_is_empty || context->value_is_empty) {
			context->sources.push_back(source);
			return true;
		}

		obs_data_t *settings = obs_source_get_settings(source);
		const char *_value = obs_data_get_string(settings, context->name);
		bool _value_is_empty = is_string_empty(_value);
		obs_data_release(settings);

		if (context->value(!_value_is_empty ? _value : "")) {
			context->sources.push_back(source);
			return true;
		}
		return true;
	};
	obs_enum_sources(enumSource, &context);
}

FRONTEND_API ITextMotionTemplateHelper *pls_get_text_motion_template_helper_instance()
{
	if (callbacks_valid()) {
		return fc->pls_get_text_motion_template_helper_instance();
	}
	return nullptr;
}

FRONTEND_API QString pls_get_current_language()
{
	if (callbacks_valid()) {
		return fc->pls_get_current_language();
	}
	return QString();
}

FRONTEND_API QLocale::Language pls_get_current_language_enum()
{
	QLocale locale(pls_get_current_language());
	return locale.language();
}

FRONTEND_API QString pls_get_current_language_short_str()
{
	return pls_get_current_language().section(QRegExp("\\W+"), 0, 0);
}

FRONTEND_API QString pls_get_current_country_short_str()
{
	return pls_get_current_language().section(QRegExp("\\W+"), 1, 1);
}

FRONTEND_API bool pls_is_match_current_language(QLocale::Language xlanguage)
{
	QLocale locale(pls_get_current_language());
	return (xlanguage == locale.language());
}

FRONTEND_API QString pls_get_current_accept_language()
{
	QString common = "%1;q=0.9, en;q=0.8, ko;q=0.7, *;q=0.5";
	if (pls_is_match_current_language(QLocale::English)) {
		common.remove("en;q=0.8,");
	} else if (pls_is_match_current_language(QLocale::Korean)) {
		common.remove("ko;q=0.7,");
	}
	common = common.arg(pls_get_current_language_short_str());
	return common;
}

FRONTEND_API bool pls_is_match_current_language(const QString &lang)
{
	if (lang.count() <= 3) {
		return !QString::compare(pls_get_current_language_short_str(), lang, Qt::CaseInsensitive);
	} else {
		return !QString::compare(pls_get_current_language(), lang, Qt::CaseInsensitive);
	}
}

FRONTEND_API int pls_get_actived_chat_channel_count()
{
	if (callbacks_valid()) {
		return fc->pls_get_actived_chat_channel_count();
	}
	return 0;
}

FRONTEND_API int pls_get_prism_live_seq()
{
	if (callbacks_valid()) {
		return fc->pls_get_prism_live_seq();
	}
	return 0;
}

FRONTEND_API QByteArray pls_get_prism_cookie_value()
{
	QList<QByteArray> cookies = pls_get_prism_cookie().split(';');
	for (int i = 0, count = cookies.count(); i < count; ++i) {
		QByteArray cookie = cookies[i].trimmed();
		if (!cookie.startsWith("NEO_SES")) {
			continue;
		}

		int index = cookie.indexOf('=');
		if (index >= 0) {
			return cookie.mid(index + 1).trimmed();
		}
		return QByteArray();
	}
	return QByteArray();
}

FRONTEND_API bool pls_is_create_souce_in_loading()
{
	if (callbacks_valid()) {
		return fc->pls_is_create_souce_in_loading();
	}
	return false;
}

FRONTEND_API void pls_network_state_monitor(std::function<void(bool)> &&callback)
{
	if (callbacks_valid()) {
		fc->pls_network_state_monitor(std::forward<std::function<void(bool)>>(callback));
	}
}

FRONTEND_API bool pls_get_network_state()
{
	if (callbacks_valid()) {
		return fc->pls_get_network_state();
	}
	return true;
}

FRONTEND_API void pls_show_virtual_background()
{
	if (callbacks_valid()) {
		fc->pls_show_virtual_background();
	}
}

FRONTEND_API QWidget *pls_create_virtual_background_resource_widget(QWidget *parent, std::function<void(QWidget *)> &&init, bool forProperty, const QString &itemId, bool checkBoxState,
								    bool switchToPrismFirst)
{
	if (callbacks_valid()) {
		return fc->pls_create_virtual_background_resource_widget(parent, std::forward<std::function<void(QWidget *)>>(init), forProperty, itemId, checkBoxState, switchToPrismFirst);
	}
	return nullptr;
}

FRONTEND_API bool pls_get_media_size(QSize &size, const char *path)
{
	media_info_t mi;
	memset(&mi, 0, sizeof(media_info_t));
	if (mi_open(&mi, path, MI_OPEN_DIRECTLY)) {
		size.setWidth(mi_get_int(&mi, "width"));
		size.setHeight(mi_get_int(&mi, "height"));
		mi_free(&mi);
		return true;
	}
	return false;
}

FRONTEND_API QPixmap pls_load_svg(const QString &path, const QSize &size)
{
	if (callbacks_valid()) {
		return fc->pls_load_svg(path, size);
	}

	return QPixmap();
}

FRONTEND_API void pls_show_mobile_source_help()
{
	if (callbacks_valid()) {
		return fc->pls_show_mobile_source_help();
	}
}

FRONTEND_API void pls_set_side_window_visible(int key, bool visible)
{
	if (callbacks_valid()) {
		return fc->pls_set_side_window_visible(key, visible);
	}
}

FRONTEND_API void pls_mixer_mute_all(bool mute)
{
	if (callbacks_valid()) {
		return fc->pls_mixer_mute_all(mute);
	}
}

FRONTEND_API bool pls_mixer_is_all_mute()
{
	if (callbacks_valid()) {
		return fc->pls_mixer_is_all_mute();
	}
	return false;
}

FRONTEND_API QString pls_get_login_state()
{
	if (callbacks_valid()) {
		return fc->pls_get_login_state();
	}
	return "";
}

FRONTEND_API QString pls_get_stream_state()
{
	if (callbacks_valid()) {
		return fc->pls_get_stream_state();
	}
	return "";
}

FRONTEND_API QString pls_get_record_state()
{
	if (callbacks_valid()) {
		return fc->pls_get_record_state();
	}
	return "";
}

FRONTEND_API bool pls_get_live_record_available()
{
	if (callbacks_valid()) {
		return fc->pls_get_live_record_available();
	}
	return false;
}

FRONTEND_API QList<SideWindowInfo> pls_get_side_windows_info()
{
	if (callbacks_valid()) {
		return fc->pls_get_side_windows_info();
	}
	return {};
}

const char *ConfigKey(ConfigId id)
{
	return QMetaEnum::fromType<ConfigId>().valueToKey(id);
}

FRONTEND_API int pls_get_toast_message_count()
{
	if (callbacks_valid()) {
		return fc->pls_get_toast_message_count();
	}
	return 0;
}

FRONTEND_API void pls_config_set_string(config_t *config, ConfigId id, const char *name, const char *value)
{
	config_set_string(config, ConfigKey(id), name, value);
}
FRONTEND_API void pls_config_set_int(config_t *config, ConfigId id, const char *name, int64_t value)
{
	config_set_int(config, ConfigKey(id), name, value);
}
FRONTEND_API void pls_config_set_uint(config_t *config, ConfigId id, const char *name, uint64_t value)
{
	config_set_uint(config, ConfigKey(id), name, value);
}
FRONTEND_API void pls_config_set_bool(config_t *config, ConfigId id, const char *name, bool value)
{
	config_set_bool(config, ConfigKey(id), name, value);
}
FRONTEND_API void pls_config_set_double(config_t *config, ConfigId id, const char *name, double value)
{
	config_set_double(config, ConfigKey(id), name, value);
}

FRONTEND_API const char *pls_config_get_string(config_t *config, ConfigId id, const char *name)
{
	return config_get_string(config, ConfigKey(id), name);
}
FRONTEND_API int64_t pls_config_get_int(config_t *config, ConfigId id, const char *name)
{
	return config_get_int(config, ConfigKey(id), name);
}
FRONTEND_API uint64_t pls_config_get_uint(config_t *config, ConfigId id, const char *name)
{
	return config_get_uint(config, ConfigKey(id), name);
}
FRONTEND_API bool pls_config_get_bool(config_t *config, ConfigId id, const char *name)
{
	return config_get_bool(config, ConfigKey(id), name);
}
FRONTEND_API double pls_config_get_double(config_t *config, ConfigId id, const char *name)
{
	return config_get_double(config, ConfigKey(id), name);
}

FRONTEND_API bool pls_config_remove_value(config_t *config, ConfigId id, const char *name)
{
	return config_remove_value(config, ConfigKey(id), name);
}

FRONTEND_API pls_blacklist_type pls_is_gpop_blacklist(QString value, pls_blacklist_type type)
{
	if (callbacks_valid()) {
		return fc->pls_is_blacklist(value, type);
	}
	return pls_blacklist_type::None;
}

FRONTEND_API void pls_alert_third_party_plugins(QString pluginName, QWidget *parent)
{
	if (callbacks_valid()) {
		return fc->pls_alert_third_party_plugins(pluginName, parent);
	}
}

FRONTEND_API bool pls_is_dev_server()
{
	static std::optional<bool> devServer;

	if (!devServer) {
		QSettings setting("NAVER Corporation", "Prism Live Studio");
		devServer = setting.value("DevServer", false).toBool();
	}

	return devServer.value();
}

FRONTEND_API QString pls_get_navershopping_deviceId()
{
	QSettings setting("NaverShopping", "Info");
	QString uuid = setting.value("DeviceId", "").toString();
	if (uuid.isEmpty()) {
		uuid = QUuid::createUuid().toString();
		uuid.remove("{").remove("}").remove("-");
		setting.setValue("DeviceId", uuid);
		setting.sync();
	}
	return uuid;
}

FRONTEND_API bool pls_run_http_server(const char *path, QString &addr, std::function<void(const QString &, const QJsonObject &)> callback)
{
	/*auto proc = new QProcess();

	proc->start("httpserver_ver252.exe", {path});
	if (!proc->waitForStarted(1000)) {
		PLS_WARN(MAIN_FRONTEND_API, "HttpServer: failed to start");
		return false;
	}

	proc->waitForReadyRead(1000);
	addr = proc->readLine().trimmed();
	if (addr.isEmpty()) {
		PLS_WARN(MAIN_FRONTEND_API, "HttpServer: failed to get addr");
		return false;
	}

	QObject::connect(proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), proc, [=] {
		callback(addr, proc->readAllStandardOutput());
		delete proc;
	});*/

	auto tcpServer = new QTcpServer;
	if (!tcpServer->listen(QHostAddress::LocalHost)) {
		return false;
	}
	addr = QString("%1:%2").arg(tcpServer->serverAddress().toString()).arg(tcpServer->serverPort());

	QObject::connect(tcpServer, &QTcpServer::acceptError, &QObject::deleteLater);
	QObject::connect(tcpServer, &QTcpServer::newConnection, [=, path = QString(path)] {
		auto tcpClient = tcpServer->nextPendingConnection();
		if (nullptr == tcpClient) {
			return;
		}

		QObject::connect(tcpClient, &QTcpSocket::readyRead, tcpServer, [=] {
			for (;;) {
				auto data = tcpClient->readLine();
				if (data.isEmpty()) {
					break;
				}

				auto line = QString(data.trimmed());
				if (line.isEmpty() || !(line.startsWith("POST") || line.startsWith("GET"))) {
					continue;
				}

				auto urls = line.split(" ");
				if (urls.length() != 3) {
					break;
				}

				if (!urls[1].startsWith(path)) {
					continue;
				}

				tcpClient->write("HTTP/1.1 200 OK\n");
				tcpClient->write("Content-Type: text/html; charset=UTF-8\n");
				tcpClient->write("\n");

				QJsonObject root;
				QUrl url(urls[1]);
				QUrlQuery query(url.query());
				auto code = query.queryItemValue("code", QUrl::FullyDecoded);
				if (code.isEmpty()) {
					root.insert("message", "code is empty");

					static QByteArray bodyFailed;
					if (bodyFailed.isEmpty()) {
						QFile fileFailed(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("loginFail.html"));
						if (fileFailed.open(QIODevice::ReadOnly)) {
							bodyFailed = fileFailed.readAll();
							fileFailed.close();
						}
					}
					tcpClient->write(bodyFailed.replace("$(lang)", pls_get_current_language_short_str().toUtf8()));
				} else {
					root.insert("code", code);
					root.insert("path", path);
					root.insert("scope", query.queryItemValue("scope", QUrl::FullyDecoded));

					static QByteArray bodyOk;
					if (bodyOk.isEmpty()) {
						QFile fileOk(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("login.html"));
						if (fileOk.open(QIODevice::ReadOnly)) {
							bodyOk = fileOk.readAll();
							fileOk.close();
						}
					}

					tcpClient->write(bodyOk.replace("$(lang)", pls_get_current_language_short_str().toUtf8()));
				}

				callback(addr, root);

				tcpClient->flush();
				tcpClient->close();
				tcpClient->deleteLater();

				tcpServer->close();
				tcpServer->deleteLater();

				break;
			}
		});
	});

	return true;
}

FRONTEND_API QDialogButtonBox::StandardButton pls_alert_warning(const char *title, const char *message)
{
	if (callbacks_valid()) {
		return fc->pls_alert_warning(title, message);
	} else {
		return QDialogButtonBox::StandardButton::NoButton;
	}
}

FRONTEND_API bool pls_get_app_exiting()
{
	return g_bMainWindowClosing;
}

FRONTEND_API void pls_set_app_exiting(bool value)
{
	g_bMainWindowClosing = value;
}

FRONTEND_API void pls_singletonWakeup()
{
	if (callbacks_valid()) {
		fc->pls_singletonWakeup();
	}
}

FRONTEND_API bool pls_is_test_mode()
{
	static std::optional<bool> testMode;

	if (!testMode) {
		QSettings setting("NAVER Corporation", "Prism Live Studio");
		testMode = setting.value("TestMode", false).toBool();
	}

	return testMode.value();
}

FRONTEND_API bool pls_is_immersive_audio()
{
	static std::optional<bool> immersiveAudio;

	QSettings setting("NAVER Corporation", "Prism Live Studio");
	immersiveAudio = setting.value("ImmersiveAudio", false).toBool();

	return immersiveAudio.value();
}

FRONTEND_API uint pls_get_live_start_time()
{
	if (callbacks_valid()) {
		return fc->pls_get_live_start_time();
	}
	return 0;
}
QString fill_mask(const int &size)
{
	return QString().fill('*', size);
}
FRONTEND_API QString pls_masking_identify_info(const QString &str)
{
	QString identifyInfo(str);
	return identifyInfo;
}

FRONTEND_API QString pls_masking_datetime_info(const QString &str)
{
	QString dateTimeInfo(str);

	return dateTimeInfo;
}

FRONTEND_API QString pls_masking_passport_info(const QString &str)
{
	QString passportInfo(str);
	return passportInfo;
}

FRONTEND_API QString pls_masking_Region_info(const QString &str)
{
	QString regionInfo(str);
	return regionInfo;
}

FRONTEND_API QString pls_masking_bank_card_info(const QString &str)
{
	QString bandCardInfo(str);
	return bandCardInfo;
}

FRONTEND_API QString pls_masking_name_info(const QString &str)
{
	QString nameInfo(str);
	auto length = str.length();
	if (length < 2) {
		nameInfo = QString("%1").arg(fill_mask(length));
	} else if (2 == length) {
		nameInfo = QString("%1%2").arg(str.left(length - 1)).arg(fill_mask(length - 1));
	} else if (length < 7 && length > 2) {
		nameInfo = QString("%1%2%3").arg(str.left(1)).arg(fill_mask(length - 2)).arg(str.right(1));

	} else {
		nameInfo = QString("%1%2%3").arg(str.left(2)).arg(fill_mask(length - 4)).arg(str.right(2));
	}
	return nameInfo;
}

FRONTEND_API QString pls_masking_ip_info(const QString &str)
{
	QString ipInfo(str);
	QStringList ipLists;
	if (3 == str.count('.')) {
		ipLists = str.split('.');
		ipInfo = QString("%1.%2.***.***").arg(ipLists[0]).arg(ipLists[1]);
	} else if (7 == str.count(':')) {
		ipLists = str.split(':');
		ipInfo = QString("%1:%2:%3:%4:****:****:****:****").arg(ipLists[0]).arg(ipLists[1]).arg(ipLists[3]).arg(ipLists[4]);
	}
	return ipInfo;
}

FRONTEND_API QString pls_masking_address_info(const QString &str)
{
	QString addressInfo(str);
	return addressInfo;
}

FRONTEND_API QString pls_masking_email_info(const QString &str)
{
	QString emailInfo(str);
	if (str.contains('@')) {
		auto emailList = str.split('@');
		QString idDomain;
		for (auto index = 0; index < emailList.size() - 1; ++index) {
			idDomain += emailList.at(index) + '@';
		}
		idDomain.remove(idDomain.length() - 1);
		auto length = idDomain.length();
		emailInfo = length <= 2 ? QString("%1@%2").arg(fill_mask(length)).arg(emailList.last()) : QString("%1%2@%3").arg(idDomain.left(2)).arg(fill_mask(length - 2)).arg(emailList.last());
	}
	return emailInfo;
}

FRONTEND_API QString pls_masking_user_id_info(const QString &str)
{
	QString userIdInfo(str);
	int length = str.length();
	if (length > 2) {
		userIdInfo = length <= 6 ? QString("%1%2").arg(str.left(2)).arg(fill_mask(4)) : QString("%1%2").arg(str.left(4)).arg(fill_mask(length - 4));
	} else {
		userIdInfo = QString("%1").arg(fill_mask(6));
	}
	return userIdInfo;
}

FRONTEND_API QString pls_masking_person_info(const QString &str)
{
	auto personInfo(str);
	if (!personInfo.isEmpty()) {
		int length = str.length();
		int needmaskIndex = length * 0.4f;
		personInfo = QString("%1%2").arg(personInfo.left(needmaskIndex)).arg(fill_mask(length - needmaskIndex));
	}
	return personInfo;
}

FRONTEND_API QString pls_masking_int_info(const qint64 &intData)
{
	auto intInfo = QString::number(intData);
	return pls_masking_person_info(intInfo);
}

FRONTEND_API QString pls_masking_double_info(const double &douData)
{
	auto doubleInfo = QString::number(douData, 'f');
	return pls_masking_person_info(doubleInfo);
}

FRONTEND_API void pls_push_dialog_view(QDialog *dialog)
{
	g_dialog_views.append(dialog);
}

FRONTEND_API void pls_pop_dialog_view(QDialog *dialog)
{
	g_dialog_views.removeAll(dialog);
}

FRONTEND_API void pls_notify_close_dialog_views()
{
	while (!g_dialog_views.isEmpty()) {
		if (auto dialog_view = g_dialog_views.takeLast(); dialog_view) {
			if (auto adapter = dynamic_cast<PLSWidgetDpiAdapter *>(dialog_view.data()); adapter) {
				adapter->closeNoButton();
			}
		}
	}
}
