#include "frontend-internal.hpp"
#include <optional>
#include <QUrl>
#include <QStyle>
#include <QDebug>
#include <QMetaEnum>
#include "login-info.hpp"
#include <QScreen>
#include <QGuiApplication>
#include "pls-net-url.hpp"
#include <util/windows/win-version.h>
#include <QNetworkInterface>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <WinDNS.h>
#include <util/windows/WinHandle.hpp>
#include <util/windows/win-version.h>
#else
#endif

#include <qdir.h>
#include "utils-api.h"
#include "pls-common-define.hpp"
#include <qnetworkcookie.h>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QHostInfo>
#include <atomic>
#include <quuid.h>
#include <QPointer>
#include <QDialog>
#include <set>
#include <QMenu>
#include <QDialogButtonBox>
#include <qcryptographichash.h>
#include <array>
#include <string>
#include <qsettings.h>
#include <qapplication.h>
#include <network-state.h>
#include <prism-version.h>
#include <util/platform.h>
#include <util/config-file.h>

#include <liblog.h>
#include <PLSAlertView.h>
#include <libutils-api.h>

#if __APPLE__
#include "mac/DNSSDRegisterManager.hpp"
#endif

#include "PLSToplevelView.h"

using namespace common;
constexpr auto VERSION_COMPARE_COUNT = 3;
#define HEADER_USER_AGENT_KEY QStringLiteral("User-Agent")
#define HEADER_PRISM_LANGUAGE QStringLiteral("Accept-Language")
#define HEADER_PRISM_GCC QStringLiteral("X-prism-cc")
#define HEADER_PRISM_USERCODE QStringLiteral("X-prism-usercode")
#define HEADER_PRISM_OS QStringLiteral("X-prism-os")
#define HEADER_PRISM_IP QStringLiteral("X-prism-ip")
#define HEADER_PRISM_DEVICE QStringLiteral("X-prism-device")
#define HEADER_PRISM_APPVERSION QStringLiteral("X-prism-appversion")
#define MAIN_FRONTEND_API "main/frontend-api"

constexpr auto HTTP_CLIENT_MODULE = "Http Request";

struct MDNSInfo {
	std::wstring strInstance;
	std::wstring strHostname;
	unsigned short wPort;

	std::wstring strId;
	std::wstring strName;
	std::wstring strDeviceType;
	std::wstring strConnectType;
	std::wstring strVersion;

	bool bRegister;
};

namespace {
struct LocalGlobalVars {
	static pls_frontend_callbacks *fc;
	static pls_translate_callback_t s_translate_cb;
	static PfnGetConfigPath getConfigPath;
	static ControlSrcType broadcastControl;
	static ControlSrcType recordControl;
	static std::recursive_mutex mutexMDNSInfo;
	static std::list<MDNSInfo> lstMDNSInfo;
	static pls_load_ndi_runtime_pfn loadNdiRuntime;
};

pls_frontend_callbacks *LocalGlobalVars::fc = nullptr;
pls_translate_callback_t LocalGlobalVars::s_translate_cb = nullptr;
PfnGetConfigPath LocalGlobalVars::getConfigPath = nullptr;

ControlSrcType LocalGlobalVars::broadcastControl = ControlSrcType::None;
ControlSrcType LocalGlobalVars::recordControl = ControlSrcType::None;

std::recursive_mutex LocalGlobalVars::mutexMDNSInfo;
std::list<MDNSInfo> LocalGlobalVars::lstMDNSInfo;

pls_load_ndi_runtime_pfn LocalGlobalVars::loadNdiRuntime = nullptr;
}

static QString getOsVersion(const win_version_info &wvi)
{
	return QString("Windows %1.%2.%3.%4").arg(wvi.major).arg(wvi.minor).arg(wvi.build).arg(wvi.revis);
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
	pls_network_state_monitor([callback](bool state) { callback(state); });
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
	LocalGlobalVars::fc = callbacks;
}

static inline bool callbacks_valid_(const char *func_name)
{
	if (!LocalGlobalVars::fc) {
		PLS_WARN(MAIN_FRONTEND_API, "Tried to call %s with no callbacks!", func_name);
		return false;
	}

	return true;
}

#define callbacks_valid() callbacks_valid_(__FUNCTION__)

FRONTEND_API void pls_set_translate_cb(pls_translate_callback_t translate_cb)
{
	LocalGlobalVars::s_translate_cb = translate_cb;
}
FRONTEND_API const char *pls_translate(const char *lookup)
{
	if (LocalGlobalVars::s_translate_cb) {
		return LocalGlobalVars::s_translate_cb(lookup);
	}
	return "";
}
FRONTEND_API QString pls_translate_qstr(const char *lookup)
{
	return QString::fromUtf8(pls_translate(lookup));
}

FRONTEND_API bool pls_register_login_info(PLSLoginInfo *login_info)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_register_login_info(login_info);
	}
	return false;
}

FRONTEND_API void pls_unregister_login_info(PLSLoginInfo *login_info)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_unregister_login_info(login_info);
	}
}

FRONTEND_API int pls_get_login_info_count()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_login_info_count();
	}
	return 0;
}

FRONTEND_API PLSLoginInfo *pls_get_login_info(int index)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_login_info(index);
	}
	return nullptr;
}

FRONTEND_API void del_pannel_cookies(const QString &pannelName)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->del_pannel_cookies(pannelName);
	}
}

FRONTEND_API void pls_set_manual_cookies(const QString &pannelName)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_set_manual_cookies(pannelName);
	}
}

FRONTEND_API void pls_delete_specific_url_cookie(const QString &url, const QString &cookieName)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_del_specific_url_cookie(url, cookieName);
	}
}

FRONTEND_API QJsonObject pls_ssmap_to_json(const QMap<QString, QString> &ssmap)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_ssmap_to_json(ssmap);
	}
	return QJsonObject();
}

FRONTEND_API bool pls_browser_view(QJsonObject &result, const QUrl &url, const pls_result_checking_callback_t &callback, QWidget *parent, bool readCookies)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_browser_view(result, url, callback, parent, readCookies);
	}
	return false;
}
FRONTEND_API bool pls_browser_view(QJsonObject &result, const QUrl &url, const std_map<std::string, std::string> &headers, const QString &pannelName, const pls_result_checking_callback_t &callback,
				   QWidget *parent, bool readCookies)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_browser_view(result, url, headers, pannelName, std::string(), callback, parent, readCookies);
	}
	return false;
}

