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

#include "pls-common-define.hpp"

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

extern QCef *cef;

namespace {
//PRISM/Zhangdewen/20200921/#/add chat source button
class ChatSourceButton : public QFrame {
	bool hovered = false;
	bool pressed = false;
	std::function<void()> clicked;

public:
	ChatSourceButton(const QString &buttonText, QWidget *parent, std::function<void()> clicked_) : QFrame(parent), clicked(std::move(clicked_))
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

		QHBoxLayout *layout = pls_new<QHBoxLayout>(this);
		layout->setContentsMargins(10, 0, 10, 0);
		layout->setSpacing(5);
		layout->addWidget(icon);
		layout->addWidget(text);
	}
	~ChatSourceButton() override = default;

private:
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

	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::channelRemoved, this, [this]() { refreshUI(); }, Qt::QueuedConnection);

	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::platformActiveDone, this, [this]() { refreshUI(); }, Qt::QueuedConnection);
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::sigAllChannelRefreshDone, this, [this]() { refreshUI(); }, Qt::QueuedConnection);

	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveStarted, this,
		[this](bool isSucceed) {
			updateRtmpPlaceText();
			if (isSucceed) {
				showToastIfNeeded();
				updateYoutubeUrlIfNeeded();
			}
		},
		Qt::QueuedConnection);
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveEnded, this,
		[this]() {
			PLS_INFO(MODULE_PlatformService, "live end chat view refresh ui");
			refreshUI();
			m_bShowToastAgain = false;
		},
		Qt::QueuedConnection);

	this->installEventFilter(this);

	PLSChatHelper::sendWebChatFontSizeChanged(PLSChatHelper::getFontSacleSize());
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
			changedSelectIndex(_index);
		});
		ui->scrollAreaWidgetContents->layout()->addWidget(tapButton);
		tapButton->setHidden(true);
		string showUrl = PLS_CHAT_HELPER->getChatUrlWithIndex(i, QVariantMap());
		m_vecChatDatas.push_back({widget, tapButton, showUrl});

		if (i != ChatPlatformIndex::All && i != ChatPlatformIndex::RTMP && i != ChatPlatformIndex::UnDefine) {
			tapButton->setStyleSheet(PLS_CHAT_HELPER->getTabButtonCss(objectName, smallName));
		} else {
			tapButton->setText(PLS_CHAT_HELPER->getString(i));
		}
	}
	m_rightHorizontalSpacer = pls_new<QSpacerItem>(0, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
	ui->scrollAreaWidgetContents->layout()->addItem(m_rightHorizontalSpacer);

	setSelectIndex(ChatPlatformIndex::UnDefine);

	QMetaObject::invokeMethod(
		this, [this]() { refreshUI(); }, Qt::QueuedConnection);
}

void PLSChatDialog::refreshUI()
{
	if (pls_is_app_exiting() || m_vecChatDatas.empty()) {
		PLS_INFO(s_chatModuleName, "refreshUI stoped, because mainview is closing");
		return;
	}

	bool facebookExisted = PLS_PLATFORM_API->isPlatformExisted(PLSServiceType::ST_FACEBOOK);
	if (facebookExisted && !facebookPrivateConn) {
		facebookPrivateConn = connect(PLS_PLATFORM_FACEBOOK, &PLSPlatformFacebook::privateChatChanged, this, &PLSChatDialog::facebookPrivateChatChanged, Qt::QueuedConnection);
	}

	bool youtubeExisted = PLS_PLATFORM_API->isPlatformExisted(PLSServiceType::ST_YOUTUBE);
	if (youtubeExisted && !youtubeIDConn) {
		youtubeIDConn = connect(
			PLS_PLATFORM_YOUTUBE, &PLSPlatformYoutube::selectIDChanged, this, [this]() { refreshUI(); }, Qt::QueuedConnection);
		connect(
			PLS_PLATFORM_YOUTUBE, &PLSPlatformYoutube::kidsChangedToOther, this,
			[this]() {
				QTimer::singleShot(1000, PLS_PLATFORM_YOUTUBE, [this] {
					PLS_INFO(MODULE_PlatformService, "youtube signal kidsChangedToOther emited");
					updateYoutubeUrlIfNeeded(true);
				});
			},
			Qt::QueuedConnection);
	}
	if (youtubeExisted && !youtubePrivateConn) {
		youtubePrivateConn = connect(
			PLS_PLATFORM_YOUTUBE, &PLSPlatformYoutube::privateChangedToOther, this,
			[this]() {
				m_bRefeshYoutubeChat = true;
				PLS_INFO(MODULE_PlatformService, "youtube signal privateChangedToOther emited");
				updateYoutubeUrlIfNeeded(true);
			},
			Qt::QueuedConnection);
	}

	updateRtmpPlaceText();
	if (!m_isForceRefresh && PLS_PLATFORM_API->isLiving()) {
		return;
	}

	m_isForceRefresh = false;

	refreshTabButtonCount();
	hideOrShowTabButton();
	updateTabPolicy();
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

	int plaltformCount = 0;
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

		plaltformCount++;
	}

	//PRISM/Zhangdewen/20200921/#/add chat source button && VLIVE not show
	m_chatSourceButtonOnePlatform->setVisible(plaltformCount == 1 && !isContainVlive);

	bool isOnlyRtmp = plaltformCount <= 0;
	//PRISM/Zhangdewen/20201021/#5310/switch delay
	//move code

	bool allWillShow = plaltformCount > 1;
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

