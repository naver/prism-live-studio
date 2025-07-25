#include "PLSLaunchWizardView.h"
#include "ui_PLSLaunchWizardView.h"
#include "utils-api.h"
#include "PLSWizardInfoView.h"
#include <qdesktopservices.h>
#include <QUrl>
#include <qscrollbar.h>
#include "PLSBasic.h"
#include "PLSIPCHandler.h"
#include "pls-shared-functions.h"
#include "pls-channel-const.h"
#include "pls-shared-values.h"
#include <algorithm>
#include <random>
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
#include "PLSCommonConst.h"
#include "window-basic-main.hpp"
#include "pls-common-define.hpp"
#include "PLSSyncServerManager.hpp"
#include <QUrlQuery>

constexpr auto urlProperty = "url";
constexpr auto defaultBannenImage = ":/resource/images/wizard/img-banner-default.svg";

#define NoScheduleStr tr("WizardView.NoSchedule")
#define ScheduleError tr("WizardView.ScheduleError")
#define TipText tr("WizardView.Restart.OnError")
#define QueText tr("WizardView.Quetion")
#define DiscordText tr("WizardView.Discord")
#define UserGuideText tr("WizardView.UserGuide")
constexpr auto ModuleName = "WizardView";
constexpr int MAXUPDATACOUNT = 3;

QPointer<PLSLaunchWizardView> PLSLaunchWizardView::g_wizardView = nullptr;
PLSLaunchWizardView *PLSLaunchWizardView::instance()
{
	if (g_wizardView == nullptr) {
		g_wizardView = new PLSLaunchWizardView();
		connect(g_wizardView, &PLSLaunchWizardView::sigTryGetScheduleList, PLSCHANNELS_API, &PLSChannelDataAPI::startUpdateScheduleList, Qt::QueuedConnection);
		connect(PLSBasic::Get(), &PLSBasic::mainClosing, g_wizardView, &PLSLaunchWizardView::hideView, Qt::DirectConnection);
	}
	return g_wizardView;
}

PLSLaunchWizardView::PLSLaunchWizardView(QWidget *parent) : PLSWindow(parent)
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
	createUserGuideView();
	createQueView();
	createAdView();
	connect(ui->leftButton, &QPushButton::clicked, this, &PLSLaunchWizardView::changeBannerView);
	connect(ui->rightButton, &QPushButton::clicked, this, &PLSLaunchWizardView::changeBannerView);
	ui->wizardScrollArea->verticalScrollBar()->setObjectName("wizardVetialBar");
	connect(ui->hideButton, &QPushButton::clicked, this, &PLSLaunchWizardView::hideView);
	connect(this, &PLSLaunchWizardView::mouseClicked, this, &PLSLaunchWizardView::onWidgetClicked, Qt::QueuedConnection);
	connect(ui->wizardScrollArea->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int min, int max) {
		if (max > min) {
			ui->verticalLayout_5->setContentsMargins(0, 0, 3, 0);
			ui->infoLayout->setContentsMargins(39, 0, 26, 0);
			ui->horizontalLayout_2->setContentsMargins(11, 0, 1, 0);
		} else {
			ui->verticalLayout_5->setContentsMargins(0, 0, 0, 0);
			ui->infoLayout->setContentsMargins(39, 0, 39, 0);
			ui->horizontalLayout_2->setContentsMargins(1, 0, 1, 0);
		}
	});
	updateBannerView();

	connect(this, &PLSLaunchWizardView::bannerJsonDownloaded, this, &PLSLaunchWizardView::loadBannerSources, Qt::QueuedConnection);
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
	pls_scroll_area_clips_to_bounds(ui->wizardScrollArea);
#endif

	auto globalConfig = App()->GetUserConfig();
	auto lastHideDate = config_get_string(globalConfig, common::LAUNCHER_CONFIG, common::CONFIG_LASTHIDEDATE);
	QString currentDate = QDate::currentDate().toString("yyyy-MM-dd");
	if (lastHideDate == currentDate) {
		PLS_INFO(ModuleName, "get dont show config is true");
		ui->checkBox->setChecked(true);
	} else {
		ui->checkBox->setChecked(false);
	}
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
	getBannerJson();
	mBannerTimer->start();
}