FRONTEND_API bool pls_browser_view(QJsonObject &result, const QUrl &url, const std_map<std::string, std::string> &headers, const QString &pannelName, const std::string &script,
				   const pls_result_checking_callback_t &callback, QWidget *parent, bool readCookies)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_browser_view(result, url, headers, pannelName, script, callback, parent, readCookies);
	}
	return false;
}

FRONTEND_API bool pls_rtmp_view(QJsonObject &result, PLSLoginInfo *login_info, QWidget *parent)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_rtmp_view(result, login_info, parent);
	}
	return false;
}

FRONTEND_API bool pls_channel_login(QJsonObject &result, QWidget *parent)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_channel_login(result, parent);
	}
	return false;
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

FRONTEND_API QString pls_prism_user_thumbnail_path()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_prism_user_thumbnail_path();
	}
	return QString();
}
FRONTEND_API void pls_prism_change_over_login_view()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_prism_change_over_login_view();
	}
}

FRONTEND_API void pls_login_async(const std::function<void(bool ok, const QJsonObject &result)> &callback, const QString &platformName, QWidget *parent, PLSLoginInfo::UseFor useFor)
{
	if (!pls_get_network_state()) {
		PLSAlertView::warning(parent, pls_translate_qstr("Alert.Title"), pls_translate_qstr("login.check.note.network"));
		return;
	}

	for (int index = 0; index != pls_get_login_info_count(); ++index) {
		const PLSLoginInfo *channelInfo = pls_get_login_info(index);
		if (!channelInfo)
			continue;
		if (platformName.compare(channelInfo->name(), Qt::CaseInsensitive))
			continue;

		if (channelInfo->loginWithAccountImplementType() == PLSLoginInfo::ImplementType::Synchronous) {
			QJsonObject result;
			bool ok = channelInfo->loginWithAccount(result, useFor, parent);
			callback(ok, result);
		} else {
			channelInfo->loginWithAccountAsync(callback, useFor, parent);
		}
	}
}

FRONTEND_API void pls_channel_login_async(const std::function<void(bool ok, const QJsonObject &result)> &callback, const QString &accountName, QWidget *parent)
{
	pls_login_async(callback, accountName, parent, PLSLoginInfo::UseFor::Channel);
}

FRONTEND_API void pls_prism_logout()
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_prism_logout(PLS_LOGOUT_URL.arg(PRISM_SSL));
	}
}

FRONTEND_API void pls_prism_signout()
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_prism_logout(PLS_SIGNOUT_URL.arg(PRISM_SSL));
	}
}

FRONTEND_API Common pls_get_gpop_common()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_common();
	}
	return Common();
}

FRONTEND_API QMap<QString, SnsCallbackUrl> pls_get_gpop_snscallback_urls()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_snscallback_urls();
	}
	return QMap<QString, SnsCallbackUrl>();
}

FRONTEND_API Connection pls_get_gpop_connection()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_connection();
	}
	return Connection();
}

FRONTEND_API QMap<int, RtmpDestination> pls_get_rtmpDestination()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_rtmpDestination();
	}
	return QMap<int, RtmpDestination>();
}

FRONTEND_API QString pls_get_gcc_data()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_gcc_data();
	}
	return QString();
}

FRONTEND_API QString pls_get_prism_token()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_prism_token();
	}
	return QString();
}

FRONTEND_API QString pls_get_prism_email()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_prism_email();
	}
	return QString();
}

FRONTEND_API QString pls_get_prism_thmbanilurl()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_prism_thmbanilurl();
	}
	return QString();
}

FRONTEND_API QString pls_get_prism_nickname()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_prism_nickname();
	}
	return QString();
}

FRONTEND_API QString pls_get_prism_usercode()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_prism_usercode();
	}
	return QString();
}

FRONTEND_API QByteArray pls_get_prism_cookie()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_prism_cookie();
	}
	return nullptr;
}

FRONTEND_API QJsonObject pls_get_resource_statistics_data()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_resource_statistics_data();
	}
	return QJsonObject();
}

FRONTEND_API bool pls_click_alert_message()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_click_alert_message();
	}
	return false;
}

FRONTEND_API bool pls_alert_message_visible()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_alert_message_visible();
	}
	return false;
}

FRONTEND_API int pls_alert_message_count()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_alert_message_count();
	}
	return 0;
}

FRONTEND_API QList<std::tuple<QString, QString>> pls_get_user_active_channles_info()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_user_active_channles_info();
	}
	return QList<std::tuple<QString, QString>>();
}

FRONTEND_API bool pls_is_rehearsal_info_display()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_is_rehearsal_info_display();
	}
	return false;
}

FRONTEND_API void pls_frontend_add_event_callback(pls_frontend_event_cb callback, void *context)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_frontend_add_event_callback(callback, context);
	}
}
FRONTEND_API void pls_frontend_add_event_callback(pls_frontend_event event, pls_frontend_event_cb callback, void *context)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_frontend_add_event_callback(event, callback, context);
	}
}
FRONTEND_API void pls_frontend_add_event_callback(QList<pls_frontend_event> events, pls_frontend_event_cb callback, void *context)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_frontend_add_event_callback(events, callback, context);
	}
}
FRONTEND_API void pls_frontend_remove_event_callback(pls_frontend_event_cb callback, void *context)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_frontend_remove_event_callback(callback, context);
	}
}
FRONTEND_API void pls_frontend_remove_event_callback(pls_frontend_event event, pls_frontend_event_cb callback, void *context)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_frontend_remove_event_callback(event, callback, context);
	}
}
FRONTEND_API void pls_frontend_remove_event_callback(QList<pls_frontend_event> events, pls_frontend_event_cb callback, void *context)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_frontend_remove_event_callback(events, callback, context);
	}
}

FRONTEND_API QString pls_get_theme_dir_path()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_theme_dir_path();
	}
	return QString();
}

FRONTEND_API QString pls_get_color_filter_dir_path()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_color_filter_dir_path();
	}
	return QString();
}

FRONTEND_API QString pls_get_user_path(const QString &path)
{
	if (LocalGlobalVars::getConfigPath) {
		std::array<char, 512> configPath;
		LocalGlobalVars::getConfigPath(configPath.data(), sizeof(configPath), path.toUtf8());
		return QString(configPath.data());
	}
	return QString();
}
FRONTEND_API void pls_toast_message(pls_toast_info_type type, const QString &message, int auto_close)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_toast_message(type, message, auto_close);
	}
}

FRONTEND_API void pls_toast_message(pls_toast_info_type type, const QString &message, const QString &url, const QString &replaceStr, int auto_close)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_toast_message(type, message, url, replaceStr, auto_close);
	}
}

