#include "ResolutionGuidePage.h"
#include "ResolutionGuideItem.h"
#include "pls-channel-const.h"
#include <QFile>
#include <QJsonDocument>

#include "liblog.h"
#include "window-basic-main.hpp"
#include <QRegularExpression>
#include "PLSMainView.hpp"
#include "ResolutionConst.h"
#include "PLSChannelDataAPI.h"
#include "ChannelCommonFunctions.h"
#include "PLSSyncServerManager.hpp"
#include "libui.h"
#include "PLSBasic.h"
#include "PLSPlatformApi.h"
#include "pls/pls-obs-api.h"
#include "login-user-info.hpp"
#include "B2BResolutionGuideItem.h"
#include "pls-common-define.hpp"
#include "frontend-api.h"
#include "PLSServerStreamHandler.hpp"
#include "pls/pls-dual-output.h"
#include "PLSLoginDataHandler.h"

const char ResolutionGroup[] = "resolution";
const char ResolutionGeo[] = "resolutionGeo";

QVariantList ResolutionGuidePage::mResolutions = QVariantList();

ResolutionGuidePage::ResolutionGuidePage(QWidget *parent) : PLSDialogView(parent)
{
	pls_add_css(this, {"ResolutionGuidePage"});
	setupUi(ui);
	setMoveInContent(true);
	initSize(720, 488);
	connect(
		this, &ResolutionGuidePage::sigSetResolutionFailed, this,
		[this]() {
			showAlertOnSetResolutionFailed();
			this->close();
		},
		Qt::QueuedConnection);

	QString userServiceName = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
	if (!userServiceName.isEmpty()) {
		ui->B2BServerName->setText(userServiceName);
		mB2BLogin = true;
		ui->MainTitleLabel->setText(tr("ResolutionGuide.NCP.MainTitle"));
	}

	initialize();
}

ResolutionGuidePage::~ResolutionGuidePage()
{
	emit visibilityChanged(false);
}

ResolutionGuidePage *ResolutionGuidePage::createInstance(QWidget *parent)
{
	static QPointer<ResolutionGuidePage> page;
	if (page != nullptr) {
		page->close();
	}

	if (parent == nullptr) {
		parent = pls_get_main_view();
	}
	page = new ResolutionGuidePage(parent);
	page->setAttribute(Qt::WA_DeleteOnClose);
	page->connectMainView();

#if defined(Q_OS_MACOS)
	page->setHasCloseButton(false);
	page->setHasMinButton(false);
	page->setHasMaxResButton(false);
	page->setWindowTitle(QTStr("ResolutionGuide.MainTitle"));
#else
	page->setHasCaption(false);
#endif
	page->setMoveInContent(true);

	return page;
}

void ResolutionGuidePage::saveSettings() const
{
	QByteArray setting = this->saveGeometry();
	config_set_string(App()->GlobalConfig(), ResolutionGroup, ResolutionGeo, setting.toBase64().constData());
	config_save(App()->GlobalConfig());
}

void ResolutionGuidePage::adjustLayout()
{
	if (mB2BLogin) {
		auto buttonGroup = pls_new<QButtonGroup>();
		buttonGroup->addButton(ui->B2BTab, 0);
		buttonGroup->addButton(ui->otherTab, 1);
		initializeB2BItem();
	} else {
		ui->TabFrame->hide();
		ui->stackedWidget->setCurrentIndex(1);
	}
}

bool ResolutionGuidePage::initializeB2BItem()
{
	ui->errorLabel->hide();
	ui->B2BHeaderFrame->hide();
	on_updateButton_clicked();
	return true;
}

void ResolutionGuidePage::showB2BErrorLabel(const int errorCode)
{
	ui->B2BHeaderFrame->hide();
	ui->B2BHeaderFrame->setContentsMargins(0, 0, 0, 0);
	ui->B2BScrollLayout->setContentsMargins(0, 0, 0, 0);
	ui->B2BScrollLayout->setAlignment(Qt::AlignCenter);
	QString errText;
	switch (errorCode) {
	case EmptyList:
	case ReturnFail:
		errText = tr("ResolutionGuide.B2BEmptyList").arg(ui->B2BServerName->text());
		break;
	case ServiceDisable:
		errText = tr("Ncb2b.Service.Disable.Status");
		break;
	default:
		errText = tr("ResolutionGuide.B2BNetworkError");
		break;
	}
	ui->errorLabel->setText(errText);
	ui->errorLabel->show();
}

