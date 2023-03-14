#include "volume-control.hpp"
#include "qt-wrappers.hpp"
#include "pls-app.hpp"
#include "mute-checkbox.hpp"
#include "slider-ignorewheel.hpp"
#include "slider-absoluteset-style.hpp"
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QStyleFactory>
#include <qcheckbox.h>
#include "log/log.h"
#include "window-basic-main.hpp"
#include "PLSDpiHelper.h"
#include "pls-common-define.hpp"

using namespace std;

#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define FADER_PRECISION 4096.0

QWeakPointer<VolumeMeterTimer> VolumeMeter::updateTimer;

void VolControl::PLSVolumeChanged(void *data, float db)
{
	Q_UNUSED(db);

	VolControl *volControl = static_cast<VolControl *>(data);

	QMetaObject::invokeMethod(volControl, "VolumeChanged");
}

void VolControl::PLSVolumeLevel(void *data, const float magnitude[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS])
{
	VolControl *volControl = static_cast<VolControl *>(data);

	volControl->volMeter->setLevels(magnitude, peak, inputPeak);
}

void VolControl::PLSVolumeMuted(void *data, calldata_t *calldata)
{
	VolControl *volControl = static_cast<VolControl *>(data);
	bool muted = calldata_bool(calldata, "muted");

	QMetaObject::invokeMethod(volControl, "VolumeMuted", Q_ARG(bool, muted));
}

void VolControl::VolumeChanged()
{
	slider->blockSignals(true);
	slider->setValue((int)(obs_fader_get_deflection(obs_fader) * FADER_PRECISION));
	slider->blockSignals(false);

	updateText();
	if (!volInit)
		PLS_UI_STEP(AUDIO_MIXER, QString("volume slider changed value:" + volLabel->text()).toUtf8().data(), ACTION_CLICK);
}

void VolControl::VolumeMuted(bool muted)
{
	if (mute->isChecked() != muted) {
		mute->setChecked(muted);
	}
	slider->setEnabled(!muted);
}

void VolControl::SetMuted(bool checked)
{
	true == checked ? mute->setToolTip(QTStr("Unmute")) : mute->setToolTip(QTStr("Mute"));
	obs_source_set_muted(source, checked);
	slider->setEnabled(!checked);
	mute->setChecked(checked);
	QString log = QString("[%1 : %2] mute control: %3").arg(obs_source_get_id(source)).arg(obs_source_get_name(source)).arg(mute->toolTip());
	PLS_UI_STEP(AUDIO_MIXER, log.toUtf8(), ACTION_CLICK);
}

void VolControl::SliderChanged(int vol)
{
	obs_fader_set_deflection(obs_fader, float(vol) / FADER_PRECISION);
	updateText();
}

void VolControl::SliderRelease()
{
	if (!volLabel) {
		return;
	}
	QString sourceName = obs_source_get_name(source);
	if (volLabel)
		PLS_UI_STEP(AUDIO_MIXER, QString("source[%1] volume slider changed value:" + volLabel->text()).arg(sourceName).toUtf8().data(), ACTION_CLICK);
}

void VolControl::updateText()
{
	QString text;
	float db = obs_fader_get_db(obs_fader);

	if (db < -96.0f)
		text = "-inf dB";
	else
		text = QString::number(db, 'f', 1).append(" dB");

	volLabel->setText(text);

	bool muted = obs_source_muted(source);
	const char *accTextLookup = muted ? "VolControl.SliderMuted" : "VolControl.SliderUnmuted";

	QString sourceName = obs_source_get_name(source);
	QString accText = QTStr(accTextLookup).arg(sourceName).arg(db);

	slider->setAccessibleName(accText);
}

void VolControl::monitorCheckChange(int index)
{
	PLS_UI_STEP(AUDIO_MIXER, "monitor check", ACTION_CLICK);
	QString type = nullptr;
	switch (index) {
	case Qt::Checked: {
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
		monitorCheckbox->setToolTip(QTStr("audio.mixer.monitor.off"));
		type = "monitor and output";
		PLS_UI_STEP(AUDIO_MIXER, "monitor check type = monitor and output", ACTION_CLICK);

		//
		break;
	}
	case Qt::PartiallyChecked: {
		monitorCheckbox->setToolTip(QTStr("audio.mixer.monitor.off"));
		type = "monitor only";
		//
		break;
	}
	case Qt::Unchecked: {
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_NONE);
		monitorCheckbox->setToolTip(QTStr("audio.mixer.monitor.on"));
		type = "none";
		PLS_UI_STEP(AUDIO_MIXER, "monitor check type = none", ACTION_CLICK);

		break;
	}
	default:
		break;
	}
	PLSBasic ::Get()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY_BACK);
	QString log = QString("[%1 : %2] monitoring control: %3").arg(obs_source_get_id(source)).arg(obs_source_get_name(source)).arg(type);
	PLS_UI_STEP(AUDIO_MIXER, log.toUtf8(), ACTION_CLICK);
}

