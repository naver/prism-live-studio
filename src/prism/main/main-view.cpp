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
#include <QMetaEnum>
#include <QButtonGroup>

#include "action.h"
#include "log/module_names.h"
#include "pls-app.hpp"
#include "window-basic-main.hpp"
#include "toast-view.hpp"
#include "alert-view.hpp"
#include "PLSChannelsVirualAPI.h"
#include "custom-help-menu-item.hpp"
#include "frontend-api.h"
#include "PLSLivingMsgView.hpp"
#include "PLSToastMsgPopup.hpp"
#include "login-common-helper.hpp"
#include "login-user-info.hpp"
#include "pls-common-define.hpp"
#include "PLSToplevelView.hpp"
#include "PLSBlockDump.h"
#include "PLSToastButton.hpp"
#include "ResolutionTipFrame.h"
#include "ResolutionGuidePage.h"
#include "PLSAction.h"

#if defined(Q_OS_WINDOWS)
#include <Windows.h>
#endif // Q_OS_WINDOWS

#include "PLSPlatformApi.h"
#include <QTimer>
#include "PLSVirtualBgManager.h"

#include "prism/PLSPlatformPrism.h"
#include "log.h"
#include "log/log.h"

#define LIVINGMSGVIEW "LivingMsgView"
#define MAINVIEWMODE "mainViewMode"

#define EMAIL_TYPE QStringLiteral("email")

const QString sideBarButtonSizeStyle = "#%1 {\
        min-width: /*hdpi*/ %2px;\
        max-width: /*hdpi*/ %3px;\
        min-height: /*hdpi*/ %4px;\
        max-height: /*hdpi*/ %5px;\
}";

const QString sideBarStyle = "#%1 {\
	image: url(\"%2\");\
	background: transparent;\
}\
#%1[showMode=\"false\"]\
{\
	image:url(\"%2\");\
}\
#%1[showMode=\"false\"]:hover\
{\
	image:url(\"%3\");\
}\
#%1[showMode=\"false\"]:pressed\
{\
	image:url(\"%4\");\
}\
#%1[showMode=\"false\"]:!enable\
{\
	image:url(\"%5\");\
}\
#%1[showMode=\"true\"]\
{\
	image:url(\"%6\");\
}\
#%1[showMode=\"true\"]:hover\
{\
	image:url(\"%7\");\
}\
#%1[showMode=\"true\"]:pressed\
{\
	image:url(\"%8\");\
}\
#%1[showMode=\"true\"]:!enable\
{\
	image:url(\"%9\");\
}";

#ifdef DEBUG
constexpr int ResolutionHoldingTime = 25000;
#else
constexpr int ResolutionHoldingTime = 5000;
#endif // DEBUG

enum HelpWidgetItemTag { PrismFAQ = 100, PrismWebsite, ContactUs, CheckForUpdate };

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

volatile bool g_mainViewValid = false;