QString ResolutionGuidePage::getFilePath(const QString &fileName)
{
	auto path = pls_get_user_path(common::CONFIGS_LIBRARY_POLICY_PATH) + "../ncp_service_res/%1";
	return path.arg(fileName);
}

void ResolutionGuidePage::handThumbnail()
{
	int thumbnailId = 0;
	for (auto B2BResolutionPara : mB2BResolutionParaList) {
		thumbnailId++;
		auto thumbnailPath = getFilePath(QString("%1_%2.png").arg("streamingPresetThumbnail").arg(thumbnailId));
		if (!QFile::exists(thumbnailPath)) {
			continue;
		}
		QImage original;
		original.load(thumbnailPath);
		auto targetPixMap = PLSLoginDataHandler::instance()->scaleAndCrop(original, QSize(34, 34));
		auto pixmap = QPixmap::fromImage(targetPixMap);
		pls_shared_circle_mask_image(pixmap);
		bool isSuccess = pixmap.save(B2BResolutionPara.streamingPresetThumbnail, "PNG");
		PLS_INFO("ResolutionGuidePage", "save tagIcon is %s", isSuccess ? "true" : "false");
	}
	downloadThumbnailFinish();
}

QList<B2BResolutionPara> ResolutionGuidePage::parseServiceStreamingPreset(QJsonObject &object)
{
	auto servicesList = object.value("services").toArray();
	if (servicesList.isEmpty()) {
		PLS_ERROR("ResolutionGuidePage", "API return ServiceStreamingPreset is null");
		return QList<B2BResolutionPara>{};
	}
	m_urlAndHowSaves.clear();
	mB2BResolutionParaList.clear();
	int thumbnailId = 0;
	for (auto services : servicesList) {
		thumbnailId++;
		B2BResolutionPara para = {};
		para.templateName = services.toObject().value("name").toString();
		auto streamingPresetThumbnail = services.toObject().value("streamingPresetThumbnail").toString();
		auto recommended = services.toObject().value("recommended").toObject();
		int videoBitrate = recommended.value("videoBitrate").toInt();
		para.bitrate = QString::number(videoBitrate) + " Kbps";
		auto keyframeInterval = recommended.value("keyint").toInt();
		para.keyframeInterval = QString::number(keyframeInterval);
		auto resolution = recommended.value("resolution").toString();
		auto fps = recommended.value("fps").toInt();
		para.output_FPS = resolution + " / " + QString::number(fps) + "FPS";
		auto thumbnailPath = getFilePath(QString("%1_%2.png").arg("streamingPresetThumbnail").arg(thumbnailId));
		pls_remove_file(thumbnailPath);
		auto downUrl = pls::rsm::UrlAndHowSave() //
				       .keyPrefix(streamingPresetThumbnail)
				       .url(streamingPresetThumbnail)
				       .filePath(thumbnailPath);
		m_urlAndHowSaves.push_back(downUrl);
		auto corpPath = pls_get_user_path(common::CONFIGS_LIBRARY_POLICY_PATH) + "images/streamingPresetThumbnail_" + QString::number(thumbnailId) + ".png";
		pls_remove_file(corpPath);

		para.streamingPresetThumbnail = corpPath;
		mB2BResolutionParaList.append(para);
	}
	auto cb = [this](const std::list<pls::rsm::DownloadResult> &results) {
		for (auto &res : results) {
			if (!res.isOk()) {
				PLS_ERROR("ResolutionGuidePage", "downlaod banner failed,url is %s", res.m_urlAndHowSave.url().toString().toUtf8().constData());
			}
		}
		handThumbnail();
		m_downLoadRequestExisted = false;
	};
	m_downLoadRequestExisted = true;
	pls::rsm::getDownloader()->download(m_urlAndHowSaves, this, cb);
	return mB2BResolutionParaList;
}

