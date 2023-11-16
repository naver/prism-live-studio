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

QString ResolutionGuidePage::getPreferResolutionStringOfPlatform(const QString &platform)
{
	auto isMatchPlatform = [&](const QVariant &resolutionData) {
		auto varMap = resolutionData.toMap();
		if (varMap.isEmpty()) {
			return false;
		}

		if (auto dataPlatform = varMap.value(ChannelData::g_platformName).toString(); platform.compare(dataPlatform, Qt::CaseInsensitive) != 0) {
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
		if (mResolutions.isEmpty()) {
			PLSGpopData::useDefaultValues(name2str(ResolutionGuide), mResolutions);
		}
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
	}
	if (mText.isEmpty() && outputState.testFlag(OutputState::VirtualCamIsOn)) {

		mText = tr("Resolution.VirtualCamIsActived");
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
	auto items = this->findChildren<ResolutionGuideItem *>();
	for (auto item : items) {
		item->setLinkEnanled(isCanChange);
	}

	if (!mCannotTip.isValid) {
		mCannotTip = createCannotTipForWidget(this, QSize(660, 0), QPoint(30, -90));
	}

	mCannotTip.updateUI();

	QTimer::singleShot(500, this, [this]() { updateSpace(mCannotTip.mTip && mCannotTip.mTip->isVisibleTo(this)); });
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
	auto ret = PLSAlertView::question(nullptr, tr("Confirm"), questionContent, {{PLSAlertView::Button::Yes, tr("ResolutionGuide.ApplyNowBtn")}, {PLSAlertView::Button::Cancel, tr("Cancel")}});

	return (ret == PLSAlertView::Button::Yes);
}

void ResolutionGuidePage::onUserSelectedResolution(const QString &txt)
{
	if (PLSCHANNELS_API->isLivingOrRecording()) {
		return;
	}

	PLS_UI_STEP("ResolutionGuidePage", "ResolutionGuidePage:", QString("user select resolution " + txt).toUtf8().constData());
	auto infoLst = txt.split(":");
	const auto &platform = infoLst[0];
	const auto &resolution = infoLst[1];

	if (!isUserNeed(platform, resolution)) {
		return;
	}
	auto resolutionData = parserStringOfResolution(resolution);
	if (platform.contains(NAVER_SHOPPING_LIVE, Qt::CaseInsensitive)) {
		resolutionData.bitrate = 2500;
	}

	if (!setResolution(resolutionData, true)) {
		emit sigSetResolutionFailed();
		return;
	}
	emit resolutionUpdated();
	if (onUpdateResolution) {
		onUpdateResolution();
	}

	this->accept();
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
	auto platformName = getInfo(platformInfo, ChannelData::g_platformName);
	if (PLSCHANNELS_API->getChannelUserStatus(uuid) != ChannelData::Enabled && channelTyp == ChannelData::ChannelDataType::ChannelType) {
		return;
	}

	checkResolutionForPlatform(parent, platformName, channelTyp);
}

void ResolutionGuidePage::checkResolutionForPlatform(QWidget *parent, const QString &platformName, int channelTyp)
{
	if (isCurrentResolutionFitableFor(platformName, channelTyp)) {
		return;
	}
	showIntroduceTip(parent, translatePlatformName(platformName));
}

bool ResolutionGuidePage::isCurrentResolutionFitableFor(const QString &platform, int channelType)
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

	auto mainView = PLSBasic::Get();
	auto settings = mainView->Config();
	auto width = config_get_uint(settings, "Video", "OutputCX");
	auto height = config_get_uint(settings, "Video", "OutputCY");
	auto fps = config_get_int(settings, "Video", "FPSInt");

	auto searchResolution = QString("%1x%2").arg(width).arg(height);

	if (auto platformResolutionMap = searchMap.value(searchKey).toMap(); platformResolutionMap.contains(searchResolution)) {
		auto validFps = platformResolutionMap.value(searchResolution);
		if (validFps.canConvert<QString>() && validFps.toString() == QString::number(fps)) {
			return true;
		}

		if (validFps.canConvert<QStringList>() && validFps.toStringList().contains(QString::number(fps))) {
			return true;
		}
	}

	return false;
}

void ResolutionGuidePage::setVisibleOfGuide(QWidget *parent, const UpdateCallback &callback, const IsUserNeed &agreeFun, bool isVisible)
{
	if (parent == nullptr) {
		parent = pls_get_main_view();
	}

	auto page = createInstance(parent);

	auto setvisibleOf = [page, callback, agreeFun](bool isVisibleL) {
		if (isVisibleL) {

			page->setUpdateResolutionFunction(callback);
			page->setAgreeFunction(agreeFun);
			page->exec();

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

bool ResolutionGuidePage::setResolution(int width, int height, int fps, bool toChangeCanvas)
{
	if (pls_is_output_actived()) {
		return false;
	}
	auto mainView = PLSBasic::instance();
	auto settings = mainView->Config();

	config_set_uint(settings, "Video", "OutputCX", width);
	config_set_uint(settings, "Video", "OutputCY", height);
	config_set_uint(settings, "Video", "FPSType", 1);
	config_set_int(settings, "Video", "FPSInt", fps);

	if (toChangeCanvas) {
		config_set_uint(settings, "Video", "BaseCX", width);
		config_set_uint(settings, "Video", "BaseCY", height);
	}
	config_save(settings);
	mainView->ResetVideo();
	mainView->showEncodingInStatusBar();
	mainView->ResetOutputs();
	return true;
}

bool ResolutionGuidePage::setResolution(const Resolution &resolution, bool toChangeCanvas)
{

	if (!setResolution(resolution.width, resolution.height, resolution.fps, toChangeCanvas)) {
		return false;
	}
	if (resolution.bitrate > 0) {
		return setVideoBitrate(resolution.bitrate);
	}
	return true;
}

OBSData GetDataFromJsonFile(const char *jsonFile);

bool ResolutionGuidePage::setVideoBitrate(int bitrate)
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
