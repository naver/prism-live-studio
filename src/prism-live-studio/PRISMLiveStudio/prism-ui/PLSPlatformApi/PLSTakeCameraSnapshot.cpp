#include "PLSTakeCameraSnapshot.h"
#include "ui_PLSTakeCameraSnapshot.h"

#include <quuid.h>
#include <QSignalBlocker>

#include "properties-view.hpp"
#include "display-helpers.hpp"
#include "ChannelCommonFunctions.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "window-basic-properties.hpp"
#include "loading-event.hpp"
#include "PLSTakePhotoHelper.h"
#include <QResizeEvent>
#include "log/log.h"
#include "utils-api.h"
#include "PLSBasicProperties.hpp"
#include "PLSComboBox.h"
#include "qt-display.hpp"
#include "PLSGetPropertiesThread.h"
#include "util/threading.h"
#if defined(Q_OS_MACOS)
#include "mac/PLSPermissionHelper.h"
#endif
#include <pls/pls-obs-api.h>
#include <util/platform.h>
#include "PLSAlertView.h"
#include "PLSCommonFunc.h"

#include <QStandardItemModel>

using namespace common;

extern QString getTempImageFilePath(const QString &suffix);

namespace {

QString captureImage(uint32_t width, uint32_t height, enum gs_color_format format, uint8_t **data, const uint32_t *linesize)
{
	if (format == gs_color_format::GS_BGRA) {
		uchar *buffer = pls_malloc<uchar>(size_t(width) * height * 4);
		if (!buffer) {
			return QString();
		}

		memset(buffer, 0, size_t(width) * height * 4);

		if (width * 4 == linesize[0]) {
			memmove(buffer, data[0], size_t(linesize[0]) * height);
		} else {
			for (uint32_t i = 0; i < height; i++) {
				memmove(buffer + size_t(i) * width * 4, data[0] + size_t(i) * linesize[0], size_t(width) * 4);
			}
		}

		QImage image(buffer, width, height, QImage::Format_ARGB32);

		QString cropedImageFile1 = getTempImageFilePath(".bmp");
		image.save(cropedImageFile1, "BMP");

		pls_free(buffer);
		return cropedImageFile1;
	}
	return QString();
}

}

#if defined(Q_OS_WINDOWS)
PLSTakeCameraSnapshotTakeTimerMask::PLSTakeCameraSnapshotTakeTimerMask(QWidget *parent) : QDialog(parent, Qt::Tool | Qt::FramelessWindowHint)
{
	setObjectName("PLSTakeCameraSnapshotTakeTimerMask");
	setAttribute(Qt::WA_TranslucentBackground);

	maskLabel = pls_new<QLabel>(this);
	maskLabel->setObjectName("maskLabel");
	maskLabel->setAlignment(Qt::AlignCenter);
}

void PLSTakeCameraSnapshotTakeTimerMask::start(int ctime_)
{
	this->ctime = ctime_;

	maskLabel->setText(QString::number(ctime));
	show();

	QTimer::singleShot(1000, this, [this]() {
		PLS_INFO("PLSTakeCameraSnapshotTakeTimerMask", "single shot timer triggered for PLSTakeCameraSnapshotTakeTimerMask start");
		if (ctime > 1) {
			start(ctime - 1);
		} else if (ctime >= 0) {
			emit timeout();
		}
	});
}

void PLSTakeCameraSnapshotTakeTimerMask::stop()
{
	ctime = -1;
	hide();
}

bool PLSTakeCameraSnapshotTakeTimerMask::event(QEvent *event)
{
	if (event->type() == QEvent::Resize) {
		maskLabel->setGeometry(QRect(QPoint(0, 0), dynamic_cast<QResizeEvent *>(event)->size()));
	}

	return QDialog::event(event);
}

#else

static OBSSource CreateLabel(const char *name, size_t h)
{
	OBSDataAutoRelease settings = obs_data_create();
	OBSDataAutoRelease font = obs_data_create();

	std::string text = name;

	obs_data_set_string(font, "face", "Arial");
	obs_data_set_int(font, "flags", OBS_FONT_BOLD);
	obs_data_set_int(font, "size", h);

	obs_data_set_obj(settings, "font", font);
	obs_data_set_string(settings, "text", text.c_str());
	obs_data_set_bool(settings, "outline", false);
	obs_data_set_int(settings, "color1", 0x7FFFFFFF);
	obs_data_set_int(settings, "color2", 0x7FFFFFFF);

	const char *text_source_id = "text_ft2_source";

	OBSSourceAutoRelease txtSource = obs_source_create_private(text_source_id, name, settings);

	return txtSource.Get();
}