FRONTEND_API void pls_toast_clear()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_toast_clear();
	}
}
FRONTEND_API void pls_set_main_view_side_bar_user_button_icon(const QIcon &icon)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_set_main_view_side_bar_user_button_icon(icon);
	}
}

FRONTEND_API void pls_unload_chat_dock(const QString &objectName)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_unload_chat_dock(objectName);
	}
}

FRONTEND_API bool pls_get_prism_user_thumbnail(QPixmap &pixMap)
{
	if (callbacks_valid()) {
		QFile file(pls_prism_user_thumbnail_path());
		if (!file.open(QIODevice::ReadOnly))
			return false;
		auto data = file.readAll();
		file.close();
		return pixMap.loadFromData(QByteArray::fromBase64(data), "PNG");
	}
	return false;
}

FRONTEND_API const char *pls_basic_config_get_string(const char *section, const char *name, const char *defaultValue)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_basic_config_get_string(section, name, defaultValue);
	}
	return defaultValue;
}

FRONTEND_API int64_t pls_basic_config_get_int(const char *section, const char *name, int64_t defaultValue)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_basic_config_get_int(section, name, defaultValue);
	}
	return defaultValue;
}

FRONTEND_API uint64_t pls_basic_config_get_uint(const char *section, const char *name, uint64_t defaultValue)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_basic_config_get_uint(section, name, defaultValue);
	}
	return defaultValue;
}

FRONTEND_API bool pls_basic_config_get_bool(const char *section, const char *name, bool defaultValue)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_basic_config_get_bool(section, name, defaultValue);
	}
	return defaultValue;
}

FRONTEND_API double pls_basic_config_get_double(const char *section, const char *name, double defaultValue)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_basic_config_get_double(section, name, defaultValue);
	}
	return defaultValue;
}

FRONTEND_API bool pls_inside_visible_screen_area(QRect geometry)
{
	for (const QScreen *screen : QApplication::screens()) {
		QRect rect = screen->availableGeometry();
		if (rect.intersects(geometry)) {
			return true;
		}
	}
	return false;
}

FRONTEND_API pls_check_update_result_t pls_check_app_update(bool &is_force, QString &version, QString &file_url, QString &update_info_url)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_check_app_update(is_force, version, file_url, update_info_url);
	}
	return pls_check_update_result_t::Failed;
}

FRONTEND_API pls_upload_file_result_t pls_upload_contactus_files(const QString &email, const QString &question, const QList<QFileInfo> files)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_upload_contactus_files(email, question, files);
	}
	return pls_upload_file_result_t::Ok;
}

FRONTEND_API bool pls_show_update_info_view(bool is_force, const QString &version, const QString &file_url, const QString &update_info_url, bool is_manual, QWidget *parent)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_show_update_info_view(is_force, version, file_url, update_info_url, is_manual, parent);
	}
	return false;
}

FRONTEND_API void pls_get_new_notice_Info(const std::function<void(const QVariantMap &noticeInfo)> &noticeCallback)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_new_notice_Info(noticeCallback);
	}
}
FRONTEND_API QString pls_get_win_os_version()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_win_os_version();
	}
	return QString();
}

FRONTEND_API bool pls_is_living_or_recording()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_is_living_or_recording();
	}
	return false;
}

FRONTEND_API bool pls_is_output_actived()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_is_output_actived();
	}
	return false;
}

// add tools menu seperator
FRONTEND_API void pls_add_tools_menu_seperator()
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_add_tools_menu_seperator();
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

FRONTEND_API bool pls_is_rehearsaling()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_is_rehearsaling();
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

	for (const QScreen *screen : QGuiApplication::screens()) {
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
	LOAD_DEV_SERVER(PLS_LAB);

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
	LOAD_DEV_SERVER(LABORATORY_REMOTECHAT_ID);
	LOAD_DEV_SERVER(LIBRARY_SENSETIME_PC_ID);

	LOAD_DEV_SERVER(MQTT_SERVER);
	LOAD_DEV_SERVER(MQTT_SERVER_PW);
	LOAD_DEV_SERVER(MQTT_SERVER_WEB);

	LOAD_DEV_SERVER(APP_INIT_URL);
	LOAD_DEV_SERVER(APP_UPDATE_URL);
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
	LOAD_DEV_SERVER(CHANNEL_BAND_LIVE_STATUS);

	//Facebook
	LOAD_DEV_SERVER(CHANNEL_FACEBOOK_CLIENT_ID);
	LOAD_DEV_SERVER(CHANNEL_FACEBOOK_SECRET);

	// Chat Widget
	LOAD_DEV_SERVER(CHAT_SOURCE_URL);

	//Naver Shopping Live
	LOAD_DEV_SERVER(CHANNEL_NAVER_SHOPPING_LIVE_LOGIN);
	LOAD_DEV_SERVER(CHANNEL_NAVER_SHOPPING_LIVE_SMART_STORE_LOGIN);
	LOAD_DEV_SERVER(CHANNEL_NAVER_SHOPPING_LIVE_NAVER_LOGIN);
	LOAD_DEV_SERVER(CHANNEL_NAVER_SHOPPING_LIVE_HEADER);
}

FRONTEND_API PfnGetConfigPath pls_get_config_path(void)
{
	return LocalGlobalVars::getConfigPath;
}

FRONTEND_API void pls_set_config_path(PfnGetConfigPath getConfigPath)
{
	LocalGlobalVars::getConfigPath = getConfigPath;
}

FRONTEND_API QString pls_get_config_dir(PfnGetConfigPath getConfigPath, const char *name)
{
	std::array<char, 512> path;
	if (getConfigPath(path.data(), sizeof(path), name) > 0)
		return QString::fromUtf8(path.data());
	return QString();
}

FRONTEND_API QString pls_get_config_dir()
{
	if (LocalGlobalVars::getConfigPath)
		return pls_get_config_dir(LocalGlobalVars::getConfigPath, "");
	return pls_get_config_dir(os_get_config_path, "");
}

FRONTEND_API QString pls_get_relative_config_path(const QString &config_path)
{
	QString normal_config_path = QDir::toNativeSeparators(config_path);
	QString vb_config_dir = QDir::toNativeSeparators(pls_get_config_dir());
	normal_config_path.remove(vb_config_dir);
	if (normal_config_path.startsWith(QDir::separator()))
		return normal_config_path.mid(1);
	return normal_config_path;
}

FRONTEND_API QString pls_get_absolute_config_path(const QString &config_path)
{
	if (QDir::isAbsolutePath(config_path))
		return config_path;
	else if (config_path[0] == '.')
		return QDir::currentPath() + config_path.mid(1);
	return pls_get_config_dir() + '/' + config_path;
}

