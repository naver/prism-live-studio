#include "PLSMainView.hpp"
#include "ui_PLSMainView.h"
#include "PLSToastButton.hpp"
#include "window-basic-main.hpp"
#include "PLSMainViewConstValues.h"
#include "PLSToastMsgPopup.hpp"
#include "PLSLivingMsgView.hpp"
#include "login-user-info.hpp"

#include <QResizeEvent>
#include <QSizePolicy>
#include <QPainter>
#include <QStyle>
#include <QWidgetAction>
#include <QLabel>
#include <QKeySequence>
#include <QLabel>
#include <qdatetime.h>
#include <QMetaEnum>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QFrame>

#include "vertical-scroll-area.hpp"

#include "PLSBasic.h"
#include "PLSNoticeUpdateCoordinator.hpp"
#include "PLSNoticeUpdateRepository.hpp"

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
#include "PLSLaunchWizardView.h"
#include <QTimer>
#include "pls-performance.h"
#include "PLSApp.h"
#include "PLSCommonFunc.h"
#include "PLSLoadingView.h"
#include "PLSOnBoardingDlg.h"
#include "PLSTrackers.h"
#include <pls/pls-base.h>

#include "PLSNewIconActionWidget.hpp"
#if defined(Q_OS_WINDOWS)
#include <Windows.h>
#include "windows/PLSBlockDump.h"
#include "windows/PLSModuleMonitor.h"
#endif // Q_OS_WINDOWS

#if defined(Q_OS_MACOS)
#include "mac/PLSBlockDump.h"
#endif

extern void printTotalStartTime();

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

enum HelpWidgetItemTag { UserGuide = 0, PrismFAQ, PrismWebsite, Discord, ContactUs, NoticeAndUpdate, CheckForUpdate };

PLSMainView::PLSMainView(QWidget *parent) : PLSToplevelView<QFrame>(CreateWinId::DontCreate, parent)
{
	PLS_PERFORMANCE_FUNCTION();
	PLS_DISABLE_UISTEP_V2(this);

#ifdef _WIN32
	// firstly read, then run detect
	readDetectResult();
	runNewDetect();
#endif

	pls_set_main_view(this);
	setWindowIcon(QIcon(":/resource/images/logo/PRISMLiveStudio.ico"));
	ui = pls_new<Ui::PLSMainView>();
	ui->setupUi(this);
	setupRightSidebarWidgets();
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
	QObject::connect(ui->min, &QToolButton::clicked, this, [this]() {
		resizeTracker()->disableTracking();
		showMinimized();
	});
	QObject::connect(ui->maxres, &QToolButton::clicked, this, [this]() {
		resizeTracker()->disableTracking();
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
	initSize({1600, 1000 - PLS_TITLE_BAR_HEIGHT});
#endif
	closeEventCallback = [this](QCloseEvent *e) { callBaseCloseEvent(e); };

	QObject::connect(m_studioModeBtn, &QPushButton::clicked, this, [this]() { emit studioModeChanged(); });

	ui->channelsArea->setMouseTracking(true);
	ui->channelsArea->installEventFilter(this);
	ui->content->installEventFilter(this);
	initSideBarButtons();
	/* Bottom strip: [alert][20px][lens][20px][plus] — PRISM Info menu opens Discord */
	if (m_bottomLayout && m_bottomLayout->count() >= 2) {
		m_bottomLayout->insertSpacing(1, 20);
	}
	initHelpMenu();

	// Side-bar menu scroll area: wheel/touch scroll OK, no visible scrollbar.
	m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_scrollArea->setObjectName("sidebarMenuScrollarea");
	m_separatorBottom->hide();
	connect(ui->logo_update, &QPushButton::clicked, this, &PLSMainView::updateAppView);

	m_toastMsg = pls_new<PLSToastMsgPopup>(this);
	m_toastMsg->hide();

	// mobile source
	DialogInfo mobileInfo;
	mobileInfo.configId = ConfigId::WiFiConfig;
	mobileInfo.defaultHeight = 817;
	mobileInfo.defaultWidth = 300;
	mobileInfo.defaultOffset = 5;
	if (pls_is_dev()) {
		ui->devLabel->setText(" (Dev)");
	} else {
		ui->devLabel->setVisible(false);
	}

#if defined(Q_OS_MACOS)
	//PRISM/jackson/20260409/PRISM_PC-5676/native title: name · v{version} (Dev)
	QString devStr = pls_is_dev_server() ? QStringLiteral(" (Dev)") : "";
	const QString ver = PLSLoginFunc::getPrismVersion();
	QString title = windowTitle();
	if (!ver.isEmpty())
		title += QStringLiteral(" \u2022 v") + ver;
	title += devStr;
	setWindowTitle(title);
#endif

	initToastMsgView();

	connect(this, &PLSMainView::onGolivePending, m_settingsBtn, &QWidget::setDisabled);
	connect(this, &PLSMainView::onGolivePending, m_userBtn, &QWidget::setDisabled);

	setUiStepLogParams();

#if defined(Q_OS_MACOS)
	// Initialize macOS sleep/wake monitor
	mac_sleep_notify = pls_new<PLSMacSleepNotify>(this);
	connect(mac_sleep_notify, &PLSMacSleepNotify::systemWillSleep, this, [this]() { pls_update_pc_sleep(true); });
	connect(mac_sleep_notify, &PLSMacSleepNotify::systemDidWake, this, [this]() { pls_update_pc_sleep(false); });
#endif

	connect(PLSUiApp::instance(), &PLSUiApp::peerAppState, this, [this](pls_app_state_t state) {
		qDebug() << "open app state: " << pls_app_state_to_string(state);
		switch (state) {
		case pls_app_state_t::ProcessStarted:
			updateSideBarButtonStyle(ConfigId::CamStudioConfig, true);
			updateLoadingState(ConfigId::CamStudioConfig, false); // TODO: support old version, lens qt need remove
			break;
		case pls_app_state_t::ProcessExited:
			updateSideBarButtonStyle(ConfigId::CamStudioConfig, false);
			updateLoadingState(ConfigId::CamStudioConfig, false);
			break;
		case pls_app_state_t::AppNotInstalled:
		case pls_app_state_t::OpenProcessFailed:
		case pls_app_state_t::AnyWindowActived:
		case pls_app_state_t::MainWindowActived:
			updateLoadingState(ConfigId::CamStudioConfig, false);
			break;
		default:
			break;
		}
	});
}

PLSMainView::~PLSMainView()
{
	PLS_PERFORMANCE_FUNCTION();

#ifdef _WIN32
	freeHandle = nullptr;
#endif

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

		if (m_isFirstShow)
			winId();
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
	PLS_PERFORMANCE_FUNCTION();
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
		bool isB2B = !PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName().isEmpty();
		chatDock->setFloating(!isB2B);
		basic->InitChatDockGeometry(isB2B);
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
	PLS_PERFORMANCE_FUNCTION();
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help Button", ACTION_CLICK);
	bool isChecked = m_helpBtn->isChecked();
	if (isChecked) {
		QListWidgetItem *checkListWidgetItem = m_helpListWidget->item(CheckForUpdate);
		PLSNewIconActionWidget *checkMenuItem = dynamic_cast<PLSNewIconActionWidget *>(m_helpListWidget->itemWidget(checkListWidgetItem));
		checkMenuItem->setBadgeVisible(PLSBasic::instance()->getUpdateResult() == AppUpdateResult::AppHasUpdate);
		bool disabled = pls_is_output_actived();
		if (disabled) {
			checkListWidgetItem->setFlags(checkListWidgetItem->flags() & ~Qt::ItemIsEnabled);
		} else {
			checkListWidgetItem->setFlags(checkListWidgetItem->flags() | Qt::ItemIsEnabled);
		}
		checkMenuItem->setItemDisabled(disabled);
		QListWidgetItem *noticeWidgetItem = m_helpListWidget->item(NoticeAndUpdate);
		PLSNewIconActionWidget *noticeWidget = dynamic_cast<PLSNewIconActionWidget *>(m_helpListWidget->itemWidget(noticeWidgetItem));
		const bool forB2B = !PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName().isEmpty();
		const bool hasUnread = PLSNoticeUpdateRepository::instance()->hasUnreadNotice(forB2B);
		PLSNoticeUpdateCoordinator::instance()->onHelpSidebarOpened(hasUnread);
		applyNoticeBadgeState();
		noticeWidget->setNoticeTipsVisible(PLSNoticeUpdateCoordinator::instance()->helpMenuBadgeVisible());
		updateHelpMenuGeometry();
		// Anchor PRISM Info menu top-left at (icon top-right + 4px, same y), not at cursor.
		const QPoint iconTopRightGlobal = m_helpBtn->mapToGlobal(m_helpBtn->rect().topRight());
		helpMenu->exec(QPoint(iconTopRightGlobal.x() + 4, iconTopRightGlobal.y()));
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
	} else if (tagInt == Discord) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList Discord Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::instance(), "on_actionDiscord_triggered");
	} else if (tagInt == ContactUs) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList ContactUs Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::Get(), "on_actionContactUs_triggered");
	} else if (tagInt == CheckForUpdate) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList ProgramInfo Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::instance(), "on_actionShowAbout_triggered");
	} else if (tagInt == UserGuide) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList UserGuide Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::instance(), "on_actionUserGuide_triggered");
	} else if (tagInt == NoticeAndUpdate) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList NoticeUpdate Button", ACTION_CLICK);
		consumeNoticeMenuBadge();
		QMetaObject::invokeMethod(PLSBasic::instance(), "onNoticeUpdateMsgTirggered");
	}
}

