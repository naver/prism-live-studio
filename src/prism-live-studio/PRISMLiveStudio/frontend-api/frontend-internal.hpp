#pragma once

#include "frontend-api.h"

#include <obs-frontend-internal.hpp>

#include <vector>
#include <string>
#include <qnetworkcookie.h>
#include "login-info.hpp"
#include <qmap.h>

class Common;
class SnsCallbackUrl;
class Connection;
class RtmpDestination;

struct pls_frontend_callbacks : public obs_frontend_callbacks {
	~pls_frontend_callbacks() override = default;

	virtual bool pls_register_login_info(PLSLoginInfo *login_info) = 0;
	virtual void pls_unregister_login_info(PLSLoginInfo *login_info) = 0;
	virtual int pls_get_login_info_count() = 0;
	virtual PLSLoginInfo *pls_get_login_info(int index) = 0;
	virtual void del_pannel_cookies(const QString &pannelName) = 0;
	virtual void pls_set_manual_cookies(const QString &pannelName) = 0;
	virtual void pls_del_specific_url_cookie(const QString &url, const QString &cookieName) = 0;
	virtual QJsonObject pls_ssmap_to_json(const QMap<QString, QString> &ssmap) = 0;

	virtual bool pls_browser_view(QVariantHash &result, const QUrl &url, const pls_result_checking_callback_t &callback, QWidget *parent, bool readCookies) = 0;
	virtual bool pls_browser_view(QVariantHash &result, const QUrl &url, const std_map<std::string, std::string> &headers, const QString &pannelName, const std::string &script,
				      const pls_result_checking_callback_t &callback, QWidget *parent, bool readCookies) = 0;
	virtual bool pls_rtmp_view(QJsonObject &result, PLSLoginInfo *login_info, QWidget *parent) = 0;
	virtual bool pls_channel_login(QJsonObject &result, QWidget *parent) = 0;

	virtual void pls_prism_change_over_login_view() = 0;
	virtual void pls_prism_logout(const QString &urlStr) = 0;
	virtual QString pls_prism_user_thumbnail_path() = 0;
	virtual Common pls_get_common() = 0;
	virtual QMap<QString, SnsCallbackUrl> pls_get_snscallback_urls() = 0;
	virtual Connection pls_get_connection() = 0;
	virtual QMap<int, RtmpDestination> pls_get_rtmpDestination() = 0;

	virtual QString pls_get_gcc_data() = 0;

	virtual QString pls_get_prism_token() = 0;
	virtual QString pls_get_prism_email() = 0;
	virtual QString pls_get_prism_thmbanilurl() = 0;
	virtual QString pls_get_prism_nickname() = 0;
	virtual QString pls_get_prism_usercode() = 0;

	virtual QByteArray pls_get_prism_cookie() = 0;
	virtual QString pls_get_b2b_auth_url() = 0;
	virtual bool pls_get_b2b_acctoken(const QString &url) = 0;

	virtual void pls_frontend_add_event_callback(pls_frontend_event_cb callback, void *context) = 0;
	virtual void pls_frontend_remove_event_callback(pls_frontend_event_cb callback, void *context) = 0;
	void on_event(enum obs_frontend_event event) override = 0;
	virtual void on_event(pls_frontend_event event, const QVariantList &params = QVariantList()) = 0;

	virtual QString pls_get_theme_dir_path() = 0;
	virtual QString pls_get_color_filter_dir_path() = 0;
	virtual void pls_toast_message(pls_toast_info_type type, const QString &message, int auto_close) = 0;
	virtual void pls_toast_message(pls_toast_info_type type, const QString &message, const QString &url, const QString &replaceStr, int auto_close) = 0;
	virtual void pls_toast_clear() = 0;

	virtual void pls_set_main_view_side_bar_user_button_icon(const QIcon &icon) = 0;

	virtual void pls_unload_chat_dock(const QString &objectName) = 0;

	virtual const char *pls_basic_config_get_string(const char *section, const char *name, const char *) = 0;
	virtual int64_t pls_basic_config_get_int(const char *section, const char *name, int64_t) = 0;
	virtual uint64_t pls_basic_config_get_uint(const char *section, const char *name, uint64_t) = 0;
	virtual bool pls_basic_config_get_bool(const char *section, const char *name, bool) = 0;
	virtual double pls_basic_config_get_double(const char *section, const char *name, double) = 0;

	virtual pls_check_update_result_t pls_check_app_update(bool &is_force, QString &version, QString &file_url, QString &update_info_url, PLSErrorHandler::RetData &retData) = 0;
	virtual pls_upload_file_result_t pls_upload_contactus_files(PLS_CONTACTUS_QUESTION_TYPE iType, const QString &email, const QString &question, const QList<QFileInfo> files) = 0;
	virtual bool pls_show_update_info_view(bool is_force, const QString &version, const QString &file_url, const QString &update_info_url, bool is_manual, QWidget *parent) = 0;
	virtual void pls_get_new_notice_Info(const std::function<void(const QVariantMap &noticeInfo)> &noticeCallback) = 0;

	virtual QString pls_get_win_os_version() = 0;

	virtual bool pls_is_living_or_recording() = 0;

	virtual bool pls_is_output_actived() = 0;

	// add tools menu seperator
	virtual void pls_add_tools_menu_seperator() = 0;

	virtual void pls_start_broadcast(bool toStart = true) = 0;

	virtual void pls_start_broadcast_in_info(bool toStart = true) = 0;

	virtual void pls_start_rehearsal(bool toStart = true) = 0;

	virtual void pls_start_record(bool toStart = true) = 0;

	virtual ITextMotionTemplateHelper *pls_get_text_motion_template_helper_instance() = 0;
	virtual ITextMotionTemplateHelper *pls_get_chat_template_helper_instance() = 0;
	virtual QString pls_get_current_language() = 0;

