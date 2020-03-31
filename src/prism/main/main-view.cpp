#include "main-view.hpp"
#include "ui_PLSMainView.h"

#include <QResizeEvent>
#include <QPainter>
#include <QStyle>
#include <QWidgetAction>
#include <QLabel>
#include <QKeySequence>
#include <QLabel>
#include <qdatetime.h>
#include <qdesktopwidget.h>

#include "log.h"
#include "action.h"
#include "log/module_names.h"
#include "pls-app.hpp"
#include "window-basic-main.hpp"
#include "toast-view.hpp"
#include "PLSChannelsVirualAPI.h"
#include "custom-help-menu-item.hpp"
#include "frontend-api.h"
#include "PLSLivingMsgView.hpp"
#include "PLSToastMsgPopup.hpp"
#include "login-common-helper.hpp"

#if defined(Q_OS_WINDOWS)
#include <Windows.h>
#endif // Q_OS_WINDOWS

#include "PLSPlatformApi.h"
#include <QTimer>

#define LIVINGMSGVIEW "LivingMsgView"
#define MAINVIEWMODE "mainViewMode"

#define EMAIL_TYPE QStringLiteral("email")

extern float getDevicePixelRatio();
extern float getDevicePixelRatio(QWidget *widget);
extern QRect getScreenRect(QWidget *widget);
extern QRect getScreenAvailableRect(QWidget *widget);

void showChatView(bool isRebackLogin, bool isOnlyShow, bool isOnlyInit);

static inline QPoint operator-(const QPoint &pt, const QSize &size)
{
	return QPoint(pt.x() - size.width(), pt.y() - size.height());
}
template<typename Type> static void limitResize(int min, int max, Type &begin, Type &end)
{
	int distance = end - begin;
	if ((min > 0) && (min < QWIDGETSIZE_MAX)) {
		if (std::abs(distance) < min) {
			end = distance > 0 ? (begin + min) : (begin - min);
		}
	}

	if ((max > 0) && (max < QWIDGETSIZE_MAX)) {
		if (std::abs(distance) > max) {
			end = distance > 0 ? (begin + max) : (begin - max);
		}
	}
}

PLSMainView::PLSMainView(QWidget *parent) : PLSToplevelView<QFrame>(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint), ui(new Ui::PLSMainView)
{
	ui->setupUi(this);
	setMinimumSize(650, 618);
	resize(1310, 802);
	setMouseTracking(true);
	ui->titleBar->setMouseTracking(true);
	ui->body->setMouseTracking(true);
	ui->content->setMouseTracking(true);
	ui->rightArea->setMouseTracking(true);
	ui->bottomArea->setMouseTracking(true);
	ui->channelsArea->setMouseTracking(true);
	ui->channelsArea->installEventFilter(this);
	ui->alert->setVisible(false);
	ui->chat->setVisible(false);
	captionHeight = captionButtonSize = GetSystemMetrics(SM_CYCAPTION);
	captionButtonMargin = 0;
	logoMarginLeft = 8;
	menuButtonMarginLeft = 16;
	menuButtonSize = QSize(34, 26);
	closeButtonMarginRight = 8;
	rightAreaWidth = 30;
	bottomAreaHeight = 30;
	channelsAreaHeight = 70;
	closeEventCallback = [this](QCloseEvent *e) { callBaseCloseEvent(e); };
	m_livingToastViewShow = false;

	toast = new PLSToastView(this);
	m_toastMsg = new PLSToastMsgPopup(this);
	m_toastMsg->hide();
	//m_livingMsgView.hide();
	QObject::connect(ui->min, &QToolButton::clicked, this, &PLSMainView::showMinimized);
	QObject::connect(ui->maxres, &QToolButton::clicked, [this]() {
		if (!isMaxState && !isFullScreenState) {
			showMaximized();
		} else {
			showNormal();
		}
	});
	QObject::connect(ui->close, &QToolButton::clicked, this, &PLSMainView::close);

	QObject::connect(&m_livingMsgView, &PLSLivingMsgView::hideSignal, [=]() { ui->alert->updateIconStyle(false); });

	QObject::connect(PLS_PLATFORM_API, &PLSPlatformApi::enterLivePrepareState, this, [=](bool disable) { ui->settings->setEnabled(!disable); });

	//default the tooltip only show the active window, so need to change tooltip default action, modify the widget window attribute
	this->setAttribute(Qt::WA_AlwaysShowToolTips, true);
}