void PLSMainView::helpMenuAboutToHide()
{
	m_helpBtn->setChecked(false);
}
void PLSMainView::on_user_clicked()
{
	if (!m_settingsBtn->isEnabled()) {
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
	last = createSidebarTipFrame(tips, m_resolutionBtn, true);
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
		last = createSidebarTipFrame(tips, m_studioModeBtn, true, "StudioModeTipsLabel");
		PLS_UI_ACTION("Show studio mode tips: %s", pls_uistep_v2_to_english(tips).toUtf8().constData());
	}
}

void PLSMainView::setStudioModeChecked(bool bValue)
{
	m_studioModeBtn->setChecked(bValue);
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

void PLSMainView::setNoticeTips(bool hasNoticeOrUpdateMsg)
{
	PLSNoticeUpdateCoordinator::instance()->syncBadgeStateFromUnread(hasNoticeOrUpdateMsg);
	applyNoticeBadgeState();
}

void PLSMainView::armNoticeTips(bool hasNoticeOrUpdateMsg)
{
	PLSNoticeUpdateCoordinator::instance()->armBadgeFromUnread(hasNoticeOrUpdateMsg);
	applyNoticeBadgeState();
}

void PLSMainView::consumeNoticeMenuBadge()
{
	PLSNoticeUpdateCoordinator::instance()->consumeAllBadges();
	applyNoticeBadgeState();
}

void PLSMainView::setHelpMenuNoticeBadgeVisible(bool visible)
{
	if (!m_helpListWidget)
		return;
	QListWidgetItem *noticeWidgetItem = m_helpListWidget->item(NoticeAndUpdate);
	if (!noticeWidgetItem)
		return;
	auto *noticeWidget = dynamic_cast<PLSNewIconActionWidget *>(m_helpListWidget->itemWidget(noticeWidgetItem));
	if (noticeWidget)
		noticeWidget->setNoticeTipsVisible(visible);
}

void PLSMainView::applyNoticeBadgeState()
{
	if (!m_helpBtn)
		return;

	if (!m_noticeTipsIcon) {
		m_noticeTipsIcon = new QLabel(m_helpBtn);
		m_noticeTipsIcon->setScaledContents(true);
		m_noticeTipsIcon->setObjectName("noticeTipsIcon");
	}
	const bool sidebarBadgeVisible = PLSNoticeUpdateCoordinator::instance()->sidebarBadgeVisible();
	m_noticeTipsIcon->setVisible(sidebarBadgeVisible);
	if (sidebarBadgeVisible) {
		m_noticeTipsIcon->move(m_helpBtn->width() - 5, 0);
	}
	setHelpMenuNoticeBadgeVisible(PLSNoticeUpdateCoordinator::instance()->helpMenuBadgeVisible());
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
	const bool firstShow = m_isFirstShow;
	m_isFirstShow = false;
	++m_showTimes;
	ui->bottomArea->StartStatusMonitor();
	if (GlobalVars::isLogined) {
		PLSApp::plsApp()->destoryLoadingApp();
		printTotalStartTime();
	}

	emit isshowSignal(true);
	PLSToplevelView::showEvent(event);
#if defined(Q_OS_WINDOWS)
	PLSBlockDump::Instance()->StartMonitor();
	PLSModuleMonitor::Instance()->StartMonitor();
#endif

#if defined(Q_OS_MACOS)
	PLSBlockDump::instance()->startMonitor();
#endif

	pls_async_call_mt(this, [this]() {
		const bool forB2B = !PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName().isEmpty();
		armNoticeTips(PLSNoticeUpdateRepository::instance()->hasUnreadNotice(forB2B));
	});
	if (firstShow) {
		pls_async_call_mt(this, [this]() { ui->logo_update->refreshMinimumWidthFromLayout(); });
	}
	PLS_PERFORMANCE_GLOBAL_END("PRISM Startup Time");
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
	if (auto forceClose = property("forceClose").toBool(); m_isFirstShow && bCloseByKeyboard && !forceClose) {
		event->ignore();
		pls_set_main_window_closing(false);
		return;
	}
	PLS_INFO(MAINSCENE_MODULE, (bCloseByKeyboard ? __FUNCTION__ " by ALT+F4" : __FUNCTION__));
#else
	PLS_INFO(MAINSCENE_MODULE, "%s%s", __FUNCTION__, (pls_libutil_api_mac::pls_get_is_app_quitting_by_dock() ? " by dock quit" : ""));
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
	ui->logo_update->refreshMinimumWidthFromLayout();
}

void PLSMainView::updateTipsEnableChanged(bool isEnable)
{
	ui->logo_update->setEnabled(isEnable);
	pls_flush_style(ui->logo_update);
	ui->logo_update->refreshMinimumWidthFromLayout();
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
			pls_sleep_start(obs_get_lagged_frames());
			pls_update_pc_sleep(true);
		} else if (msg.wParam == PBT_APMRESUMESUSPEND || msg.wParam == PBT_APMRESUMEAUTOMATIC) {
			PLS_INFO(MAINFRAME_MODULE, "[PC STATE] wake up from event: %d", msg.wParam);
			pls_sleep_end(obs_get_lagged_frames());
			pls_update_pc_sleep(false);
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

void PLSMainView::winIdChanged(WId winId)
{
	if (winId > 0)
		PLSBasic::InitInteractData(winId, font());
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
			}
		}
	}
	return PLSToplevelView::eventFilter(watcher, event);
}
void PLSMainView::updateAppView()
{
	PLS_PERFORMANCE_FUNCTION();
	PLS_PERFORMANCE_GLOBAL_START("updateLogoUpdateView");
	PLS_UI_STEP(MAINMENU_MODULE, "Left Corner Logo Check Update", ACTION_CLICK);
	auto basic = PLSBasic::instance();
	if (basic) {
		if (m_requestUpdate) {
			return;
		}
		m_requestUpdate = true;
		PLS_PERFORMANCE_START(CheckAppUpdate);
		basic->CheckAppUpdate();
		PLS_PERFORMANCE_END(CheckAppUpdate);
		PLS_INFO(MAINMENU_MODULE, "Left Corner Logo Finished Check Update");
		pls_modal_check_app_exiting();
		if (basic->getUpdateResult() == AppUpdateResult::AppHasUpdate || basic->isForceUpdateApp()) {
			PLS_INFO(MAINMENU_MODULE, "Left Corner Logo ShowUpdateView");
			PLS_PERFORMANCE_GLOBAL_START("ShowUpdateView");
			if (basic->ShowUpdateView(basic->getMainView())) {
				PLS_INFO(MAINMENU_MODULE, "Left Corner Logo StartDownloading");
				pls_async_call_mt(basic, [basic]() { basic->startDownloading(basic->isForceUpdateApp()); });
			}
		} else if (basic->getUpdateResult() == AppUpdateResult::AppNoUpdate) {
			PLS_INFO(MAINMENU_MODULE, "Left Corner Logo Show About View");
			PLSAboutView aboutView;
			if (aboutView.exec() == PLSAboutView::Accepted) {
				basic->on_checkUpdate_triggered();
			}
			PLS_INFO(MAINMENU_MODULE, "Left Corner Logo Finished Show About View");
		}
		m_requestUpdate = false;
	}
}

