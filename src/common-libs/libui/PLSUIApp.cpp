#include "PLSUIApp.h"

#include <private/qapplication_p.h>
#include <qdir.h>
#include <qproxystyle.h>
#include <qtooltip.h>
#include <qabstractbutton.h>
#include <qabstractspinbox.h>
#include <qlistwidget.h>

#include "libui.h"
#include "PLSComboBox.h"
#include "PLSCheckBox.h"
#include "PLSRadioButton.h"

constexpr QSize CHECKBOX_ICON_SIZE{15, 15};
constexpr QSize RADIOBUTTON_ICON_SIZE{18, 18};
constexpr QSize SWITCHBUTTON_ICON_SIZE{28, 17};
constexpr int FACTOR = 4;

static PLSUiApp *s_instance = nullptr;

class PLSStyle : public QProxyStyle {
public:
	using QProxyStyle::QProxyStyle;
	~PLSStyle() = default;
	virtual void drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override
	{
		if (element != QStyle::PE_FrameTabBarBase) {
			QProxyStyle::drawPrimitive(element, option, painter, widget);
		}
	}
	int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override
	{
		if (hint == SH_TabBar_Alignment) {
			return Qt::AlignLeft;
		} else if (hint == QStyle::SH_ToolTip_WakeUpDelay) {
			return 0;
		}
		return QProxyStyle::styleHint(hint, option, widget, returnData);
	}
	int pixelMetric(QStyle::PixelMetric metric, const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const override
	{
		if (metric == PM_SubMenuOverlap) {
			return 0;
		}
		return QProxyStyle::pixelMetric(metric, option, widget);
	}
#if defined(Q_OS_MACOS)
	void drawItemText(QPainter *painter, const QRect &rect, int flags, const QPalette &pal, bool enabled, const QString &text, QPalette::ColorRole textRole) const override
	{
		if (textRole == QPalette::ToolTipText) {
			flags &= ~(Qt::AlignTop | Qt::AlignBottom);
			flags |= Qt::AlignVCenter;
		}
		QProxyStyle::drawItemText(painter, rect, flags, pal, enabled, text, textRole);
	}
#endif
};
PLSUiApp::PLSUiApp(int &argc, char **argv) : pls::Application<QApplication>(argc, argv)
{
	s_instance = this;

	qApp->setEffectEnabled(Qt::UI_AnimateMenu, false);
	qApp->setEffectEnabled(Qt::UI_AnimateTooltip, false);
	qApp->setEffectEnabled(Qt::UI_FadeMenu, false);
	qApp->setEffectEnabled(Qt::UI_FadeTooltip, false);

	QString fontStyle;
#if defined(Q_OS_WIN)
	fontStyle = "* {font-family : \"Segoe UI\", \"MalgunGothic\", \"Malgun Gothic\", \"Dotum\", \"Gulim\";}";
#endif
	QDir::setCurrent(applicationDirPath());
	pls_add_global_css({"Common",   "QCheckBox",  "QComboBox",  "QLineEdit", "QMenu",       "QPlainTextEdit", "QPushButton",  "QRadioButton", "QScrollBar",    "QSlider",
			    "QSpinBox", "QTableView", "QTabWidget", "QTextEdit", "QToolButton", "QToolTip",       "CommonDialog", "PLSHelpIcon",  "PLSDialogView", "PLSWindow"},
			   {fontStyle});
	setStyle(new PLSStyle());

	m_checkBoxIcons[CheckedNormal] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-chekedbox-normal.svg"), CHECKBOX_ICON_SIZE * FACTOR);
	m_checkBoxIcons[CheckedHover] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-chekedbox-over.svg"), CHECKBOX_ICON_SIZE * FACTOR);
	m_checkBoxIcons[CheckedPressed] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-chekedbox-clicked.svg"), CHECKBOX_ICON_SIZE * FACTOR);
	m_checkBoxIcons[CheckedDisabled] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-chekedbox-disable.svg"), CHECKBOX_ICON_SIZE * FACTOR);
	m_checkBoxIcons[UncheckedNormal] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-unchekedbox-normal.svg"), CHECKBOX_ICON_SIZE * FACTOR);
	m_checkBoxIcons[UncheckedHover] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-unchekedbox-over.svg"), CHECKBOX_ICON_SIZE * FACTOR);
	m_checkBoxIcons[UncheckedPressed] = m_checkBoxIcons[UncheckedHover];
	m_checkBoxIcons[UncheckedDisabled] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-unchekedbox-disable.svg"), CHECKBOX_ICON_SIZE * FACTOR);

	m_radioButtonIcons[CheckedNormal] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/radio-button-checked.svg"), RADIOBUTTON_ICON_SIZE * FACTOR);
	m_radioButtonIcons[CheckedHover] = m_radioButtonIcons[CheckedNormal];
	m_radioButtonIcons[CheckedPressed] = m_radioButtonIcons[CheckedNormal];
	m_radioButtonIcons[CheckedDisabled] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/radio-button-checked-disable.svg"), RADIOBUTTON_ICON_SIZE * FACTOR);
	m_radioButtonIcons[UncheckedNormal] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/radio-button-unchecked.svg"), RADIOBUTTON_ICON_SIZE * FACTOR);
	m_radioButtonIcons[UncheckedHover] = m_radioButtonIcons[UncheckedNormal];
	m_radioButtonIcons[UncheckedPressed] = m_radioButtonIcons[UncheckedNormal];
	m_radioButtonIcons[UncheckedDisabled] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/radio-button-unchecked-disable.svg"), RADIOBUTTON_ICON_SIZE * FACTOR);

	m_switchButtonIcons[CheckedNormal] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-on.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[CheckedHover] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-on-over.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[CheckedPressed] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-on-click.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[CheckedDisabled] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-on-disable.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[UncheckedNormal] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-off-default.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[UncheckedHover] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-off-over.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[UncheckedPressed] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-off-click.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[UncheckedDisabled] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-off-disable.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);