PLSMainView::~PLSMainView()
{
	pls_frontend_remove_event_callback(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGOUT, PLSMainView::resetToastViewPostion, this);
	delete ui;
}

QWidget *PLSMainView::content() const
{
	return ui->content;
}

QWidget *PLSMainView::channelsArea() const
{
	return ui->channelsArea;
}

QPushButton *PLSMainView::menuButton() const
{
	return ui->menu;
}

PLSBasicStatusBar *PLSMainView::statusBar() const
{
	return ui->bottomArea;
}

int PLSMainView::getCaptionHeight() const
{
	return captionHeight;
}

void PLSMainView::setCaptionHeight(int captionHeight)
{
	this->captionHeight = captionHeight - BORDER_WIDTH;
	ui->titleBar->setFixedHeight(captionHeight);
}

int PLSMainView::getCaptionButtonSize() const
{
	return captionButtonSize;
}

void PLSMainView::setCaptionButtonSize(int captionButtonSize)
{
	this->captionButtonSize = captionButtonSize;
	ui->min->setFixedSize(captionButtonSize, captionButtonSize);
	ui->maxres->setFixedSize(captionButtonSize, captionButtonSize);
	ui->close->setFixedSize(captionButtonSize, captionButtonSize);
}

int PLSMainView::getCaptionButtonMargin() const
{
	return captionButtonMargin;
}

void PLSMainView::setCaptionButtonMargin(int captionButtonMargin)
{
	this->captionButtonMargin = captionButtonMargin;
	ui->titleButtonLayout->setSpacing(captionButtonMargin);
}

int PLSMainView::getLogoMarginLeft() const
{
	return logoMarginLeft;
}

void PLSMainView::setLogoMarginLeft(int logoMarginLeft)
{
	this->logoMarginLeft = logoMarginLeft;
	//updateLayout(this->size());
}

int PLSMainView::getMenuButtonMarginLeft() const
{
	return menuButtonMarginLeft;
}

void PLSMainView::setMenuButtonMarginLeft(int menuButtonMarginLeft)
{
	this->menuButtonMarginLeft = menuButtonMarginLeft;
	QMargins margins = ui->titleBarLayout->contentsMargins();
	margins.setLeft(menuButtonMarginLeft);
	ui->titleBarLayout->setContentsMargins(margins);
}

QSize PLSMainView::getMenuButtonSize() const
{
	return menuButtonSize;
}

void PLSMainView::setMenuButtonSize(const QSize &menuButtonSize)
{
	this->menuButtonSize = menuButtonSize;
	ui->menu->setFixedSize(menuButtonSize);
}

int PLSMainView::getCloseButtonMarginRight() const
{
	return closeButtonMarginRight;
}

void PLSMainView::setCloseButtonMarginRight(int closeButtonMarginRight)
{
	this->closeButtonMarginRight = closeButtonMarginRight;
	QMargins margins = ui->titleBarLayout->contentsMargins();
	margins.setRight(closeButtonMarginRight);
	ui->titleBarLayout->setContentsMargins(margins);
}

int PLSMainView::getRightAreaWidth() const
{
	return rightAreaWidth;
}

void PLSMainView::setRightAreaWidth(int rightAreaWidth)
{
	this->rightAreaWidth = rightAreaWidth;
	ui->rightArea->setFixedWidth(this->rightAreaWidth);
}

int PLSMainView::getBottomAreaHeight() const
{
	return bottomAreaHeight;
}

void PLSMainView::setBottomAreaHeight(int bottomAreaHeight)
{
	this->bottomAreaHeight = bottomAreaHeight;
	ui->bottomArea->setFixedHeight(this->bottomAreaHeight);
}

int PLSMainView::getChannelsAreaHeight() const
{
	return channelsAreaHeight;
}

void PLSMainView::setChannelsAreaHeight(int channelsAreaHeight)
{
	this->channelsAreaHeight = channelsAreaHeight;
	ui->channelsArea->setFixedHeight(this->channelsAreaHeight);
}

void PLSMainView::setCloseEventCallback(std::function<void(QCloseEvent *)> closeEventCallback)
{
	this->closeEventCallback = closeEventCallback;
}

