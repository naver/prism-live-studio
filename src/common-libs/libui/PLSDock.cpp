#include "PLSDock.h"

#include <QCheckBox>
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionFrame>
#include <QToolButton>

#include <QMenu>
#include <QDynamicPropertyChangeEvent>
#include <QStyle>
#include <QMouseEvent>

#include <libutils-api.h>
#include "PLSMenuPushButton.h"
#include "PLSPushButton.h"

#include "PLSAlertView.h"
#include <liblog.h>

const int DOCK_WIDGET_MIN_WIDTH = 20;
// Delay before clearing isChangeTopLevel after drag/topLevel change; must be long enough so any QEvent::Show that follows attach/detach is still suppressed.
static const int DOCK_LAYOUT_CHANGE_SUPPRESS_MS = 500;

class PLSDockTitlePopupMenu {
public:
	virtual ~PLSDockTitlePopupMenu() = default;

	virtual void exec(const QPoint &pt) = 0;
};

namespace {

class PLSDockAdvButton : public PLSIconButton {
	using PLSIconButton::PLSIconButton;

	void keyPressEvent(QKeyEvent *event) override { event->ignore(); }
	void keyReleaseEvent(QKeyEvent *event) override { event->ignore(); }
};
}

PLSDockTitle::PLSDockTitle(PLSDock *parent) : QFrame(parent), dock(parent)
{
	captionHeight = 22;

	titleHLayout = pls_new<QHBoxLayout>();
	titleHLayout->setContentsMargins(0, 0, 0, 0);
	titleHLayout->setSpacing(0);
	titleHLayout->setAlignment(Qt::AlignLeft | Qt::AlignCenter);

	titleLabel = pls_new<QLabel>(parent->windowTitle(), this);
	titleLabel->setObjectName("titleLabel");
	titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	titleHLayout->addWidget(titleLabel);

	advButton = pls_new<PLSDockAdvButton>(this);
	advButton->setObjectName(QString::fromUtf8("advButton"));
	advButton->setProperty("useFor", "dockTitle");
	advButton->setToolTip(tr("Dock.Advance.More"));
	pls_uistep_v2_set_value(advButton, "More");
	pls_uistep_v2_set_custom_enter_leave_name(advButton, [this]() { return (pls_uistep_v2_get_title(dock) + "'s More Button").toUtf8(); });

	buttonsLayout = pls_new<QHBoxLayout>();
	buttonsLayout->setSpacing(contentSpacing);
	buttonsLayout->setContentsMargins(0, 0, 0, 0);
	buttonsLayout->setAlignment(Qt::AlignRight | Qt::AlignCenter);

	closeButton = pls_new<QToolButton>();
	closeButton->setObjectName("closeButton");
	pls_uistep_v2_set_value(closeButton, "Close");
	connect(closeButton, &QToolButton::clicked, [this]() {
		dock->setProperty("vis", false);
		dock->setVisible(false);
	});
	QHBoxLayout *l = pls_new<QHBoxLayout>(this);
	l->setSpacing(contentSpacing);
	l->setContentsMargins(1, 1, 1, 1);
	l->addLayout(titleHLayout, 1);
	l->addLayout(buttonsLayout, 0);
	l->addWidget(advButton, 0, Qt::AlignRight);
	l->addWidget(closeButton, 0, Qt::AlignRight);

	closeButton->hide();
	advButton->hide();
	titleLabel->installEventFilter(this);

	connect(parent, &QWidget::windowTitleChanged, titleLabel, [this](const QString &title) { updateTitle(title); });
	connect(advButton, &QToolButton::clicked, [this]() {
		if (advButtonMenu) {
			advButtonMenu->exec(QCursor::pos());
			pls_check_app_exiting();
			advButton->update();
		}
	});
}

PLSDockTitle::~PLSDockTitle()
{
	pls_delete(advButtonMenu, nullptr);
}

int PLSDockTitle::getCaptionHeight() const
{
	return captionHeight;
}

void PLSDockTitle::setCaptionHeight(int captionHeight_)
{
	this->captionHeight = captionHeight_;
	update();
}

int PLSDockTitle::getMarginLeft() const
{
	return marginLeft;
}

void PLSDockTitle::setMarginLeft(int marginLeft_)
{
	this->marginLeft = marginLeft_;
	setContentsMargins(marginLeft, 0, marginRight, 0);
	update();
}

int PLSDockTitle::getMarginRight() const
{
	return marginRight;
}

