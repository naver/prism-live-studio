#include "PLSMainView.hpp"
#include "ui_PLSMainView.h"
#include "PLSToastButton.hpp"
#include "window-basic-main.hpp"
#include "PLSMainViewConstValues.h"
#include "PLSToastMsgPopup.hpp"
#include "PLSLivingMsgView.hpp"
#include "login-user-info.hpp"

#include <QResizeEvent>
#include <QPainter>
#include <QStyle>
#include <QWidgetAction>
#include <QLabel>
#include <QKeySequence>
#include <QLabel>
#include <qdatetime.h>
#include <QMetaEnum>
#include <QButtonGroup>

#include "PLSBasic.h"

#include <libutils-api.h>
#include <libui.h>
#include <liblog.h>
#include <log/module_names.h>
#include <pls/pls-source.h>
#include "PLSContactView.hpp"
#include "ResolutionTipFrame.h"
#include "ChannelCommonFunctions.h"
#include "ResolutionGuidePage.h"
#include "PLSAboutView.hpp"
#include "PLSChatDialog.h"
#include "PLSAction.h"
#include "PLSLaunchWizardView.h"

#if defined(Q_OS_WINDOWS)
#include <Windows.h>
#include "windows/PLSBlockDump.h"
#include "windows/PLSModuleMonitor.h"
#endif // Q_OS_WINDOWS

#if defined(Q_OS_MACOS)
#include "mac/PLSBlockDump.h"
#endif

static auto PC_STATE_FILE = "pc_state_log.txt";

const QString sideBarButtonSizeStyle = "#%1 {"
				       "min-width: /*hdpi*/ %2px;"
				       "max-width: /*hdpi*/ %3px;"
				       "min-height: /*hdpi*/ %4px;"
				       "max-height: /*hdpi*/ %5px;"
				       "padding: 0;"
				       "margin: 0;}";

const QString sideBarStyle = "#%1 {"
			     "image: url(\"%2\");"
			     "background: transparent;}"
			     "#%1[showMode=\"false\"]{"
			     "image:url(\"%2\");}"
			     "#%1[showMode =\"false\"]:hover{"
			     "image:url(\"%3\");}"
			     "#%1[showMode =\"false\"]:pressed{"
			     "image:url(\"%4\");}"
			     "#%1[showMode =\"false\"]:!enable{"
			     "image:url(\"%5\");}"
			     "#%1[showMode=\"true\"]{"
			     "image:url(\"%6\");}"
			     "#%1[showMode=\"true\"]:hover{"
			     "image:url(\"%7\");}"
			     "#%1[showMode =\"true\"]:pressed{"
			     "image:url(\"%8\");}"
			     "#%1[showMode=\"true\"]:!enable{"
			     "image:url(\"%9\");}";

enum HelpWidgetItemTag { UserGuide = 0, PrismFAQ, PrismWebsite, ContactUs, CheckForUpdate };

PLSMainView::PLSMainView(QWidget *parent) : PLSToplevelView<QFrame>(parent)
{
#ifdef _WIN32
	// firstly read, then run detect
	readDetectResult();
	runNewDetect();
#endif

	pls_set_main_view(this);
	setWindowIcon(QIcon(":/resource/images/logo/PRISMLiveStudio.ico"));

	ui = pls_new<Ui::PLSMainView>();
	ui->setupUi(this);
	pls_set_css(this, {"PLSMainView", "HelpMenu"});

	ui->logo_update->setAttribute(Qt::WA_NativeWindow);
#if defined(Q_OS_WIN)
	setAttribute(Qt::WA_NativeWindow);
#endif

#if defined(Q_OS_MACOS)
	ui->titleBar->hide();
	customMacWindow()->setMaxButtonHidden(false);
	customMacWindow()->setMinButtonHidden(false);
	customMacWindow()->setCloseButtonHidden(false);
#else
	QObject::connect(ui->min, &QToolButton::clicked, this, [this]() { showMinimized(); });
	QObject::connect(ui->maxres, &QToolButton::clicked, this, [this]() {
		if (!getMaxState() && !getFullScreenState()) {
			showMaximized();
		} else {
			showNormal();
		}
		pls_flush_style(ui->maxres);
	});
	QObject::connect(ui->close, &QToolButton::clicked, this, [this]() { close(); });
#endif

	setMinimumSize({975, 633});
#if defined(Q_OS_WIN)
	initSize({1600, 1000});
#elif defined(Q_OS_MACOS)
	initSize({1600, 964});
#endif
	closeEventCallback = [this](QCloseEvent *e) { callBaseCloseEvent(e); };

	QObject::connect(ui->studioMode, &QToolButton::clicked, this, [this]() { emit studioModeChanged(); });

	ui->channelsArea->setMouseTracking(true);
	ui->channelsArea->installEventFilter(this);
	ui->content->installEventFilter(this);
	initSideBarButtons();
	initHelpMenu();

	// Side-bar menu scroll area.
	ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	ui->scrollArea->setObjectName("sidebarMenuScrollarea");
	ui->label_separator_bottom->hide();
	connect(ui->logo_update, &QPushButton::clicked, this, &PLSMainView::updateAppView);

	m_toastMsg = pls_new<PLSToastMsgPopup>(this);
	m_toastMsg->hide();

	// set user photo tooltip
	ui->user->setAttribute(Qt::WA_AlwaysShowToolTips, true);

	m_userPlusImage = new QWidget(ui->user);
	m_userPlusImage->setObjectName("userPlusImage");
	m_userPlusImage->move(21, 13);
	m_userPlusImage->setHidden(true);

	// mobile source
	DialogInfo mobileInfo;
	mobileInfo.configId = ConfigId::WiFiConfig;
	mobileInfo.defaultHeight = 817;
	mobileInfo.defaultWidth = 300;
	mobileInfo.defaultOffset = 5;
	if (pls_prism_is_dev()) {
		ui->devLabel->setText(" (Dev)");
	} else {
		ui->devLabel->setVisible(false);
	}

#if defined(Q_OS_MACOS)
	QString devStr = pls_is_dev_server() ? " (Dev)" : "";
	QString title = windowTitle() + devStr;
	setWindowTitle(title);
#endif

	initToastMsgView();

	connect(this, &PLSMainView::onGolivePending, ui->settings, &QWidget::setDisabled);
	connect(this, &PLSMainView::onGolivePending, ui->user, &QWidget::setDisabled);
}

PLSMainView::~PLSMainView()
{
	pls_set_main_window_destroyed(true);
	pls_delete(ui, nullptr);
	pls_delete(m_livingMsgView);
}