void PLSMainView::callBaseCloseEvent(QCloseEvent *event)
{
	QFrame::closeEvent(event);
}

void PLSMainView::setStudioMode(bool studioMode)
{
	ui->studioMode->setChecked(studioMode);
}

void PLSMainView::toastMessage(pls_toast_info_type type, const QString &message, int autoClose)
{
	PLS_INFO(NOTICE_MODULE, QString("a new toast message:%1").arg(message).toUtf8().data());

	qint64 currtime = QDateTime::currentMSecsSinceEpoch();
	toastMessages.insert(currtime, message);
	ui->alert->setNum(toastMessages.size());

	m_toastMsg->showMsg(message, static_cast<pls_toast_info_type>(type));
	int width = ui->content->width();
	QPoint pt = ui->content->mapToGlobal(QPoint(width, 0));
	pt = mapFromParent(pt);
	m_toastMsg->show();
	m_toastMsg->move(pt.x() - m_toastMsg->width() - 10, pt.y() + 10);

	//add toast msg view
	m_livingMsgView.addMsgItem(message, currtime, type);
}

void PLSMainView::toastClear()
{
	toastMessages.clear();
	ui->alert->setNum(toastMessages.size());
	m_toastMsg->hide();
	m_livingMsgView.clearMsgView();
}

void PLSMainView::setUserButtonIcon(const QIcon &icon)
{
	ui->user->setIcon(icon);
}

void PLSMainView::initToastMsgView()
{
	static bool registerOne = true;
	if (registerOne) {
		pls_frontend_add_event_callback(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGOUT, PLSMainView::resetToastViewPostion, this);
		registerOne = false;
	}

	const char *geometry = nullptr;
	geometry = config_get_string(App()->GlobalConfig(), LIVINGMSGVIEW, "geometry");
	if (QString(geometry).isEmpty()) {
		const static int defaultWidth = 300;
		const static int defaultHeight = 400;
		const static int mainRightOffest = 5;
		PLSMainView *mainView = App()->getMainView();
		QPoint mainTopRight = App()->getMainView()->mapToGlobal(QPoint(mainView->frameGeometry().width(), 0));
		m_livingMsgView.setGeometry(mainTopRight.x() + mainRightOffest, mainTopRight.y(), defaultWidth, defaultHeight);

	} else {
		QByteArray byteArray = QByteArray::fromBase64(QByteArray(geometry));
		m_livingMsgView.restoreGeometry(byteArray);
	}
	pls_window_right_margin_fit(&m_livingMsgView);

	bool isVisible = config_get_bool(App()->GlobalConfig(), LIVINGMSGVIEW, "showMode");
	if (!isVisible) {
		ui->alert->updateIconStyle(false);
	} else {
		ui->alert->updateIconStyle(true);
	}
	m_livingMsgView.setVisible(isVisible);
}
PLSToastButton *PLSMainView::getToastButton()
{
	return ui->alert;
}
void PLSMainView::showMainFloatView()
{
	//show toast view ;chat view
	initToastMsgView();
	if (!config_get_bool(App()->GlobalConfig(), LIVINGMSGVIEW, "showMode")) {
		if (m_livingToastViewShow) {
			m_livingMsgView.setVisible(true);
		} else {
			m_livingMsgView.setVisible(false);
		}
	} else {
		m_livingMsgView.setVisible(true);
	}

	if (!m_livingToastViewShow) {
		m_livingMsgView.setVisible(false);
	} else {
	}
	showChatView(false, true);
}
void PLSMainView::hideMainFloatView()
{
	//hide toast view ;chat view
	m_livingToastViewShow = m_livingMsgView.isVisible();
	m_livingMsgView.hide();

	showChatView(true);
}

QRect PLSMainView::titleBarRect() const
{
	return ui->titleBar->geometry();
}

bool PLSMainView::canMaximized() const
{
	return true;
}

bool PLSMainView::canFullScreen() const
{
	return true;
}

void PLSMainView::flushMaxFullScreenStateStyle()
{
	pls_flush_style(ui->maxres);
}

void PLSMainView::onMaxFullScreenStateChanged()
{
	config_set_bool(App()->GlobalConfig(), "BasicWindow", "MaximizedState", isMaxState);
	config_set_bool(App()->GlobalConfig(), "BasicWindow", "FullScreenState", isFullScreenState);
}

