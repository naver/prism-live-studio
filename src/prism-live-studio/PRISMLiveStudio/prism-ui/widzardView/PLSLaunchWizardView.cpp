#include "PLSLaunchWizardView.h"
#include "ui_PLSLaunchWizardView.h"
#include "utils-api.h"
#include "PLSWizardInfoView.h"
#include <qdesktopservices.h>
#include <QUrl>
#include <qscrollbar.h>
#include "PLSBasic.h"
#include "PLSResourceManager.h"
#include "PLSIPCHandler.h"
#include "pls-shared-functions.h"
#include "pls-channel-const.h"
#include "pls-shared-values.h"
#include <algorithm>
#include <random>
#include "PLSResCommonFuns.h"
#include "pls-common-define.hpp"
#include "frontend-api.h"
#include "PLSChannelDataAPI.h"
#include <qpicture.h>
#include <qpainterpath.h>
#include "./log/log.h"
#include "ChannelCommonFunctions.h"
#include <qscopeguard.h>
#include "network-state.h"
#include <qscroller.h>
#include "window-basic-main.hpp"

constexpr auto urlProperty = "url";
constexpr auto discordUrl = "https://discord.gg/vxzDZ9V6f9";
constexpr auto discordUrlKR = "https://discord.gg/9j7mFY5g9a";
constexpr auto blogUrl = "https://medium.com/prismlivestudio";
constexpr auto blogUrlKR = "https://blog.naver.com/prismlivestudio";
constexpr auto defaultBannenImage = ":/resource/images/wizard/img-banner-default.svg";

#define NoScheduleStr tr("WizardView.NoSchedule")
#define ScheduleError tr("WizardView.ScheduleError")
#define TipText tr("WizardView.Restart.OnError")
#define QueText tr("WizardView.Quetion")
#define DiscordText tr("WizardView.Discord")
#define BlogText tr("WizardView.Blog")
constexpr auto ModuleName = "WizardView";
constexpr int MAXUPDATACOUNT = 3;

QPointer<PLSLaunchWizardView> PLSLaunchWizardView::g_wizardView = nullptr;
PLSLaunchWizardView *PLSLaunchWizardView::instance()
{
	if (g_wizardView == nullptr) {
		g_wizardView = new PLSLaunchWizardView();
		connect(g_wizardView, &PLSLaunchWizardView::sigTryGetScheduleList, PLSCHANNELS_API, &PLSChannelDataAPI::startUpdateScheduleList, Qt::QueuedConnection);
		connect(PLSBasic::Get(), &PLSBasic::mainClosing, g_wizardView, &PLSLaunchWizardView::close, Qt::DirectConnection);
	}
	return g_wizardView;
}

PLSLaunchWizardView::PLSLaunchWizardView(QWidget *parent) : PLSDialogView(parent)
{
	g_wizardView = this;
	ui = pls_new<Ui::PLSLaunchWizardView>();
	setupUi(ui);
	ui->banerPointLayout->setAlignment(Qt::AlignCenter);

	this->setHasMinButton(true);

	pls_add_css(this, {"PLSLaunchWizardView"});

	createBannerScrollView();
	createAlertInfoView();
	createLiveInfoView();
	createBlogView();
	createQueView();

	connect(ui->leftButton, &QPushButton::clicked, this, &PLSLaunchWizardView::changeBannerView);
	connect(ui->rightButton, &QPushButton::clicked, this, &PLSLaunchWizardView::changeBannerView);
	ui->wizardScrollArea->verticalScrollBar()->setObjectName("wizardVetialBar");
	connect(ui->hideButton, &QPushButton::clicked, this, &PLSLaunchWizardView::hideView);
	connect(this, &PLSLaunchWizardView::mouseClicked, this, &PLSLaunchWizardView::onWidgetClicked, Qt::QueuedConnection);

	updatebannerView();

	connect(PLSResourceManager::instance(), &PLSResourceManager::bannerJsonDownloaded, this, &PLSLaunchWizardView::loadBannerSources, Qt::QueuedConnection);
	connect(this, &PLSLaunchWizardView::bannerImageLoadFinished, this, &PLSLaunchWizardView::finishedDownloadBanner, Qt::QueuedConnection);
	startUpdateBanner();
	ui->bannerFrame->setMouseTracking(true);
	ui->bannerFrame->installEventFilter(this);
	ui->stackedWidget->setMouseTracking(true);
	ui->stackedWidget->installEventFilter(this);

	this->installEventFilter(this);

	updateTipsHtml();
	this->setCurrentIndex(this->currentIndex(), false);

	setFixedWidth(648);
	setMinimumHeight(510);
	setWidthResizeEnabled(false);
	addMacTopMargin();

#if defined(Q_OS_MACOS)
	this->setWindowTitle(QTStr("Basic.Main.Wizard"));
#endif
}

