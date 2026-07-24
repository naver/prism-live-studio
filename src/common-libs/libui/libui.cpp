#include "libui.h"

#include <private/qobject_p.h>
#include <private/qmetaobject_p.h>
#include <qapplication.h>
#include <qstyle.h>
#include <qscreen.h>
#include <qhash.h>
#include <qsettings.h>
#include <qdir.h>
#include <qfile.h>
#include <qabstractbutton.h>
#include <qaction.h>
#include <qtranslator.h>
#include <qpainter.h>
#include <qclipboard.h>
#include <qframe.h>
#include <qtextlayout.h>
#include <qlineedit.h>
#include <qtabwidget.h>
#include <qlistwidget.h>
#include <qformlayout.h>
#include <qslider.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>
#include <qradiobutton.h>
#include <qgroupbox.h>

#include "PLSDialogView.h"
#include "pls-shared-values.h"
#include "pls-shared-functions.h"
#include "pls-common-define.hpp"
#include <libutils-api.h>
#include <QQRCoder.h>
#include <liblog.h>
#include <action.h>
#include <QQueue>

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <shellscalingapi.h>
#else
#include <sys/types.h>
#include "PLSCustomMacWindow.h"
#include "mac/PLSMacNotificationCenter.h"
#include "mac/PLSMacProductProcessMonitor.h"
#endif

#include "PLSAlertView.h"

#include "PLSWatchers.h"
#include "PLSCheckBox.h"
#include "PLSRadioButton.h"
#include "PLSComboBox.h"
#include "PLSEdit.h"
#include "PLSDialogButtonBox.h"
#include "PLSUIApp.h"

#include <QScrollArea>

namespace {
struct LocalGlobalVars {
	static QPointer<QWidget> g_main_view;
	static QList<QPointer<QDialog>> g_dialog_views;
	static QList<QPointer<QMenu>> g_menu_views;
	static std::atomic<uint64_t> g_main_window_closing;
	static std::atomic<uint64_t> g_main_window_destroyed;
	static const QList<QString> g_css_prefixs;
	static std::atomic<int> g_hotKeyLockerCount;
	static std::function<void()> g_enableHotKeyCb;
	static std::function<void()> g_disableHotKeyCb;
	static std::function<QString(const QString &text)> g_to_english_cb;
	static QList<QPair<QByteArray, std::function<QString(const QByteArray &plugin_id, const QString &text)>>> g_plugin_to_english_cbs;
	static std::function<QString(const QByteArray &key)> g_get_english_cb;
	static QList<QPair<QByteArray, std::function<QString(const QByteArray &plugin_id, const QByteArray &key)>>> g_plugin_get_english_cbs;
};
QPointer<QWidget> LocalGlobalVars::g_main_view;
QList<QPointer<QDialog>> LocalGlobalVars::g_dialog_views;
QList<QPointer<QMenu>> LocalGlobalVars::g_menu_views;
std::atomic<uint64_t> LocalGlobalVars::g_main_window_closing = 0;
std::atomic<uint64_t> LocalGlobalVars::g_main_window_destroyed = 0;
const QList<QString> LocalGlobalVars::g_css_prefixs{QStringLiteral(":/resource/css/%1.css"), QStringLiteral(":/css/%1.css")};
std::atomic<int> LocalGlobalVars::g_hotKeyLockerCount = 0;
std::function<void()> LocalGlobalVars::g_enableHotKeyCb = nullptr;
std::function<void()> LocalGlobalVars::g_disableHotKeyCb = nullptr;
std::function<QString(const QString &text)> LocalGlobalVars::g_to_english_cb = pls_uistep_v2_to_english_cb_default;
QList<QPair<QByteArray, std::function<QString(const QByteArray &plugin_id, const QString &text)>>> LocalGlobalVars::g_plugin_to_english_cbs;
std::function<QString(const QByteArray &key)> LocalGlobalVars::g_get_english_cb = pls_uistep_v2_get_english_cb_default;
QList<QPair<QByteArray, std::function<QString(const QByteArray &plugin_id, const QByteArray &key)>>> LocalGlobalVars::g_plugin_get_english_cbs;

class SignalSpyCallback {
	enum class CallerType { AbstractButton, Action, CustomButton, CustomControl };

	static const QByteArray s_clickedMethod;
	static const QByteArray s_triggeredMethod;

public:
	SignalSpyCallback() { qt_register_signal_spy_callbacks(&m_signalSpyCallbackSet); }
	~SignalSpyCallback() {}

	static void signalBeginCallback(QObject *caller, int signal_or_method_index, void **argv)
	{
		pls_check_app_exiting();
		if (!caller->inherits("QWidget") && !caller->inherits("QAction"))
			return;

		/*pls_unused(argv);
		QMetaMethod method;
		QString action;
		if (caller && needUiStep(method, action, caller, signal_or_method_index)) {
			PLS_UI_STEP(getModuleName(caller).toUtf8().constData(), getControls(caller).toUtf8().constData(), action.toUtf8().constData());
		}*/

		if (caller && pls_uistep_v2_enabled(caller)) {
			uiStepV2(caller, signal_or_method_index, argv);
		}
	}
	static void slotBeginCallback(QObject *caller, int signal_or_method_index, void **argv) {}
	static void signalEndCallback(QObject *caller, int signal_or_method_index) {}
	static void slotEndCallback(QObject *caller, int signal_or_method_index) {}

	static bool getBool(const QObject *object, const char *name, bool defaultValue = false)
	{
		if (auto value = object->property(name); value.type() == QVariant::Bool)
			return value.toBool();
		return defaultValue;
	}
	static QString getString(const QObject *object, const char *name, const QString &defaultValue = QString())
	{
		if (auto value = object->property(name); value.type() == QVariant::String)
			return value.toString();
		return defaultValue;
	}
	static QByteArray getUtf8(const QObject *object, const char *name, const QByteArray &defaultValue = QByteArray())
	{
		if (auto value = object->property(name); value.type() == QVariant::String)
			return value.toString().toUtf8();
		else if (value.type() == QVariant::ByteArray)
			return value.toByteArray();
		return defaultValue;
	}
	static QByteArray getUtf8(const QVariantHash &object, const QString &name, const QByteArray &defaultValue = QByteArray())
	{
		if (auto value = object[name]; value.type() == QVariant::String)
			return value.toString().toUtf8();
		else if (value.type() == QVariant::ByteArray)
			return value.toByteArray();
		return defaultValue;
	}
	static bool needUiStep(QMetaMethod &method, QString &action, QObject *caller, int signal_or_method_index)
	{
		auto mo = caller->metaObject();
		if (mo->inherits(&QAbstractButton::staticMetaObject)) {
			return isButton(CallerType::AbstractButton, method, action, caller, signal_or_method_index, getUtf8(caller, "ui-step.signalName", s_clickedMethod));
		} else if (mo->inherits(&QAction::staticMetaObject)) {
			return isButton(CallerType::Action, method, action, caller, signal_or_method_index, getUtf8(caller, "ui-step.signalName", s_triggeredMethod));
		} else if (getBool(caller, "ui-step.customButton")) {
			return isButton(CallerType::CustomButton, method, action, caller, signal_or_method_index, getUtf8(caller, "ui-step.signalName", s_clickedMethod));
		} else if (getBool(caller, "ui-step.customControl")) {
			return isCustomControl(CallerType::CustomControl, method, action, caller, signal_or_method_index, getUtf8(caller, "ui-step.signalName", s_clickedMethod));
		}
		extUiStep(caller, signal_or_method_index);
		return false;
	}
	static bool isButton(CallerType callerType, QMetaMethod &method, QString &action, QObject *caller, int signal_or_method_index, const QByteArray &name)
	{
		if (method = QMetaObjectPrivate::signal(caller->metaObject(), signal_or_method_index); method.name() == name) {
			action = getString(caller, "ui-step.action", ACTION_CLICK);
			if (auto actionData = getActionData(callerType, caller); !actionData.isEmpty()) {
				action.append(" <");
				action.append(actionData);
				action.append(">");
			}
			return true;
		}
		extUiStep(caller, signal_or_method_index);
		return false;
	}
	static bool isCustomControl(CallerType callerType, QMetaMethod &method, QString &action, QObject *caller, int signal_or_method_index, const QByteArray &name)
	{
		if (method = QMetaObjectPrivate::signal(caller->metaObject(), signal_or_method_index); method.name() == name) {
			action = getString(caller, "ui-step.action", QString::fromUtf8(name));
			if (auto actionData = getActionData(callerType, caller); !actionData.isEmpty()) {
				action.append(" <");
				action.append(actionData);
				action.append(">");
			}
			return true;
		}
		extUiStep(caller, signal_or_method_index);
		return false;
	}
	static QString getModuleName(QObject *caller, const QString &defModuleName = QString())
	{
		if (!defModuleName.isEmpty())
			return defModuleName;

		for (auto object = caller; object; object = object->parent()) {
			if (QString moduleName = getString(caller, "ui-step.moduleName"); !moduleName.isEmpty()) {
				return moduleName;
			} else if (moduleName = getString(caller, "log.moduleName"); !moduleName.isEmpty()) {
				return moduleName;
			}

			if (isToplevel(object)) {
				break;
			}
		}
		return QStringLiteral("libui");
	}
	static QString getControls(QObject *caller)
	{
		return getControls(caller, getString(caller, "ui-step.controlFullname"), getString(caller, "ui-step.controls"), getString(caller, "ui-step.additional"));
	}
	static QString getControls(QObject *caller, const QString &controlFullname, const QString &controls, const QString &additional)
	{
		if (!controlFullname.isEmpty()) {
			return controlFullname;
		}

		QString fullObjectName = getFullObjectName(caller);
		if (!controls.isEmpty()) {
			fullObjectName.append(' ');
			fullObjectName.append(controls);
		}

		if (!additional.isEmpty()) {
			fullObjectName.append(' ');
			fullObjectName.append(additional);
		}

		return fullObjectName;
	}
	static QString getFullObjectName(QObject *caller)
	{
		QString fullObjectName;
		for (auto object = caller; object; object = object->parent()) {
			QString objectName = QString::fromUtf8(object->metaObject()->className()) + '[' + object->objectName() + ']';
			if (!fullObjectName.isEmpty()) {
				objectName.append(QStringLiteral("->"));
				objectName.append(fullObjectName);
			}

			fullObjectName = objectName;

			if (isToplevel(object)) {
				break;
			}
		}
		return fullObjectName;
	}
	static bool isToplevel(QObject *caller)
	{ //
		return caller->isWidgetType() && pls_is_toplevel_view(static_cast<QWidget *>(caller));
	}
	static QString getActionData(CallerType callerType, QObject *caller)
	{
		if (auto actionData = pls_call_object_getter(caller, "ui-step.getActionData").toString(); !actionData.isEmpty()) {
			return actionData;
		}

		switch (callerType) {
		case CallerType::AbstractButton:
			if (auto mo = caller->metaObject(); mo->inherits(&QCheckBox::staticMetaObject))
				return static_cast<QCheckBox *>(caller)->isChecked() ? QStringLiteral("checked") : QStringLiteral("unchecked");
			return QString();
		case CallerType::Action:
			return static_cast<QAction *>(caller)->text();
		case CallerType::CustomButton:
			return QString();
		default:
			return QString();
		}
	}
	static void extUiStep(QObject *caller, int signal_or_method_index)
	{
		auto vuss = caller->property("ui-steps");
		if (vuss.type() != QVariant::Hash)
			return;

		auto method = QMetaObjectPrivate::signal(caller->metaObject(), signal_or_method_index);
		auto signalName = QString::fromUtf8(method.name());

		auto uss = vuss.toHash();
		auto iter = uss.find(signalName);
		if (iter == uss.end())
			return;

		auto us = iter.value().toHash();
		bool customButton = us[QStringLiteral("customButton")].toBool(), customControl = us[QStringLiteral("customControl")].toBool();
		if (!(customButton || customControl))
			return;

		auto moduleName = getModuleName(caller, us[QStringLiteral("moduleName")].toString());
		auto controlFullname = getControls(caller, us[QStringLiteral("controlFullname")].toString(), us[QStringLiteral("controls")].toString(), us[QStringLiteral("additional")].toString());
		auto action = us[QStringLiteral("action")].toString();
		auto actionData = pls_call_object_getter(caller, QString("ui-steps.%1.getActionData").arg(signalName).toUtf8()).toString();
		if (!actionData.isEmpty()) {
			action.append(" <");
			action.append(actionData);
			action.append(">");
		}

		PLS_UI_STEP(moduleName.toUtf8().constData(), controlFullname.toUtf8().constData(), action.toUtf8().constData());
	}

