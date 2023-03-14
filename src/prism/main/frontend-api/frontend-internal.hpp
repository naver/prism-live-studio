#pragma once

#include "frontend-api.h"

#include <obs-frontend-internal.hpp>

#include <vector>
#include <string>
#include <QNetworkCookie>
#include "../../prism/main/pls-gpop-data.hpp"
#include "../../prism/main/login-web-handler.hpp"
#include "login-info.hpp"

struct pls_frontend_callbacks : public obs_frontend_callbacks {
	virtual ~pls_frontend_callbacks() {}

	virtual bool pls_register_login_info(PLSLoginInfo *login_info) = 0;
	virtual void pls_unregister_login_info(PLSLoginInfo *login_info) = 0;
	virtual int pls_get_login_info_count() = 0;
	virtual PLSLoginInfo *pls_get_login_info(int index) = 0;
	virtual void del_pannel_cookies(const QString &pannelName) = 0;
	virtual void pls_del_specific_url_cookie(const QString &url, const QString &cookieName) = 0;
	virtual QJsonObject pls_ssmap_to_json(const QMap<QString, QString> &ssmap) = 0;

	virtual bool pls_browser_view(QJsonObject &result, const QUrl &url, pls_result_checking_callback_t callback, QWidget *parent) = 0;
	virtual bool pls_browser_view(QJsonObject &result, const QUrl &url, const std::map<std::string, std::string> &headers, const QString &pannelName, const std::string &script,
				      pls_result_checking_callback_t callback, QWidget *parent) = 0;
	virtual bool pls_rtmp_view(QJsonObject &result, PLSLoginInfo *login_info, QWidget *parent) = 0;
	virtual bool pls_channel_login(QJsonObject &result, QWidget *parent) = 0;

	virtual bool pls_sns_user_info(QJsonObject &result, const QList<QNetworkCookie> &cookies, const QString &urlStr) = 0;
	virtual bool pls_google_user_info(std::function<void(bool ok, const QJsonObject &)> callback, const QString &redirect_uri, const QString &code, QObject *receiver) = 0;
	virtual bool pls_network_environment_reachable() = 0;

	virtual bool pls_prism_token_is_vaild(const QString &urlStr) = 0;
	virtual void pls_prism_change_over_login_view() = 0;
	virtual void pls_prism_logout(const QString &urlStr, LoginInfoType infoType = LoginInfoType::PrismLogoutInfo) = 0;
	virtual QString pls_prism_user_thumbnail_path() = 0;
	virtual Common pls_get_common() = 0;
	virtual VliveNotice pls_get_vlive_notice() = 0;
	virtual QMap<QString, SnsCallbackUrl> pls_get_snscallback_urls() = 0;
	virtual Connection pls_get_connection() = 0;
	virtual QMap<int, RtmpDestination> pls_get_rtmpDestination() = 0;

	virtual QString pls_get_gcc_data() = 0;

	virtual QString pls_get_prism_token() = 0;
	virtual QString pls_get_prism_email() = 0;
	virtual QString pls_get_prism_thmbanilurl() = 0;
	virtual QString pls_get_prism_nickname() = 0;
	virtual QString pls_get_prism_usercode() = 0;

	virtual QWidget *pls_get_main_view() = 0;
	virtual QWidget *pls_get_toplevel_view(QWidget *widget) = 0;

	virtual QByteArray pls_get_prism_cookie() = 0;

	virtual void pls_frontend_add_event_callback(pls_frontend_event_cb callback, void *context) = 0;
	virtual void pls_frontend_add_event_callback(pls_frontend_event event, pls_frontend_event_cb callback, void *context) = 0;
	virtual void pls_frontend_add_event_callback(QList<pls_frontend_event> events, pls_frontend_event_cb callback, void *context) = 0;
	virtual void pls_frontend_remove_event_callback(pls_frontend_event_cb callback, void *context) = 0;
	virtual void pls_frontend_remove_event_callback(pls_frontend_event event, pls_frontend_event_cb callback, void *context) = 0;
	virtual void pls_frontend_remove_event_callback(QList<pls_frontend_event> events, pls_frontend_event_cb callback, void *context) = 0;
	virtual void on_event(enum obs_frontend_event event) = 0;
	virtual void on_event(pls_frontend_event event, const QVariantList &params = QVariantList()) = 0;