void PLSLaunchWizardView::getBannerJson()
{
	pls::rsm::UrlAndHowSave downJsonUrl;
	downJsonUrl.url(pls_http_api_func::getPrismSynGateWay() + QStringLiteral("/pc-banner"));
	downJsonUrl.fileName(pls::rsm::FileName::FromUrl);
	downJsonUrl.noCache(true);
	downJsonUrl.hmacKey(pls_http_api_func::getPrismHamcKey());
	auto cb = [this](const pls::rsm::DownloadResult &result) {
		if (result.isOk()) {
			PLS_INFO(ModuleName, " request new banner json OK");
		} else {
			PLS_ERROR(ModuleName, " request new banner json failed");
		}
		if (result.hasFilePath())
			m_jsonPath = result.filePath();
		emit bannerJsonDownloaded();
	};
	pls::rsm::getDownloader()->download(downJsonUrl, this, cb);
}
void PLSLaunchWizardView::createAdView()
{
	if (m_browser) {
		return;
	}
	QString fullName = pls_get_user_path(QStringLiteral("PRISMLiveStudio/resources/library/library_Policy_PC/AD/index.html"));
	if (!QFile::exists(fullName)) {
#if defined(Q_OS_WIN)
		fullName = QCoreApplication::applicationDirPath() + QStringLiteral("/../../data/prism-studio/AD/index.html");
#elif defined(Q_OS_MACOS)
		fullName = QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/data/prism-studio/AD/index.html");
#endif
	}
	QUrl URL = QUrl::fromLocalFile(fullName);
	QUrlQuery query;
	query.addQueryItem(QStringLiteral("country"), QUrl::toPercentEncoding(pls_get_gcc_data()));
	URL.setQuery(query);
	m_browser = pls::browser::newBrowserWidget(pls::browser::Params() //
							   .url(URL.toString())
							   .initBkgColor(QColor(39, 39, 39))
							   .css("html, body { background-color: #272727; }")
							   .allowPopups(false)
							   .showAtLoadEnded(true)
							   .parent(this));
	m_browserContainer = new QWidget(this);
	m_browserContainer->setStyleSheet("background-color: #1e1e1f;");
	QVBoxLayout *containerLayout = new QVBoxLayout(m_browserContainer);
	containerLayout->setContentsMargins(0, 10, 0, 10);
	containerLayout->addWidget(m_browser);

	ui->infoLayout->addWidget(m_browserContainer, 0, 0, 1, 2);
	m_browser->setObjectName("browserWidget");
	m_browser->show();
}
void PLSLaunchWizardView::releaseAdView()
{
	if (m_browser) {
		m_browser->closeBrowser();
		m_browser->deleteLater();
		m_browser = nullptr;
	}
	if (m_browserContainer) {
		ui->infoLayout->removeWidget(m_browserContainer);
		m_browserContainer->deleteLater();
		m_browserContainer = nullptr;
	}
}
void PLSLaunchWizardView::updateBannerView()
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
	return PLSWindow::eventFilter(watched, event);
}

void PLSLaunchWizardView::closeEvent(QCloseEvent *event)
{
	hideView();
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

	if (!QFile::exists(m_jsonPath)) {
		PLS_INFO(ModuleName, "banner json not exists!");
		return;
	}
	auto data = pls_read_data(m_jsonPath);
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

	auto cachePath = pls_get_app_data_dir_pn(common::PLS_BANNANR_PATH);
	std::list<pls::rsm::UrlAndHowSave> urlAndHowSaves;
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
		auto downUrl = pls::rsm::UrlAndHowSave() //
				       .keyPrefix(url)
				       .url(url)
				       .filePath(filePath);
		urlAndHowSaves.push_back(downUrl);
		mLinks.insert(filePath, jobj.value("bannerClickLinkUrl").toString());
	}
	auto cb = [this](const std::list<pls::rsm::DownloadResult> &results) { emit bannerImageLoadFinished(results); };

	if (mBannerUrls.isEmpty()) {
		return;
	}
	pls::rsm::getDownloader()->download(urlAndHowSaves, this, cb);
}