void VolControl::rnNoiseClicked()
{
	PLS_UI_STEP(AUDIO_MIXER, "RNNoise checkbox", ACTION_CLICK);
	QString type;
	auto currentState = rnNoiseCheckbox->checkState();
	switch (currentState) {
	case Qt::Unchecked: {
		type = "unchecked";
		if (rnNoiseFilter) {
			obs_source_set_enabled(rnNoiseFilter, false);
		}
		break;
	}
	case Qt::Checked: {
		type = "checked";
		if (rnNoiseFilter) {
			obs_source_set_enabled(rnNoiseFilter, true);
			return;
		}
		// No RNNoise applyed yet, create a new one
		QString strFilterName = QString::asprintf("%s_%1", FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE).arg(QDateTime::currentMSecsSinceEpoch());
		obs_source_t *existing_filter = obs_source_get_filter_by_name(source, strFilterName.toUtf8().constData());
		if (existing_filter) {
			PLS_INFO(AUDIO_MIXER, "source %s is already exsit", strFilterName.toUtf8().constData());
			obs_source_release(existing_filter);
			return;
		}
		obs_source_t *filter = obs_source_create(FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE, strFilterName.toUtf8().constData(), nullptr, nullptr);
		if (filter) {
			rnNoiseCheckbox->setEnabled(false);
			const char *sourceName = obs_source_get_name(source);
			PLS_INFO(AUDIO_MIXER,
				 "User added '%s' filter "
				 "to source '%s'",
				 FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE, sourceName);
			obs_source_filter_add(source, filter);
			obs_source_release(filter);
		}
		break;
	}
	default:
		break;
	}

	QString log = QString("[%1 : %2] rnnoise control: %3").arg(obs_source_get_id(source)).arg(obs_source_get_name(source)).arg(type);
	PLS_UI_STEP(AUDIO_MIXER, log.toUtf8(), ACTION_CLICK);
}

void VolControl::AddFilter(OBSSource filter)
{
	// add RNNoise filter succeed callback
	rnNoiseFilter = filter;
	signal_handler_connect_ref(obs_source_get_signal_handler(rnNoiseFilter), "enable", VolControl::OBSSourceFilterEnabled, this);
	if (rnNoiseCheckbox) {
		rnNoiseCheckbox->setEnabled(true);
		rnNoiseCheckbox->setChecked(true);
		rnNoiseCheckbox->setToolTip(QTStr("audio.mixer.rnnoise.off"));
	}
}

void VolControl::RemoveFilter()
{
	// remove RNNoise filter succeed callback
	if (rnNoiseCheckbox) {
		rnNoiseCheckbox->setEnabled(true);
		rnNoiseCheckbox->setChecked(false);
		rnNoiseCheckbox->setToolTip(QTStr("audio.mixer.rnnoise.on"));
	}

	PLSBasic *main = PLSBasic::Get();
	if (main)
		main->SaveProject();
}

void VolControl::SourceEnabled(bool enabled)
{
	// set RNNoise filter enable state succeed callback
	PLS_INFO(AUDIO_MIXER, "%s RNNoise %s", obs_source_get_name(source), enabled ? "on" : "off");
	if (rnNoiseCheckbox) {
		rnNoiseCheckbox->setChecked(enabled);
		rnNoiseCheckbox->setToolTip(enabled ? QTStr("audio.mixer.rnnoise.off") : QTStr("audio.mixer.rnnoise.on"));
	}
}

QString VolControl::GetName() const
{
	return currentDisplayName;
}

void VolControl::SetName(const QString &newName)
{
	currentDisplayName = newName;
	QFontMetrics fontWidth(nameLabel->font());
	int space = PLSDpiHelper::calculate(this, 148);
	if (fontWidth.horizontalAdvance(currentDisplayName) > width() - space) {
		nameLabel->setText(fontWidth.elidedText(newName, Qt::ElideRight, width() - space));
	} else {
		nameLabel->setText(newName);
	}
}

void VolControl::EmitConfigClicked()
{
	emit ConfigClicked();
}

void VolControl::monitorStateChangeFromAdv(Qt::CheckState state)
{
	QSignalBlocker blocker(monitorCheckbox);
	switch (state) {
	case Qt::Checked:
		monitorCheckbox->setChecked(true);
		monitorCheckbox->setToolTip(QTStr("audio.mixer.monitor.off"));

		break;
	case Qt::Unchecked:
		monitorCheckbox->setChecked(false);
		monitorCheckbox->setToolTip(QTStr("audio.mixer.monitor.on"));
		break;
	case Qt::PartiallyChecked:
		monitorCheckbox->setChecked(true);
		monitorCheckbox->setToolTip(QTStr("audio.mixer.monitor.off"));
		break;
	default:
		break;
	}
}

void VolControl::UpdateRnNoiseState()
{
	// Get RNNoise filter from this source
	struct FilterData {
		bool isRNNoiseOn;
		obs_source_t *source;
	};

	FilterData data;
	data.isRNNoiseOn = false;
	data.source = nullptr;
	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *param) {
			if (filter) {
				const char *id = obs_source_get_id(filter);
				if (id && *id && strcmp(id, FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE) == 0) {
					auto filterData = reinterpret_cast<FilterData *>(param);
					filterData->isRNNoiseOn = obs_source_enabled(filter);
					filterData->source = filter;
				}
			}
		},
		&data);
	if (rnNoiseCheckbox) {
		rnNoiseCheckbox->setChecked(data.isRNNoiseOn);
		rnNoiseCheckbox->setToolTip(data.isRNNoiseOn ? QTStr("audio.mixer.rnnoise.off") : QTStr("audio.mixer.rnnoise.on"));
	}
	rnNoiseFilter = data.source;
	if (rnNoiseFilter)
		signal_handler_connect_ref(obs_source_get_signal_handler(rnNoiseFilter), "enable", VolControl::OBSSourceFilterEnabled, this);
}