	virtual QString pls_get_theme_dir_path() = 0;
	virtual QString pls_get_color_filter_dir_path() = 0;
	virtual void pls_toast_message(pls_toast_info_type type, const QString &message, int auto_close) = 0;
	virtual void pls_toast_message(pls_toast_info_type type, const QString &message, const QString &url, const QString &replaceStr, int auto_close) = 0;
	virtual void pls_toast_clear() = 0;

	virtual void pls_set_main_view_side_bar_user_button_icon(const QIcon &icon) = 0;

	virtual void pls_load_chat_dock(const QString &objectName, const std::string &url, const std::string &white_popup_url, const std::string &startup_script) = 0;
	virtual void pls_unload_chat_dock(const QString &objectName) = 0;

	virtual const char *pls_basic_config_get_string(const char *section, const char *name, const char *) = 0;
	virtual int64_t pls_basic_config_get_int(const char *section, const char *name, int64_t) = 0;
	virtual uint64_t pls_basic_config_get_uint(const char *section, const char *name, uint64_t) = 0;
	virtual bool pls_basic_config_get_bool(const char *section, const char *name, bool) = 0;
	virtual double pls_basic_config_get_double(const char *section, const char *name, double) = 0;

	virtual pls_check_update_result_t pls_check_update(QString &gcc, bool &is_force, QString &version, QString &file_url, QString &update_info_url) = 0;
	virtual bool pls_check_lastest_version(QString &update_info_url) = 0;
	virtual pls_upload_file_result_t pls_upload_contactus_files(const QString &email, const QString &question, const QList<QFileInfo> files) = 0;
	virtual bool pls_show_update_info_view(bool is_force, const QString &version, const QString &file_url, const QString &update_info_url, bool is_manual, QWidget *parent) = 0;
	virtual bool pls_download_update(QString &local_file_path, const QString &file_url, PLSCancel &cancel, const std::function<void(qint64 download_bytes, qint64 total_bytes)> &progress) = 0;
	virtual bool pls_install_update(const QString &file_path) = 0;

	virtual QVariantMap pls_get_new_notice_Info() = 0;

	virtual QString pls_get_win_os_version() = 0;

	virtual bool pls_is_living_or_recording() = 0;

	// add tools menu seperator
	virtual void pls_add_tools_menu_seperator() = 0;

	virtual void pls_start_broadcast(bool toStart = true) = 0;

	virtual void pls_start_record(bool toStart = true) = 0;

	virtual ITextMotionTemplateHelper *pls_get_text_motion_template_helper_instance() = 0;

	virtual QString pls_get_current_language() = 0;

	virtual int pls_get_actived_chat_channel_count() = 0;
	virtual int pls_get_prism_live_seq() = 0;
	virtual bool pls_is_create_souce_in_loading() = 0;

	virtual void pls_network_state_monitor(std::function<void(bool)> &&callback) = 0;
	virtual bool pls_get_network_state() = 0;

	virtual void pls_show_virtual_background() = 0;
	virtual QWidget *pls_create_virtual_background_resource_widget(QWidget *parent, std::function<void(QWidget *)> &&init, bool forProperty, const QString &itemId, bool checkBoxState,
								       bool switchToPrismFirst) = 0;

	virtual QPixmap pls_load_svg(const QString &path, const QSize &size) = 0;

	virtual void pls_show_mobile_source_help() = 0;

	virtual pls_blacklist_type pls_is_blacklist(QString value, pls_blacklist_type type) = 0;
	virtual void pls_alert_third_party_plugins(QString pluginName, QWidget *parent = nullptr) = 0;

	//PRISM/Xiewei/20210113/#/add apis for stream deck
	virtual void pls_set_side_window_visible(int key, bool visible) = 0;
	virtual void pls_mixer_mute_all(bool mute) = 0;
	virtual bool pls_mixer_is_all_mute() = 0;
	virtual QList<SideWindowInfo> pls_get_side_windows_info() = 0;
	virtual int pls_get_toast_message_count() = 0;
	virtual QString pls_get_login_state() = 0;
	virtual QString pls_get_stream_state() = 0;
	virtual QString pls_get_record_state() = 0;
	virtual bool pls_get_live_record_available() = 0;

	virtual QDialogButtonBox::StandardButton pls_alert_warning(const char *title, const char *message) = 0;

	virtual void pls_singletonWakeup() = 0;

	virtual uint pls_get_live_start_time() = 0;
};

FRONTEND_API void pls_frontend_set_callbacks_internal(pls_frontend_callbacks *callbacks);