FRONTEND_API QVariantMap pls_http_request_default_headers(bool hasGcc)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_http_request_head(hasGcc);
	}
	return QVariantMap();
}
FRONTEND_API std::string pls_get_offline_javaScript()
{
#if defined(Q_OS_WIN)
	QString path = pls_get_app_dir() + QString("/../../data/prism-studio/webpage/pls_offline_page.js");
#elif defined(Q_OS_MACOS)
	QString path = pls_get_app_resource_dir() + "/data/prism-studio/webpage/pls_offline_page.js";
#endif
	QFile file(path);
	file.open(QIODevice::ReadOnly | QIODevice::Text);
	QByteArray byteArray = file.readAll();
	file.close();
	QString str = byteArray;
	QString _strNet = pls_translate_qstr("login.check.note.network").replace("\n", "\\n");
	QString _strRetry = pls_translate_qstr("Retry").replace("\n", "\\n");
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

FRONTEND_API ControlSrcType pls_previous_broadcast_control_by()
{
	return LocalGlobalVars::broadcastControl;
}

FRONTEND_API void pls_set_broadcast_control(const ControlSrcType &control)
{
	LocalGlobalVars::broadcastControl = control;
}

FRONTEND_API ControlSrcType pls_previous_record_control_by()
{
	return LocalGlobalVars::recordControl;
}

FRONTEND_API void pls_start_broadcast(bool toStart, const ControlSrcType &control)
{
	if (callbacks_valid()) {
		LocalGlobalVars::broadcastControl = control;
		LocalGlobalVars::fc->pls_start_broadcast(toStart);
	}
}

FRONTEND_API void pls_start_broadcast_in_info(bool toStart)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_start_broadcast_in_info(toStart);
	}
}

FRONTEND_API void pls_start_rehearsal(bool toStart)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_start_rehearsal(toStart);
	}
}

FRONTEND_API void pls_start_record(bool toStart, const ControlSrcType &control)
{
	if (callbacks_valid()) {
		LocalGlobalVars::recordControl = control;
		LocalGlobalVars::fc->pls_start_record(toStart);
	}
}

FRONTEND_API OBSSource pls_get_source_by_name(const char *name)
{
	if (nullptr == name || std::string(name).empty()) {
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

FRONTEND_API OBSData pls_get_source_private_setting(obs_source_t *source)
{
	OBSData sourceSettings = obs_source_get_private_settings(source);
	if (sourceSettings)
		obs_data_release(sourceSettings);
	return sourceSettings;
}

FRONTEND_API OBSSource pls_get_source_by_pointer_address(const void *pointerAddress)
{
	OBSSource source_output = nullptr;

	if (!pointerAddress)
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
	auto enumSource = [](void *param, obs_source_t *source) { return (*(SearchForSource_t *)(param))(source); };
	obs_enum_all_sources(enumSource, &searchForSource);

	return source_output;
}

FRONTEND_API OBSScene pls_get_scene_by_pointer_address(const void *pointerAddress)
{
	OBSScene scene_output = nullptr;

	if (!pointerAddress)
		return scene_output;

	obs_frontend_source_list sourcelist{};
	obs_frontend_get_scenes(&sourcelist);
	for (size_t i = 0; i < sourcelist.sources.num; i++) {
		if (!sourcelist.sources.array[i])
			continue;

		auto scene = obs_scene_from_source(sourcelist.sources.array[i]);
		if (!scene)
			continue;

		if (scene == pointerAddress) {
			scene_output = scene;
			break;
		}
	}
	obs_frontend_source_list_free(&sourcelist);

	return scene_output;
}

struct FindSceneitemHelper {
	void *address;
	OBSSceneItem output;
};

static bool FindSceneItemCb(obs_scene_t *scene, obs_sceneitem_t *item, void *ptr)
{
	pls_unused(scene);
	auto helper = (FindSceneitemHelper *)ptr;

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
	auto _vec = pls_new<std::vector<OBSSource>>();

	auto enumSource = [](void *param, obs_source_t *source) {
		auto _data = static_cast<std::vector<OBSSource> *>(param);
		if (_data == nullptr) {
			return false;
		}
		_data->push_back(source);
		return true;
	};
	obs_enum_sources(enumSource, _vec);

	vecSources.assign(_vec->begin(), _vec->end());
	pls_delete(_vec, nullptr);
}

static const std::set<std::string, std::less<>> supportedRnnoiseSet = {common::AUDIO_INPUT_SOURCE_ID, common::PRISM_MOBILE_SOURCE_ID, common::OBS_DSHOW_SOURCE_ID, common::PRISM_NDI_SOURCE_ID};

FRONTEND_API bool pls_source_support_rnnoise(const char *id)
{
	if (id && *id)
		return supportedRnnoiseSet.find(std::string(id)) != supportedRnnoiseSet.end();
	return false;
}

FRONTEND_API bool pls_get_chat_info(QString &id, QString &cookie, bool &isSinglePlatform)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_chat_info(id, cookie, isSinglePlatform);
	}
	return false;
}

FRONTEND_API int pls_get_current_selected_channel_count()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_current_selected_channel_count();
	}
	return 0;
}

static bool is_string_empty(const char *s)
{
	return !s || !s[0];
}

struct GetAllSourceContext {
	std::vector<OBSSource> &sources;
	const char *source_id;
	const char *name;
	std::function<bool(const char *value)> value;
	bool source_id_is_empty;
	bool name_is_empty;
	bool value_is_empty;
};

static bool pls_get_all_source_enum_callback(void *param, obs_source_t *source)
{
	auto context = static_cast<GetAllSourceContext *>(param);
	pls_unused(source);

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
}

FRONTEND_API void pls_get_all_source(std::vector<OBSSource> &sources, const char *source_id, const char *name, const std::function<bool(const char *value)> &value)
{
	GetAllSourceContext context = {sources, source_id, name, value, is_string_empty(source_id), is_string_empty(name), !value};
	obs_enum_sources(pls_get_all_source_enum_callback, &context);
}

FRONTEND_API ITextMotionTemplateHelper *pls_get_text_motion_template_helper_instance()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_text_motion_template_helper_instance();
	}
	return nullptr;
}

FRONTEND_API QString pls_get_current_language()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_current_language();
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
	return pls_get_current_language().section(QRegularExpression("\\W+"), 0, 0);
}

FRONTEND_API QString pls_get_current_country_short_str()
{
	return pls_get_current_language().section(QRegularExpression("\\W+"), 1, 1);
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
		return LocalGlobalVars::fc->pls_get_actived_chat_channel_count();
	}
	return 0;
}

FRONTEND_API int pls_get_prism_live_seq()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_prism_live_seq();
	}
	return 0;
}

