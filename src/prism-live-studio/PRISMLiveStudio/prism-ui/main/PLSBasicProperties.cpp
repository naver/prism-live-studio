
#include "PLSBasicProperties.hpp"
#include "frontend-api.h"
#include "libui.h"
#include "libutils-api.h"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "TextMotionTemplateDataHelper.h"
#include "qtimer.h"
#include "loading-event.hpp"
#include "obs-app.hpp"
#include "PLSPropertiesView.hpp"
#include <qtimer.h>
#include <qmetatype.h>
#include <qpushbutton.h>
#include "PLSMotionNetwork.h"
#include "PLSMotionFileManager.h"
#include "pls/pls-source.h"
#include "PLSAction.h"
#include "qt-display.hpp"
#include "PLSBasic.h"
#if defined(Q_OS_MACOS)
#include "mac/PLSPermissionHelper.h"
#endif

using namespace std;
using namespace common;
const auto PROPERTY_WINDOW_DEFAULT_W = 720;
const auto PROPERTY_WINDOW_DEFAULT_H = 710;

extern bool hasActivedChatChannel();
extern PlatformType viewerCountCheckPlatform();

static void checkViewerCountSourceTip(PLSBasicProperties *properties, OBSQTDisplay *preview)
{
	switch (viewerCountCheckPlatform()) {
	case PlatformType::Band:
		preview->showGuideText(QObject::tr("ViewerCountSource.Property.SinglePlatform.NoSupportChannel.Band"));
		break;
	case PlatformType::CustomRTMP:
		preview->showGuideText(QObject::tr("ViewerCountSource.Property.SinglePlatform.NoSupportChannel.RTMP"));
		break;
	case PlatformType::BandAndCustomRTMP:
		preview->showGuideText(QObject::tr("ViewerCountSource.Property.SinglePlatform.NoSupportChannel.BandRTMP"));
		break;
	case PlatformType::MpBand:
		properties->showToast(QObject::tr("ViewerCountSource.Property.SinglePlatform.NoSupportChannel.Band"));
		break;
	case PlatformType::MpCustomRTMP:
		properties->showToast(QObject::tr("ViewerCountSource.Property.SinglePlatform.NoSupportChannel.RTMP"));
		break;
	case PlatformType::MpBandAndCustomRTMP:
		properties->showToast(QObject::tr("ViewerCountSource.Property.SinglePlatform.NoSupportChannel.BandRTMP"));
		break;
	default:
		break;
	}
}