PLSMainView *PLSMainView::instance()
{
	return App()->getMainView();
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

void PLSMainView::setCaptionHeight(int captionHeight_)
{
	this->captionHeight = captionHeight_ - 0;
	ui->titleBar->setFixedHeight(captionHeight_);
}

int PLSMainView::getCaptionButtonSize() const
{
	return captionButtonSize;
}

void PLSMainView::setCaptionButtonSize(int captionButtonSize_)
{
	this->captionButtonSize = captionButtonSize_;
	ui->min->setFixedSize(captionButtonSize_, captionButtonSize_);
	ui->maxres->setFixedSize(captionButtonSize_, captionButtonSize_);
	ui->close->setFixedSize(captionButtonSize_, captionButtonSize_);
}

int PLSMainView::getCaptionButtonMargin() const
{
	return captionButtonMargin;
}

void PLSMainView::setCaptionButtonMargin(int captionButtonMargin_)
{
	this->captionButtonMargin = captionButtonMargin_;
	ui->titleButtonLayout->setSpacing(captionButtonMargin_);
}

int PLSMainView::getLogoMarginLeft() const
{
	return logoMarginLeft;
}

void PLSMainView::setLogoMarginLeft(int logoMarginLeft_)
{
	this->logoMarginLeft = logoMarginLeft_;
#if 0
	updateLayout(this->size());
#endif
}

int PLSMainView::getMenuButtonMarginLeft() const
{
	return menuButtonMarginLeft;
}

void PLSMainView::setMenuButtonMarginLeft(int menuButtonMarginLeft_)
{
	this->menuButtonMarginLeft = menuButtonMarginLeft_;
	QMargins margins = ui->titleBarLayout->contentsMargins();
	margins.setLeft(menuButtonMarginLeft_);
	ui->titleBarLayout->setContentsMargins(margins);
}

QSize PLSMainView::getMenuButtonSize() const
{
	return menuButtonSize;
}

void PLSMainView::setMenuButtonSize(const QSize &menuButtonSize_)
{
	this->menuButtonSize = menuButtonSize_;
	ui->menu->setFixedSize(menuButtonSize_);
}

int PLSMainView::getCloseButtonMarginRight() const
{
	return closeButtonMarginRight;
}

void PLSMainView::setCloseButtonMarginRight(int closeButtonMarginRight_)
{
	this->closeButtonMarginRight = closeButtonMarginRight_;
	QMargins margins = ui->titleBarLayout->contentsMargins();
	margins.setRight(closeButtonMarginRight_);
	ui->titleBarLayout->setContentsMargins(margins);
}

int PLSMainView::getRightAreaWidth() const
{
	return rightAreaWidth;
}

void PLSMainView::setRightAreaWidth(int rightAreaWidth_)
{
	this->rightAreaWidth = rightAreaWidth_;
	ui->rightArea->setFixedWidth(this->rightAreaWidth);
}

int PLSMainView::getBottomAreaHeight() const
{
	return bottomAreaHeight;
}

void PLSMainView::setBottomAreaHeight(int bottomAreaHeight_)
{
	this->bottomAreaHeight = bottomAreaHeight_;
	ui->bottomArea->setFixedHeight(this->bottomAreaHeight);
}

int PLSMainView::getChannelsAreaHeight() const
{
	return channelsAreaHeight;
}

void PLSMainView::setChannelsAreaHeight(int channelsAreaHeight_)
{
	this->channelsAreaHeight = channelsAreaHeight_;
	ui->channelsArea->setFixedHeight(this->channelsAreaHeight);
}

QColor PLSMainView::getTitleBarBkgColor() const
{
	return m_bkgColor;
}

void PLSMainView::setTitleBarBkgColor(QColor bkgColor)
{
	m_bkgColor = bkgColor;
}

void PLSMainView::setCloseEventCallback(const std::function<void(QCloseEvent *)> &closeEventCallback_)
{
	this->closeEventCallback = closeEventCallback_;
}

void PLSMainView::callBaseCloseEvent(QCloseEvent *event)
{
	if (!m_isFirstShow) {
		onSaveGeometry();
	}
	PLSToplevelView<QFrame>::closeEvent(event);
}

void PLSMainView::close()
{
	if (!pls_is_main_window_closing()) {
		pls_set_main_window_closing(true);
		PLSToplevelView::close();
	}
}

bool PLSMainView::getMaxState() const
{
	return windowState().testFlag(Qt::WindowMaximized);
}

bool PLSMainView::getFullScreenState() const
{
	return windowState().testFlag(Qt::WindowFullScreen);
}

void PLSMainView::onSaveGeometry() const
{
	config_set_string(App()->GetUserConfig(), "BasicWindow", "geometry", saveGeometry().toBase64().constData());
	config_save(App()->GetUserConfig());
}
void PLSMainView::onRestoreGeometry()
{
	if (const char *geometry = config_get_string(App()->GetUserConfig(), "BasicWindow", "geometry"); !pls_is_empty(geometry)) {
		restoreGeometry(QByteArray::fromBase64(QByteArray(geometry)));
	} else {
		PLSToplevelView<QFrame>::onRestoreGeometry();
	}
}

int PLSMainView::titleBarHeight() const
{
	return getCaptionHeight();
}
void PLSMainView::onSideBarButtonClicked(int buttonId)
{
	emit sideBarButtonClicked(buttonId);
}
void PLSMainView::showChatView(bool isOnlyShow, bool isOnlyInit) const
{
	auto basic = PLSBasic::instance();
	if (!basic) {
		return;
	}

	auto chatDock = basic->GetChatDock();
	if (!chatDock) {
		return;
	}

	bool initializedGeometry = config_get_bool(App()->GetUserConfig(), "ChatConfig", "initializedGeometry");
	bool isResetClicked = config_get_bool(App()->GetUserConfig(), "ChatConfig", "isResetDockClicked");
	bool isNeedRestPosAndShow = !isOnlyInit && isResetClicked;

	if (!initializedGeometry || isNeedRestPosAndShow) {
		chatDock->setFloating(true);
		basic->InitChatDockGeometry();
		chatDock->setVisible(true);
		config_set_bool(App()->GetUserConfig(), "ChatConfig", "initializedGeometry", true);
		config_set_bool(App()->GetUserConfig(), "ChatConfig", "isResetDockClicked", false);
		config_set_string(App()->GetUserConfig(), "BasicWindow", "DockState", basic->saveState().toBase64().constData());
		config_save(App()->GetUserConfig());
		return;
	}

#define BOOL_To_STR(x) (x) ? "true" : "false"
	PLS_INFO("PLSChat", "Show chat with parameter \n\tisOnlyShow:%s, \n\tisOnlyInit:%s, \n\tisHiddenNow:%s", BOOL_To_STR(isOnlyShow), BOOL_To_STR(isOnlyInit), BOOL_To_STR(chatDock->isHidden()));

	if (isOnlyInit) {
		return;
	}

	if (chatDock->isHidden()) {
		chatDock->setProperty("vis", true);
		chatDock->show();
	} else {
		chatDock->hide();
	}
}

void PLSMainView::on_help_clicked()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help Button", ACTION_CLICK);
	bool isChecked = ui->help->isChecked();
	if (isChecked) {
		QListWidgetItem *checkListWidgetItem = m_helpListWidget->item(CheckForUpdate);
		PLSNewIconActionWidget *checkMenuItem = dynamic_cast<PLSNewIconActionWidget *>(m_helpListWidget->itemWidget(checkListWidgetItem));
		bool disabled = pls_is_output_actived();
		if (disabled) {
			checkListWidgetItem->setFlags(checkListWidgetItem->flags() & ~Qt::ItemIsEnabled);
		} else {
			checkListWidgetItem->setFlags(checkListWidgetItem->flags() | Qt::ItemIsEnabled);
		}
		checkMenuItem->setItemDisabled(disabled);
		helpMenu->exec(QCursor::pos());
	} else {
		helpMenu->setHidden(true);
	}
}