FRONTEND_API QByteArray pls_get_prism_cookie_value()
{
	QList<QByteArray> cookies = pls_get_prism_cookie().split(';');
	for (qsizetype i = 0, count = cookies.count(); i < count; ++i) {
		QByteArray cookie = cookies[i].trimmed();
		if (!cookie.startsWith("NEO_SES")) {
			continue;
		}

		auto index = cookie.indexOf('=');
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
		return LocalGlobalVars::fc->pls_is_create_souce_in_loading();
	}
	return false;
}

FRONTEND_API void pls_network_state_monitor(const std::function<void(bool)> &callback)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_network_state_monitor(callback);
	}
}

FRONTEND_API bool pls_get_network_state()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_network_state();
	}
	return true;
}

FRONTEND_API void pls_show_virtual_background()
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_show_virtual_background();
	}
}

FRONTEND_API QWidget *pls_create_virtual_background_resource_widget(QWidget *parent, const std::function<void(QWidget *)> &init, bool forProperty, const QString &itemId, bool checkBoxState,
								    bool switchToPrismFirst)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_create_virtual_background_resource_widget(parent, init, forProperty, itemId, checkBoxState, switchToPrismFirst);
	}
	return nullptr;
}

FRONTEND_API bool pls_get_media_size(QSize &size, const char *path)
{
#if 0
	media_info_t mi;
	memset(&mi, 0, sizeof(media_info_t));
	if (mi_open(&mi, path, MI_OPEN_DIRECTLY)) {
		size.setWidth((int)mi_get_int(&mi, "width"));
		size.setHeight((int)mi_get_int(&mi, "height"));
		mi_free(&mi);
		return true;
	}
#endif
	return false;
}

FRONTEND_API QPixmap pls_load_svg(const QString &path, const QSize &size)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_load_svg(path, size);
	}

	return QPixmap();
}

FRONTEND_API void pls_set_bgm_visible(bool visible)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_set_bgm_visible(visible);
	}
}

FRONTEND_API bool pls_set_side_window_visible(int key, bool visible)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_set_side_window_visible(key, visible);
	}
	return false;
}

FRONTEND_API void pls_mixer_mute_all(bool mute)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_mixer_mute_all(mute);
	}
}

FRONTEND_API bool pls_mixer_is_all_mute()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_mixer_is_all_mute();
	}
	return false;
}

FRONTEND_API QString pls_get_stream_state()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_stream_state();
	}
	return "";
}

FRONTEND_API QString pls_get_record_state()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_record_state();
	}
	return "";
}

FRONTEND_API bool pls_get_hotkey_enable()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_hotkey_enable();
	}
	return false;
}

FRONTEND_API QList<SideWindowInfo> pls_get_side_windows_info()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_side_windows_info();
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
		return LocalGlobalVars::fc->pls_get_toast_message_count();
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

FRONTEND_API bool pls_is_dev_server()
{
	return pls_prism_is_dev();
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

template<class T> void onWriteClient(const QString &addr, T &&callback, QTcpSocket *tcpClient, const QUrl &url, const QString &path)
{
	tcpClient->write("HTTP/1.1 200 OK\n");
	tcpClient->write("Content-Type: text/html; charset=UTF-8\n");
	tcpClient->write("\n");

	QJsonObject root;
	QUrlQuery query(url.query());
#if defined(Q_OS_WIN)
	QString htmlPath = pls_get_app_dir() + QString("/../../data/prism-studio/webpage/%1");
#elif defined(Q_OS_MACOS)
	QString htmlPath = pls_get_app_resource_dir() + "/data/prism-studio/webpage/%1";
#endif
	if (auto code = query.queryItemValue("code", QUrl::FullyDecoded); code.isEmpty()) {
		root.insert("message", "code is empty");

		static QByteArray bodyFailed;
		if (bodyFailed.isEmpty()) {
			QFile fileFailed(htmlPath.arg("loginFail.html"));
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
			QFile fileOk(htmlPath.arg("login.html"));
			if (fileOk.open(QIODevice::ReadOnly)) {
				bodyOk = fileOk.readAll();
				fileOk.close();
			}
		}

		tcpClient->write(bodyOk.replace("$(lang)", pls_get_current_language_short_str().toUtf8()));
	}

	callback(addr, root);
}

void onReadyRead(const QString &addr, const std::function<void(const QString &, const QJsonObject &)> &callback, QTcpServer *tcpServer, QTcpSocket *tcpClient, const QString &path)
{
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
			break;
		}

		onWriteClient(addr, callback, tcpClient, QUrl(urls[1]), path);

		tcpClient->flush();
		tcpClient->close();
		tcpClient->deleteLater();

		tcpServer->close();
		tcpServer->deleteLater();

		break;
	}
}

FRONTEND_API bool pls_run_http_server(const char *path, QString &addr, const std::function<void(const QString &, const QJsonObject &)> &callback)
{
	auto tcpServer = pls_new<QTcpServer>();
	if (!tcpServer->listen(QHostAddress::LocalHost)) {
		tcpServer->deleteLater();
		return false;
	}
	addr = QString("%1:%2").arg(tcpServer->serverAddress().toString()).arg(tcpServer->serverPort());

	QObject::connect(tcpServer, &QTcpServer::acceptError, &QObject::deleteLater);
	QObject::connect(tcpServer, &QTcpServer::newConnection, [addr, callback, tcpServer, path = QString(path)] {
		auto tcpClient = tcpServer->nextPendingConnection();
		if (nullptr == tcpClient) {
			return;
		}

		QObject::connect(tcpClient, &QTcpSocket::readyRead, tcpServer, [addr, callback, tcpClient, tcpServer, path] {
			if (pls_get_app_exiting()) {
				return;
			}
			onReadyRead(addr, callback, tcpServer, tcpClient, path);
		});
	});

	return true;
}

FRONTEND_API int pls_alert_warning(const char *title, const char *message)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_alert_warning(title, message);
	} else {
		return QDialogButtonBox::StandardButton::NoButton;
	}
}

FRONTEND_API void pls_singleton_wakeup()
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_singleton_wakeup();
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
		return LocalGlobalVars::fc->pls_get_live_start_time();
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
	int length = static_cast<int>(str.length());
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
		idDomain.remove(idDomain.left(idDomain.length() - 1));
		int length = static_cast<int>(idDomain.length());
		emailInfo = length <= 2 ? QString("%1@%2").arg(fill_mask(length)).arg(emailList.last()) : QString("%1%2@%3").arg(idDomain.left(2)).arg(fill_mask(length - 2)).arg(emailList.last());
	}
	return emailInfo;
}