PLSLaunchWizardView::~PLSLaunchWizardView()
{
	clearDumpInfos();
	pls_delete(ui);
}

void PLSLaunchWizardView::updateParent(QWidget *wid)
{
	auto flg = this->windowFlags();
	this->setParent(wid, flg);
}

void PLSLaunchWizardView::startUpdateBanner()
{
	m_UpdateCount++;
	if (m_UpdateCount >= MAXUPDATACOUNT) {
		return;
	}
	if (mBannerTimer == nullptr) {
		mBannerTimer = QSharedPointer<QTimer>::create(this);
		mBannerTimer->setInterval(10 * 1000);

		connect(mBannerTimer.data(), &QTimer::timeout, this, &PLSLaunchWizardView::startUpdateBanner, Qt::QueuedConnection);
		connect(this, &PLSLaunchWizardView::loadBannerFailed, this, &PLSLaunchWizardView::startUpdateBanner, Qt::QueuedConnection);
		connect(
			pls::NetworkState::instance(), &pls::NetworkState::stateChanged, this,
			[this]() {
				if (pls::NetworkState::instance()->isAvailable()) {
					this->startUpdateBanner();
				}
			},
			Qt::QueuedConnection);
	}

	if (isLoadBannerSuccess) {
		mBannerTimer->stop();
		return;
	}
	if (isLoadingBanner) {
		static int loadingCount = 0;
		++loadingCount;
		if (loadingCount >= 10) {
			isLoadingBanner = false;
		}
		return;
	}

	isLoadingBanner = true;
	PLSResourceManager::instance()->getBannerJson();
	mBannerTimer->start();
}

void PLSLaunchWizardView::updatebannerView()
{
	QStringList images = mBannerUrls.values();
	if (images.isEmpty()) {
		images.append(QString(defaultBannenImage));
	}
	std::default_random_engine engine((int)QDateTime::currentSecsSinceEpoch());
	std::shuffle(images.begin(), images.end(), engine);
	initBannerView(images);
	delayCheckButtonsState();
}

void PLSLaunchWizardView::initBannerView(const QStringList &paths)
{
	if (!paths.isEmpty()) {
		auto old = scrollLay->count();

		for (int index = 0; index < paths.count(); ++index) {
			auto path = paths[index];
			if (old > 0) {
				updateBannerPath(index, path);
				--old;
				continue;
			}
			createBanner(path);
		}
		return;
	}
}

bool PLSLaunchWizardView::eventFilter(QObject *watched, QEvent *event)
{
	switch (event->type()) {
	case QEvent::MouseButtonPress:
		emit mouseClicked(watched);
		qDebug() << " clicked obj " << watched << " win " << dynamic_cast<QWidget *>(watched)->isWindow();

		break;
	case QEvent::FocusOut:
		break;
	case QEvent::MouseMove:
	case QEvent::HoverEnter:
	case QEvent::HoverMove:
	case QEvent::HoverLeave:
		delayCheckButtonsState();
		break;
	case QEvent::Wheel:
		if (dynamic_cast<QWidget *>(watched)->property("index").toInt() == currentIndex()) {

			QPoint numPixels = dynamic_cast<QWheelEvent *>(event)->pixelDelta();
			bool forwartStep = false;
			if (!numPixels.isNull()) {
				forwartStep = numPixels.x() < 0;
				wheelChangeBannerView(forwartStep);
			}
		}
		return true;
	default:
		break;
	}
	return PLSDialogView::eventFilter(watched, event);
}