void VolControl::SetMeterDecayRate(qreal q)
{
	volMeter->setPeakDecayRate(q);
}

void VolControl::setPeakMeterType(enum obs_peak_meter_type peakMeterType)
{
	volMeter->setPeakMeterType(peakMeterType);
}
bool VolControl::eventFilter(QObject *watched, QEvent *e)
{
	if (e->type() == QEvent::Resize) {
		if (watched == this) {
			QTimer::singleShot(0, this, [=]() { SetName(currentDisplayName); });
			return true;
		}
	}
	return QWidget::eventFilter(watched, e);
}

void VolControl::monitorChange(pls_frontend_event event, const QVariantList &params, void *context)
{
	Q_UNUSED(params)

	if (pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY == event) {

		VolControl *control = (VolControl *)context;
		control->monitorStateChangeFromAdv(static_cast<Qt::CheckState>(obs_source_get_monitoring_type(control->source)));
	}
}

void VolControl::OBSSourceFilterAdded(void *param, calldata_t *data)
{
	obs_source_t *filter = (obs_source_t *)calldata_ptr(data, "filter");
	const char *id = obs_source_get_id(filter);
	if (id && strcmp(id, FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE) == 0) {
		VolControl *window = reinterpret_cast<VolControl *>(param);
		QMetaObject::invokeMethod(window, "AddFilter", Q_ARG(OBSSource, OBSSource(filter)));
	}
}

void VolControl::OBSSourceFilterRemoved(void *param, calldata_t *data)
{
	obs_source_t *filter = (obs_source_t *)calldata_ptr(data, "filter");
	const char *id = obs_source_get_id(filter);
	if (id && strcmp(id, FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE) == 0) {
		VolControl *window = reinterpret_cast<VolControl *>(param);
		QMetaObject::invokeMethod(window, "RemoveFilter");
	}
}

void VolControl::OBSSourceFilterEnabled(void *param, calldata_t *data)
{
	obs_source_t *filter = (obs_source_t *)calldata_ptr(data, "source");
	const char *id = obs_source_get_id(filter);
	bool enabled = calldata_bool(data, "enabled");
	if (id && strcmp(id, FILTER_TYPE_ID_NOISE_SUPPRESSION_RNNOISE) == 0) {
		VolControl *window = reinterpret_cast<VolControl *>(param);
		QMetaObject::invokeMethod(window, "SourceEnabled", Q_ARG(bool, enabled));
	}
}