void PLSMainView::on_listWidget_itemClicked(const QListWidgetItem *item)
{
	helpMenu->setHidden(true);
	int tagInt = item->data(Qt::UserRole).toInt();
	if (tagInt == PrismFAQ) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList PrismFAQ Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::instance(), "on_actionHelpPortal_triggered");
	} else if (tagInt == PrismWebsite) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList PrismWebsite Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::instance(), "on_actionWebsite_triggered");
	} else if (tagInt == ContactUs) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList ContactUs Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::Get(), "on_actionContactUs_triggered");
	} else if (tagInt == CheckForUpdate) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList checkFourUpdate Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::instance(), "on_checkUpdate_triggered");
	} else if (tagInt == UserGuide) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList UserGuide Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::instance(), "on_actionUserGuide_triggered");
	}
}

void PLSMainView::helpMenuAboutToHide()
{
	ui->help->setChecked(false);
}
void PLSMainView::on_user_clicked()
{
	if (!ui->settings->isEnabled()) {
		return;
	}
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar User Button", ACTION_CLICK);
	emit popupSettingView(QStringLiteral("General"), QStringLiteral("user"));
}
void PLSMainView::showResolutionTips(const QString &platform)
{
	static QPointer<ResolutionTipFrame> last = nullptr;
	if (last != nullptr) {
		last->on_CloseBtn_clicked();
		last = nullptr;
	}
	QString tips;
	if (platform.contains(CUSTOM_RTMP)) {
		tips = tr("Resolution.CustomRTMPTips");
	} else {
		tips = tr("Resolution.ButtonTips").arg(translatePlatformName(platform));
	}
	last = createSidebarTipFrame(tips, ui->ResolutionBtn, true);
}

void PLSMainView::showVirtualCameraTips(const QString &tips)
{
	static QPointer<ResolutionTipFrame> last = nullptr;
	if (last) {
		last->on_CloseBtn_clicked();
	}
	static auto btn = sideBarBtnGroup->button(ConfigId::VirtualCameraConfig);
	if (!tips.isEmpty()) {
		btn->setToolTip(QTStr("Basic.Main.StopVirtualCam"));
		last = createSidebarTipFrame(tips, btn, false, "VirtualCameraTipsLabel");
	} else {
		btn->setToolTip(QTStr("Basic.Main.StartVirtualCam"));
	}
}

void PLSMainView::showStudioModeTips(const QString &tips)
{
	static QPointer<ResolutionTipFrame> last = nullptr;
	if (last) {
		last->on_CloseBtn_clicked();
	}

	if (!tips.isEmpty()) {
		last = createSidebarTipFrame(tips, ui->studioMode, true, "StudioModeTipsLabel");
	}
}

void PLSMainView::setStudioModeChecked(bool bValue)
{
	ui->studioMode->setChecked(bValue);
}

void PLSMainView::closeMobileDialog() const
{
	if (m_pAlertViewMobileLost) {
		m_pAlertViewMobileLost->done(PLSAlertView::Button::Ok);
	}
	if (m_pAlertViewMobileDisconnect) {
		m_pAlertViewMobileDisconnect->done(PLSAlertView::Button::Ok);
	}
}

void PLSMainView::on_discordBtn_clicked()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar discord Button", ACTION_CLICK);

	PLSBasic::instance()->on_actionDiscord_triggered();
}

void PLSMainView::on_plusBtn_clicked()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar plus Button", ACTION_CLICK);
	PLSBasic::instance()->showsPrismPlusIntroWindow();
	PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"action", "ViewPlus"}, {"feature", "Plus Button"}}, "feature subscription reminder popup");
	pls_send_analog(AnalogType::ANALOG_PAID_STATE, {{"fsrPopup", QVariantMap({{"action", "ViewPlus"}, {"feature", "Plus Button"}})}});
}

void PLSMainView::on_paid_status_changed()
{
	if (m_userPlusImage) {
		m_userPlusImage->setHidden(true);
	}
}

#ifdef DEBUG
constexpr int ResolutionHoldingTime = 25000;
#else
constexpr int ResolutionHoldingTime = 5000;
#endif // DEBUG