PLSTakeCameraSnapshotTakeTimerMask::PLSTakeCameraSnapshotTakeTimerMask(QObject *parent) : QObject(parent)
{
	setObjectName("PLSTakeCameraSnapshotTakeTimerMask");
	count = -1;
	display = nullptr;
	sources = std::vector<OBSSource>();
}

PLSTakeCameraSnapshotTakeTimerMask::~PLSTakeCameraSnapshotTakeTimerMask()
{
	this->stop();
}

void PLSTakeCameraSnapshotTakeTimerMask::start(obs_display_t *display, int count)
{
	if (!display || count < 0)
		return;

	this->stop();

	for (int i = 1; i <= count; i++) {
		sources.push_back(CreateLabel(std::to_string(i).c_str(), 95));
	}

	this->count = count;
	this->display = display;
	obs_display_add_draw_callback(display, PLSTakeCameraSnapshotTakeTimerMask::draw, this);

	this->start(count);
}

void PLSTakeCameraSnapshotTakeTimerMask::start(int count)
{
	if (count < 0)
		return;

	this->count = count;
	QPointer<PLSTakeCameraSnapshotTakeTimerMask> qThis = this;
	QTimer::singleShot(1000, this, [qThis, count]() {
		PLS_INFO("PLSTakeCameraSnapshotTakeTimerMask", "single shot timer triggered for PLSTakeCameraSnapshotTakeTimerMask start");
		if (qThis == nullptr || qThis.isNull())
			return;
		
		if (count > 1) {
			qThis->start(count - 1);
		} else {
			if (qThis->display) {
				obs_display_remove_draw_callback(qThis->display, PLSTakeCameraSnapshotTakeTimerMask::draw, qThis.get());
				qThis->display = nullptr;
			}
			emit qThis->timeout();
		}
	});
}

void PLSTakeCameraSnapshotTakeTimerMask::stop()
{
	this->count = -1;

	if (display) {
		obs_display_remove_draw_callback(display, PLSTakeCameraSnapshotTakeTimerMask::draw, this);
		this->display = nullptr;
	}

	sources.clear();
}

void PLSTakeCameraSnapshotTakeTimerMask::draw(void *data, uint32_t baseWidth, uint32_t baseHeight)
{
	auto self = static_cast<PLSTakeCameraSnapshotTakeTimerMask *>(data);
	if (!self || self->count < 0 || self->count > self->sources.size())
		return;

	OBSSource source = self->sources[self->count - 1];
	
	uint32_t newCX = baseWidth * 106 / 900;
	uint32_t newCY = baseHeight * 182 / 588;
	uint32_t sourceWidth = std::max(obs_source_get_width(source), 1u);
	uint32_t sourceHeight = std::max(obs_source_get_height(source), 1u);
	uint32_t x = (uint32_t)((baseWidth - newCX) / 2.0);
	uint32_t y = (uint32_t)((baseHeight - newCY) / 2.0);

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0, sourceWidth, 0, sourceHeight, -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);

	obs_source_video_render(source);

	gs_viewport_pop();
	gs_projection_pop();
}

#endif