void PLSDockTitle::setMarginRight(int marginRight_)
{
	this->marginRight = marginRight_;
	setContentsMargins(marginLeft, 0, marginRight, 0);
	update();
}

int PLSDockTitle::getContentSpacing() const
{
	return contentSpacing;
}

void PLSDockTitle::setContentSpacing(int contentSpacing_)
{
	this->contentSpacing = contentSpacing_;
	layout()->setSpacing(contentSpacing);
	update();
}

QList<QToolButton *> PLSDockTitle::getButtons() const
{
	return buttons;
}

PLSIconButton *PLSDockTitle::getAdvButton() const
{
	return advButton;
}

void PLSDockTitle::setButtonActions(QList<QAction *> actions)
{
	buttonActions = actions;

	for (auto &action : actions) {
		QToolButton *button = pls_new<QToolButton>(this);
		button->setObjectName(action->objectName());
		setButtonPropertiesFromAction(button, action);
		connect(button, &QToolButton::clicked, action, &QAction::triggered);
		if (auto toolTip = action->toolTip(); !toolTip.isEmpty())
			button->setToolTip(toolTip);
		else if (auto text = action->text(); !text.isEmpty())
			button->setToolTip(text);
		if (auto value = pls_uistep_v2_get_value(action, PLS_UI_STEPS_V2_SIGNAL_TRIGGERED); !value.isEmpty()) {
			pls_uistep_v2_set_value(button, [action]() { return pls_uistep_v2_get_value(action, PLS_UI_STEPS_V2_SIGNAL_TRIGGERED); });
			pls_uistep_v2_set_custom_enter_leave_name(button, [action]() { return pls_uistep_v2_get_value(action, PLS_UI_STEPS_V2_SIGNAL_TRIGGERED).toUtf8(); });
		}
		buttonsLayout->addWidget(button);
		buttons.append(button);
		pls_uistep_v2_enable(action, false);
	}
}

void PLSDockTitle::setAdvButtonMenu(QMenu *menu)
{
	pls_delete(advButtonMenu, nullptr);

	if (menu) {
		advButtonMenu = menu;
		advButton->show();
	}
}

void PLSDockTitle::setAdvButtonActions(QList<QAction *> actions)
{
	pls_delete(advButtonMenu, nullptr);

	advButtonMenu = pls_new<QMenu>(this);
	advButtonMenu->setWindowFlags(advButtonMenu->windowFlags() | Qt::NoDropShadowWindowHint);
	advButtonMenu->addActions(actions);
	advButton->show();

#if defined(PLS_UI_ACTION_STATS)
	if (auto custom_show_hide_name = pls_uistep_v2_get_custom_show_hide_name(dock); custom_show_hide_name.has_value()) {
		pls_uistep_v2_set_custom_show_hide_name(advButtonMenu, custom_show_hide_name.value() + QByteArrayLiteral("'s More Menu"));
	}
#endif
}

void PLSDockTitle::addAdvButtonMenu(QMenu *menu)
{
	if (!advButtonMenu || !menu) {
		return;
	}
	advButtonMenu->addMenu(menu);
}

QList<QAction *> PLSDockTitle::GetAdvButtonActions() const
{
	if (advButtonMenu) {
		return advButtonMenu->actions();
	}
	return QList<QAction *>();
}

void PLSDockTitle::setAdvButtonActionsEnabledByObjName(const QString &objName, bool enable)
{
	auto actions = GetAdvButtonActions();
	for (auto action : actions) {
		if (0 != action->objectName().compare(objName)) {
			continue;
		}
		action->setEnabled(enable);
	}
}

void PLSDockTitle::setWidgets(QList<QWidget *> widgets)
{
	for (auto &widget : widgets) {
		buttonsLayout->addWidget(widget);
	}
}

void PLSDockTitle::setTitleWidgets(QList<QWidget *> widgets)
{
	for (auto &widget : widgets) {
		titleHLayout->addWidget(widget);
		titleWidgets.push_back(widget);
	}
}

void PLSDockTitle::setCloseButtonVisible(bool visible)
{
	if (closeButton) {
		closeButton->setVisible(hasCloseButton && visible);
	}
}

void PLSDockTitle::setButtonLocked(bool locked)
{
	if (closeButton) {
		closeButton->setEnabled(!locked);
	}

	setAdvButtonActionsEnabledByObjName("detachBtn", !locked);
}

void PLSDockTitle::setHasCloseButton(bool has)
{
	hasCloseButton = has;
}