void PLSMainView::onSaveNormalGeometry()
{
	config_set_string(App()->GlobalConfig(), "BasicWindow", "geometry", saveGeometry().toBase64().constData());
}

bool PLSMainView::isClosing() const
{
	return closing;
}
void PLSMainView::close()
{
	if (!closing) {
		closing = true;
		QFrame::close();
	}
}

void PLSMainView::onUpdateChatSytle(bool isShow)
{
	ui->chat->setProperty("hasShowChat", isShow);
	LoginCommonHelpers::refreshStyle(ui->chat);
}

void PLSMainView::resetToastViewPostion(pls_frontend_event event, const QVariantList &params, void *context)
{
	if (event == pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGOUT) {
		config_set_string(App()->GlobalConfig(), "LivingMsgView", "geometry", "");
		config_set_bool(App()->GlobalConfig(), "LivingMsgView", "showMode", false);
		static_cast<PLSMainView *>(pls_get_main_view())->m_livingToastViewShow = false;
		config_set_bool(App()->GlobalConfig(), "General", "Watermark", true);
		config_save(App()->GlobalConfig());
	}
}

void PLSMainView::on_menu_clicked()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Menu Button", ACTION_CLICK);
}

void PLSMainView::on_user_clicked()
{
	if (!ui->settings->isEnabled()) {
		return;
	}
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar User Button", ACTION_CLICK);
	emit popupSettingView(QStringLiteral("General"), QStringLiteral("user"));
}

void PLSMainView::on_studioMode_clicked()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Studio Mode Button", ACTION_CLICK);
	emit studioModeChanged();
}

void PLSMainView::on_chat_clicked()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Chat Button", ACTION_CLICK);
	showChatView(false, true);
}

void PLSMainView::on_alert_clicked()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Alert Button", ACTION_CLICK);
	m_livingMsgView.setVisible(!m_livingMsgView.isVisible());
}

void PLSMainView::on_settings_clicked()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Setting Button", ACTION_CLICK);
	emit popupSettingView(QStringLiteral("General"), QString());
	ui->settings->setChecked(false);
}

void PLSMainView::closeEvent(QCloseEvent *event)
{
	bool isLivingView = config_get_bool(App()->GlobalConfig(), LIVINGMSGVIEW, "showMode");

	//config_set_bool(App()->GlobalConfig(), LIVINGMSGVIEW, MAINVIEWMODE, isLivingView);

	closeEventCallback(event);
}

void PLSMainView::showEvent(QShowEvent *event)
{
	//if (config_get_bool(App()->GlobalConfig(), LIVINGMSGVIEW, "showMode")) {
	//	on_alert_clicked();
	//} else {
	//	m_livingMsgView.setVisible(false);
	//}
	// set user photo tooltip
	ui->user->setToolTip(pls_get_prism_nickname());
	emit isshowSignal(true);
	QFrame::showEvent(event);
}

void PLSMainView::hideEvent(QHideEvent *event)
{
	//config_set_bool(App()->GlobalConfig(), LIVINGMSGVIEW, MAINVIEWMODE, m_livingMsgView.isVisible());
	//m_livingMsgView.setVisible(false);
	emit isshowSignal(false);
	config_save(App()->GlobalConfig());
	QFrame::hideEvent(event);
}

void PLSMainView::mousePressEvent(QMouseEvent *event)
{
	mousePress(event);
}

void PLSMainView::mouseReleaseEvent(QMouseEvent *event)
{
	mouseRelease(event);
}

void PLSMainView::mouseDoubleClickEvent(QMouseEvent *event)
{
	mouseDbClick(event);
}

void PLSMainView::mouseMoveEvent(QMouseEvent *event)
{
	mouseMove(event);
}

bool PLSMainView::eventFilter(QObject *watcher, QEvent *event)
{
	if (watcher == ui->channelsArea) {
		switch (event->type()) {
		case QEvent::ChildAdded:
			if (QWidget *widget = dynamic_cast<QWidget *>(static_cast<QChildEvent *>(event)->child()); widget) {
				widget->setCursor(Qt::ArrowCursor);
			}
			break;
		}
	}

	return QFrame::eventFilter(watcher, event);
}