VolControl::VolControl(OBSSource source_, bool showConfig, bool vertical, PLSDpiHelper dpiHelper)
	: source(std::move(source_)), levelTotal(0.0f), levelCount(0.0f), obs_fader(obs_fader_create(OBS_FADER_LOG)), obs_volmeter(obs_volmeter_create(OBS_FADER_LOG)), vertical(vertical)
{
	Q_UNUSED(vertical)
	dpiHelper.setCss(this, {PLSCssIndex::PrismAudioMixer});

	pls_frontend_add_event_callback(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY, VolControl::monitorChange, this);

	nameLabel = new QLabel();
	nameLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	nameLabel->setObjectName("audioMixerNameLabel");
	this->installEventFilter(this);
	volLabel = new QLabel();
	volLabel->setObjectName("volumeValueLabel");
	monitorCheckbox = new QCheckBox();
	monitorCheckbox->setObjectName("volumeMonitorCheck");
	connect(monitorCheckbox, &QCheckBox::stateChanged, this, &VolControl::monitorCheckChange);
	monitorStateChangeFromAdv(static_cast<Qt::CheckState>(obs_source_get_monitoring_type(source)));

	//PRISM/XieWei/20201110/#None/Add a switch for RNNoise
	const char *id = obs_source_get_id(source);
	if (id && *id && (strcmp(AUDIO_INPUT_SOURCE_ID, id) == 0 || strcmp(PRISM_MOBILE_SOURCE_ID, id) == 0 || strcmp(DSHOW_SOURCE_ID, id) == 0 || 0 == strcmp(PRISM_NDI_SOURCE_ID, id))) {
		rnNoiseCheckbox = new QCheckBox(this);
		rnNoiseCheckbox->setChecked(false);
		rnNoiseCheckbox->setObjectName("volumeRNNoiseCheckbox");
		rnNoiseCheckbox->setToolTip(QTStr("audio.mixer.rnnoise.on"));
		connect(rnNoiseCheckbox, &QCheckBox::clicked, this, &VolControl::rnNoiseClicked);
		UpdateRnNoiseState();
	}

	mute = new MuteCheckBox();

	QString sourceName = obs_source_get_name(source);
	setObjectName(sourceName);
	double dpi = PLSDpiHelper::getDpi(this);
	if (showConfig) {
		//config button is show right menu
		config = new QPushButton(this);
		//config->setProperty("themeID", "configIconSmall");
		config->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
		config->setMaximumSize(PLSDpiHelper::calculate(dpi, 22), PLSDpiHelper::calculate(dpi, 22));

		config->setAccessibleName(QTStr("VolControl.Properties").arg(sourceName));
		config->setObjectName("volumeMenuButton");

		connect(config, &QAbstractButton::clicked, this, &VolControl::EmitConfigClicked);
	}

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(16, 4, 0, 13); //before:19 ui:modify is 16
	mainLayout->setSpacing(2);

	QHBoxLayout *volLayout = new QHBoxLayout;
	QHBoxLayout *textLayout = new QHBoxLayout;
	QHBoxLayout *botLayout = new QHBoxLayout;
	QHBoxLayout *textLeftLayout = new QHBoxLayout;
	QHBoxLayout *textRightLayout = new QHBoxLayout;
	textLeftLayout->setSpacing(5);
	textLeftLayout->setContentsMargins(0, 0, 0, 0);
	textLeftLayout->setAlignment(Qt::AlignLeft);

	textRightLayout->setSpacing(0);
	textRightLayout->setContentsMargins(15, 0, 0, 0);
	textRightLayout->setAlignment(Qt::AlignRight);

	volMeter = new VolumeMeter(nullptr, obs_volmeter, false);
	slider = new SliderIgnoreScroll(Qt::Horizontal);

	textLayout->setContentsMargins(0, 0, 4, 0);
	textLeftLayout->addWidget(nameLabel);
	QLabel *speratorLabel = new QLabel();
	speratorLabel->setObjectName("volumeSperatorLabel");
	textLeftLayout->addWidget(speratorLabel);
	textLeftLayout->addWidget(volLabel);
	textRightLayout->addWidget(monitorCheckbox);

	textLayout->addLayout(textLeftLayout);
	textLayout->addLayout(textRightLayout);

	textLayout->setAlignment(textLeftLayout, Qt::AlignLeft);

	textLayout->setAlignment(textRightLayout, Qt::AlignRight);
	textLayout->setSpacing(0);

	QHBoxLayout *layout_slider = new QHBoxLayout;
	layout_slider->setContentsMargins(0, 0, 2, 0);
	layout_slider->setSpacing(0);
	layout_slider->addWidget(slider);

	volLayout->addItem(layout_slider);
	volLayout->addWidget(mute);
	if (rnNoiseCheckbox) {
		volLayout->addWidget(rnNoiseCheckbox);
	}
	volLayout->setContentsMargins(0, 0, 0, 0);
	volLayout->setSpacing(10);
	botLayout->setContentsMargins(0, 3, 0, 0);
	botLayout->setSpacing(10);
	botLayout->addLayout(volLayout);

	if (showConfig) {
		botLayout->addWidget(config);
	}

	mainLayout->addItem(textLayout);
	mainLayout->addWidget(volMeter);
	mainLayout->addItem(botLayout);

	volMeter->setFocusProxy(slider);

	setLayout(mainLayout);

	SetName(sourceName);

	slider->setMinimum(0);
	slider->setMaximum(int(FADER_PRECISION));

	bool muted = obs_source_muted(source);
	true == muted ? mute->setToolTip(QTStr("Unmute")) : mute->setToolTip(QTStr("Mute"));
	mute->setChecked(muted);
	slider->setEnabled(!muted);
	mute->setAccessibleName(QTStr("VolControl.Mute").arg(sourceName));
	obs_fader_add_callback(obs_fader, PLSVolumeChanged, this);
	obs_volmeter_add_callback(obs_volmeter, PLSVolumeLevel, this);

	signal_handler_connect_ref(obs_source_get_signal_handler(source), "mute", PLSVolumeMuted, this);
	signal_handler_connect_ref(obs_source_get_signal_handler(source), "filter_add", VolControl::OBSSourceFilterAdded, this);
	signal_handler_connect_ref(obs_source_get_signal_handler(source), "filter_remove", VolControl::OBSSourceFilterRemoved, this);

	QWidget::connect(slider, SIGNAL(valueChanged(int)), this, SLOT(SliderChanged(int)));
	QWidget::connect(slider, SIGNAL(mouseReleaseSignal()), this, SLOT(SliderRelease()));
	QWidget::connect(mute, &MuteCheckBox::clicked, [=](bool cliked) { SetMuted(cliked); });

	obs_fader_attach_source(obs_fader, source);
	obs_volmeter_attach_source(obs_volmeter, source);

	QString styleName = slider->style()->objectName();
	QStyle *style;
	style = QStyleFactory::create(styleName);
	if (!style) {
		style = new SliderAbsoluteSetStyle();
	} else {
		style = new SliderAbsoluteSetStyle(style);
	}

	style->setParent(slider);
	slider->setStyle(style);

	/* Call volume changed once to init the slider position and label */
	volInit = true;
	VolumeChanged();
	volInit = false;
}