void PLSDockTitle::setButtonPropertiesFromAction(QToolButton *button, const QAction *action) const
{
	for (const QByteArray &name : action->dynamicPropertyNames()) {
		button->setProperty(name.constData(), action->property(name.constData()));
	}

	pls_flush_style(button);
}

QString PLSDockTitle::title() const
{
	return titleLabel->text();
}

void PLSDockTitle::updateTitle(const QString &title)
{
	int buttonWidth = (buttons.count() + 1) * contentSpacing;
	for (const auto &button : buttons) {
		buttonWidth += button->width();
	}
	if (!titleWidgets.empty()) {
		buttonWidth += 10;
	}
	for (const auto &widget : titleWidgets) {
		if (!widget) {
			continue;
		}
		const auto btn = static_cast<PLSMenuPushButton *>(widget);
		if (btn) {
			buttonWidth += btn->ContentWidth();
		} else {
			buttonWidth += widget->width();
		}
	}
	int closeBtnWidth = closeButton->isVisible() ? closeButton->width() : 0;
	int availWidth = this->width() - advButton->width() - buttonWidth - marginLeft - marginRight - closeBtnWidth;
	QFontMetrics font(titleLabel->font());
	int fixedWidth = font.horizontalAdvance(title.isEmpty() ? dock->windowTitle() : title) + 2;

	int minWidth = 30;
	titleLabel->setFixedWidth(qMax(minWidth, qMin(availWidth, fixedWidth)));
	titleLabel->setText(titleLabel->fontMetrics().elidedText(title.isEmpty() ? dock->windowTitle() : title, Qt::ElideRight, qMax(minWidth, qMin(availWidth, fixedWidth))));
}

QSize PLSDockTitle::minimumSizeHint() const
{
	QSize result(DOCK_WIDGET_MIN_WIDTH, captionHeight);
	if (dock->features() & QDockWidget::DockWidgetVerticalTitleBar) {
		result.transpose();
	}
	return result;
}

void PLSDockTitle::mousePressEvent(QMouseEvent *event)
{
	// fix only left mouse button can move
	dock->setMoving(event->button() == Qt::LeftButton);
	QFrame::mousePressEvent(event);
}

void PLSDockTitle::mouseReleaseEvent(QMouseEvent *event)
{
	dock->setMoving(false);
	QFrame::mouseReleaseEvent(event);
}

bool PLSDockTitle::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == this && event->type() == QEvent::MouseButtonDblClick && dock) {
		emit dock->doubleClicked();
		return true;
	}
	return QFrame::eventFilter(watched, event);
}

void PLSDockTitle::resizeEvent(QResizeEvent *event)
{
	updateTitle();
	QFrame::resizeEvent(event);
}

void PLSDockTitle::showEvent(QShowEvent *event)
{
	pls_async_call(this, [this]() { updateTitle(); });
	QFrame::showEvent(event);
}

PLSDock::PLSDock(QWidget *parent) : PLSWidgetCloseHookQt<QDockWidget>(parent)
{
	PLS_DISABLE_UISTEP_V2(this);
	pls_add_css(this, {"PLSDock"});

	dockTitle = pls_new<PLSDockTitle>(this);
	setTitleBarWidget(dockTitle);

	// remove dock title context menu
	setContextMenuPolicy(Qt::PreventContextMenu);

	setProperty("windows", !pls_is_os_sys_macos());

#ifdef Q_OS_MAC
	setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
#endif

	content = pls_new<QFrame>();
	content->setObjectName(QStringLiteral("content"));
	content->setFrameShadow(QFrame::Plain);
	content->setFrameShape(QFrame::StyledPanel);
	contentLayout = pls_new<QHBoxLayout>(content);
	contentLayout->setContentsMargins(0, 0, 0, 0);
	contentLayout->setContentsMargins(contentMarginLeft, contentMarginTop, contentMarginRight, contentMarginBottom);
	PLSWidgetCloseHookQt<QDockWidget>::setWidget(content);
	setCursor(Qt::ArrowCursor);

	connect(this, &PLSDock::topLevelChanged, this, [this](bool toplevel) {
#if defined(PLS_UI_ACTION_STATS)
		if (auto custom_show_hide_name = pls_uistep_v2_get_custom_show_hide_name(this); custom_show_hide_name.has_value()) {
			PLS_UI_ACTION(toplevel ? "Dock %s Detached" : "Dock %s Attached", custom_show_hide_name.value().constData());
		}
#endif

		pls_flush_style(this);
		pls_flush_style(dockTitle);
		pls_flush_style(content);

		if (!toplevel) {
			setMoving(false);
		}

		if (toplevel) {
			QMetaObject::invokeMethod(this, [this] { geometryOfNormal = geometry(); }, Qt::QueuedConnection);
		}
		setChangeState(true);
		delayReleaseState();
	});
	connect(&mouseReleaseChecker, &QTimer::timeout, this, [this]() {
		if (!pls_is_mouse_pressed(Qt::LeftButton)) {
			setMoving(false);
		}
	});

	connect(this, &QDockWidget::featuresChanged, this, &PLSDock::onDockFeaturesChanged, Qt::QueuedConnection);
	onDockFeaturesChanged(features());

	pls_uistep_v2_set_title(this, [this]() { return dockTitle->title(); });
}