FRONTEND_API QString pls_masking_user_id_info(const QString &str)
{
	QString userIdInfo(str);
	int length = static_cast<int>(str.length());
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
		int length = static_cast<int>(str.length());
		auto needmaskIndex = static_cast<int>(static_cast<float>(length) * 0.4f);
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

FRONTEND_API void pls_laboratory_click_open_button(const QString &laboratoryId, bool targetStatus)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_laboratory_click_open_button(laboratoryId, targetStatus);
	}
}

FRONTEND_API void pls_set_laboratory_status(const QString &laboratoryId, bool on)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_set_laboratory_status(laboratoryId, on);
	}
}

FRONTEND_API void pls_laboratory_detail_page_js_event(const QString &page, const QString &action, const QString &info)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_laboratory_detail_page_js_event(page, action, info);
	}
}

FRONTEND_API bool pls_get_laboratory_status(const QString &laboratoryId)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_laboratory_status(laboratoryId);
	}
	return false;
}

FRONTEND_API void pls_navershopping_get_store_login_url(QWidget *widget, const std::function<void(const QString &storeLoginUrl)> &ok, const std::function<void()> &fail)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_navershopping_get_store_login_url(widget, ok, fail);
	} else {
		fail();
	}
}

FRONTEND_API void pls_enum_sources(const std::function<bool(obs_source_t *)> &callback)
{
	obs_enum_sources([](void *context, obs_source_t *source) { return (*(std::function<bool(obs_source_t *)> *)context)(source); }, (void *)&callback);
}

FRONTEND_API bool pls_copy_file(const QString &fileName, const QString &newName, bool overwrite, bool sendSre)
{
	if (fileName.isEmpty() || newName.isEmpty())
		return false;
#ifdef Q_OS_WIN
	auto ret = CopyFile(fileName.toStdWString().c_str(), newName.toStdWString().c_str(), !overwrite);
	if (!ret) {
		auto errorCode = GetLastError();
		auto name = pls_get_path_file_name(fileName);
		if (sendSre) {
			auto errorCodeStr = std::to_string(errorCode);
			PLS_LOGEX(PLS_LOG_ERROR, MAIN_FRONTEND_API, {{"CopyErrorCode", errorCodeStr.c_str()}, {"FileName", qUtf8Printable(name)}}, "Failed to copy file '%s'. ErrorCode: %lu",
				  name.data(), errorCode);
		} else {
			PLS_WARN(MAIN_FRONTEND_API, "Failed to copy file '%s'. ErrorCode: %lu", qUtf8Printable(name), errorCode);
		}
		return false;
	}
	return true;
#else
	int errorCode = 0;
	bool ret = pls_copy_file_with_error_code(fileName, newName, overwrite, errorCode);
	if (!ret) {
		auto name = pls_get_path_file_name(fileName);
		if (sendSre) {
			auto errorCodeStr = std::to_string(errorCode);
			PLS_LOGEX(PLS_LOG_ERROR, MAIN_FRONTEND_API, {{"CopyErrorCode", errorCodeStr.c_str()}, {"FileName", qUtf8Printable(name)}}, "Failed to copy file '%s'. ErrorCode: %lu",
				  name.data(), errorCode);
		} else {
			PLS_WARN(MAIN_FRONTEND_API, "Failed to copy file '%s'. ErrorCode: %lu", qUtf8Printable(name), errorCode);
		}
		return false;
	}
	return true;
#endif
}

FRONTEND_API void pls_on_frontend_event(pls_frontend_event event, const QVariantList &params)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->on_event(event, params);
	} else {
		assert(false);
	}
}

FRONTEND_API void pls_send_analog(AnalogType logType, const QVariantMap &info)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_send_analog(logType, info);
	}
}

FRONTEND_API QString pls_get_analog_filter_id(const char *id)
{
	if (!id)
		return "unknown";

	if (0 == strcmp(id, FILTER_TYPE_ID_VIDEODELAY_ASYNC))
		return "Video Delay (Async)";
	else if (0 == strcmp(id, FILTER_TYPE_ID_CHROMAKEY))
		return "ChromaKey";
	else if (0 == strcmp(id, FILTER_TYPE_ID_COLOR_FILTER))
		return "Color Correction";
	else if (0 == strcmp(id, FILTER_TYPE_ID_APPLYLUT))
		return "Color Filter (LUT)";
	else if (0 == strcmp(id, FILTER_TYPE_ID_COLOR_KEY_FILTER))
		return "Color Key";
	else if (0 == strcmp(id, FILTER_TYPE_ID_COMPRESSOR))
		return "Compressor";
	else if (0 == strcmp(id, FILTER_TYPE_ID_CROP_PAD))
		return "Crop/Pad";
	else if (0 == strcmp(id, FILTER_TYPE_ID_EXPANDER))
		return "Expander";
	else if (0 == strcmp(id, FILTER_TYPE_ID_GAIN))
		return "Gain";
	else if (0 == strcmp(id, FILTER_TYPE_ID_RENDER_DELAY))
		return "Render Delay";
	else if (0 == strcmp(id, FILTER_TYPE_ID_INVERT_POLARITY))
		return "Invert Polarity";
	else if (0 == strcmp(id, FILTER_TYPE_ID_LIMITER))
		return "Limiter";
	else if (0 == strcmp(id, FILTER_TYPE_ID_LUMAKEY))
		return "Luma Key";
	else if (0 == strcmp(id, FILTER_TYPE_ID_IMAGEMASK_BLEND))
		return "Image Mask/Blend";
	else if (0 == strcmp(id, FILTER_TYPE_ID_NOISEGATE))
		return "Noise Gate";
	else if (0 == strcmp(id, FILTER_TYPE_ID_NOISE_SUPPRESSION))
		return "Noise Suppression";
	else if (0 == strcmp(id, FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE))
		return "Rnnoise";
	else if (0 == strcmp(id, FILTER_TYPE_ID_SCALING_ASPECTRATIO))
		return "Scale/Aspect Ratio";
	else if (0 == strcmp(id, FILTER_TYPE_ID_SCROLL))
		return "Scroll";
	else if (0 == strcmp(id, FILTER_TYPE_ID_SHARPEN))
		return "Sharpen";
	else if (0 == strcmp(id, FILTER_TYPE_ID_SOUND_TOUCH_FILTER))
		return "Soundtouch Filter";
	else if (0 == strcmp(id, FILTER_TYPE_ID_VSTPLUGIN))
		return "VST 2.x Plug-in";
	else if (0 == strcmp(id, FILTER_TYPE_ID_PREMULTIPLIED_ALPHA_FILTER))
		return "Fix alpha blending";
	else
		return id;
}