VolControl::~VolControl()
{
	pls_frontend_remove_event_callback(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY, VolControl::monitorChange, this);

	obs_fader_remove_callback(obs_fader, PLSVolumeChanged, this);
	obs_volmeter_remove_callback(obs_volmeter, PLSVolumeLevel, this);

	signal_handler_disconnect(obs_source_get_signal_handler(source), "mute", PLSVolumeMuted, this);
	signal_handler_disconnect(obs_source_get_signal_handler(source), "filter_add", VolControl::OBSSourceFilterAdded, this);
	signal_handler_disconnect(obs_source_get_signal_handler(source), "filter_remove", VolControl::OBSSourceFilterRemoved, this);
	if (rnNoiseFilter)
		signal_handler_disconnect(obs_source_get_signal_handler(rnNoiseFilter), "enable", VolControl::OBSSourceFilterEnabled, this);

	obs_fader_destroy(obs_fader);
	obs_volmeter_destroy(obs_volmeter);
}

QColor VolumeMeter::getBackgroundNominalColor() const
{
	return backgroundNominalColor;
}

void VolumeMeter::setBackgroundNominalColor(QColor c)
{
	backgroundNominalColor = std::move(c);
}

QColor VolumeMeter::getBackgroundWarningColor() const
{
	return backgroundWarningColor;
}

void VolumeMeter::setBackgroundWarningColor(QColor c)
{
	backgroundWarningColor = std::move(c);
}

QColor VolumeMeter::getBackgroundErrorColor() const
{
	return backgroundErrorColor;
}

void VolumeMeter::setBackgroundErrorColor(QColor c)
{
	backgroundErrorColor = std::move(c);
}

QColor VolumeMeter::getForegroundNominalColor() const
{
	return foregroundNominalColor;
}

void VolumeMeter::setForegroundNominalColor(QColor c)
{
	foregroundNominalColor = std::move(c);
}

QColor VolumeMeter::getForegroundWarningColor() const
{
	return foregroundWarningColor;
}

void VolumeMeter::setForegroundWarningColor(QColor c)
{
	foregroundWarningColor = std::move(c);
}

QColor VolumeMeter::getForegroundErrorColor() const
{
	return foregroundErrorColor;
}

void VolumeMeter::setForegroundErrorColor(QColor c)
{
	foregroundErrorColor = std::move(c);
}

QColor VolumeMeter::getClipColor() const
{
	return clipColor;
}

void VolumeMeter::setClipColor(QColor c)
{
	clipColor = std::move(c);
}

QColor VolumeMeter::getMagnitudeColor() const
{
	return magnitudeColor;
}

void VolumeMeter::setMagnitudeColor(QColor c)
{
	magnitudeColor = std::move(c);
}

QColor VolumeMeter::getMajorTickColor() const
{
	return majorTickColor;
}

void VolumeMeter::setMajorTickColor(QColor c)
{
	majorTickColor = std::move(c);
}

QColor VolumeMeter::getMinorTickColor() const
{
	return minorTickColor;
}

void VolumeMeter::setMinorTickColor(QColor c)
{
	minorTickColor = std::move(c);
}

qreal VolumeMeter::getMinimumLevel() const
{
	return minimumLevel;
}

void VolumeMeter::setMinimumLevel(qreal v)
{
	minimumLevel = v;
}

qreal VolumeMeter::getWarningLevel() const
{
	return warningLevel;
}

void VolumeMeter::setWarningLevel(qreal v)
{
	warningLevel = v;
}

qreal VolumeMeter::getErrorLevel() const
{
	return errorLevel;
}

void VolumeMeter::setErrorLevel(qreal v)
{
	errorLevel = v;
}

qreal VolumeMeter::getClipLevel() const
{
	return clipLevel;
}

void VolumeMeter::setClipLevel(qreal v)
{
	clipLevel = v;
}

qreal VolumeMeter::getMinimumInputLevel() const
{
	return minimumInputLevel;
}

void VolumeMeter::setMinimumInputLevel(qreal v)
{
	minimumInputLevel = v;
}

qreal VolumeMeter::getPeakDecayRate() const
{
	return peakDecayRate;
}

void VolumeMeter::setPeakDecayRate(qreal v)
{
	peakDecayRate = v;
}

qreal VolumeMeter::getMagnitudeIntegrationTime() const
{
	return magnitudeIntegrationTime;
}

void VolumeMeter::setMagnitudeIntegrationTime(qreal v)
{
	magnitudeIntegrationTime = v;
}

qreal VolumeMeter::getPeakHoldDuration() const
{
	return peakHoldDuration;
}

void VolumeMeter::setPeakHoldDuration(qreal v)
{
	peakHoldDuration = v;
}

qreal VolumeMeter::getInputPeakHoldDuration() const
{
	return inputPeakHoldDuration;
}

void VolumeMeter::setInputPeakHoldDuration(qreal v)
{
	inputPeakHoldDuration = v;
}