void PLSDock::onDockFeaturesChanged(QDockWidget::DockWidgetFeatures f)
{
	if (!dockTitle) {
		return;
	}
	const bool locked = (f == QDockWidget::NoDockWidgetFeatures);
	dockTitle->setButtonLocked(locked);
}

bool PLSDock::isMoving() const
{
	return moving;
}

static const QScreen *_getCompareAnyScreenSize(const QSize &cmpSize)
{
	QStringList screenList;
	for (const QScreen *screen : QGuiApplication::screens()) {
		if (cmpSize == screen->size()) {
			return screen;
		}
	}
	return nullptr;
}

void PLSDock::resizeEvent(QResizeEvent *event)
{
#ifdef __APPLE__
	auto printFullScreenLog = [](QResizeEvent *rEvent, const QRect &screenRect, const QRect &finnalRect, const QString &objName) {
		auto rect2String = [](const QRect &rect) { return QString("QRect(%1,%2 %3x%4)").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()); };
		auto size2String = [](const QSize &size) { return QString("QSize(%1x%2)").arg(size.width()).arg(size.height()); };

		QString gemoryLog = QString("QResizeEvent(%1 -> %2)\t screen:%3\t adjustGeometry:%4")
					    .arg(size2String(rEvent->oldSize()))
					    .arg(size2String(rEvent->size()))
					    .arg(rect2String(screenRect))
					    .arg(rect2String(finnalRect));
		PLS_INFO("PLSDock", "fullScreen adjust %s: %s", objName.toUtf8().constData(), gemoryLog.toUtf8().constData());
	};

	if (testAttribute(Qt::WA_NativeWindow)) {
		if (QSize(-1, -1) == event->oldSize()) {
			auto screen = _getCompareAnyScreenSize(event->size());
			if (screen) {
				event->ignore();

				auto finalRect = QRect(0, 0, 300, 300);
				if (objectName() == "chatDock") {
					finalRect.setSize({300, 810});
				}
				QRect screenGeometry = screen->availableGeometry();
				finalRect.setX((screenGeometry.width() - finalRect.width()) / 2);
				finalRect.setY((screenGeometry.height() - finalRect.height()) / 2);
				setGeometry(finalRect);
				printFullScreenLog(event, screen->geometry(), geometry(), objectName());

				return;
			}
		} else if (event->size() == screen()->size()) {
			event->ignore();

			setGeometry(geometry().x(), geometry().y(), event->oldSize().width(), event->oldSize().height());
			printFullScreenLog(event, screen()->geometry(), geometry(), objectName());
			return;
		}
	}
#endif
	PLSWidgetCloseHookQt<QDockWidget>::resizeEvent(event);
}
void PLSDock::setMoving(bool moving_)
{
	if (this->moving == moving_) {
		return;
	}

	this->moving = moving_;
	pls_flush_style(this);
	pls_flush_style(dockTitle);
	pls_flush_style(content);

	if (this->moving) {
		mouseReleaseChecker.start(100);
		// Mark layout change so any Show during/after drag (e.g. when floating window appears) does not emit dockReallyShown.
		setChangeState(true);
		delayReleaseState();
	} else {
		mouseReleaseChecker.stop();
		// Also mark and delay on release: Show may come after drag-in, and the start-drag timer may have already expired.
		setChangeState(true);
		delayReleaseState();
	}
}

bool PLSDock::isMovingAndFloating() const
{
	return isMoving() && isFloating();
}

int PLSDock::getContentMarginLeft() const
{
	return contentMarginLeft;
}

void PLSDock::setContentMarginLeft(int contentMarginLeft_)
{
	this->contentMarginLeft = contentMarginLeft_;
	contentLayout->setContentsMargins(contentMarginLeft, contentMarginTop, contentMarginRight, contentMarginBottom);
}

int PLSDock::getContentMarginTop() const
{
	return contentMarginTop;
}