PLSBasicProperties::PLSBasicProperties(QWidget *parent, OBSSource source_, unsigned flag) : OBSBasicProperties(parent, source_), operationFlags(flag)
{

	//invoke async to ensure sub-sources were created
	QTimer::singleShot(0, this, [this]() { pls_source_properties_edit_start(source); });

	OBSBasicProperties::ui->windowSplitter->setObjectName(common::OBJECT_NAME_PROPERTY_SPLITTER);
	ui->windowSplitter->setMouseTracking(true);
	ui->windowSplitter->setContentsMargins(0, 0, 0, 0);

	ui->windowSplitter->setStretchFactor(1, -1);
	ui->windowSplitter->setChildrenCollapsible(false);

	ui->windowSplitter->setStretchFactor(0, 1);

	ui->buttonBox->setContentsMargins(30, 0, 30, 0);
	ui->buttonBox->setProperty("owner", "PLSBasicProperties");

	ui->preview->setCursor(Qt::ArrowCursor);
	ui->preview->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	ui->preview->setObjectName(common::OBJECT_NAME_PROPERTYVIEW);
	ui->preview->setMouseTracking(true);

	const char *id = obs_source_get_id(source);
	if (id) {
		PLS_INFO(PROPERTY_MODULE, "Property window for %s is openned", id);
		setProperty("sourceId", id);
	}

	int marginViewTop = pls_conditional_select(pls_is_in_str(id, common::PRISM_MOBILE_SOURCE_ID, common::PRISM_CHAT_SOURCE_ID, common::BGM_SOURCE_ID), 10, 0);
	ui->propertiesLayout->setContentsMargins(15, marginViewTop, 6, 0);

	setWidthResizeEnabled(false);

	connect(view, &PLSPropertiesView::OpenFilters, this, [this]() { emit OpenFilters(source); });
	connect(view, &PLSPropertiesView::OpenStickers, this, [this]() { emit OpenStickers(source); });
	connect(view, &PLSPropertiesView::OpenMusicButtonClicked, this, [this](OBSSource) { emit OpenMusicButtonClicked(); });
	connect(view, &PLSPropertiesView::okButtonControl, ui->buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::setEnabled);
	connect(view, &PLSPropertiesView::reloadOldSettings, this, &PLSBasicProperties::onReloadOldSettings);

	updatePropertiesOKButtonSignal.Connect(obs_source_get_signal_handler(source), "update_properties_ok_button_enable", PLSBasicProperties::UpdatePropertiesOkButtonEnable, this);

	pls_set_css(this, {"PLSBasicProperties", "PLSLoadingBtn"});
	initSize({PROPERTY_WINDOW_DEFAULT_W, PROPERTY_WINDOW_DEFAULT_H});

	if (pls_is_equal(id, PRISM_TEXT_TEMPLATE_ID)) {
		QTimer::singleShot(0, this, [this]() { AsyncLoadTextmotionProperties(); });
	}
	if (pls_is_equal(id, common::PRISM_MOBILE_SOURCE_ID)) {
		_customPreview();
	} else {
#if defined(Q_OS_MACOS)
		ui->windowSplitter->setSizes({DISPLAY_VIEW_DEFAULT_HEIGHT, 404});
#else
		ui->windowSplitter->setSizes({DISPLAY_VIEW_DEFAULT_HEIGHT, 364});
#endif
	}

	installEventFilter(this);
	if (pls_is_equal(id, PRISM_CHAT_SOURCE_ID) && obs_frontend_streaming_active() && !hasActivedChatChannel()) {
		ui->preview->showGuideText(tr("Chat.Property.ChatSource.NoSupportChannel.InStreaming"));
	} else if (pls_is_equal(id, PRISM_VIEWER_COUNT_SOURCE_ID)) {
		checkViewerCountSourceTip(this, ui->preview);
	}

#if defined(Q_OS_MACOS)
	PLSPermissionHelper::AVType avType;
	auto permissionStatus = PLSPermissionHelper::checkPermissionWithSource(source, avType);
	QMetaObject::invokeMethod(
		this,
		[avType, permissionStatus, this]() {
			PLSPermissionHelper::showPermissionAlertIfNeeded(avType, permissionStatus, this, [this]() {
				acceptClicked = true;
				reject();
			});
		},
		Qt::QueuedConnection);
#endif
}

PLSBasicProperties::~PLSBasicProperties()
{
	view->CheckValues();
	if (acceptClicked) {
		OBSData srcSettings = obs_source_get_settings(source);
		action::CheckPropertyAction(source, oldSettings, srcSettings, operationFlags);
		obs_data_release(srcSettings);
	}
}

static QString getLoadingString(const char *source_id)
{
	if (!source_id)
		return QString();

	if (0 == strcmp(source_id, OBS_DSHOW_SOURCE_ID))
		return QTStr("main.camera.loading.devicelist");

	return QString();
}

void PLSBasicProperties::ShowLoading()
{
	//"main.camera.loading.devicelist"
	HideLoading();
	m_pWidgetLoadingBG = pls_new<QWidget>(content());
	m_pWidgetLoadingBG->setObjectName("loadingBG");
	m_pWidgetLoadingBG->setGeometry(content()->geometry());
	m_pWidgetLoadingBG->show();

	m_pLoadingEvent = pls_new<PLSLoadingEvent>();

	auto layout = pls_new<QVBoxLayout>(m_pWidgetLoadingBG);
	auto loadingBtn = pls_new<QPushButton>(m_pWidgetLoadingBG);
	auto tips = pls_new<QLabel>(getLoadingString(obs_source_get_id(source)));
	tips->setWordWrap(true);
	tips->setAlignment(Qt::AlignCenter);
	tips->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	tips->setObjectName("loadingTipsLabel");
	layout->addStretch();
	layout->addWidget(loadingBtn, 0, Qt::AlignHCenter);
	layout->addWidget(tips);
	layout->addStretch();
	loadingBtn->setObjectName("loadingBtn");
	loadingBtn->show();

	m_pLoadingEvent->startLoadingTimer(loadingBtn);
}