ResolutionTipFrame *PLSMainView::createSidebarTipFrame(const QString &txt, QWidget *aliginWidget, bool isAutoColose, const QString &objectName)
{
	QPointer<ResolutionTipFrame> lb = new ResolutionTipFrame(this);
	lb->setObjectName(objectName);
	lb->setWindowOpacity(0.95);
	lb->setAttribute(Qt::WA_TranslucentBackground);
	lb->setAttribute(Qt::WA_DeleteOnClose);

#if defined(Q_OS_WINDOWS)
	lb->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
#else
	lb->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
	lb->setAttribute(Qt::WA_NativeWindow);
	lb->setAttribute(Qt::WA_DontCreateNativeAncestors);
	connect(
		this, &PLSMainView::isshowSignal, lb,
		[lb, this](bool isVisible) {
			lb->updateUI();
			lb->setVisible(isVisible);
		},
		Qt::DirectConnection);
#endif

	lb->setAliginWidget(aliginWidget);
	connect(this, &PLSMainView::mainViewUIChanged, lb, &ResolutionTipFrame::updateUI, Qt::DirectConnection);
	connect(this, &PLSMainView::mainViewUIChanged, PLSBasic::instance(), [this]() { PLSBasic::instance()->SetDpi(this->devicePixelRatioF()); });

	lb->setText(txt);
	if (this->isVisible() && !this->isMinimized()) {
		lb->show();
	}

	lb->updateUI();
	if (isAutoColose) {
		QTimer::singleShot(ResolutionHoldingTime, this, [lb]() {
			PLS_INFO(MAINFRAME_MODULE, "ResolutionHoldingTime");
			if (lb) {
				lb->on_CloseBtn_clicked();
			}
		});
	}

	return lb;
}

void PLSMainView::showEvent(QShowEvent *event)
{
	m_isFirstShow = false;
	++m_showTimes;
	ui->bottomArea->StartStatusMonitor();
	emit isshowSignal(true);
	PLSToplevelView::showEvent(event);
#if defined(Q_OS_WINDOWS)
	PLSBlockDump::Instance()->StartMonitor();
	PLSModuleMonitor::Instance()->StartMonitor();
#endif

#if defined(Q_OS_MACOS)
	PLSBlockDump::instance()->startMonitor();
#endif

	if (PLSLoginUserInfo::getInstance()->getAuthType() == "email") {
		ui->user->setToolTip(PLSLoginUserInfo::getInstance()->getEmail());
	} else {
		ui->user->setToolTip(PLSLoginUserInfo::getInstance()->getNickname());
	}
}

void PLSMainView::hideEvent(QHideEvent *event)
{
	//fix mac close app by click dock icon
#ifdef Q_OS_MACOS
	if (!pls_is_main_window_closing()) {
		emit isshowSignal(false);
	}
#endif // Q_OS_MACOS

#ifdef Q_OS_WIN
	emit isshowSignal(false);

#endif // Q_OS_WIN

	PLSToplevelView::hideEvent(event);
}
void PLSMainView::closeEvent(QCloseEvent *event)
{
#ifdef Q_OS_WIN
	auto bCloseByKeyboard = (GetAsyncKeyState(VK_MENU) < 0) && (GetAsyncKeyState(VK_F4) < 0);
	if (m_isFirstShow && bCloseByKeyboard) {
		event->ignore();
		return;
	}
	PLS_INFO(MAINSCENE_MODULE, (bCloseByKeyboard ? __FUNCTION__ " by ALT+F4" : __FUNCTION__));
#endif
	pls_set_main_window_closing(true);
	closeEventCallback(event);
}
void PLSMainView::windowStateChanged(QWindowStateChangeEvent *event)
{
#ifdef Q_OS_MACOS

	if (!isVisible() && !m_isFirstShow) {
		OBSBasic::Get()->SetShowing(true, false);
	}
#endif
	PLSToplevelView<QFrame>::windowStateChanged(event);
	pls_flush_style(ui->maxres);
	auto basic = PLSBasic::instance();
	basic->mainViewChangeEvent(event);
}

void PLSMainView::setUpdateTipsStatus(bool isShowTips)
{
	ui->logo_update->setVisableTips(isShowTips);
}
void PLSMainView::updateTipsEnableChanged(bool isEnable)
{
	ui->logo_update->setEnabled(isEnable);
	pls_flush_style(ui->logo_update);
}

bool PLSMainView::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::WindowActivate:
		if (!pls_inside_visible_screen_area(this->geometry())) {
			//restoreGeometry(saveGeometry());
		}
		break;
	case QEvent::Show:
	case QEvent::Resize:
		pls_async_call_mt(this, [this]() {
			PLS_INFO(MAINFRAME_MODULE, "AdjustSideBarMenu");
			AdjustSideBarMenu();
		});
		break;
	default:
		break;
	}

	if (event->type() == QEvent::Move || event->type() == QEvent::Resize) {
		emit mainViewUIChanged();
	}
	return PLSToplevelView::event(event);
}

bool PLSMainView::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef _WIN32
	const MSG &msg = *static_cast<MSG *>(message);
	switch (msg.message) {
	case WM_MOVE:
		for (OBSQTDisplay *const display : findChildren<OBSQTDisplay *>()) {
			display->OnMove();
		}
		break;

	case WM_DISPLAYCHANGE: {
		for (OBSQTDisplay *const display : findChildren<OBSQTDisplay *>()) {
			display->OnDisplayChange();
		}

		if (!winrt_notify) {
			winrt_notify = pls_new<PLSWinRTNotify>(this);
		}

		winrt_notify->onDisplayChanged();
		break;
	}

	case WM_POWERBROADCAST: {
		if (msg.wParam == PBT_APMSUSPEND) {
			PLS_WARN(MAINFRAME_MODULE, "[PC STATE] enter sleep");
		} else if (msg.wParam == PBT_APMRESUMESUSPEND || msg.wParam == PBT_APMRESUMEAUTOMATIC) {
			PLS_INFO(MAINFRAME_MODULE, "[PC STATE] wake up from event: %d", msg.wParam);

			if (!winrt_notify) {
				winrt_notify = pls_new<PLSWinRTNotify>(this);
			}

			winrt_notify->onDisplayChanged();
		}
		break;
	}

	case WM_QUERYENDSESSION:
		PLS_WARN(MAINFRAME_MODULE, "[PC STATE] WM_QUERYENDSESSION arrived, pc will shutdown");
		break;

	case WM_ENDSESSION: {
		close();
		PLS_WARN(MAINFRAME_MODULE, "[PC STATE] WM_ENDSESSION arrived, really shutdown: %s", msg.wParam ? "yes" : "no");
		break;
	}

	default:
		break;
	}
#else
	UNUSED_PARAMETER(message);
#endif

	return PLSToplevelView<QFrame>::nativeEvent(eventType, message, result);
}

