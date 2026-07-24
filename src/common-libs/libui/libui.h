#if !defined(COMMONLIBS_LIBUI_LIBUI_H)
#define COMMONLIBS_LIBUI_LIBUI_H

#include "libui-globals.h"

#include <atomic>
#include <functional>

#include <qpointer.h>
#include <qwidget.h>
#include <qvariant.h>
#include <qdialog.h>
#include <qmenu.h>
#include <qmessagebox.h>
#include <qdialogbuttonbox.h>
#include <libutils-api.h>
#include <liblog.h>
#include <pls-performance.h>

LIBUI_API QPointer<QWidget> pls_get_main_view();
LIBUI_API void pls_set_main_view(const QPointer<QWidget> &main_view);

LIBUI_API bool pls_is_main_window_closing();
LIBUI_API void pls_set_main_window_closing(bool);

LIBUI_API bool pls_is_main_window_destroyed();
LIBUI_API void pls_set_main_window_destroyed(bool);

LIBUI_API bool pls_is_toplevel_view(QWidget *widget);
LIBUI_API QWidget *pls_get_toplevel_view(QWidget *widget, QWidget *defval = pls_get_main_view());

LIBUI_API void pls_push_modal_view(QDialog *dialog);
LIBUI_API void pls_pop_modal_view(QDialog *dialog);
LIBUI_API void pls_push_modal_view(QMenu *menu);
LIBUI_API void pls_pop_modal_view(QMenu *menu);
LIBUI_API void pls_notify_close_modal_views();
LIBUI_API void pls_notify_close_modal_views_with_parent(QWidget *parent);
LIBUI_API bool pls_has_modal_view();
LIBUI_API QPointer<QDialog> pls_get_last_modal_view();

LIBUI_API QString pls_load_css(const QStringList &cssNames);
LIBUI_API void pls_set_css(QWidget *widget, const QStringList &cssNames);
LIBUI_API void pls_add_css(QWidget *widget, const QStringList &cssNames);
LIBUI_API void pls_set_global_css(const QStringList &cssNames, const QStringList &csses = QStringList());
LIBUI_API void pls_add_global_css(const QStringList &cssNames, const QStringList &csses = QStringList());

LIBUI_API void pls_flush_style(QWidget *widget);
LIBUI_API void pls_flush_style_recursive(QWidget *widget, int recursiveDeep = -1);
LIBUI_API void pls_flush_style(QWidget *widget, const char *propertyName, const QVariant &propertyValue);
LIBUI_API void pls_flush_style_if_visible(QWidget *widget, const char *propertyName, const QVariant &propertyValue);
LIBUI_API void pls_flush_style_recursive(QWidget *widget, const char *propertyName, const QVariant &propertyValue, int recursiveDeep = -1);

LIBUI_API void pls_scroll_area_clips_to_bounds(QWidget *widget, bool isClips = true);

class QColor;
LIBUI_API QColor pls_qint64_to_qcolor(qint64 icolor);
LIBUI_API qint64 pls_qcolor_to_qint64(const QColor &qcolor);

LIBUI_API QPixmap pls_load_pixmap(const QString &imagePath, const QSize &size);
LIBUI_API QPixmap pls_load_pixmap_with_mode(const QString &imagePath, const QSize &size, Qt::AspectRatioMode ratioMode = Qt::IgnoreAspectRatio,
					    Qt::TransformationMode transMode = Qt::SmoothTransformation);
LIBUI_API QPixmap pls_rounded_pixmap(const QPixmap &pixmap, int radius, bool properties);
LIBUI_API QPixmap pls_selected_rounded_pixmap(const QSize &size, const QPixmap &crop, const QMargins &margin, int radius);
class QTranslator;
LIBUI_API QHash<QString, QString> pls_load_language_values(const QString &languageDir, const QString &language, const QString &defaultLanguage = QStringLiteral("en-US"));
LIBUI_API QHash<QByteArray, QByteArray> pls_load_language_values_utf8(const QString &languageDir, const QString &language, const QString &defaultLanguage = QStringLiteral("en-US"));
LIBUI_API const QByteArray &pls_translate_language_utf8(const QHash<QByteArray, QByteArray> &texts, const QByteArray &key);
LIBUI_API QTranslator *pls_load_language_translator(const QString &languageDir, const QString &language, const QString &defaultLanguage = QStringLiteral("en-US"));
LIBUI_API void pls_load_language(const QString &languageDir, const QString &language, const QString &defaultLanguage = QStringLiteral("en-US"));