PLSTakeCameraSnapshot::PLSTakeCameraSnapshot(QString &camera_, QWidget *parent) : PLSDialogView(parent), camera(camera_)
{
	ui = pls_new<Ui::PLSTakeCameraSnapshot>();

	pls_add_css(this, {"PLSLoadingBtn", "PLSTakeCameraSnapshot"});
	setResizeEnabled(false);

	timerMask = pls_new<PLSTakeCameraSnapshotTakeTimerMask>(this);
	connect(timerMask, &PLSTakeCameraSnapshotTakeTimerMask::timeout, this, &PLSTakeCameraSnapshot::onTimerEnd);
#if defined(Q_OS_WINDOWS)
	timerMask->hide();
#endif
	setupUi(ui);
	ui->preview->installEventFilter(this);
	ui->preview->setMinimumSize({20, 150});
	ui->preview->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	initSize({510, 521});

	cameraUninitMask = pls_new<QFrame>(content());
	cameraUninitMask->setObjectName("cameraUninitMask");

	QVBoxLayout *cameraUninitMaskLayout = pls_new<QVBoxLayout>(cameraUninitMask);
	cameraUninitMaskLayout->setContentsMargins(0, 0, 0, 0);
	cameraUninitMaskLayout->setSpacing(15);

	QLabel *cameraUninitMaskIcon = pls_new<QLabel>(cameraUninitMask);
	cameraUninitMaskIcon->setObjectName("cameraUninitMaskIcon");
	cameraUninitMaskText = pls_new<QLabel>(cameraUninitMask);
	cameraUninitMaskText->setObjectName("cameraUninitMaskText");
	cameraUninitMaskText->setAlignment(Qt::AlignCenter);
	cameraUninitMaskText->setText(tr("TakeCameraSnapshot.Wait"));
	cameraUninitMaskText->setWordWrap(true);
	cameraUninitMaskLayout->addStretch(1);
	cameraUninitMaskLayout->addWidget(cameraUninitMaskIcon, 0, Qt::AlignHCenter);
	cameraUninitMaskLayout->addWidget(cameraUninitMaskText, 0/*, Qt::AlignHCenter*/);
	cameraUninitMaskLayout->addStretch(1);

	QLabel *cameraSettingButtonIcon = pls_new<QLabel>(ui->cameraSettingButton);
	cameraSettingButtonIcon->setObjectName("cameraSettingButtonIcon");
	QHBoxLayout *cameraSettingButtonLayout = pls_new<QHBoxLayout>(ui->cameraSettingButton);
	cameraSettingButtonLayout->setContentsMargins(0, 0, 0, 0);
	cameraSettingButtonLayout->addWidget(cameraSettingButtonIcon, 0, Qt::AlignCenter);

	auto addDrawCallback = [this]() { obs_display_add_draw_callback(ui->preview->GetDisplay(), PLSTakeCameraSnapshot::drawPreview, this); };
	connect(ui->preview, &OBSQTDisplay::DisplayCreated, addDrawCallback);
	ui->preview->show();

	cameraUninitMaskText->setText(tr("TakeCameraSnapshot.NoDevice"));
	ui->cameraList->addItem(tr("TakeCameraSnapshot.NoDevice"));
	ui->cameraList->setEnabled(false);
	ui->cameraSettingButton->setEnabled(false);
	ui->okButton->setEnabled(false);
#if defined(Q_OS_MACOS)
	ui->cameraSettingButton->hide();
#endif

	pls_async_call(this, [this]() { InitCameraList(); });
}

PLSTakeCameraSnapshot::~PLSTakeCameraSnapshot()
{
	HideLoading();
	ClearSource();
	obs_display_remove_draw_callback(ui->preview->GetDisplay(), PLSTakeCameraSnapshot::drawPreview, this);
	if (m_pScreenCapture)
		pls_delete(m_pScreenCapture);
	pls_delete(ui, nullptr);
}

QString PLSTakeCameraSnapshot::getSnapshot()
{
	if (hasError) {
		return QString();
	} else if (exec() == Accepted) {
		return imageFilePath;
	}
	return QString();
}

QList<QPair<QString, QString>> PLSTakeCameraSnapshot::getCameraList()
{
	QList<QPair<QString, QString>> cameraList;
	if (obs_properties_t *properties = obs_get_source_properties(OBS_DSHOW_SOURCE_ID); properties) {
		if (obs_property_t *property = obs_properties_get(properties, takephoto::CSTR_VIDEO_DEVICE_ID); property) {
			for (size_t i = 0, count = obs_property_list_item_count(property); i < count; ++i) {
				QString name = QString::fromUtf8(obs_property_list_item_name(property, i));
				QString value = QString::fromUtf8(obs_property_list_item_string(property, i));
				cameraList.append({name, value});
			}
		}

		obs_properties_destroy(properties);
	}
	return cameraList;
}
void PLSTakeCameraSnapshot::setSourceValid(bool isValid)
{
	sourceValid = isValid;
}

struct TaskParam {
	os_event_t *event;
	obs_source_t *source;
	uint32_t cx;
	uint32_t cy;
	int idx;
};