void PLSBasicProperties::HideLoading()
{
	if (nullptr != m_pLoadingEvent) {
		m_pLoadingEvent->stopLoadingTimer();
		pls_delete(m_pLoadingEvent, nullptr);
	}

	if (nullptr != m_pWidgetLoadingBG) {
		m_pWidgetLoadingBG->hide();
		pls_delete(m_pWidgetLoadingBG, nullptr);
	}
}
void PLSBasicProperties::AsyncLoadTextmotionProperties()
{
	connect(
		TextMotionRemoteDataHandler::instance(), &TextMotionRemoteDataHandler::loadedTextmotionRes, this,
		[this](bool isSuccess) {
			HideLoading();
			auto propertiesView = dynamic_cast<PLSPropertiesView *>(view);
			if (propertiesView && isSuccess) {
				QTimer::singleShot(0, this, [propertiesView, this]() {
					if (propertiesView) {
						pls_get_text_motion_template_helper_instance()->initTemplateButtons();
						propertiesView->RefreshProperties();
					}
					if (ui->preview)
						ui->preview->show();
				});
			}
		},
		Qt::QueuedConnection);

	//load textmotion resource,if not exist,download res
	if (!TextMotionRemoteDataHandler::instance()->initTMData()) {
		ShowLoading();
	}
}

void PLSBasicProperties::ShowMobileNotice()
{
	const char *id = obs_source_get_id(source);
	if (!id || 0 != strcmp(PRISM_LENS_MOBILE_SOURCE_ID, id)) {
		return;
	}

	bool checked = config_get_bool(App()->GlobalConfig(), "General", "NotRemindScanQRcode");
	if (checked) {
		return;
	}

	OBSDataAutoRelease data = obs_source_get_private_settings(source);
	bool show = obs_data_get_bool(data, "showMobileNotice");
	if (!show) {

#ifdef Q_OS_MACOS
		//Installing the Mac version of the lens app did not run once
		bool lensHasRun = pls_libutil_api_mac::pls_is_lens_has_run();
		if (!lensHasRun) {
			return;
		}
#endif // Q_OS_MACOS

		std::optional<int> timeout = std::optional<int>();
		PLSAlertView::Result res = PLSAlertView::information(this, QTStr("Confirm"), QTStr("main.property.mobile.scan.QRcode.tips"), QTStr("main.property.prism.dont.show.again"),
								     {{PLSAlertView::Button::Open, QTStr("main.property.prism.mobile.open")}, {PLSAlertView::Button::No, QTStr("Close")}},
								     PLSAlertView::Button::No, timeout, QMap<QString, QVariant>());
		if (res.isChecked) {
			config_set_bool(App()->GlobalConfig(), "General", "NotRemindScanQRcode", true);
			config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
		}
		if (res.button == PLSAlertView::Button::Open) {
			QStringList arguments{"--display_control=top", "--active_tab=mobile", "--tip_key=mobile_connect_tip", "--select_mobile_cam=2"};
			int outputCamIndex = view->getPrismLensOutputIndex();
			if (outputCamIndex != -1) {
				arguments << QString("--output_cam=%1").arg(outputCamIndex);
			}
			pls_open_cam_studio(arguments, this);
		}
		obs_data_set_bool(data, "showMobileNotice", true);
	}
}

void PLSBasicProperties::ShowPrismLensNaverRunNotice(bool isMobileSource)
{
	QString content = QTStr("main.property.lens.launch");
	if (isMobileSource) {
		content = QTStr("main.property.mobile.launch");
	}

	PLSAlertView::Button button =
		PLSAlertView::information(this, QTStr("Confirm"), content, {{PLSAlertView::Button::Open, QTStr("main.property.prism.mobile.open")}, {PLSAlertView::Button::No, QTStr("Close")}});
	if (button == PLSAlertView::Button::Open) {
		QStringList arguments{"--display_control=top"};
		int outputCam = isMobileSource ? 2 : 0;
		arguments << QString("--output_cam=%1").arg(outputCam);
		pls_open_cam_studio(arguments, this);
		accept();
	}
}

void PLSBasicProperties::UpdatePropertiesOkButtonEnable(void *data, calldata_t *params)
{
	pls_unused(params);
	auto source = (obs_source_t *)calldata_ptr(params, "source");
	if (!source) {
		return;
	}
	bool enable = calldata_bool(params, "enable");
	QMetaObject::invokeMethod(static_cast<PLSBasicProperties *>(data)->view, "okButtonControl", Qt::QueuedConnection, Q_ARG(bool, enable));
}

static bool isValidBackgroundResource(const QString &itemId)
{
	const PLSMotionNetwork *network = PLSMotionNetwork::instance();
	const PLSMotionFileManager *fileManager = PLSMotionFileManager::instance();

	MotionData md;
	if (network->findPrismOrFree(md, itemId)) {
		return fileManager->isValidMotionData(md, true);
	} else if (fileManager->findMy(md, itemId)) {
		return fileManager->isValidMotionData(md, true);
	}
	return false;
}