void PLSLaunchWizardView::loadBannerSources()
{

	if (isParsaring) {
		return;
	}
	isParsaring = true;
	mBannerUrls.clear();
	mLinks.clear();
	auto cleanup = qScopeGuard([this]() {
		this->isParsaring = false;
		if (this->mBannerUrls.isEmpty()) {
			this->isLoadingBanner = false;
			PLS_INFO(ModuleName, "empty for json loaded");
		}
		if (this->isLoadingBanner || this->isLoadBannerSuccess) {
			return;
		}
		emit loadBannerFailed();
		return;
	});

	auto path = PLSResCommonFuns::getUserSubPath(common::PLS_BANNANR_JSON);
	if (!QFile::exists(path)) {
		PLS_INFO(ModuleName, "bannar json not exists!");
		return;
	}
	auto data = PLSResCommonFuns::readFile(path);
	if (data.isEmpty()) {
		return;
	}
	auto jsonObj = QJsonDocument::fromJson(data).object().value("launcherBanner").toObject().value("bannerList").toObject();
	QString key = pls_get_current_country_short_str().toUpper();
	auto array = jsonObj.value(key).toArray();
	if (array.isEmpty()) {
		if (key == "US") {
			return;
		}
		key = "US";
		array = jsonObj.value(key).toArray();
		if (array.isEmpty()) {
			return;
		}
	}

	auto cachePath = PLSResCommonFuns::getUserSubPath(common::PLS_BANNANR_PATH);
	int index = 0;
	for (const auto &obj : array) {
		auto jobj = obj.toObject();
		auto url = jobj.value("bannerUrl").toString();
		if (url.isEmpty()) {
			continue;
		}
		QUrl urlO(url);
		++index;
		auto strIndex = QString::number(index) + "_";
		auto filePath = cachePath + "/" + strIndex + urlO.fileName();
		mBannerUrls.insert(url, filePath);
		mLinks.insert(filePath, jobj.value("bannerClickLinkUrl").toString());
	}
	QPointer context(this);
	auto callback = [this, context](const QMap<QString, bool> &resDownloadStatus, PLSResEvents) {
		if (context == nullptr) {
			return;
		}
		emit bannerImageLoadFinished(resDownloadStatus);
	};
	if (mBannerUrls.isEmpty()) {
		return;
	}
	auto tmp = mBannerUrls; //copy
	PLSResCommonFuns::downloadResources(tmp, callback);
}

void PLSLaunchWizardView::finishedDownloadBanner(const QMap<QString, bool> &resDownloadStatus)
{

	int errorCount = 0;
	for (auto it = resDownloadStatus.begin(); it != resDownloadStatus.end(); ++it) {
		if (!*it) {
			this->mBannerUrls.remove(it.key());
			++errorCount;
			continue;
		}
	}

	isLoadBannerSuccess = errorCount <= 0;
	isLoadingBanner = false;
	updatebannerView();
	setCurrentIndex(0, false);
	scrollArea->horizontalScrollBar()->setValue(0);
}

void PLSLaunchWizardView::createLiveInfoView()
{
	if (!m_liveInfoView) {
		m_liveInfoView = pls_new<PLSWizardInfoView>(PLSWizardInfoView::ViewType::LiveInfo, this);
		ui->infoLayout->addWidget(m_liveInfoView, 1, 0, 1, 2);

		m_liveInfoView->installEventFilter(this);

		auto refreshBtn = new QPushButton(m_liveInfoView);
		refreshBtn->setObjectName("refreshBtn");
		refreshBtn->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
		auto lay = m_liveInfoView->layout();
		lay->addWidget(refreshBtn);
		lay->setAlignment(refreshBtn, Qt::AlignRight | Qt::AlignVCenter);
		connect(refreshBtn, &QPushButton::clicked, this, &PLSLaunchWizardView::scheduleClicked, Qt::QueuedConnection);

		updateInfoView(NoScheduleStr);
	}
}