void PLSMainView::on_settings_clicked()
{
	emit popupSettingView(QStringLiteral("General"), QString());
	if (!pls_get_app_exiting()) {
		m_settingsBtn->setChecked(false);
	}
}
void PLSMainView::on_alert_clicked()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Alert Button", ACTION_CLICK);
	m_livingMsgView->setShow(!m_livingMsgView->isVisible());
	if (auto btn = qobject_cast<PLSToastButton *>(getSiderBarButton("alert")); btn) {
		btn->setNum(0);
	}
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
	PLS_PERFORMANCE_GLOBAL_START("ShowResolutionGuidAllTime");
	ResolutionGuidePage::setVisibleOfGuide(this);
	m_resolutionBtn->setChecked(false);
}

QWidget *PLSMainView::getSiderBarButton(const QString &objName)
{
	if (QWidget *w = findChild<QWidget *>(objName, Qt::FindChildrenRecursively))
		return w;
	int count = m_sideBarLayout->count();
	while (count--) {
		auto w = m_sideBarLayout->itemAt(count)->widget();
		if (w && (0 == objName.compare(w->objectName(), Qt::CaseInsensitive)))
			return w;
	}
	count = m_bottomLayout->count();
	while (count--) {
		auto w = m_bottomLayout->itemAt(count)->widget();
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

void PLSMainView::createSidebarUtilityButtons()
{
	if (m_resolutionBtn) {
		return;
	}
	QWidget *wparent = ui->rightArea;
	m_resolutionBtn = pls_new<QPushButton>(wparent);
	m_resolutionBtn->setObjectName(QStringLiteral("ResolutionBtn"));
	m_resolutionBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_resolutionBtn->setCheckable(true);
	m_resolutionBtn->setText(QString());
	m_resolutionBtn->setToolTip(QTStr("ResolutionGuide.MainTitle"));
	m_resolutionBtn->setAutoDefault(false);
	m_resolutionBtn->setDefault(false);
	QObject::connect(m_resolutionBtn, &QPushButton::clicked, this, &PLSMainView::on_ResolutionBtn_clicked);

	m_settingsBtn = pls_new<QPushButton>(wparent);
	m_settingsBtn->setObjectName(QStringLiteral("settings"));
	m_settingsBtn->setProperty("useFor", "QToolButton");
	m_settingsBtn->setCursor(Qt::ArrowCursor);
	m_settingsBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_settingsBtn->setCheckable(true);
	m_settingsBtn->setText(QString());
	m_settingsBtn->setToolTip(QTStr("RightArea.Settings.TooTip"));
	m_settingsBtn->setAutoDefault(false);
	m_settingsBtn->setDefault(false);
	QObject::connect(m_settingsBtn, &QPushButton::clicked, this, &PLSMainView::on_settings_clicked);

	m_helpBtn = pls_new<QPushButton>(wparent);
	m_helpBtn->setObjectName(QStringLiteral("help"));
	m_helpBtn->setProperty("useFor", "QToolButton");
	m_helpBtn->setCursor(Qt::ArrowCursor);
	m_helpBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_helpBtn->setCheckable(true);
	m_helpBtn->setText(QString());
	m_helpBtn->setToolTip(QTStr("RightArea.Help.TooTip"));
	m_helpBtn->setAutoDefault(false);
	m_helpBtn->setDefault(false);
	QObject::connect(m_helpBtn, &QPushButton::clicked, this, &PLSMainView::on_help_clicked);

	pls_flush_style(m_resolutionBtn);
	pls_flush_style(m_settingsBtn);
	pls_flush_style(m_helpBtn);
}

void PLSMainView::registerSideBarButton(ConfigId id, const IconData &data, bool isAddLoading)
{
	if (!sideBarBtnGroup) {
		sideBarBtnGroup = pls_new<QButtonGroup>(this);
		sideBarBtnGroup->setExclusive(false);
		connect(sideBarBtnGroup, &QButtonGroup::idClicked, this, &PLSMainView::onSideBarButtonClicked);
	}
	QPushButton *btn = nullptr;
	QVBoxLayout *layout = nullptr;
	int insertIndex = -1;
	if (ConfigId::LivingMsgView == id) {
		/* Notice + Lens: fixed bottom, not in VScrollArea */
		layout = m_bottomLayout;
		insertIndex = 0;
	} else if (ConfigId::CamStudioConfig == id) {
		layout = m_bottomLayout;
		insertIndex = 1;
	} else {
		layout = m_sideBarLayout;
	}
	auto addToBarLayout = [&](QWidget *w) {
		if (insertIndex >= 0)
			layout->insertWidget(insertIndex, w, 0, Qt::AlignHCenter | Qt::AlignTop);
		else
			layout->addWidget(w, 0, Qt::AlignHCenter | Qt::AlignTop);
	};
	if (ConfigId::LivingMsgView == id) {
		auto button = pls_new<PLSToastButton>(this);
		button->setObjectName("alert");
		addToBarLayout(button);
		btn = button->getButton();
	} else if (ConfigId::GiphyStickersConfig == id) {
		btn = pls_new<PLSIconButton>(this);
		btn->setProperty("useFor", "giphy");
		addToBarLayout(btn);
	} else if (ConfigId::PrismStickerConfig == id) {
		btn = pls_new<PLSIconButton>(this);
		btn->setProperty("useFor", "prismSticker");
		addToBarLayout(btn);
	} else if (ConfigId::DrawPenConfig == id) {
		btn = pls_new<PLSIconButton>(this);
		btn->setProperty("useFor", "drawPen");
		addToBarLayout(btn);
	} else if (ConfigId::Ncb2bBrowserSettings == id) {
		btn = pls_new<PLSIconButton>(this);
		btn->setProperty("useFor", "ncb2bBrowserSettings");
		btn->setVisible(false);
		addToBarLayout(btn);
	} else if (ConfigId::SceneTemplateConfig == id) {
		btn = pls_new<PLSIconButton>(this);
		btn->setProperty("useFor", "sceneTemplate");
		addToBarLayout(btn);
	} else if (ConfigId::DualOutputConfig == id) {
		btn = pls_new<PLSIconButton>(this);
		btn->setProperty("useFor", "dualOutput");
		addToBarLayout(btn);
	} else {
		btn = pls_new<QPushButton>(this);
		addToBarLayout(btn);
	}

	if (isAddLoading) {
		connect(btn, &QPushButton::clicked, this, [this, id]() { this->updateLoadingState(id, true); });
	}

	sideBarBtnGroup->addButton(btn, id);
	const char *objName = QMetaEnum::fromType<ConfigId>().valueToKey(id);
	btn->setObjectName(objName);
	btn->setToolTip(data.tooltip);

	pls_uistep_v2_set_custom_enter_leave_name(btn, pls_uistep_v2_to_english(data.tooltip).toUtf8());

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

void PLSMainView::setupRightSidebarWidgets()
{
	QWidget *const ra = ui->rightArea;
	QVBoxLayout *const vl = ui->verticalLayout;
	if (!vl || !ra) {
		return;
	}

	m_raSpacerTop = new QSpacerItem(20, 16, QSizePolicy::Fixed, QSizePolicy::Fixed);
	vl->addItem(m_raSpacerTop);

	m_scrollArea = pls_new<VScrollArea>(ra);
	m_scrollArea->setObjectName(QStringLiteral("scrollArea"));
	m_scrollArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	m_scrollArea->setFrameShape(QFrame::NoFrame);
	m_scrollArea->setWidgetResizable(true);

	m_sideBarMenus = pls_new<QWidget>();
	m_sideBarMenus->setObjectName(QStringLiteral("side_bar_menus"));
	m_sideBarMenus->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
	m_sideBarLayout = pls_new<QVBoxLayout>(m_sideBarMenus);
	m_sideBarLayout->setObjectName(QStringLiteral("verticalLayout_side_bar"));
	m_sideBarLayout->setSpacing(20);
	m_sideBarLayout->setContentsMargins(0, 8, 0, 8);

	m_userBtn = pls_new<QPushButton>(m_sideBarMenus);
	m_userBtn->setObjectName(QStringLiteral("user"));
	m_userBtn->setCursor(Qt::ArrowCursor);
	m_userBtn->setText(QString());
	m_userBtn->setIconSize(QSize(34, 34));
	m_userBtn->setProperty("useFor", "QToolButton");
	m_sideBarLayout->addWidget(m_userBtn, 0, Qt::AlignHCenter | Qt::AlignTop);

	m_studioModeBtn = pls_new<QPushButton>(m_sideBarMenus);
	m_studioModeBtn->setObjectName(QStringLiteral("studioMode"));
	m_studioModeBtn->setCursor(Qt::ArrowCursor);
	m_studioModeBtn->setToolTip(QTStr("main.rightbar.studiomode.tooltip"));
	m_studioModeBtn->setText(QString());
	m_studioModeBtn->setCheckable(true);
	m_studioModeBtn->setProperty("useFor", "QToolButton");
	m_sideBarLayout->addWidget(m_studioModeBtn, 0, Qt::AlignHCenter | Qt::AlignTop);

	m_scrollArea->setWidget(m_sideBarMenus);
	vl->addWidget(m_scrollArea, 1);

	m_separatorBottom = pls_new<QLabel>(ra);
	m_separatorBottom->setObjectName(QStringLiteral("label_separator_bottom"));
	m_separatorBottom->setText(QString());
	vl->addWidget(m_separatorBottom, 0, Qt::AlignHCenter);

	/* Former #label_space: fixed gap between scroll and bottom strip (see PLSMainView.css) */
	vl->addItem(new QSpacerItem(20, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));

	m_bottomStack = pls_new<QWidget>(ra);
	m_bottomStack->setObjectName(QStringLiteral("sidebarBottomStack"));
	m_bottomStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
	m_bottomLayout = pls_new<QVBoxLayout>(m_bottomStack);
	/* No inter-item spacing: explicit addSpacing(20) gives fixed 20px bottom margin */
	m_bottomLayout->setSpacing(0);
	m_bottomLayout->setContentsMargins(0, 0, 0, 0);

	m_bottomLayout->addSpacing(20);

	vl->addWidget(m_bottomStack);

	QObject::connect(m_userBtn, &QPushButton::clicked, this, &PLSMainView::on_user_clicked);
}

void PLSMainView::initSideBarButtons()
{
	PLS_PERFORMANCE_FUNCTION();
	// Section 1: Dual Output, Chat (scroll). Profile + Studio Mode are already at top of m_sideBarMenus (setupRightSidebarWidgets).
	// Bottom fixed strip: Notice → Lens; not in scroll. (Discord: PRISM Info menu)
	registerSideBarButton(ConfigId::DualOutputConfig,
			      IconData{PRISM_DUALOUTPUT_OFF_NORMAL, PRISM_DUALOUTPUT_OFF_OVER, PRISM_DUALOUTPUT_OFF_CLICKED, PRISM_DUALOUTPUT_OFF_DISABLE, PRISM_DUALOUTPUT_ON_NORMAL,
				       PRISM_DUALOUTPUT_ON_OVER, PRISM_DUALOUTPUT_ON_CLICKED, PRISM_DUALOUTPUT_ON_DISABLE, QTStr("DualOutput.Title"), 22, 22, 22, 22});

	registerSideBarButton(ConfigId::ChatConfig, IconData{CHAT_OFF_NORMAL, CHAT_OFF_OVER, CHAT_OFF_CLICKED, CHAT_OFF_DISABLE, CHAT_ON_NORMAL, CHAT_ON_OVER, CHAT_ON_CLICKED, CHAT_ON_DISABLE,
							     QTStr("main.rightbar.chat.tooltip"), 22, 22, 22, 22});

	registerSideBarButton(ConfigId::Ncb2bBrowserSettings, IconData{NCB2B_BROWSER_SETTINGS_OFF_NORMAL, NCB2B_BROWSER_SETTINGS_OFF_OVER, NCB2B_BROWSER_SETTINGS_OFF_CLICKED,
								       NCB2B_BROWSER_SETTINGS_OFF_DISABLE, NCB2B_BROWSER_SETTINGS_ON_NORMAL, NCB2B_BROWSER_SETTINGS_ON_OVER,
								       NCB2B_BROWSER_SETTINGS_ON_CLICKED, NCB2B_BROWSER_SETTINGS_ON_DISABLE, QTStr("Ncpb2b.Browser.Settings.Tooltip"), 22, 22, 22, 22});

	addSideBarSeparator();

	// Section 2: Feature Area - Scene Template, Stickers, Music, Drawing, Virtual Camera (order: Preparation -> Decoration -> Performance -> Extension)
	registerSideBarButton(ConfigId::SceneTemplateConfig,
			      IconData{PRISM_SCENETEMPLATE_OFF_NORMAL, PRISM_SCENETEMPLATE_OFF_OVER, PRISM_SCENETEMPLATE_OFF_CLICKED, PRISM_SCENETEMPLATE_OFF_DISABLE, PRISM_SCENETEMPLATE_ON_NORMAL,
				       PRISM_SCENETEMPLATE_ON_OVER, PRISM_SCENETEMPLATE_ON_CLICKED, PRISM_SCENETEMPLATE_ON_DISABLE, QTStr("SceneTemplate.Title"), 22, 22, 22, 22});

	registerSideBarButton(ConfigId::PrismStickerConfig, IconData{PRISM_STICKER_OFF_NORMAL, PRISM_STICKER_OFF_OVER, PRISM_STICKER_OFF_CLICKED, PRISM_STICKER_OFF_DISABLE, PRISM_STICKER_ON_NORMAL,
								     PRISM_STICKER_ON_OVER, PRISM_STICKER_ON_CLICKED, PRISM_STICKER_ON_DISABLE, QTStr("main.prism.sticker.title"), 22, 22, 22, 22});

	registerSideBarButton(ConfigId::GiphyStickersConfig, IconData{GIPHY_OFF_NORMAL, GIPHY_OFF_OVER, GIPHY_OFF_CLICKED, GIPHY_OFF_DISABLE, GIPHY_ON_NORMAL, GIPHY_ON_OVER, GIPHY_ON_CLICKED,
								      GIPHY_ON_DISABLE, QTStr("main.giphy.title"), 22, 22, 22, 22});

	registerSideBarButton(ConfigId::BgmConfig,
			      IconData{BGM_OFF_NORMAL, BGM_OFF_OVER, BGM_OFF_CLICKED, BGM_OFF_DISABLE, BGM_ON_NORMAL, BGM_ON_OVER, BGM_ON_CLICKED, BGM_ON_DISABLE, QTStr("Bgm.Title"), 22, 22, 22, 22});

	registerSideBarButton(ConfigId::DrawPenConfig, IconData{DEAW_PEN_OFF_NORMAL, DEAW_PEN_OFF_OVER, DEAW_PEN_OFF_CLICKED, DEAW_PEN_OFF_DISABLE, DEAW_PEN_ON_NORMAL, DEAW_PEN_ON_OVER,
								DEAW_PEN_ON_CLICKED, DEAW_PEN_ON_DISABLE, QTStr("drawpen.view.main.button.toolTip"), 22, 22, 22, 22});

	registerSideBarButton(ConfigId::VirtualCameraConfig,
			      IconData{VIRTUAL_CAMERA_OFF_NORMAL, VIRTUAL_CAMERA_OFF_OVER, VIRTUAL_CAMERA_OFF_CLICKED, VIRTUAL_CAMERA_OFF_DISABLE, VIRTUAL_CAMERA_ON_NORMAL, VIRTUAL_CAMERA_ON_OVER,
				       VIRTUAL_CAMERA_ON_CLICKED, VIRTUAL_CAMERA_ON_DISABLE, QTStr("sidebar.virtualcamera.inactive.tips"), 22, 22, 22, 22});

	/* Resolution / Settings / Help: inside sidebar scroll area */
	createSidebarUtilityButtons();
	{
		auto *lay = m_sideBarLayout;
		addSideBarSeparator();
		lay->addSpacing(10);
		lay->addWidget(m_resolutionBtn, 0, Qt::AlignHCenter | Qt::AlignTop);
		lay->addWidget(m_settingsBtn, 0, Qt::AlignHCenter | Qt::AlignTop);
		lay->addWidget(m_helpBtn, 0, Qt::AlignHCenter | Qt::AlignTop);
	}

	addSideBarStretch();

	/* Bottom strip: Notice → Lens; not in scroll */
	registerSideBarButton(ConfigId::LivingMsgView,
			      IconData{TOAST_OFF_NORMAL, TOAST_OFF_OVER, TOAST_OFF_CLICKED, TOAST_OFF_DISABLE, TOAST_ON_NORMAL, TOAST_ON_OVER, TOAST_ON_CLICKED, TOAST_ON_DISABLE, QTStr("Alert.Title"),
				       22, 22, 22, 22},
			      false);

	registerSideBarButton(ConfigId::CamStudioConfig,
			      IconData{PRISM_CAM_OFF_NORMAL, PRISM_CAM_OFF_OVER, PRISM_CAM_OFF_CLICKED, PRISM_CAM_OFF_DISABLE, PRISM_CAM_ON_NORMAL, PRISM_CAM_ON_OVER, PRISM_CAM_ON_CLICKED,
				       PRISM_CAM_ON_DISABLE, QTStr("Siderbar.Cam.Studio.Title"), 22, 22, 22, 22},
			      true);
}

void PLSMainView::initHelpMenu()
{
	PLS_PERFORMANCE_FUNCTION();
	helpMenu = pls_new<QMenu>(this);
#if defined(Q_OS_MACOS)
	helpMenu->setWindowFlag(Qt::NoDropShadowWindowHint);
#endif
	helpMenu->setObjectName("helpMenu");
	pls_uistep_v2_set_custom_show_hide_name(helpMenu, "Right SiderBar Help Menu");
	m_helpListWidget = pls_new<QListWidget>(this);
	QObject::connect(m_helpListWidget, &QListWidget::itemClicked, this, &PLSMainView::on_listWidget_itemClicked);
	m_helpListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_helpListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_helpListWidget->setObjectName("helpListWidget");
	m_helpListWidget->setSelectionMode(QAbstractItemView::NoSelection);
	m_helpListWidget->setFocusPolicy(Qt::NoFocus);
	QList<QString> titleList;
	titleList << QTStr("MainFrame.SideBar.Help.UserGuide") << QTStr("MainFrame.SideBar.Help.PrismFAQ") << QTStr("MainFrame.SideBar.Help.PrismWebsite") << QTStr("MainFrame.SideBar.Help.Discord")
		  << QTStr("MainFrame.SideBar.Help.ContactUs") << QTStr("NoticeAndUpdateMsg") << QTStr("MainFrame.SideBar.Help.ProgramInfo");
	/* Icons: prefer repo assets under resource/images/help-menu; Discord uses existing ic_Discord_off_normal.svg. */
	static const char *const kHelpMenuItemIconQrc[] = {
		":/resource/images/help-menu/ic-helpmenu-userguide.svg",
		":/resource/images/help-menu/ic-helpmenu-faq.svg",
		":/resource/images/help-menu/ic-helpmenu-website.svg",
		":/resource/images/ic_Discord_off_normal.svg",
		":/resource/images/help-menu/ic-helpmenu-contact.svg",
		":/resource/images/help-menu/ic-helpmenu-notiandupdate.svg",
		":/resource/images/help-menu/ic-helpmenu-update.svg",
	};
	static constexpr int kHelpMenuItemIconCount = int(sizeof(kHelpMenuItemIconQrc) / sizeof(kHelpMenuItemIconQrc[0]));
	int index = 0;

	const QString joined = titleList.join(", ");
	PLS_DEBUG(MAINMENU_MODULE, "initHelpMenu helpList size=%d, items=%s", titleList.size(), joined.toUtf8().constData());
	for (const QString &title : titleList) {
		QListWidgetItem *item = pls_new<QListWidgetItem>();
		item->setData(Qt::UserRole, UserGuide + index);
		const QString iconPath = (index < kHelpMenuItemIconCount) ? QString::fromUtf8(kHelpMenuItemIconQrc[index]) : QString();
		PLSNewIconActionWidget *widget = pls_new<PLSNewIconActionWidget>(title, nullptr, iconPath);
		m_helpListWidget->addItem(item);
		m_helpListWidget->setItemWidget(item, widget);
		index++;
	}
	QWidgetAction *action = pls_new<QWidgetAction>(helpMenu);
	action->setDefaultWidget(m_helpListWidget);
	helpMenu->addAction(action);
	QObject::connect(helpMenu, &QMenu::aboutToHide, this, &PLSMainView::helpMenuAboutToHide);
	updateHelpMenuGeometry();
}

void PLSMainView::updateHelpMenuGeometry()
{
	if (!m_helpListWidget || !helpMenu) {
		return;
	}

	constexpr int kHelpMenuMinWidthFallbackPx = 212;
	int minFromStyle = m_helpListWidget->minimumSizeHint().width();
	if (minFromStyle <= 0) {
		minFromStyle = helpMenu->minimumSizeHint().width();
	}
	const int minW = qMax(minFromStyle, kHelpMenuMinWidthFallbackPx);

	/* Row height must respect stylesheet (e.g. PLSNewIconActionWidget / helpListWidget::item min-height 40px).
	 * sizeHint() alone can be smaller than min-height, and setSizeHint would then squash rows. */
	constexpr int kHelpMenuRowMinHeightPx = 40;

	int maxRowW = minW;
	int rowH = 0;
	for (int i = 0; i < m_helpListWidget->count(); ++i) {
		auto *row = qobject_cast<PLSNewIconActionWidget *>(m_helpListWidget->itemWidget(m_helpListWidget->item(i)));
		if (!row) {
			continue;
		}
		const QSize sh = row->sizeHint();
		const QSize minSh = row->minimumSizeHint();
		const int rowW = qMax(qMax(sh.width(), minSh.width()), row->helpMenuRowMinimumWidth());
		maxRowW = qMax(maxRowW, rowW);
		rowH = qMax(rowH, qMax(sh.height(), minSh.height()));
	}
	rowH = qMax(rowH, kHelpMenuRowMinHeightPx);

	const int targetW = maxRowW;
	for (int i = 0; i < m_helpListWidget->count(); ++i) {
		if (QListWidgetItem *it = m_helpListWidget->item(i)) {
			it->setSizeHint(QSize(targetW, rowH));
		}
	}

	m_helpListWidget->setFixedWidth(targetW);
	helpMenu->setFixedWidth(targetW);
}

void PLSMainView::addSideBarSeparator()
{
	QLabel *label_separate = pls_new<QLabel>(this);
	label_separate->setObjectName("label_separate");
	m_sideBarLayout->addWidget(label_separate, 0, Qt::AlignHCenter | Qt::AlignTop);
}
void PLSMainView::addSideBarStretch()
{
	m_sideBarLayout->addStretch();
}

void PLSMainView::AdjustSideBarMenu()
{
	pls_check_app_exiting();
	// Always hide: no divider line between sidebar scroll area and bottom buttons.
	m_separatorBottom->hide();
}
void PLSMainView::hiddenWidget(QWidget *widget)
{
	QSizePolicy policy = widget->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	widget->setSizePolicy(policy);
	widget->hide();
}

void PLSMainView::setUiStepLogParams()
{
	pls_uistep_v2_set_title(this, QStringLiteral("Main Window"));
	pls_uistep_v2_set_info(ui->min, QStringLiteral("TitleBar Button"), QStringLiteral("Minimize"));
	pls_uistep_v2_set_info(ui->maxres, QStringLiteral("TitleBar Button"), [this]() { return isMaximized() ? QStringLiteral("Restore") : QStringLiteral("Maximize"); });
	pls_uistep_v2_set_info(ui->close, QStringLiteral("TitleBar Button"), QStringLiteral("Close"));

	pls_uistep_v2_set_value(m_userBtn, QStringLiteral("clicked"), QStringLiteral("User"));
	pls_uistep_v2_set_custom_enter_leave_name(m_userBtn, "Profile Icon");

	pls_uistep_v2_custom(m_studioModeBtn, QStringLiteral("clicked"), QStringLiteral("Choose"), QStringLiteral("Studio Mode"),
			     [this]() { return m_studioModeBtn->isChecked() ? QStringLiteral("Checked") : QStringLiteral("Unchecked"); });
	pls_uistep_v2_set_custom_enter_leave_name(m_studioModeBtn, "Studio Mode");

	pls_uistep_v2_set_value(m_resolutionBtn, QStringLiteral("clicked"), QStringLiteral("Resolution"));
	pls_uistep_v2_set_value(m_helpBtn, QStringLiteral("clicked"), QStringLiteral("Help"));
	pls_uistep_v2_set_value(m_settingsBtn, QStringLiteral("clicked"), QStringLiteral("Settings"));
	pls_uistep_v2_set_custom_enter_leave_name(m_resolutionBtn, "Resolution Guide");
	pls_uistep_v2_set_custom_enter_leave_name(m_settingsBtn, "Sider Bar Settings");
	pls_uistep_v2_set_custom_enter_leave_name(m_helpBtn, "PRISM Info");

	for (auto button : sideBarBtnGroup->buttons()) {
		if (!button->property("showMode").isNull()) {
			pls_uistep_v2_custom(button, QStringLiteral("clicked"), QStringLiteral("Choose"), button->toolTip(),
					     [button]() { return button->property("showMode").toBool() ? QStringLiteral("Unchecked") : QStringLiteral("Checked"); });
		} else {
			pls_uistep_v2_set_value(button, QStringLiteral("clicked"), button->toolTip());
		}
	}
}

bool PLSMainView::isSidebarButtonInScroll(ConfigId id) const
{
	if (sideBarBtnGroup) {
		auto btn = sideBarBtnGroup->button(id);
		return m_scrollArea->rect().contains(btn->mapTo(m_scrollArea, QPoint(0, 0)));
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

void PLSMainView::updateLoadingState(ConfigId id, bool visible)
{
	if (auto it = m_LoadingFrameMap.find(id); it == m_LoadingFrameMap.end()) {
		if (!visible) {
			return;
		}

		auto btn = sideBarBtnGroup->button(id);
		PLSLoadingView *frame = PLSLoadingView::newLoadingView(btn, -1, nullptr, QString(), QColor(39, 39, 39, 255));
		m_LoadingFrameMap.insert(id, frame);
	} else {
		PLSLoadingView *loadingFram = it.value();
		if (visible) {
			loadingFram->setVisible(visible);
		} else {
			pls_delete(loadingFram, nullptr);
			m_LoadingFrameMap.erase(it);
		}
	}
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
	m_studioModeBtn->setEnabled(enabled);
}

void PLSMainView::setStudioModeDimmed(bool bValue)
{
	m_studioModeBtn->setProperty("dimmed", bValue);
	pls_flush_style(m_studioModeBtn);
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
void PLSMainView::setUserButtonIcon(const QIcon &icon)
{
	m_userBtn->setIcon(icon);
}
void PLSMainView::initToastMsgView(bool isInitShow)
{
	PLS_PERFORMANCE_FUNCTION();
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
	m_studioModeBtn->setChecked(studioMode);
}

bool PLSMainView::isFirstShow() const
{
	return m_isFirstShow;
}

void PLSMainView::setSettingIconCheck(bool bCheck)
{
	QSignalBlocker block(m_settingsBtn);
	m_settingsBtn->setChecked(bCheck);
}

void PLSMainView::setResolutionBtnCheck(bool bCheck)
{
	QSignalBlocker block(m_resolutionBtn);
	m_resolutionBtn->setChecked(bCheck);
}

bool PLSMainView::isSettingEnabled() const
{
	return m_settingsBtn->isEnabled();
}

#ifdef _WIN32
void PLSMainView::readDetectResult()
{
	PLS_PERFORMANCE_FUNCTION();
	// read previous file, then remove this file
	auto pc_state_file = pls_get_app_user_data_file_path_pn(QStringLiteral("/Cache/") + PC_STATE_FILE, false);
	QFile file(pc_state_file);
	if (!file.exists())
		return;

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
	PLS_PERFORMANCE_FUNCTION();
	// run process to detect pc and write new file if need
	auto pc_state_file = pls_get_app_user_data_file_path_pn(QStringLiteral("/Cache/") + PC_STATE_FILE);

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

	std::shared_ptr<int> auto_free(nullptr, [job_handle, state_event](int *) {
		if (job_handle) {
			CloseHandle(job_handle);
		}

		if (state_event) {
			CloseHandle(state_event);
		}
	});
	freeHandle = auto_free;
}
#endif

bool PLSMainView::isMatchOBSSupportPluginVersion(const QString &winObsPath)
{

	auto isVersionOK = [](int major, int minor) -> bool { return (LIBOBS_API_MAJOR_VER == major && LIBOBS_API_MINOR_VER == minor); };

	bool isRunningOk = false;
	QString obsVer;
	auto verSuccessed = false;

#ifdef __APPLE__
	obsVer = pls_get_installed_obs_version();
	auto versionList = obsVer.split(".");
	verSuccessed = versionList.size() >= 2;
	if (verSuccessed) {
		isRunningOk = isVersionOK(versionList[0].toInt(), versionList[1].toInt());
	}
#else
	pls_win_ver_t vers;
	memset(&vers, 0, sizeof(vers));
	verSuccessed = pls_get_win_dll_ver(vers, winObsPath + "/bin/64bit/obs64.exe");

	if (verSuccessed) {
		obsVer = QString("%1.%2.%3").arg(vers.major).arg(vers.minor).arg(vers.build);
	} else {
		obsVer = QString::fromUtf8(u8"notInstalled");
	}
	isRunningOk = isVersionOK(vers.major, vers.minor);
#endif

	PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE,
		  {
			  {"obsPluginUsed", isRunningOk ? "yes" : "no"},
			  {"obsInstalled", verSuccessed ? "yes" : "no"},
			  {"obsVersion", obsVer.toUtf8().constData()},
		  },
		  "current OBS version = %d.%d.%d (PRISM), "
		  "OBS installed = %s, "
		  "OBS version = %s, "
		  "use 3rd-plugin from OBS: %s",
		  LIBOBS_API_MAJOR_VER, LIBOBS_API_MINOR_VER, LIBOBS_API_PATCH_VER, verSuccessed ? "yes" : "no", obsVer.toUtf8().constData(), isRunningOk ? "yes" : "no");

	return isRunningOk;
}