PLSMainView::PLSMainView(QWidget *parent, PLSDpiHelper dpiHelper) : PLSToplevelView<QFrame>(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint), ui(new Ui::PLSMainView)
{
	g_mainViewValid = true;

	dpiHelper.setCss(this, {PLSCssIndex::QCheckBox,
				PLSCssIndex::QLineEdit,
				PLSCssIndex::QMenu,
				PLSCssIndex::QPlainTextEdit,
				PLSCssIndex::QPushButton,
				PLSCssIndex::QRadioButton,
				PLSCssIndex::QScrollBar,
				PLSCssIndex::QSlider,
				PLSCssIndex::QSpinBox,
				PLSCssIndex::QTableView,
				PLSCssIndex::QTabWidget,
				PLSCssIndex::QTextEdit,
				PLSCssIndex::QToolButton,
				PLSCssIndex::QToolTip,
				PLSCssIndex::QComboBox,
				PLSCssIndex::PLSDock,
				PLSCssIndex::PLSMainView,
				PLSCssIndex::ChannelCapsule,
				PLSCssIndex::PLSChannelsArea,
				PLSCssIndex::ChannelConfigPannel,
				PLSCssIndex::DefaultPlatformAddList,
				PLSCssIndex::GoLivePannel,
				PLSCssIndex::PLSRTMPConfig});
	dpiHelper.setMinimumSize(this, {870, 633});
	dpiHelper.setInitSize(this, {1310, 817});
	ui->setupUi(this);
	auto currentEnvironment = pls_is_dev_server() ? "(Dev)" : "";
	ui->devLabel->setText(currentEnvironment);
	setMouseTracking(true);
	ui->titleBar->setMouseTracking(true);
	ui->body->setMouseTracking(true);
	ui->content->setMouseTracking(true);
	ui->rightArea->setMouseTracking(true);
	ui->scrollArea->setMouseTracking(true);
	ui->side_bar_menus->setMouseTracking(true);
	ui->bottomArea->setMouseTracking(true);
	ui->channelsArea->setMouseTracking(true);
	ui->channelsArea->installEventFilter(this);
	ui->content->installEventFilter(this);
	this->installEventFilter(this);

	initSideBarButtons();

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

	helpMenu = new QMenu(this);
	helpMenu->setObjectName("helpMenu");
	QListWidget *listWidget = new QListWidget(this);
	QObject::connect(listWidget, &QListWidget::itemClicked, this, &PLSMainView::on_listWidget_itemClicked);
	listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	listWidget->setObjectName("helpListWidget");
	listWidget->setSelectionMode(QAbstractItemView::NoSelection);
	listWidget->setFocusPolicy(Qt::NoFocus);
	QList<QString> titleList;
	titleList << QTStr("MainFrame.SideBar.Help.PrismFAQ") << QTStr("MainFrame.SideBar.Help.PrismWebsite") << QTStr("MainFrame.SideBar.Help.ContactUs")
		  << QTStr("MainFrame.SideBar.Help.checkUpdate");
	for (int i = 0; i < titleList.size(); i++) {
		QListWidgetItem *item = new QListWidgetItem();
		item->setData(Qt::UserRole, PrismFAQ + i);
		CustomHelpMenuItem *widget = new CustomHelpMenuItem(titleList[i]);
		if (i == titleList.size() - 1) {
			checkMenuItem = widget;
			checkListWidgetItem = item;
		}
		listWidget->addItem(item);
		listWidget->setItemWidget(item, widget);
	}
	QHBoxLayout *layout = new QHBoxLayout(listWidget);
	listWidget->setLayout(layout);
	QWidgetAction *action = new QWidgetAction(helpMenu);
	action->setDefaultWidget(listWidget);
	helpMenu->addAction(action);
	QObject::connect(helpMenu, &QMenu::aboutToHide, this, &PLSMainView::helpMenuAboutToHide);

	DialogInfo mobileInfo;
	mobileInfo.configId = ConfigId::WiFiConfig;
	mobileInfo.defaultHeight = 817;
	mobileInfo.defaultWidth = 300;
	mobileInfo.defaultOffset = 5;
	m_usbWiFiView = new PLSUSBWiFiHelpView(mobileInfo);

	toast = new PLSToastView(this);
	m_toastMsg = new PLSToastMsgPopup(this);
	m_toastMsg->hide();

	DialogInfo info;
	info.configId = ConfigId::LivingMsgView;
	info.defaultHeight = 400;
	info.defaultWidth = 300;
	info.defaultOffset = 5;
	m_livingMsgView = new PLSLivingMsgView(info);

	QObject::connect(ui->min, &QToolButton::clicked, this, [this]() {
		PLS_UI_STEP(MAINFRAME_MODULE, "Main Frame Title Bar's Minimized Button", ACTION_CLICK);
		showMinimized();
	});
	QObject::connect(ui->maxres, &QToolButton::clicked, this, [this]() {
		PLS_UI_STEP(MAINFRAME_MODULE, "Main Frame Title Bar's Maximized Button", ACTION_CLICK);
		if (!isMaxState && !isFullScreenState) {
			showMaximized();
		} else {
			showNormal();
		}
	});
	QObject::connect(ui->close, &QToolButton::clicked, this, [this]() {
		PLS_UI_STEP(MAINFRAME_MODULE, "Main Frame Title Bar's Close Button", ACTION_CLICK);
		close();
	});

	QObject::connect(PLS_PLATFORM_API, &PLSPlatformApi::enterLivePrepareState, this, [=](bool disable) { ui->settings->setEnabled(!disable); });
	QObject::connect(ui->ResolutionBtn, &QAbstractButton::toggled, this, &PLSMainView::showResolutionGuidePage, Qt::QueuedConnection);

	//default the tooltip only show the active window, so need to change tooltip default action, modify the widget window attribute
	this->setAttribute(Qt::WA_AlwaysShowToolTips, true);

	// Side-bar menu scroll area.
	ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	ui->scrollArea->setObjectName("sidebarMenuScrollarea");
	ui->label_separator_bottom->hide();
}