void PLSChatDialog::changedSelectIndex(int index)
{
	if (m_selectIndex == index) {
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
	QMetaObject::invokeMethod(
		this,
		[this, _selIndex]() {
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
		},
		Qt::QueuedConnection);
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
	QTimer::singleShot(100, ce, [ce, url] { ce->setURL(url); });
	PLS_INFO(s_chatModuleName, "PLSChat Dialog %s set new url", PLS_CHAT_HELPER->getString(index));
	PLS_INFO_KR(s_chatModuleName, "PLSChat Dialog %s set new url: %s", PLS_CHAT_HELPER->getString(index), url.c_str());
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

QCefWidget *PLSChatDialog::createANewCefWidget(const string &url, int index)
{
	if (!PLS_CHAT_HELPER->isCefWidgetIndex(index)) {
		return nullptr;
	}
	if (cef == nullptr) {
		PLS_INFO(s_chatModuleName, "PLSChat Dialog %s create cef with new url, but failed, the cef is nullptr", PLS_CHAT_HELPER->getString(index));
		return nullptr;
	}
	PLS_INFO(s_chatModuleName, "PLSChat Dialog %s create cef with new url", PLS_CHAT_HELPER->getString(index));

	QCefWidget *cefWidget = nullptr;

	auto name = PLS_CHAT_HELPER->getPlatformNameFromIndex(index);
	if (index == ChatPlatformIndex::All) {
		name = NAVER_TV;
	}
	chat_panel_cookies = PLSBasic::getBrowserPannelCookieMgr(name);
	cefWidget = cef->create_widget(nullptr, url, PLSChatHelper::getDispatchJS(index).toStdString(), chat_panel_cookies, {}, false, QColor(30, 30, 31), {}, true);

	if (PLS_CHAT_HELPER->isLocalHtmlPage(index)) {
		cefWidget->installEventFilter(this);
	}

	if (index == ChatPlatformIndex::Youtube) {
		QObject::connect(cefWidget, SIGNAL(urlChanged(const QString &)), this, SLOT(urlDidChanged(const QString &)));
		QObject::connect(cefWidget, SIGNAL(titleChanged(const QString &)), this, SLOT(titleChanged(const QString &)));
	}
	return cefWidget;
}

void PLSChatDialog::channelRemoveToDeleteCef(int index)
{
	ChatDatas &data = m_vecChatDatas[index];
	if (!pls_object_is_valid(data.widget)) {
		return;
	}

	data.url = PLS_CHAT_HELPER->getChatUrlWithIndex(index, QVariantMap());
	data.isWebLoaded = false;

#if defined(Q_OS_WIN)
	auto ce = dynamic_cast<QCefWidget *>(data.widget.data());
#else
	auto ce = static_cast<QCefWidget *>(data.widget.data());
#endif
	if (ce != nullptr) {
		ce->closeBrowser();
	}

	ui->stackedWidget->removeWidget(data.widget);
	data.widget = nullptr;
	pls_delete(data.widget);
	PLS_INFO(s_chatModuleName, "PLSChat Dialog channelRemoveToDeleteCef index:%s", PLS_CHAT_HELPER->getString(index));
}

void PLSChatDialog::showEvent(QShowEvent *event)
{
	PLS_INFO(s_chatModuleName, "PLSChat Dialog showEvent");
	QWidget::showEvent(event);
	updateTabPolicy();

	App()->getMainView()->updateSideBarButtonStyle(ConfigId::ChatConfig, true);

	emit chatShowOrHide(true);

	if (m_vecChatDatas.size() > m_selectIndex) {
#if defined(Q_OS_WIN)
		auto ce = dynamic_cast<QCefWidget *>(m_vecChatDatas[m_selectIndex].widget.data());
#else
		auto ce = static_cast<QCefWidget *>(m_vecChatDatas[m_selectIndex].widget.data());
#endif
		if (ce == nullptr || ce->isHidden()) {
			return;
		}
		forceResizeDialog();
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
	m_rtmpPlaceTextLabel->setText(PLS_CHAT_HELPER->getRtmpPlaceholderString());
//PRISM/Zhangdewen/20200921/#/add chat source button
#if 0
	m_chatSourceButtonNoPlatform->setVisible(PLSCHANNELS_API->getCurrentSelectedChannels().isEmpty());
#endif
}

void PLSChatDialog::updateNewUrlByIndex(int index, const QVariantMap &info, bool forceSet)
{
	switch (index) {
	case ChatPlatformIndex::Twitch:
	case ChatPlatformIndex::Youtube:
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
	bool forwartStep = numDegrees.y() > 0;
	auto bar = ui->scrollArea->horizontalScrollBar();
	bar->setValue(bar->value() + (forwartStep ? -1 : 1) * 50);
	event->accept();
}

void PLSChatDialog::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	updateTabPolicy();
	adjustToastSize();
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

	int framWidth = dialogWidth - 2 * spaceWidth;
	int textWidth = framWidth - 40;

	int topoffeet = ui->titleWidget->isVisible() ? ui->titleWidget->height() : 0;
	m_pTestFrame->move(QPoint{spaceWidth, spaceWidth + topoffeet});
	m_pTestFrame->show();

	QFontMetrics fontMetricsRight(m_pLabelToast->font());
	int textHeight = fontMetricsRight.boundingRect(QRect(0, 0, textWidth, 1000), Qt::AlignBottom | Qt::AlignLeft | Qt::TextWordWrap, m_pLabelToast->text()).height();
	textHeight = qMax(30, textHeight);
	auto framSize = QSize(framWidth, textHeight + 20);
	m_pTestFrame->resize(framSize);
}

void PLSChatDialog::createToasWidget()
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

	createToasWidget();
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
	QWidget *button = pls_new<ChatSourceButton>(tr("Chat.SourceButton.OnePlatform"), widget, [this]() { chatSourceButtonClicked(); });
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
	int errorShowncount = 6;
	int onlyOnePixMinWidth = 350;

	int space = 10;
	if (shownCount >= errorShowncount && this->frameGeometry().size().width() < onlyOnePixMinWidth) {
		space = 1;
	}
	ui->scrollAreaWidgetContents->layout()->setSpacing(space);

	auto mar = ui->horizontalLayoutTitleWidget->contentsMargins();
	int left = shownCount > 1 ? 5 : 10;
	mar.setLeft(left);
	ui->horizontalLayoutTitleWidget->setContentsMargins(mar);
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
	if (oldPrivate == false && newPrivate == true) {
		createToasWidget();
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
	PLSBasic::Get()->AddSource(PRISM_CHAT_SOURCE_ID);
}

void PLSChatDialog::fontZoomButtonClicked()
{
	auto showDialog = [this]() {
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

	bool isT_Y = ChatPlatformIndex::Twitch == m_selectIndex || ChatPlatformIndex::Youtube == m_selectIndex;
	if (getShownBtnCount() == 1 && isT_Y) {
		m_fontChangeBtn->setHidden(true);
		emit fontBtnDisabled();
		return;
	}
	m_fontChangeBtn->setHidden(false);

	if (PLS_CHAT_HELPER->isLocalHtmlPage(m_selectIndex) && !isT_Y) {
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
		PLS_INFO(s_chatModuleName, "chat youtube login succed, redirect to chat pop up url");
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