void PLSBasicProperties::onReloadOldSettings() const
{
	const char *id = obs_source_get_id(source);
	if (!id || !id[0]) {
		return;
	}

	if (!strcmp(id, common::PRISM_BACKGROUND_TEMPLATE_SOURCE_ID)) {
		const char *item_id = obs_data_get_string(oldSettings, "item_id");
		if (!item_id || !item_id[0] || !isValidBackgroundResource(QString::fromUtf8(item_id))) {
			obs_data_set_bool(oldSettings, "prism_resource", false);
			obs_data_set_string(oldSettings, "item_id", "");
			obs_data_set_int(oldSettings, "item_type", 0);
			obs_data_set_string(oldSettings, "file_path", "");
			obs_data_set_string(oldSettings, "image_file_path", "");
			obs_data_set_string(oldSettings, "thumbnail_file_path", "");
		}
	}
}

void PLSBasicProperties::reject()
{
	dialogClosedToSendNoti();
}

void PLSBasicProperties::accept()
{
	dialogClosedToSendNoti();
}

void PLSBasicProperties::dialogClosedToSendNoti()
{
	PLS_INFO(PROPERTY_MODULE, "Property window will close to send notifications");
	if (!acceptClicked && (CheckSettings() != 0)) {
		if (!ConfirmQuit()) {
			return;
		}
	}

	//RenJinbo/add is save button clicked
	pls_source_properties_edit_end(source, m_isSaveClick);

	Cleanup();
	emit AboutToClose();
	done(0);
}

// MARK: - preview

void PLSBasicProperties::_customPreview()
{
	this->ui->preview->setFixedSize(QSize(720, 250));

	imagePreview = pls_new<QLabel>();
	imagePreview->setFixedSize(QSize(720, 250));
	imagePreview->setScaledContents(true);
	imagePreview->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	imagePreview->setStyleSheet("QLabel { background-color : #151515; color : blue; }");
	ui->verticalLayout_7->addWidget(imagePreview);
	ui->verticalLayout_7->setContentsMargins(0, 0, 0, 0);

	this->ui->preview->setVisible(true);
	this->imagePreview->setVisible(false);
}