void ResolutionGuidePage::UpdateB2BUI()
{
	ui->B2BHeaderFrame->show();
	ui->B2BHeaderFrame->setContentsMargins(30, 30, 0, 0);
	ui->errorLabel->hide();
	ui->B2BScrollLayout->setContentsMargins(0, 0, 0, 30);
	ui->B2BScrollLayout->setAlignment(Qt::AlignTop);
	for (auto B2BResolutionPara : mB2BResolutionParaList) {
		auto item = new B2BResolutionGuideItem(this);
		ui->B2BScrollLayout->addWidget(item);
		B2BResolutionPara.serviceName = ui->B2BServerName->text();
		item->initialize(B2BResolutionPara, this);
		connect(item, &B2BResolutionGuideItem::sigResolutionSelected, this, &ResolutionGuidePage::onUserSelectedResolution);
	}
}

QString ResolutionGuidePage::getPreferResolutionStringOfPlatform(const QString &platform)
{
	auto isMatchPlatform = [&](const QVariant &resolutionData) {
		auto varMap = resolutionData.toMap();
		if (varMap.isEmpty()) {
			return false;
		}

		if (auto dataPlatform = varMap.value(ChannelData::g_channelName).toString(); platform.compare(dataPlatform, Qt::CaseInsensitive) != 0) {
			return false;
		}

		return true;
	};
	const auto &resolutions = getResolutionsList();
	auto it = std::find_if(resolutions.cbegin(), resolutions.cend(), isMatchPlatform);
	if (it == resolutions.cend()) {
		return "(1280x720/30FPS)";
	}
	return it->toMap().value(resolution_const_space::g_PreferResolution).toString();
}

Resolution ResolutionGuidePage::getPreferResolutionOfPlatform(const QString &platform)
{
	return parserStringOfResolution(getPreferResolutionStringOfPlatform(platform));
}

Resolution ResolutionGuidePage::parserStringOfResolution(const QString &resolution)
{
	QRegularExpression regx("\\d+");
	auto it = regx.globalMatch(resolution);
	Resolution ret;
	ret.width = it.next().captured(0).toUInt();
	ret.height = it.next().captured(0).toUInt();
	ret.fps = it.next().captured(0).toInt();

	return ret;
}

void ResolutionGuidePage::loadSettings()
{
	QByteArray setting = QByteArray::fromBase64(config_get_string(App()->GlobalConfig(), ResolutionGroup, ResolutionGeo));
	this->restoreGeometry(setting);
}

const QVariantList &ResolutionGuidePage::getResolutionsList()
{
	if (mResolutions.isEmpty()) {
		mResolutions = PLSSyncServerManager::instance()->getResolutionsList();
		PLS_INFO("ResolutionGuidePage", "get resoltion item size is %d", mResolutions.size());
	}

	return mResolutions;
}

void ResolutionGuidePage::initialize()
{
	const auto &resolutions = getResolutionsList();
	for (const auto &varmap : resolutions) {
		auto item = new ResolutionGuideItem(this);
		ui->ScrollWidgetLayOut->insertWidget(ui->ScrollWidgetLayOut->count() - 1, item);
		item->initialize(varmap.toMap());
		connect(item, &ResolutionGuideItem::sigResolutionSelected, this, &ResolutionGuidePage::onUserSelectedResolution);
	}

	connect(PLSPlatformApi::instance(), &PLSPlatformApi::outputStateChanged, this, &ResolutionGuidePage::updateItemsState);
	adjustLayout();
	updateItemsState();
}

ResolutionGuidePage::CannotTipObject ResolutionGuidePage::createCannotTipForWidget(QWidget *parentWidget, const QSize &fixSize, const QPoint &moveDistance)
{
	CannotTipObject obj;
	obj.mFixSize = fixSize;
	obj.mParentWidget = parentWidget;
	obj.mMoveDistance = moveDistance;

	auto label = new QLabel(parentWidget);
	label->setObjectName("CannotTip");
	label->setWindowOpacity(0.95);

	obj.mTip = label;
	obj.isValid = true;
	return obj;
}