void PLSDock::setContentMarginTop(int contentMarginTop_)
{
	this->contentMarginTop = contentMarginTop_;
	contentLayout->setContentsMargins(contentMarginLeft, contentMarginTop, contentMarginRight, contentMarginBottom);
}

int PLSDock::getContentMarginRight() const
{
	return contentMarginRight;
}

void PLSDock::setContentMarginRight(int contentMarginRight_)
{
	this->contentMarginRight = contentMarginRight_;
	contentLayout->setContentsMargins(contentMarginLeft, contentMarginTop, contentMarginRight, contentMarginBottom);
}

int PLSDock::getContentMarginBottom() const
{
	return contentMarginBottom;
}

void PLSDock::setContentMarginBottom(int contentMarginBottom_)
{
	this->contentMarginBottom = contentMarginBottom_;
	contentLayout->setContentsMargins(contentMarginLeft, contentMarginTop, contentMarginRight, contentMarginBottom);
}

PLSDockTitle *PLSDock::titleWidget() const
{
	return dockTitle;
}

QWidget *PLSDock::widget() const
{
	return owidget;
}
void PLSDock::setWidget(QWidget *widget)
{
	if (!widget->windowTitle().isEmpty()) {
		this->setWindowTitle(widget->windowTitle());
	}
	this->owidget = widget;
	widget->setParent(content);
	contentLayout->addWidget(widget);
}

QWidget *PLSDock::getContent() const
{
	return content;
}

void PLSDock::delayReleaseState()
{
	if (delayTimer == nullptr) {
		delayTimer = QSharedPointer<QTimer>::create(this);
		delayTimer->setInterval(DOCK_LAYOUT_CHANGE_SUPPRESS_MS);
		delayTimer->setSingleShot(true);
		connect(delayTimer.data(), &QTimer::timeout, this, [this]() { isChangeTopLevel = false; });
	}
	delayTimer->start();
}

void PLSDock::hideEvent(QHideEvent *event)
{
	m_wasHidden = true;
	PLSWidgetCloseHookQt<QDockWidget>::hideEvent(event);
}

void PLSDock::closeEvent(QCloseEvent *event)
{
	if (pls_is_app_exiting()) {
		PLSWidgetCloseHookQt<QDockWidget>::closeEvent(event);
	} else {
		event->ignore();
		hide();
	}
}

void PLSDock::paintEvent(QPaintEvent *)
{
	QStylePainter painter(this);
	if (isFloating() || isMoving()) {
		QStyleOptionFrame framOpt;
		framOpt.initFrom(this);
		painter.drawPrimitive(QStyle::PE_FrameDockWidget, framOpt);
	}
}

bool PLSDock::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::MouseButtonRelease:
		setMoving(false);
		return PLSWidgetCloseHookQt<QDockWidget>::event(event);
	case QEvent::ChildAdded:
		return PLSWidgetCloseHookQt<QDockWidget>::event(event);
	case QEvent::Show: {
		const bool reallyShown = !isMoving() && !isChangeTopLevel && (m_wasHidden || !m_hasEverBeenShown);
		if (reallyShown) {
			m_wasHidden = false;
			m_hasEverBeenShown = true;
			emit dockReallyShown();
		}
		pls_async_call(this, [this]() {
			raise();
			activateWindow();
		});
		if (isFloating()) {
#if __APPLE__
			PLSCustomMacWindow::removeMacTitleBar(this);
#endif
			pls_window_left_right_margin_fit(this);
			printChatGeometryLog("PLSDock after fit");
		}
		return PLSWidgetCloseHookQt<QDockWidget>::event(event);
	}
	default:
		return PLSWidgetCloseHookQt<QDockWidget>::event(event);
	}
}

void PLSDock::printChatGeometryLog(const QString &preLog)
{
	if (objectName() != "chatDock") {
		return;
	}
	auto rect2String = [](const QRect &rect) { return QString("QRect(%1,%2 %3x%4)").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()); };

	QStringList screenList;
	for (const QScreen *screen : QGuiApplication::screens()) {
		screenList.append(rect2String(screen->availableGeometry()));
	}

	QString gemoryLog =
		QString("%1\tisShown:%2\tisFloating:%3\tgeometry:%4\tscreens:%5").arg(objectName()).arg(isVisible()).arg(isFloating()).arg(rect2String(geometry())).arg(screenList.join(","));
	PLS_INFO("PLSDock", "%s %s", preLog.toUtf8().constData(), gemoryLog.toUtf8().constData());
}