LIBUI_API QDialogButtonBox::StandardButton pls_to_standard_button(QMessageBox::StandardButton from);
LIBUI_API QMessageBox::StandardButton pls_to_standard_button(QDialogButtonBox::StandardButton from);
LIBUI_API QDialogButtonBox::StandardButtons pls_to_standard_buttons(QMessageBox::StandardButtons from);
LIBUI_API QMessageBox::StandardButtons pls_to_standard_buttons(QDialogButtonBox::StandardButtons from);

#ifdef Q_OS_WIN
LIBUI_API QSize pls_get_win_cursor_size(QWidget *widget);
LIBUI_API void pls_flood_fill_color(QImage &image, const QColor &targetColor, const QColor &fillColor);
LIBUI_API void pls_flood_fill_color2(QImage &image, const QColor &targetColor, const QColor &fillColor);
LIBUI_API QColor pls_get_win_cursor_main_color();
LIBUI_API QColor pls_get_win_cursor_main_color2();
LIBUI_API QPixmap pls_get_win_custom_drag_pixmap(QWidget *widget);
LIBUI_API bool pls_is_win_cursor_colored();
LIBUI_API QPixmap pls_get_win_custom_grab_pixmap(QWidget *widget);
LIBUI_API QPixmap pls_get_win_custom_hover_pixmap(QWidget *widget);
#endif

struct PLSMonitor {
	void *monitor = nullptr;
	int index = 0;
	bool primary = false;
	bool current = false;
	double dpi = 1.0;
	QRect screenRect;
	QRect availableRect;
};
LIBUI_API QList<PLSMonitor> pls_get_monitors();
LIBUI_API PLSMonitor pls_get_primary_monitor();
LIBUI_API PLSMonitor pls_get_current_monitor();
LIBUI_API PLSMonitor pls_get_monitor(const QPoint &point);
LIBUI_API PLSMonitor pls_get_monitor(QWidget *widget);
LIBUI_API PLSMonitor pls_get_monitor(int index);
LIBUI_API PLSMonitor pls_get_monitor_or_primary(int index);
LIBUI_API QRect pls_get_screen_rect(QWidget *widget);
LIBUI_API QRect pls_get_screen_available_rect(QWidget *widget);
LIBUI_API QRect pls_get_screen_rect(const QPoint &pt);
LIBUI_API QRect pls_get_screen_available_rect(const QPoint &pt);
LIBUI_API bool pls_is_visible_in_some_screen(const QRect &geometry);

enum class Progress;

LIBUI_API QString pls_get_process_code_str(Progress progress, bool next = false);

LIBUI_API QImage pls_generate_qr_image(const QJsonObject &info, int width, int margin, const QPixmap &logo);

LIBUI_API void pls_window_left_right_margin_fit(QWidget *widget);

LIBUI_API QString pls_get_orignal_text(const QString &text);
LIBUI_API QSize pls_calculate_size_for_width(const QString &text, const QFont &font, int width);

LIBUI_API QString pls_get_elided_text(const QString &text, const QFont &font, const QSize &size, bool single_line = true);
class QLabel;
LIBUI_API void pls_elided_text(QLabel *label, const QString &text, bool show_tooltip = true);
LIBUI_API void pls_elided_text(QLabel *label, bool show_tooltip = true);

class pls_singleton_app_t;
LIBUI_API bool pls_singleton_app_instance(std::function<void(const QStringList &second_instance_cmdlines)> &&second_instance_notify, int max_data_size = 1024);
LIBUI_API void pls_singleton_app_destroy(pls_singleton_app_t *singleton_app);

LIBUI_API bool pls_is_app_installed(pls_product_type_t product, QString *program = nullptr, QString *home = nullptr, QString *version = nullptr);
LIBUI_API bool pls_is_app_running(pls_product_type_t product);
LIBUI_API bool pls_is_app_started(pls_product_type_t product);
LIBUI_API bool pls_is_app_exited(pls_process_t *process);

// use for:
// 1 PRISMLiveStudio start PRISMLens
// 2 PRISMLens start PRISMLiveStudio
using pls_app_t = QPointer<QObject>;
LIBUI_API pls_app_t pls_app_create(pls_product_type_t product);
LIBUI_API void pls_app_destroy(pls_app_t app);
enum class pls_app_state_t { AppNotInstalled, OpenProcessOk, OpenProcessFailed, ProcessStarted, ProcessExited, Inactived, Actived, AnyWindowShow, MainWindowShow, AnyWindowActived, MainWindowActived };
Q_DECLARE_METATYPE(pls_app_state_t)
LIBUI_API QString pls_app_state_to_string(pls_app_state_t state);
	// return:
//	true: continue process, false: cancel process
using pls_app_on_state_t = std::function<bool(pls_app_state_t state)>;
// receiver and on_state cannot be nullptr
LIBUI_API void pls_app_open(pls_app_t app, const QStringList &args, QPointer<QObject> receiver, const pls_app_on_state_t &on_state);

//signalName=clicked, action=Click, moduleName=, controls=full path
LIBUI_API void pls_button_uistep_custom(QObject *object, const pls_getter_t &getter = nullptr);
//action=Click, moduleName=, controls=full path
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const pls_getter_t &getter = nullptr);
//moduleName=, controls=full path
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const QString &action, const pls_getter_t &getter = nullptr);
//controls=full path
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &action, const pls_getter_t &getter = nullptr);
//controls=controlFullname
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &controlFullname, const QString &action,
					const pls_getter_t &getter = nullptr);
//controls=full path+controls+additional
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &controls, const QString &additional, const QString &action,
					const pls_getter_t &getter = nullptr);
//controls=controlFullname or controls=full path+controls+additional
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &controlFullname, const QString &controls, const QString &additional,
					const QString &action, const pls_getter_t &getter = nullptr);

//moduleName=, controls=full path
LIBUI_API void pls_control_uistep_custom(QObject *object, const QString &signalName, const QString &action, const pls_getter_t &getter = nullptr);
//controls=full path
LIBUI_API void pls_control_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &action, const pls_getter_t &getter = nullptr);
//controls=controlFullname
LIBUI_API void pls_control_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &controlFullname, const QString &action,
					 const pls_getter_t &getter = nullptr);
//controls=full path+controls+additional
LIBUI_API void pls_control_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &controls, const QString &additional, const QString &action,
					 const pls_getter_t &getter = nullptr);
//controls=controlFullname or controls=full path+controls+additional
LIBUI_API void pls_control_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &controlFullname, const QString &controls, const QString &additional,
					 const QString &action, const pls_getter_t &getter = nullptr);

#if !defined(PTS_LOG_TYPE)
#define PTS_LOG_TYPE "PTSLogType" // field name for log type
#endif

#if !defined(PTS_TYPE_EVENT)
#define PTS_TYPE_EVENT "event" // field value for exception or app stats
#endif

#if !defined(PTS_TYPE_UISTEP_STEP)
#define PTS_TYPE_UISTEP_STEP "uiStep" // field value for UI STEP
#endif

#define PLS_UI_STEPS_V2 "ui-steps.v2"
#define PLS_UI_STEPS_V2_CUSTOM_SHOW_HIDE_NAME "ui-steps.v2.custom.show.hide.name"
#define PLS_UI_STEPS_V2_CUSTOM_ENABLE_DISABLE_NAME "ui-steps.v2.custom.enable.disable.name"
#define PLS_UI_STEPS_V2_CUSTOM_ENTER_LEAVE_NAME "ui-steps.v2.custom.enter.leave.name"
#define PLS_UI_STEPS_V2_CUSTOM_HOVER_ENTER_LEAVE_NAME "ui-steps.v2.custom.hover.enter.leave.name"
#define PLS_UI_STEPS_V2_LISTEN_WINDOW_STATE "ui-steps.v2.listen.window.state"
#define PLS_UI_STEPS_V2_LISTEN_WINDOW_CLOSE "ui-steps.v2.listen.window.close"
#define PLS_UI_STEPS_V2_ENABLED "ui-steps.v2.enabled"
#define PLS_UI_STEPS_V2_DYN_ENABLED "ui-steps.v2.%1.enabled"
#define PLS_UI_STEPS_V2_TITLE "ui-steps.v2.title"
#define PLS_UI_STEPS_V2_TITLE_AUTO_TO_ENGLISH_ENABLED "ui-steps.v2.title.auto.to.english.enabled"
#define PLS_UI_STEPS_V2_ACTION "ui-steps.v2.action"
#define PLS_UI_STEPS_V2_NAME "ui-steps.v2.name"
#define PLS_UI_STEPS_V2_NAME_AUTO_TO_ENGLISH_ENABLED "ui-steps.v2.name.auto.to.english.enabled"
#define PLS_UI_STEPS_V2_DYN_NAME "ui-steps.v2.%1.name"
#define PLS_UI_STEPS_V2_DYN_NAME_AUTO_TO_ENGLISH_ENABLED "ui-steps.v2.%1.name.auto.to.english.enabled"
#define PLS_UI_STEPS_V2_ALL_NAME "ui-steps.v2.*.name"
#define PLS_UI_STEPS_V2_ALL_NAME_AUTO_TO_ENGLISH_ENABLED "ui-steps.v2.*.name.auto.to.english.enabled"
#define PLS_UI_STEPS_V2_NAME_OBJECT "ui-steps.v2.name.object"
#define PLS_UI_STEPS_V2_VALUE "ui-steps.v2.value"
#define PLS_UI_STEPS_V2_VALUE_AUTO_TO_ENGLISH_ENABLED "ui-steps.v2.value.auto.to.english.enabled"
#define PLS_UI_STEPS_V2_DYN_VALUE "ui-steps.v2.%1.value"
#define PLS_UI_STEPS_V2_DYN_VALUE_AUTO_TO_ENGLISH_ENABLED "ui-steps.v2.%1.value.auto.to.english.enabled"
#define PLS_UI_STEPS_V2_ALL_VALUE "ui-steps.v2.*.value"
#define PLS_UI_STEPS_V2_ALL_VALUE_AUTO_TO_ENGLISH_ENABLED "ui-steps.v2.*.value.auto.to.english.enabled"
#define PLS_UI_STEPS_V2_TITLE_WITH_GROUP "ui-steps.v2.title.with.group"