void PLSLaunchWizardView::createAlertInfoView()
{
	if (!m_alertInfoView) {
		m_alertInfoView = pls_new<PLSWizardInfoView>(PLSWizardInfoView::ViewType::Alert, this);
		m_alertInfoView->setObjectName("alertView");
		ui->infoLayout->addWidget(m_alertInfoView, 0, 0, 1, 2);
		m_alertInfoView->hide();
	}

	onDumpCreated();
}

QFileInfoList getDumpInfoList()
{
	QDir dir(PLSResCommonFuns::getUserSubPath("crashDump"));
	dir.setNameFilters({"dumpInfo*"});
	dir.setSorting(QDir::Time);
	return dir.entryInfoList(QDir::QDir::NoDotAndDotDot | QDir::Files);
}

QString getLatestDumpInfoFile()
{
	QString infoFile;

	if (auto files = getDumpInfoList(); !files.isEmpty()) {
		infoFile = files.first().absoluteFilePath();
	}
	return infoFile;
}

//dump creat at prism directory PRISMLiveStudio\crashDump ,file name is dumpInfo.......json
//json as bellow
// "context": "The program crashed due to an error. Visit the official PRISM blog for troubleshooting details."
//"location": "prism-spectralizer.dll + 0x1abee"
//"timestamp": "1649916854969"
void PLSLaunchWizardView::onDumpCreated()
{
	QJsonObject json;

	if (QString lastInfoFile = getLatestDumpInfoFile(); !lastInfoFile.isEmpty()) {
		auto data = PLSResCommonFuns::readFile(lastInfoFile);
		if (!data.isEmpty()) {
			auto doc = QJsonDocument::fromJson(data);
			json = doc.object();
		}
	}

	handleErrorMessgage(json);
}
void PLSLaunchWizardView::wheelChangeBannerView(bool bPre)
{
	auto size = scrollLay->count();
	if (size <= 0) {
		return;
	}

	int currentIndex = this->currentIndex();
	if (bPre) {
		--currentIndex;
	} else {
		++currentIndex;
	}

	if (currentIndex < 0) {
		currentIndex = size - 1;
	} else if (currentIndex >= size) {
		currentIndex = 0;
	}

	moveWidget(currentIndex, bPre);
	this->setCurrentIndex(currentIndex);
}
void PLSLaunchWizardView::clearDumpInfos() const
{

	auto infos = getDumpInfoList();
	for (const auto &info : infos) {
		QFile::remove(info.absoluteFilePath());
	}
}

void PLSLaunchWizardView::onWidgetClicked(QObject *obj) const
{

	auto wid = dynamic_cast<QLabel *>(obj);
	if (wid == nullptr) {
		return;
	}

	auto link = wid->property("url").toString();
	if (link.isEmpty()) {
		return;
	}
	QDesktopServices::openUrl(QUrl(link));
	qDebug() << " index clicked " << wid->property("index");
}

void PLSLaunchWizardView::updateTipsHtml()
{
#if defined(Q_OS_WIN)
	QString srxTxt = tr("WizardView.Restart.OnError");
#else
	QString srxTxt = tr("WizardView.Restart.Mac.OnError");
#endif
	QString g_tipTextHtml = "<p style=\"line-height:1.2;\">%1</p>";
	ui->tipsLabel->setText(g_tipTextHtml.arg(srxTxt));
}

void PLSLaunchWizardView::checkStackOrder() const
{
	if (!pls_has_modal_view()) {
		return;
	}
	PLS_INFO(ModuleName, "check stack");
	pls_get_main_view()->raise();
	pls_get_main_view()->activateWindow();
}

void PLSLaunchWizardView::createBlogView()
{
	auto blogView = pls_new<PLSWizardInfoView>(PLSWizardInfoView::ViewType::Blog, this);
	blogView->setObjectName("buttonInfoView");
	ui->infoLayout->addWidget(blogView, 2, 0, 1, 1);
	blogView->setInfoText(BlogText);

	auto url = IS_KR() ? blogUrlKR : blogUrl;
	blogView->setProperty(urlProperty, url);
	connect(blogView, &QPushButton::clicked, this, &PLSLaunchWizardView::onUrlButtonClicked);
}