void VolumeMeter::setPeakMeterType(enum obs_peak_meter_type peakMeterType)
{
	obs_volmeter_set_peak_meter_type(obs_volmeter, peakMeterType);
	switch (peakMeterType) {
	case TRUE_PEAK_METER:
		// For true-peak meters EBU has defined the Permitted Maximum,
		// taking into account the accuracy of the meter and further
		// processing required by lossy audio compression.
		//
		// The alignment level was not specified, but I've adjusted
		// it compared to a sample-peak meter. Incidentally Youtube
		// uses this new Alignment Level as the maximum integrated
		// loudness of a video.
		//
		//  * Permitted Maximum Level (PML) = -2.0 dBTP
		//  * Alignment Level (AL) = -13 dBTP
		setErrorLevel(-2.0);
		setWarningLevel(-13.0);
		break;

	case SAMPLE_PEAK_METER:
	default:
		// For a sample Peak Meter EBU has the following level
		// definitions, taking into account inaccuracies of this meter:
		//
		//  * Permitted Maximum Level (PML) = -9.0 dBFS
		//  * Alignment Level (AL) = -20.0 dBFS
		setErrorLevel(-9.0);
		setWarningLevel(-20.0);
		break;
	}
}

void VolumeMeter::mousePressEvent(QMouseEvent *event)
{
	setFocus(Qt::MouseFocusReason);
	event->accept();
}

void VolumeMeter::wheelEvent(QWheelEvent *event)
{
	QApplication::sendEvent(focusProxy(), event);
}

VolumeMeter::VolumeMeter(QWidget *parent, obs_volmeter_t *obs_volmeter, bool vertical) : QWidget(parent), obs_volmeter(obs_volmeter), vertical(vertical)
{
	// Use a font that can be rendered small.
	tickFont = QFont("Arial");
	double dpi = PLSDpiHelper::getDpi(this);
	tickFont.setPixelSize(7 * dpi);
	// Default meter color settings, they only show if
	// there is no stylesheet, do not remove.
	backgroundNominalColor.setRgb(0x26, 0x7f, 0x26); // Dark green
	backgroundWarningColor.setRgb(0x7f, 0x7f, 0x26); // Dark yellow
	backgroundErrorColor.setRgb(0x7f, 0x26, 0x26);   // Dark red
	foregroundNominalColor.setRgb(0x4c, 0xff, 0x4c); // Bright green
	foregroundWarningColor.setRgb(0xff, 0xff, 0x4c); // Bright yellow
	foregroundErrorColor.setRgb(0xff, 0x4c, 0x4c);   // Bright red
	clipColor.setRgb(0xff, 0xff, 0xff);              // Bright white
	magnitudeColor.setRgb(0x00, 0x00, 0x00);         // Black
	majorTickColor.setRgb(0xff, 0xff, 0xff);         // Black
	minorTickColor.setRgb(0xcc, 0xcc, 0xcc);         // Black
	minimumLevel = -60.0;                            // -60 dB
	warningLevel = -20.0;                            // -20 dB
	errorLevel = -9.0;                               //  -9 dB
	clipLevel = -0.5;                                //  -0.5 dB
	minimumInputLevel = -50.0;                       // -50 dB
	peakDecayRate = 11.76;                           //  20 dB / 1.7 sec
	magnitudeIntegrationTime = 0.3;                  //  99% in 300 ms
	peakHoldDuration = 20.0;                         //  20 seconds
	inputPeakHoldDuration = 1.0;                     //  1 second

	channels = (int)audio_output_get_channels(obs_get_audio());

	PLSDpiHelper dpiHelper;
	dpiHelper.notifyDpiChanged(this, [=](double dpi) { handleChannelCofigurationChange(dpi, true); });

	updateTimerRef = updateTimer.toStrongRef();
	if (!updateTimerRef) {
		updateTimerRef = QSharedPointer<VolumeMeterTimer>::create();
		updateTimerRef->start(34);
		updateTimer = updateTimerRef;
	}

	updateTimerRef->AddVolControl(this);
}

VolumeMeter::~VolumeMeter()
{
	updateTimerRef->RemoveVolControl(this);
	delete tickPaintCache;
}

void VolumeMeter::setLevels(const float magnitude[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS])
{
	uint64_t ts = os_gettime_ns();
	QMutexLocker locker(&dataMutex);

	currentLastUpdateTime = ts;
	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
		currentMagnitude[channelNr] = magnitude[channelNr];
		currentPeak[channelNr] = peak[channelNr];
		currentInputPeak[channelNr] = inputPeak[channelNr];
	}

	// In case there are more updates then redraws we must make sure
	// that the ballistics of peak and hold are recalculated.
	locker.unlock();
	calculateBallistics(ts);
}

inline void VolumeMeter::resetLevels()
{
	currentLastUpdateTime = 0;
	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
		currentMagnitude[channelNr] = -M_INFINITE;
		currentPeak[channelNr] = -M_INFINITE;
		currentInputPeak[channelNr] = -M_INFINITE;

		displayMagnitude[channelNr] = -M_INFINITE;
		displayPeak[channelNr] = -M_INFINITE;
		displayPeakHold[channelNr] = -M_INFINITE;
		displayPeakHoldLastUpdateTime[channelNr] = 0;
		displayInputPeakHold[channelNr] = -M_INFINITE;
		displayInputPeakHoldLastUpdateTime[channelNr] = 0;
	}
}