	connect(this, &PLSUiApp::focusChanged, pls_uistep_v2_focus_changed);
}

PLSUiApp::~PLSUiApp()
{
	pls_delete(m_singletonApp, pls_singleton_app_destroy, nullptr);
	pls_delete(m_betweenPrismLens, pls_ipc_destroy, nullptr);
	pls_delete(m_peerApp, pls_app_destroy, nullptr);
	s_instance = nullptr;
}

PLSUiApp *PLSUiApp::instance()
{
	return s_instance;
}

void PLSUiApp::setAppState(bool actived)
{
	m_appActived = actived;
	pls_ipc_send_app_actived(m_betweenPrismLens, actived);
	appStateChanged(actived);
}

void PLSUiApp::initIpc()
{
#if defined(PRODUCT_PRISM)
	m_betweenPrismLens = pls_ipc_connect(QStringLiteral("IPC_Between_PRISM_Lens"));
#elif defined(PRODUCT_LENS)
	// IPC between PRISM and Lens
	m_betweenPrismLens = pls_ipc_listen(QStringLiteral("IPC_Between_PRISM_Lens"));
#endif

	pls_ipc_on_connected(m_betweenPrismLens, [this](pls_ipc_t ipc) { onIpcConnected(ipc); });
	pls_ipc_on_disconnected(m_betweenPrismLens, [this](pls_ipc_t ipc) { onIpcDisconnected(ipc); });
	pls_ipc_on_message(m_betweenPrismLens, [this](pls_ipc_t ipc, const pls_ipc_message_t &message) { onIpcMessage(ipc, message.type, message.data); });
	pls_ipc_on_inner_message(m_betweenPrismLens, [this](pls_ipc_inner_message_t message, const QJsonValue &data) {
		onIpcInnerMessage(message, data);
		switch (message) {
		case pls_ipc_inner_message_t::PeerProcessId:
			pls_async_call(this, [this]() { emit peerProcessId(); });
			break;
		case pls_ipc_inner_message_t::WakeUp:
			pls_async_call(this, [this, args = pls_to_string_list(data.toArray())]() { emit wakeUp(args); });
			break;
		case pls_ipc_inner_message_t::PeerAppActived:
			pls_async_call(this, [this, actived = data.toBool()]() { emit peerAppStateChanged(actived); });
			break;
		case pls_ipc_inner_message_t::PeerAnyWindowShow:
			pls_async_call(this, [this]() { emit peerAnyWindowShow(); });
			break;
		case pls_ipc_inner_message_t::PeerMainWindowShow:
			pls_async_call(this, [this]() { emit peerMainWindowShow(); });
			break;
		case pls_ipc_inner_message_t::PeerAnyWindowActived:
			pls_async_call(this, [this]() { emit peerAnyWindowActived(); });
			break;
		case pls_ipc_inner_message_t::PeerMainWindowActived:
			pls_async_call(this, [this]() { emit peerMainWindowActived(); });
			break;
		default:
			break;
		}
	});
}