void PLSBasicProperties::updatePreview()
{
	OBSDataAutoRelease data = obs_source_get_private_settings(source.Get());
	auto isPreviewImagePathInProperty = obs_data_get_bool(data, "isPreviewImagePathInProperty");

	this->ui->preview->setVisible(!isPreviewImagePathInProperty);
	this->imagePreview->setVisible(isPreviewImagePathInProperty);
	if (isPreviewImagePathInProperty) {
		QImage image;
		auto previewImagePathInProperty = obs_data_get_string(data, "previewImagePathInProperty");
		auto isPreviewImageScaled = obs_data_get_bool(data, "isPreviewImageScaled");
		imagePreview->setScaledContents(isPreviewImageScaled);
		image.load(previewImagePathInProperty);
		imagePreview->setPixmap(QPixmap::fromImage(image));
		imagePreview->setVisible(true);
	}
}
void PLSBasicProperties::showToast(const QString &message)
{
	if (toast && toastLabel && toastButton) {
		setToastMessage(message);
		return;
	}

	toast = pls_new<QWidget>(this, Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
	toast->setAttribute(Qt::WA_TranslucentBackground, true);

	QHBoxLayout *toastLayout = pls_new<QHBoxLayout>(toast);
	toastLayout->setContentsMargins(0, 0, 0, 0);

	QFrame *toastFrame = pls_new<QFrame>(toast);
	toastFrame->setObjectName("toastFrame");
	toastLayout->addWidget(toastFrame);

	QHBoxLayout *layout = pls_new<QHBoxLayout>(toastFrame);
	layout->setContentsMargins(32, 11, 32, 12);

	toastLabel = pls_new<QLabel>(toastFrame);
	toastLabel->setObjectName("toastLabel");
	layout->addWidget(toastLabel);

	toastButton = pls_new<QPushButton>(toastFrame);
	toastButton->setObjectName("toastButton");
	connect(toastButton, &QPushButton::clicked, toastFrame, &QFrame::hide);
	setToastMessage(message);

	QMetaObject::invokeMethod(
		this, [this]() { updateToastGeometry(); }, Qt::QueuedConnection);
}

void PLSBasicProperties::setToastMessage(const QString &message)
{
	if (!toast || !toastLabel || !toastButton) {
		showToast(message);
		return;
	}

	toastLabel->setText(message);
	toast->show();

	updateToastGeometry();
}

void PLSBasicProperties::updateToastGeometry()
{
	pls_flush_style_recursive(toast);
	toast->adjustSize();

	updateToastPosition(geometry());
}

void PLSBasicProperties::updateToastPosition(const QRect &geometry)
{
	toastButton->move(toast->width() - toastButton->width() - 5, 5);
#if defined(Q_OS_WIN)
	int offset = 49;
#elif defined(Q_OS_MACOS)
	int offset = 9;
#endif

	toast->move(geometry.left() + (geometry.width() - toast->width()) / 2, geometry.top() + offset);
}

void PLSBasicProperties::PropertyUpdateNotify(void *data, calldata_t *params)
{
	pls_used(params);
	const char *cname = nullptr;
	calldata_get_ptr(params, "name", (void *)&cname);
	QString name = cname && cname[0] ? QString::fromUtf8(cname) : QString();
	QMetaObject::invokeMethod(static_cast<PLSBasicProperties *>(data)->view, "PropertyUpdateNotify", Qt::QueuedConnection, Q_ARG(QString, name));
}

void PLSBasicProperties::resizeEvent(QResizeEvent *event)
{
	OBSBasicProperties::resizeEvent(event);

	if (toast && toastLabel && toastButton) {
		updateToastGeometry();
	}
}
void PLSBasicProperties::moveEvent(QMoveEvent *event)
{
	OBSBasicProperties::moveEvent(event);

	if (toast && toastLabel && toastButton) {
		updateToastGeometry();
	}
}

void PLSBasicProperties::showEvent(QShowEvent *event)
{
	OBSBasicProperties::showEvent(event);
	auto id = obs_source_get_id(source);
#if defined(Q_OS_WIN)
	bool isDshow = pls_is_equal(id, OBS_DSHOW_SOURCE_ID);
#elif defined(Q_OS_MACOS)
	bool isDshow = pls_is_equal(id, OBS_DSHOW_SOURCE_ID_V2) || pls_is_equal(id, OBS_DSHOW_SOURCE_ID);
#endif
	if (isDshow) {
		int outputCamIndex = view->getPrismLensOutputIndex();
		if (outputCamIndex == -1) {
			return;
		}
		if (PLSBasic *basic = PLSBasic::instance(); basic) {
			QString program;
			bool installed = basic->CheckCamStudioInstalled(program);
			if (!installed) {
				return;
			}
		}
		QStringList arguments;
		arguments << "--display_control=keep";
		arguments << QString("--output_cam=%1").arg(outputCamIndex);
		pls_open_cam_studio(arguments, this);
		return;
	}

	if (!pls_is_equal(id, PRISM_LENS_SOURCE_ID) && !pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID)) {
		return;
	}
	QTimer::singleShot(0, this, [this, id]() {
		if (PLSBasic *basic = PLSBasic::instance(); basic) {
			QString program;
			bool installed = basic->CheckCamStudioInstalled(program);
			if (!installed) {
				QString content = QTStr("main.property.lens.install");
				if (pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID)) {
					content = QTStr("main.property.mobile.install");
				}
				basic->ShowInstallCamStudioTips(this, QTStr("Alert.Title"), content, QTStr("Main.cam.install.now"), QTStr("main.property.lens.later"));
				return;
			}
		}
		//Installing the Mac version of the lens app did not run once
#ifdef Q_OS_MACOS
		OBSDataAutoRelease data = obs_source_get_private_settings(source);
		bool show = obs_data_get_bool(data, "showLensRunNotice");
		bool lensHasRun = pls_libutil_api_mac::pls_is_lens_has_run();
		if (!lensHasRun && !show) {
			ShowPrismLensNaverRunNotice(pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID));
			obs_data_set_bool(data, "showLensRunNotice", true);
			return;
		}
#endif // Q_OS_MACOS

		QStringList arguments;
		arguments << "--display_control=keep";

		int outputCamIndex = view->getPrismLensOutputIndex();
		if (outputCamIndex != -1) {
			arguments << QString("--output_cam=%1").arg(outputCamIndex);
		}

		pls_open_cam_studio(arguments, this);

		ShowMobileNotice();
	});
}