inline void VolumeMeter::handleChannelCofigurationChange(double dpi, bool dpiChanged)
{
	QMutexLocker locker(&dataMutex);

	int currentNrAudioChannels = obs_volmeter_get_nr_channels(obs_volmeter);
	if (dpiChanged || displayNrAudioChannels != currentNrAudioChannels) {
		// Make room for 3 pixels meter, with one pixel between each.
		// Then 9/13 pixels for ticks and numbers.

		if (displayNrAudioChannels != currentNrAudioChannels) {
			displayNrAudioChannels = currentNrAudioChannels;
			setMinimumSize(130 * dpi, (displayNrAudioChannels - 1) * 7 * dpi + 12 * dpi);
			resetLevels();
		} else
			setMinimumSize(130 * dpi, (displayNrAudioChannels - 1) * 7 * dpi + 12 * dpi);
	}
}

inline bool VolumeMeter::detectIdle(uint64_t ts)
{
	double timeSinceLastUpdate = (ts - currentLastUpdateTime) * 0.000000001;
	if (timeSinceLastUpdate > 0.5) {
		resetLevels();
		return true;
	} else {
		return false;
	}
}

inline void VolumeMeter::calculateBallisticsForChannel(int channelNr, uint64_t ts, qreal timeSinceLastRedraw)
{
	if (currentPeak[channelNr] >= displayPeak[channelNr] || isnan(displayPeak[channelNr])) {
		// Attack of peak is immediate.
		displayPeak[channelNr] = currentPeak[channelNr];
	} else {
		// Decay of peak is 40 dB / 1.7 seconds for Fast Profile
		// 20 dB / 1.7 seconds for Medium Profile (Type I PPM)
		// 24 dB / 2.8 seconds for Slow Profile (Type II PPM)
		float decay = float(peakDecayRate * timeSinceLastRedraw);
		displayPeak[channelNr] = CLAMP(displayPeak[channelNr] - decay, currentPeak[channelNr], 0);
	}

	if (currentPeak[channelNr] >= displayPeakHold[channelNr] || !isfinite(displayPeakHold[channelNr])) {
		// Attack of peak-hold is immediate, but keep track
		// when it was last updated.
		displayPeakHold[channelNr] = currentPeak[channelNr];
		displayPeakHoldLastUpdateTime[channelNr] = ts;
	} else {
		// The peak and hold falls back to peak
		// after 20 seconds.
		qreal timeSinceLastPeak = (uint64_t)(ts - displayPeakHoldLastUpdateTime[channelNr]) * 0.000000001;
		if (timeSinceLastPeak > peakHoldDuration) {
			displayPeakHold[channelNr] = currentPeak[channelNr];
			displayPeakHoldLastUpdateTime[channelNr] = ts;
		}
	}

	if (currentInputPeak[channelNr] >= displayInputPeakHold[channelNr] || !isfinite(displayInputPeakHold[channelNr])) {
		// Attack of peak-hold is immediate, but keep track
		// when it was last updated.
		displayInputPeakHold[channelNr] = currentInputPeak[channelNr];
		displayInputPeakHoldLastUpdateTime[channelNr] = ts;
	} else {
		// The peak and hold falls back to peak after 1 second.
		qreal timeSinceLastPeak = (uint64_t)(ts - displayInputPeakHoldLastUpdateTime[channelNr]) * 0.000000001;
		if (timeSinceLastPeak > inputPeakHoldDuration) {
			displayInputPeakHold[channelNr] = currentInputPeak[channelNr];
			displayInputPeakHoldLastUpdateTime[channelNr] = ts;
		}
	}

	if (!isfinite(displayMagnitude[channelNr])) {
		// The statements in the else-leg do not work with
		// NaN and infinite displayMagnitude.
		displayMagnitude[channelNr] = currentMagnitude[channelNr];
	} else {
		// A VU meter will integrate to the new value to 99% in 300 ms.
		// The calculation here is very simplified and is more accurate
		// with higher frame-rate.
		float attack = float((currentMagnitude[channelNr] - displayMagnitude[channelNr]) * (timeSinceLastRedraw / magnitudeIntegrationTime) * 0.99);
		displayMagnitude[channelNr] = CLAMP(displayMagnitude[channelNr] + attack, (float)minimumLevel, 0);
	}
}

inline void VolumeMeter::calculateBallistics(uint64_t ts, qreal timeSinceLastRedraw)
{
	QMutexLocker locker(&dataMutex);

	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++)
		calculateBallisticsForChannel(channelNr, ts, timeSinceLastRedraw);
}

void VolumeMeter::paintInputMeter(QPainter &painter, int x, int y, int width, int height, float peakHold)
{
	QMutexLocker locker(&dataMutex);
	QColor color;

	if (peakHold < minimumInputLevel)
		color = backgroundNominalColor;
	else if (peakHold < warningLevel)
		color = foregroundNominalColor;
	else if (peakHold < errorLevel)
		color = foregroundWarningColor;
	else if (peakHold <= clipLevel)
		color = foregroundErrorColor;
	else
		color = clipColor;

	painter.fillRect(x, y, width, height, color);
}

#define CLIP_FLASH_DURATION_MS 1000

void VolumeMeter::ClipEnding()
{
	clipping = false;
}

