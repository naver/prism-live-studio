#include "ResolutionGuidePage.h"
#include "ui_ResolutionGuidePage.h"
#include "ResolutionGuideItem.h"
#include "ChannelConst.h"
#include <QFile>
#include <QJsonDocument>
#include "PLSDpiHelper.h"
#include "alert-view.hpp"
#include "liblog.h"
#include "window-basic-main.hpp"
#include <QRegularExpression>
#include "main-view.hpp"
#include "ResolutionConst.h"
#include "PLSChannelDataAPI.h"
#include "ChannelCommonFunctions.h"
#include "PLSSyncServerManager.hpp"
#include "../main-view.hpp"

const char ResolutionGroup[] = "resolution";
const char ResolutionGeo[] = "resolutionGeo";

QVariantList ResolutionGuidePage::mResolutions = QVariantList();

ResolutionGuidePage::ResolutionGuidePage(QWidget *parent, PLSDpiHelper dpiHelper) : PLSDialogView(parent), ui(new Ui::ResolutionGuidePage)
{
	dpiHelper.setCss(this, {PLSCssIndex::ResolutionGuidePage});
	ui->setupUi(this->content());
	this->setHasCaption(false);
	this->setIsMoveInContent(true);
	QMetaObject::connectSlotsByName(this);

	initialize();
}

ResolutionGuidePage::~ResolutionGuidePage()
{
	emit visibilityChanged(false);
	delete ui;
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
	page->setWindowFlags(Qt::Dialog | Qt::NoDropShadowWindowHint | Qt::FramelessWindowHint);
	page->setAttribute(Qt::WA_AlwaysStackOnTop);
	page->setAttribute(Qt::WA_DeleteOnClose);
	page->connectMainView();

	return page;
}

void ResolutionGuidePage::saveSettings()
{
	QByteArray setting = this->saveGeometry();
	config_set_string(App()->GlobalConfig(), ResolutionGroup, ResolutionGeo, setting.toBase64().constData());
	config_save(App()->GlobalConfig());
}

const QString ResolutionGuidePage::getPreferResolutionStringOfPlatform(const QString &platform)
{
	auto isMatchPlatform = [&](const QVariant &resolutionData) {
		auto varMap = resolutionData.toMap();
		if (varMap.isEmpty()) {
			return false;
		}
		auto dataPlatform = varMap.value(ChannelData::g_platformName).toString();
		if (platform.compare(dataPlatform, Qt::CaseInsensitive) != 0) {
			return false;
		}

		return true;
	};
	const auto &resolutions = getResolutionsList();
	auto it = std::find_if(resolutions.cbegin(), resolutions.cend(), isMatchPlatform);
	if (it == resolutions.cend()) {
		return "(1280x720/30FPS)";
	}
	return it->toMap().value(ResolutionConstSpace::g_PreferResolution).toString();
}

const Resolution ResolutionGuidePage::getPreferResolutionOfPlatform(const QString &platform)
{
	return parserStringOfResolution(getPreferResolutionStringOfPlatform(platform));
}

const Resolution ResolutionGuidePage::parserStringOfResolution(const QString &resolution)
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
		auto updateItemState = [item]() {
			bool isEnbaled = (PLSCHANNELS_API->currentBroadcastState() == ChannelData::ReadyState) && (PLSCHANNELS_API->currentReocrdState() == ChannelData::RecordReady);
			item->setLinkEnanled(isEnbaled);
		};

		connect(PLSCHANNELS_API, &PLSChannelDataAPI::liveStateChanged, item, updateItemState);
		connect(PLSCHANNELS_API, &PLSChannelDataAPI::recordingChanged, item, updateItemState);
		updateItemState();
	}
	//loadSettings();
}

extern const QString translatePlatformName(const QString &platformName);

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

	if (isUserNeed(platform, resolution)) {
		auto resolutionData = parserStringOfResolution(resolution);
		setResolution(resolutionData, true);
		emit resolutionUpdated();
		if (onUpdateResolution) {
			onUpdateResolution();
		}

		this->accept();
	}
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
	case QEvent::Show: {
		emit visibilityChanged(true);
	} break;
	case QEvent::Hide: {
		emit visibilityChanged(false);
		this->setModal(false);
		saveSettings();
	} break;
	case QEvent::Close: {
		this->accept();
		return true;
	} break;
	case QEvent::Resize: {
		saveSettings();
	} break;
	default:

		break;
	}
	return PLSDialogView::event(event);
}