PLSMainView::~PLSMainView()
{
	PLS_INFO(MAINSCENE_MODULE, __FUNCTION__);

	g_mainViewValid = false;

	if (m_usbWiFiView)
		delete m_usbWiFiView;
	if (m_livingMsgView)
		delete m_livingMsgView;
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
	this->captionHeight = captionHeight - BORDER_WIDTH();
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
	PLSDpiHelper::setDynamicContentsMargins(ui->titleBarLayout, true);
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
	PLSDpiHelper::setDynamicContentsMargins(ui->titleBarLayout, true);
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
	ToplevelView::closeEvent(event);
}

void PLSMainView::setStudioMode(bool studioMode)
{
	ui->studioMode->setChecked(studioMode);
}

void PLSMainView::toastMessage(pls_toast_info_type type, const QString &message, int)
{
	QString mesIngoreChar = message;

	PLS_INFO(NOTICE_MODULE, QString("a new toast message:%1").arg(mesIngoreChar.replace("%", "%%")).toUtf8().data());

	qint64 currtime = QDateTime::currentMSecsSinceEpoch();
	toastMessages.insert(currtime, message);
	auto btn = qobject_cast<PLSToastButton *>(getSiderBarButton("alert"));
	if (btn) {
		int messageSize = toastMessages.size();
		btn->setNum(messageSize);
		auto api = PLSBasic::Get()->getApi();
		if (api) {
			api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_NOTICE_MESSAGE, {messageSize});
		}
	}

	m_toastMsg->showMsg(message, static_cast<pls_toast_info_type>(type));
	int width = ui->content->width();
	QPoint pt = ui->content->mapToGlobal(QPoint(width, 0));
	pt = mapFromGlobal(pt);
	m_toastMsg->show();
	m_toastMsg->move(pt.x() - m_toastMsg->width() - PLSDpiHelper::calculate(this, 10), pt.y() + PLSDpiHelper::calculate(this, 10));

	//add toast msg view
	PLS_LIVE_INFO(MODULE_PlatformService, message.toUtf8().constData());
	m_livingMsgView->addMsgItem(message, currtime, type);
}

void PLSMainView::toastMessage(pls_toast_info_type type, const QString &message, const QString &url, const QString &replaceStr, int autoClose)
{
	QString msg(message);
	toastMessage(type, m_livingMsgView->getInfoWithUrl(message, url, replaceStr), autoClose);
	m_toastMsg->showMsg(msg.replace(url, replaceStr), static_cast<pls_toast_info_type>(type));
}

void PLSMainView::toastClear()
{
	toastMessages.clear();
	auto btn = qobject_cast<PLSToastButton *>(getSiderBarButton("alert"));
	if (btn) {
		int messageSize = toastMessages.size();
		btn->setNum(messageSize);
		auto api = PLSBasic::Get()->getApi();
		if (api) {
			api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_NOTICE_MESSAGE, {messageSize});
		}
	}
	m_toastMsg->hide();
	m_livingMsgView->clearMsgView();
}

void PLSMainView::setUserButtonIcon(const QIcon &icon)
{
	ui->user->setIcon(icon);
}

void PLSMainView::initMobileHelperView(bool isInitShow)
{
	m_usbWiFiView->setVisible(isInitShow);
}

void PLSMainView::initToastMsgView(bool isInitShow)
{
	m_livingMsgView->setShow(isInitShow);
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
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
	emit maxFullScreenStateChanged(isMaxState, isFullScreenState);
}

void PLSMainView::onSaveNormalGeometry()
{
	config_set_string(App()->GlobalConfig(), "BasicWindow", "geometry", saveGeometry().toBase64().constData());
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
}