	static bool uiStepV2(QObject *caller, int signal_or_method_index, void **argv)
	{
		if (QString signalName; !isSignalEnabled(signalName, caller, signal_or_method_index)) {
			return false;
		} else if (auto cs = getUiStepV2ControlSignal(caller); extUiStepV2(signalName, caller, signal_or_method_index, cs, argv)) {
			return true;
		} else if (cs && std::get<0>(cs.value()) == signalName) {
			return pls_uistep_v2(caller, std::get<1>(cs.value()), getUiStepV2Name(caller, signalName, std::get<2>(cs.value())),
					     getUiStepV2Value(caller, signalName, [caller, value = std::get<3>(cs.value()), argv]() { return value(caller, argv); }));
		} else if (signalName == PLS_UI_STEPS_V2_SIGNAL_CUSTOMCONTEXTMENUREQUESTED) {
			return pls_uistep_v2(caller, PLS_UI_STEPS_V2_ACTION_RCLICK, getUiStepV2Name(caller, signalName), getUiStepV2Value(caller, signalName));
		}
		return false;
	}
	static void uiStepV2FocusChanged(QWidget *old, QWidget *now)
	{
		if (!old || !pls_uistep_v2_enabled(old))
			return;
		else if (auto ics = getUiStepV2InputControlSignal(old); ics) {
			pls_uistep_v2(old, std::get<0>(ics.value()), getUiStepV2Name(old, QStringLiteral("*"), std::get<1>(ics.value())),
				      getUiStepV2Value(old, QStringLiteral("*"), [old, value = std::get<2>(ics.value())]() { return value(old); }));
		}
	}
	static bool isSignalEnabled(QString &signalName, QObject *caller, int signal_or_method_index)
	{
		if (signalName.isEmpty()) {
			signalName = QString::fromUtf8(QMetaObjectPrivate::signal(caller->metaObject(), signal_or_method_index).name());
		}
		return pls_uistep_v2_enabled(caller, signalName);
	}
	template<typename T> static T getArg(void **argv, int index, const T &defval)
	{
		if (argv && argv[index])
			return *(T *)(argv[index]);
		return defval;
	}
	static bool extUiStepV2(const QString &signalName, QObject *caller, int signal_or_method_index,
				const std::optional<std::tuple<QString, QString, QString, std::function<QString(QObject *, void **argv)>>> &cs, void **argv)
	{
		auto vus = caller->property(PLS_UI_STEPS_V2);
		if (vus.type() != QVariant::Hash)
			return false;

		auto us = vus.toHash();
		auto iter = us.find(signalName);
		if (iter == us.end())
			return false;

		auto action = iter.value().toString();
		auto attrs = us[signalName + QStringLiteral(".attrs")].toHash();
		auto name = getUiStepV2Name(cs, caller, signalName, attrs);
		auto value = getUiStepV2Value(cs, caller, signalName, attrs, argv);
		pls_uistep_v2(caller, action, name, value);
		return true;
	}
	static std::optional<std::tuple<QString, QString, QString, std::function<QString(QObject *, void **argv)>>> getUiStepV2ControlSignal(QObject *object)
	{
		if (auto mo = object->metaObject(); mo->inherits(&QPushButton::staticMetaObject) || mo->inherits(&QToolButton::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_SIGNAL_CLICKED, PLS_UI_STEPS_V2_ACTION_CLICK, QStringLiteral("button"),
					       [](QObject *object, void **argv) { return getUiStepV2ButtonText(object); });
		} else if (mo->inherits(&QAction::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_SIGNAL_TRIGGERED, PLS_UI_STEPS_V2_ACTION_CLICK, QStringLiteral("menu"), [](QObject *object, void **argv) {
				return pls_uistep_v2_to_english(getUiStepV2WidgetText<QAction>(object), pls_uistep_v2_value_auto_to_english_enabled(object, PLS_UI_STEPS_V2_SIGNAL_TRIGGERED));
			});
		} else if (mo->inherits(&PLSCheckBox::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_SIGNAL_TOGGLED, PLS_UI_STEPS_V2_ACTION_CHOOSE, QStringLiteral("checkbox"), [](QObject *object, void **argv) {
				return static_cast<PLSCheckBox *>(object)->isChecked() ? PLS_UI_STEPS_V2_VALUE_CHECKED : PLS_UI_STEPS_V2_VALUE_UNCHECKED;
			});
		} else if (mo->inherits(&QCheckBox::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_SIGNAL_TOGGLED, PLS_UI_STEPS_V2_ACTION_CHOOSE, QStringLiteral("checkbox"), [](QObject *object, void **argv) {
				return static_cast<QCheckBox *>(object)->isChecked() ? PLS_UI_STEPS_V2_VALUE_CHECKED : PLS_UI_STEPS_V2_VALUE_UNCHECKED;
			});
		} else if (mo->inherits(&PLSRadioButton::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_SIGNAL_TOGGLED, PLS_UI_STEPS_V2_ACTION_CHOOSE, QStringLiteral("radiobutton"), [](QObject *object, void **argv) {
				return pls_uistep_v2_to_english(getUiStepV2WidgetText<PLSRadioButton>(object), pls_uistep_v2_value_auto_to_english_enabled(object, PLS_UI_STEPS_V2_SIGNAL_TOGGLED));
			});
		} else if (mo->inherits(&QRadioButton::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_SIGNAL_TOGGLED, PLS_UI_STEPS_V2_ACTION_CHOOSE, QStringLiteral("radiobutton"), [](QObject *object, void **argv) {
				return pls_uistep_v2_to_english(getUiStepV2WidgetText<QRadioButton>(object), pls_uistep_v2_value_auto_to_english_enabled(object, PLS_UI_STEPS_V2_SIGNAL_TOGGLED));
			});
		} else if (mo->inherits(&QTabWidget::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_SIGNAL_TABBARCLICKED, PLS_UI_STEPS_V2_ACTION_CHOOSE, QStringLiteral("tab"), [](QObject *object, void **argv) {
				return pls_uistep_v2_to_english(getUiStepV2TabItemText(object, argv), pls_uistep_v2_value_auto_to_english_enabled(object, PLS_UI_STEPS_V2_SIGNAL_CURRENTCHANGED));
			});
		} else if (mo->inherits(&QListWidget::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_SIGNAL_CURRENTITEMCHANGED, PLS_UI_STEPS_V2_ACTION_CHOOSE, QStringLiteral("list"), [](QObject *object, void **argv) {
				return pls_uistep_v2_to_english(getUiStepV2ListItemText(object, argv), pls_uistep_v2_value_auto_to_english_enabled(object, PLS_UI_STEPS_V2_SIGNAL_CURRENTITEMCHANGED));
			});
		} else if (mo->inherits(&PLSComboBox::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_SIGNAL_CURRENTINDEXCHANGED, PLS_UI_STEPS_V2_ACTION_CHOOSE, QStringLiteral("combobox"),
					       [](QObject *object, void **argv) { return getUiStepV2ComboBoxText<PLSComboBox>(object, PLS_UI_STEPS_V2_SIGNAL_CURRENTINDEXCHANGED, argv); });
		} else if (mo->inherits(&QComboBox::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_SIGNAL_CURRENTINDEXCHANGED, PLS_UI_STEPS_V2_ACTION_CHOOSE, QStringLiteral("combobox"),
					       [](QObject *object, void **argv) { return getUiStepV2ComboBoxText<QComboBox>(object, PLS_UI_STEPS_V2_SIGNAL_CURRENTINDEXCHANGED, argv); });
		} else if (mo->inherits(&QSlider::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_SIGNAL_SLIDERRELEASED, PLS_UI_STEPS_V2_ACTION_CHOOSE, QStringLiteral("slider"),
					       [](QObject *object, void **argv) { return QString::number(static_cast<QSlider *>(object)->value()); });
		} else {
			return std::nullopt;
		}
	}
	static std::optional<std::tuple<QString, QString, std::function<QString(QObject *)>>> getUiStepV2InputControlSignal(QObject *object)
	{
		if (auto mo = object->metaObject(); mo->inherits(&PLSLineEdit::staticMetaObject) || mo->inherits(&QLineEdit::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_ACTION_INPUT, QStringLiteral("lineedit"), [](QObject *object) { return getUiStepV2WidgetText<QLineEdit>(object); });
		} else if (mo->inherits(&PLSTextEdit::staticMetaObject) || mo->inherits(&QTextEdit::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_ACTION_INPUT, QStringLiteral("textedit"), [](QObject *object) { return getUiStepV2WidgetPlainText<QTextEdit>(object); });
		} else if (mo->inherits(&PLSPlainTextEdit::staticMetaObject) || mo->inherits(&QPlainTextEdit::staticMetaObject)) {
			return std::make_tuple(PLS_UI_STEPS_V2_ACTION_INPUT, QStringLiteral("plaintextedit"), [](QObject *object) { return getUiStepV2WidgetPlainText<QPlainTextEdit>(object); });
		} else {
			return std::nullopt;
		}
	}
	static QString getUiStepV2ButtonText(QObject *object) { return getUiStepV2DialogButtonBoxButtonValue(static_cast<QAbstractButton *>(object)); }
	template<typename T> static QString getUiStepV2WidgetText(QObject *object) { return static_cast<T *>(object)->text(); }
	template<typename T> static QString getUiStepV2WidgetPlainText(QObject *object) { return static_cast<T *>(object)->toPlainText(); }
	static QString getUiStepV2TabItemText(QObject *object, void **argv)
	{
		auto widget = static_cast<QTabWidget *>(object);
		return widget->tabText(getArg<int>(argv, 1, widget->currentIndex()));
	}
	static QString getUiStepV2ListItemText(QObject *object, void **argv)
	{
		auto widget = static_cast<QListWidget *>(object);
		if (auto item = getArg<QListWidgetItem *>(argv, 1, widget->currentItem()); item)
			return item->text();
		return QString();
	}
	template<typename T> static QString getUiStepV2ComboBoxText(QObject *object, const QString &signalName, void **argv)
	{
		auto widget = static_cast<T *>(object);
		return pls_uistep_v2_to_english(widget->itemText(getArg<int>(argv, 1, widget->currentIndex())), pls_uistep_v2_value_auto_to_english_enabled(object, signalName), false);
	}
	static QString getUiStepV2DialogButtonBoxButtonValue(QAbstractButton *button)
	{
		if (auto box = pls_get_spec_parent<PLSDialogButtonBox>(button); box)
			return getUiStepV2DialogButtonBoxButtonValue(button, box->standardButton(button));
		else if (auto qbox = pls_get_spec_parent<QDialogButtonBox>(button); qbox)
			return getUiStepV2DialogButtonBoxButtonValue(button, qbox->standardButton(button));
		return getUiStepV2ButtonValue(button);
	}
	static QString getUiStepV2DialogButtonBoxButtonValue(QAbstractButton *button, QDialogButtonBox::StandardButton id)
	{
		switch (id) {
		case QDialogButtonBox::NoButton:
		default:
			return getUiStepV2ButtonValue(button);
		case QDialogButtonBox::Ok:
			return QStringLiteral("OK");
		case QDialogButtonBox::Save:
			return QStringLiteral("Save");
		case QDialogButtonBox::SaveAll:
			return QStringLiteral("Save all");
		case QDialogButtonBox::Open:
			return QStringLiteral("Open");
		case QDialogButtonBox::Yes:
			return QStringLiteral("Yes");
		case QDialogButtonBox::YesToAll:
			return QStringLiteral("Yes to all");
		case QDialogButtonBox::No:
			return QStringLiteral("No");
		case QDialogButtonBox::NoToAll:
			return QStringLiteral("No to all");
		case QDialogButtonBox::Abort:
			return QStringLiteral("Abort");
		case QDialogButtonBox::Retry:
			return QStringLiteral("Retry");
		case QDialogButtonBox::Ignore:
			return QStringLiteral("Ignore");
		case QDialogButtonBox::Close:
			return QStringLiteral("Close");
		case QDialogButtonBox::Cancel:
			return QStringLiteral("Cancel");
		case QDialogButtonBox::Discard:
			return QStringLiteral("Discard");
		case QDialogButtonBox::Help:
			return QStringLiteral("Help");
		case QDialogButtonBox::Apply:
			return QStringLiteral("Apply");
		case QDialogButtonBox::Reset:
			return QStringLiteral("Reset");
		case QDialogButtonBox::RestoreDefaults:
			return QStringLiteral("Restore defaults");
		}
	}
	static QString getUiStepV2ButtonValue(QAbstractButton *button)
	{
		if (auto text = button->text(); !text.isEmpty())
			return pls_uistep_v2_to_english(text, pls_uistep_v2_value_auto_to_english_enabled(button, PLS_UI_STEPS_V2_SIGNAL_CLICKED));
		else if (text = button->toolTip(); !text.isEmpty())
			return pls_uistep_v2_to_english(text, pls_uistep_v2_value_auto_to_english_enabled(button, PLS_UI_STEPS_V2_SIGNAL_CLICKED));
		for (auto child : button->children()) {
			if (!child->isWidgetType())
				continue;
			else if (auto label = dynamic_cast<QLabel *>(child); !label)
				continue;
			else if (auto text = label->text(); !text.isEmpty())
				return pls_uistep_v2_to_english(text, pls_uistep_v2_value_auto_to_english_enabled(button, PLS_UI_STEPS_V2_SIGNAL_CLICKED));
		}
		return QString();
	}
	static QString getUiStepV2Name(const std::optional<std::tuple<QString, QString, QString, std::function<QString(QObject *, void **argv)>>> &cs, QObject *caller, const QString &signalName,
				       const QVariantHash &attrs)
	{
		if (auto defval = getUiStepV2GetCustomDefName(attrs); !defval.isEmpty())
			return getUiStepV2Name(caller, signalName, defval);
		else if (cs)
			return getUiStepV2Name(caller, signalName, std::get<2>(cs.value()));
		return getUiStepV2Name(caller, signalName);
	}
	static QString getUiStepV2Name(QObject *caller, const QString &signalName, const QString &defval = QString())
	{
		if (auto name = caller->property(PLS_UI_STEPS_V2_NAME).toString(); !name.isEmpty())
			return pls_uistep_v2_to_english(name, pls_uistep_v2_name_auto_to_english_enabled(caller, signalName), true);
		else if (name = pls_call_object_getter(caller, QStringLiteral(PLS_UI_STEPS_V2_DYN_NAME).arg(signalName).toUtf8()).toString(); !name.isEmpty())
			return pls_uistep_v2_to_english(name, pls_uistep_v2_name_auto_to_english_enabled(caller, signalName), true);
		else if (name = getUiStepV2AllName(caller, signalName); !name.isEmpty())
			return getUiStepV2NameWithSubName(caller, name, signalName);
		else if (name = getUiStepV2SubName(caller, signalName); !name.isEmpty())
			return name;
		return defval;
	}
	static QString getUiStepV2NameWithSubName(QObject *caller, const QString &name, const QString &signalName)
	{
		if (auto subname = getUiStepV2SubName(caller, signalName); !subname.isEmpty())
			return name + QStringLiteral(" > ") + subname;
		return name;
	}
	static QString getUiStepV2AllName(QObject *caller, const QString &signalName)
	{
		if (auto name = pls_call_object_getter(caller, QByteArrayLiteral(PLS_UI_STEPS_V2_ALL_NAME)).toString(); !name.isEmpty())
			return pls_uistep_v2_to_english(name, pls_uistep_v2_name_auto_to_english_enabled(caller, signalName), true);
		else if (auto parent = caller->parent(); parent)
			return getUiStepV2AllName(parent, signalName);
		return QString();
	}
	static QString getUiStepV2Name(QObject *caller, const QString &signalName, std::function<QString()> &&defval)
	{
		if (auto name = getUiStepV2Name(caller, signalName); !name.isEmpty())
			return name;
		return defval();
	}
	static QString getUiStepV2SubName(QObject *caller, const QString &signalName)
	{
		if (auto mo = caller->metaObject(); mo->inherits(&PLSCheckBox::staticMetaObject))
			return pls_uistep_v2_to_english(static_cast<PLSCheckBox *>(caller)->text(), pls_uistep_v2_name_auto_to_english_enabled(caller, signalName), true);
		return QString();
	}
	static QString getUiStepV2GetCustomDefName(const QVariantHash &attrs) { return attrs[QStringLiteral("name")].toString(); }
	static QString getUiStepV2Value(const std::optional<std::tuple<QString, QString, QString, std::function<QString(QObject *, void **argv)>>> &cs, QObject *caller, const QString &signalName,
					const QVariantHash &attrs, void **argv)
	{
		if (auto defval = getUiStepV2GetCustomDefValue(attrs); !defval.isEmpty())
			return getUiStepV2Value(caller, signalName, [defval]() { return defval; });
		else if (cs)
			return getUiStepV2Value(caller, signalName, [caller, value = std::get<3>(cs.value()), argv]() { return value(caller, argv); });
		return getUiStepV2Value(caller, signalName);
	}
	static QString getUiStepV2Value(QObject *caller, const QString &signalName, std::function<QString()> &&defval = nullptr)
	{
		if (auto value = caller->property(PLS_UI_STEPS_V2_VALUE).toString(); !value.isEmpty())
			return pls_uistep_v2_to_english(value, pls_uistep_v2_value_auto_to_english_enabled(caller, signalName), false);
		else if (value = pls_call_object_getter(caller, QStringLiteral(PLS_UI_STEPS_V2_DYN_VALUE).arg(signalName).toUtf8()).toString(); !value.isEmpty())
			return pls_uistep_v2_to_english(value, pls_uistep_v2_value_auto_to_english_enabled(caller, signalName), false);
		else if (value = getUiStepV2AllValue(caller, signalName); !value.isEmpty())
			return value;
		else if (defval)
			return defval();
		return QString();
	}
	static QString getUiStepV2AllValue(QObject *caller, const QString &signalName)
	{
		if (auto value = pls_call_object_getter(caller, QByteArrayLiteral(PLS_UI_STEPS_V2_ALL_VALUE)).toString(); !value.isEmpty())
			return pls_uistep_v2_to_english(value, pls_uistep_v2_value_auto_to_english_enabled(caller, signalName), false);
		else if (auto parent = caller->parent(); parent)
			return getUiStepV2AllValue(parent, signalName);
		return QString();
	}
	static QString getUiStepV2GetCustomDefValue(const QVariantHash &attrs) { return attrs[QStringLiteral("value")].toString(); }

	QSignalSpyCallbackSet m_signalSpyCallbackSet{signalBeginCallback, slotBeginCallback, signalEndCallback, slotEndCallback};
};

const QByteArray SignalSpyCallback::s_clickedMethod("clicked");
const QByteArray SignalSpyCallback::s_triggeredMethod("triggered");

class Translator : public QTranslator {
public:
	Translator(QObject *parent = nullptr) : QTranslator(parent) {}

public:
	QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr, int n = -1) const override
	{
		if (pls_is_empty(sourceText))
			return {};

		QString key = QString::fromUtf8(sourceText).toLower();
		if (auto iter = m_texts.find(key); iter != m_texts.end()) {
			return iter.value();
		}
		return {};
	}
	bool isEmpty() const override
	{
		return m_texts.isEmpty(); //
	}

public:
	QHash<QString, QString> m_texts;
};

class Initializer {
public:
	Initializer() {}
	static Initializer *initializer() { return &pls::Initializer<Initializer>::s_initializer; }

private:
	const SignalSpyCallback m_signalSpyCallback;
};
}

LIBUI_API QPointer<QWidget> pls_get_main_view()
{
	return LocalGlobalVars::g_main_view;
}
LIBUI_API void pls_set_main_view(const QPointer<QWidget> &main_view)
{
	LocalGlobalVars::g_main_view = main_view;
}

LIBUI_API bool pls_is_main_window_closing()
{
	return LocalGlobalVars::g_main_window_closing == 1;
}
LIBUI_API void pls_set_main_window_closing(bool main_window_closing)
{
	LocalGlobalVars::g_main_window_closing = main_window_closing ? 1 : 0;
}

LIBUI_API bool pls_is_main_window_destroyed()
{
	return LocalGlobalVars::g_main_window_destroyed == 1;
}
LIBUI_API void pls_set_main_window_destroyed(bool main_window_destroyed)
{
	LocalGlobalVars::g_main_window_destroyed = main_window_destroyed ? 1 : 0;
}

LIBUI_API bool pls_is_toplevel_view(QWidget *widget)
{
	if (!widget->isWindow()) {
		return false;
	} else if (dynamic_cast<PLSToplevelWidget *>(widget)) {
		return true;
	}
	return false;
}
LIBUI_API QWidget *pls_get_toplevel_view(QWidget *widget, QWidget *defval)
{
	if (!widget) {
		return defval;
	} else if (pls_is_toplevel_view(widget)) {
		return widget;
	}

	for (widget = widget->parentWidget(); widget; widget = widget->parentWidget()) {
		if (pls_is_toplevel_view(widget)) {
			return widget;
		}
	}
	return defval;
}

LIBUI_API void pls_push_modal_view(QDialog *dialog)
{
	if (!LocalGlobalVars::g_dialog_views.contains(dialog)) {
		LocalGlobalVars::g_dialog_views.append(dialog);
	}
}

LIBUI_API void pls_pop_modal_view(QDialog *dialog)
{
	LocalGlobalVars::g_dialog_views.removeAll(dialog);
}

LIBUI_API void pls_push_modal_view(QMenu *menu)
{
	if (!LocalGlobalVars::g_menu_views.contains(menu)) {
		LocalGlobalVars::g_menu_views.append(menu);
	}
}

LIBUI_API void pls_pop_modal_view(QMenu *menu)
{
	LocalGlobalVars::g_menu_views.removeAll(menu);
}

static void close_dialog(QDialog *dlg, bool close)
{
	if (auto closeDialog = dynamic_cast<pls::ICloseDialog *>(dlg); closeDialog) {
		closeDialog->closeNoButton();
	} else if (close) {
		dlg->close();
	} else {
		dlg->setProperty("exit", true);
		dlg->reject();
	}
}