#define PLS_UI_STEPS_V2_ATTR_NAME QStringLiteral("name")
#define PLS_UI_STEPS_V2_ATTR_VALUE QStringLiteral("value")
#define PLS_UI_STEPS_V2_ATTR_VALUE_BUTTON QStringLiteral("button")

#define PLS_UI_STEPS_V2_SIGNAL_CLICKED QStringLiteral("clicked")
#define PLS_UI_STEPS_V2_SIGNAL_TOGGLED QStringLiteral("toggled")
#define PLS_UI_STEPS_V2_SIGNAL_TRIGGERED QStringLiteral("triggered")
#define PLS_UI_STEPS_V2_SIGNAL_EDITINGFINISHED QStringLiteral("editingFinished")
#define PLS_UI_STEPS_V2_SIGNAL_CURRENTCHANGED QStringLiteral("currentChanged")
#define PLS_UI_STEPS_V2_SIGNAL_CURRENTITEMCHANGED QStringLiteral("currentItemChanged")
#define PLS_UI_STEPS_V2_SIGNAL_CURRENTINDEXCHANGED QStringLiteral("currentIndexChanged")
#define PLS_UI_STEPS_V2_SIGNAL_SLIDERRELEASED QStringLiteral("sliderReleased")
#define PLS_UI_STEPS_V2_SIGNAL_CUSTOMCONTEXTMENUREQUESTED QStringLiteral("customContextMenuRequested")
#define PLS_UI_STEPS_V2_SIGNAL_VALUECHANGED QStringLiteral("valueChanged")
#define PLS_UI_STEPS_V2_SIGNAL_TABBARCLICKED QStringLiteral("tabBarClicked")

#define PLS_UI_STEPS_V2_ACTION_CLICK QStringLiteral("Click")
#define PLS_UI_STEPS_V2_ACTION_RCLICK QStringLiteral("RClick")
#define PLS_UI_STEPS_V2_ACTION_INPUT QStringLiteral("Input")
#define PLS_UI_STEPS_V2_ACTION_CHOOSE QStringLiteral("Choose")
#define PLS_UI_STEPS_V2_ACTION_SWITCH QStringLiteral("Switch")

#define PLS_UI_STEPS_V2_VALUE_CHECKED QStringLiteral("Checked")
#define PLS_UI_STEPS_V2_VALUE_UNCHECKED QStringLiteral("Unchecked")

#define PLS_DISABLE_UISTEP_V2(object) pls::StepV2Disabled PLS_CONCAT(stepV2Disabled, __COUNTER__)(object)

class LIBUI_API pls_language_key_t {
	QByteArray m_key;

public:
	pls_language_key_t() = default;
	explicit pls_language_key_t(const char *key) : m_key(key) {}
	explicit pls_language_key_t(const QByteArray &key) : m_key(key) {}

	operator QByteArray() const { return key(); }
	pls_language_key_t &operator=(const char *key) { return operator=(QByteArray(key)); }
	pls_language_key_t &operator=(const QByteArray &key);