bool ResolutionGuidePage::CannotTipObject::checkIsCanChange()
{
	auto state = ResolutionGuidePage::getCurrentOutputState();
	if (state == OutputState::OuputIsOff || state == OutputState::NoState) {
		isCanChange = true;
		return isCanChange;
	}
	isCanChange = false;

	return isCanChange;
}

void ResolutionGuidePage::CannotTipObject::updateText()
{
	mText.clear();

	auto outputState = ResolutionGuidePage::getCurrentOutputState();
	if (outputState == OutputState::OuputIsOff) {
		return;
	}

	if (outputState.testFlag(OutputState::StreamIsOn) || outputState.testFlag(OutputState::ReplayBufferIsOn) || outputState.testFlag(OutputState::RecordIsOn)) {

		mText = tr("Resolution.InputIsActived");
		return;
	}
	if (mText.isEmpty() && outputState.testFlag(OutputState::VirtualCamIsOn)) {

		mText = tr("Resolution.VirtualCamIsActived");
		return;
	}

	auto iCount = pls_get_active_output_count();
	if (iCount > 0) {
		auto name = pls_get_active_output_name(0);
		mText = tr("Resolution.OtherPluginOutputActived").arg(name);
	}
}

void ResolutionGuidePage::CannotTipObject::updateGeometry()
{
	if (mTip == nullptr) {
		return;
	}

	auto newSize = mFixSize;
	if (newSize.width() != 0) {
		mTip->setFixedWidth(newSize.width());
	}
	if (newSize.height() != 0) {
		mTip->setFixedHeight(newSize.height());
	} else {
		mTip->setWordWrap(true);
	}

	mTip->setText(mText);
	mTip->adjustSize();
	auto pos = mParentWidget->rect().bottomLeft() + mMoveDistance - mTip->rect().bottomLeft();

	mTip->move(pos);
}

void ResolutionGuidePage::updateItemsState()
{
	bool isCanChange = false;
	auto mainview = PLSBasic::Get();
	if (mainview == nullptr) {
		return;
	}

	if (!mainview->Active()) {
		isCanChange = true;
	}

	if (obs_video_active()) {
		isCanChange = false;
	}
	auto items = this->findChildren<ResolutionGuideItem *>();
	for (auto item : items) {
		item->setLinkEnanled(isCanChange);
	}

	auto B2Bitems = this->findChildren<B2BResolutionGuideItem *>();
	for (auto item : B2Bitems) {
		item->setLinkEnanled(isCanChange);
	}

	if (!mCannotTip.isValid) {
		mCannotTip = createCannotTipForWidget(this, QSize(660, 0), QPoint(30, -90));
	}

	mCannotTip.updateUI();

	QTimer::singleShot(500, this, [this]() {
		PLS_INFO("ResolutionGuidePage", "singleShot updateItemsState");
		updateSpace(mCannotTip.mTip && mCannotTip.mTip->isVisibleTo(this));
	});
}

void ResolutionGuidePage::updateSpace(bool isAdd)
{

	if (isAdd) {
		if (mSpaceItem == nullptr) {
			mSpaceItem = new QSpacerItem(mCannotTip.mTip->width(), mCannotTip.mTip->height() + 10, QSizePolicy::Fixed, QSizePolicy::Fixed);
			ui->ContentFrameLayout->addSpacerItem(mSpaceItem);
		}

		return;
	}
	if (mSpaceItem != nullptr) {
		ui->ContentFrameLayout->removeItem(mSpaceItem);
		delete mSpaceItem;
		mSpaceItem = nullptr;
	}
}

void ResolutionGuidePage::CannotTipObject::updateUI(bool autoShow)
{
	checkIsCanChange();
	updateText();
	updateGeometry();
	if (autoShow && mTip) {
		mTip->setVisible(!isCanChange);
	}
}

extern QString translatePlatformName(const QString &platformName);