void PLSLaunchWizardView::finishedDownloadBanner(const std::list<pls::rsm::DownloadResult> &results)
{

	int errorCount = 0;
	for (auto it = results.begin(); it != results.end(); ++it) {
		if (!it->isOk()) {
			PLS_ERROR(ModuleName, "downlaod banner failed,url is %s", it->m_urlAndHowSave.url().toString().toUtf8().constData());
			this->mBannerUrls.remove(it->filePath());
			++errorCount;
			continue;
		}
	}

	isLoadBannerSuccess = errorCount <= 0;
	isLoadingBanner = false;
	updateBannerView();
	setCurrentIndex(0, false);
	scrollArea->horizontalScrollBar()->setValue(0);
}

void PLSLaunchWizardView::createLiveInfoView()
{
	if (!m_liveInfoView) {
		m_liveInfoView = pls_new<PLSWizardInfoView>(PLSWizardInfoView::ViewType::LiveInfo, this);
		ui->infoLayout->addWidget(m_liveInfoView, 2, 0, 1, 2);

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
		ui->infoLayout->addWidget(m_alertInfoView, 1, 0, 1, 2);
		m_alertInfoView->hide();
	}

	onDumpCreated();
}

QFileInfoList getDumpInfoList()
{
	QDir dir(pls_get_app_data_dir_pn("crashDump"));
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
		auto data = pls_read_data(lastInfoFile);
		if (!data.isEmpty()) {
			auto doc = QJsonDocument::fromJson(data);
			json = doc.object();
		}
	}

	handleErrorMessage(json.toVariantMap());
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

void PLSLaunchWizardView::createUserGuideView()
{
	auto blogView = pls_new<PLSWizardInfoView>(PLSWizardInfoView::ViewType::Blog, this);
	blogView->setObjectName("buttonInfoView");
	ui->infoLayout->addWidget(blogView, 3, 0, 1, 1);
	blogView->setInfoText(UserGuideText);

	blogView->setProperty(urlProperty, g_userGuide);
	connect(blogView, &QPushButton::clicked, this, &PLSLaunchWizardView::onUrlButtonClicked);
}