void PLSLaunchWizardView::createQueView()
{
	auto queView = pls_new<PLSWizardInfoView>(PLSWizardInfoView::ViewType::Que, this);
	queView->setObjectName("buttonInfoView");
	ui->infoLayout->addWidget(queView, 2, 1, 1, 1);
	queView->setInfoText(DiscordText);
	auto url = IS_KR() ? discordUrlKR : discordUrl;
	queView->setProperty(urlProperty, url);
	connect(queView, &QPushButton::clicked, this, &PLSLaunchWizardView::onUrlButtonClicked);
}

void PLSLaunchWizardView::createBanner(const QString &path, int index)
{
	auto label = pls_new<QLabel>(scrollArea);
	label->setObjectName("bannerContentLabel");
	label->setScaledContents(true);
	label->setAlignment(Qt::AlignCenter);
	label->setWordWrap(true);
	label->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
	label->setOpenExternalLinks(true);
	label->setMouseTracking(true);
	label->installEventFilter(this);

	scrollLay->insertWidget(index, label);
	auto bannerPointer = pls_new<QPushButton>();
	bannerPointer->setObjectName("wizardBannerPointer");
	m_bannerBtnGroup.addButton(bannerPointer, (int)m_bannerBtnGroup.buttons().size());
	ui->banerPointLayout->addWidget(bannerPointer, 0, Qt::AlignCenter);

	int retIndex = scrollLay->indexOf(label);
	updateBannerPath(retIndex, path);
}

void PLSLaunchWizardView::updateBannerPath(int index, const QString &path) const
{
	auto item = scrollLay->itemAt(index);
	if (item == nullptr) {
		return;
	}
	auto label = dynamic_cast<QLabel *>(item->widget());
	if (label == nullptr) {
		return;
	}
	label->setStyleSheet(QString("image:url(%1);").arg(path));
	auto link = mLinks.value(path);
	if (link.isEmpty()) {
		return;
	}
	label->setCursor(QCursor(Qt::PointingHandCursor));
	label->setProperty("url", link);
	label->setProperty("index", index);
}

bool PLSLaunchWizardView::isInWidgets(const QWidget *wid) const
{
	auto pos = wid->mapFromGlobal(QCursor::pos());
	auto rect = wid->contentsRect();

	return rect.contains(pos);
}

const QString currentDayHtml = R"(<p style="color:#EFFC35;">%1</p>)";
const QString otherDayHtml = R"(<p style="color:#BABABA;">%1</p>)";
extern QVariantMap createErrorMap(int errorType);
extern void addErrorForType(int errorType);
extern void showNetworkErrorAlert();

void PLSLaunchWizardView::checkShowErrorAlert()
{
	//ux error for refreshing not by click   show on prism
	if (!isRefresh) {
		return;
	}
	if (!this->isVisible()) {
		return;
	}
	//error for clicking refresh button on lancher
	auto errorMap = createErrorMap(ChannelData::NetWorkErrorType::NetWorkNoStable);
	if (getInfo<bool>(errorMap, ChannelData::g_errorIsErrMsg, false)) {
		pls_alert_error_message(this, getInfo(errorMap, ChannelData::g_errorTitle), getInfo(errorMap, ChannelData::g_errorString));
	} else {
		PLSAlertView::warning(this, getInfo(errorMap, ChannelData::g_errorTitle), getInfo(errorMap, ChannelData::g_errorString));
	}
	isRefresh = false;
}