LIBUI_API void pls_notify_close_modal_views()
{
	QList<QDialog *> dlgs;
	for (auto w : qApp->topLevelWidgets()) {
		if (auto dlg = dynamic_cast<QDialog *>(w); dlg && dlg->isModal() && !LocalGlobalVars::g_dialog_views.contains(dlg)) {
			PLS_INFO("libui", "close unmanaged Qt modal window, className: %s, objectName: %s", dlg->metaObject()->className(), dlg->objectName().toUtf8().constData());
			dlg->setParent(nullptr);
			dlgs.append(dlg);
		}
	}
	while (!dlgs.isEmpty()) {
		auto dlg = dlgs.takeLast();
		close_dialog(dlg, true);
	}

	while (!LocalGlobalVars::g_dialog_views.isEmpty()) {
		if (auto dlg = LocalGlobalVars::g_dialog_views.takeLast(); dlg) {
			PLS_INFO("libui", "close managed Qt modal window, className: %s, objectName: %s", dlg->metaObject()->className(), dlg->objectName().toUtf8().constData());
			dlg->setParent(nullptr);
			close_dialog(dlg, false);
		}
	}

	while (!LocalGlobalVars::g_menu_views.isEmpty()) {
		if (auto menu = LocalGlobalVars::g_menu_views.takeLast(); menu) {
			PLS_INFO("libui", "close managed Qt menu, className: %s, objectName: %s", menu->metaObject()->className(), menu->objectName().toUtf8().constData());
			menu->setParent(nullptr);
		}
	}
}

static bool _findParent(QObject *widget, QObject *cmpParentWidget)
{
	while (widget) {
		if (widget == cmpParentWidget) {
			return true;
		}
		widget = widget->parent();
	}
	return false;
}

LIBUI_API void pls_notify_close_modal_views_with_parent(QWidget *parent)
{
	QList<QDialog *> dlgs;
	for (auto w : qApp->topLevelWidgets()) {
		if (auto dlg = dynamic_cast<QDialog *>(w); dlg && dlg->isModal() && !LocalGlobalVars::g_dialog_views.contains(dlg) && _findParent(dlg->parent(), parent)) {
			PLS_INFO("libui", "close unmanaged Qt modal window, className: %s, objectName: %s", dlg->metaObject()->className(), dlg->objectName().toUtf8().constData());
			dlg->setParent(nullptr);
			dlgs.append(dlg);
		}
	}
	while (!dlgs.isEmpty()) {
		auto dlg = dlgs.takeLast();
		close_dialog(dlg, true);
	}

	for (auto i = LocalGlobalVars::g_dialog_views.size() - 1; i >= 0; --i) {
		if (auto dlg = LocalGlobalVars::g_dialog_views.at(i); dlg && _findParent(dlg->parent(), parent)) {
			PLS_INFO("libui", "close managed Qt modal window, className: %s, objectName: %s", dlg->metaObject()->className(), dlg->objectName().toUtf8().constData());
			dlg->setParent(nullptr);
			close_dialog(dlg, false);
			LocalGlobalVars::g_dialog_views.removeAt(i);
		}
	}

	for (auto i = LocalGlobalVars::g_menu_views.size() - 1; i >= 0; --i) {
		if (auto menu = LocalGlobalVars::g_menu_views.at(i); menu && _findParent(menu->parent(), parent)) {
			PLS_INFO("libui", "close managed Qt menu, className: %s, objectName: %s", menu->metaObject()->className(), menu->objectName().toUtf8().constData());
			menu->setParent(nullptr);
			LocalGlobalVars::g_menu_views.removeAt(i);
		}
	}
}
LIBUI_API bool pls_has_modal_view()
{
	return !LocalGlobalVars::g_dialog_views.isEmpty();
}
LIBUI_API QPointer<QDialog> pls_get_last_modal_view()
{
	if (pls_has_modal_view()) {
		return LocalGlobalVars::g_dialog_views.last();
	}
	return {};
}

LIBUI_API QString pls_load_css(const QStringList &cssNames)
{
	QString css;
	for (const auto &cssName : cssNames) {
		if (cssName.startsWith(QStringLiteral(":/"))) {
			QByteArray cssData;
			if (pls_read_data(cssData, cssName))
				css.append(QString::fromUtf8(cssData));
			continue;
		}

		for (const auto &prefix : LocalGlobalVars::g_css_prefixs) {
			QString cssFile = prefix.arg(cssName);
			QByteArray cssData;
			if (pls_read_data(cssData, cssFile)) {
				css.append(QString::fromUtf8(cssData));
				break;
			}
		}
	}
	return css;
}

LIBUI_API void pls_set_css(QWidget *widget, const QStringList &cssNames)
{
	widget->setProperty("$.QWidget.cssNames", cssNames);
	widget->setStyleSheet(pls_load_css(cssNames));
}
LIBUI_API void pls_add_css(QWidget *widget, const QStringList &cssNames)
{
	QStringList allCssNames = widget->property("$.QWidget.cssNames").toStringList();
	allCssNames.append(cssNames);
	pls_set_css(widget, allCssNames);
}
LIBUI_API void pls_set_global_css(const QStringList &cssNames, const QStringList &csses)
{
	qApp->setProperty("$.QWidget.csses", csses);
	qApp->setProperty("$.QWidget.cssNames", cssNames);
	qApp->setStyleSheet(csses.join(' ') + pls_load_css(cssNames));
}
LIBUI_API void pls_add_global_css(const QStringList &cssNames, const QStringList &csses)
{
	QStringList allCssNames = qApp->property("$.QWidget.cssNames").toStringList();
	QStringList allCsses = qApp->property("$.QWidget.csses").toStringList();
	allCssNames.append(cssNames);
	allCsses.append(csses);
	pls_set_global_css(allCssNames, allCsses);
}

LIBUI_API void pls_flush_style(QWidget *widget)
{
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
}
LIBUI_API void pls_flush_style_recursive(QWidget *widget, int recursiveDeep)
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
LIBUI_API void pls_flush_style(QWidget *widget, const char *propertyName, const QVariant &propertyValue)
{
	widget->setProperty(propertyName, propertyValue);
	pls_flush_style(widget);
}
LIBUI_API void pls_flush_style_if_visible(QWidget *widget, const char *propertyName, const QVariant &propertyValue)
{
	if (widget->isVisible()) {
		pls_flush_style(widget, propertyName, propertyValue);
	}
}
LIBUI_API void pls_flush_style_recursive(QWidget *widget, const char *propertyName, const QVariant &propertyValue, int recursiveDeep)
{
	widget->setProperty(propertyName, propertyValue);
	pls_flush_style_recursive(widget, recursiveDeep);
}

LIBUI_API void pls_scroll_area_clips_to_bounds(QWidget *widget, bool isClips)
{
#ifdef __APPLE__
	PLSCustomMacWindow::clipsToBounds(widget, isClips);
#endif
	//windows ignore this properties.
}

LIBUI_API QColor pls_qint64_to_qcolor(qint64 icolor)
{
	return QColor(icolor & 0xff, (icolor >> 8) & 0xff, (icolor >> 16) & 0xff, (icolor >> 24) & 0xff);
}
static qint64 shift(qint64 color, int shift)
{
	return (color & 0xff) << shift;
}
LIBUI_API qint64 pls_qcolor_to_qint64(const QColor &qcolor)
{
	qint64 icolor = qcolor.alpha();
	icolor = (icolor << 8) | qcolor.blue();
	icolor = (icolor << 8) | qcolor.green();
	icolor = (icolor << 8) | qcolor.red();
	return icolor;
}

LIBUI_API QPixmap pls_load_pixmap(const QString &imagePath, const QSize &size)
{
	if (imagePath.isEmpty()) {
		return QPixmap();
	}

	if (imagePath.toLower().endsWith(".svg")) {
		return pls_shared_paint_svg(imagePath, size);
	}

	QPixmap pix;
	if (!pix.load(imagePath))
		pix.load(imagePath, "PNG");
	return pix;
}

LIBUI_API QPixmap pls_load_pixmap_with_mode(const QString &imagePath, const QSize &size, Qt::AspectRatioMode ratioMode, Qt::TransformationMode transMode)
{
	return pls_load_pixmap(imagePath, size).scaled(size, ratioMode, transMode);
}

LIBUI_API QPixmap pls_rounded_pixmap(const QPixmap &pixmap, int radius, bool properties)
{
	QPixmap image(pixmap.size());
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QRect rect = image.rect();

	QPainterPath path;
	path.addRoundedRect(rect, radius, radius);
	painter.setClipPath(path);

	painter.fillRect(rect, properties ? QColor(39, 39, 39) : QColor(30, 30, 31));
	painter.drawPixmap(rect, pixmap);

	return image;
}

LIBUI_API QPixmap pls_selected_rounded_pixmap(const QSize &size, const QPixmap &crop, const QMargins &margin, int radius)
{
	QPixmap image(size);
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

	QRect rect = image.rect();
	painter.setPen(Qt::NoPen);
	painter.setBrush(Qt::yellow);
	painter.drawRoundedRect(rect, radius, radius);

	rect -= margin;
	painter.drawPixmap(rect, crop);

	return image;
}

static void insert_text(QHash<QString, QString> &texts, const QString &key, const QString &value)
{
	texts.insert(key.toLower(), value);
}
static void insert_text(QHash<QByteArray, QByteArray> &texts, const QString &key, const QString &value)
{
	texts.insert(key.toLower().toUtf8(), value.toUtf8());
	qDebug() << "insert_text: " << key << '=' << value;
}
template<typename Texts> static void add_text(Texts &texts, const QString &languageFile, const QString &relativeFileName)
{
	PLS_INFO("libui", "Load locale file %s", relativeFileName.toUtf8().constData());

	QSettings settings(languageFile, QSettings::IniFormat);
	for (const QString &key : settings.allKeys()) {
		QString value = pls_remove_quotes(settings.value(key).toString());
		value.replace("\\n", "\n").replace("\\\"", "\"");
		insert_text(texts, key, value);
	}
}
template<typename Texts> static void load_language(Texts &texts, const QString &languageDir, const QString &language, const QString &relativeFileName = QString())
{
	QDir dir(languageDir);
	auto fis = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsLast | QDir::Name);
	for (const auto &fi : fis) {
		QString relFileName;
		if (!relativeFileName.isEmpty()) {
			relFileName = relativeFileName + '/' + fi.fileName();
		} else {
			relFileName = fi.fileName();
		}

		if (fi.isDir()) {
			load_language(texts, fi.absoluteFilePath(), language, relFileName);
		} else if (fi.completeBaseName() == language) {
			add_text(texts, fi.absoluteFilePath(), relFileName);
		}
	}
}
LIBUI_API QHash<QString, QString> pls_load_language_values(const QString &languageDir, const QString &language, const QString &defaultLanguage)
{
	QHash<QString, QString> texts;
	load_language(texts, languageDir, defaultLanguage);
	if (language != defaultLanguage)
		load_language(texts, languageDir, language);
	return texts;
}
LIBUI_API QHash<QByteArray, QByteArray> pls_load_language_values_utf8(const QString &languageDir, const QString &language, const QString &defaultLanguage)
{
	QHash<QByteArray, QByteArray> texts;
	load_language(texts, languageDir, defaultLanguage);
	if (language != defaultLanguage)
		load_language(texts, languageDir, language);
	return texts;
}
LIBUI_API const QByteArray &pls_translate_language_utf8(const QHash<QByteArray, QByteArray> &texts, const QByteArray &key)
{
	if (auto iter = texts.find(key); iter != texts.end()) {
		return iter.value();
	}

	auto iter = pls_ref(texts).insert(key, key);
	return iter.value();
}
LIBUI_API QTranslator *pls_load_language_translator(const QString &languageDir, const QString &language, const QString &defaultLanguage)
{
	Translator *translator = pls_new<Translator>(qApp);
	translator->m_texts = pls_load_language_values(languageDir, language, defaultLanguage);
	return translator;
}
LIBUI_API void pls_load_language(const QString &languageDir, const QString &language, const QString &defaultLanguage)
{
	QTranslator *translator = pls_load_language_translator(languageDir, language, defaultLanguage);
	QApplication::installTranslator(translator);
}

#ifdef Q_OS_WIN
LIBUI_API QSize pls_get_win_cursor_size(QWidget *widget)
{
	if (!widget) {
		return QSize(0, 0);
	}
	qreal dpr = widget->devicePixelRatioF();
	QSettings settings(R"(HKEY_CURRENT_USER\Control Panel\Cursors)", QSettings::NativeFormat);
	int baseSize = settings.value("CursorBaseSize", 32).toUInt() * dpr;
	return QSize(32, 32) * (baseSize / 32.0f);
}

LIBUI_API void pls_flood_fill_color(QImage &image, const QColor &targetColor, const QColor &fillColor)
{
	if (targetColor == fillColor)
		return;

	const int w = image.width();
	const int h = image.height();
	QPoint seedPoint(w / 2, h / 2);
	if (image.pixelColor(seedPoint) != targetColor)
		return;

	QQueue<QPoint> queue;
	queue.enqueue(seedPoint);
	image.setPixelColor(seedPoint, fillColor);

	while (!queue.isEmpty()) {
		QPoint p = queue.dequeue();
		const QPoint neighbors[4] = {QPoint(p.x() + 1, p.y()), QPoint(p.x() - 1, p.y()), QPoint(p.x(), p.y() + 1), QPoint(p.x(), p.y() - 1)};
		for (const QPoint &n : neighbors) {
			if (n.x() >= 0 && n.x() < w && n.y() >= 0 && n.y() < h) {
				if (image.pixelColor(n) != targetColor) {
					continue;
				}
				image.setPixelColor(n, fillColor);
				queue.enqueue(n);
			}
		}
	}
}

LIBUI_API void pls_flood_fill_color2(QImage &image, const QColor &targetColor, const QColor &fillColor)
{
	if (targetColor == fillColor)
		return;

	const int w = image.width();
	const int h = image.height();
	QPoint seedPoint(-1, -1);
	for (int y = 0; y < h && seedPoint == QPoint(-1, -1); ++y) {
		for (int x = 0; x < w; ++x) {
			if (image.pixelColor(x, y) == targetColor) {
				seedPoint = QPoint(x, y);
				break;
			}
		}
	}
	if (seedPoint == QPoint(-1, -1))
		return;

	QQueue<QPoint> queue;
	queue.enqueue(seedPoint);
	image.setPixelColor(seedPoint, fillColor);

	while (!queue.isEmpty()) {
		QPoint p = queue.dequeue();
		const QPoint neighbors[4] = {QPoint(p.x() + 1, p.y()), QPoint(p.x() - 1, p.y()), QPoint(p.x(), p.y() + 1), QPoint(p.x(), p.y() - 1)};
		for (const QPoint &n : neighbors) {
			if (n.x() >= 0 && n.x() < w && n.y() >= 0 && n.y() < h) {
				QColor nColor = image.pixelColor(n);
				if (nColor != targetColor) {
					continue;
				}
				image.setPixelColor(n, fillColor);
				queue.enqueue(n);
			}
		}
	}
}

