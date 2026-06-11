#include "PLSChatDialog.h"
#include <assert.h>
#include <frontend-api.h>
#include <qdebug.h>
#include <qdesktopservices.h>
#include <qlabel.h>
#include <qstyle.h>
#include <qtabbar.h>
#include <QFile>
#include <QSettings>
#include "../PLSPlatformApi/PLSPlatformApi.h"
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "PLSChatFontZoomFrame.h"
#include "PLSMainView.hpp"
#include "log/log.h"

#include <PLSBrowserPanel.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include "PLSSyncServerManager.hpp"
#include "pls-common-define.hpp"

#include "PLSWatchers.h"
#include "libbrowser.h"
#include "prism/PLSPlatformPrism.h"
#include "ui_PLSChatDialog.h"
#include "window-basic-main.hpp"

using namespace std;
using namespace common;

static const char *const s_chatModuleName = "PLSChat";
static const char *const s_geometrySection = "BasicWindow";
static const char *const s_geometryName = "geometryChat";
static const char *const s_maxSection = "ChatDialog";
static const char *const s_maxName = "MaximizedState";
static const char *const s_hiddenSection = "Basic";
static const char *const s_hiddenName = "chatIsHidden";

extern PLSQCef *plsCef;

namespace {
//PRISM/Zhangdewen/20200921/#/add chat source button
class ChatSourceButton : public QFrame {
	bool hovered = false;
	bool pressed = false;
	std::function<void()> clicked;

public:
	void updateTextWidth(int moreWidth)
	{
		QFontMetrics fontWidth(m_text->font());
		QString elidedText = fontWidth.elidedText(m_showStr, Qt::ElideRight, qMax(m_text->size().width() + moreWidth - 10, 0));
		if (elidedText == "…") {
			elidedText = "";
		}
		m_text->setText(elidedText);
	}
	ChatSourceButton(const QString &buttonText, QWidget *parent, std::function<void()> clicked_) : QFrame(parent), clicked(std::move(clicked_)), m_showStr(buttonText)
	{
		pls_add_css(this, {"ChatSourceButton"});
		setObjectName("chatSourceButton");
		setProperty("lang", pls_get_current_language());
		setMouseTracking(true);
		setProperty("showHandCursor", true);

		QLabel *icon = pls_new<QLabel>(this);
		icon->setObjectName("chatSourceButtonIcon");
		icon->setMouseTracking(true);

		QLabel *text = pls_new<QLabel>(this);
		text->setObjectName("chatSourceButtonText");
		text->setMouseTracking(true);
		text->setText(buttonText);
		m_text = text;

		QHBoxLayout *layout = pls_new<QHBoxLayout>(this);
		layout->setContentsMargins(10, 0, 10, 0);
		layout->setSpacing(5);
		layout->addWidget(icon);
		layout->addWidget(text);
	}
	~ChatSourceButton() override = default;

private:
	QLabel *m_text;
	QString m_showStr;

	void setState(const char *name, bool &state, bool value)
	{
		if (state != value) {
			pls_flush_style_recursive(this, name, state = value);
		}
	}

protected:
	bool event(QEvent *event) override
	{
		switch (event->type()) {
		case QEvent::Enter:
			setState("hovered", hovered, true);
			break;
		case QEvent::Leave:
			setState("hovered", hovered, false);
			break;
		case QEvent::MouseButtonPress:
			setState("pressed", pressed, true);
			break;
		case QEvent::MouseButtonRelease:
			setState("pressed", pressed, false);
			if (rect().contains(dynamic_cast<QMouseEvent *>(event)->pos())) {
				clicked();
			}
			break;
		case QEvent::MouseMove:
			setState("hovered", hovered, rect().contains(dynamic_cast<QMouseEvent *>(event)->pos()));
			break;
		default:
			break;
		}
		return QFrame::event(event);
	}
};
}

PLSChatDialog::PLSChatDialog(QWidget *parent) : QWidget(parent)
{
	ui = pls_new<Ui::PLSChatDialog>();
	ui->setupUi(this);

	pls_add_css(this, {"PLSChatDialog"});
	setupFirstUI();

	connect(PLS_PLATFORM_API, &PLSPlatformApi::channelRemoved, this, [this]() { refreshUI(); }, Qt::QueuedConnection);

	connect(PLS_PLATFORM_API, &PLSPlatformApi::platformActiveDone, this, [this]() { refreshUI(); }, Qt::QueuedConnection);
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::sigOperationChannelDone, this,
		[this]() {
			pls_check_app_exiting();
			refreshUI();
			updateTabBtnCss();
		},
		Qt::QueuedConnection);

	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveStarted, this,
		[this](bool isSucceed) {
			pls_check_app_exiting();
			updateRtmpPlaceText();
			if (isSucceed) {
				showToastIfNeeded();
				updateYoutubeUrlIfNeeded();
				PLSBasic::setManualPannelCookies(ALL_CHAT);
			}
		},
		Qt::QueuedConnection);
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveEnded, this,
		[this]() {
			PLS_INFO(s_chatModuleName, "live end chat view refresh ui");
			refreshUI();
			m_bShowToastAgain = false;
		},
		Qt::QueuedConnection);

	connect(PLS_SYNC_SERVER_MANAGE, &PLSSyncServerManager::libraryNeedUpdate, this, [this](bool isSucceed) {
		pls_check_app_exiting();
		if (!isSucceed) {
			PLS_INFO(s_chatModuleName, "chat all page download failed, maybe the chat page is loaded older.");
			return;
		}
		PLS_INFO(s_chatModuleName, "chat all page download succeed, to force update chat all page url");
		auto size = m_vecChatDatas.size();
		for (int index = 0; index < size; ++index) {
			QVariantMap info;
			PLS_CHAT_HELPER->getSelectInfoFromIndex(index, info);
			std::string newUrl = PLS_CHAT_HELPER->getChatUrlWithIndex(index, info);
			m_vecChatDatas[index].url = newUrl;
			if (m_vecChatDatas[index].isWebLoaded) {
				setupNewUrl(index, newUrl, true);
				PLS_INFO(s_chatModuleName, "chat page is webloaded");
			}
			PLS_INFO(s_chatModuleName, "force update chat %s page, chat index = %d", PLS_CHAT_HELPER->getPlatformNameFromIndex(index).toUtf8().constData(), index);
		}
	});

	this->installEventFilter(this);

	PLSChatHelper::sendWebChatFontSizeChanged(PLSChatHelper::getFontScaleSize());
}