	const QByteArray &key() const { return m_key; }
};
class LIBUI_API pls_text_t {
	using builder_t = std::function<QString(const QString &format)>;

	QByteArray m_key;
	mutable QString m_text;
	mutable QString m_english;
	mutable QString m_stage;
	builder_t m_builder = nullptr;

public:
	pls_text_t() = default;
	pls_text_t(const char *text) : m_text(QString::fromUtf8(text)) {}
	pls_text_t(const wchar_t *text) : m_text(QString::fromWCharArray(text)) {}
	pls_text_t(const std::string &text) : m_text(QString::fromStdString(text)) {}
	pls_text_t(const std::wstring &text) : m_text(QString::fromStdWString(text)) {}
	pls_text_t(const QString &text) : m_text(text) {}
	pls_text_t(const char *text, const pls_language_key_t &key) : m_key(key.key()), m_text(QString::fromUtf8(text)) {}
	pls_text_t(const wchar_t *text, const pls_language_key_t &key) : m_key(key.key()), m_text(QString::fromWCharArray(text)) {}
	pls_text_t(const std::string &text, const pls_language_key_t &key) : m_key(key.key()), m_text(QString::fromStdString(text)) {}
	pls_text_t(const std::wstring &text, const pls_language_key_t &key) : m_key(key.key()), m_text(QString::fromStdWString(text)) {}
	pls_text_t(const QString &text, const pls_language_key_t &key) : m_key(key.key()), m_text(text) {}
	pls_text_t(const QString &text, const QString &english, const QString &stage = QString()) : m_text(text), m_english(english), m_stage(stage) {}
	pls_text_t(const pls_language_key_t &key, const QString &stage = QString()) : m_key(key.key()), m_stage(stage) {}
	pls_text_t(const pls_language_key_t &key, builder_t &&builder, const QString &stage = QString()) : m_key(key.key()), m_stage(stage), m_builder(std::move(builder)) {}

	operator QString() const { return text(); }
	pls_text_t &operator=(const char *text) { return operator=(QString::fromUtf8(text)); }
	pls_text_t &operator=(const wchar_t *text) { return operator=(QString::fromWCharArray(text)); }
	pls_text_t &operator=(const std::string &text) { return operator=(QString::fromStdString(text)); }
	pls_text_t &operator=(const std::wstring &text) { return operator=(QString::fromStdWString(text)); }
	pls_text_t &operator=(const QString &text);
	pls_text_t &operator=(const pls_language_key_t &key);