bool ResolutionGuidePage::isAcceptToChangeResolution(const QString &platform, const QString &resolution)
{
	auto questionContent = tr("ResolutionGuide.QuestionContent").arg(translatePlatformName(platform)).arg(resolution);
	auto ret = PLSAlertView::question(pls_get_main_view(), tr("Confirm"), questionContent,
					  {{PLSAlertView::Button::Yes, tr("ResolutionGuide.ApplyNowBtn")}, {PLSAlertView::Button::Cancel, tr("Cancel")}});

	return (ret == PLSAlertView::Button::Yes);
}

void ResolutionGuidePage::onUserSelectedResolution(const QString &txt)
{
	if (PLSCHANNELS_API->isLivingOrRecording() || obs_video_active()) {
		return;
	}

	PLS_UI_STEP("ResolutionGuidePage", "ResolutionGuidePage:", QString("user select resolution " + txt).toUtf8().constData());
	auto infoLst = txt.split(":");
	const auto &platform = infoLst[0];
	const auto &resolution = infoLst[1];
	bool bVerticalOutput = false;
	if (pls_is_dual_output_on()) {
		auto questionContent = tr("ResolutionGuide.QuestionContent").arg(translatePlatformName(platform)).arg(resolution);
		auto ret = PLSAlertView::dualOutputApplyResolutionWarn(pls_get_main_view(), tr("Confirm"), questionContent,
								       {{PLSAlertView::Button::Ok, tr("ResolutionGuide.ApplyNowBtn")}, {PLSAlertView::Button::Cancel, tr("Cancel")}},
								       tr("ResolutionGuide.HorizontalApply"), tr("ResolutionGuide.VerticalApply"), bVerticalOutput);
		if (ret == PLSAlertView::Button::Cancel) {
			return;
		}
	} else {
		if (!isUserNeed(platform, resolution)) {
			return;
		}
	}

	auto resolutionData = parserStringOfResolution(resolution);
	if (platform.contains(NAVER_SHOPPING_LIVE, Qt::CaseInsensitive)) {
		resolutionData.bitrate = 2500;
	}
	bool applyB2BItem = false;
	if (mB2BLogin && infoLst.size() == 4) {
		QString strBitrate = infoLst[2];
		QRegularExpression re("^(\\d+)");
		QRegularExpressionMatch match = re.match(strBitrate);
		if (match.hasMatch()) {
			QString numberStr = match.captured(1);
			resolutionData.bitrate = numberStr.toInt();
		}
		resolutionData.keyframeInterval = infoLst[3].toInt();
		applyB2BItem = true;
	}

	if (!setResolution(resolutionData, true, bVerticalOutput)) {
		emit sigSetResolutionFailed();
		return;
	}
	emit resolutionUpdated();
	if (onUpdateResolution) {
		onUpdateResolution();
	}

	QVariantMap uploadVariantMap;
	uploadVariantMap.insert("platform", platform);
	if (applyB2BItem) {
		uploadVariantMap.insert("platform", "NCP");
		uploadVariantMap.insert("serviceName", ui->B2BServerName->text());
		uploadVariantMap.insert("outputParam", platform);
	}
	pls_send_analog(AnalogType::ANALOG_PLATFORM_OUTPUTGUIDE, uploadVariantMap);
	this->accept();
}

void ResolutionGuidePage::on_B2BTab_clicked()
{
	ui->stackedWidget->setCurrentIndex(0);
	ui->B2BServerName->setProperty("slected", true);
	pls_flush_style(ui->B2BServerName);
}

void ResolutionGuidePage::on_otherTab_clicked()
{
	ui->stackedWidget->setCurrentIndex(1);
	ui->B2BServerName->setProperty("slected", false);
	pls_flush_style(ui->B2BServerName);
}

