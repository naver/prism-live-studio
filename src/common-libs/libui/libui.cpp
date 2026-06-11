#include "libui.h"

#include <private/qobject_p.h>
#include <private/qmetaobject_p.h>
#include <qapplication.h>
#include <qstyle.h>
#include <qscreen.h>
#include <qhash.h>
#include <qsettings.h>
#include <qdir.h>
#include <qabstractbutton.h>
#include <qaction.h>
#include <qtranslator.h>
#include <qpainter.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qframe.h>

#include "PLSDialogView.h"
#include "pls-shared-values.h"
#include "pls-shared-functions.h"

#include <libutils-api.h>
#include <QQRCoder.h>
#include <liblog.h>
#include <action.h>

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <shellscalingapi.h>
#else
#include "PLSCustomMacWindow.h"
#endif

#include "PLSAlertView.h"

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

class SignalSpyCallback {
	enum class CallerType { AbstractButton, Action, CustomButton, CustomControl };

	static const QByteArray s_clickedMethod;
	static const QByteArray s_triggeredMethod;

public:
	SignalSpyCallback() { qt_register_signal_spy_callbacks(&m_signalSpyCallbackSet); }
	~SignalSpyCallback() {}

	static void signalBeginCallback(QObject *caller, int signal_or_method_index, void **argv)
	{
		if (pls_is_app_exiting() || !pls_object_is_valid(caller))
			return;

		pls_unused(argv);
		QMetaMethod method;
		QString action;
		if (caller && needUiStep(method, action, caller, signal_or_method_index)) {
			PLS_UI_STEP(getModuleName(caller).toUtf8().constData(), getControls(caller).toUtf8().constData(), action.toUtf8().constData());
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

		QString key = QString::fromUtf8(sourceText);
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
	} else if (dynamic_cast<PLSToplevelView<QFrame> *>(widget)) {
		return true;
	} else if (dynamic_cast<PLSToplevelView<QDialog> *>(widget)) {
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

static void insert_text(QHash<QString, QString> &texts, const QString &key, const QString &value)
{
	texts.insert(key, value);
}
static void insert_text(QHash<QByteArray, QByteArray> &texts, const QString &key, const QString &value)
{
	texts.insert(key.toUtf8(), value.toUtf8());
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

//signalName=clicked, action=Click, moduleName=, controls=full path
LIBUI_API void pls_button_uistep_custom(QObject *object, const pls_getter_t &getter)
{
	pls_button_uistep_custom(object, QStringLiteral("clicked"), QString(), QString(), QString(), QString(), QStringLiteral("Click"), getter);
}
//action=Click, moduleName=, controls=full path
LIBUI_API void pls_button_uistep_custom(QObject *object, const QString &signalName, const pls_getter_t &getter)
{
	pls_button_uistep_custom(object, signalName, QString(), QString(), QString(), QString(), QStringLiteral("Click"), getter);
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