	virtual int pls_get_actived_chat_channel_count() = 0;
	virtual void pls_get_prism_live_seq(int &seqHorizontal, int &seqVertical) = 0;
	virtual bool pls_is_create_souce_in_loading() = 0;

	virtual void pls_network_state_monitor(const std::function<void(bool)> &callback) = 0;
	virtual bool pls_get_network_state() = 0;

	virtual void pls_show_virtual_background() = 0;
	virtual QWidget *pls_create_virtual_background_resource_widget(QWidget *parent, const std::function<void(QWidget *)> &init, bool forProperty, const QString &itemId, bool checkBoxState,
								       bool switchToPrismFirst) = 0;

	virtual QPixmap pls_load_svg(const QString &path, const QSize &size) = 0;

	virtual void pls_set_bgm_visible(bool visible) = 0;

	//PRISM/Xiewei/20210113/#/add apis for stream deck
	virtual bool pls_set_side_window_visible(int key, bool visible) = 0;
	virtual void pls_mixer_mute_all(bool mute) = 0;
	virtual bool pls_mixer_is_all_mute() = 0;
	virtual QList<SideWindowInfo> pls_get_side_windows_info() = 0;
	virtual int pls_get_toast_message_count() = 0;
	virtual QString pls_get_stream_state() = 0;
	virtual QString pls_get_record_state() = 0;
	virtual int pls_get_record_duration() = 0;
	virtual bool pls_get_hotkey_enable() = 0;

	virtual int pls_alert_warning(const char *title, const char *message) = 0;

	virtual void pls_singleton_wakeup() = 0;

	virtual uint pls_get_live_start_time() = 0;

	virtual void pls_navershopping_get_store_login_url(QWidget *widget, const std::function<void(const QString &storeLoginUrl)> &ok, const std::function<void(const QByteArray &)> &fail) = 0;
	virtual void pls_navershopping_get_error_code_message(const QByteArray &data, QString &errorCode, QString &errorMessage) = 0;

	virtual void pls_send_analog(AnalogType logType, const QVariantMap &info) = 0;

	virtual void pls_get_scene_source_count(int &sceneCount, int &sourceCount) = 0;

	virtual void pls_laboratory_click_open_button(const QString &laboratoryId, bool targetStatus) = 0;

	virtual void pls_set_laboratory_status(const QString &laboratoryId, bool on) = 0;
	virtual void pls_laboratory_detail_page_js_event(const QString &page, const QString &action, const QString &info) = 0;
	virtual bool pls_get_laboratory_status(const QString &laboratoryId) = 0;
	virtual QJsonObject pls_get_resource_statistics_data() = 0;
	virtual bool pls_click_alert_message() = 0;
	virtual bool pls_alert_message_visible() = 0;
	virtual int pls_alert_message_count() = 0;
	virtual QList<std::tuple<QString, QString>> pls_get_user_active_channles_info() = 0;
	virtual bool pls_is_rehearsal_info_display() = 0;
	virtual QString pls_get_remote_control_mobile_name(const QString &platformName) = 0;

	virtual bool pls_is_rehearsaling() = 0;

	virtual void pls_sys_tray_notify(const QString &text, QSystemTrayIcon::MessageIcon n, bool usePrismLogo = true) = 0;

	virtual bool pls_get_chat_info(QString &id, int &seqHorizontal, int &seqVertical, QString &cookie, bool &isSinglePlatform) = 0;
	virtual int pls_get_current_selected_channel_count() = 0;

	virtual QVector<QString> pls_get_scene_collections() = 0;

	virtual pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, const QString &userId, pls::Buttons buttons,
						    pls::Button defaultButton, const std::optional<int> &timeout, const QMap<QString, QVariant> &properties = QMap<QString, QVariant>()) = 0;
	virtual pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, const QString &userId,
						    const QMap<pls::Button, QString> &buttons, pls::Button defaultButton, const std::optional<int> &timeout,
						    const QMap<QString, QVariant> &properties = QMap<QString, QVariant>()) = 0;

	virtual pls::IPropertyModel *pls_get_property_model(obs_source_t *source, obs_data_t *settings, obs_properties_t *props, obs_property_t *prop) = 0;
	virtual void pls_template_button_refresh_gif_geometry(QAbstractButton *button) const = 0;
	virtual config_t *pls_get_global_cookie_config(void) const = 0;
	virtual QWidget *pls_get_banner_widget() const = 0;
	virtual void pls_open_cam_studio(QStringList arguments, QWidget *parent) const = 0;
	virtual void pls_show_cam_studio_uninstall(QWidget *parent, QString title, QString content, QString okTip, QString cancelTip) = 0;
	virtual bool pls_is_install_cam_studio(QString &program) const = 0;
	virtual const char *pls_source_get_display_name(const char *id) = 0;
	virtual QVariantMap pls_http_request_head(bool hasGacc) = 0;

	virtual QStringList getChannelWithChatList(bool bAddNCPPrefix) = 0;
	virtual bool pls_is_ncp(QString &channlName) = 0;
	virtual bool pls_is_ncp_first_login(QString &serviceName) = 0;
	virtual bool pls_install_scene_template(const SceneTemplateItem &item) = 0;
	virtual QString get_channel_cookie_path(const QString &channelLoginName) = 0;
	virtual bool pls_get_output_stream_dealy_active() = 0;
	virtual bool pls_is_chzzk_checked(bool forHorizontal) = 0;
	virtual obs_output_t *pls_frontend_get_streaming_output_v(void) = 0;
};

FRONTEND_API void pls_frontend_set_callbacks_internal(pls_frontend_callbacks *callbacks);