static void sourceSizeValid(void *param, uint32_t, uint32_t)
{
	auto context = (TaskParam *)param;

	struct obs_source_frame *frame = obs_source_get_frame(context->source);
	if (!frame)
		return;

	obs_source_release_frame(context->source, frame);
	auto cx = obs_source_get_base_width(context->source);
	auto cy = obs_source_get_base_height(context->source);
	context->cx = cx;
	context->cy = cy;
	os_event_signal(context->event);
}

void PLSTakeCameraSnapshot::init(const QString &camera_)
{
	if (inprocess)
		return;

	inprocess = true;
	ui->cameraList->setEnabled(false);
	camera = camera_;

	cameraUninitMaskText->setText(tr("TakeCameraSnapshot.Wait"));
	cameraUninitMask->show();
	ui->okButton->setEnabled(false);

	ClearSource();

	QByteArray name = "take-camera-snapshot-" + QUuid::createUuid().toString().toUtf8();
	sourceValid = false;
	source = takephoto::createSource(camera.toUtf8().constData(), name.constData());
	if (!source) {
		ui->cameraList->setEnabled(true);
		hasError = true;
		return;
	}
	
	updatePropertiesSignal.Connect(obs_source_get_signal_handler(source),
								   "update_properties",
								   PLSTakeCameraSnapshot::updateProperties,
								   this);

	sourceValid = true;
	QPointer<PLSTakeCameraSnapshot> guard(this);
	PLSFrameCheck check(source);
	QEventLoop loop;
	connect(&check, &PLSFrameCheck::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (!guard)
		return;

	inprocess = false;
	ui->cameraList->setEnabled(true);
	bool failed = !check.getResult();

	obs_data_t* settings = obs_source_get_settings(source);
	bool not_support_hdr = obs_data_get_bool(settings, "not_support_hdr");
	obs_data_release(settings);

	if (failed || not_support_hdr) {
		hasError = true;
		sourceValid = false;
		obs_source_release(source);

		if(failed)
			cameraUninitMaskText->setText(tr("TakeCameraSnapshot.DeviceError"));
		else if(not_support_hdr)
			cameraUninitMaskText->setText(tr("source.camera.snapshot.notsupport.hdr"));

		cameraUninitMask->show();
		ui->okButton->setEnabled(false);
		return;
	}

	//PLSBasicProperties::SetOwnerWindow(source, window()->winId());

	signal_handler_connect_ref(obs_source_get_signal_handler(source), takephoto::CSTR_CAPTURE_STATE, &PLSTakeCameraSnapshot::onSourceCaptureState, this);
	signal_handler_connect_ref(obs_source_get_signal_handler(source), takephoto::CSTR_SOURCE_IMAGE_STATUS, &PLSTakeCameraSnapshot::onSourceImageStatus, this);
	//ui->preview->UpdateSourceState(source);
	//ui->preview->AttachSource(source);

	onSourceCaptureState();
	//onSourceImageStatus(obs_source_get_image_status(source));

	obs_source_inc_showing(source);

	obs_source_release(source);
}

void PLSTakeCameraSnapshot::ClearSource()
{
	if (source) {
		//PLSBasicProperties::SetOwnerWindow(source, 0);
		signal_handler_disconnect(obs_source_get_signal_handler(source), takephoto::CSTR_CAPTURE_STATE, &PLSTakeCameraSnapshot::onSourceCaptureState, this);
		signal_handler_disconnect(obs_source_get_signal_handler(source), takephoto::CSTR_SOURCE_IMAGE_STATUS, &PLSTakeCameraSnapshot::onSourceImageStatus, this);
		obs_source_dec_showing(source);
		sourceValid = false;
		source = nullptr;
	}
}

void PLSTakeCameraSnapshot::drawPreview(void *data, uint32_t cx, uint32_t cy)
{
	auto window = static_cast<PLSTakeCameraSnapshot *>(data);

	if (!window->sourceValid) {
		return;
	}

	if (!window->source) {
		return;
	}

	uint32_t sourceCX = std::max(obs_source_get_width(window->source), 1u);
	uint32_t sourceCY = std::max(obs_source_get_height(window->source), 1u);

	int x;
	int y;
	int newCX;
	int newCY;
	float scale;
	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	newCX = int(scale * float(sourceCX));
	newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);

	obs_source_video_render(window->source);

	gs_projection_pop();
	gs_viewport_pop();
}