bool PLSMainView::isClosing() const
{
	return closing;
}
void PLSMainView::close()
{
	if (!closing) {
		closing = true;
		ToplevelView::close();
	}
}

void PLSMainView::OnBeautySourceDownloadFinished()
{
	emit BeautySourceDownloadFinished();
}

void PLSMainView::registerSideBarButton(ConfigId id, const IconData &data, bool inFixedArea)
{
	if (!sideBarBtnGroup) {
		sideBarBtnGroup = new QButtonGroup(this);
		sideBarBtnGroup->setExclusive(false);
		connect(sideBarBtnGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &PLSMainView::onSideBarButtonClicked);
	}
	QPushButton *btn = nullptr;
	auto layout = inFixedArea ? ui->verticalLayout_fixed : ui->verticalLayout_side_bar;
	if (ConfigId::LivingMsgView == id) {
		PLSToastButton *button = new PLSToastButton(this);
		button->setObjectName("alert");
		layout->addWidget(button, 0, Qt::AlignHCenter | Qt::AlignTop);
		btn = button->getButton();
	} else {
		btn = new QPushButton(this);
		layout->addWidget(btn, 0, Qt::AlignHCenter | Qt::AlignTop);
	}

	sideBarBtnGroup->addButton(btn, id);
	PLSDpiHelper dpiHelper;
	const char *objName = QMetaEnum::fromType<ConfigId>().valueToKey(id);
	btn->setObjectName(objName);
	btn->setToolTip(data.tooltip);

	auto styleStr = generalStyleSheet(objName, data);
	dpiHelper.setStyleSheet(btn, styleStr);

	SideWindowInfo info;
	info.id = id;
	info.windowName = data.tooltip;

	if (objName && *objName) {
		bool show = config_get_bool(App()->GlobalConfig(), objName, "showMode");
		btn->setProperty("showMode", show);
		pls_flush_style(btn);
		info.visible = show;
	}
	windowInfo << info;
}

void PLSMainView::updateSideBarButtonStyle(ConfigId id, bool on)
{
	if (sideBarBtnGroup) {
		auto btn = sideBarBtnGroup->button(id);
		if (btn) {
			btn->setProperty("showMode", on);
			pls_flush_style(btn);
		}

		for (auto &item : windowInfo) {
			if (id == item.id) {
				item.visible = on;
				break;
			}
		}
		PLSBasic::Get()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_SIDE_WINDOW_VISIBLE_CHANGED, {id, on});
	}
}

QList<SideWindowInfo> PLSMainView::getSideWindowInfo() const
{
	return windowInfo;
}

int PLSMainView::getToastMessageCount() const
{
	return toastMessages.size();
}

void PLSMainView::setSidebarWindowVisible(int windowId, bool visible)
{
	if (sideBarBtnGroup) {
		auto button = sideBarBtnGroup->button(windowId);
		if (button) {
			bool checked = button->property("showMode").toBool();
			if ((visible && !checked) || (!visible && checked)) {
				onSideBarButtonClicked(windowId);
			}
		}
	}
}

void PLSMainView::resetToastViewPostion(pls_frontend_event event, const QVariantList &, void *)
{
	if (event == pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGOUT) {
		config_set_string(App()->GlobalConfig(), "LivingMsgView", "geometry", "");
		config_set_bool(App()->GlobalConfig(), "LivingMsgView", "showMode", false);
		static_cast<PLSMainView *>(pls_get_main_view())->m_livingToastViewShow = false;
		config_set_bool(App()->GlobalConfig(), "General", "Watermark", true);
		config_save(App()->GlobalConfig());
	}
}

void PLSMainView::helpMenuAboutToHide()
{
	ui->help->setChecked(false);
}