	const QByteArray &key() const { return m_key; }
	const QString &stage() const { return m_stage; }
	QString text() const;
	QString english() const;
	QString build(const QString &format) const;
};
LIBUI_API QByteArray pls_formatv(const char *format, va_list args);
LIBUI_API QByteArray pls_format(const char *format, ...);
LIBUI_API void pls_uistep_v2_focus_changed(QWidget *old, QWidget *now);
LIBUI_API QString pls_uistep_v2_to_english_cb_default(const QString &text);
LIBUI_API void pls_uistep_v2_set_to_english_cb(std::function<QString(const QString &text)> &&to_english_cb);
LIBUI_API void pls_uistep_v2_add_to_english_cb(const QByteArray &plugin_id, std::function<QString(const QByteArray &plugin_id, const QString &text)> &&to_english_cb);
LIBUI_API void pls_uistep_v2_add_to_english_cb(const char *plugin_id, bool (*to_english_cb)(const char *kr_str, const char *plugin_id, const char **out));
LIBUI_API void pls_uistep_v2_remove_to_english_cb(const QByteArray &plugin_id);
LIBUI_API QString pls_uistep_v2_to_english(const QString &text);
LIBUI_API QString pls_uistep_v2_get_english_cb_default(const QByteArray &key);
LIBUI_API void pls_uistep_v2_set_get_english_cb(std::function<QString(const QByteArray &key)> &&get_english_cb);
LIBUI_API void pls_uistep_v2_add_get_english_cb(const QByteArray &plugin_id, std::function<QString(const QByteArray &plugin_id, const QByteArray &key)> &&get_english_cb);
LIBUI_API void pls_uistep_v2_remove_get_english_cb(const QByteArray &plugin_id);
LIBUI_API QString pls_uistep_v2_get_english(const QByteArray &key);
#if defined(PLS_UI_ACTION_STATS)
using pls_get_bytearray_t = std::function<QByteArray()>;
LIBUI_API std::optional<QByteArray> pls_uistep_v2_get_custom_show_hide_name(const QObject *object);
LIBUI_API void pls_uistep_v2_set_custom_show_hide_name(const QObject *object, const QByteArray &custom_show_hide_name);
LIBUI_API void pls_uistep_v2_set_custom_show_hide_name(const QObject *object, const pls_get_bytearray_t &custom_show_hide_name);
LIBUI_API std::optional<QByteArray> pls_uistep_v2_get_custom_enable_disable_name(const QObject *object);
LIBUI_API void pls_uistep_v2_set_custom_enable_disable_name(const QObject *object, const QByteArray &custom_enable_disable_name);
LIBUI_API void pls_uistep_v2_set_custom_enable_disable_name(const QObject *object, const pls_get_bytearray_t &custom_enable_disable_name);
LIBUI_API std::optional<QByteArray> pls_uistep_v2_get_custom_enter_leave_name(const QObject *object);
LIBUI_API void pls_uistep_v2_set_custom_enter_leave_name(const QObject *object, const QByteArray &custom_enter_leave_name);
LIBUI_API void pls_uistep_v2_set_custom_enter_leave_name(const QObject *object, const pls_get_bytearray_t &custom_enter_leave_name);
LIBUI_API std::optional<QByteArray> pls_uistep_v2_get_custom_hover_enter_leave_name(const QObject *object);
LIBUI_API void pls_uistep_v2_set_custom_hover_enter_leave_name(const QObject *object, const QByteArray &custom_hover_enter_leave_name);
LIBUI_API void pls_uistep_v2_set_custom_hover_enter_leave_name(const QObject *object, const pls_get_bytearray_t &custom_hover_enter_leave_name);
LIBUI_API bool pls_uistep_v2_window_state_is_listening(const QObject *object);
LIBUI_API void pls_uistep_v2_listen_window_state(QObject *object, bool listen);
LIBUI_API bool pls_uistep_v2_window_close_is_listening(const QObject *object);
LIBUI_API void pls_uistep_v2_listen_window_close(QObject *object, bool listen);
#else
#define pls_uistep_v2_set_custom_show_hide_name(...)
#define pls_uistep_v2_set_custom_enable_disable_name(...)
#define pls_uistep_v2_set_custom_enter_leave_name(...)
#define pls_uistep_v2_set_custom_hover_enter_leave_name(...)
#define pls_uistep_v2_listen_window_state(...)
#define pls_uistep_v2_listen_window_close(...)
#endif
using pls_get_enabled_t = std::function<bool()>;
using pls_get_text_t = std::function<QString()>;
LIBUI_API QString pls_uistep_v2_to_english(const QString &text, bool enabled, bool to_original_text = false);
LIBUI_API QString pls_uistep_v2_to_english(const QString &text, const pls_get_enabled_t &enabled, bool to_original_text = false);
LIBUI_API bool pls_uistep_v2_title_auto_to_english_enabled(QObject *object);
LIBUI_API void pls_uistep_v2_title_auto_to_english_enable(QObject *object, bool enabled);
LIBUI_API void pls_uistep_v2_title_auto_to_english_enable(QObject *object, const pls_get_enabled_t &enabled);
LIBUI_API QString pls_uistep_v2_get_title(QObject *object);
LIBUI_API void pls_uistep_v2_set_title(QObject *object, const QString &title, bool with_group = false, bool auto_to_english_enabled = true);
LIBUI_API void pls_uistep_v2_set_title(QObject *object, const pls_get_text_t &title, bool with_group = false, bool auto_to_english_enabled = true);
LIBUI_API bool pls_uistep_v2_enabled(QObject *object);
LIBUI_API void pls_uistep_v2_enable(QObject *object, bool enabled);
LIBUI_API bool pls_uistep_v2_enabled(QObject *object, const QString &signalName);
LIBUI_API void pls_uistep_v2_enable(QObject *object, const QString &signalName, bool enabled);
LIBUI_API void pls_uistep_v2_tab(const QObjectList &objects, const QString &signalName);
LIBUI_API void pls_uistep_v2_custom(QObject *object, const QString &signalName, const QString &action, const QVariantHash &attrs);
LIBUI_API bool pls_uistep_v2_name_auto_to_english_enabled(QObject *object, const QString &signalName);
LIBUI_API void pls_uistep_v2_name_auto_to_english_enable(QObject *object, const QString &signalName, bool enabled);
LIBUI_API void pls_uistep_v2_name_auto_to_english_enable(QObject *object, const QString &signalName, const pls_get_enabled_t &enabled);
LIBUI_API QString pls_uistep_v2_get_name(QObject *object, const QString &signalName);
LIBUI_API void pls_uistep_v2_set_name(QObject *object, const QString &signalName, const QString &name, bool auto_to_english_enabled = true);
LIBUI_API void pls_uistep_v2_set_name(QObject *object, const QString &signalName, const pls_get_text_t &name, bool auto_to_english_enabled = true);
LIBUI_API bool pls_uistep_v2_value_auto_to_english_enabled(QObject *object, const QString &signalName);
LIBUI_API void pls_uistep_v2_value_auto_to_english_enable(QObject *object, const QString &signalName, bool enabled);
LIBUI_API void pls_uistep_v2_value_auto_to_english_enable(QObject *object, const QString &signalName, const pls_get_enabled_t &enabled);
LIBUI_API QString pls_uistep_v2_get_value(QObject *object, const QString &signalName);
LIBUI_API void pls_uistep_v2_set_value(QObject *object, const QString &signalName, const QString &value, bool auto_to_english_enabled = true);
LIBUI_API void pls_uistep_v2_set_value(QObject *object, const QString &signalName, const pls_get_text_t &value, bool auto_to_english_enabled = true);
LIBUI_API QString pls_uistep_v2_auto_bind_get_name(QObject *name, QWidget *widget);
/** Remove auto-bind name getters from widget (e.g. before reusing in a new layout to avoid stale name callback). */
LIBUI_API void pls_uistep_v2_clear_auto_bind_name(QWidget *widget);
LIBUI_API void pls_uistep_v2_auto_bind(QWidget *widget, const std::function<QString(QObject *name, QWidget *widget)> &get_name = pls_uistep_v2_auto_bind_get_name);
LIBUI_API void pls_uistep_v2_bind(QWidget *widget, QObject *name, const std::function<QString(QObject *name, QWidget *widget)> &get_name = pls_uistep_v2_auto_bind_get_name);
LIBUI_API void pls_uistep_v2_bind(QWidget *widget, const QString &signalName, QObject *name, const std::function<QString(QObject *name, QWidget *widget)> &get_name = pls_uistep_v2_auto_bind_get_name);
LIBUI_API void pls_uistep_v2_bind(QWidget *widget, const QObjectList &names, const std::function<QString(QObject *name, QWidget *widget)> &get_name = pls_uistep_v2_auto_bind_get_name);
LIBUI_API void pls_uistep_v2_bind(QWidget *widget, const QString &signalName, const QObjectList &names,
				  const std::function<QString(QObject *name, QWidget *widget)> &get_name = pls_uistep_v2_auto_bind_get_name);
