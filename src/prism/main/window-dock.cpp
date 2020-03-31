#include "window-dock.hpp"
#include "pls-app.hpp"
#include "main-view.hpp"

#include <QCheckBox>
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionFrame>

#include <QMenu>
#include <QDynamicPropertyChangeEvent>
#include <QStyle>
#include <QMouseEvent>

#include <private/qwidgetresizehandler_p.h>

#include <Windows.h>

#include <alert-view.hpp>

#include "PLSMenu.hpp"
#include "window-basic-main.hpp"
#include "frontend-api.h"

#define DOCK_WIDGET_MIN_WIDTH 20

class PLSDockTitlePopupMenu {
public:
	virtual ~PLSDockTitlePopupMenu() {}

	virtual void exec(const QPoint &pt) = 0;
};

namespace {
class PLSDockTitlePopupMenuQMenu : public PLSDockTitlePopupMenu {
	QMenu *m_menu;

public:
	PLSDockTitlePopupMenuQMenu(QMenu *menu) : m_menu(menu) {}

	virtual void exec(const QPoint &pt) { m_menu->exec(pt); }
};

class PLSDockTitlePopupMenuPLSPopupMenu : public PLSDockTitlePopupMenu {
	PLSPopupMenu *m_menu;

public:
	PLSDockTitlePopupMenuPLSPopupMenu(PLSPopupMenu *menu) : m_menu(menu) {}

	virtual void exec(const QPoint &pt) { m_menu->exec(pt); }
};
}

PLSDockTitle::PLSDockTitle(PLSDock *parent) : QFrame(parent), dock(parent)
{
	captionHeight = GetSystemMetrics(SM_CYCAPTION);
	marginLeft = 0;
	marginRight = 0;
	contentSpacing = 10;
	titleLabel = new QLabel(parent->windowTitle(), this);
	titleLabel->setObjectName("titleLabel");
	titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	advButton = new QToolButton(this);
	advButton->setObjectName(QString::fromUtf8("advButton"));
	advButtonMenu = nullptr;

	buttonsLayout = new QHBoxLayout();
	buttonsLayout->setSpacing(contentSpacing);
	buttonsLayout->setMargin(0);

	QHBoxLayout *l = new QHBoxLayout(this);
	l->setSpacing(contentSpacing);
	l->setMargin(1);
	l->addWidget(titleLabel, 1);
	l->addLayout(buttonsLayout);
	l->addWidget(advButton);

	advButton->hide();

	connect(parent, &QWidget::windowTitleChanged, titleLabel, &QLabel::setText);
	connect(advButton, &QToolButton::clicked, [this]() {
		if (advButtonMenu) {
			advButtonMenu->exec(QCursor::pos());
			advButton->update();
		}
	});
}

PLSDockTitle::~PLSDockTitle()
{
	if (advButtonMenu) {
		delete advButtonMenu;
		advButtonMenu = nullptr;
	}
}

int PLSDockTitle::getCaptionHeight() const
{
	return captionHeight;
}

void PLSDockTitle::setCaptionHeight(int captionHeight)
{
	this->captionHeight = captionHeight;
	update();
}

int PLSDockTitle::getMarginLeft() const
{
	return marginLeft;
}

void PLSDockTitle::setMarginLeft(int marginLeft)
{
	this->marginLeft = marginLeft;
	setContentsMargins(marginLeft, 0, marginRight, 0);
	update();
}

int PLSDockTitle::getMarginRight() const
{
	return marginRight;
}

void PLSDockTitle::setMarginRight(int marginRight)
{
	this->marginRight = marginRight;
	setContentsMargins(marginLeft, 0, marginRight, 0);
	update();
}

int PLSDockTitle::getContentSpacing() const
{
	return contentSpacing;
}

void PLSDockTitle::setContentSpacing(int contentSpacing)
{
	this->contentSpacing = contentSpacing;
	layout()->setSpacing(contentSpacing);
	update();
}

QList<QToolButton *> PLSDockTitle::getButtons() const
{
	return buttons;
}

QToolButton *PLSDockTitle::getAdvButton() const
{
	return advButton;
}

void PLSDockTitle::setButtonActions(QList<QAction *> actions)
{
	buttonActions = actions;

	for (auto &action : actions) {
		QToolButton *button = new QToolButton(this);
		button->setObjectName(action->objectName());
		setButtonPropertiesFromAction(button, action);
		connect(button, &QToolButton::clicked, action, &QAction::triggered);
		button->setToolTip(action->toolTip());
		buttonsLayout->addWidget(button);
		buttons.append(button);
	}
}