void PLSLaunchWizardView::createQueView()
{
	auto queView = pls_new<PLSWizardInfoView>(PLSWizardInfoView::ViewType::Que, this);
	queView->setObjectName("buttonInfoView");
	ui->infoLayout->addWidget(queView, 3, 1, 1, 1);
	queView->setInfoText(DiscordText);
	auto url = PLSSyncServerManager::instance()->getDiscordUrl();
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
	auto pix = QPixmap(path);
	label->setPixmap(pix);
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

void PLSLaunchWizardView::checkShowErrorAlert(const QVariantMap &body)
{
	//ux error for refreshing not by click   show on prism
	if (!isRefresh) {
		return;
	}
	if (!this->isVisible()) {
		return;
	}
	//error for clicking refresh button on lancher
	auto retData = body.value(ChannelData::g_errorRetdata).value<PLSErrorHandler::RetData>();
	PLSErrorHandler::directShowAlert(retData, this);
	isRefresh = false;
}

void PLSLaunchWizardView::handleChannelMessage(const QVariantMap &body)
{
	QString title;
	QString TimeStr;
	QString platformName;

	if (body.contains(ChannelData::g_errorString)) {
		title = ScheduleError;
		checkShowErrorAlert(body);
	} else if (body.contains(ChannelData::g_timeStamp)) {
		auto time = body.value(ChannelData::g_timeStamp).toLongLong();
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
		platformName = body.value(ChannelData::g_channelName).toString();
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
	auto msgBody = params.value("msg").toMap();
	switch (MessageType(type)) {
	case MessageType::ChannelsModuleMsg:
		handleChannelMessage(msgBody);
		break;
	case MessageType::AppState:
		handlePrismState(msgBody);
		break;
	case MessageType::ErrorInfomation:
		handleErrorMessage(msgBody);
		break;
	default:
		break;
	}
}

bool PLSLaunchWizardView::isNeedShow()
{
	if (m_bShowFlag) {
		return false;
	}
	auto lastHideDate = config_get_string(App()->GetUserConfig(), common::LAUNCHER_CONFIG, common::CONFIG_LASTHIDEDATE);
	QString currentDate = QDate::currentDate().toString("yyyy-MM-dd");
	if (lastHideDate == currentDate) {
		PLS_INFO(ModuleName, "get dont show config is true");
		return false;
	}

	return true;
}

void PLSLaunchWizardView::handlePrismState(const QVariantMap &body)
{
	if (m_stopLoadingTimer == nullptr) {
		m_stopLoadingTimer = QSharedPointer<QTimer>::create(this);
		m_stopLoadingTimer->setInterval(15000);
		m_stopLoadingTimer->setSingleShot(true);
		connect(m_stopLoadingTimer.data(), &QTimer::timeout, this, [this]() { m_liveInfoView->loading(false); }, Qt::QueuedConnection);
	}

	auto state = body.value(ChannelData::g_prismState).toInt();
	if (state == int(ChannelData::PrismState::Bussy)) {
		m_liveInfoView->loading(true);
		m_stopLoadingTimer->start();
	} else {
		m_liveInfoView->loading(false);
		m_stopLoadingTimer->stop();
	}
}

#ifdef _DEBUG
const int disapearTime = 5 * 1000;
#else
const int disapearTime = 60 * 1000;
#endif // DEBUG

void PLSLaunchWizardView::handleErrorMessage(const QVariantMap &body)
{
	auto location = body.value(shared_values::errorTitle).toString();
	auto content = body.value(shared_values::errorContent).toString();

	auto timeI = body.value(shared_values::errorTime).toLongLong();
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
	if (m_bShowFlag) {
		createAdView();
		PLS_INFO(ModuleName, "launch singletonWakeup");
		this->show();
		if (this->isMinimized()) {
			this->setWindowState(this->windowState().setFlag(Qt::WindowMinimized, false));
		}
		raise();
		activateWindow();
#if defined(Q_OS_MACOS)
		pls_scroll_area_clips_to_bounds(ui->wizardScrollArea);
		this->setWindowState(windowState() & ~Qt::WindowFullScreen);
#endif
	} else {
		firstShow();
	}
}

void PLSLaunchWizardView::hideView()
{
	this->hide();
	releaseAdView();
	pls_check_app_exiting();
	if (!isLoadBannerSuccess) {
		m_UpdateCount = 0;
		startUpdateBanner();
	}
	bool state = ui->checkBox->isChecked();
	PLS_INFO(ModuleName, "set dont show config is %d", state);
	auto globalConfig = App()->GetUserConfig();
	if (state) {
		auto byteArray = QDate::currentDate().toString("yyyy-MM-dd").toUtf8();
		auto currentDate = byteArray.constData();
		config_set_string(globalConfig, common::LAUNCHER_CONFIG, common::CONFIG_LASTHIDEDATE, currentDate);
		PLS_INFO(ModuleName, "set last show date is %s", currentDate);
	} else {
		config_set_string(globalConfig, common::LAUNCHER_CONFIG, common::CONFIG_LASTHIDEDATE, "");
		PLS_INFO(ModuleName, "set last show date is empty");
	}
	config_save_safe(globalConfig, "tmp", nullptr);
}

void PLSLaunchWizardView::firstShow(QWidget *parent)
{
	pls_unused(parent);
	this->updateUserInfo();
	QTimer::singleShot(1000, this, [this]() {
		pls_check_app_exiting();
		m_bShowFlag = true;
		PLS_INFO(ModuleName, "launch show");
		scrollArea->horizontalScrollBar()->setValue(0);
		this->show();
#if defined(Q_OS_MACOS)
		this->setWindowState(windowState() & ~Qt::WindowFullScreen);
#endif
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
