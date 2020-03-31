#pragma once

#include "frontend-api.h"

#include <obs-frontend-internal.hpp>

#include <vector>
#include <string>
#include <QNetworkCookie>

struct pls_frontend_callbacks : public obs_frontend_callbacks {
	virtual ~pls_frontend_callbacks() {}

	virtual void pls_del_specific_url_cookie(const QString &url) = 0;

	virtual bool pls_browser_view(QJsonObject &result, const QUrl &url, pls_result_checking_callback_t callback, QWidget *parent) = 0;
	virtual bool pls_browser_view(QJsonObject &result, const QUrl &url, const std::map<std::string, std::string> &headers, pls_result_checking_callback_t callback, QWidget *parent) = 0;
	
	virtual bool pls_network_environment_reachable() = 0;

	virtual QWidget *pls_get_main_view() = 0;
	virtual QWidget *pls_get_toplevel_view(QWidget *widget) = 0;


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
	virtual void pls_toast_clear() = 0;

	virtual void pls_set_main_view_side_bar_user_button_icon(const QIcon &icon) = 0;

	virtual void pls_load_chat_dock(const QString &objectName, const std::string &url, const std::string &white_popup_url, const std::string &startup_script) = 0;
	virtual void pls_unload_chat_dock(const QString &objectName) = 0;

	virtual const char *pls_basic_config_get_string(const char *section, const char *name, const char *) = 0;
	virtual int64_t pls_basic_config_get_int(const char *section, const char *name, int64_t) = 0;
	virtual uint64_t pls_basic_config_get_uint(const char *section, const char *name, uint64_t) = 0;
	virtual bool pls_basic_config_get_bool(const char *section, const char *name, bool) = 0;
	virtual double pls_basic_config_get_double(const char *section, const char *name, double) = 0;

	virtual QString pls_get_win_os_version() = 0;

	virtual bool pls_is_living_or_recording() = 0;

	// add tools menu seperator
	virtual void pls_add_tools_menu_seperator() = 0;
};

FRONTEND_API void pls_frontend_set_callbacks_internal(pls_frontend_callbacks *callbacks);