void PLSLaunchWizardView::handleChannelMessage(const QJsonObject &body)
{
	m_scheduleInfo = body;
	QString title;
	QString TimeStr;
	QString platformName;

	if (body.contains(ChannelData::g_errorString)) {
		title = ScheduleError;
		checkShowErrorAlert();
	} else if (body.contains(ChannelData::g_platformName)) {
		auto time = body.value(ChannelData::g_timeStamp).toVariant().toLongLong();
		auto timeObj = QDateTime::fromSecsSinceEpoch(time);
		QString html;
		if (timeObj.daysTo(QDateTime::currentDateTime()) == 0) {
			html = currentDayHtml;
		} else {
			html = otherDayHtml;
		}
		TimeStr = formatTimeStr(timeObj);
		TimeStr = html.arg(TimeStr);
		title = body.value(ChannelData::g_nickName).toString();
		platformName = body.value(ChannelData::g_platformName).toString();
	} else {
		title = NoScheduleStr;
	}

	updateInfoView(title, TimeStr, platformName);
}

extern QString getElidedText(const QWidget *widget, const QString &srcTxt, int minWidth, Qt::TextElideMode mode, int flag);
extern QString translatePlatformName(const QString &platformName);
void PLSLaunchWizardView::updateInfoView(const QString &title, const QString &timeStr, const QString &plaform) const
{

	auto elidTex = getElidedText(m_liveInfoView, title, 436, Qt::ElideRight);
	m_liveInfoView->setInfo(elidTex, timeStr, translatePlatformName(plaform));
}

void PLSLaunchWizardView::onPrismMessageCome(const QVariantHash &params)
{
	int type = params.value("type").toInt();
	auto msgBody = params.value("msg").toJsonObject();
	switch (MessageType(type)) {
	case MessageType::ChannelsModuleMsg:
		handleChannelMessage(msgBody);
		break;
	case MessageType::AppState:
		handlePrismState(msgBody);
		break;
	case MessageType::ErrorInfomation:
		handleErrorMessgage(msgBody);
		break;
	default:
		break;
	}
}

void PLSLaunchWizardView::handlePrismState(const QJsonObject &body) const
{
	auto state = body.value(ChannelData::g_prismState).toInt();
	m_liveInfoView->loading(state == int(ChannelData::PrismState::Bussy));
}

#ifdef _DEBUG
const int disapearTime = 5 * 1000;
#else
const int disapearTime = 60 * 1000;
#endif // DEBUG

void PLSLaunchWizardView::handleErrorMessgage(const QJsonObject &body)
{
	auto location = body.value(shared_values::errorTitle).toString();
	auto content = body.value(shared_values::errorContent).toString();

	auto timeI = body.value(shared_values::errorTime).toVariant().toLongLong();
	auto time = QDateTime::fromMSecsSinceEpoch(timeI);

	m_alertInfoView->setInfoText(location + "\n" + content);
	m_alertInfoView->setTipText(formatTimeStr(time));
	if (!content.isEmpty()) {
		setErrorViewVisible(true);
		QTimer::singleShot(disapearTime, this, [this]() { setErrorViewVisible(false); });
	} else {
		setErrorViewVisible(false);
		PLS_INFO(ModuleName, "dump info file not found");
	}
	clearDumpInfos();
}

void PLSLaunchWizardView::setErrorViewVisible(bool visible)
{
	m_alertInfoView->setVisible(visible);
	if (visible) {
		setProperty("type", "alert");
	} else {
		setProperty("type", "normal");
	}
	pls_flush_style(this);
}
QString getSupportLanguage()
{
	return PLSBasic::instance()->getSupportLanguage();
}
void PLSLaunchWizardView::onUrlButtonClicked() const
{
	auto btn = sender();
	auto url = btn->property("url").toString().arg(getSupportLanguage());
	QDesktopServices::openUrl(QUrl(url));
}

void PLSLaunchWizardView::scheduleClicked()
{
	PLS_UI_STEP("widzard", "schedule refresh", "clicked");
	isRefresh = true;
	emit sigTryGetScheduleList();
}