LIBUI_API QPixmap pls_get_win_custom_drag_pixmap(QWidget *widget)
{
	if (!widget) {
		return QPixmap();
	}
	QSize cursorSize = pls_get_win_cursor_size(widget);
	auto imgPath = ":/resource/images/drag.png";
	QImage img(imgPath);
	if (!pls_is_win_cursor_colored()) {
		return QPixmap::fromImage(img).scaled(cursorSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}

	QColor targetColor("#FFFFFE");
	QColor fillColor = pls_get_win_cursor_main_color();
	if (!fillColor.isValid()) {
		return pls_load_pixmap_with_mode(imgPath, cursorSize);
	}
	pls_flood_fill_color(img, targetColor, fillColor);
	return QPixmap::fromImage(img).scaled(cursorSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

static bool is_win_cursor_inverted()
{
	QSettings reg(R"(HKEY_CURRENT_USER\Control Panel\Cursors)", QSettings::NativeFormat);
	QString defaultScheme = reg.value(".").toString();

	if (defaultScheme.contains("Windows Inverted", Qt::CaseInsensitive))
		return true;
	return false;
}

static bool is_win_cursor_black()
{
	QSettings reg(R"(HKEY_CURRENT_USER\Control Panel\Cursors)", QSettings::NativeFormat);
	QString defaultScheme = reg.value(".").toString();

	if (defaultScheme.contains("Windows Black", Qt::CaseInsensitive))
		return true;
	return false;
}

static void invert_cursor_image_for_black_mode(QImage &img)
{
	if (img.isNull())
		return;
	img = img.convertToFormat(QImage::Format_ARGB32);
	const int w = img.width();
	const int h = img.height();
	for (int y = 0; y < h; ++y) {
		QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
		for (int x = 0; x < w; ++x) {
			QRgb c = line[x];
			int a = qAlpha(c);
			if (a == 0)
				continue;
			line[x] = qRgba(255 - qRed(c), 255 - qGreen(c), 255 - qBlue(c), a);
		}
	}
}

LIBUI_API QPixmap pls_get_win_custom_grab_pixmap(QWidget *widget)
{
	if (!widget) {
		return QPixmap();
	}
	QSize cursorSize = pls_get_win_cursor_size(widget);
	cursorSize = QSize(cursorSize.width() * 0.8, cursorSize.height() * 0.8);
	auto imgPath = ":/resource/images/grab.png";
	QImage img(imgPath);
	if (is_win_cursor_black()) {
		invert_cursor_image_for_black_mode(img);
		return QPixmap::fromImage(img).scaled(cursorSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}

	if (!pls_is_win_cursor_colored() || is_win_cursor_inverted()) {
		return QPixmap::fromImage(img).scaled(cursorSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}

	QColor targetColor("#FFFFFE");
	QColor fillColor = pls_get_win_cursor_main_color2();
	if (!fillColor.isValid()) {
		return pls_load_pixmap_with_mode(imgPath, cursorSize);
	}
	pls_flood_fill_color(img, targetColor, fillColor);
	return QPixmap::fromImage(img).scaled(cursorSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

LIBUI_API QPixmap pls_get_win_custom_hover_pixmap(QWidget *widget)
{
	if (!widget) {
		return QPixmap();
	}
	QSize cursorSize = pls_get_win_cursor_size(widget);
	cursorSize = QSize(cursorSize.width() * 0.8, cursorSize.height() * 0.8);
	auto imgPath = ":/resource/images/hover.png";
	QImage img(imgPath);
	if (is_win_cursor_black()) {
		invert_cursor_image_for_black_mode(img);
		return QPixmap::fromImage(img).scaled(cursorSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}

	if (!pls_is_win_cursor_colored() || is_win_cursor_inverted()) {
		return QPixmap::fromImage(img).scaled(cursorSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}

	QColor targetColor("#FFFFFE");
	QColor fillColor = pls_get_win_cursor_main_color2();
	if (!fillColor.isValid()) {
		return pls_load_pixmap_with_mode(imgPath, cursorSize);
	}
	pls_flood_fill_color2(img, targetColor, fillColor);
	return QPixmap::fromImage(img).scaled(cursorSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

LIBUI_API bool pls_is_win_cursor_colored()
{
	QSettings reg(R"(HKEY_CURRENT_USER\Control Panel\Cursors)", QSettings::NativeFormat);
	QString arrowPath = reg.value("Arrow").toString().toLower();

	// Colored styles usually contain "aero_arrow_l.cur", "aero_arrow_xl.cur", or "aero_arrow.cur"
	if (arrowPath.contains("aero_arrow_l.cur") || arrowPath.contains("aero_arrow_xl.cur") || arrowPath.contains("aero_arrow.cur") || arrowPath.contains("arrow_eoa.cur")) {
		return true;
	}
	// Classic styles usually contain "arrow_m.cur" or "arrow.cur"
	if (arrowPath.contains("arrow_m.cur") || arrowPath.contains("arrow.cur")) {
		return false;
	}
	// For other cases, return false (cannot determine accurately)
	return false;
}

static QColor get_win_cursor_main_color(HCURSOR hCursor)
{
	if (!hCursor) {
		CURSORINFO ci = {sizeof(CURSORINFO)};
		if (!GetCursorInfo(&ci) || !ci.hCursor)
			return QColor();
		hCursor = ci.hCursor;
	}

	ICONINFO iconInfo;
	if (!GetIconInfo(hCursor, &iconInfo))
		return QColor();

	HBITMAP hBitmap = iconInfo.hbmColor ? iconInfo.hbmColor : iconInfo.hbmMask;
	if (!hBitmap) {
		if (iconInfo.hbmColor)
			DeleteObject(iconInfo.hbmColor);
		if (iconInfo.hbmMask)
			DeleteObject(iconInfo.hbmMask);
		return QColor();
	}

	BITMAP bmp;
	GetObject(hBitmap, sizeof(BITMAP), &bmp);

	int w = bmp.bmWidth;
	int h = bmp.bmHeight;
	int pixelCount = w * h;
	std::map<QRgb, int> colorCount;

	HDC hdc = GetDC(NULL);
	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = -h; // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	std::vector<QRgb> pixels(pixelCount);
	GetDIBits(hdc, hBitmap, 0, h, pixels.data(), &bmi, DIB_RGB_COLORS);
	ReleaseDC(NULL, hdc);

	for (QRgb rgb : pixels) {
		if (qAlpha(rgb) > 0) // ignore transparent pixels
			colorCount[rgb]++;
	}

	QRgb mainColor = 0;
	int maxCount = 0;
	for (const auto &kv : colorCount) {
		if (kv.second > maxCount) {
			mainColor = kv.first;
			maxCount = kv.second;
		}
	}

	if (iconInfo.hbmColor)
		DeleteObject(iconInfo.hbmColor);
	if (iconInfo.hbmMask)
		DeleteObject(iconInfo.hbmMask);

	return QColor::fromRgba(mainColor);
}

LIBUI_API QColor pls_get_win_cursor_main_color()
{
	return get_win_cursor_main_color(nullptr);
}

LIBUI_API QColor pls_get_win_cursor_main_color2()
{
	HCURSOR hCursor = nullptr;
	bool needDestroy = false;
	QSettings reg(R"(HKEY_CURRENT_USER\Control Panel\Cursors)", QSettings::NativeFormat);
	QString arrowPath = reg.value("Arrow").toString().trimmed();
	if (!arrowPath.isEmpty()) {
		arrowPath.replace("%SystemRoot%", qEnvironmentVariable("SystemRoot"));
		arrowPath.replace("%systemroot%", qEnvironmentVariable("SystemRoot"));
		if (QFile::exists(arrowPath)) {
			const std::wstring pathW = arrowPath.toStdWString();
			hCursor = static_cast<HCURSOR>(LoadImageW(nullptr, pathW.c_str(), IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE));
			if (hCursor)
				needDestroy = true;
		}
	}

	QColor color = get_win_cursor_main_color(hCursor);

	if (needDestroy)
		DestroyCursor(hCursor);

	return color;
}
#endif

LIBUI_API QDialogButtonBox::StandardButton pls_to_standard_button(QMessageBox::StandardButton from)
{
	switch (from) {
	case QMessageBox::NoButton:
		return QDialogButtonBox::StandardButton::NoButton;
	case QMessageBox::Ok:
		return QDialogButtonBox::StandardButton::Ok;
	case QMessageBox::Save:
		return QDialogButtonBox::StandardButton::Save;
	case QMessageBox::SaveAll:
		return QDialogButtonBox::StandardButton::SaveAll;
	case QMessageBox::Open:
		return QDialogButtonBox::StandardButton::Open;
	case QMessageBox::Yes:
		return QDialogButtonBox::StandardButton::Yes;
	case QMessageBox::YesToAll:
		return QDialogButtonBox::StandardButton::YesToAll;
	case QMessageBox::No:
		return QDialogButtonBox::StandardButton::No;
	case QMessageBox::NoToAll:
		return QDialogButtonBox::StandardButton::NoToAll;
	case QMessageBox::Abort:
		return QDialogButtonBox::StandardButton::Abort;
	case QMessageBox::Retry:
		return QDialogButtonBox::StandardButton::Retry;
	case QMessageBox::Ignore:
		return QDialogButtonBox::StandardButton::Ignore;
	case QMessageBox::Close:
		return QDialogButtonBox::StandardButton::Close;
	case QMessageBox::Cancel:
		return QDialogButtonBox::StandardButton::Cancel;
	case QMessageBox::Discard:
		return QDialogButtonBox::StandardButton::Discard;
	case QMessageBox::Help:
		return QDialogButtonBox::StandardButton::Help;
	case QMessageBox::Apply:
		return QDialogButtonBox::StandardButton::Apply;
	case QMessageBox::Reset:
		return QDialogButtonBox::StandardButton::Reset;
	case QMessageBox::RestoreDefaults:
		return QDialogButtonBox::StandardButton::RestoreDefaults;
	default:
		return QDialogButtonBox::StandardButton::NoButton;
	}
}
LIBUI_API QMessageBox::StandardButton pls_to_standard_button(QDialogButtonBox::StandardButton from)
{
	switch (from) {
	case QDialogButtonBox::NoButton:
		return QMessageBox::StandardButton::NoButton;
	case QDialogButtonBox::Ok:
		return QMessageBox::StandardButton::Ok;
	case QDialogButtonBox::Save:
		return QMessageBox::StandardButton::Save;
	case QDialogButtonBox::SaveAll:
		return QMessageBox::StandardButton::SaveAll;
	case QDialogButtonBox::Open:
		return QMessageBox::StandardButton::Open;
	case QDialogButtonBox::Yes:
		return QMessageBox::StandardButton::Yes;
	case QDialogButtonBox::YesToAll:
		return QMessageBox::StandardButton::YesToAll;
	case QDialogButtonBox::No:
		return QMessageBox::StandardButton::No;
	case QDialogButtonBox::NoToAll:
		return QMessageBox::StandardButton::NoToAll;
	case QDialogButtonBox::Abort:
		return QMessageBox::StandardButton::Abort;
	case QDialogButtonBox::Retry:
		return QMessageBox::StandardButton::Retry;
	case QDialogButtonBox::Ignore:
		return QMessageBox::StandardButton::Ignore;
	case QDialogButtonBox::Close:
		return QMessageBox::StandardButton::Close;
	case QDialogButtonBox::Cancel:
		return QMessageBox::StandardButton::Cancel;
	case QDialogButtonBox::Discard:
		return QMessageBox::StandardButton::Discard;
	case QDialogButtonBox::Help:
		return QMessageBox::StandardButton::Help;
	case QDialogButtonBox::Apply:
		return QMessageBox::StandardButton::Apply;
	case QDialogButtonBox::Reset:
		return QMessageBox::StandardButton::Reset;
	case QDialogButtonBox::RestoreDefaults:
		return QMessageBox::StandardButton::RestoreDefaults;
	default:
		return QMessageBox::StandardButton::NoButton;
	}
}
LIBUI_API QDialogButtonBox::StandardButtons pls_to_standard_buttons(QMessageBox::StandardButtons from)
{
	QDialogButtonBox::StandardButtons to;
	to.setFlag(QDialogButtonBox::StandardButton::Ok, from.testFlag(QMessageBox::Ok));
	to.setFlag(QDialogButtonBox::StandardButton::Save, from.testFlag(QMessageBox::Save));
	to.setFlag(QDialogButtonBox::StandardButton::SaveAll, from.testFlag(QMessageBox::SaveAll));
	to.setFlag(QDialogButtonBox::StandardButton::Open, from.testFlag(QMessageBox::Open));
	to.setFlag(QDialogButtonBox::StandardButton::Yes, from.testFlag(QMessageBox::Yes));
	to.setFlag(QDialogButtonBox::StandardButton::YesToAll, from.testFlag(QMessageBox::YesToAll));
	to.setFlag(QDialogButtonBox::StandardButton::No, from.testFlag(QMessageBox::No));
	to.setFlag(QDialogButtonBox::StandardButton::NoToAll, from.testFlag(QMessageBox::NoToAll));
	to.setFlag(QDialogButtonBox::StandardButton::Abort, from.testFlag(QMessageBox::Abort));
	to.setFlag(QDialogButtonBox::StandardButton::Retry, from.testFlag(QMessageBox::Retry));
	to.setFlag(QDialogButtonBox::StandardButton::Ignore, from.testFlag(QMessageBox::Ignore));
	to.setFlag(QDialogButtonBox::StandardButton::Close, from.testFlag(QMessageBox::Close));
	to.setFlag(QDialogButtonBox::StandardButton::Cancel, from.testFlag(QMessageBox::Cancel));
	to.setFlag(QDialogButtonBox::StandardButton::Discard, from.testFlag(QMessageBox::Discard));
	to.setFlag(QDialogButtonBox::StandardButton::Help, from.testFlag(QMessageBox::Help));
	to.setFlag(QDialogButtonBox::StandardButton::Apply, from.testFlag(QMessageBox::Apply));
	to.setFlag(QDialogButtonBox::StandardButton::Reset, from.testFlag(QMessageBox::Reset));
	to.setFlag(QDialogButtonBox::StandardButton::RestoreDefaults, from.testFlag(QMessageBox::RestoreDefaults));
	return to;
}
LIBUI_API QMessageBox::StandardButtons pls_to_standard_buttons(QDialogButtonBox::StandardButtons from)
{
	QMessageBox::StandardButtons to;
	to.setFlag(QMessageBox::StandardButton::Ok, from.testFlag(QDialogButtonBox::Ok));
	to.setFlag(QMessageBox::StandardButton::Save, from.testFlag(QDialogButtonBox::Save));
	to.setFlag(QMessageBox::StandardButton::SaveAll, from.testFlag(QDialogButtonBox::SaveAll));
	to.setFlag(QMessageBox::StandardButton::Open, from.testFlag(QDialogButtonBox::Open));
	to.setFlag(QMessageBox::StandardButton::Yes, from.testFlag(QDialogButtonBox::Yes));
	to.setFlag(QMessageBox::StandardButton::YesToAll, from.testFlag(QDialogButtonBox::YesToAll));
	to.setFlag(QMessageBox::StandardButton::No, from.testFlag(QDialogButtonBox::No));
	to.setFlag(QMessageBox::StandardButton::NoToAll, from.testFlag(QDialogButtonBox::NoToAll));
	to.setFlag(QMessageBox::StandardButton::Abort, from.testFlag(QDialogButtonBox::Abort));
	to.setFlag(QMessageBox::StandardButton::Retry, from.testFlag(QDialogButtonBox::Retry));
	to.setFlag(QMessageBox::StandardButton::Ignore, from.testFlag(QDialogButtonBox::Ignore));
	to.setFlag(QMessageBox::StandardButton::Close, from.testFlag(QDialogButtonBox::Close));
	to.setFlag(QMessageBox::StandardButton::Cancel, from.testFlag(QDialogButtonBox::Cancel));
	to.setFlag(QMessageBox::StandardButton::Discard, from.testFlag(QDialogButtonBox::Discard));
	to.setFlag(QMessageBox::StandardButton::Help, from.testFlag(QDialogButtonBox::Help));
	to.setFlag(QMessageBox::StandardButton::Apply, from.testFlag(QDialogButtonBox::Apply));
	to.setFlag(QMessageBox::StandardButton::Reset, from.testFlag(QDialogButtonBox::Reset));
	to.setFlag(QMessageBox::StandardButton::RestoreDefaults, from.testFlag(QDialogButtonBox::RestoreDefaults));
	return to;
}

#if defined(Q_OS_WIN)
static QPoint toQPoint(const POINT &pt)
{
	return QPoint(pt.x, pt.y);
}
static QRect toQRect(const RECT &rc)
{
	return QRect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
}
static double getDpi(HMONITOR monitor)
{
	UINT dpiX;
	UINT dpiY;
	GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
	return dpiX / 96.0;
}
struct EnumMonitorCblParam {
	QPoint cursorPos;
	QList<PLSMonitor> monitors;
};
static BOOL CALLBACK monitorEnumMonitorCb(HMONITOR monitor, HDC hdc, LPRECT lpRect, LPARAM lParam)
{
	pls_unused(hdc, lpRect);

	auto cblParam = (EnumMonitorCblParam *)lParam;

	PLSMonitor om;
	om.monitor = monitor;
	om.dpi = getDpi(monitor);

	MONITORINFO mi;
	ZeroMemory(&mi, sizeof(mi));
	mi.cbSize = sizeof(mi);
	GetMonitorInfoW(monitor, &mi);
	om.screenRect = toQRect(mi.rcMonitor);
	om.availableRect = toQRect(mi.rcWork);
	if (mi.dwFlags & MONITORINFOF_PRIMARY)
		om.primary = true;
	if (om.screenRect.contains(cblParam->cursorPos))
		om.current = true;

	cblParam->monitors.append(om);
	return true;
}
LIBUI_API QList<PLSMonitor> pls_get_monitors()
{
	POINT cursorPos;
	GetCursorPos(&cursorPos);
	EnumMonitorCblParam cblParam;
	cblParam.cursorPos = toQPoint(cursorPos);
	EnumDisplayMonitors(nullptr, nullptr, &monitorEnumMonitorCb, (LPARAM)&cblParam);
	std::sort(cblParam.monitors.begin(), cblParam.monitors.end(), [](const PLSMonitor &a, const PLSMonitor &b) { return a.screenRect.x() < b.screenRect.x(); });
	for (int i = 0, count = cblParam.monitors.count(); i < count; ++i)
		cblParam.monitors[i].index = i;
	return cblParam.monitors;
}
LIBUI_API PLSMonitor pls_get_primary_monitor()
{
	for (const auto &m : pls_get_monitors())
		if (m.primary)
			return m;
	return PLSMonitor();
}
LIBUI_API PLSMonitor pls_get_current_monitor()
{
	for (const auto &m : pls_get_monitors())
		if (m.current)
			return m;
	return PLSMonitor();
}
LIBUI_API PLSMonitor pls_get_monitor(const QPoint &point)
{
	HMONITOR monitor = MonitorFromPoint({point.x(), point.y()}, MONITOR_DEFAULTTOPRIMARY);
	for (const auto &m : pls_get_monitors())
		if (m.monitor == monitor)
			return m;
	return PLSMonitor();
}
LIBUI_API PLSMonitor pls_get_monitor(HWND hwnd)
{
	HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
	for (const auto &m : pls_get_monitors())
		if (m.monitor == monitor)
			return m;
	return PLSMonitor();
}
LIBUI_API PLSMonitor pls_get_monitor(QWidget *widget)
{
	if (auto toplevel = pls_get_toplevel_view(widget); toplevel) {
		return pls_get_monitor((HWND)toplevel->winId());
	}
	return pls_get_primary_monitor();
}
LIBUI_API PLSMonitor pls_get_monitor(int index)
{
	auto monitors = pls_get_monitors();
	if (index >= 0 && index < monitors.count())
		return monitors[index];
	return PLSMonitor();
}
LIBUI_API PLSMonitor pls_get_monitor_or_primary(int index)
{
	auto monitors = pls_get_monitors();
	if (index >= 0 && index < monitors.count())
		return monitors[index];
	for (const auto &m : monitors)
		if (m.primary)
			return m;
	return PLSMonitor();
}
LIBUI_API QRect pls_get_screen_rect(QWidget *widget)
{
	return pls_get_monitor(widget).screenRect;
}
LIBUI_API QRect pls_get_screen_available_rect(QWidget *widget)
{
	return pls_get_monitor(widget).availableRect;
}
LIBUI_API QRect pls_get_screen_rect(const QPoint &pt)
{
	return pls_get_monitor(pt).screenRect;
}
LIBUI_API QRect pls_get_screen_available_rect(const QPoint &pt)
{
	return pls_get_monitor(pt).availableRect;
}
#endif

LIBUI_API bool pls_is_visible_in_some_screen(const QRect &geometry)
{
	for (const QScreen *screen : QApplication::screens()) {
		if (screen->availableGeometry().intersects(geometry)) {
			return true;
		}
	}
	return false;
}

LIBUI_API QString pls_get_process_code_str(Progress progress, bool next)
{
	static QMap<Progress, QString> processCodes{{Progress::launchPrism, "000: Launch prim"},
						    {Progress::loadLocalized, "005: Loading prism locale"},
						    {Progress::initTheme, "010: Loading prism themes"},
						    {Progress::loadUserDir, "012: Loading prism user directories"},
						    {Progress::loadUserData, "015: Loading prism user datas"},
						    {Progress::loadMainData, "020: Loading prism app datas"},
						    {Progress::initCore, "030: init core libraries"},
						    {Progress::loadMainView, "038: Loading prism main window"},
						    {Progress::initAudio, "045: init audio"},
						    {Progress::initVideo, "050: init video"},
						    {Progress::loadPlugin, "070: load plugin"},
						    {Progress::loadSourceStart, "070: Loading scenes and sources start"},
						    {Progress::loadSourceEnd, "098: Loading scenes and sources"},
						    {Progress::loadComplete, "100: show main view"},
						    {Progress::prismUpAndRunning, "Prism is up and running"}};

	if (!next)
		return processCodes.value(progress, "");
	else {
		auto keys = processCodes.keys();
		auto itr = std::find_if(keys.begin(), keys.end(), [progress](Progress code) { return code == progress; });
		if (itr != keys.end()) {

			auto nextKey = (*itr == keys.last()) ? Progress::prismUpAndRunning : *(++itr);
			return processCodes.value(nextKey, "");
		}
		return processCodes.value(Progress::loadLocalized, "");
	}
}

QImage _generate_qr_image(const QJsonObject &info, int width, int margin)
{
	QJsonDocument info_json(info);
	auto info_bytes = info_json.toJson();
	static auto tips = QObject::tr("QRCode.tips");

	QImage image(width, width, QImage::Format_RGB888);
	image.fill(Qt::white);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	QRcode *pQRcode = QRcode_encodeString(info_bytes.data(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
	if (pQRcode == nullptr) {
		return QImage();
	}

	QImage imageqQRCode(pQRcode->width, pQRcode->width, QImage::Format_Mono);
	imageqQRCode.fill(1);

	auto pData = pQRcode->data;
	for (int y = 0; y < pQRcode->width; ++y) {
		for (int x = 0; x < pQRcode->width; ++x) {
			if (*pData++ & 1) {
				imageqQRCode.setPixel(x, y, 0);
			}
		}
	}

	painter.fillRect(0, 0, width, width, Qt::white);
	painter.drawImage(QRect(margin, margin, width - 2 * margin, width - 2 * margin), imageqQRCode);

	QRcode_free(pQRcode);
	return image;
}

LIBUI_API QImage pls_generate_qr_image(const QJsonObject &info, int width, int margin, const QPixmap &logo)
{
	QImage resultImage;
	QImage qrCodeImage = _generate_qr_image(info, width, margin);
	QSize resultSize;
	resultSize.setWidth(qrCodeImage.width());
	resultSize.setHeight(qrCodeImage.height());
	resultImage = QImage(resultSize, QImage::Format_ARGB32_Premultiplied);
	QPoint logoPoint((qrCodeImage.width() - logo.width()) / 2, (qrCodeImage.height() - logo.height()) / 2);
	QPainter painter(&resultImage);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.fillRect(resultImage.rect(), Qt::transparent);
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.drawPixmap(QRect(logoPoint, logo.size()), logo);
	painter.setCompositionMode(QPainter::CompositionMode_DestinationAtop);
	painter.drawImage(0, 0, qrCodeImage);
	painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
	painter.fillRect(resultImage.rect(), Qt::white);
	painter.end();

#if _DEBUG
	QApplication::clipboard()->setPixmap(QPixmap::fromImage(resultImage), QClipboard::Clipboard);
#endif
	return resultImage;
}

LIBUI_API void pls_window_left_right_margin_fit(QWidget *widget)
{
	QRect windowGeometry = widget->normalGeometry();
	int mostRightPostion = INT_MIN;
	int mostLeftPostion = INT_MAX;

	for (const QScreen *screen : QGuiApplication::screens()) {
		if (screen->availableGeometry().right() > mostRightPostion) {
			mostRightPostion = screen->availableGeometry().right();
		}
		if (screen->availableGeometry().left() < mostLeftPostion) {
			mostLeftPostion = screen->availableGeometry().left();
		}
	}

	if (mostRightPostion < windowGeometry.right()) {
		widget->move(mostRightPostion - windowGeometry.width(), windowGeometry.y());
		widget->repaint();
	}

	if (mostLeftPostion > (windowGeometry.left() + widget->width())) {
		widget->move(mostLeftPostion, windowGeometry.y());
		widget->repaint();
	}
}

LIBUI_API QString pls_get_orignal_text(const QString &text)
{
	if (!Qt::mightBeRichText(text))
		return text;

	QTextDocument doc;
	doc.setHtml(text);
	return doc.toPlainText();
}
LIBUI_API QSize pls_calculate_size_for_width(const QString &text, const QFont &font, int width)
{
	auto orignal = pls_get_orignal_text(text);
	auto parts = orignal.split(QRegularExpression(QStringLiteral("\\r?\\n")));

	QFontMetrics fm(font);
	auto leading = fm.leading();

	QSizeF size(0.0, 0.0);
	for (const auto &part : parts) {
		QTextLayout tl(part, font);
		tl.beginLayout();
		for (auto line = tl.createLine(); line.isValid(); line = tl.createLine()) {
			line.setLineWidth(width);
			size.setHeight(size.height() + leading);
			line.setPosition(QPointF(0, size.height()));
			size.setWidth(qMax(size.width(), line.naturalTextWidth()));
			size.setHeight(size.height() + line.height());
		}
		tl.endLayout();
	}
	return size.toSize();
}

static QString get_single_line_elided_text(const QString &text, const QFont &font, int width)
{
	auto orignal = pls_get_orignal_text(text);
	QFontMetrics fm(font);
	return fm.elidedText(orignal, Qt::ElideRight, width);
}
static QString get_multi_line_elided_text(const QString &text, const QFont &font, const QSize &size)
{
	auto orignal = pls_get_orignal_text(text);
	auto parts = orignal.split(QRegularExpression(QStringLiteral("\\r?\\n")));

	QFontMetrics fm(font);
	auto leading = fm.leading();

	QStringList lines;
	auto height = 0;
	for (qsizetype i = 0, count = parts.count(); i < count; ++i) {
		const auto &part = parts.at(i);
		bool has_more_lines = false;

		QTextLayout tl(part, font);
		tl.beginLayout();
		for (auto line = tl.createLine(); line.isValid(); line = tl.createLine()) {
			line.setLineWidth(size.width());
			height += leading;
			line.setPosition(QPointF(0, height));
			height += qRound(line.height());
			if (height <= size.height()) {
				lines.append(part.mid(line.textStart(), line.textLength()));
			} else {
				has_more_lines = true;
				break;
			}
		}
		tl.endLayout();

		if (!has_more_lines) {
			continue;
		} else if (lines.isEmpty()) {
			return QString();
		} else {
			lines.append(fm.elidedText(lines.takeLast() + QStringLiteral("..."), Qt::ElideRight, size.width()));
			return lines.join('\n');
		}
	}
	return lines.join('\n');
}
LIBUI_API QString pls_get_elided_text(const QString &text, const QFont &font, const QSize &size, bool single_line)
{
	if (single_line)
		return get_single_line_elided_text(text, font, size.width());
	else
		return get_multi_line_elided_text(text, font, size);
}
static void set_label_elided_text(QLabel *label, const QSize &size, const QString &orignal_text, bool show_tooltip)
{
	auto elided_text = pls_get_elided_text(orignal_text, label->font(), size, !label->wordWrap());
	label->setText(elided_text);
	if (show_tooltip && orignal_text != elided_text)
		label->setToolTip(orignal_text);
}
LIBUI_API void pls_elided_text(QLabel *label, const QString &text, bool show_tooltip)
{
	auto watcher = label->findChild<PLSResizeWatcher *>();
	if (!watcher) {
		watcher = pls_new<PLSResizeWatcher>(label);
		QObject::connect(watcher, &PLSResizeWatcher::signalSizeChanged, label, [label, watcher](const QSize &size) {
			set_label_elided_text(label, size, watcher->property("$elided_text_orignal_text").toString(), watcher->property("$elided_text_show_tooltip").toBool());
		});
	}

	watcher->setProperty("$elided_text_orignal_text", text);
	watcher->setProperty("$elided_text_show_tooltip", show_tooltip);
	set_label_elided_text(label, label->size(), text, show_tooltip);
}
LIBUI_API void pls_elided_text(QLabel *label, bool show_tooltip)
{
	pls_elided_text(label, label->text(), show_tooltip);
}

#if defined(Q_OS_WIN)
class singleton_wakeup_thread_t : public QThread {
	HANDLE m_event;
	pls_shm_base_t *m_shm;
	std::function<void(const QStringList &second_instance_cmdlines)> m_second_instance_notify;

public:
	explicit singleton_wakeup_thread_t(HANDLE event, pls_shm_base_t *shm, std::function<void(const QStringList &second_instance_cmdlines)> &&second_instance_notify)
		: m_event(event), m_shm(shm), m_second_instance_notify(std::move(second_instance_notify))
	{
	}
	~singleton_wakeup_thread_t() override = default;

private:
	void run() override
	{
		while (!isInterruptionRequested()) {
			if ((WaitForSingleObject(m_event, INFINITE) == WAIT_OBJECT_0) && !isInterruptionRequested()) {
				ResetEvent(m_event);

				if (m_second_instance_notify) {
					QStringList cmdlines;
					if (m_shm) {
						auto size = pls_shm_base_get_max_data_size(m_shm);
						QByteArray buf(size, '\0');
						pls_shm_base_read(m_shm, buf.data(), size);
						buf[size - 1] = 0;
						cmdlines = QString::fromUtf8(buf.data()).split('\n', Qt::SkipEmptyParts);
					}

					qDebug() << "Second instance cmdlines: " << cmdlines;
					m_second_instance_notify(cmdlines);
				}
			}
		}
	}
};
#endif
class pls_singleton_app_t {
#if defined(Q_OS_WIN)
	pls_shm_base_t *m_shm;
	HANDLE m_event;
	QPointer<singleton_wakeup_thread_t> m_wakeup_thread;

public:
	pls_singleton_app_t(pls_shm_base_t *shm, HANDLE event, singleton_wakeup_thread_t *wakeup_thread) : m_shm(shm), m_event(event), m_wakeup_thread(wakeup_thread) {}
	~pls_singleton_app_t() = default;

	void destroy()
	{
		m_wakeup_thread->requestInterruption();

		SetEvent(m_event);
		pls_delete_thread(m_wakeup_thread, nullptr);
		pls_delete(m_shm, pls_shm_base_destroy, nullptr);
		pls_delete(m_event, CloseHandle, nullptr);
	}
#elif defined(Q_OS_MACOS)
public:
	pls_singleton_app_t() = default;
	~pls_singleton_app_t() = default;
#endif
};
LIBUI_API bool pls_singleton_app_instance(std::function<void(const QStringList &second_instance_cmdlines)> &&second_instance_notify, int max_data_size)
{
	auto app = PLSUiApp::instance();
	if (!app)
		return false;

#if defined(PRODUCT_PRISM)
	auto unique_name = QStringLiteral("PRISMLiveStudio");
#elif defined(PRODUCT_LENS)
	auto unique_name = QStringLiteral("PRISMLens");
#elif defined(PRODUCT_INSTALLER)
	auto unique_name = QStringLiteral("PRISMInstaller");
#else
#endif

	auto name = unique_name.toStdWString();
#if defined(Q_OS_WIN)
	auto shm = pls_shm_base_create(unique_name + QStringLiteral("_shm"), max_data_size);
	if (!shm) {
		PLS_INFO("singleton_app", "crate singleton app instance shm failed");
	}

	HANDLE event = OpenEventW(EVENT_ALL_ACCESS, FALSE, name.c_str());
	bool already_running = !!event;
	if (!already_running) {
		event = CreateEventW(nullptr, TRUE, FALSE, name.c_str());
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			already_running = true;
		}
	}
	if (already_running) {
		auto args = PLSUiApp::arguments().mid(1).join('\n').toStdString();
		pls_shm_base_write(shm, args.c_str(), args.length() + 1);
		pls_delete(shm, pls_shm_base_destroy, nullptr);

		SetEvent(event);
		pls_delete(event, CloseHandle, nullptr);
		return false;
	}

	PLS_INFO("singleton_app", "start wakeup thread");
	auto wakeup_thread = pls_new<singleton_wakeup_thread_t>(event, shm, std::move(second_instance_notify));
	wakeup_thread->start();

	app->m_singletonApp = pls_new<pls_singleton_app_t>(shm, event, wakeup_thread);
#elif defined(Q_OS_MACOS)
	bool already_running = pls_check_mac_app_is_existed(name.c_str());
	if (already_running) {
		QJsonObject payload;
		auto args = PLSUiApp::arguments().mid(1);
		payload.insert("args", pls_to_json_array(args));
		payload.insert("active", true);
		pls::mac::sendSignal(pls::mac::PLS_MAC_ACTIVE_SIGNAL_NAME, payload);
		return false;
	}
	pls::mac::listenSignal(pls::mac::PLS_MAC_ACTIVE_SIGNAL_NAME, [notify = std::move(second_instance_notify)](const QString &signalName, const QJsonObject &payload) {
		if (!notify) {
			return;
		} else if (signalName == pls::mac::PLS_MAC_ACTIVE_SIGNAL_NAME) {
			PLS_INFO("singleton_app", "receive active notification");
			notify(pls_to_string_list(payload["args"].toArray()));
		}
	});
	PLS_INFO("singleton_app", "start listen active notification");
#endif
	return true;
}
LIBUI_API void pls_singleton_app_destroy(pls_singleton_app_t *singleton_app)
{
	if (singleton_app) {
#if defined(Q_OS_WIN)
		singleton_app->destroy();
#endif
		pls_delete(singleton_app);
	}
}

static bool is_prism_app_installed(QString *program, QString *home, QString *version)
{
#if defined(Q_OS_WIN)
	QSettings settings1("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PRISM Live Studio", QSettings::NativeFormat);
	auto displayVersion = settings1.value("DisplayVersion").toString();
	if (displayVersion.isEmpty())
		return false;

	auto installDir = pls_get_qsetting_value(pls_product_type_t::Prism, "InstallDir").toString();
	if (installDir.isEmpty())
		return false;

	QString exePath;
	if (installDir.endsWith('\\') || installDir.endsWith('/'))
		exePath = installDir + QStringLiteral("bin\\64bit\\PRISMLiveStudio.exe");
	else
		exePath = installDir + QStringLiteral("\\bin\\64bit\\PRISMLiveStudio.exe");
	if (!QFileInfo(exePath).isFile())
		return false;

	pls_set_value(program, exePath);
	pls_set_value(home, installDir);
	pls_set_value(version, displayVersion);
	return true;
#elif defined(Q_OS_MACOS)
	QString installDir = pls_get_install_app(pls::KIdentifier_PRISM);
	if (installDir.isEmpty())
		return false;

	pls_set_value(program, installDir + "/Contents/MacOS/PRISMLiveStudio");
	pls_set_value(home, installDir);
	pls_set_value(version, pls_libutil_api_mac::pls_get_app_version_by_app_path(installDir));
	return true;
#else
	return false;
#endif
}
static bool is_lens_app_installed(QString *program, QString *home, QString *version)
{
#if defined(Q_OS_WIN)
	QSettings settings1("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PRISM Lens", QSettings::NativeFormat);
	auto displayVersion = settings1.value("DisplayVersion").toString();
	if (displayVersion.isEmpty())
		return false;

	auto installDir = pls_get_qsetting_value(pls_product_type_t::Lens, "InstallDir").toString();
	if (installDir.isEmpty())
		return false;

	QString exePath;
	if (installDir.endsWith('\\') || installDir.endsWith('/'))
		exePath = installDir + QStringLiteral("bin\\64bit\\PRISMLens.exe");
	else
		exePath = installDir + QStringLiteral("\\bin\\64bit\\PRISMLens.exe");
	if (!QFileInfo(exePath).isFile()) {
		exePath = installDir + QStringLiteral("\\PRISMLens.exe");
		if (!QFileInfo(exePath).isFile())
			return false;
	}

	pls_set_value(program, exePath);
	pls_set_value(home, installDir);
	pls_set_value(version, displayVersion);
	return true;
#elif defined(Q_OS_MACOS)
	QString installDir = pls_get_install_app(pls::KIdentifier_LENS);
	if (installDir.isEmpty())
		return false;

	pls_set_value(program, installDir + "/Contents/MacOS/PRISMLens");
	pls_set_value(home, installDir);
	pls_set_value(version, pls_libutil_api_mac::pls_get_app_version_by_app_path(installDir));
	return true;
#else
	return false;
#endif
}
LIBUI_API bool pls_is_app_installed(pls_product_type_t product, QString *program, QString *home, QString *version)
{
	switch (product) {
	case pls_product_type_t::Prism:
		return is_prism_app_installed(program, home, version);
	case pls_product_type_t::Lens:
		return is_lens_app_installed(program, home, version);
	default:
		return false;
	}
}

#if defined(Q_OS_WIN)
static bool is_event_existed(const wchar_t *name)
{
	if (auto handle = OpenEventW(SYNCHRONIZE, false, name); handle && (handle != INVALID_HANDLE_VALUE)) {
		CloseHandle(handle);
		return true;
	}
	return false;
}
#endif
LIBUI_API bool pls_is_app_running(pls_product_type_t product)
{
#if defined(Q_OS_WIN)
	switch (product) {
	case pls_product_type_t::Prism:
		return is_event_existed(L"PRISMLiveStudio");
	case pls_product_type_t::Lens:
		return is_event_existed(L"PRISMLens");
	default:
		return false;
	}
#elif defined(Q_OS_MACOS)
	return pls::mac::is_product_process_running(product);
#else
	return false;
#endif
}
LIBUI_API bool pls_is_app_started(pls_product_type_t product)
{
#if defined(Q_OS_WIN)
	switch (product) {
	case pls_product_type_t::Prism:
		return is_event_existed(L"PRISMLiveStudio");
	case pls_product_type_t::Lens:
		return is_event_existed(L"com.prism.cam.running.flag");
	default:
		return false;
	}
#elif defined(Q_OS_MACOS)
	return pls::mac::is_product_process_running(product);
#else
	return false;
#endif
}
LIBUI_API bool pls_is_app_exited(pls_process_t *process)
{
#if defined(Q_OS_WIN)
	return pls_process_wait(process, 0) > 0;
#elif defined(Q_OS_MACOS)
	return pls::mac::pls_is_app_exited(process);
#else
	return false;
#endif
}

class pls_app_impl_t : public QObject {
	pls_product_type_t m_product;
	int m_monitor_timer = -1;
	pls_process_t *m_process = nullptr;
	std::map<const QObject *, std::pair<QPointer<QObject>, pls_app_on_state_t>> m_on_states;

public:
	explicit pls_app_impl_t(PLSUiApp *uiapp, pls_product_type_t product) : m_product(product)
	{
		QObject::connect(uiapp, &PLSUiApp::peerProcessId, this, &pls_app_impl_t::onPeerProcessId);
		QObject::connect(uiapp, &PLSUiApp::peerAppStateChanged, this, &pls_app_impl_t::onPeerAppStateChanged);
		QObject::connect(uiapp, &PLSUiApp::peerAnyWindowShow, this, &pls_app_impl_t::onPeerAnyWindowShow);
		QObject::connect(uiapp, &PLSUiApp::peerMainWindowShow, this, &pls_app_impl_t::onPeerMainWindowShow);
		QObject::connect(uiapp, &PLSUiApp::peerAnyWindowActived, this, &pls_app_impl_t::onPeerAnyWindowActived);
		QObject::connect(uiapp, &PLSUiApp::peerMainWindowActived, this, &pls_app_impl_t::onPeerMainWindowActived);

		auto thread = pls_new<QThread>();
		thread->start();
		moveToThread(thread);
		pls_async_call(this, [uiapp, pthis = QPointer<QObject>(this), this]() {
			if (!pthis || !uiapp->ipcIsConnected())
				return;

			m_process = pls_process_create(uiapp->ipcGetPeerProcessId());
			if (m_process)
				startMonitorTimer();
		});
	}
	~pls_app_impl_t() override
	{
		stopMonitorTimer();
		pls_delete(m_process, pls_process_destroy, nullptr);
	}

	void open(PLSUiApp *uiapp, const QStringList &args, QPointer<QObject> receiver, const pls_app_on_state_t &on_state)
	{
		pls_async_call(this, [uiapp, args, receiver, on_state, pthis = QPointer<QObject>(this), this]() {
			if (!pthis || !receiver || !on_state)
				return;

			add(receiver, on_state);

			if (uiapp->ipcIsConnected()) {
				if (!m_process)
					m_process = pls_process_create(uiapp->ipcGetPeerProcessId());
				processOpened(receiver, on_state);
				pls_ipc_send_wake_up(uiapp->m_betweenPrismLens, args);
				globalInvoke(pls_app_state_t::ProcessStarted);
				invoke(receiver, on_state, pls_app_state_t::ProcessStarted);
			} else if (QString program; pls_is_app_installed(m_product, &program)) {
				if (m_process)
					pls_delete(m_process, pls_process_destroy, nullptr);
				m_process = pls_process_create(program, args, true);
				processOpened(receiver, on_state);
			} else {
				globalInvoke(pls_app_state_t::AppNotInstalled);
				invoke(receiver, on_state, pls_app_state_t::AppNotInstalled);
			}
		});
	}
	void destroy()
	{
		auto thread = this->thread();
		deleteLater();
		pls_delete_thread(thread);
	}

protected:
	void startMonitorTimer()
	{
		stopMonitorTimer();
		m_monitor_timer = startTimer(200);
	}
	void stopMonitorTimer()
	{
		if (m_monitor_timer != -1) {
			killTimer(m_monitor_timer);
			m_monitor_timer = -1;
		}
	}

	void add(QObject *receiver, const pls_app_on_state_t &on_state) { m_on_states[receiver] = {receiver, on_state}; }
	void remove(const QObject *receiver)
	{
		pls_async_call(this, [receiver, this]() { m_on_states.erase(receiver); });
	}
	void globalInvoke(pls_app_state_t state)
	{
		pls_async_invoke(PLSUiApp::instance(), [state, pthis = QPointer<QObject>(this)]() {
			pls_check_app_exiting();
			if (!pthis)
				return;
			PLSUiApp::instance()->peerAppState(state);
		});
	}
	void invoke(QObject *receiver, const pls_app_on_state_t &on_state, pls_app_state_t state)
	{
		if (auto iter = m_on_states.find(receiver); (iter == m_on_states.end()) || (!iter->second.first))
			return;

		pls_async_invoke(receiver, [pthis = QPointer<QObject>(this), qpreceiver = QPointer<QObject>(receiver), receiver, on_state, state, this]() {
			pls_check_app_exiting();
			if (!pthis)
				return;
			else if (!qpreceiver || !on_state || !on_state(state))
				remove(receiver);
		});
	}
	void invoke(pls_app_state_t state)
	{
		globalInvoke(state);
		for (const auto &[receiver, on_state] : m_on_states) {
			if (on_state.first && on_state.second)
				invoke(on_state.first, on_state.second, state);
			else
				remove(receiver);
		}
	}
	void processOpened(QObject *receiver, const pls_app_on_state_t &on_state)
	{
		if (!m_process) {
			stopMonitorTimer();
			globalInvoke(pls_app_state_t::OpenProcessFailed);
			invoke(receiver, on_state, pls_app_state_t::OpenProcessFailed);
			return;
		}

		globalInvoke(pls_app_state_t::OpenProcessOk);
		invoke(receiver, on_state, pls_app_state_t::OpenProcessOk);

		if (auto uiapp = PLSUiApp::instance(); uiapp->ipcIsConnected()) {
			//
		} else if (m_product == pls_product_type_t::Prism || m_product == pls_product_type_t::Lens) {
			for (bool startedFlag = false; !startedFlag;) {
				if (pls_process_wait(m_process, 200) != 0)
					break;
				else if (!startedFlag && pls_is_app_started(m_product)) {
					startedFlag = true;
					if (m_product == pls_product_type_t::Lens)
						pls_ipc_connect(uiapp->m_betweenPrismLens);
					globalInvoke(pls_app_state_t::ProcessStarted);
					invoke(receiver, on_state, pls_app_state_t::ProcessStarted);
				}
			}
		}

		startMonitorTimer();
	}

	void timerEvent(QTimerEvent *event) override
	{
		if (m_monitor_timer == event->timerId()) {
			if (!m_process)
				stopMonitorTimer();
			else if (pls_is_app_exited(m_process))
				onPeerExit();
		}

		QObject::timerEvent(event);
	}

private:
	void onPeerExit()
	{
		stopMonitorTimer();
		pls_delete(m_process, pls_process_destroy, nullptr);
		invoke(pls_app_state_t::ProcessExited);
	}
	void onPeerProcessId()
	{
		stopMonitorTimer();
		pls_delete(m_process, pls_process_destroy, nullptr);
		m_process = pls_process_create(PLSUiApp::instance()->ipcGetPeerProcessId());
		if (m_process)
			startMonitorTimer();
		if (m_on_states.empty())
			globalInvoke(pls_app_state_t::ProcessStarted);
	}
	void onPeerAppStateChanged(bool actived) { invoke(actived ? pls_app_state_t::Actived : pls_app_state_t::Inactived); }
	void onPeerAnyWindowShow() { invoke(pls_app_state_t::AnyWindowShow); }
	void onPeerMainWindowShow() { invoke(pls_app_state_t::MainWindowShow); }
	void onPeerAnyWindowActived() { invoke(pls_app_state_t::AnyWindowActived); }
	void onPeerMainWindowActived() { invoke(pls_app_state_t::MainWindowActived); }
};
LIBUI_API pls_app_t pls_app_create(pls_product_type_t product)
{
	return pls_new<pls_app_impl_t>(PLSUiApp::instance(), product);
}
LIBUI_API void pls_app_destroy(pls_app_t app)
{
	if (auto impl = dynamic_cast<pls_app_impl_t *>(app.data()); impl)
		impl->destroy();
}
LIBUI_API QString pls_app_state_to_string(pls_app_state_t state)
{
	switch (state) {
	case pls_app_state_t::AppNotInstalled:
		return QStringLiteral("AppNotInstalled");
	case pls_app_state_t::OpenProcessOk:
		return QStringLiteral("OpenProcessOk");
	case pls_app_state_t::OpenProcessFailed:
		return QStringLiteral("OpenProcessFailed");
	case pls_app_state_t::ProcessStarted:
		return QStringLiteral("ProcessStarted");
	case pls_app_state_t::ProcessExited:
		return QStringLiteral("ProcessExited");
	case pls_app_state_t::Inactived:
		return QStringLiteral("Inactived");
	case pls_app_state_t::Actived:
		return QStringLiteral("Actived");
	case pls_app_state_t::AnyWindowShow:
		return QStringLiteral("AnyWindowShow");
	case pls_app_state_t::MainWindowShow:
		return QStringLiteral("MainWindowShow");
	case pls_app_state_t::AnyWindowActived:
		return QStringLiteral("AnyWindowActived");
	case pls_app_state_t::MainWindowActived:
		return QStringLiteral("MainWindowActived");
	default:
		return QStringLiteral("Unknown");
	}
}
LIBUI_API void pls_app_open(pls_app_t app, const QStringList &args, QPointer<QObject> receiver, const pls_app_on_state_t &on_state)
{
	if (auto impl = dynamic_cast<pls_app_impl_t *>(app.data()); impl)
		impl->open(PLSUiApp::instance(), args, receiver, on_state);
}

//signalName=clicked, action=Click, moduleName=, controls=full path
LIBUI_API void pls_button_uistep_custom(QObject *object, const pls_getter_t &getter)
{
	pls_button_uistep_custom(object, PLS_UI_STEPS_V2_SIGNAL_CLICKED, QString(), QString(), QString(), QString(), PLS_UI_STEPS_V2_ACTION_CLICK, getter);
}
//action=Click, moduleName=, controls=full path
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const pls_getter_t &getter)
{
	pls_button_uistep_custom(object, signalName, QString(), QString(), QString(), QString(), PLS_UI_STEPS_V2_ACTION_CLICK, getter);
}
//moduleName=, controls=full path
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const QString &action, const pls_getter_t &getter)
{
	pls_button_uistep_custom(object, signalName, QString(), QString(), QString(), QString(), action, getter);
}
//controls=full path
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &action, const pls_getter_t &getter)
{
	pls_button_uistep_custom(object, signalName, moduleName, QString(), QString(), QString(), action, getter);
}
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &controlFullname, const QString &action, const pls_getter_t &getter)
{
	pls_button_uistep_custom(object, signalName, moduleName, controlFullname, QString(), QString(), action, getter);
}
//controls=full path+controls+additional
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &controls, const QString &additional, const QString &action,
					const pls_getter_t &getter)
{
	pls_button_uistep_custom(object, signalName, moduleName, QString(), controls, additional, action, getter);
}
//controls=controlFullname or controls=full path+controls+additional
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &controlFullname, const QString &controls, const QString &additional,
					const QString &action, const pls_getter_t &getter)
{
	auto uss = object->property("ui-steps").toHash();
	auto us = uss[signalName].toHash();
	us[QStringLiteral("customButton")] = true;
	us[QStringLiteral("moduleName")] = moduleName;
	us[QStringLiteral("controlFullname")] = controlFullname;
	us[QStringLiteral("controls")] = controls;
	us[QStringLiteral("additional")] = additional;
	us[QStringLiteral("action")] = action;
	uss[signalName] = us;
	object->setProperty("ui-steps", uss);
	if (getter)
		pls_add_object_getter(object, QString("ui-steps.%1.getActionData").arg(signalName).toUtf8(), getter);
}

//moduleName=, controls=full path
LIBUI_API void pls_control_uistep_custom(QObject *object, const QString &signalName, const QString &action, const pls_getter_t &getter)
{
	pls_control_uistep_custom(object, signalName, QString(), QString(), QString(), QString(), action, getter);
}
//controls=full path
LIBUI_API void pls_control_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &action, const pls_getter_t &getter)
{
	pls_control_uistep_custom(object, signalName, moduleName, QString(), QString(), QString(), action, getter);
}
//controls=controlFullname
LIBUI_API void pls_control_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &controlFullname, const QString &action, const pls_getter_t &getter)
{
	pls_control_uistep_custom(object, signalName, moduleName, controlFullname, QString(), QString(), action, getter);
}
//controls=full path+controls+additional
LIBUI_API void pls_control_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &controls, const QString &additional, const QString &action,
					 const pls_getter_t &getter)
{
	pls_control_uistep_custom(object, signalName, moduleName, QString(), controls, additional, action, getter);
}
//controls=controlFullname or controls=full path+controls+additional
LIBUI_API void pls_control_uistep_custom(QObject *object, const QString &signalName, const QString &moduleName, const QString &controlFullname, const QString &controls, const QString &additional,
					 const QString &action, const pls_getter_t &getter)
{
	auto uss = object->property("ui-steps").toHash();
	auto us = uss[signalName].toHash();
	us[QStringLiteral("customControl")] = true;
	us[QStringLiteral("moduleName")] = moduleName;
	us[QStringLiteral("controlFullname")] = controlFullname;
	us[QStringLiteral("controls")] = controls;
	us[QStringLiteral("additional")] = additional;
	us[QStringLiteral("action")] = action;
	uss[signalName] = us;
	object->setProperty("ui-steps", uss);
	if (getter)
		pls_add_object_getter(object, QString("ui-steps.%1.getActionData").arg(signalName).toUtf8(), getter);
}

pls_language_key_t &pls_language_key_t::operator=(const QByteArray &key)
{
	m_key = key;
	return *this;
}

pls_text_t &pls_text_t::operator=(const QString &text)
{
	m_key.clear();
	m_text = text;
	m_english.clear();
	return *this;
}
pls_text_t &pls_text_t::operator=(const pls_language_key_t &key)
{
	m_key = key;
	m_text.clear();
	m_english.clear();
	return *this;
}
QString pls_text_t::text() const
{
	if (m_text.isEmpty() && !m_key.isEmpty())
		m_text = build(QObject::tr(m_key.constData()));
	return m_text;
}
QString pls_text_t::english() const
{
	if (m_english.isEmpty())
		m_english = m_key.isEmpty() ? pls_uistep_v2_to_english(text()) : build(pls_uistep_v2_get_english(m_key));
	return m_english;
}
QString pls_text_t::build(const QString &format) const
{
	return m_builder ? m_builder(format) : format;
}
static bool uistep_v2_get_title_with_group(QString &title, QObject *object, bool with_group)
{
	if (!with_group || !object->inherits("QGroupBox"))
		return true;
	else if (auto gbtitle = static_cast<QGroupBox *>(object)->title(); !gbtitle.isEmpty())
		title = title + QStringLiteral(" - ") + pls_uistep_v2_to_english(gbtitle, pls_uistep_v2_title_auto_to_english_enabled(object), true);
	return true;
}
static bool uistep_v2_get_widget_title(QString &title, QWidget *widget, bool with_group)
{
	if (!widget) {
		return false;
	} else if (QVariant result; pls_call_object_getter(result, widget, PLS_UI_STEPS_V2_TITLE)) {
		title = pls_uistep_v2_to_english(result.toString(), pls_uistep_v2_title_auto_to_english_enabled(widget), true);
		return true;
	} else if (auto mo = widget->metaObject(); !mo || mo->inherits(&QMenu::staticMetaObject)) {
		return uistep_v2_get_widget_title(title, widget->parentWidget(), with_group);
	} else if (widget->isWindow()) {
		title = widget->isVisible() ? pls_uistep_v2_to_english(widget->windowTitle(), pls_uistep_v2_title_auto_to_english_enabled(widget), true) : QString();
		return true;
	} else {
		return uistep_v2_get_widget_title(title, widget->parentWidget(), with_group) ? uistep_v2_get_title_with_group(title, widget, with_group) : false;
	}
}
static bool uistep_v2_get_title(QString &title, QObject *object, bool with_group)
{
	if (!object) {
		return false;
	} else if (object->isWidgetType()) {
		return uistep_v2_get_widget_title(title, static_cast<QWidget *>(object), with_group);
	} else if (QVariant result; pls_call_object_getter(result, object, PLS_UI_STEPS_V2_TITLE)) {
		title = pls_uistep_v2_to_english(result.toString(), pls_uistep_v2_title_auto_to_english_enabled(object), true);
		return true;
	} else {
		return uistep_v2_get_title(title, object->parent(), with_group);
	}
}
LIBUI_API QByteArray pls_formatv(const char *format, va_list args)
{
	auto length = vsnprintf(nullptr, 0, format, args) + 1;
	QByteArray result(length, Qt::Initialization::Uninitialized);
	vsnprintf(result.data(), length, format, args);
	return result;
}
LIBUI_API QByteArray pls_format(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	auto result = pls_formatv(format, args);
	va_end(args);
	return result;
}
LIBUI_API void pls_uistep_v2_focus_changed(QWidget *old, QWidget *now)
{
	SignalSpyCallback::uiStepV2FocusChanged(old, now);
}
LIBUI_API QString pls_uistep_v2_to_english_cb_default(const QString &text)
{
	return text;
}
LIBUI_API void pls_uistep_v2_set_to_english_cb(std::function<QString(const QString &text)> &&to_english_cb)
{
	LocalGlobalVars::g_to_english_cb = std::move(to_english_cb);
}
LIBUI_API void pls_uistep_v2_add_to_english_cb(const QByteArray &plugin_id, std::function<QString(const QByteArray &plugin_id, const QString &text)> &&to_english_cb)
{
	if (auto iter = std::find_if(LocalGlobalVars::g_plugin_to_english_cbs.begin(), LocalGlobalVars::g_plugin_to_english_cbs.end(), [plugin_id](const auto &i) { return i.first == plugin_id; });
	    iter != LocalGlobalVars::g_plugin_to_english_cbs.end()) {
		iter->second = to_english_cb;
	} else {
		LocalGlobalVars::g_plugin_to_english_cbs.emplaceBack(plugin_id, to_english_cb);
	}
}
LIBUI_API void pls_uistep_v2_add_to_english_cb(const char *plugin_id, bool (*to_english_cb)(const char *kr_str, const char *plugin_id, const char **out))
{
	if (pls_is_empty(plugin_id) || !to_english_cb)
		return;

	pls_uistep_v2_add_to_english_cb(plugin_id, [to_english_cb](const QByteArray &plugin_id, const QString &text) {
		if (const char *out = nullptr; to_english_cb(text.toUtf8().constData(), plugin_id.constData(), &out)) {
			return QString::fromUtf8(out);
		}
		return QString();
	});
}
LIBUI_API void pls_uistep_v2_remove_to_english_cb(const QByteArray &plugin_id)
{
	if (auto iter = std::find_if(LocalGlobalVars::g_plugin_to_english_cbs.begin(), LocalGlobalVars::g_plugin_to_english_cbs.end(), [plugin_id](const auto &i) { return i.first == plugin_id; });
	    iter != LocalGlobalVars::g_plugin_to_english_cbs.end()) {
		LocalGlobalVars::g_plugin_to_english_cbs.erase(iter);
	}
}
LIBUI_API QString pls_uistep_v2_to_english(const QString &text)
{
	if (text.isEmpty() || pls_get_locale() == QStringLiteral("en-US"))
		return text;
	for (auto i : LocalGlobalVars::g_plugin_to_english_cbs)
		if (auto english = i.second(i.first, text); !english.isEmpty())
			return english;
	if (LocalGlobalVars::g_to_english_cb)
		return LocalGlobalVars::g_to_english_cb(text);
	return text;
}
LIBUI_API QString pls_uistep_v2_get_english_cb_default(const QByteArray &key)
{
	return QString::fromUtf8(key);
}
LIBUI_API void pls_uistep_v2_set_get_english_cb(std::function<QString(const QByteArray &key)> &&get_english_cb)
{
	LocalGlobalVars::g_get_english_cb = std::move(get_english_cb);
}
LIBUI_API void pls_uistep_v2_add_get_english_cb(const QByteArray &plugin_id, std::function<QString(const QByteArray &plugin_id, const QByteArray &key)> &&get_english_cb)
{
	if (auto iter = std::find_if(LocalGlobalVars::g_plugin_get_english_cbs.begin(), LocalGlobalVars::g_plugin_get_english_cbs.end(), [plugin_id](const auto &i) { return i.first == plugin_id; });
	    iter != LocalGlobalVars::g_plugin_get_english_cbs.end()) {
		iter->second = get_english_cb;
	} else {
		LocalGlobalVars::g_plugin_get_english_cbs.emplaceBack(plugin_id, get_english_cb);
	}
}
LIBUI_API void pls_uistep_v2_remove_get_english_cb(const QByteArray &plugin_id)
{
	if (auto iter = std::find_if(LocalGlobalVars::g_plugin_get_english_cbs.begin(), LocalGlobalVars::g_plugin_get_english_cbs.end(), [plugin_id](const auto &i) { return i.first == plugin_id; });
	    iter != LocalGlobalVars::g_plugin_get_english_cbs.end()) {
		LocalGlobalVars::g_plugin_get_english_cbs.erase(iter);
	}
}
LIBUI_API QString pls_uistep_v2_get_english(const QByteArray &key)
{
	for (auto i : LocalGlobalVars::g_plugin_get_english_cbs)
		if (auto text = i.second(i.first, key); !text.isEmpty())
			return text;
	if (LocalGlobalVars::g_get_english_cb)
		return LocalGlobalVars::g_get_english_cb(key);
	return QString::fromUtf8(key);
}
#if defined(PLS_UI_ACTION_STATS)
LIBUI_API std::optional<QByteArray> pls_uistep_v2_get_custom_show_hide_name(const QObject *object)
{
	if (QVariant custom_show_hide_name; !pls_call_object_getter(custom_show_hide_name, object, PLS_UI_STEPS_V2_CUSTOM_SHOW_HIDE_NAME))
		return std::nullopt;
	else if (auto value = custom_show_hide_name.toByteArray(); !value.isEmpty())
		return value;
	return std::nullopt;
}
LIBUI_API void pls_uistep_v2_set_custom_show_hide_name(const QObject *object, const QByteArray &custom_show_hide_name)
{
	pls_uistep_v2_set_custom_show_hide_name(object, [custom_show_hide_name]() { return custom_show_hide_name; });
}
LIBUI_API void pls_uistep_v2_set_custom_show_hide_name(const QObject *object, const pls_get_bytearray_t &custom_show_hide_name)
{
	pls_add_object_getter(object, PLS_UI_STEPS_V2_CUSTOM_SHOW_HIDE_NAME, [custom_show_hide_name]() -> QVariant { return custom_show_hide_name(); });
}
LIBUI_API std::optional<QByteArray> pls_uistep_v2_get_custom_enable_disable_name(const QObject *object)
{
	if (QVariant custom_enable_disable_name; !pls_call_object_getter(custom_enable_disable_name, object, PLS_UI_STEPS_V2_CUSTOM_ENABLE_DISABLE_NAME))
		return std::nullopt;
	else if (auto value = custom_enable_disable_name.toByteArray(); !value.isEmpty())
		return value;
	return std::nullopt;
}
LIBUI_API void pls_uistep_v2_set_custom_enable_disable_name(const QObject *object, const QByteArray &custom_enable_disable_name)
{
	pls_uistep_v2_set_custom_enable_disable_name(object, [custom_enable_disable_name]() { return custom_enable_disable_name; });
}
LIBUI_API void pls_uistep_v2_set_custom_enable_disable_name(const QObject *object, const pls_get_bytearray_t &custom_enable_disable_name)
{
	pls_add_object_getter(object, PLS_UI_STEPS_V2_CUSTOM_ENABLE_DISABLE_NAME, [custom_enable_disable_name]() -> QVariant { return custom_enable_disable_name(); });
}
LIBUI_API std::optional<QByteArray> pls_uistep_v2_get_custom_enter_leave_name(const QObject *object)
{
	if (QVariant custom_enter_leave_name; !pls_call_object_getter(custom_enter_leave_name, object, PLS_UI_STEPS_V2_CUSTOM_ENTER_LEAVE_NAME))
		return std::nullopt;
	else if (auto value = custom_enter_leave_name.toByteArray(); !value.isEmpty())
		return value;
	return std::nullopt;
}
LIBUI_API void pls_uistep_v2_set_custom_enter_leave_name(const QObject *object, const QByteArray &custom_enter_leave_name)
{
	pls_uistep_v2_set_custom_enter_leave_name(object, [custom_enter_leave_name]() { return custom_enter_leave_name; });
}
LIBUI_API void pls_uistep_v2_set_custom_enter_leave_name(const QObject *object, const pls_get_bytearray_t &custom_enter_leave_name)
{
	pls_add_object_getter(object, PLS_UI_STEPS_V2_CUSTOM_ENTER_LEAVE_NAME, [custom_enter_leave_name]() -> QVariant { return custom_enter_leave_name(); });
}
LIBUI_API std::optional<QByteArray> pls_uistep_v2_get_custom_hover_enter_leave_name(const QObject *object)
{
	if (QVariant custom_hover_enter_leave_name; !pls_call_object_getter(custom_hover_enter_leave_name, object, PLS_UI_STEPS_V2_CUSTOM_HOVER_ENTER_LEAVE_NAME))
		return std::nullopt;
	else if (auto value = custom_hover_enter_leave_name.toByteArray(); !value.isEmpty())
		return value;
	return std::nullopt;
}
LIBUI_API void pls_uistep_v2_set_custom_hover_enter_leave_name(const QObject *object, const QByteArray &custom_hover_enter_leave_name)
{
	pls_uistep_v2_set_custom_hover_enter_leave_name(object, [custom_hover_enter_leave_name]() { return custom_hover_enter_leave_name; });
}
LIBUI_API void pls_uistep_v2_set_custom_hover_enter_leave_name(const QObject *object, const pls_get_bytearray_t &custom_hover_enter_leave_name)
{
	pls_add_object_getter(object, PLS_UI_STEPS_V2_CUSTOM_HOVER_ENTER_LEAVE_NAME, [custom_hover_enter_leave_name]() -> QVariant { return custom_hover_enter_leave_name(); });
}
LIBUI_API bool pls_uistep_v2_window_state_is_listening(const QObject *object)
{
	auto listen_window_state = object->property(PLS_UI_STEPS_V2_LISTEN_WINDOW_STATE);
	return (listen_window_state.type() == QVariant::Bool) ? listen_window_state.toBool() : false;
}
LIBUI_API void pls_uistep_v2_listen_window_state(QObject *object, bool listen)
{
	object->setProperty(PLS_UI_STEPS_V2_LISTEN_WINDOW_STATE, listen);
}
LIBUI_API bool pls_uistep_v2_window_close_is_listening(const QObject *object)
{
	auto listen_window_close = object->property(PLS_UI_STEPS_V2_LISTEN_WINDOW_CLOSE);
	return (listen_window_close.type() == QVariant::Bool) ? listen_window_close.toBool() : false;
}
LIBUI_API void pls_uistep_v2_listen_window_close(QObject *object, bool listen)
{
	object->setProperty(PLS_UI_STEPS_V2_LISTEN_WINDOW_CLOSE, listen);
}
#endif
LIBUI_API QString pls_uistep_v2_to_english(const QString &text, bool enabled, bool to_original_text)
{
	return enabled ? pls_uistep_v2_to_english(to_original_text ? pls_get_original_of_rich_text(text) : text) : text;
}
LIBUI_API QString pls_uistep_v2_to_english(const QString &text, const pls_get_enabled_t &enabled, bool to_original_text)
{
	return pls_uistep_v2_to_english(text, enabled(), to_original_text);
}
static bool uistep_v2_is_with_group(const QObject *object)
{
	if (object->property(PLS_UI_STEPS_V2_TITLE_WITH_GROUP).toBool())
		return true;
	else if (auto parent = object->parent(); !parent)
		return false;
	else if (parent->isWindowType())
		return parent->property(PLS_UI_STEPS_V2_TITLE_WITH_GROUP).toBool();
	else
		return uistep_v2_is_with_group(parent);
}
LIBUI_API QString pls_uistep_v2_get_title(QObject *object)
{
	if (QString title; uistep_v2_get_title(title, object, uistep_v2_is_with_group(object)))
		return title;
	return QString();
}
LIBUI_API bool pls_uistep_v2_title_auto_to_english_enabled(QObject *object)
{
	if (QVariant enabled; pls_call_object_getter(enabled, object, PLS_UI_STEPS_V2_TITLE_AUTO_TO_ENGLISH_ENABLED)) {
		return enabled.toBool();
	}
	return true;
}
LIBUI_API void pls_uistep_v2_title_auto_to_english_enable(QObject *object, bool enabled)
{
	pls_uistep_v2_title_auto_to_english_enable(object, [enabled]() -> bool { return enabled; });
}
LIBUI_API void pls_uistep_v2_title_auto_to_english_enable(QObject *object, const pls_get_enabled_t &enabled)
{
	pls_add_object_getter(object, PLS_UI_STEPS_V2_TITLE_AUTO_TO_ENGLISH_ENABLED, [enabled]() -> QVariant { return enabled(); });
}
LIBUI_API void pls_uistep_v2_set_title(QObject *object, const QString &title, bool with_group, bool auto_to_english_enabled)
{
	pls_uistep_v2_set_title(object, [title]() { return title; }, with_group, auto_to_english_enabled);
}
LIBUI_API void pls_uistep_v2_set_title(QObject *object, const pls_get_text_t &title, bool with_group, bool auto_to_english_enabled)
{
	pls_add_object_getter(object, PLS_UI_STEPS_V2_TITLE, [title]() -> QVariant { return title(); });
	pls_uistep_v2_title_auto_to_english_enable(object, auto_to_english_enabled);
	object->setProperty(PLS_UI_STEPS_V2_TITLE_WITH_GROUP, with_group);
}
static bool uistep_v2_enabled(QObject *object, const char *property)
{
	if (auto value = object->property(property); value.type() == QVariant::Bool)
		return value.toBool();
	else if (auto parent = object->parent(); parent)
		return uistep_v2_enabled(parent, property);
	return true;
}
LIBUI_API bool pls_uistep_v2_enabled(QObject *object)
{
	return uistep_v2_enabled(object, PLS_UI_STEPS_V2_ENABLED);
}
LIBUI_API void pls_uistep_v2_enable(QObject *object, bool enabled)
{
	object->setProperty(PLS_UI_STEPS_V2_ENABLED, enabled);
}
LIBUI_API bool pls_uistep_v2_enabled(QObject *object, const QString &signalName)
{
	return uistep_v2_enabled(object, QStringLiteral(PLS_UI_STEPS_V2_DYN_ENABLED).arg(signalName).toUtf8().constData());
}
LIBUI_API void pls_uistep_v2_enable(QObject *object, const QString &signalName, bool enabled)
{
	object->setProperty(QStringLiteral(PLS_UI_STEPS_V2_DYN_ENABLED).arg(signalName).toUtf8().constData(), enabled);
}
LIBUI_API void pls_uistep_v2_tab(const QObjectList &objects, const QString &signalName)
{
	for (auto object : objects) {
		pls_uistep_v2_custom(object, signalName, PLS_UI_STEPS_V2_ACTION_CHOOSE, {{QStringLiteral("name"), QStringLiteral("tab")}});
	}
}
LIBUI_API void pls_uistep_v2_custom(QObject *object, const QString &signalName, const QString &action, const QVariantHash &attrs)
{
	auto us = object->property(PLS_UI_STEPS_V2).toHash();
	us[signalName] = action;
	us[signalName + QStringLiteral(".attrs")] = attrs;
	object->setProperty(PLS_UI_STEPS_V2, us);
}
LIBUI_API bool pls_uistep_v2_name_auto_to_english_enabled(QObject *object, const QString &signalName)
{
	if (QVariant enabled; pls_call_object_getter(enabled, object, QStringLiteral(PLS_UI_STEPS_V2_DYN_NAME_AUTO_TO_ENGLISH_ENABLED).arg(signalName).toUtf8())) {
		return enabled.toBool();
	} else if (pls_call_object_getter(enabled, object, QByteArrayLiteral(PLS_UI_STEPS_V2_ALL_NAME_AUTO_TO_ENGLISH_ENABLED))) {
		return enabled.toBool();
	} else if (pls_call_object_getter(enabled, object, QByteArrayLiteral(PLS_UI_STEPS_V2_NAME_AUTO_TO_ENGLISH_ENABLED))) {
		return enabled.toBool();
	}
	return true;
}
LIBUI_API void pls_uistep_v2_name_auto_to_english_enable(QObject *object, const QString &signalName, bool enabled)
{
	pls_uistep_v2_name_auto_to_english_enable(object, signalName, [enabled]() -> bool { return enabled; });
}
LIBUI_API void pls_uistep_v2_name_auto_to_english_enable(QObject *object, const QString &signalName, const pls_get_enabled_t &enabled)
{
	pls_add_object_getter(object, QStringLiteral(PLS_UI_STEPS_V2_DYN_NAME_AUTO_TO_ENGLISH_ENABLED).arg(signalName).toUtf8(), [enabled]() -> QVariant { return enabled(); });
}
LIBUI_API QString pls_uistep_v2_get_name(QObject *object, const QString &signalName)
{
	if (auto cs = SignalSpyCallback::getUiStepV2ControlSignal(object); cs)
		return SignalSpyCallback::getUiStepV2Name(object, signalName, std::get<2>(cs.value()));
	return SignalSpyCallback::getUiStepV2Name(object, signalName);
}
LIBUI_API void pls_uistep_v2_set_name(QObject *object, const QString &signalName, const QString &name, bool auto_to_english_enabled)
{
	pls_uistep_v2_set_name(object, signalName, [name]() { return name; }, auto_to_english_enabled);
}
LIBUI_API void pls_uistep_v2_set_name(QObject *object, const QString &signalName, const pls_get_text_t &name, bool auto_to_english_enabled)
{
	pls_add_object_getter(object, QStringLiteral(PLS_UI_STEPS_V2_DYN_NAME).arg(signalName).toUtf8(), [name]() -> QVariant { return name(); });
	pls_uistep_v2_name_auto_to_english_enable(object, signalName, auto_to_english_enabled);
}
LIBUI_API bool pls_uistep_v2_value_auto_to_english_enabled(QObject *object, const QString &signalName)
{
	if (QVariant enabled; pls_call_object_getter(enabled, object, QStringLiteral(PLS_UI_STEPS_V2_DYN_VALUE_AUTO_TO_ENGLISH_ENABLED).arg(signalName).toUtf8())) {
		return enabled.toBool();
	} else if (pls_call_object_getter(enabled, object, QByteArrayLiteral(PLS_UI_STEPS_V2_ALL_VALUE_AUTO_TO_ENGLISH_ENABLED))) {
		return enabled.toBool();
	} else if (pls_call_object_getter(enabled, object, QByteArrayLiteral(PLS_UI_STEPS_V2_VALUE_AUTO_TO_ENGLISH_ENABLED))) {
		return enabled.toBool();
	}
	return true;
}
LIBUI_API void pls_uistep_v2_value_auto_to_english_enable(QObject *object, const QString &signalName, bool enabled)
{
	pls_uistep_v2_value_auto_to_english_enable(object, signalName, [enabled]() -> bool { return enabled; });
}
LIBUI_API void pls_uistep_v2_value_auto_to_english_enable(QObject *object, const QString &signalName, const pls_get_enabled_t &enabled)
{
	pls_add_object_getter(object, QStringLiteral(PLS_UI_STEPS_V2_DYN_VALUE_AUTO_TO_ENGLISH_ENABLED).arg(signalName).toUtf8(), [enabled]() -> QVariant { return enabled(); });
}
LIBUI_API QString pls_uistep_v2_get_value(QObject *object, const QString &signalName)
{
	if (auto cs = SignalSpyCallback::getUiStepV2ControlSignal(object); cs)
		return SignalSpyCallback::getUiStepV2Value(object, signalName, [object, value = std::get<3>(cs.value())]() { return value(object, nullptr); });
	return SignalSpyCallback::getUiStepV2Value(object, signalName);
}
LIBUI_API void pls_uistep_v2_set_value(QObject *object, const QString &signalName, const QString &value, bool auto_to_english_enabled)
{
	pls_uistep_v2_set_value(object, signalName, [value]() { return value; }, auto_to_english_enabled);
}
LIBUI_API void pls_uistep_v2_set_value(QObject *object, const QString &signalName, const pls_get_text_t &value, bool auto_to_english_enabled)
{
	pls_add_object_getter(object, QStringLiteral(PLS_UI_STEPS_V2_DYN_VALUE).arg(signalName).toUtf8(), [value]() -> QVariant { return value(); });
	pls_uistep_v2_value_auto_to_english_enable(object, signalName, auto_to_english_enabled);
}
LIBUI_API QString pls_uistep_v2_auto_bind_get_name(QObject *name, QWidget *widget)
{
	if (auto mo = name->metaObject(); mo->inherits(&QAction::staticMetaObject))
		return static_cast<QAction *>(name)->text();
	else if (mo->inherits(&QLabel::staticMetaObject))
		return static_cast<QLabel *>(name)->text();
	else if (mo->inherits(&PLSComboBox::staticMetaObject))
		return static_cast<PLSComboBox *>(name)->currentText();
	else if (mo->inherits(&PLSCheckBox::staticMetaObject))
		return static_cast<PLSCheckBox *>(name)->text();

	for (auto child : name->children()) {
		if (!child->isWidgetType())
			continue;
		else if (auto text = pls_uistep_v2_auto_bind_get_name(static_cast<QWidget *>(child), widget); !text.isEmpty())
			return text;
	}
	return QString();
}
LIBUI_API void pls_uistep_v2_clear_auto_bind_name(QWidget *widget)
{
	pls_remove_object_getter(widget, QByteArrayLiteral(PLS_UI_STEPS_V2_ALL_NAME));
	pls_remove_object_getter(widget, QByteArrayLiteral(PLS_UI_STEPS_V2_ALL_NAME_AUTO_TO_ENGLISH_ENABLED));
}

static void uistep_v2_bind_by_name(QWidget *widget, QMap<QString, QWidget *> &widgets, const std::function<QString(QObject *name, QWidget *widget)> &get_name)
{
	if (auto name = widget->property(PLS_UI_STEPS_V2_NAME_OBJECT); name.type() != QVariant::String)
		return;
	else if (auto iter = widgets.find(name.toString()); iter != widgets.end())
		pls_uistep_v2_bind(widget, iter.value(), get_name);
}
static void uistep_v2_bind_by_layout(QWidget *label, QLayout *layout, const std::function<QString(QObject *name, QWidget *widget)> &get_name)
{
	for (int i = 0, count = layout->count(); i < count; ++i) {
		if (auto item = layout->itemAt(i); !item)
			continue;
		else if (auto widget = item->widget(); widget)
			pls_uistep_v2_set_name(widget, QStringLiteral("*"), [label, widget, get_name]() { return get_name(label, widget); });
		else if (auto sublayout = item->layout(); sublayout)
			uistep_v2_bind_by_layout(label, sublayout, get_name);
	}
}
static QWidget *uistep_v2_form_layout_item_get_label(QLayoutItem *item)
{
	if (auto label = item->widget(); label)
		return label;
	else if (auto layout = item->layout(); layout) {
		for (auto i = 0, count = layout->count(); i < count; ++i) {
			if (auto layoutItem = layout->itemAt(i); !layoutItem)
				continue;
			else if (label = uistep_v2_form_layout_item_get_label(layoutItem); label)
				return label;
		}
	}
	return nullptr;
}
static void uistep_v2_auto_bind(QBoxLayout *boxLayout, const std::function<QString(QObject *name, QWidget *widget)> &get_name);
static void uistep_v2_bind_by_form_layout(QFormLayout *formLayout, const std::function<QString(QObject *name, QWidget *widget)> &get_name)
{
	for (int row = 0, rowCount = formLayout->rowCount(); row < rowCount; ++row) {
		if (auto item = formLayout->itemAt(row, QFormLayout::SpanningRole); item) {
			if (auto layout = item->layout(); !layout)
				continue;
			else if (auto fl = dynamic_cast<QFormLayout *>(layout); fl)
				uistep_v2_bind_by_form_layout(fl, get_name);
			else if (auto bl = dynamic_cast<QBoxLayout *>(layout); bl)
				uistep_v2_auto_bind(bl, get_name);
		} else if (auto labelItem = formLayout->itemAt(row, QFormLayout::LabelRole); !labelItem)
			continue;
		else if (auto widgetItem = formLayout->itemAt(row, QFormLayout::FieldRole); !widgetItem)
			continue;
		else if (auto label = uistep_v2_form_layout_item_get_label(labelItem); !label)
			continue;
		else if (auto widget = widgetItem->widget(); widget)
			pls_uistep_v2_set_name(widget, QStringLiteral("*"), [label, widget, get_name]() { return get_name(label, widget); });
		else if (auto layout = widgetItem->layout(); layout)
			uistep_v2_bind_by_layout(label, layout, get_name);
	}
}
static void uistep_v2_auto_bind(QBoxLayout *boxLayout, const std::function<QString(QObject *name, QWidget *widget)> &get_name)
{
	for (auto i = 0, count = boxLayout->count(); i < count; ++i) {
		if (auto layoutItem = boxLayout->itemAt(i); !layoutItem)
			continue;
		else if (auto layout = layoutItem->layout(); !layout)
			continue;
		else if (auto mo = layout->metaObject(); mo->inherits(&QFormLayout::staticMetaObject))
			uistep_v2_bind_by_form_layout(static_cast<QFormLayout *>(layout), get_name);
		else if (mo->inherits(&QBoxLayout::staticMetaObject))
			uistep_v2_auto_bind(static_cast<QBoxLayout *>(layout), get_name);
	}
}
static void uistep_v2_auto_bind(QWidget *widget, QMap<QString, QWidget *> &widgets, const std::function<QString(QObject *name, QWidget *widget)> &get_name)
{
	for (auto child : widget->children()) {
		if (child->isWidgetType())
			uistep_v2_auto_bind(static_cast<QWidget *>(child), widgets, get_name);
		else if (auto mo = child->metaObject(); mo->inherits(&QFormLayout::staticMetaObject))
			uistep_v2_bind_by_form_layout(static_cast<QFormLayout *>(child), get_name);
		else if (mo->inherits(&QBoxLayout::staticMetaObject))
			uistep_v2_auto_bind(static_cast<QBoxLayout *>(child), get_name);
	}

	uistep_v2_bind_by_name(widget, widgets, get_name);
}
static void uistep_v2_auto_bind_enum_widgets(QMap<QString, QWidget *> &widgets, QWidget *widget)
{
	if (auto name = widget->objectName(); !name.isEmpty())
		widgets[name] = widget;
	for (auto child : widget->children())
		if (child->isWidgetType())
			uistep_v2_auto_bind_enum_widgets(widgets, static_cast<QWidget *>(child));
}
LIBUI_API void pls_uistep_v2_auto_bind(QWidget *widget, const std::function<QString(QObject *name, QWidget *widget)> &get_name)
{
	if (widget && get_name) {
		QMap<QString, QWidget *> widgets;
		uistep_v2_auto_bind_enum_widgets(widgets, widget);
		uistep_v2_auto_bind(widget, widgets, get_name);
	}
}
LIBUI_API void pls_uistep_v2_bind(QWidget *widget, QObject *name, const std::function<QString(QObject *name, QWidget *widget)> &get_name)
{
	if (auto cs = SignalSpyCallback::getUiStepV2ControlSignal(widget); cs && !std::get<0>(cs.value()).isEmpty())
		pls_uistep_v2_bind(widget, std::get<0>(cs.value()), name, get_name);
	else
		pls_uistep_v2_bind(widget, QStringLiteral("*"), name, get_name);
}
LIBUI_API void pls_uistep_v2_bind(QWidget *widget, const QString &signalName, QObject *name, const std::function<QString(QObject *name, QWidget *widget)> &get_name)
{
	if (widget && !signalName.isEmpty() && name && get_name)
		pls_uistep_v2_set_name(widget, signalName, [name, widget, get_name]() { return get_name(name, widget); });
}
LIBUI_API void pls_uistep_v2_bind(QWidget *widget, const QObjectList &names, const std::function<QString(QObject *name, QWidget *widget)> &get_name)
{
	if (auto cs = SignalSpyCallback::getUiStepV2ControlSignal(widget); cs && !std::get<0>(cs.value()).isEmpty())
		pls_uistep_v2_bind(widget, std::get<0>(cs.value()), names, get_name);
	else
		pls_uistep_v2_bind(widget, QStringLiteral("*"), names, get_name);
}
static QString uistep_v2_get_name(QWidget *widget, const QString &signalName, const QObjectList &names, const std::function<QString(QObject *name, QWidget *widget)> &get_name)
{
	QString name;
	for (auto i : names) {
		if (!name.isEmpty())
			name.append(QStringLiteral(" -> "));
		name.append(pls_uistep_v2_to_english(get_name(i, widget)));
	}
	return name;
}
LIBUI_API void pls_uistep_v2_bind(QWidget *widget, const QString &signalName, const QObjectList &names, const std::function<QString(QObject *name, QWidget *widget)> &get_name)
{
	if (widget && !signalName.isEmpty() && !names.isEmpty() && get_name) {
		pls_uistep_v2_set_name(widget, signalName, [widget, signalName, names, get_name]() { return uistep_v2_get_name(widget, signalName, names, get_name); });
		pls_uistep_v2_name_auto_to_english_enable(widget, signalName, false);
	}
}

pls::HotKeyLocker::HotKeyLocker()
{
	auto count = LocalGlobalVars::g_hotKeyLockerCount++;
	if (!count && LocalGlobalVars::g_disableHotKeyCb) {
		LocalGlobalVars::g_disableHotKeyCb();
	}
}
pls::HotKeyLocker ::~HotKeyLocker()
{
	auto count = --LocalGlobalVars::g_hotKeyLockerCount;
	if (!count && LocalGlobalVars::g_enableHotKeyCb) {
		LocalGlobalVars::g_enableHotKeyCb();
	}
}

void pls::HotKeyLocker::setHotKeyCb(const std::function<void()> &enableHotKeyCb, const std::function<void()> &disableHotKeyCb)
{
	LocalGlobalVars::g_enableHotKeyCb = enableHotKeyCb;
	LocalGlobalVars::g_disableHotKeyCb = disableHotKeyCb;
}

LIBUI_API pls_process_t *pls_create_loading_app(const QString &prismSession, const QString &prismSubSession)
{
#if defined(Q_OS_WIN)
	QString loadingAppPath = pls_get_app_dir() + "/PLSAppLoadingView.exe";
#elif defined(Q_OS_MACOS)
	QString loadingAppPath = pls_get_app_dir() + "/PLSAppLoadingView";
#endif
	QStringList startParams;
	startParams.append(shared_values::k_launcher_command_locale + pls_get_locale());
	startParams.append(shared_values::k_launcher_command_log_prism_session + prismSession);
	startParams.append(shared_values::k_launcher_command_log_sub_prism_session + prismSubSession);
	startParams.append(shared_values::k_launcher_prism_version + pls_get_prism_version_string());
#if defined(Q_OS_MACOS)
	startParams.append(shared_values::k_launcher_command_prism_pid + QString::number(pls_current_process_id()));
#endif
	auto loadingAppPro = pls_process_create(loadingAppPath, startParams);
	if (!loadingAppPro) {
		PLS_ERROR("loadingApp", "create app loading process failed, error: %d", pls_last_error());
	}
	return loadingAppPro;
}

LIBUI_API void pls_destory_loading_app(pls_process_t *pro)
{
	if (pro) {
		pls_process_terminate(pro, 0);
		pls_process_destroy(pro);
	}
}

LIBUI_API QString pls_get_original_of_rich_text(const QString &rich_text)
{
	if (!Qt::mightBeRichText(rich_text)) {
		return rich_text;
	}

	QTextDocument doc;
	doc.setHtml(rich_text);
	auto plain_text = doc.toPlainText();
	if (plain_text.endsWith(QStringLiteral("*")))
		return plain_text.left(plain_text.size() - 1);
	return plain_text;
}

void scroll_to_category_button(QPushButton *categoryButton, QScrollArea *scrollArea)
{
	if (!categoryButton || !scrollArea)
		return;
	int leftPosX = categoryButton->mapToParent(QPoint(0, 0)).x();
	int rightPosX = categoryButton->mapToParent(QPoint(0, 0)).x() + categoryButton->width();
	int value = scrollArea->horizontalScrollBar()->value();
	int viewportLeft = value;
	int viewportRight = value + scrollArea->width();
	int offset = leftPosX - viewportLeft;
	if (offset < 0) {
		scrollArea->horizontalScrollBar()->setValue(viewportLeft + offset);
	}
	offset = rightPosX - viewportRight;
	if (offset > 0) {
		int maxValue = scrollArea->horizontalScrollBar()->maximum();
		int newValue = value + offset;
		if (maxValue < newValue)
			newValue = maxValue;
		scrollArea->horizontalScrollBar()->setValue(newValue);
	}
}