void PLSMainView::initSideBarButtons()
{
	registerSideBarButton(ConfigId::ChatConfig, IconData{CHAT_OFF_NORMAL, CHAT_OFF_OVER, CHAT_OFF_CLICKED, CHAT_OFF_DISABLE, CHAT_ON_NORMAL, CHAT_ON_OVER, CHAT_ON_CLICKED, CHAT_ON_DISABLE,
							     QTStr("main.rightbar.chat.tooltip"), 22, 22, 22, 22});

	registerSideBarButton(ConfigId::LivingMsgView, IconData{TOAST_OFF_NORMAL, TOAST_OFF_OVER, TOAST_OFF_CLICKED, TOAST_OFF_DISABLE, TOAST_ON_NORMAL, TOAST_ON_OVER, TOAST_ON_CLICKED,
								TOAST_ON_DISABLE, QTStr("main.rightbar.alert.tooltip"), 22, 22, 22, 22});
	addSideBarSeparator();

	registerSideBarButton(ConfigId::BeautyConfig,
			      IconData{BEAUTYEFFECT_OFF_NORMAL, BEAUTYEFFECT_OFF_OVER, BEAUTYEFFECT_OFF_CLICKED, BEAUTYEFFECT_OFF_DISABLE, BEAUTYEFFECT_ON_NORMAL, BEAUTYEFFECT_ON_OVER,
				       BEAUTYEFFECT_ON_CLICKED, BEAUTYEFFECT_ON_DISABLE, QTStr("main.beauty.title")},
			      false);

	registerSideBarButton(ConfigId::VirtualbackgroundConfig,
			      IconData{VIRTUAL_OFF_NORMAL, VIRTUAL_OFF_OVER, VIRTUAL_OFF_CLICKED, VIRTUAL_OFF_DISABLE, VIRTUAL_ON_NORMAL, VIRTUAL_ON_OVER, VIRTUAL_ON_CLICKED, VIRTUAL_ON_DISABLE,
				       QTStr("virtual.view.main.button.toolTip"), 26, 26, 26, 26},
			      false);

	registerSideBarButton(ConfigId::PrismStickerConfig,
			      IconData{PRISM_STICKER_OFF_NORMAL, PRISM_STICKER_OFF_OVER, PRISM_STICKER_OFF_CLICKED, PRISM_STICKER_OFF_DISABLE, PRISM_STICKER_ON_NORMAL, PRISM_STICKER_ON_OVER,
				       PRISM_STICKER_ON_CLICKED, PRISM_STICKER_ON_DISABLE, QTStr("main.prism.sticker.title")},
			      false);

	registerSideBarButton(
		ConfigId::GiphyStickersConfig,
		IconData{GIPHY_OFF_NORMAL, GIPHY_OFF_OVER, GIPHY_OFF_CLICKED, GIPHY_OFF_DISABLE, GIPHY_ON_NORMAL, GIPHY_ON_OVER, GIPHY_ON_CLICKED, GIPHY_ON_DISABLE, QTStr("main.giphy.title")}, false);

	registerSideBarButton(ConfigId::BgmConfig,
			      IconData{BGM_OFF_NORMAL, BGM_OFF_OVER, BGM_OFF_CLICKED, BGM_OFF_DISABLE, BGM_ON_NORMAL, BGM_ON_OVER, BGM_ON_CLICKED, BGM_ON_DISABLE, QTStr("Bgm.Title")}, false);

	registerSideBarButton(ConfigId::WiFiConfig,
			      IconData{WIFI_OFF_NORMAL, WIFI_OFF_OVER, WIFI_OFF_CLICKED, WIFI_OFF_DISABLE, WIFI_ON_NORMAL, WIFI_ON_OVER, WIFI_ON_CLICKED, WIFI_ON_DISABLE,
				       QTStr("sidebar.wifihelper.tooltip")},
			      false);
	addSideBarStretch();
}

void PLSMainView::addSideBarSeparator(bool inFixedArea)
{
	QLabel *label_separate = new QLabel(this);
	label_separate->setObjectName("label_separate");
	auto layout = inFixedArea ? ui->verticalLayout_fixed : ui->verticalLayout_side_bar;
	layout->addWidget(label_separate, 0, Qt::AlignHCenter | Qt::AlignTop);
}

void PLSMainView::addSideBarStretch()
{
	ui->verticalLayout_side_bar->addStretch();
}