void ResolutionGuidePage::on_updateButton_clicked()
{
	ui->B2BTab->click();

	if (m_updateRequestExisted || m_downLoadRequestExisted) {
		PLS_INFO("ResolutionGuidePage", "The refresh serviceStreamingPreset request already exists, avoid duplicate request.");
		return;
	}

	QLayoutItem *item;
	while ((item = ui->B2BScrollLayout->takeAt(1)) != nullptr) {
		if (item->widget()) {
			delete item->widget();
		}
		delete item;
	}

	if (!pls_get_network_state()) {
		showB2BErrorLabel(NetworkError);
		return;
	}

	auto okCallback = [this](const QJsonObject &data) {
		QJsonObject serviceStreamingPreset = data.value("serviceStreamingPreset").toObject();
		if (serviceStreamingPreset.isEmpty()) {
			PLS_WARN("ResolutionGuidePage", "There was no ResolutionGuidePage field value was retrieved from the api.");
			showB2BErrorLabel(EmptyList);
		} else {
			parseServiceStreamingPreset(serviceStreamingPreset);
			UpdateB2BUI();
			updateItemsState();
		}
		m_updateRequestExisted = false;
	};

	auto failCallback = [this](const QJsonObject &data, const PLSErrorHandler::RetData &retData) {
		if (retData.prismCode == PLSErrorHandler::CHANNEL_NCP_B2B_1101_SERVICE_DISABLED) {
			showB2BErrorLabel(ServiceDisable);
		} else {
			showB2BErrorLabel(ReturnFail);
		}
		m_updateRequestExisted = false;
		PLS_WARN("ResolutionGuidePage", "get serviceStreamingPreset error code is %d", retData.prismCode);
	};
	m_updateRequestExisted = true;
	PLSLoginDataHandler::instance()->getNCB2BServiceResFromRemote(okCallback, failCallback, this);
}