FRONTEND_API void pls_get_scene_source_count(int &sceneCount, int &sourceCount)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_get_scene_source_count(sceneCount, sourceCount);
	}
}

FRONTEND_API int pls_show_download_failed_alert(QWidget *paren)
{
	QMap<PLSAlertView::Button, QString> buttons = {{PLSAlertView::Button::Ok, pls_translate_qstr("Retry")}, {PLSAlertView::Button::Cancel, pls_translate_qstr("OK")}};
	return PLSAlertView::question(paren, pls_translate_qstr("Alert.Title"), pls_translate_qstr("Basic.resource.downlaod.failed.retry"), buttons);
}

FRONTEND_API QVector<QString> pls_get_scene_collections()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_scene_collections();
	}
	return {};
}

#ifdef ENABLE_CAPTURE_IMAGE
FRONTEND_API bool pls_compress_directory(const char *src_dir, const char *zip_file, bool sdel)
{
	if (!src_dir || !zip_file)
		return false;

	QStringList args{"a", QString::fromUtf8(zip_file).replace("\\", "/"), QString::fromUtf8(src_dir).replace("\\", "/").append("*")};
	if (sdel)
		args.append("-sdel");
	QProcess compressor;

	compressor.start("7z.exe", args);
	if (!compressor.waitForStarted()) {
		PLS_INFO(MAIN_FRONTEND_API, "Failed to start compress file. errorCode: %d", compressor.error());
		return false;
	}

	if (!compressor.waitForFinished()) {
		PLS_INFO(MAIN_FRONTEND_API, "Failed to finish compress file. errorCode: %d", compressor.error());
		return false;
	}

	return true;
}
#endif

#ifdef Q_OS_WIN

using PDnsServiceRegister = DWORD(WINAPI *)(PDNS_SERVICE_REGISTER_REQUEST, PDNS_SERVICE_CANCEL);
using PDnsServiceDeRegister = DWORD(WINAPI *)(PDNS_SERVICE_REGISTER_REQUEST, PDNS_SERVICE_CANCEL);

void _invokeMDNSInfo()
{
	std::lock_guard _lockMDNSInfo(LocalGlobalVars::mutexMDNSInfo);

	if (LocalGlobalVars::lstMDNSInfo.empty()) {
		return;
	}

	MDNSInfo &info = LocalGlobalVars::lstMDNSInfo.front();

	DNS_SERVICE_REGISTER_REQUEST request = {};
	request.Version = DNS_QUERY_REQUEST_VERSION1;
	request.pRegisterCompletionCallback = [](DWORD, PVOID, PDNS_SERVICE_INSTANCE) {
		std::lock_guard _lockMDNSInfoCallback(LocalGlobalVars::mutexMDNSInfo);

		if (!LocalGlobalVars::lstMDNSInfo.empty()) {
			LocalGlobalVars::lstMDNSInfo.pop_front();
		}

		if (!LocalGlobalVars::lstMDNSInfo.empty()) {
			_invokeMDNSInfo();
		}
	};

	DNS_SERVICE_INSTANCE instance = {};
	request.pServiceInstance = &instance;

	instance.pszInstanceName = info.strInstance.data();
	instance.pszHostName = info.strHostname.data();
	instance.wPort = info.wPort;

	static std::wstring strId(L"id");
	static std::wstring strName(L"name");
	static std::wstring strDeviceType(L"deviceType");
	static std::wstring strConnectType(L"connectType");
	static std::wstring strVersion(L"version");

	static std::array<PWSTR, 5> keys = {strId.data(), strName.data(), strDeviceType.data(), strConnectType.data(), strVersion.data()};
	std::array<PWSTR, 5> values = {info.strId.data(), info.strName.data(), info.strDeviceType.data(), info.strConnectType.data(), info.strVersion.data()};
	instance.keys = keys.data();
	instance.values = values.data();
	instance.dwPropertyCount = keys.size();

	static auto hDll = LoadLibrary(L"dnsapi.dll");
	static auto pDnsServiceRegister = (PDnsServiceRegister)GetProcAddress(hDll, "DnsServiceRegister");
	static auto pDnsServiceDeRegister = (PDnsServiceDeRegister)GetProcAddress(hDll, "DnsServiceDeRegister");

	if (info.bRegister && nullptr != pDnsServiceRegister) {
		pDnsServiceRegister(&request, nullptr);
	} else if (!info.bRegister && nullptr != pDnsServiceDeRegister) {
		pDnsServiceDeRegister(&request, nullptr);
	}
}
#else
void _invokeMDNSInfo() {}
#endif

#if _WIN32
FRONTEND_API bool pls_register_mdns_service(const char *pszName, unsigned short wPort, const ServiceRecord &record, bool bRegister)
{
	MDNSInfo info = {};

	std::string strInstance(pszName);
	strInstance.append(".").append("_prismconnect._tcp.").append("local");
	info.strInstance = pls_utf8_to_unicode(strInstance.data());
	info.strHostname = QHostInfo::localHostName().toStdWString() + L".local";
	info.wPort = wPort;

	info.strId = pls_utf8_to_unicode(record.id);
	info.strName = pls_utf8_to_unicode(record.name);
	info.strDeviceType = pls_utf8_to_unicode(record.deviceType);
	info.strConnectType = pls_utf8_to_unicode(record.connectType);
	info.strVersion = std::to_wstring(record.version);

	info.bRegister = bRegister;

	LocalGlobalVars::mutexMDNSInfo.lock();
	LocalGlobalVars::lstMDNSInfo.push_back(info);
	auto iSize = LocalGlobalVars::lstMDNSInfo.size();
	LocalGlobalVars::mutexMDNSInfo.unlock();

	if (iSize == 1) {
		_invokeMDNSInfo();
	}

	return true;
}

#else

FRONTEND_API void pls_register_mdns_service_ex(const char *pszName, unsigned short wPort, const ServiceRecord &record, bool bRegister, void *context, void (*completion)(void *context, bool success))
{
	std::string uuid(pszName);
	if (uuid.empty())
		return;

	if (bRegister) {
		DNSSDRegisterManager::getInstance()->registerService(uuid, wPort, record, context, completion);
	} else {
		DNSSDRegisterManager::getInstance()->unregisterService(uuid);
		completion(context, true);
	}
}

#endif