void PLSDockTitle::setAdvButtonMenu(QMenu *menu)
{
	if (advButtonMenu) {
		delete advButtonMenu;
		advButtonMenu = nullptr;
	}

	if (menu) {
		advButtonMenu = new PLSDockTitlePopupMenuQMenu(menu);
		advButton->show();
	}
}

void PLSDockTitle::setAdvButtonMenu(PLSPopupMenu *menu)
{
	if (advButtonMenu) {
		delete advButtonMenu;
		advButtonMenu = nullptr;
	}

	if (menu) {
		advButtonMenu = new PLSDockTitlePopupMenuPLSPopupMenu(menu);
		advButton->show();
	}
}

void PLSDockTitle::setAdvButtonActions(QList<QAction *> actions)
{
	if (advButtonMenu) {
		delete advButtonMenu;
		advButtonMenu = nullptr;
	}

	PLSPopupMenu *menu = new PLSPopupMenu(this);
	menu->addActions(actions);

	advButtonMenu = new PLSDockTitlePopupMenuPLSPopupMenu(menu);
	advButton->show();
}

void PLSDockTitle::setButtonPropertiesFromAction(QToolButton *button, QAction *action)
{
	for (const QByteArray &name : action->dynamicPropertyNames()) {
		button->setProperty(name.constData(), action->property(name.constData()));
	}

	pls_flush_style(button);
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

PLSWidgetResizeHandler::PLSWidgetResizeHandler(PLSDock *dock_, QWidgetResizeHandler *resizeHandler_) : QObject(dock_), dock(dock_), resizeHandler(resizeHandler_), isForResize(false), isResizing(false)
{
	dock->removeEventFilter(resizeHandler);
	dock->installEventFilter(this);

	resizePausedCheckTimer.setSingleShot(true);
	resizePausedCheckTimer.setInterval(200);

	connect(&resizePausedCheckTimer, &QTimer::timeout, this, [this]() {
		if (isResizing) {
			isResizing = false;
			resizePausedCheckTimer.stop();
			emit dock->endResizeSignal();
		}
	});
}

PLSWidgetResizeHandler::~PLSWidgetResizeHandler() {}

bool PLSWidgetResizeHandler::eventFilter(QObject *watched, QEvent *event)
{
	QObject *resizeHandlerObj = resizeHandler;
	switch (event->type()) {
	case QEvent::MouseButtonPress: {
		bool retval = resizeHandlerObj->eventFilter(watched, event);
		isForResize = resizeHandler->isButtonDown() && resizeHandler->isActive(QWidgetResizeHandler::Resize);
		isResizing = false;
		dockSizeMousePress = dock->size();
		globalPosMouseMoveForResize = static_cast<QMouseEvent *>(event)->globalPos();
		return retval;
	}
	case QEvent::MouseButtonRelease: {
		if (isResizing) {
			isResizing = false;
			resizePausedCheckTimer.stop();
			emit dock->endResizeSignal();
		}
		if (isForResize) {
			isForResize = false;
		}
		return resizeHandlerObj->eventFilter(watched, event);
	}
	case QEvent::MouseMove:
		if (isForResize && (dockSizeMousePress != dock->size())) {
			QPoint globalPos = static_cast<QMouseEvent *>(event)->globalPos();
			if (globalPosMouseMoveForResize != globalPos) {
				globalPosMouseMoveForResize = globalPos;

				if (!isResizing) {
					isResizing = true;
					resizePausedCheckTimer.start();
					emit dock->beginResizeSignal();
				} else {
					resizePausedCheckTimer.stop();
					resizePausedCheckTimer.start();
				}
			}
		}
		return resizeHandlerObj->eventFilter(watched, event);
	default:
		return resizeHandlerObj->eventFilter(watched, event);
	}
}

PLSDock::PLSDock(QWidget *parent) : QDockWidget(parent), moving(false), contentMarginLeft(0), contentMarginTop(0), contentMarginRight(0), contentMarginBottom(0)
{
	setTitleBarWidget(dockTitle = new PLSDockTitle(this));
	// remove dock title context menu
	setContextMenuPolicy(Qt::PreventContextMenu);

	content = new QFrame();
	content->setObjectName(QStringLiteral("content"));
	content->setFrameShadow(QFrame::Plain);
	content->setFrameShape(QFrame::StyledPanel);
	contentLayout = new QHBoxLayout(content);
	contentLayout->setMargin(0);
	contentLayout->setContentsMargins(contentMarginLeft, contentMarginTop, contentMarginRight, contentMarginBottom);
	QDockWidget::setWidget(content);
	owidget = nullptr;

	resizeHandlerProxy = nullptr;

	connect(this, &PLSDock::topLevelChanged, this, [this]() {
		pls_flush_style(this);
		pls_flush_style(dockTitle);
		pls_flush_style(content);
	});

	QWidget *toplevelView = pls_get_toplevel_view(this);
	connect(toplevelView, SIGNAL(beginResizeSignal()), this, SLOT(beginResizeSlot()));
	connect(toplevelView, SIGNAL(endResizeSignal()), this, SLOT(endResizeSlot()));

	connect(
		this, &PLSDock::initResizeHandlerProxySignal, this,
		[this](QObject *child) {
			if (!resizeHandlerProxy) {
				if (QWidgetResizeHandler *resizeHandler = dynamic_cast<QWidgetResizeHandler *>(child); resizeHandler) {
					resizeHandlerProxy = new PLSWidgetResizeHandler(this, resizeHandler);
				}
			}
		},
		Qt::QueuedConnection);
}

bool PLSDock::isMoving() const
{
	return moving;
}

void PLSDock::setMoving(bool moving)
{
	if (this->moving == moving) {
		return;
	}

	this->moving = moving;
	pls_flush_style(this);
	pls_flush_style(dockTitle);
	pls_flush_style(content);
}

bool PLSDock::isMovingAndFloating() const
{
	return isMoving() && isFloating();
}

int PLSDock::getContentMarginLeft() const
{
	return contentMarginLeft;
}

void PLSDock::setContentMarginLeft(int contentMarginLeft)
{
	this->contentMarginLeft = contentMarginLeft;
	contentLayout->setContentsMargins(contentMarginLeft, contentMarginTop, contentMarginRight, contentMarginBottom);
}

int PLSDock::getContentMarginTop() const
{
	return contentMarginTop;
}

void PLSDock::setContentMarginTop(int contentMarginTop)
{
	this->contentMarginTop = contentMarginTop;
	contentLayout->setContentsMargins(contentMarginLeft, contentMarginTop, contentMarginRight, contentMarginBottom);
}

int PLSDock::getContentMarginRight() const
{
	return contentMarginRight;
}

void PLSDock::setContentMarginRight(int contentMarginRight)
{
	this->contentMarginRight = contentMarginRight;
	contentLayout->setContentsMargins(contentMarginLeft, contentMarginTop, contentMarginRight, contentMarginBottom);
}

int PLSDock::getContentMarginBottom() const
{
	return contentMarginBottom;
}

void PLSDock::setContentMarginBottom(int contentMarginBottom)
{
	this->contentMarginBottom = contentMarginBottom;
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
	this->setWindowTitle(widget->windowTitle());
	this->owidget = widget;
	widget->setParent(content);
	contentLayout->addWidget(widget);
}

QWidget *PLSDock::getContent() const
{
	return content;
}

void PLSDock::beginResizeSlot()
{
	if (!isFloating()) {
		emit beginResizeSignal();
	}
}

void PLSDock::endResizeSlot()
{
	if (!isFloating()) {
		emit endResizeSignal();
	}
}

void PLSDock::closeEvent(QCloseEvent *event)
{
	auto msgBox = []() {
		PLSAlertView::Result result = PLSAlertView::information(App()->getMainView(), QTStr("DockCloseWarning.Title"), QTStr("DockCloseWarning.Text"), QTStr("DoNotShowAgain"),
									PLSAlertView::Button::Ok, PLSAlertView::Button::Ok);
		if (result.isChecked) {
			config_set_bool(App()->GlobalConfig(), "General", "WarnedAboutClosingDocks", true);
			config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
		}
	};

	bool warned = config_get_bool(App()->GlobalConfig(), "General", "WarnedAboutClosingDocks");
	if (!warned) {
		QMetaObject::invokeMethod(App(), "Exec", Qt::QueuedConnection, Q_ARG(VoidFunc, msgBox));
	}

	QDockWidget::closeEvent(event);
}

void PLSDock::paintEvent(QPaintEvent *event)
{
	QStylePainter painter(this);
	if (isFloating() || isMoving()) {
		QStyleOptionFrame framOpt;
		framOpt.init(this);
		painter.drawPrimitive(QStyle::PE_FrameDockWidget, framOpt);
	}
}

bool PLSDock::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::MouseButtonRelease:
		setMoving(false);
		return QDockWidget::event(event);
	case QEvent::MouseButtonDblClick: {
		setMoving(false);
		if (!isFloating()) {
			PLSBasic *basic = PLSBasic::Get();
			QPoint pre = basic->mapToGlobal(pos());
			bool result = QDockWidget::event(event);
			basic->docksMovePolicy(this, pre);
			return result;
		} else {
			return QDockWidget::event(event);
		}
	}
	case QEvent::ChildAdded: {
		if (!resizeHandlerProxy) {
			emit initResizeHandlerProxySignal(static_cast<QChildEvent *>(event)->child());
		}
	}
	default:
		return QDockWidget::event(event);
	}
}