inline void pls_uistep_v2_custom(QObject *object, const QString &signalName, const QString &action)
{
	pls_uistep_v2_custom(object, signalName, action, QVariantHash{});
}
template<typename Name> void pls_uistep_v2_custom(QObject *object, const QString &signalName, const QString &action, const Name &name)
{
	pls_uistep_v2_custom(object, signalName, action);
	pls_uistep_v2_set_name(object, signalName, name);
}
template<typename Name, typename Value> void pls_uistep_v2_custom(QObject *object, const QString &signalName, const QString &action, const Name &name, const Value &value)
{
	pls_uistep_v2_custom(object, signalName, action);
	pls_uistep_v2_set_name(object, signalName, name);
	pls_uistep_v2_set_value(object, signalName, value);
}
inline void pls_uistep_v2_custom_button(QObject *object, const QString &signalName)
{
	pls_uistep_v2_custom(object, signalName, PLS_UI_STEPS_V2_ACTION_CLICK, PLS_UI_STEPS_V2_ATTR_VALUE_BUTTON);
}
template<typename Value> void pls_uistep_v2_custom_button(QObject *object, const QString &signalName, const Value &value)
{
	pls_uistep_v2_custom(object, signalName, PLS_UI_STEPS_V2_ACTION_CLICK, PLS_UI_STEPS_V2_ATTR_VALUE_BUTTON, value);
}
template<typename Name> void pls_uistep_v2_set_name(QObject *object, const Name &name)
{
	pls_uistep_v2_set_name(object, QStringLiteral("*"), name);
}
template<typename Value> void pls_uistep_v2_set_value(QObject *object, const Value &value)
{
	pls_uistep_v2_set_value(object, QStringLiteral("*"), value);
}
template<typename Name, typename Value> void pls_uistep_v2_set_info(QObject *object, const QString &signalName, const Name &name, const Value &value)
{
	pls_uistep_v2_set_name(object, signalName, name);
	pls_uistep_v2_set_value(object, signalName, value);
}
template<typename Name, typename Value> void pls_uistep_v2_set_info(QObject *object, const Name &name, const Value &value)
{
	pls_uistep_v2_set_info(object, QStringLiteral("*"), name, value);
}
template<typename Parent> Parent *pls_get_spec_parent(QObject *object, Parent *defval = nullptr)
{
	if (!object)
		return defval;

	for (auto i = object->parent(); i; i = i->parent())
		if (auto p = qobject_cast<Parent *>(i); p)
			return p;
	return defval;
}