QString PLSMainView::generalStyleSheet(const QString &objectName, IconData data)
{
	QString strStyle;
	strStyle = sideBarStyle.arg(objectName, data.iconOffNormal, data.iconOffHover, data.iconOffPress, data.iconOffDisabled, data.iconOnNormal, data.iconOnHover, data.iconOnPress,
				    data.iconOnDisabled);

	QString btnStyle = sideBarButtonSizeStyle.arg(objectName).arg(data.minWidth).arg(data.maxWidth).arg(data.minHeight).arg(data.maxHeight);
	return strStyle + btnStyle;
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

void PLSMainView::toggleResolutionButton(bool isVisible)
{
	QSignalBlocker blokcer(ui->ResolutionBtn);
	ui->ResolutionBtn->setChecked(isVisible);
}

void PLSMainView::AdjustSideBarMenu()
{
	if (ui->scrollArea->verticalScrollBar()->maximum() > 0) {
		ui->label_separator_bottom->show();
	} else {
		ui->label_separator_bottom->hide();
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
	m_livingMsgView->setShow(!m_livingMsgView->isVisible());
}

extern const QString translatePlatformName(const QString &platformName);

void PLSMainView::showResolutionGuidePage(bool visible)
{
#ifdef DEBUG
	showResolutionTips(NAVER_SHOPPING_LIVE);
#endif // _DEBUG

	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Resolution Button", ACTION_CLICK);

	ResolutionGuidePage::setVisibleOfGuide(this, nullptr, ResolutionGuidePage::isAcceptToChangeResolution, visible);
}

void PLSMainView::showResolutionTips(const QString &platform)
{
	static QPointer<ResolutionTipFrame> last = nullptr;
	if (last != nullptr) {
		last->close();
		last = nullptr;
	}
	ResolutionTipFrame *lb = new ResolutionTipFrame(this);
	lb->setObjectName("ResolutionTipsLabel");
	lb->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
	lb->setWindowOpacity(0.95);
	lb->setAttribute(Qt::WA_TranslucentBackground);
	lb->setAttribute(Qt::WA_DeleteOnClose);

	auto CalculatePos = [=]() {
		lb->adjustSize();
		auto differ = QPoint(lb->frameRect().width(), (lb->height() - ui->ResolutionBtn->height()) / 2);
		auto pos = ui->ResolutionBtn->mapToGlobal(QPoint(0, 0) - differ);
		lb->move(pos);
	};

	auto tryUpdateTip = [=]() {
		if (lb->isVisible()) {
			CalculatePos();
		}
	};

	connect(this, &PLSMainView::mainViewUIChanged, lb, tryUpdateTip, Qt::DirectConnection);
	connect(
		this, &PLSMainView::isshowSignal, lb,
		[lb](bool visible) {
			if (!visible) {
				last = nullptr;
				lb->on_CloseBtn_clicked();
			}
		},
		Qt::DirectConnection);
	PLSDpiHelper helper;
	helper.notifyDpiChanged(lb, [=]() { QTimer::singleShot(500, lb, tryUpdateTip); });

	QString tips;
	if (platform.contains(CUSTOM_RTMP)) {
		tips = tr("Resolution.CustomRTMPTips");
	} else {
		tips = tr("Resolution.ButtonTips").arg(translatePlatformName(platform));
	}
	lb->setText(tips);
	CalculatePos();
	lb->show();
	last = lb;
	CalculatePos();
	QTimer::singleShot(ResolutionHoldingTime, lb, [lb]() {
		last = nullptr;
		lb->on_CloseBtn_clicked();
	});
}

void PLSMainView::on_help_clicked()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help Button", ACTION_CLICK);
	bool isChecked = ui->help->isChecked();
	if (isChecked) {
		PLSBasic *main = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());
		checkMenuItem->setBadgeVisible(main->getUpdateResult() == MainCheckUpdateResult::HasUpdate);
		bool disabled = pls_is_living_or_recording();
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

void PLSMainView::on_settings_clicked()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Setting Button", ACTION_CLICK);
	emit popupSettingView(QStringLiteral("General"), QString());
	if (g_mainViewValid) {
		ui->settings->setChecked(false);
	}
}

void PLSMainView::on_listWidget_itemClicked(QListWidgetItem *item)
{
	helpMenu->setHidden(true);
	int tagInt = item->data(Qt::UserRole).toInt();
	if (tagInt == PrismFAQ) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList PrismFAQ Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::Get(), "on_actionPrismFAQ_triggered");
	} else if (tagInt == PrismWebsite) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList PrismWebsite Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::Get(), "on_actionPrismWebsite_triggered");
	} else if (tagInt == ContactUs) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList ContactUs Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::Get(), "on_actionContactUs_triggered");
	} else if (tagInt == CheckForUpdate) {
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Help MenuItemList checkFourUpdate Button", ACTION_CLICK);
		QMetaObject::invokeMethod(PLSBasic::Get(), "on_actionCheckUpdate_triggered");
	}
}