bool PLSMainView::eventFilter(QObject *watcher, QEvent *event)
{
	if (watcher == ui->channelsArea) {
		switch (event->type()) {
		case QEvent::ChildAdded:
			if (auto widget = dynamic_cast<QWidget *>(static_cast<QChildEvent *>(event)->child()); widget) {
				widget->setCursor(Qt::ArrowCursor);
			}
			break;
		default:
			break;
		}
	} else if (watcher == ui->content) {
		if (event->type() == QEvent::Resize) {
			if (m_toastMsg->isVisible()) {
				int width = ui->content->width();
				QPoint pt = ui->content->mapToGlobal(QPoint(width, 0));
				pt = mapFromGlobal(pt);
				m_toastMsg->move(pt.x() - m_toastMsg->width() - 10, pt.y() + 10);
				m_toastMsg->adjustSize();
			}
		}
	}
	return PLSToplevelView::eventFilter(watcher, event);
}

void PLSMainView::updateAppView()
{
}

void PLSMainView::on_settings_clicked()
{
	emit popupSettingView(QStringLiteral("General"), QString());
	if (!pls_get_app_exiting()) {
		ui->settings->setChecked(false);
	}
}
void PLSMainView::on_alert_clicked()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Alert Button", ACTION_CLICK);
	m_livingMsgView->setShow(!m_livingMsgView->isVisible());
}

void PLSMainView::on_chat_clicked() const
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Chat Button", ACTION_CLICK);
	showChatView(true);
}

bool PLSMainView::alert_message_visible() const
{
	return m_livingMsgView->isVisible();
}

int PLSMainView::alert_message_count() const
{
	return m_livingMsgView->alertMessageCount();
}

void PLSMainView::on_ResolutionBtn_clicked()
{
	ResolutionGuidePage::setVisibleOfGuide(this);
	ui->ResolutionBtn->setChecked(false);
}