void ResolutionGuidePage::changeEvent(QEvent *e)
{
	QWidget::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

bool ResolutionGuidePage::event(QEvent *event)
{

	switch (event->type()) {
	case QEvent::Show:
		emit visibilityChanged(true);
		break;
	case QEvent::Hide:
		emit visibilityChanged(false);
		this->setModal(false);

		break;
	case QEvent::Close:
		this->accept();
		return true;

	case QEvent::Resize:
		mCannotTip.updateGeometry();
		break;
	default:

		break;
	}
	return PLSDialogView::event(event);
}

void ResolutionGuidePage::connectMainView()
{
	/*const PLSMainView *View = dynamic_cast<PLSMainView *>(pls_get_main_view());
	QObject::connect(this, &ResolutionGuidePage::visibilityChanged, View, &PLSMainView::toggleResolutionButton, Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));

	connect(
		View, &PLSMainView::isshowSignal, this,
		[this](bool visible) {
			if (!visible) {
				this->close();
			}
		},
		Qt::DirectConnection);*/
}

void ResolutionGuidePage::on_CloseBtn_clicked()
{
	PLS_UI_STEP("ResolutionGuide", (" ResolutionGuide close button "), "Clicked");
	this->close();
}

void ResolutionGuidePage::showResolutionGuideCloseAfterChange(QWidget *parent)
{
	PLS_UI_STEP("ResolutionGuide", QString(parent->objectName() + " Resolution button ").toUtf8().constData(), "Clicked");
	setVisibleOfGuide(parent, [parent]() {
		parent->hide();
		parent->deleteLater();
		pls_get_main_view()->activateWindow();
	});
}

void ResolutionGuidePage::showIntroduceTip(QWidget *parent, const QString &platformName)
{
	auto frame = new QFrame(parent, Qt::SubWindow);
#if defined(Q_OS_MACOS)
	frame->setAttribute(Qt::WA_NativeWindow);
	frame->setAttribute(Qt::WA_DontCreateNativeAncestors);
#endif
	frame->setObjectName("CheckInvalidTipFrame");
	auto lay = new QHBoxLayout(frame);

	auto tip = new QLabel(frame);
	tip->setObjectName("CheckInvalidTip");
	tip->setWordWrap(true);
	tip->setText(QObject::tr("ResolutionGuide.InvalidResolution").arg(platformName));
	lay->addWidget(tip);
	lay->setAlignment(tip, Qt::AlignTop);

	auto btn = new QPushButton(frame);
	lay->addWidget(btn);
	btn->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
	btn->setObjectName("CheckInvalidTipCloseBtn");
	lay->addWidget(btn);
	lay->setAlignment(btn, Qt::AlignTop);

	lay->setContentsMargins(0, 0, 0, 0);

	auto deleteFrame = [frame]() {
		PLS_INFO("ResolutionGuidePage", "frame delete");
		frame->hide();
		frame->deleteLater();
	};
	connect(btn, &QAbstractButton::clicked, frame, deleteFrame);

	auto calculate = [frame, parent]() {
		auto tipSize = QSize(parent->size().width() - 50, 61);
#if defined(Q_OS_MACOS)
		frame->setGeometry(25, 73, tipSize.width(), tipSize.height());
#else
		frame->setGeometry(25, 95 + 15, tipSize.width(), tipSize.height());
#endif
	};

	calculate();
	frame->show();
	QTimer::singleShot(5000, frame, deleteFrame);
}

ResolutionGuidePage::OutputStates ResolutionGuidePage::getCurrentOutputState()
{
	OutputStates retState = OutputState::NoState;

	auto mainView = PLSBasic::Get();
	if (mainView) {
		//we can't use output state because it changed after event
		retState.setFlag(OutputState::StreamIsOn, PLSCHANNELS_API->isLiving());
		retState.setFlag(OutputState::RecordIsOn, PLSCHANNELS_API->isRecording());
		retState.setFlag(OutputState::VirtualCamIsOn, mainView->VirtualCamActive());
		auto iCount = pls_get_active_output_count();
		if (iCount > 0) {
			retState.setFlag(OutputState::OtherPluginOutputIsOn, true);
		}
	}

	return retState;
}

void ResolutionGuidePage::checkResolution(QWidget *parent, const QString &uuid)
{
	if (PLSCHANNELS_API->isLiving()) {
		return;
	}
	auto platformInfo = PLSCHANNELS_API->getChannelInfo(uuid);
	auto channelTyp = getInfo(platformInfo, ChannelData::g_data_type, ChannelData::ChannelDataType::ChannelType);
	auto platformName = getInfo(platformInfo, ChannelData::g_channelName);
	if (PLSCHANNELS_API->getChannelUserStatus(uuid) != ChannelData::Enabled && channelTyp == ChannelData::ChannelDataType::ChannelType) {
		return;
	}

	auto dualOutput = getInfo(platformInfo, ChannelData::g_channelDualOutput, ChannelData::NoSet);
	checkResolutionForPlatform(parent, platformName, channelTyp, pls_is_dual_output_on() && dualOutput == ChannelData::VerticalOutput);
}

void ResolutionGuidePage::checkResolutionForPlatform(QWidget *parent, const QString &platformName, int channelTyp, bool bVerticalOutput)
{
	if (isCurrentResolutionFitableFor(platformName, channelTyp, bVerticalOutput)) {
		return;
	}
	showIntroduceTip(parent, translatePlatformName(platformName));
}

bool ResolutionGuidePage::isCurrentResolutionFitableFor(const QString &platform, int channelType, bool bVerticalOutput)
{
	const auto &platformResolution = PLSSyncServerManager::instance()->getLivePlatformResolutionFPSMap();
	static const auto &rtmpResolution = PLSSyncServerManager::instance()->getRtmpPlatformFPSMap();

	QVariantMap searchMap;

	const QString searchKey = platform.toLower().remove(QRegularExpression("\\W+"));
	if (channelType >= ChannelData::CustomType) {
		searchMap = rtmpResolution;
	} else {
		searchMap = platformResolution;
	}

	if (!searchMap.contains(searchKey)) {
		return true;
	}

	QString outFps = PLSServerStreamHandler::instance()->getOutputFps();
	QString searchResolution = PLSServerStreamHandler::instance()->getOutputResolution(bVerticalOutput);

	if (auto platformResolutionMap = searchMap.value(searchKey).toMap(); platformResolutionMap.contains(searchResolution)) {
		auto validFps = platformResolutionMap.value(searchResolution);
		if (validFps.canConvert<QString>() && validFps.toString() == outFps) {
			return true;
		}

		if (validFps.canConvert<QStringList>() && validFps.toStringList().contains(outFps)) {
			return true;
		}
	}

	return false;
}

void ResolutionGuidePage::setVisibleOfGuide(QWidget *parent, const UpdateCallback &callback, const IsUserNeed &agreeFun, bool isVisible)
{
	auto mianView = PLSBasic::instance()->getMainView();
	mianView->setResolutionBtnCheck(true);
	if (parent == nullptr) {
		parent = mianView;
	}

	auto page = createInstance(parent);

	auto setvisibleOf = [page, callback, agreeFun, mianView](bool isVisibleL) {
		if (isVisibleL) {

			page->setUpdateResolutionFunction(callback);
			page->setAgreeFunction(agreeFun);
			page->exec();
			mianView->setResolutionBtnCheck(false);
			return;
		}
		page->reject();
	};

	setvisibleOf(isVisible);
}

bool ResolutionGuidePage::setUsingPlatformPreferResolution(const QString &platform)
{
	auto resolution = getPreferResolutionOfPlatform(platform);
	if (platform.contains(NAVER_SHOPPING_LIVE, Qt::CaseInsensitive)) {
		resolution.bitrate = 2500;
	}
	return setResolution(resolution);
}

bool ResolutionGuidePage::setResolution(int width, int height, int fps, bool toChangeCanvas, bool bVerticalOutput)
{
	if (pls_is_output_actived()) {
		return false;
	}

	if (0 == width) {
		width = 1280;
	}
	if (0 == height) {
		height = 720;
	}
	if (0 == fps) {
		fps = 30;
	}
	auto mainView = PLSBasic::instance();
	auto settings = mainView->Config();

	config_set_uint(settings, "Video", bVerticalOutput ? "OutputCXV" : "OutputCX", width);
	config_set_uint(settings, "Video", bVerticalOutput ? "OutputCYV" : "OutputCY", height);
	config_set_uint(settings, "Video", "FPSType", 1);
	config_set_int(settings, "Video", "FPSInt", fps);

	if (toChangeCanvas) {
		config_set_uint(settings, "Video", bVerticalOutput ? "BaseCXV" : "BaseCX", width);
		config_set_uint(settings, "Video", bVerticalOutput ? "BaseCYV" : "BaseCY", height);
	}
	config_save(settings);
	mainView->ResetVideo();
	mainView->ResetOutputs();
	return true;
}

bool ResolutionGuidePage::setResolution(const Resolution &resolution, bool toChangeCanvas, bool bVerticalOutput)
{

	if (!setResolution(resolution.width, resolution.height, resolution.fps, toChangeCanvas, bVerticalOutput)) {
		return false;
	}
	if (resolution.bitrate > 0) {
		return setVideoBitrateAndKeyFrameInterval(resolution.bitrate, resolution.keyframeInterval);
	}
	return true;
}

OBSData GetDataFromJsonFile(const char *jsonFile);

bool ResolutionGuidePage::setVideoBitrateAndKeyFrameInterval(int bitrate, int keyframeInterval)
{
	if (pls_is_output_actived()) {
		return false;
	}
	auto mainView = PLSBasic::Get();
	auto settings = mainView->Config();
	config_set_uint(settings, "SimpleOutput", "VBitrate", bitrate);
	config_save(settings);
	OBSData streamEncSettings = GetDataFromJsonFile("streamEncoder.json");
	if (streamEncSettings == nullptr) {
		return false;
	}
	obs_data_set_int(streamEncSettings, "bitrate", bitrate);
	if (keyframeInterval > 20) {
		PLS_WARN("ResolutionGuidePage", "keyframeInterval can not > 20");
	}
	if (keyframeInterval != -1) {
		obs_data_set_int(streamEncSettings, "keyint_sec", keyframeInterval);
	}
	std::string fullPath(512, '\0');

	if (GetProfilePath(fullPath.data(), fullPath.size(), "streamEncoder.json") <= 0) {
		return false;
	}
	obs_data_save_json_safe(streamEncSettings, fullPath.c_str(), "tmp", "bak");

	return true;
}

void ResolutionGuidePage::showAlertOnSetResolutionFailed()
{
	CannotTipObject obj;
	obj.updateText();
	QMetaObject::invokeMethod(
		pls_get_main_view(), [obj]() { pls_alert_error_message(pls_get_main_view(), tr("Notice"), obj.mText); }, Qt::QueuedConnection);
}