#if defined(PLS_PERFORMANCE_STATS)
#include "PLSWatchers.h"
#define PLS_PERFORMANCE_END_WHEN_WIDGET_SHOW(widget, performance_ends /*call PLS_PERFORMANCE_END()*/) \
	QObject::connect(new PLSShowWatcher(widget), &PLSShowWatcher::signalShow, widget, [&]() { performance_ends });
#define PLS_PERFORMANCE_GLOBAL_END_WHEN_WIDGET_SHOW(widget, performance_ends /*call PLS_PERFORMANCE_END()*/) \
	QObject::connect(new PLSShowWatcher(widget), &PLSShowWatcher::signalShow, widget, [=]() { performance_ends; });
#else
#define PLS_PERFORMANCE_END_WHEN_WIDGET_SHOW(widget, /*call PLS_PERFORMANCE_END()*/...)
#define PLS_PERFORMANCE_GLOBAL_END_WHEN_WIDGET_SHOW(widget, /*call PLS_PERFORMANCE_END()*/...)
#endif

//loading app
LIBUI_API pls_process_t *pls_create_loading_app(const QString &prismSession, const QString &prismSubSession);
LIBUI_API void pls_destory_loading_app(pls_process_t *pro);

//move to common
class QScrollArea;
LIBUI_API void scroll_to_category_button(QPushButton *categoryButton, QScrollArea *scrollArea);

namespace pls {
struct ICloseDialog {
	virtual ~ICloseDialog() = default;
	virtual void closeNoButton() = 0;
};

struct LIBUI_API HotKeyLocker {
	HotKeyLocker();
	~HotKeyLocker();

	static void setHotKeyCb(const std::function<void()> &enableHotKeyCb, const std::function<void()> &disableHotKeyCb);

private:
	HotKeyLocker(const HotKeyLocker &) = delete;
	HotKeyLocker(const HotKeyLocker &&) noexcept = delete;
	void operator=(const HotKeyLocker &) = delete;
	void operator=(const HotKeyLocker &&) = delete;
};
class StepV2Disabled {
	QPointer<QObject> m_obeject;

public:
	explicit StepV2Disabled(QObject *object) : m_obeject(object)
	{
		if (m_obeject)
			pls_uistep_v2_enable(m_obeject, false);
	}
	~StepV2Disabled()
	{
		if (m_obeject)
			pls_uistep_v2_enable(m_obeject, true);
	}
};

struct QByteArrayToCString {
	QByteArray m_text;
	QByteArrayToCString(const QByteArray &text) : m_text(text) {}
	const char *c_str() const { return m_text.constData(); }
};
inline QByteArrayToCString to_cstring(const QString &s)
{
	return QByteArrayToCString(s.toUtf8());
}
inline QByteArrayToCString to_cstring(const QByteArray &s)
{
	return QByteArrayToCString(s);
}
struct QCStringToCString {
	const char *m_text;
	QCStringToCString(const char *text) : m_text(text) {}
	const char *c_str() const { return m_text; }
};
inline QCStringToCString to_cstring(const char *s)
{
	return QCStringToCString(s);
}
}

LIBUI_API QString pls_get_original_of_rich_text(const QString &rich_text);
template<typename Action> inline bool pls_uistep_v2(QObject *object, Action &&action, const QString &name, const QString &value)
{
	if (auto title = pls_uistep_v2_get_title(object); !title.isEmpty()) {
		PLS_LOGEX(PLS_LOG_UI_STEP, "[Operation]", {{PTS_LOG_TYPE, PTS_TYPE_UISTEP_STEP}}, "In [%s], %s [%s: %s]", pls::to_cstring(title).c_str(), pls::to_cstring(action).c_str(),
			  pls::to_cstring(name).c_str(), pls::to_cstring(value).c_str());
		return true;
	}
	return false;
}

#endif // COMMONLIBS_LIBUI_LIBUI_H