void PLSUiApp::sendIpcMessage(int type, const QJsonValue &data)
{
	pls_ipc_send(m_betweenPrismLens, type, data);
}
void PLSUiApp::initPeerApp()
{
	m_peerApp = pls_app_create(
#if defined(PRODUCT_LENS)
		pls_product_type_t::Prism
#elif defined(PRODUCT_PRISM)
		pls_product_type_t::Lens
#endif
	);
}
void PLSUiApp::openApp(const QStringList &args, QPointer<QObject> receiver, const pls_app_on_state_t &on_state)
{
	pls_app_open(m_peerApp, args, receiver, on_state);
}

QString PLSUiApp::openFilePath() const
{
	return m_openFilePath;
}
void PLSUiApp::setOpenFilePath(const QString &openFilePath)
{
	m_openFilePath = openFilePath;
}
void PLSUiApp::parseOpenFilePath(const std::function<QString(const QStringList &cmdlines)> &openFilePathParser)
{
	m_openFilePathParser = openFilePathParser;
	parseOpenFilePath(arguments());
}
void PLSUiApp::parseOpenFilePath(const QStringList &cmdlines)
{
	if (m_openFilePathParser && !cmdlines.isEmpty())
		setOpenFilePath(m_openFilePathParser(cmdlines));
	else
		setOpenFilePath(QString());
}

void PLSUiApp::processHandCursor(QWidget *widget, bool enter)
{
	if (enter) { // QEvent::Enter
		if (widget->isVisible() && isShowHandWidget(widget->metaObject(), widget)) {
			widget->setCursor(Qt::PointingHandCursor);
		}
	} else { // QEvent::Leave
		if (auto metaObject = widget->metaObject(); metaObject->inherits(&QMenu::staticMetaObject) && widget->isVisible()) {
			return;
		} else if (isShowHandWidget(metaObject, widget)) {
			widget->unsetCursor();
		}
	}
}

bool PLSUiApp::isShowHandWidget(const QMetaObject *metaObject, const QWidget *widget) const
{
	if (widget->property("notShowHandCursor").toBool())
		return false;

	return metaObject->inherits(&QComboBox::staticMetaObject) || metaObject->inherits(&QAbstractButton::staticMetaObject) || metaObject->inherits(&PLSCheckBox::staticMetaObject) ||
	       metaObject->inherits(&PLSRadioButton::staticMetaObject) || metaObject->inherits(&QSlider::staticMetaObject) || metaObject->inherits(&QListWidget::staticMetaObject) ||
	       metaObject->inherits(&PLSComboBox::staticMetaObject) || metaObject->inherits(&QMenu::staticMetaObject) || metaObject->inherits(&PLSComboBoxListView::staticMetaObject) ||
	       metaObject->inherits(&QAbstractSpinBox::staticMetaObject) || metaObject->inherits(&QTabBar::staticMetaObject) || widget->property("showHandCursor").toBool();
}

bool PLSUiApp::ipcIsConnected() const
{
	return pls_ipc_is_connected(m_betweenPrismLens);
}
uint32_t PLSUiApp::ipcGetPeerProcessId() const
{
	return pls_ipc_get_peer_process_id(m_betweenPrismLens);
}
void PLSUiApp::onIpcConnected(pls_ipc_t ipc) {}
void PLSUiApp::onIpcDisconnected(pls_ipc_t ipc) {}
void PLSUiApp::onIpcMessage(pls_ipc_t ipc, int type, const QJsonValue &data) {}
void PLSUiApp::onIpcInnerMessage(pls_ipc_inner_message_t inner_message, const QJsonValue &data) {}