QWidget *PLSMainView::getSiderBarButton(const QString &objName)
{
	int count = ui->verticalLayout_fixed->count();
	while (count--) {
		auto w = ui->verticalLayout_fixed->itemAt(count)->widget();
		if (w && (0 == objName.compare(w->objectName(), Qt::CaseInsensitive)))
			return w;
	}
	return nullptr;
}
QString PLSMainView::generalStyleSheet(const QString &objectName, IconData data) const
{
	QString strStyle;
	strStyle = sideBarStyle.arg(objectName, data.iconOffNormal, data.iconOffHover, data.iconOffPress, data.iconOffDisabled, data.iconOnNormal, data.iconOnHover, data.iconOnPress,
				    data.iconOnDisabled);

	QString btnStyle = sideBarButtonSizeStyle.arg(objectName).arg(data.minWidth).arg(data.maxWidth).arg(data.minHeight).arg(data.maxHeight);
	return strStyle + btnStyle;
}
void PLSMainView::registerSideBarButton(ConfigId id, const IconData &data, bool inFixedArea)
{
	if (!sideBarBtnGroup) {
		sideBarBtnGroup = pls_new<QButtonGroup>(this);
		sideBarBtnGroup->setExclusive(false);
		connect(sideBarBtnGroup, &QButtonGroup::idClicked, this, &PLSMainView::onSideBarButtonClicked);
	}
	QPushButton *btn = nullptr;
	auto layout = inFixedArea ? ui->verticalLayout_fixed : ui->verticalLayout_side_bar;
	if (ConfigId::LivingMsgView == id) {
		auto button = pls_new<PLSToastButton>(this);
		button->setObjectName("alert");
		layout->addWidget(button, 0, Qt::AlignHCenter | Qt::AlignTop);
		btn = button->getButton();
	} else if (ConfigId::GiphyStickersConfig == id) {
		btn = pls_new<PLSIconButton>(this);
		btn->setProperty("useFor", "giphy");
		layout->addWidget(btn, 0, Qt::AlignHCenter | Qt::AlignTop);
	} else if (ConfigId::PrismStickerConfig == id) {
		btn = pls_new<PLSIconButton>(this);
		btn->setProperty("useFor", "prismSticker");
		layout->addWidget(btn, 0, Qt::AlignHCenter | Qt::AlignTop);
	} else if (ConfigId::DrawPenConfig == id) {
		btn = pls_new<PLSIconButton>(this);
		btn->setProperty("useFor", "drawPen");
		layout->addWidget(btn, 0, Qt::AlignHCenter | Qt::AlignTop);
	} else if (ConfigId::Ncb2bBrowserSettings == id) {
		btn = pls_new<PLSIconButton>(this);
		btn->setProperty("useFor", "ncb2bBrowserSettings");
		btn->setVisible(false);
		layout->addWidget(btn, 0, Qt::AlignHCenter | Qt::AlignTop);
	} else if (ConfigId::SceneTemplateConfig == id) {
		btn = pls_new<PLSIconButton>(this);
		btn->setProperty("useFor", "sceneTemplate");
		layout->addWidget(btn, 0, Qt::AlignHCenter | Qt::AlignTop);
	} else if (ConfigId::DualOutputConfig == id) {
		btn = pls_new<PLSIconButton>(this);
		btn->setProperty("useFor", "dualOutput");
		layout->addWidget(btn, 0, Qt::AlignHCenter | Qt::AlignTop);
	} else {
		btn = pls_new<QPushButton>(this);
		layout->addWidget(btn, 0, Qt::AlignHCenter | Qt::AlignTop);
	}

	sideBarBtnGroup->addButton(btn, id);
	const char *objName = QMetaEnum::fromType<ConfigId>().valueToKey(id);
	btn->setObjectName(objName);
	btn->setToolTip(data.tooltip);

	auto styleStr = generalStyleSheet(objName, data);

	btn->setStyleSheet(styleStr);
	SideWindowInfo info;
	info.id = id;
	info.windowName = data.tooltip;

	if (objName && *objName) {
		bool show = config_get_bool(App()->GetUserConfig(), objName, "showMode");
		show = show && ConfigId::SceneTemplateConfig != id && ConfigId::DualOutputConfig != id;
		btn->setProperty("showMode", show);
		pls_flush_style(btn);
		info.visible = show;
	}
	windowInfo << info;
}
void PLSMainView::initSideBarButtons()
{
	registerSideBarButton(ConfigId::DualOutputConfig,
			      IconData{PRISM_DUALOUTPUT_OFF_NORMAL, PRISM_DUALOUTPUT_OFF_OVER, PRISM_DUALOUTPUT_OFF_CLICKED, PRISM_DUALOUTPUT_OFF_DISABLE, PRISM_DUALOUTPUT_ON_NORMAL,
				       PRISM_DUALOUTPUT_ON_OVER, PRISM_DUALOUTPUT_ON_CLICKED, PRISM_DUALOUTPUT_ON_DISABLE, QTStr("DualOutput.Title"), 22, 22, 22, 22});

	registerSideBarButton(ConfigId::ChatConfig, IconData{CHAT_OFF_NORMAL, CHAT_OFF_OVER, CHAT_OFF_CLICKED, CHAT_OFF_DISABLE, CHAT_ON_NORMAL, CHAT_ON_OVER, CHAT_ON_CLICKED, CHAT_ON_DISABLE,
							     QTStr("main.rightbar.chat.tooltip"), 22, 22, 22, 22});

	registerSideBarButton(ConfigId::LivingMsgView, IconData{TOAST_OFF_NORMAL, TOAST_OFF_OVER, TOAST_OFF_CLICKED, TOAST_OFF_DISABLE, TOAST_ON_NORMAL, TOAST_ON_OVER, TOAST_ON_CLICKED,
								TOAST_ON_DISABLE, QTStr("Alert.Title"), 22, 22, 22, 22});

	registerSideBarButton(ConfigId::Ncb2bBrowserSettings, IconData{NCB2B_BROWSER_SETTINGS_OFF_NORMAL, NCB2B_BROWSER_SETTINGS_OFF_OVER, NCB2B_BROWSER_SETTINGS_OFF_CLICKED,
								       NCB2B_BROWSER_SETTINGS_OFF_DISABLE, NCB2B_BROWSER_SETTINGS_ON_NORMAL, NCB2B_BROWSER_SETTINGS_ON_OVER,
								       NCB2B_BROWSER_SETTINGS_ON_CLICKED, NCB2B_BROWSER_SETTINGS_ON_DISABLE, QTStr("Ncpb2b.Browser.Settings.Tooltip"), 22, 22, 22, 22});

	ui->verticalLayout_fixed->addItem(new QSpacerItem(10, 2, QSizePolicy::Fixed, QSizePolicy::Fixed));
	addSideBarSeparator();

	registerSideBarButton(ConfigId::VirtualCameraConfig,
			      IconData{VIRTUAL_CAMERA_OFF_NORMAL, VIRTUAL_CAMERA_OFF_OVER, VIRTUAL_CAMERA_OFF_CLICKED, VIRTUAL_CAMERA_OFF_DISABLE, VIRTUAL_CAMERA_ON_NORMAL, VIRTUAL_CAMERA_ON_OVER,
				       VIRTUAL_CAMERA_ON_CLICKED, VIRTUAL_CAMERA_ON_DISABLE, QTStr("sidebar.virtualcamera.inactive.tips"), 22, 22, 22, 22},
			      false);
#if 0
	registerSideBarButton(ConfigId::BeautyConfig,
			      IconData{BEAUTYEFFECT_OFF_NORMAL, BEAUTYEFFECT_OFF_OVER, BEAUTYEFFECT_OFF_CLICKED, BEAUTYEFFECT_OFF_DISABLE, BEAUTYEFFECT_ON_NORMAL, BEAUTYEFFECT_ON_OVER,
				       BEAUTYEFFECT_ON_CLICKED, BEAUTYEFFECT_ON_DISABLE, QTStr("main.beauty.title"), 26, 25, 26, 25},
			      false);

	registerSideBarButton(ConfigId::VirtualbackgroundConfig,
			      IconData{VIRTUAL_OFF_NORMAL, VIRTUAL_OFF_OVER, VIRTUAL_OFF_CLICKED, VIRTUAL_OFF_DISABLE, VIRTUAL_ON_NORMAL, VIRTUAL_ON_OVER, VIRTUAL_ON_CLICKED, VIRTUAL_ON_DISABLE,
				       QTStr("virtual.view.main.button.toolTip"), 26, 25, 26, 25},
			      false);
#endif
	registerSideBarButton(ConfigId::CamStudioConfig,
			      IconData{PRISM_CAM_OFF_NORMAL, PRISM_CAM_OFF_OVER, PRISM_CAM_OFF_CLICKED, PRISM_CAM_OFF_DISABLE, PRISM_CAM_ON_NORMAL, PRISM_CAM_ON_OVER, PRISM_CAM_ON_CLICKED,
				       PRISM_CAM_ON_DISABLE, QTStr("Siderbar.Cam.Studio.Title"), 22, 22, 22, 22},
			      false);

	registerSideBarButton(ConfigId::SceneTemplateConfig,
			      IconData{PRISM_SCENETEMPLATE_OFF_NORMAL, PRISM_SCENETEMPLATE_OFF_OVER, PRISM_SCENETEMPLATE_OFF_CLICKED, PRISM_SCENETEMPLATE_OFF_DISABLE, PRISM_SCENETEMPLATE_ON_NORMAL,
				       PRISM_SCENETEMPLATE_ON_OVER, PRISM_SCENETEMPLATE_ON_CLICKED, PRISM_SCENETEMPLATE_ON_DISABLE, QTStr("SceneTemplate.Title"), 22, 22, 22, 22},
			      false);

	registerSideBarButton(ConfigId::DrawPenConfig,
			      IconData{DEAW_PEN_OFF_NORMAL, DEAW_PEN_OFF_OVER, DEAW_PEN_OFF_CLICKED, DEAW_PEN_OFF_DISABLE, DEAW_PEN_ON_NORMAL, DEAW_PEN_ON_OVER, DEAW_PEN_ON_CLICKED,
				       DEAW_PEN_ON_DISABLE, QTStr("drawpen.view.main.button.toolTip"), 22, 22, 22, 22},
			      false);

	registerSideBarButton(ConfigId::PrismStickerConfig,
			      IconData{PRISM_STICKER_OFF_NORMAL, PRISM_STICKER_OFF_OVER, PRISM_STICKER_OFF_CLICKED, PRISM_STICKER_OFF_DISABLE, PRISM_STICKER_ON_NORMAL, PRISM_STICKER_ON_OVER,
				       PRISM_STICKER_ON_CLICKED, PRISM_STICKER_ON_DISABLE, QTStr("main.prism.sticker.title"), 22, 22, 22, 22},
			      false);

	registerSideBarButton(ConfigId::GiphyStickersConfig,
			      IconData{GIPHY_OFF_NORMAL, GIPHY_OFF_OVER, GIPHY_OFF_CLICKED, GIPHY_OFF_DISABLE, GIPHY_ON_NORMAL, GIPHY_ON_OVER, GIPHY_ON_CLICKED, GIPHY_ON_DISABLE,
				       QTStr("main.giphy.title"), 22, 22, 22, 22},
			      false);

	registerSideBarButton(ConfigId::BgmConfig,
			      IconData{BGM_OFF_NORMAL, BGM_OFF_OVER, BGM_OFF_CLICKED, BGM_OFF_DISABLE, BGM_ON_NORMAL, BGM_ON_OVER, BGM_ON_CLICKED, BGM_ON_DISABLE, QTStr("Bgm.Title"), 22, 22, 22, 22},
			      false);

	addSideBarStretch();
}