int PLSLaunchWizardView::currentIndex() const
{
	for (int i = 0; i < m_bannerBtnGroup.buttons().count(); ++i) {
		auto btn = m_bannerBtnGroup.button(i);
		if (btn->isEnabled()) {
			return i;
		}
	}
	if (mBannerUrls.isEmpty()) {
		return -1;
	}
	return 0;
}
void PLSLaunchWizardView::setCurrentIndex(int index, bool useAnimation) const
{

	//do scroll
	auto item = this->currentItem();
	if (item == nullptr) {
		return;
	}
	if (!useAnimation) {
		scrollArea->ensureWidgetVisible(item->widget());
	}

	//change index
	for (int i = 0; i < m_bannerBtnGroup.buttons().size(); ++i) {
		auto btn = m_bannerBtnGroup.button(i);
		if (btn) {
			btn->setEnabled(i == index);
		}
	}
}

QLayoutItem *PLSLaunchWizardView::itemAt(int index) const
{
	auto count = scrollLay->count();
	for (int i = 0; i < count; ++i) {
		auto item = scrollLay->itemAt(i);
		if (item == nullptr) {
			continue;
		}
		auto wid = item->widget();
		if (wid == nullptr) {
			continue;
		}
		auto widIndex = wid->property("index").toInt();
		qDebug() << " index order " << i << " act " << widIndex;
		if (widIndex == index) {
			return item;
		}
	}

	return nullptr;
}

QWidget *PLSLaunchWizardView::widgetAt(int index) const
{
	auto item = this->itemAt(index);
	return item->widget();
}

QLayoutItem *PLSLaunchWizardView::currentItem() const
{
	auto index = this->currentIndex();
	qDebug() << " current index " << index;
	return this->itemAt(index);
}

QWidget *PLSLaunchWizardView::currentLabel() const
{
	auto item = currentItem();
	return item->widget();
}

void PLSLaunchWizardView::changeBannerView()
{
	auto sender_ = sender();

	auto size = scrollLay->count();
	if (size <= 0) {
		return;
	}
	bool isPre = true;
	int currentIndex = this->currentIndex();
	if (sender_->objectName() == "leftButton") {
		--currentIndex;

	} else if (sender_->objectName() == "rightButton") {
		++currentIndex;
		isPre = false;
	}
	if (currentIndex < 0) {
		currentIndex = size - 1;
	} else if (currentIndex >= size) {
		currentIndex = 0;
	}
	moveWidget(currentIndex, isPre);
	this->setCurrentIndex(currentIndex);
}

void PLSLaunchWizardView::checkBannerButtonsState()
{

	int count = static_cast<int>(mBannerUrls.count());

	ui->leftButton->setEnabled(count > 1);
	ui->rightButton->setEnabled(count > 1);

	updateGuideButtonState(count > 1 && isInWidgets(ui->bannerFrame));
	scrollArea->setProperty("showHandCursor", !mBannerUrls.isEmpty());
}

void PLSLaunchWizardView::delayCheckButtonsState()
{

	if (mDelayTimer == nullptr) {
		mDelayTimer = QSharedPointer<QTimer>::create(this);
		mDelayTimer->setInterval(200);
		connect(mDelayTimer.data(), &QTimer::timeout, this, &PLSLaunchWizardView::checkBannerButtonsState, Qt::QueuedConnection);
	}
	mDelayTimer->start();
	checkBannerButtonsState();
}

void PLSLaunchWizardView::updateGuideButtonState(bool on)
{
	ui->leftButton->setProperty("isOn", on);
	pls_flush_style(ui->leftButton);
	ui->rightButton->setProperty("isOn", on);
	pls_flush_style(ui->rightButton);
	if (!on && mDelayTimer) {
		mDelayTimer->stop();
	}
}

void PLSLaunchWizardView::singletonWakeup()
{
	this->show();
	if (this->isMinimized()) {
		this->setWindowState(this->windowState().setFlag(Qt::WindowMinimized, false));
	}
	activateWindow();
}

void PLSLaunchWizardView::hideView()
{
	PLS_UI_STEP("widzard", "hide button", "clicked");
	this->hide();
	pls_check_app_exiting();
	if (!isLoadBannerSuccess) {
		m_UpdateCount = 0;
		startUpdateBanner();
	}
}
void PLSLaunchWizardView::updateLangcherTips()
{
	ui->tipsLabel->setText(TipText);
}