bool PLSUiApp::notify(QObject *receiver, QEvent *e)
{
	if (!receiver->isWidgetType())
		return pls::Application<QApplication>::notify(receiver, e);

	auto widget = static_cast<QWidget *>(receiver);
	switch (e->type()) {
	case QEvent::WindowActivate:
		if (widget->isWindow()) {
			pls_ipc_send_any_window_actived(m_betweenPrismLens);
		}

		if (widget == pls_get_main_view()) {
			pls_ipc_send_main_window_actived(m_betweenPrismLens);
		}
		break;
	case QEvent::MouseMove:
		if (m_tipLabel && m_tipLabel->isVisible() && QApplicationPrivate::instance()->toolTipWidget != receiver && !receiver->property("ignoreHideToolTip").toBool()) {
			m_tipLabel->hide();
		}
		break;
	case QEvent::Show:
		if (widget->isWindow()) {
			pls_ipc_send_any_window_show(m_betweenPrismLens);
		}

		if (widget == pls_get_main_view()) {
			pls_ipc_send_main_window_show(m_betweenPrismLens);
		}

		if (receiver->inherits("QTipLabel")) {
			m_tipLabel = widget;
#if defined(PLS_UI_ACTION_STATS)
			if (auto toolTipWidget = QApplicationPrivate::instance()->toolTipWidget; toolTipWidget) {
				auto title = pls_uistep_v2_get_title(toolTipWidget);
				auto customEnterLeaveName = pls_uistep_v2_get_custom_enter_leave_name(toolTipWidget);
				if (!title.isEmpty() && customEnterLeaveName.has_value()) {
					PLS_UI_ACTION("In %s, Widget ToolTip Show: %s", title.toUtf8().constData(), customEnterLeaveName.value().constData());
				} else if (!title.isEmpty()) {
					PLS_UI_ACTION("In %s, Widget ToolTip Show", title.toUtf8().constData());
				} else if (customEnterLeaveName.has_value()) {
					PLS_UI_ACTION("Widget ToolTip Show: %s", customEnterLeaveName.value().constData());
				} else {
					PLS_UI_ACTION("Widget ToolTip Show");
				}
			} else {
				PLS_UI_ACTION("Widget ToolTip Show");
			}
#endif
		}
#if defined(PLS_UI_ACTION_STATS)
		else if (receiver->inherits("QFileDialog")) {
			PLS_UI_ACTION("Widget FileDialog Show");
		} else if (auto customShowHideName = pls_uistep_v2_get_custom_show_hide_name(receiver); customShowHideName.has_value()) {
			if (auto title = pls_uistep_v2_get_title(receiver); !title.isEmpty())
				PLS_UI_ACTION("In %s, Widget %s Show", title.toUtf8().constData(), customShowHideName.value().constData());
			else
				PLS_UI_ACTION("Widget %s Show", customShowHideName.value().constData());
		}
#endif
		break;
	case QEvent::Hide:
		if (receiver->inherits("QTipLabel")) {
			m_tipLabel = nullptr;
			PLS_UI_ACTION("Widget ToolTip Hide");
		}
#if defined(PLS_UI_ACTION_STATS)
		else if (receiver->inherits("QFileDialog")) {
			PLS_UI_ACTION("Widget FileDialog Hide");
		} else if (auto customShowHideName = pls_uistep_v2_get_custom_show_hide_name(receiver); customShowHideName.has_value()) {
			if (auto title = pls_uistep_v2_get_title(receiver); !title.isEmpty())
				PLS_UI_ACTION("In %s, Widget %s Hide", title.toUtf8().constData(), customShowHideName.value().constData());
			else
				PLS_UI_ACTION("Widget %s Hide", customShowHideName.value().constData());
		}
#endif
		break;
#if defined(PLS_UI_ACTION_STATS)
	case QEvent::EnabledChange:
		if (auto customEnableDisableName = pls_uistep_v2_get_custom_enable_disable_name(receiver); customEnableDisableName.has_value()) {
			if (auto title = pls_uistep_v2_get_title(receiver); !title.isEmpty())
				PLS_UI_ACTION(widget->isEnabled() ? "In %s, Widget %s Enabled" : "In %s, Widget %s Disabled", title.toUtf8().constData(), customEnableDisableName.value().constData());
			else
				PLS_UI_ACTION(widget->isEnabled() ? "Widget %s Enabled" : "Widget %s Disabled", customEnableDisableName.value().constData());
		}
		break;
#endif
	case QEvent::Enter:
		processHandCursor(widget, true);
#if defined(PLS_UI_ACTION_STATS)
		if (auto customEnterLeaveName = pls_uistep_v2_get_custom_enter_leave_name(receiver); customEnterLeaveName.has_value()) {
			if (auto title = pls_uistep_v2_get_title(receiver); !title.isEmpty())
				PLS_UI_ACTION("In %s, Widget %s Enter", title.toUtf8().constData(), customEnterLeaveName.value().constData());
			else
				PLS_UI_ACTION("Widget %s Enter", customEnterLeaveName.value().constData());
		}
#endif
		break;
	case QEvent::Leave:
		processHandCursor(widget, false);
#if defined(PLS_UI_ACTION_STATS)
		if (auto customEnterLeaveName = pls_uistep_v2_get_custom_enter_leave_name(receiver); customEnterLeaveName.has_value()) {
			if (auto title = pls_uistep_v2_get_title(receiver); !title.isEmpty())
				PLS_UI_ACTION("In %s, Widget %s Leave", title.toUtf8().constData(), customEnterLeaveName.value().constData());
			else
				PLS_UI_ACTION("Widget %s Leave", customEnterLeaveName.value().constData());
		}
#endif
		break;
#if defined(PLS_UI_ACTION_STATS)
	case QEvent::HoverEnter:
		if (auto customHoverEnterLeaveName = pls_uistep_v2_get_custom_hover_enter_leave_name(receiver); customHoverEnterLeaveName.has_value()) {
			if (auto title = pls_uistep_v2_get_title(receiver); !title.isEmpty())
				PLS_UI_ACTION("In %s, Widget %s Hover Enter", title.toUtf8().constData(), customHoverEnterLeaveName.value().constData());
			else
				PLS_UI_ACTION("Widget %s Hover Enter", customHoverEnterLeaveName.value().constData());
		}
		break;
	case QEvent::HoverLeave:
		if (auto customHoverEnterLeaveName = pls_uistep_v2_get_custom_hover_enter_leave_name(receiver); customHoverEnterLeaveName.has_value()) {
			if (auto title = pls_uistep_v2_get_title(receiver); !title.isEmpty())
				PLS_UI_ACTION("In %s, Widget %s Hover Leave", title.toUtf8().constData(), customHoverEnterLeaveName.value().constData());
			else
				PLS_UI_ACTION("Widget %s Hover Leave", customHoverEnterLeaveName.value().constData());
		}
		break;
	case QEvent::WindowStateChange:
		if (pls_uistep_v2_window_state_is_listening(receiver)) {
			auto old = static_cast<QWindowStateChangeEvent *>(e)->oldState();
			auto now = widget->windowState();

			if (old.testFlag(Qt::WindowMinimized) && !now.testFlag(Qt::WindowMinimized)) {
				PLS_UI_ACTION("Widget %s From Minimized To Show", receiver->metaObject()->className());
			} else if (!old.testFlag(Qt::WindowMinimized) && now.testFlag(Qt::WindowMinimized)) {
				PLS_UI_ACTION("Widget %s From Show To Minimized", receiver->metaObject()->className());
			}

			if (old.testFlag(Qt::WindowMaximized) && !now.testFlag(Qt::WindowMaximized)) {
				PLS_UI_ACTION("Widget %s From Maximized To Normal", receiver->metaObject()->className());
			} else if (!old.testFlag(Qt::WindowMaximized) && now.testFlag(Qt::WindowMaximized)) {
				PLS_UI_ACTION("Widget %s From Normal To Maximized", receiver->metaObject()->className());
			}

			if (old.testFlag(Qt::WindowFullScreen) && !now.testFlag(Qt::WindowFullScreen)) {
				PLS_UI_ACTION("Widget %s From FullScreen To Not FullScreen", receiver->metaObject()->className());
			} else if (!old.testFlag(Qt::WindowFullScreen) && now.testFlag(Qt::WindowFullScreen)) {
				PLS_UI_ACTION("Widget %s From Not FullScreen To FullScreen", receiver->metaObject()->className());
			}

			if (old.testFlag(Qt::WindowActive) && !now.testFlag(Qt::WindowActive)) {
				PLS_UI_ACTION("Widget %s From Active To Inactive", receiver->metaObject()->className());
			} else if (!old.testFlag(Qt::WindowActive) && now.testFlag(Qt::WindowActive)) {
				PLS_UI_ACTION("Widget %s From Inactive To Active", receiver->metaObject()->className());
			}
		}
		break;
	case QEvent::Close:
		if (pls_uistep_v2_window_close_is_listening(receiver)) {
			if (auto title = pls_uistep_v2_get_title(receiver); !title.isEmpty())
				PLS_UI_ACTION("In %s, Widget %s Close", title.toUtf8().constData(), receiver->metaObject()->className());
			else
				PLS_UI_ACTION("Widget %s Close", receiver->metaObject()->className());
		}
		break;
#endif
#if defined(Q_OS_MACOS)
	case QEvent::ToolTip:
		if (auto toolTip = widget->toolTip(); (!toolTip.isEmpty() && !receiver->property("customToolTip").isValid())) {
			if (widget->property("posFollowCursor").toBool()) {
				auto pos = QCursor::pos();
				QToolTip::showText(QPoint(pos.x(), pos.y() - 20), toolTip, widget);
				return true;
			}
			QPoint global = widget->mapToGlobal(widget->rect().center());
			QToolTip::showText(QPoint(global.x(), global.y() - 20), toolTip, widget);
			e->ignore();
			return true;
		}
		break;
#endif
	default:
		break;
	}

	return pls::Application<QApplication>::notify(receiver, e);
}