void PLSMainView::initHelpMenu()
{
	helpMenu = pls_new<QMenu>(this);
#if defined(Q_OS_MACOS)
	helpMenu->setWindowFlag(Qt::NoDropShadowWindowHint);
#endif
	helpMenu->setObjectName("helpMenu");
	m_helpListWidget = pls_new<QListWidget>(this);
	QObject::connect(m_helpListWidget, &QListWidget::itemClicked, this, &PLSMainView::on_listWidget_itemClicked);
	m_helpListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_helpListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_helpListWidget->setObjectName("helpListWidget");
	m_helpListWidget->setSelectionMode(QAbstractItemView::NoSelection);
	m_helpListWidget->setFocusPolicy(Qt::NoFocus);
	QList<QString> titleList;
	titleList << QTStr("WizardView.UserGuide") << QTStr("MainFrame.SideBar.Help.PrismFAQ") << QTStr("MainFrame.SideBar.Help.PrismWebsite") << QTStr("MainFrame.SideBar.Help.ContactUs")
		  << QTStr("MainFrame.SideBar.Help.checkUpdate");
	int index = 0;
	for (const QString &title : titleList) {
		QListWidgetItem *item = pls_new<QListWidgetItem>();
		item->setData(Qt::UserRole, UserGuide + index);
		PLSNewIconActionWidget *widget = pls_new<PLSNewIconActionWidget>(title);
		m_helpListWidget->addItem(item);
		m_helpListWidget->setItemWidget(item, widget);
		index++;
	}
	QWidgetAction *action = pls_new<QWidgetAction>(helpMenu);
	action->setDefaultWidget(m_helpListWidget);
	helpMenu->addAction(action);
	QObject::connect(helpMenu, &QMenu::aboutToHide, this, &PLSMainView::helpMenuAboutToHide);
}

void PLSMainView::addSideBarSeparator(bool inFixedArea)
{
	QLabel *label_separate = pls_new<QLabel>(this);
	label_separate->setObjectName("label_separate");
	auto layout = inFixedArea ? ui->verticalLayout_fixed : ui->verticalLayout_side_bar;
	layout->addWidget(label_separate, 0, Qt::AlignHCenter | Qt::AlignTop);
}
void PLSMainView::addSideBarStretch()
{
	ui->verticalLayout_side_bar->addStretch();
}

void PLSMainView::AdjustSideBarMenu()
{
	pls_check_app_exiting();
	if (ui->scrollArea->verticalScrollBar()->maximum() > 0) {
		ui->label_separator_bottom->show();
	} else {
		ui->label_separator_bottom->hide();
	}
}
void PLSMainView::hiddenWidget(QWidget *widget)
{
	QSizePolicy policy = widget->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	widget->setSizePolicy(policy);
	widget->hide();
}

bool PLSMainView::isSidebarButtonInScroll(ConfigId id) const
{
	if (sideBarBtnGroup) {
		auto btn = sideBarBtnGroup->button(id);
		return ui->scrollArea->rect().contains(btn->mapTo(ui->scrollArea, QPoint(0, 0)));
	}

	return false;
}

void PLSMainView::updateSideBarButtonStyle(ConfigId id, bool on)
{
	pls_check_app_exiting();
	if (sideBarBtnGroup) {

		if (auto btn = sideBarBtnGroup->button(id); btn) {
			btn->setProperty("showMode", on);
			pls_flush_style(btn);
		}

		for (auto &item : windowInfo) {
			if (id == item.id) {
				item.visible = on;
				break;
			}
		}
		PLSBasic::instance()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_SIDE_WINDOW_VISIBLE_CHANGED, {id, on});
	}
}
void PLSMainView::updateSidebarButtonTips(ConfigId id, const QString &tips) const
{
	if (sideBarBtnGroup == nullptr) {
		return;
	}
	auto btn = sideBarBtnGroup->button(id);
	if (btn == nullptr) {
		return;
	}
	btn->setToolTip(tips);
}

void PLSMainView::blockSidebarButton(ConfigId id, bool toBlock) const
{
	if (sideBarBtnGroup == nullptr) {
		return;
	}
	auto btn = sideBarBtnGroup->button(id);
	if (btn == nullptr) {
		return;
	}
	btn->blockSignals(toBlock);
}

QList<SideWindowInfo> PLSMainView::getSideWindowInfo() const
{
	return windowInfo;
}

int PLSMainView::getToastMessageCount() const
{
	return static_cast<int>(toastMessages.size());
}

bool PLSMainView::setSidebarWindowVisible(int windowId, bool visible)
{
	if (sideBarBtnGroup) {
		auto button = sideBarBtnGroup->button(windowId);
		if (button) {
			bool checked = button->property("showMode").toBool();
			if ((visible && !checked) || (!visible && checked)) {
				onSideBarButtonClicked(windowId);
				return true;
			}
		}
	}
	return false;
}
bool PLSMainView::setSidebarButtonVisible(int windowId, bool visible)
{
	if (sideBarBtnGroup) {
		auto button = sideBarBtnGroup->button(windowId);
		if (button) {
			button->setVisible(visible);
			return true;
		}
	}
	return false;
}

bool PLSMainView::setSidebarButtonEnabled(int windowId, bool enabled)
{
	if (sideBarBtnGroup) {
		auto button = sideBarBtnGroup->button(windowId);
		if (button) {
			button->setEnabled(enabled);
			return true;
		}
	}
	return false;
}

void PLSMainView::setStudioModeEnabled(bool enabled)
{
	ui->studioMode->setEnabled(enabled);
}

void PLSMainView::setStudioModeDimmed(bool bValue)
{
	ui->studioMode->setProperty("dimmed", bValue);
	pls_flush_style(ui->studioMode);
}