void ResolutionGuidePage::connectMainView()
{
	PLSMainView *View = dynamic_cast<PLSMainView *>(pls_get_main_view());
	QObject::connect(this, &ResolutionGuidePage::visibilityChanged, View, &PLSMainView::toggleResolutionButton, Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));

	connect(
		View, &PLSMainView::isshowSignal, this,
		[this](bool visible) {
			if (!visible) {
				this->close();
			}
		},
		Qt::DirectConnection);
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
		parent->close();
		pls_get_main_view()->activateWindow();
	});
}

void ResolutionGuidePage::showIntroduceTip(QWidget *parent, const QString &platformName)
{

	auto frame = new QFrame(parent, Qt::SubWindow);
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

	lay->setMargin(0);

	auto deleteFrame = [=]() {
		frame->hide();
		frame->deleteLater();
	};
	connect(btn, &QAbstractButton::clicked, frame, deleteFrame);

	PLSDpiHelper helper;
	auto calculate = [=](double dpi) {
		auto tipSize = QSize(parent->size().width() - PLSDpiHelper::calculate(parent, 50), PLSDpiHelper::calculate(parent, 61));
		frame->resize(tipSize);
		frame->move(PLSDpiHelper::calculate(dpi, QPoint(25, 95 + 15)));
	};
	helper.notifyDpiChanged(frame, calculate);
	calculate(PLSDpiHelper::getDpi(parent));
	frame->show();
	QTimer::singleShot(5000, frame, deleteFrame);
}

void ResolutionGuidePage::checkResolution(QWidget *parent, const QString &uuid)
{
	if (PLSCHANNELS_API->isLiving()) {
		return;
	}
	if (PLSCHANNELS_API->getChannelUserStatus(uuid) != ChannelData::Enabled) {
		return;
	}
	auto platformInfo = PLSCHANNELS_API->getChannelInfo(uuid);
	auto platformName = getInfo(platformInfo, ChannelData::g_platformName);
	auto channelTyp = getInfo(platformInfo, ChannelData::g_data_type, ChannelData::ChannelType);

	if (isCurrentResolutionFitableFor(platformName, channelTyp)) {
		return;
	}
	showIntroduceTip(parent, translatePlatformName(platformName));
}

bool ResolutionGuidePage::isCurrentResolutionFitableFor(const QString &platform, int channelType)
{
	static const auto &platformResolution = PLSSyncServerManager::instance()->platformFPSMap();
	static const auto &rtmpResolution = PLSSyncServerManager::instance()->platformFPSMap();

	QVariantMap searchMap;

	const QString searchKey = platform.toLower().remove(QRegularExpression("\\W+"));
	if (channelType == ChannelData::RTMPType) {
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

	auto platformResolutionMap = searchMap.value(searchKey).toMap();
	if (platformResolutionMap.contains(searchResolution)) {
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

void ResolutionGuidePage::setVisibleOfGuide(QWidget *parent, UpdateCallback callback, IsUserNeed agreeFun, bool isVisible)
{
	if (parent == nullptr) {
		parent = pls_get_main_view();
	}

	auto page = createInstance(parent);

	auto setvisibleOf = [=](bool isVisible) {
		if (isVisible) {

			page->setUpdateResolutionFunction(callback);
			page->setAgreeFunction(agreeFun);
			page->exec();

			return;
		}
		page->reject();
	};

	setvisibleOf(isVisible);
}

void ResolutionGuidePage::setUsingPlatformPreferResolution(const QString &platform)
{
	auto resolution = getPreferResolutionOfPlatform(platform);
	setResolution(resolution);
}

void ResolutionGuidePage::setResolution(int width, int height, int fps, bool toChangeCanvas)
{
	auto mainView = PLSBasic::Get();
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
}

void ResolutionGuidePage::setResolution(const Resolution &resolution, bool toChangeCanvas)
{
	setResolution(resolution.width, resolution.height, resolution.fps, toChangeCanvas);
}