FRONTEND_API void pls_set_remote_control_server_info(quint16 port)
{
	if (!LocalGlobalVars::fc || !LocalGlobalVars::fc->obs_frontend_get_global_config()) {
		return;
	}

	auto config = LocalGlobalVars::fc->obs_frontend_get_global_config();
	config_set_int(config, ConfigKey(ConfigId::RemoteControlConfig), "port", port);
	config_save(config);
}
FRONTEND_API void pls_get_remote_control_server_info(quint16 &port)
{
	if (!LocalGlobalVars::fc || !LocalGlobalVars::fc->obs_frontend_get_global_config()) {
		return;
	}

	auto config = LocalGlobalVars::fc->obs_frontend_get_global_config();
	port = (quint16)config_get_int(config, ConfigKey(ConfigId::RemoteControlConfig), "port");
}
FRONTEND_API void pls_set_remote_control_client_info(const QString &peerName, bool connected)
{
	if (!LocalGlobalVars::fc || !LocalGlobalVars::fc->obs_frontend_get_global_config()) {
		return;
	}

	auto config = LocalGlobalVars::fc->obs_frontend_get_global_config();
	config_set_string(config, ConfigKey(ConfigId::RemoteControlConfig), "peerName", peerName.toStdString().c_str());
	config_set_bool(config, ConfigKey(ConfigId::RemoteControlConfig), "connected", connected);
	config_save(config);

	if (callbacks_valid()) {
		LocalGlobalVars::fc->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_REMOTE_CONTROL_CONNECTION_CHANGED, {peerName, connected});
	}
}
FRONTEND_API void pls_get_remote_control_client_info(QString &peerName, bool &connected)
{
	if (!LocalGlobalVars::fc || !LocalGlobalVars::fc->obs_frontend_get_global_config()) {
		return;
	}

	auto config = LocalGlobalVars::fc->obs_frontend_get_global_config();
	auto p = config_get_string(config, ConfigKey(ConfigId::RemoteControlConfig), "peerName");
	peerName = p == nullptr ? "" : QString(p);
	connected = config_get_bool(config, ConfigKey(ConfigId::RemoteControlConfig), "connected");
}
FRONTEND_API void pls_set_remote_control_log_file(const QString &logFile)
{
	if (!LocalGlobalVars::fc || !LocalGlobalVars::fc->obs_frontend_get_global_config()) {
		return;
	}

	auto config = LocalGlobalVars::fc->obs_frontend_get_global_config();
	config_set_string(config, ConfigKey(ConfigId::RemoteControlConfig), "logFile", logFile.toStdString().c_str());
	config_save(config);
}
FRONTEND_API void pls_get_remote_control_log_file(QString &logFile)
{
	if (!LocalGlobalVars::fc || !LocalGlobalVars::fc->obs_frontend_get_global_config()) {
		return;
	}

	auto config = LocalGlobalVars::fc->obs_frontend_get_global_config();
	auto p = config_get_string(config, ConfigKey(ConfigId::RemoteControlConfig), "logFile");
	logFile = p == nullptr ? "" : QString(p);
}

FRONTEND_API void pls_sys_tray_notify(const QString &text, QSystemTrayIcon::MessageIcon n, bool usePrismLogo)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_sys_tray_notify(text, n, usePrismLogo);
	}
}
FRONTEND_API config_t *pls_get_global_cookie_config(void)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_global_cookie_config();
	}
	return nullptr;
}

static pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, const QString &userId, pls::Buttons buttons,
					   pls::Button defaultButton, const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_alert_error_message(parent, title, message, errorCode, userId, buttons, defaultButton, timeout, properties);
	}
	return PLSAlertView::warning(parent, title, message, buttons, defaultButton, timeout, properties);
}
static pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, const QString &userId, const QMap<pls::Button, QString> &buttons,
					   pls::Button defaultButton, const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_alert_error_message(parent, title, message, errorCode, userId, buttons, defaultButton, timeout, properties);
	}
	return PLSAlertView::warning(parent, title, message, buttons, defaultButton, timeout, properties);
}
FRONTEND_API pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, pls::Buttons buttons, pls::Button defaultButton, const std::optional<int> &timeout,
						 const QMap<QString, QVariant> &properties)
{
	return pls_alert_error_message(parent, title, message, QString(), buttons, defaultButton, timeout, properties);
}
FRONTEND_API pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, const QMap<pls::Button, QString> &buttons, pls::Button defaultButton,
						 const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return pls_alert_error_message(parent, title, message, QString(), buttons, defaultButton, timeout, properties);
}
FRONTEND_API pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, pls::Buttons buttons, pls::Button defaultButton,
						 const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return pls_alert_error_message(parent, title, message, errorCode, pls_get_prism_usercode(), buttons, defaultButton, timeout, properties);
}
FRONTEND_API pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, const QMap<pls::Button, QString> &buttons,
						 pls::Button defaultButton, const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return pls_alert_error_message(parent, title, message, errorCode, pls_get_prism_usercode(), buttons, defaultButton, timeout, properties);
}

FRONTEND_API pls::IPropertyModel *pls_get_property_model(obs_source_t *source, obs_data_t *settings, obs_properties_t *props, obs_property_t *prop)
{
	if (source && settings && props && prop && callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_property_model(source, settings, props, prop);
	}
	return nullptr;
}
FRONTEND_API void pls_template_button_refresh_gif_geometry(QAbstractButton *button)
{
	if (callbacks_valid()) {
		LocalGlobalVars::fc->pls_template_button_refresh_gif_geometry(button);
	}
}

FRONTEND_API pls_load_ndi_runtime_pfn pls_get_load_ndi_runtime()
{
	return LocalGlobalVars::loadNdiRuntime;
}
FRONTEND_API void pls_set_load_ndi_runtime(pls_load_ndi_runtime_pfn load_ndi_runtime)
{
	LocalGlobalVars::loadNdiRuntime = load_ndi_runtime;
}

FRONTEND_API QWidget *pls_get_banner_widget()
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_get_banner_widget();
	}
	return nullptr;
}

FRONTEND_API void pls_open_cam_studio(QStringList arguments, QWidget *parent)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_open_cam_studio(arguments, parent);
	}
}

FRONTEND_API void pls_show_cam_studio_uninstall(QWidget *parent, QString title, QString content, QString okTip, QString cancelTip)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_show_cam_studio_uninstall(parent, title, content, okTip, cancelTip);
	}
}

FRONTEND_API bool pls_is_install_cam_studio(QString &program)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_is_install_cam_studio(program);
	}
	return false;
}

FRONTEND_API const char *pls_source_get_display_name(const char *id)
{
	if (callbacks_valid()) {
		return LocalGlobalVars::fc->pls_source_get_display_name(id);
	}
	return nullptr;
}

FRONTEND_API bool pls_is_always_on_top(const QWidget *widget)
{
	return PLSToplevelWidget::isAlwaysOnTop(widget);
}