void PLSMainView::onSideBarButtonClicked(int buttonId)
{
	emit sideBarButtonClicked(buttonId);
}

void PLSMainView::wifiHelperclicked()
{
	if (m_usbWiFiView->isVisible()) {
		m_usbWiFiView->hide();
	} else {
		m_usbWiFiView->show();
	}
}

void PLSMainView::on_mobile_lost()
{
	static bool bShowingAlert = false;
	if (bShowingAlert) {
		return;
	}

	bShowingAlert = true;
	if (pls_is_streaming()) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("PrsimMobile.device_lost"));
	} else {
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("PrsimMobile.device_lost"));
	}
	bShowingAlert = false;
}

void PLSMainView::on_mobile_disconnect()
{
	static bool bShowingAlert = false;
	if (bShowingAlert) {
		return;
	}

	bShowingAlert = true;
	if (pls_is_streaming()) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, QTStr("PrsimMobile.device_disconnected"));
	} else {
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("PrsimMobile.device_disconnected"));
	}
	bShowingAlert = false;
}

void PLSMainView::on_mobile_connected(QString mobileName)
{
}

void PLSMainView::closeEvent(QCloseEvent *event)
{
	auto bCloseByKeyboard = (GetAsyncKeyState(VK_MENU) < 0) && (GetAsyncKeyState(VK_F4) < 0);
	PLS_INFO(MAINSCENE_MODULE, bCloseByKeyboard ? __FUNCTION__ " by ALT+F4" : __FUNCTION__);

	closing = true;
	emit mainViewUIChanged();

	closeEventCallback(event);
}

void PLSMainView::showEvent(QShowEvent *event)
{
	// set user photo tooltip
	if (PLSLoginUserInfo::getInstance()->getAuthType() == EMAIL_TYPE) {
		ui->user->setToolTip(PLSLoginUserInfo::getInstance()->getEmail());
	} else {
		ui->user->setToolTip(PLSLoginUserInfo::getInstance()->getNickname());
	}

	ui->bottomArea->StartStatusMonitor();

	emit isshowSignal(true);
	ToplevelView::showEvent(event);

	PLSBlockDump::Instance()->StartMonitor();
}

void PLSMainView::hideEvent(QHideEvent *event)
{
	emit isshowSignal(false);
	emit mainViewUIChanged();
	if (!getMaxState() && !getFullScreenState()) {
		config_set_string(App()->GlobalConfig(), "BasicWindow", "geometry", saveGeometry().toBase64().constData());
	}
	config_save(App()->GlobalConfig());
	ToplevelView::hideEvent(event);
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
	} else if (watcher == ui->content) {
		if (event->type() == QEvent::Resize) {
			if (m_toastMsg->isVisible()) {
				int width = ui->content->width();
				QPoint pt = ui->content->mapToGlobal(QPoint(width, 0));
				pt = mapFromGlobal(pt);
				m_toastMsg->move(pt.x() - m_toastMsg->width() - PLSDpiHelper::calculate(this, 10), pt.y() + PLSDpiHelper::calculate(this, 10));
				m_toastMsg->adjustSize();
			}
		}
	} else if (watcher == this) {
		switch (event->type()) {
		case QEvent::WindowActivate: {
			if (!pls_inside_visible_screen_area(this->geometry())) {
				PLSWidgetDpiAdapter::restoreGeometry(this, saveGeometry());
			}
			break;
		}
		case QEvent::Show:
		case QEvent::Resize: {
			QTimer::singleShot(0, this, [=]() { AdjustSideBarMenu(); });
			break;
		}
		}
		if (event->type() == QEvent::Move || event->type() == QEvent::DragMove || event->type() == QEvent::Resize)
			emit mainViewUIChanged();
	}

	return ToplevelView::eventFilter(watcher, event);
}