void PLSMainView::toastMessage(pls_toast_info_type type, const QString &message, int)
{
	QString mesIngoreChar = message;

	PLS_INFO(NOTICE_MODULE, QString("a new toast message:%1").arg(mesIngoreChar.replace("%", "%%")).toUtf8().data());

	qint64 currtime = QDateTime::currentMSecsSinceEpoch();
	toastMessages.insert(currtime, message);
	auto btn = qobject_cast<PLSToastButton *>(getSiderBarButton("alert"));
	if (btn) {
		int messageSize = static_cast<int>(toastMessages.size());
		btn->setNum(messageSize);
		auto api = PLSBasic::instance()->getApi();
		if (api) {
			api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_NOTICE_MESSAGE, {messageSize});
		}
	}

	m_toastMsg->showMsg(message, type);
	int width = ui->content->width();
	QPoint pt = ui->content->mapToGlobal(QPoint(width, 0));
	pt = mapFromGlobal(pt);
	m_toastMsg->show();
	m_toastMsg->move(pt.x() - m_toastMsg->width() - 10, pt.y() + 10);

	PLS_INFO(MAINFRAME_MODULE, "%s", qUtf8Printable(message));
	m_livingMsgView->addMsgItem(message, currtime, type);
}
void PLSMainView::toastMessage(pls_toast_info_type type, const QString &message, const QString &url, const QString &replaceStr, int autoClose)
{
	QString msg(message);
	toastMessage(type, m_livingMsgView->getInfoWithUrl(message, url, replaceStr), autoClose);
	m_toastMsg->showMsg(msg.replace(url, replaceStr), type);
}

void PLSMainView::toastClear()
{
	m_toastMsg->hide();
	m_livingMsgView->clearMsgView();

	toastMessages.clear();
	auto btn = qobject_cast<PLSToastButton *>(getSiderBarButton("alert"));
	if (btn) {
		int messageSize = static_cast<int>(toastMessages.size());
		btn->setNum(messageSize);

		auto api = PLSBasic::instance()->getApi();
		if (api) {
			api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_NOTICE_MESSAGE, {messageSize});
		}
	}
	QApplication::sendPostedEvents();
}
void PLSMainView::showPaidToast(pls_toast_info_type type, const QString &title, const QString &message, const QString &bottomButton, const std::function<void()> &btnCallback, bool containCloseBtn,
				int auto_close)
{
	
}
void PLSMainView::clearPaidToast() {}
void PLSMainView::setUserButtonIcon(const QIcon &icon)
{
	ui->user->setIcon(icon);
}
void PLSMainView::initToastMsgView(bool isInitShow)
{
	if (!m_livingMsgView) {
		DialogInfo info;
		info.configId = ConfigId::LivingMsgView;
		info.defaultHeight = 400;
		info.defaultWidth = 300;
		info.defaultOffset = 5;
		m_livingMsgView = pls_new<PLSLivingMsgView>(info);
	}
}
void PLSMainView::setToastMsgViewVisible(bool isShow)
{
	m_livingMsgView->setShow(isShow);
}

void PLSMainView::setStudioMode(bool studioMode)
{
	ui->studioMode->setChecked(studioMode);
}

bool PLSMainView::isFirstShow() const
{
	return m_isFirstShow;
}

void PLSMainView::setSettingIconCheck(bool bCheck)
{
	QSignalBlocker block(ui->settings);
	ui->settings->setChecked(bCheck);
}

void PLSMainView::setResolutionBtnCheck(bool bCheck)
{
	QSignalBlocker block(ui->ResolutionBtn);
	ui->ResolutionBtn->setChecked(bCheck);
}

bool PLSMainView::isSettingEnabled() const
{
	return ui->settings->isEnabled();
}

#ifdef _WIN32
void PLSMainView::readDetectResult()
{
	// read previous file, then remove this file
	auto pc_state_file = pls_get_app_data_dir("PRISMLiveStudio") + "/Cache/" + PC_STATE_FILE;
	QFile file(pc_state_file);

	if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QTextStream in(&file);
		QString file_content = in.readAll();

		if (!file_content.isEmpty()) {
			PLS_INFO(MAINFRAME_MODULE, "Readed previous pc state: \n%s", file_content.toUtf8().constData());
		}

		file.close();
	}

	if (file.remove()) {
		PLS_INFO(MAINFRAME_MODULE, "Successed to remove state file");
	} else {
		PLS_WARN(MAINFRAME_MODULE, "Failed to remove state file, reason: %s", file.errorString().toUtf8().constData());
	}
}

void PLSMainView::runNewDetect()
{
	// run process to detect pc and write new file if need
	auto pc_state_file = pls_get_app_data_dir("PRISMLiveStudio") + "/Cache/" + PC_STATE_FILE;

	HANDLE job_handle = ::CreateJobObject(nullptr, nullptr);
	if (!job_handle) {
		PLS_WARN(MAINFRAME_MODULE, "Failed to create pc dectect job, last error:%u", GetLastError());
		return;
	}

	QString session_id = QString::fromStdString(GlobalVars::prismSession);
	QString event_id = session_id + "_pc_shutdown";

	HANDLE state_event = CreateEventW(nullptr, TRUE, FALSE, event_id.toStdWString().c_str());
	if (!state_event) {
		PLS_WARN(MAINFRAME_MODULE, "Failed to create pc state event, last error:%u", GetLastError());
	}

	// send its event name to logger process
	pls_add_global_field("pcShutdownState", event_id.toUtf8().constData(), PLS_SET_TAG_CN);

	PROCESS_INFORMATION proc_info = {};
	STARTUPINFOW startup_info = {};
	startup_info.cb = sizeof(startup_info);
	startup_info.wShowWindow = SW_HIDE;

	QString program = pls_get_app_dir() + QStringLiteral("/util-pc-detect.exe");
	QString cmd = QString("\"%1\" \"%2\" \"%3\" \"%4\"").arg(program).arg(pc_state_file).arg(session_id).arg(event_id);

	if (!::CreateProcessW(nullptr, (LPWSTR)cmd.toStdWString().c_str(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup_info, &proc_info)) {
		PLS_WARN(MAINFRAME_MODULE, "Failed to run pc dectect, last error:%u", GetLastError());
		::CloseHandle(job_handle);
		return;
	}

	PLS_INFO(MAINFRAME_MODULE, "created pc dectect, pid:%u", proc_info.dwProcessId);
	::AssignProcessToJobObject(job_handle, proc_info.hProcess);

	::CloseHandle(proc_info.hThread);
	::CloseHandle(proc_info.hProcess);

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info = {};
	limit_info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	::SetInformationJobObject(job_handle, JobObjectExtendedLimitInformation, &limit_info, sizeof(limit_info));

	static std::shared_ptr<int> auto_free(nullptr, [job_handle, state_event](int *) {
		if (job_handle) {
			CloseHandle(job_handle);
		}

		if (state_event) {
			CloseHandle(state_event);
		}
	});
}
#endif