void VolumeMeter::paintHMeter(QPainter &painter, int x, int y, int width, int height, float magnitude, float peak, float peakHold)
{
	if (minimumLevel == 0) {
		return;
	}

	double dpi = PLSDpiHelper::getDpi(this);

	qreal scale = width / minimumLevel;

	QMutexLocker locker(&dataMutex);
	int minimumPosition = x + 0;
	int maximumPosition = x + width;
	int magnitudePosition = scaleToInt32(x + width - (magnitude * scale));
	int peakPosition = scaleToInt32(x + width - (peak * scale));
	int peakHoldPosition = scaleToInt32(x + width - (peakHold * scale));
	int warningPosition = scaleToInt32(x + width - (warningLevel * scale));
	int errorPosition = scaleToInt32(x + width - (errorLevel * scale));

	int nominalLength = warningPosition - minimumPosition;
	int warningLength = errorPosition - warningPosition;
	int errorLength = maximumPosition - errorPosition;
	locker.unlock();

	if (clipping) {
		peakPosition = maximumPosition;
	}

	if (peakPosition < minimumPosition) {
		painter.fillRect(minimumPosition, y, nominalLength, height, backgroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height, backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height, backgroundErrorColor);
	} else if (peakPosition < warningPosition) {
		painter.fillRect(minimumPosition, y, peakPosition - minimumPosition, height, foregroundNominalColor);
		painter.fillRect(peakPosition, y, warningPosition - peakPosition, height, backgroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height, backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height, backgroundErrorColor);
	} else if (peakPosition < errorPosition) {
		painter.fillRect(minimumPosition, y, nominalLength, height, foregroundNominalColor);
		painter.fillRect(warningPosition, y, peakPosition - warningPosition, height, foregroundWarningColor);
		painter.fillRect(peakPosition, y, errorPosition - peakPosition, height, backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height, backgroundErrorColor);
	} else if (peakPosition < maximumPosition) {
		painter.fillRect(minimumPosition, y, nominalLength, height, foregroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height, foregroundWarningColor);
		painter.fillRect(errorPosition, y, peakPosition - errorPosition, height, foregroundErrorColor);
		painter.fillRect(peakPosition, y, maximumPosition - peakPosition, height, backgroundErrorColor);
	} else {
		if (!clipping) {
			QTimer::singleShot(CLIP_FLASH_DURATION_MS, this, SLOT(ClipEnding()));
			clipping = true;
		}

		int end = errorLength + warningLength + nominalLength;
		painter.fillRect(minimumPosition, y, end, height, QBrush(foregroundErrorColor));
	}

	if (peakHoldPosition - 3 * dpi < minimumPosition)
		; // Peak-hold below minimum, no drawing.
	else if (peakHoldPosition < warningPosition)
		painter.fillRect(scaleToInt32(peakHoldPosition - 3 * dpi), y, 3 * dpi, height, foregroundNominalColor);
	else if (peakHoldPosition < errorPosition)
		painter.fillRect(scaleToInt32(peakHoldPosition - 3 * dpi), y, 3 * dpi, height, foregroundWarningColor);
	else
		painter.fillRect(scaleToInt32(peakHoldPosition - 3 * dpi), y, 3 * dpi, height, foregroundErrorColor);

	if (magnitudePosition - 3 >= minimumPosition)
		painter.fillRect(scaleToInt32(magnitudePosition - 3 * dpi), y, 3 * dpi, height, magnitudeColor);
}

void VolumeMeter::paintEvent(QPaintEvent *event)
{
	double dpi = PLSDpiHelper::getDpi(this);
	uint64_t ts = os_gettime_ns();
	qreal timeSinceLastRedraw = (ts - lastRedrawTime) * 0.000000001;

	const QRect rect = event->region().boundingRect();
	int width = rect.width();

	handleChannelCofigurationChange(dpi);
	calculateBallistics(ts, timeSinceLastRedraw);
	bool idle = detectIdle(ts);

	// Actual painting of the widget starts here.
	QPainter painter(this);

	for (int channelNr = 0; channelNr < displayNrAudioChannels; channelNr++) {

		int channelNrFixed = (displayNrAudioChannels == 1 && channels > 2) ? 0 : channelNr;

		paintHMeter(painter, 0, channelNr * 7 * dpi + 10 * dpi, width - 4 * dpi, 2 * dpi, displayMagnitude[channelNrFixed], displayPeak[channelNrFixed], displayPeakHold[channelNrFixed]);

		if (idle)
			continue;

		// By not drawing the input meter boxes the user can
		// see that the audio stream has been stopped, without
		// having too much visual impact.
		paintInputMeter(painter, 0, channelNr * 7 * dpi + 10 * dpi, 3 * dpi, 2 * dpi, displayInputPeakHold[channelNrFixed]);
	}

	lastRedrawTime = ts;
}

void VolumeMeterTimer::AddVolControl(VolumeMeter *meter)
{
	volumeMeters.push_back(meter);
}

void VolumeMeterTimer::RemoveVolControl(VolumeMeter *meter)
{
	volumeMeters.removeOne(meter);
}

void VolumeMeterTimer::timerEvent(QTimerEvent *)
{
	for (VolumeMeter *meter : volumeMeters)
		meter->update();
}