void PLSTakeCameraSnapshot::onSourceCaptureState(void *data, calldata_t *calldata)
{
	pls_unused(calldata);
	QMetaObject::invokeMethod(static_cast<PLSTakeCameraSnapshot *>(data), "onSourceCaptureState", Qt::QueuedConnection);
}

void PLSTakeCameraSnapshot::onSourceImageStatus(void *data, calldata_t *calldata)
{
	pls_unused(calldata);
	auto *tcs = static_cast<PLSTakeCameraSnapshot *>(data);
	bool status = calldata_bool(calldata, "image_status");
	QMetaObject::invokeMethod(tcs, "onSourceImageStatus", Qt::QueuedConnection, Q_ARG(bool, status));
}

#if defined(Q_OS_MACOS)
static void macPermissionCallback(void *inUserData, bool isUserClickOK)
{
	auto cameraClass = static_cast<PLSTakeCameraSnapshot *>(inUserData);
	if (pls_object_is_valid(cameraClass)) {
		cameraClass->setSourceValid(isUserClickOK);
	}
}
#endif

void PLSTakeCameraSnapshot::updateProperties(void *data, calldata_t *calldata) {
	auto self = static_cast<PLSTakeCameraSnapshot *>(data);
	
	bool cameraRemoved = true;
	
	if (obs_properties_t *properties = obs_get_source_properties(OBS_DSHOW_SOURCE_ID); properties) {
		if (obs_property_t *property = obs_properties_get(properties, takephoto::CSTR_VIDEO_DEVICE_ID); property) {
			for (size_t i = 0, count = obs_property_list_item_count(property); i < count; ++i) {
				QString name = QString::fromUtf8(obs_property_list_item_name(property, i));
				QString value = QString::fromUtf8(obs_property_list_item_string(property, i));
				
				if (value == self->camera) {
					cameraRemoved = false;
					break;
				}
			}
		}
		obs_properties_destroy(properties);
	}
	
	if (cameraRemoved) {
		int index = self->ui->cameraList->findData(self->camera);
		if (index >= 0) {
			QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(self->ui->cameraList->model());
			if (model) {
				QStandardItem *item = model->item(index);
				item->setFlags(Qt::NoItemFlags);
			}
		}
	}
}

void PLSTakeCameraSnapshot::InitCameraList()
{
	ShowLoading();
#if defined(Q_OS_WINDOWS)
	/* issue #3024, check if enum thread was blocked every 2s. */
	QPointer<QTimer> timer = pls_new<QTimer>();
	timer->setInterval(2000);
	connect(timer, &QTimer::timeout, this, &PLSTakeCameraSnapshot::CheckEnumTimeout);
	connect(
		PLSGetPropertiesThread::Instance(), &PLSGetPropertiesThread::OnProperties, this,
		[this, timer](const PropertiesParam_t &param) {
			if (param.id == (quint64)(void *)(this) && param.properties) {
				if (timer)
					timer->deleteLater();
				initCameraListLambda(param.properties);
			}
		},
		Qt::QueuedConnection);
	PLSGetPropertiesThread::Instance()->GetPropertiesBySourceId(OBS_DSHOW_SOURCE_ID, (quint64)(void *)(this));
	timer->start();
#else
	connect(
		PLSGetPropertiesThread::Instance(), &PLSGetPropertiesThread::OnProperties, this,
		[this](const PropertiesParam_t &param) {
			if (param.id == (quint64)(void *)(this) && param.properties) {
				initCameraListLambda(param.properties);
			}
		},
		Qt::QueuedConnection);
	PLSGetPropertiesThread::Instance()->GetPropertiesBySourceId(OBS_DSHOW_SOURCE_ID, (quint64)(void *)(this));
#endif

#if defined(Q_OS_MACOS)
	auto permissionStatus = PLSPermissionHelper::getVideoPermissonStatus(this, macPermissionCallback);
	QMetaObject::invokeMethod(
		this, [permissionStatus, this]() { PLSPermissionHelper::showPermissionAlertIfNeeded(PLSPermissionHelper::AVType::Video, permissionStatus); }, Qt::QueuedConnection);
#endif
}

