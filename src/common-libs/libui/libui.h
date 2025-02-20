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
}

#endif // COMMONLIBS_LIBUI_LIBUI_H