PLSChatDialog::~PLSChatDialog()
{
	PLS_INFO(s_chatModuleName, __FUNCTION__);
	m_vecChatDatas.clear();
	pls_delete(ui, nullptr);
}

void PLSChatDialog::quitAppToReleaseCefData()
{
	PLS_INFO(s_chatModuleName, "PLSChat quit to release cef data");

	ui->titleWidget->setHidden(true);

	for (int i = 0; i < m_vecChatDatas.size(); i++) {
		channelRemoveToDeleteCef(i);
	}
	changedSelectIndex(ChatPlatformIndex::RTMP);
	for (int i = 0; i < m_vecChatDatas.size(); i++) {
		if (m_vecChatDatas[i].widget == nullptr) {
			continue;
		}
		m_vecChatDatas[i].button->setHidden(m_selectIndex != i);
	}
}

void PLSChatDialog::setupFirstUI()
{
	this->setWindowTitle(tr("chat.title"));
	ui->scrollArea->verticalScrollBar()->setDisabled(true);
	ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
	ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
	ui->titleWidget->setHidden(true);
	ui->titleWidget->setCursor(Qt::ArrowCursor); //PRISM/Zhangdewen/20200921/#/fix cursor show problem
	ui->scrollArea->setCursor(Qt::CursorShape::ArrowCursor);

	//PRISM/Zhangdewen/20200921/#/add chat source button
	m_chatSourceButtonOnePlatform = createChatSourceButton(ui->titleWidget, false);
	ui->horizontalLayoutTitleWidget->addWidget(m_chatSourceButtonOnePlatform);

	m_vecChatDatas.clear();
	QString addStyle = "";
	for (int i = 0; i <= ChatPlatformIndex::UnDefine; i++) {
		QWidget *widget = nullptr;

		if (ChatPlatformIndex::RTMP == i) {
			auto rtmpWidget = pls_new<QWidget>();
			rtmpWidget->setObjectName("rtmpWidget");
			rtmpWidget->setCursor(Qt::ArrowCursor); //PRISM/Zhangdewen/20200921/#/fix cursor show problem
			setupFirstRtmpUI(rtmpWidget);
			widget = rtmpWidget;
		}

		auto tapButton = pls_new<QPushButton>("", ui->scrollAreaWidgetContents);
		auto smallName = PLS_CHAT_HELPER->getString(i, true);
		QString objectName = QString(smallName).append("btn");
		tapButton->setObjectName(objectName);
		connect(tapButton, &QPushButton::clicked, [this, i]() {
			auto _index = i;
			PLS_UI_STEP(s_chatModuleName, QString("PLSChat Dialog Tab Click To Index %1").arg(PLS_CHAT_HELPER->getString(_index)).toUtf8().constData(), ACTION_CLICK);
			changedSelectIndex(_index, true);
		});
		ui->scrollAreaWidgetContents->layout()->addWidget(tapButton);
		tapButton->setHidden(true);
		string showUrl = PLS_CHAT_HELPER->getChatUrlWithIndex(i, QVariantMap());
		m_vecChatDatas.push_back({widget, tapButton, showUrl});

		if (i != ChatPlatformIndex::RTMP && i != ChatPlatformIndex::UnDefine) {
			tapButton->setStyleSheet(PLS_CHAT_HELPER->getTabButtonCss(objectName, smallName, PLS_CHAT_HELPER->getString(i)));
		} else {
			tapButton->setText(PLS_CHAT_HELPER->getString(i));
		}
	}
	m_rightHorizontalSpacer = pls_new<QSpacerItem>(0, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
	ui->scrollAreaWidgetContents->layout()->addItem(m_rightHorizontalSpacer);

	setSelectIndex(ChatPlatformIndex::UnDefine);

	QMetaObject::invokeMethod(this, [this]() { refreshUI(); }, Qt::QueuedConnection);
}

void PLSChatDialog::refreshUI()
{
	if (pls_is_app_exiting() || m_vecChatDatas.empty()) {
		PLS_INFO(s_chatModuleName, "refreshUI stoped, because mainview is closing");
		return;
	}

	if (PLS_PLATFORM_API->isPlatformExisted(PLSServiceType::ST_FACEBOOK)) {
		connect(PLS_PLATFORM_FACEBOOK, &PLSPlatformFacebook::privateChatChanged, this, &PLSChatDialog::facebookPrivateChatChanged,
			Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
	}

	if (PLS_PLATFORM_API->isPlatformExisted(PLSServiceType::ST_YOUTUBE)) {
		QObject::connect(PLS_PLATFORM_YOUTUBE, &PLSPlatformYoutube::selectIDChanged, this, &PLSChatDialog::refreshUI, Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
		QObject::connect(PLS_PLATFORM_YOUTUBE, &PLSPlatformYoutube::privateChangedToOther, this, &PLSChatDialog::youtubePrivateChange,
				 Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
	}
	updateRtmpPlaceText();
	if (!m_isForceRefresh && PLS_PLATFORM_API->isLiving()) {
		return;
	}

	m_isForceRefresh = false;

	refreshTabButtonCount();
	hideOrShowTabButton();
	updateTabPolicy();
	updateTopAddSourceText();
	for (int i = 0; i < m_vecChatDatas.size(); i++) {
		const ChatDatas &data = m_vecChatDatas[i];
		if (data.isWebLoaded && data.button->isHidden()) {
			channelRemoveToDeleteCef(i);
		}
	}
}

void PLSChatDialog::refreshTabButtonCount()
{
	for (auto &data : m_vecChatDatas) {
		data.button->setHidden(true);
	}

	for (const auto &info : PLSCHANNELS_API->getCurrentSelectedChannels()) {
		if (info.empty()) {
			continue;
		}

		int index = PLS_CHAT_HELPER->getIndexFromInfo(info);
		if (ChatPlatformIndex::UnDefine == index || ChatPlatformIndex::RTMP == index) {
			continue;
		}

		auto channelUserStatus = info.value(ChannelData::g_channelUserStatus, ChannelData::NotExist).toInt();
		updateNewUrlByIndex(index, info);
		bool willHidden = channelUserStatus != ChannelData::Enabled;
		m_vecChatDatas[index].button->setHidden(willHidden);
	}
}

void PLSChatDialog::hideOrShowTabButton()
{

	int platformCount = 0;
	bool isContainVlive = false;
	for (const auto &info : PLSCHANNELS_API->getCurrentSelectedChannels()) {
		if (info.empty()) {
			continue;
		}

		int index = PLS_CHAT_HELPER->getIndexFromInfo(info);
		if (index == ChatPlatformIndex::VLive) {
			isContainVlive = true;
		}
		if (ChatPlatformIndex::UnDefine == index || ChatPlatformIndex::RTMP == index) {
			continue;
		}

		platformCount++;
	}

	//PRISM/Zhangdewen/20200921/#/add chat source button && VLIVE not show
	m_chatSourceButtonOnePlatform->setVisible(platformCount == 1 && !isContainVlive);

	bool isOnlyRtmp = platformCount <= 0;
	//PRISM/Zhangdewen/20201021/#5310/switch delay
	//move code

	bool allWillShow = platformCount > 1;
	m_vecChatDatas[ChatPlatformIndex::All].button->setHidden(!allWillShow);

	if (isOnlyRtmp) {
		//if only rtmp or not channel selected, change to rtmp
		changedSelectIndex(ChatPlatformIndex::RTMP);
	} else if (allWillShow && !m_vecChatDatas[ChatPlatformIndex::All].isWebLoaded) {
		//all tab first to show, change to all
		changedSelectIndex(ChatPlatformIndex::All);
	} else if (m_vecChatDatas[m_selectIndex].button->isHidden()) {
		changedSelectIndex(foundFirstShowedButton());
	} else {
		changedSelectIndex(m_selectIndex);
	}

	//PRISM/Zhangdewen/20201021/#5310/switch delay
	ui->titleWidget->setHidden(isOnlyRtmp);
}

int PLSChatDialog::foundFirstShowedButton()
{
	int index = ChatPlatformIndex::All;
	for (int i = 0; i < m_vecChatDatas.size(); i++) {
		if (!m_vecChatDatas[i].button->isHidden()) {
			index = i;
			break;
		}
	}
	return index;
}

void PLSChatDialog::changedSelectIndex(int index, bool isClicked)
{
	bool isSameIndex = m_selectIndex == index;
	if (isSameIndex && isClicked) {
		//when isClicked == false, need force reload style sheet
		return;
	}
	setSelectIndex(index);
	int showButtonCount = getShownBtnCount();
	auto count = static_cast<int>(m_vecChatDatas.size());
	for (int i = 0; i < count; i++) {
		QPushButton *button = m_vecChatDatas[i].button;
		if (showButtonCount <= 1) {
			button->setProperty(s_ChatStatusType, s_ChatStatusOnlyOne);
		} else if (m_selectIndex == i) {
			button->setProperty(s_ChatStatusType, s_ChatStatusSelect);
		} else {
			button->setProperty(s_ChatStatusType, s_ChatStatusNormal);
		}

		button->style()->unpolish(button);
		button->style()->polish(button);
	}
	if (isSameIndex) {
		return;
	}
	PLS_INFO(s_chatModuleName, QString("PLSChat Dialog Tab Switch To Index %1").arg(PLS_CHAT_HELPER->getString(index)).toUtf8().constData());

	//PRISM/Zhangdewen/20201021/#5310/switch delay
	for (QObject *child : ui->stackedWidget->children()) {
		if (!child->isWidgetType()) {
			continue;
		}
		if (auto childWidget = dynamic_cast<QWidget *>(child); childWidget && !childWidget->isWindow()) {
			ui->stackedWidget->removeWidget(childWidget);
			childWidget->hide();
		}
	}

	auto _selIndex = m_selectIndex;
	if (this->m_vecChatDatas.empty()) {
		return;
	}
	auto &data = this->m_vecChatDatas[_selIndex];
	QPointer<QWidget> ce = data.widget;
	if (ce == nullptr && PLS_CHAT_HELPER->isCefWidgetIndex(_selIndex) && !data.isWebLoaded) {

		data.isWebLoaded = true;
		data.widget = createANewCefWidget(data.url, _selIndex);
		ce = data.widget;
	}
	if (ce == nullptr) {
		return;
	}
	switchStackWidget(_selIndex);
}

void PLSChatDialog::setupFirstRtmpUI(QWidget *parent)
{
	auto rtmpSpacerTop = pls_new<QSpacerItem>(40, 20, QSizePolicy::Expanding, QSizePolicy::Expanding);
	auto chatPlaceImageLabel = pls_new<QLabel>();
	chatPlaceImageLabel->setObjectName("chatPlaceImageLabel");
	chatPlaceImageLabel->setAlignment(Qt::AlignCenter);

	m_rtmpPlaceTextLabel = pls_new<QLabel>();
	m_rtmpPlaceTextLabel->setObjectName("rtmpPlaceTextLabel");
	m_rtmpPlaceTextLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
	m_rtmpPlaceTextLabel->setWordWrap(true);

	//PRISM/Zhangdewen/20200921/#/add chat source button
	//no channel select chat source button
	m_chatSourceButtonNoPlatform = createChatSourceButton(parent, true);

	m_fontChangeBtn = pls_new<QPushButton>(ui->titleWidget);
	m_fontChangeBtn->setObjectName("fontChangeBtn");
	m_fontChangeBtn->setProperty("isKR", IS_KR());
	m_fontChangeBtn->setMouseTracking(true);
	connect(m_fontChangeBtn, &QPushButton::clicked, this, &PLSChatDialog::fontZoomButtonClicked);
	ui->horizontalLayoutTitleWidget->addWidget(m_fontChangeBtn);
	m_fontChangeBtn->installEventFilter(this);
	updateRtmpPlaceText();

	auto rtmpSpacerBottom = pls_new<QSpacerItem>(40, 20, QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto rtmpVLayout = pls_new<QVBoxLayout>(parent);
	rtmpVLayout->setSpacing(0);
	rtmpVLayout->setContentsMargins(0, 0, 0, 0);
	rtmpVLayout->addItem(rtmpSpacerTop);
	rtmpVLayout->addWidget(chatPlaceImageLabel, 0, Qt::AlignCenter);
	rtmpVLayout->addWidget(m_rtmpPlaceTextLabel);
	rtmpVLayout->addWidget(m_chatSourceButtonNoPlatform, 0, Qt::AlignCenter);
	rtmpVLayout->addItem(rtmpSpacerBottom);
}

void PLSChatDialog::setupNewUrl(int index, const string &url, bool forceSet)
{
	if (m_vecChatDatas.empty() || index < 0 || index > m_vecChatDatas.size()) {
		return;
	}

	if (url == m_vecChatDatas[index].url && !forceSet) {
		return;
	}
	auto oldUrl = QString::fromStdString(m_vecChatDatas[index].url);
	m_vecChatDatas[index].url = url;
#if defined(Q_OS_WIN)
	auto ce = dynamic_cast<QCefWidget *>(m_vecChatDatas[index].widget.data());
#else
	auto ce = (QCefWidget *)m_vecChatDatas[index].widget.data();
#endif
	if (ce == nullptr) {
		return;
	}
	ce->setURL(url);

	//set url again, sometimes set new url, the cef will not refresh, so set it again.
	//the cef init will cause some time, when init not complected, set url will failed.
	QTimer::singleShot(100, ce, [ce, url] {
		PLS_INFO(s_chatModuleName, "PLSChat Dialog setupNewUrl with singleShot");
		ce->setURL(url);
	});
	PLS_INFO(s_chatModuleName, "PLSChat Dialog %s set new url", PLS_CHAT_HELPER->getString(index).toUtf8().constData());
	PLS_INFO_KR(s_chatModuleName, "PLSChat Dialog %s set new url: %s", PLS_CHAT_HELPER->getString(index).toUtf8().constData(), url.c_str());
}

int PLSChatDialog::rtmpChannelCount() const
{
	int count = 0;
	for (const auto &info : PLSCHANNELS_API->getCurrentSelectedChannels()) {
		if (info.empty()) {
			continue;
		}
		auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt();
		if (dataType >= ChannelData::CustomType) {
			count++;
		}
	}
	return count;
}

PLSQCefWidget *PLSChatDialog::createANewCefWidget(const string &url, int index)
{
	if (!PLS_CHAT_HELPER->isCefWidgetIndex(index)) {
		return nullptr;
	}
	if (plsCef == nullptr) {
		PLS_INFO(s_chatModuleName, "PLSChat Dialog %s create cef with new url, but failed, the cef is nullptr", PLS_CHAT_HELPER->getString(index).toUtf8().constData());
		return nullptr;
	}
	PLS_INFO(s_chatModuleName, "PLSChat Dialog %s create cef with new url", PLS_CHAT_HELPER->getString(index).toUtf8().constData());

	PLSQCefWidget *cefWidget = nullptr;

	auto name = PLS_CHAT_HELPER->getPlatformNameFromIndex(index);
	if (index == ChatPlatformIndex::All) {
		name = ALL_CHAT;
	}
	chat_panel_cookies = PLSBasic::getBrowserPannelCookieMgr(name);
	cefWidget = static_cast<PLSQCefWidget *>(plsCef->create_widget(nullptr, url, PLSChatHelper::getDispatchJS(index, QString::fromStdString(url)).toStdString(), chat_panel_cookies, {}, false, QColor(30, 30, 31), {}, true));

	if (PLS_CHAT_HELPER->isLocalHtmlPage(index)) {
		cefWidget->installEventFilter(this);
	}

	if (index == ChatPlatformIndex::Youtube) {
		QObject::connect(cefWidget, SIGNAL(urlChanged(const QString &)), this, SLOT(urlDidChanged(const QString &)));
		QObject::connect(cefWidget, SIGNAL(titleChanged(const QString &)), this, SLOT(titleChanged(const QString &)));
		QObject::connect(cefWidget, SIGNAL(loadEnded()), this, SLOT(loadEnded()));
	}
	if (index == ChatPlatformIndex::NCB2B || index == ChatPlatformIndex::All) {
		QObject::connect(cefWidget, SIGNAL(msgRecevied(const QString &, const QString &)), this, SLOT(recvLocalChatWebMsg(const QString &, const QString &)));
	}
	return cefWidget;
}

void PLSChatDialog::channelRemoveToDeleteCef(int index)
{
	ChatDatas &data = m_vecChatDatas[index];
	if (!pls_object_is_valid(data.widget)) {
		return;
	}
	PLS_INFO(s_chatModuleName, "PLSChat Dialog channelRemoveToDeleteCef index:%s", PLS_CHAT_HELPER->getString(index).toUtf8().constData());
	data.url = PLS_CHAT_HELPER->getChatUrlWithIndex(index, QVariantMap());
	auto ce = getCefWidgetByWidget(data);
	if (ce) {
		ce->closeBrowser();
	}
	data.isWebLoaded = false;
	ui->stackedWidget->removeWidget(data.widget);
	pls_delete(data.widget);
}

void PLSChatDialog::showEvent(QShowEvent *event)
{
	PLS_INFO(s_chatModuleName, "PLSChat Dialog showEvent");
	QWidget::showEvent(event);
	updateTabPolicy();

	App()->getMainView()->updateSideBarButtonStyle(ConfigId::ChatConfig, true);

	emit chatShowOrHide(true);

	if (m_vecChatDatas.size() > m_selectIndex) {
		auto ce = getCefWidgetByWidget(m_vecChatDatas[m_selectIndex]);
		if (ce && !ce->isHidden()) {
			forceResizeDialog();
		}
	}

	if (m_bShowToastAgain) {
		showToastIfNeeded();
	}

	auto basic = PLSBasic::instance();
	if (!basic) {
		return;
	}
	auto chatDock = basic->GetChatDock();
	if (!chatDock) {
		return;
	}
	chatDock->printChatGeometryLog("PLSChatDialog showEvent");
}

void PLSChatDialog::hideEvent(QHideEvent *event)
{
	PLS_INFO(s_chatModuleName, "PLSChat Dialog hideEvent");
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::ChatConfig, false);
	QWidget::hideEvent(event);
	emit chatShowOrHide(false);

	if (!m_pTestFrame.isNull()) {
		m_pTestFrame->hide();
		m_pTestFrame->deleteLater();
	}
}

void PLSChatDialog::closeEvent(QCloseEvent *event)
{
	PLS_INFO(s_chatModuleName, "PLSChat Dialog closeEvent");
	//can't emit close event, otherwise the cef browser will all deleted.
	hide();
	event->ignore();
}

void PLSChatDialog::updateRtmpPlaceText()
{
	if (m_rtmpPlaceTextLabel) {
		m_rtmpPlaceTextLabel->setText(PLS_CHAT_HELPER->getRtmpPlaceholderString());
	}
}

void PLSChatDialog::updateNewUrlByIndex(int index, const QVariantMap &info, bool forceSet)
{
	switch (index) {
	case ChatPlatformIndex::Twitch:
	case ChatPlatformIndex::Youtube:
	case ChatPlatformIndex::NCB2B:
	case ChatPlatformIndex::Chzzk:
	case ChatPlatformIndex::All:
		setupNewUrl(index, PLS_CHAT_HELPER->getChatUrlWithIndex(index, info), forceSet);
		break;
	default:
		break;
	}
}

bool PLSChatDialog::eventFilter(QObject *watched, QEvent *event)
{
	if (m_fontChangeBtn && watched == m_fontChangeBtn && (event->type() == QEvent::Leave)) {
		refreshStyle(m_fontChangeBtn);
	}
	return QWidget::eventFilter(watched, event);
}

void PLSChatDialog::wheelEvent(QWheelEvent *event)
{
	QPoint numDegrees = event->angleDelta() / 8;
	bool forwardStep = numDegrees.y() > 0;
	auto bar = ui->scrollArea->horizontalScrollBar();
	bar->setValue(bar->value() + (forwardStep ? -1 : 1) * 50);
	event->accept();
}

void PLSChatDialog::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	updateTabPolicy();
	adjustToastSize();
	updateTopAddSourceText();
}

void PLSChatDialog::adjustToastSize(int dialogWidth)
{
	if (m_pTestFrame.isNull()) {
		return;
	}
	if (dialogWidth == 0) {
		dialogWidth = this->size().width();
	}

	int spaceWidth = 10;
	int closeTop = 5;
	int closeLeft = 21;

	int frameWidth = dialogWidth - 2 * spaceWidth;
	int textWidth = frameWidth - 40;

	int topoffeet = ui->titleWidget->isVisible() ? ui->titleWidget->height() : 0;
	m_pTestFrame->move(QPoint{spaceWidth, spaceWidth + topoffeet});
	m_pTestFrame->show();

	QFontMetrics fontMetricsRight(m_pLabelToast->font());
	int textHeight = fontMetricsRight.boundingRect(QRect(0, 0, textWidth, 1000), Qt::AlignBottom | Qt::AlignLeft | Qt::TextWordWrap, m_pLabelToast->text()).height();
	textHeight = qMax(30, textHeight);
	auto frameSize = QSize(frameWidth, textHeight + 20);
	m_pTestFrame->resize(frameSize);
}

void PLSChatDialog::createToastWidget()
{
	if (m_pTestFrame.isNull()) {
		auto frame = pls_new<QFrame>(this);
		frame->setAttribute(Qt::WA_NativeWindow);
		frame->setObjectName("CheckInvalidTipFrame");
		auto lay = pls_new<QHBoxLayout>(frame);

		m_pLabelToast = pls_new<QLabel>(frame);
		QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		sizePolicy.setHorizontalStretch(0);
		sizePolicy.setVerticalStretch(0);
		sizePolicy.setHeightForWidth(m_pLabelToast->sizePolicy().hasHeightForWidth());
		m_pLabelToast->setSizePolicy(sizePolicy);
		m_pLabelToast->setAlignment(Qt::AlignCenter);
		m_pLabelToast->setWordWrap(true);
		m_pLabelToast->setObjectName("labelToast");

		lay->addWidget(m_pLabelToast);
		auto btn = pls_new<QPushButton>(frame);
		btn->setObjectName("pushButtonClear");

		QSizePolicy sizePolicy1(QSizePolicy::Fixed, QSizePolicy::Fixed);
		sizePolicy1.setHorizontalStretch(0);
		sizePolicy1.setVerticalStretch(0);
		sizePolicy1.setHeightForWidth(btn->sizePolicy().hasHeightForWidth());
		btn->setSizePolicy(sizePolicy1);

		lay->addWidget(btn);
		lay->setAlignment(btn, Qt::AlignTop);
		lay->setSpacing(2);
		lay->setContentsMargins(0, 0, 0, 0);

		auto deleteFrame = [frame]() {
			PLS_INFO(s_chatModuleName, "PLSChat Dialog delete deleteFrame maybe by singleShot");
			frame->hide();
			frame->deleteLater();
		};
		connect(btn, &QPushButton::clicked, frame, deleteFrame);

		m_pTestFrame = frame;
		adjustToastSize();
		frame->raise();
		frame->show();
		QTimer::singleShot(5000, frame, deleteFrame);
	}
}

void PLSChatDialog::showToastIfNeeded()
{
	if (!isVisible()) {
		m_bShowToastAgain = true;
		return;
	}

	m_bShowToastAgain = false;

	QString showStr("");
	if (!PLS_CHAT_HELPER->showToastWhenStart(showStr)) {
		return;
	}

	createToastWidget();
	m_pLabelToast->setText(showStr);
	adjustToastSize();
}

void PLSChatDialog::updateYoutubeUrlIfNeeded(bool forceSet)
{
	auto index = ChatPlatformIndex::Youtube;
	QVariantMap info;
	PLS_CHAT_HELPER->getSelectInfoFromIndex(index, info);
	if (!info.isEmpty()) {
		updateNewUrlByIndex(index, info, forceSet);
	}
}

void PLSChatDialog::setSelectIndex(int index)
{
	m_selectIndex = index;
	updateFontBtnStatus();
	if (!m_vecChatDatas[index].isWebLoaded) {
		return;
	}
	PLS_CHAT_HELPER->sendWebShownEventIfNeeded(m_selectIndex);
}

//PRISM/Zhangdewen/20200921/#/add chat source button
QWidget *PLSChatDialog::createChatSourceButton(QWidget *parent, bool noPlatform)
{
	if (noPlatform) {
		return pls_new<ChatSourceButton>(tr("Chat.SourceButton.NoPlatform"), parent, [this]() { chatSourceButtonClicked(); });
	}

	QWidget *widget = pls_new<QWidget>(parent);
	m_chatSourceBtn = pls_new<ChatSourceButton>(tr("Chat.SourceButton.OnePlatform"), widget, [this]() { chatSourceButtonClicked(); });
	QWidget *button = m_chatSourceBtn;
	QHBoxLayout *layout = pls_new<QHBoxLayout>(widget);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(button);
	return widget;
}

void PLSChatDialog::forceResizeDialog()
{
	QSize originSize = this->frameGeometry().size();
	auto newSize = QSize(originSize.width(), originSize.height() + 1);
	resize(newSize);
	resize(originSize);
}

int PLSChatDialog::getShownBtnCount()
{
	int showCount = 0;
	auto count = static_cast<int>(m_vecChatDatas.size());
	for (int i = 0; i < count; i++) {
		if (!m_vecChatDatas[i].button->isHidden()) {
			showCount++;
		}
	}
	return showCount;
}

void PLSChatDialog::updateTabPolicy()
{
	int shownCount = getShownBtnCount();
	auto mar = ui->horizontalLayoutTitleWidget->contentsMargins();
	int left = shownCount > 1 ? 5 : 10;
	mar.setLeft(left);
	ui->horizontalLayoutTitleWidget->setContentsMargins(mar);
}

void PLSChatDialog::updateTopAddSourceText()
{
	pls_async_call_mt(this, [this]() {
		pls_check_app_exiting();
		if (m_chatSourceBtn) {
			ChatSourceButton *cBtn = dynamic_cast<ChatSourceButton *>(m_chatSourceBtn.data());
			if (cBtn) {
				cBtn->updateTextWidth(qMax(m_rightHorizontalSpacer->geometry().width() - 10, 0));
			}
		}
	});
}

void PLSChatDialog::switchStackWidget(int index)
{
	//when switch to the cefwidget, which is the first loaded, in some computer maybe will delay a short time to show the cef page.
	//so when switch to other page, first to remove all sub view from superview.
	for (QObject *child : ui->stackedWidget->children()) {
		if (!child->isWidgetType()) {
			continue;
		}
		if (auto childWidget = dynamic_cast<QWidget *>(child); childWidget && !childWidget->isWindow()) {
			ui->stackedWidget->removeWidget(childWidget);
		}
	}
	QPointer<QWidget> widget = m_vecChatDatas[index].widget;
	if (widget == nullptr) {
		return;
	}
	ui->stackedWidget->addWidget(widget);
	ui->stackedWidget->setCurrentWidget(widget);
}

void PLSChatDialog::facebookPrivateChatChanged(bool oldPrivate, bool newPrivate)
{
	pls_check_app_exiting();
	if (oldPrivate == false && newPrivate == true) {
		createToastWidget();
		m_pLabelToast->setText(tr("facebook.living.chat.Private"));
		adjustToastSize();
	} else {
		if (!m_pLabelToast.isNull()) {
			m_pLabelToast->deleteLater();
		}
	}
}

//PRISM/Zhangdewen/20200921/#/add chat source button
void PLSChatDialog::chatSourceButtonClicked() const
{
	PLSBasic::Get()->AddSource(PRISM_CHATV2_SOURCE_ID);
}

void PLSChatDialog::fontZoomButtonClicked()
{
	auto showDialog = [this]() {
		PLS_INFO(s_chatModuleName, "PLSChat Dialog fontZoomButtonClicked singleShot");
		if (!m_fontChangeBtn)
			return;
		auto *dialog = pls_new<PLSChatFontZoomFrame>(m_fontChangeBtn, ui->stackedWidget);
		dialog->setAttribute(Qt::WA_DeleteOnClose, true);
		dialog->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);
		connect(this, &PLSChatDialog::fontBtnDisabled, dialog, &PLSChatFontZoomFrame::close);
		dialog->show();
	};
	QTimer::singleShot(200, this, showDialog);
}

void PLSChatDialog::updateFontBtnStatus()
{
	if (!m_fontChangeBtn)
		return;
	bool isRemote = PLS_CHAT_HELPER->isRemoteHtmlPage(m_selectIndex);
	if (getShownBtnCount() == 1 && isRemote) {
		m_fontChangeBtn->setHidden(true);
		emit fontBtnDisabled();
		return;
	}
	m_fontChangeBtn->setHidden(false);

	if (PLS_CHAT_HELPER->isLocalHtmlPage(m_selectIndex) && !isRemote) {
		m_fontChangeBtn->setEnabled(true);
		return;
	}

	m_fontChangeBtn->setEnabled(false);
	emit fontBtnDisabled();
}

void PLSChatDialog::urlDidChanged(const QString &url)
{
	QString redirectUrl = QString("https://www.youtube.com/watch?v=%1").arg(PLS_PLATFORM_YOUTUBE ? PLS_PLATFORM_YOUTUBE->getSelectID() : "");
	if (url.startsWith(redirectUrl)) {
		PLS_INFO(s_chatModuleName, "chat youtube login succeed, redirect to chat pop up url");
		updateYoutubeUrlIfNeeded(true);
	}
}

void PLSChatDialog::titleChanged(const QString &title)
{
	if (PLS_PLATFORM_YOUTUBE && !PLS_PLATFORM_YOUTUBE->isPrivateStatus() && title == "Oops" && m_bRefeshYoutubeChat) {
		m_bRefeshYoutubeChat = false;
		PLS_INFO(s_chatModuleName, "chat youtube will refreshed, because not private but title is oops");
		QTimer::singleShot(1500, this, [this]() {
			PLS_INFO(s_chatModuleName, "chat youtube will refreshed now");
			updateYoutubeUrlIfNeeded(true);
		});
	}
}
void PLSChatDialog::loadEnded()
{
	PLSQCefWidget *cefWidget = static_cast<PLSQCefWidget *>(sender());
	QString js = PLSChatHelper::getDispatchJS(ChatPlatformIndex::Youtube, QString::fromStdString(m_vecChatDatas[ChatPlatformIndex::Youtube].url));
	if (!js.isEmpty()) {
		cefWidget->executeJavaScript(js.toStdString());
	}
}

void PLSChatDialog::youtubePrivateChange()
{
	pls_check_app_exiting();
	m_bRefeshYoutubeChat = true;
	PLS_INFO(MODULE_PlatformService, "youtube signal privateChangedToOther emitteded");
	updateYoutubeUrlIfNeeded(true);
}

void PLSChatDialog::recvLocalChatWebMsg(const QString &type, const QString &msg)
{
	if (type == "open_browser") {
		QDesktopServices::openUrl(QUrl(msg));
		return;
	}
	if (type != "menu_show") {
		return;
	}

	m_chatActionSelect = false;

	QPointer<PLSQCefWidget> sendW = static_cast<PLSQCefWidget *>(sender());
	if (m_maskWidget && m_maskWidget->parentWidget() != sendW) {
		m_maskWidget->setParent(sendW);
	}
	if (!m_maskWidget) {
		m_maskWidget = new QFrame(sendW, Qt::Tool | Qt::FramelessWindowHint);
		m_maskWidget->setAttribute(Qt::WA_NativeWindow, true);
		m_maskWidget->setWindowOpacity(0.1);
	}
	m_maskWidget->setGeometry(QRect(sendW->mapToGlobal(QPoint{0, 0}), sendW->size()));
	m_maskWidget->setFixedSize(sendW->size());
	m_maskWidget->setVisible(true);

	QMenu chatWebMenu;
	PLSHideWatcher watcher(&chatWebMenu);
	QObject::connect(&watcher, &PLSHideWatcher::signalHide, &chatWebMenu, [sendW, &chatWebMenu, this]() {
		if (m_maskWidget) {
			m_maskWidget->setVisible(false);
		}

		pls_async_call_mt([sendW, this]() {
			PLS_INFO(UPDATE_MODULE, "PLSShowWatcher notify webMenu hide event");
			if (!m_chatActionSelect && sendW) {
				sendW->sendMsg(L"menu_handle", getSendWebData({{"id", ""}}));
			}
			m_chatActionSelect = false;
		});
	});
	QObject::connect(PLSUiApp::instance(), &PLSUiApp::appStateChanged, &chatWebMenu, [cwm = QPointer<QMenu>(&chatWebMenu), this](bool actived) {
		PLS_INFO(UPDATE_MODULE, "PLSChatDialog::recvLocalChatWebMsg state=%s", actived ? "active" : "inactive");
		if (m_maskWidget) {
			m_maskWidget->setVisible(false);
		}
		if (cwm) {
			cwm->close();
		}
	});

	auto dataArray = QJsonDocument::fromJson(msg.toUtf8()).array();

	for (auto data : dataArray) {
		auto jsobj = data.toObject();
		auto action = chatWebMenu.addAction(jsobj.value("label").toString(), [this, sendW, jsobj]() {
			m_chatActionSelect = true;
			if (sendW) {
				sendW->activateWindow();
				sendW->sendMsg(L"menu_handle", getSendWebData(jsobj));
			}
		});
		action->setEnabled(jsobj.value("enable").toBool());
	}

	chatWebMenu.exec(QCursor::pos());
}

QCefWidget *PLSChatDialog::getCefWidgetByWidget(const ChatDatas &data)
{
#if defined(Q_OS_WIN)
	auto ce = dynamic_cast<QCefWidget *>(data.widget.data());
	return ce;
#else
	if (!data.isWebLoaded) {
		return nullptr;
	}
	auto ce = static_cast<QCefWidget *>(data.widget.data());
	return ce;
#endif
}

void PLSChatDialog::updateTabBtnCss()
{
	for (int i = 0; i <= ChatPlatformIndex::UnDefine; i++) {

		auto smallName = PLS_CHAT_HELPER->getString(i, true);
		QString objectName = QString(smallName).append("btn");
		auto tabButton = m_vecChatDatas[i].button;
		if (!tabButton) {
			continue;
		}
		if (i != ChatPlatformIndex::RTMP && i != ChatPlatformIndex::UnDefine) {
			tabButton->setObjectName(objectName);
			tabButton->setStyleSheet(PLS_CHAT_HELPER->getTabButtonCss(objectName, smallName, PLS_CHAT_HELPER->getString(i)));
		}
	}
}

std::wstring PLSChatDialog::getSendWebData(const QJsonObject &data)
{
	QJsonObject sendParam;
	sendParam.insert("data", data);
	QJsonDocument doc;
	doc.setObject(sendParam);
	return QString::fromUtf8(doc.toJson()).toStdWString();
}