void PLSTakeCameraSnapshot::initCameraListLambda(obs_properties_t *properties)
{
	HideLoading();
	QList<QPair<QString, QString>> cameraList;
	if (obs_property_t *property = obs_properties_get(properties, takephoto::CSTR_VIDEO_DEVICE_ID); property) {
		for (size_t i = 0, count = obs_property_list_item_count(property); i < count; ++i) {
			QString name = QString::fromUtf8(obs_property_list_item_name(property, i));
			QString value = QString::fromUtf8(obs_property_list_item_string(property, i));
			if (!name.isEmpty()) {
				cameraList.append({name, value});
			}
		}
	}
	obs_properties_destroy(properties);

	if (!cameraList.isEmpty()) {
		QSignalBlocker blocker(ui->cameraList);
		ui->cameraList->clear();
		int cameraIndex = -1;
		for (int index = 0, count = cameraList.count(); index < count; ++index) {
			const auto &camera_ = cameraList[index];
			ui->cameraList->addItem(camera_.first, camera_.second);
			if (!this->camera.isEmpty() && this->camera == camera_.second) {
				cameraIndex = index;
			}
		}

		if (cameraIndex >= 0) {
			ui->cameraList->setCurrentIndex(cameraIndex);
			init(this->camera);
		} else {
			init(cameraList.first().second);
		}
		ui->cameraList->setEnabled(true);
		ui->cameraSettingButton->setEnabled(true);
	} else {
		cameraUninitMaskText->setText(tr("TakeCameraSnapshot.NoDevice"));
		ui->cameraList->addItem(tr("TakeCameraSnapshot.NoDevice"));
		ui->cameraList->setEnabled(false);
		ui->cameraSettingButton->setEnabled(false);
		ui->okButton->setEnabled(false);
	}
}