void PLSLaunchWizardView::firstShow(QWidget *parent)
{
	pls_unused(parent);
	this->updateUserInfo();
	QTimer::singleShot(1000, this, [this]() {
		m_bShowFlag = true;
		scrollArea->horizontalScrollBar()->setValue(0);
		this->show();
		auto type = property("type").toString();
		if (type == "normal") {
			this->resize(QSize(650, 660));
		} else {
			this->resize(QSize(650, 774));
		}

		this->activateWindow();
		this->raise();
		auto mainView = pls_get_main_view();
		if (mainView) {
			pls_aligin_to_widget_center(this, mainView);
		}
		this->checkStackOrder();
	});
}

void PLSLaunchWizardView::moveWidget(int index, bool pre)
{

	if (m_bNeedSwapItem) {
		auto first = scrollLay->takeAt(0);
		scrollLay->addItem(first);
	}
	auto item = this->currentItem();
	int count = scrollLay->count();
	auto bar = scrollArea->horizontalScrollBar();
	auto width = item->geometry().width();
	auto scroller = QScroller::scroller(scrollArea);

	if (pre) {
		//end to first
		bar->setValue(width);
		auto end = scrollLay->takeAt(count - 1);
		scrollLay->insertItem(0, end);
		scroller->scrollTo(QPoint(0, 0), 500);
		m_bNeedSwapItem = false;
	} else {

		//first to end
		bar->setValue(0);
		scroller->scrollTo(QPoint(width, 0), 500);
		m_bNeedSwapItem = true;
	}
}

void PLSLaunchWizardView::updateUserInfo()
{
	QVariantMap info;
	info.insert(ChannelData::g_nickName, pls_get_prism_nickname());
	setUserInfo(info);
}
#define HELLO_TXT QString("Hello,")
void PLSLaunchWizardView::setUserInfo(const QVariantMap &info)
{
	QPixmap pix;
	pls_get_prism_user_thumbnail(pix);
	if (pix.isNull()) {
		pix = pls_shared_paint_svg(channel_data::g_defaultHeaderIcon, QSize(200, 200));
	}
	pix = pls_shared_circle_mask_image(pix);
	ui->userThumbnailLabel->setPixmap(pix);

	auto name = info.value(ChannelData::g_nickName).toString();
	ui->nickNameLabel->setText(HELLO_TXT + " " + name + "!");
}

void PLSLaunchWizardView::createBannerScrollView()
{
	scrollArea = new QScrollArea(this);
	scrollArea->setObjectName("bannerScrollArea");
	scrollArea->setWidgetResizable(true);
	auto scrollAreaWidgetContents = new QWidget();
	scrollAreaWidgetContents->setObjectName(QString::fromUtf8("scrollAreaWidgetContents"));
	scrollAreaWidgetContents->setContentsMargins(0, 0, 0, 0);
	scrollLay = new QHBoxLayout(scrollAreaWidgetContents);
	scrollLay->setContentsMargins(0, 0, 0, 0);
	scrollLay->setSpacing(0);
	scrollArea->setWidget(scrollAreaWidgetContents);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	maskWidget(scrollArea);
	ui->stackedWidget->addWidget(scrollArea);
}

void PLSLaunchWizardView::maskWidget(QWidget *wid)
{
	auto hlay = new QHBoxLayout(wid);
	auto coverLay = new QLabel(wid);
	coverLay->setScaledContents(true);
	coverLay->setObjectName("overlay");
	hlay->addWidget(coverLay);
	hlay->setContentsMargins(0, 0, 0, 0);
	coverLay->setMouseTracking(true);
	coverLay->installEventFilter(this);
	coverLay->setAttribute(Qt::WA_TransparentForMouseEvents);
}

QString formatTimeStr(const QDateTime &time)
{
	auto locale = IS_KR() ? QLocale(pls_get_current_language()) : QLocale::c();
	QString timeFormat = IS_KR() ? "yyyy. MM. dd. AP h:mm" : "MMM dd, yyyy h:mm AP";
	auto timeStr = locale.toString(time, timeFormat);
	return timeStr;
}