void PLSTakeCameraSnapshot::ShowLoading()
{
	HideLoading();
	m_pWidgetLoadingBG = pls_new<QWidget>(content());
	m_pWidgetLoadingBG->setObjectName("loadingBG");
	m_pWidgetLoadingBG->setGeometry(ui->preview->geometry());
	m_pWidgetLoadingBG->show();

	m_pLoadingEvent = pls_new<PLSLoadingEvent>();

	auto layout = pls_new<QVBoxLayout>(m_pWidgetLoadingBG);
	auto loadingBtn = pls_new<QPushButton>(m_pWidgetLoadingBG);
	auto tips = pls_new<QLabel>(tr("main.camera.loading.devicelist"));
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

void PLSTakeCameraSnapshot::HideLoading()
{
	if (m_pLoadingEvent) {
		m_pLoadingEvent->stopLoadingTimer();
		pls_delete(m_pLoadingEvent, nullptr);
	}

	if (m_pWidgetLoadingBG) {
		m_pWidgetLoadingBG->hide();
		pls_delete(m_pWidgetLoadingBG, nullptr);
	}
}

void PLSTakeCameraSnapshot::stopTimer()
{
	timerMask->stop();
	ui->cameraList->setEnabled(true);
	ui->cameraSettingButton->setEnabled(true);
	ui->cancelButton->setEnabled(true);
}

void PLSTakeCameraSnapshot::on_cameraList_currentIndexChanged(int index)
{
	if (QString camera_ = ui->cameraList->itemData(index).toString(); !camera_.isEmpty()) {
		init(camera_);
	}
}
void PLSTakeCameraSnapshot::on_cameraSettingButton_clicked() const
{
	if (source) {
		if (obs_properties_t *properties = obs_source_properties(source); properties) {
			if (obs_property_t *property = obs_properties_get(properties, "video_config"); property) {
				obs_property_button_clicked(property, source);
			}
		}
	}
}
void PLSTakeCameraSnapshot::on_okButton_clicked()
{
	ui->cameraList->setEnabled(false);
	ui->cameraSettingButton->setEnabled(false);
	ui->okButton->setEnabled(false);
	ui->cancelButton->setEnabled(false);

#if defined(Q_OS_WINDOWS)
	timerMask->setGeometry(QRect(ui->preview->mapToGlobal(QPoint(0, 0)), ui->preview->size()));
	timerMask->start(3);
#else
	timerMask->start(ui->preview->GetDisplay(), 3);
#endif
}
void PLSTakeCameraSnapshot::on_cancelButton_clicked()
{
	reject();
}

void PLSTakeCameraSnapshot::onTimerEnd()
{
	/*obs_source_get_capture_valid(source, &error);
	bool ok = error == OBS_SOURCE_ERROR_OK;
	ui->okButton->setEnabled(ok);*/
	ui->cameraList->setEnabled(true);
	ui->cameraSettingButton->setEnabled(true);
	ui->cancelButton->setEnabled(true);
	//captureImage = ok;
	if (m_pScreenCapture) {
		PLS_INFO("PLSTakeCameraSnapshot", "Cannot take new screenshot, "
						  "screenshot currently in progress");
		return;
	}

	m_pScreenCapture = pls_new<ScreenshotObj>(source);
	connect(m_pScreenCapture, &ScreenshotObj::Finished, this, [this](const QString &path) {
		imageFilePath = path;
		QMetaObject::invokeMethod(this, &PLSTakeCameraSnapshot::accept, Qt::QueuedConnection);
	});
	m_pScreenCapture->Start(false);
}

void PLSTakeCameraSnapshot::onSourceCaptureState()
{
	cameraUninitMask->hide();
	ui->okButton->setEnabled(true);
}

void PLSTakeCameraSnapshot::onSourceImageStatus(bool status)
{
	cameraUninitMask->setHidden(status);
	ui->okButton->setEnabled(status);
}

#if defined(Q_OS_WINDOWS)
void PLSTakeCameraSnapshot::CheckEnumTimeout()
{
	std::array<wchar_t, 128> buffer{};
	auto result = pls_get_enum_timeout_device(buffer.data(), buffer.size());
	if (result) {
		std::wstring deviceName(buffer.data());
		auto timer = qobject_cast<QTimer *>(sender());
		if (timer) {
			timer->stop();
			timer->deleteLater();
		}
		pls_async_call(this, [this, deviceName]() {
			std::array<char, 512> deviceUtf8 = {0};
			os_wcs_to_utf8(deviceName.c_str(), 0, deviceUtf8.data(), deviceUtf8.size());
			PLSUIFunc::showEnumTimeoutAlertView(QString::fromUtf8(deviceUtf8.data()));
		});
	}
}

bool PLSTakeCameraSnapshot::event(QEvent *event)
{
	if ((event->type() == QEvent::Move) && timerMask->isVisible()) {
		timerMask->setGeometry(QRect(ui->preview->mapToGlobal(QPoint(0, 0)), ui->preview->size()));
	}
	return PLSDialogView::event(event);
}
#endif

void PLSTakeCameraSnapshot::done(int result)
{
#if defined(Q_OS_WINDOWS)
	timerMask->hide();
#else
	timerMask->stop();
#endif
	PLSDialogView::done(result);
}

bool PLSTakeCameraSnapshot::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->preview) {
		switch (event->type()) {
		case QEvent::Resize:
			cameraUninitMask->setGeometry(QRect(ui->preview->pos(), dynamic_cast<QResizeEvent *>(event)->size()));
			break;
		case QEvent::Move:
			cameraUninitMask->setGeometry(QRect(dynamic_cast<QMoveEvent *>(event)->pos(), ui->preview->size()));
			break;
		default:
			break;
		}
	}
	return PLSDialogView::eventFilter(watched, event);
}

PLSFrameCheck::PLSFrameCheck(obs_source_t *source, QObject *object) : m_source(source), QObject(object)
{
	m_timer500ms = startTimer(500);
}

PLSFrameCheck::~PLSFrameCheck()
{
	if (m_timer500ms > 0)
		killTimer(m_timer500ms);
}

void PLSFrameCheck::timerEvent(QTimerEvent *event)
{
	if (m_timer500ms != event->timerId()) {
		QObject::timerEvent(event);
		return;
	}

	if (m_count == 0) { // 500ms
		checkSourceSizeValid(true);
	} else if (m_count == 1) { // 1000ms
		checkSourceSizeValid(true);
	} else if (m_count == 3) { // 2000ms
		checkSourceSizeValid(true);
	} else if (m_count == 5) { // 3000ms
		checkSourceSizeValid(false);
	}

	m_count++;

	QObject::timerEvent(event);
}

void PLSFrameCheck::checkSourceSizeValid(bool needWait)
{
	if (m_source) {
		auto cx = obs_source_get_base_width(m_source);
		auto cy = obs_source_get_base_height(m_source);
		bool valid = (cx > 0 && cy > 0);
		m_result = valid;
		if (valid)
			emit finished(true);
		else if (!needWait)
			emit finished(false);
	}
}
