#include "window-basic-main.hpp"
#include "moc_volume-control.cpp"
#include "obs-app.hpp"
#include "mute-checkbox.hpp"
#include "absolute-slider.hpp"
#include "source-label.hpp"

#include <slider-ignorewheel.hpp>
#include <qt-wrappers.hpp>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include "pls-common-define.hpp"
#include "PLSBasic.h"

using namespace std;
using namespace common;

#if defined(_WIN32)
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif
#endif

#define FADER_PRECISION 4096.0

// Size of the audio indicator in pixels
#define INDICATOR_THICKNESS 2

// Padding on top and bottom of vertical meters
#define METER_PADDING 1

std::weak_ptr<VolumeMeterTimer> VolumeMeter::updateTimer;

static inline Qt::CheckState GetCheckState(bool muted, bool unassigned)
{
	if (muted)
		return Qt::Checked;
	else if (unassigned)
		return Qt::PartiallyChecked;
	else
		return Qt::Unchecked;
}

static inline bool IsSourceUnassigned(obs_source_t *source)
{
	uint32_t mixes = (obs_source_get_audio_mixers(source) & ((1 << MAX_AUDIO_MIXES) - 1));
	obs_monitoring_type mt = obs_source_get_monitoring_type(source);

	return mixes == 0 && mt != OBS_MONITORING_TYPE_MONITOR_ONLY;
}

static void ShowUnassignedWarning(const char *name)
{
	auto msgBox = [=]() {
		auto ret = PLSAlertView::information(App()->getMainView(), QTStr("Alert.Title"),
						     QTStr("audio.mixer.unassigned.warning.text").arg(name),
						     QTStr("DoNotShowAgain"));

		if (ret.isChecked) {
			config_set_bool(App()->GetUserConfig(), "General", "WarnedAboutUnassignedSources", true);
			config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
		}
	};
	bool warned = config_get_bool(App()->GetUserConfig(), "General", "WarnedAboutClosingDocks");
	if (!warned) {
		QMetaObject::invokeMethod(App(), "Exec", Qt::QueuedConnection, Q_ARG(VoidFunc, msgBox));
	}
}

void VolControl::OBSVolumeChanged(void *data, float db)
{
	Q_UNUSED(db);
	VolControl *volControl = static_cast<VolControl *>(data);
	pls_check_app_exiting();
	QMetaObject::invokeMethod(volControl, "VolumeChanged");
}

void VolControl::OBSVolumeLevel(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS])
{
	pls_check_app_exiting();
	VolControl *volControl = static_cast<VolControl *>(data);

	volControl->volMeter->setLevels(magnitude, peak, inputPeak);
}

void VolControl::OBSVolumeMuted(void *data, calldata_t *calldata)
{
	VolControl *volControl = static_cast<VolControl *>(data);
	bool muted = calldata_bool(calldata, "muted");
	pls_check_app_exiting();
	QMetaObject::invokeMethod(volControl, "VolumeMuted", Q_ARG(bool, muted));
}

void VolControl::OBSVolumeRename(void *data, calldata_t *calldata)
{
	pls_check_app_exiting();
	VolControl *volControl = static_cast<VolControl *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(calldata, "source");
	const char *name = calldata_string(calldata, "new_name");
	volControl->SetName(name);
}

void VolControl::MonitorChange(pls_frontend_event event, const QVariantList &params, void *context)
{
	Q_UNUSED(params)
	pls_check_app_exiting();
	if (pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY == event) {

		auto control = (VolControl *)context;
		control->MonitorStateChangeFromAdv(
			static_cast<Qt::CheckState>(obs_source_get_monitoring_type(control->source)));
	}
}

void VolControl::VolumeChanged()
{
	pls_check_app_exiting();
	slider->blockSignals(true);
	slider->setValue((int)(obs_fader_get_deflection(obs_fader) * FADER_PRECISION));
	slider->blockSignals(false);

	updateText();
	timerDelay.start(300);
}

void VolControl::VolumeMuted(bool muted)
{
	pls_check_app_exiting();
	bool unassigned = IsSourceUnassigned(source);

	auto newState = GetCheckState(muted, unassigned);
	if (mute->checkState() != newState)
		mute->setCheckState(newState);

	volMeter->muted = muted || unassigned;

	if (!muted && unassigned) {
		mute->setCheckState(Qt::PartiallyChecked);
		/* Show notice about the source no being assigned to any tracks */
		bool has_shown_warning =
			config_get_bool(App()->GetUserConfig(), "General", "WarnedAboutUnassignedSources");
		if (!has_shown_warning)
			ShowUnassignedWarning(obs_source_get_name(source));
	}
}

void VolControl::OBSMixersOrMonitoringChanged(void *data, calldata_t *)
{

	VolControl *volControl = static_cast<VolControl *>(data);
	QMetaObject::invokeMethod(volControl, "MixersOrMonitoringChanged", Qt::QueuedConnection);
}

void VolControl::MixersOrMonitoringChanged()
{
	bool muted = obs_source_muted(source);
	bool unassigned = IsSourceUnassigned(source);

	auto newState = GetCheckState(muted, unassigned);
	if (mute->checkState() != newState)
		mute->setCheckState(newState);

	volMeter->muted = muted || unassigned;
}

void VolControl::SetMuted(bool)
{
	bool checked = mute->checkState() == Qt::Checked;
	bool prev = obs_source_muted(source);
	obs_source_set_muted(source, checked);
	bool unassigned = IsSourceUnassigned(source);

	auto undo_redo = [](const std::string &uuid, bool val) {
		OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid.c_str());
		obs_source_set_muted(source, val);
	};

	QString text = QTStr(checked ? "Undo.Volume.Mute" : "Undo.Volume.Unmute");

	const char *name = obs_source_get_name(source);
	const char *uuid = obs_source_get_uuid(source);
	OBSBasic::Get()->undo_s.add_action(text.arg(name), std::bind(undo_redo, std::placeholders::_1, prev),
					   std::bind(undo_redo, std::placeholders::_1, checked), uuid, uuid);
}

void VolControl::SliderChanged(int vol)
{
	float prev = obs_source_get_volume(source);

	obs_fader_set_deflection(obs_fader, float(vol) / FADER_PRECISION);
	updateText();

	auto undo_redo = [](const std::string &uuid, float val) {
		OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid.c_str());
		obs_source_set_volume(source, val);
	};

	float val = obs_source_get_volume(source);
	const char *name = obs_source_get_name(source);
	const char *uuid = obs_source_get_uuid(source);
	OBSBasic::Get()->undo_s.add_action(QTStr("Undo.Volume.Change").arg(name),
					   std::bind(undo_redo, std::placeholders::_1, prev),
					   std::bind(undo_redo, std::placeholders::_1, val), uuid, uuid, true);
	timerDelay.start(300);
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
	QString accText = QTStr(accTextLookup).arg(sourceName);

	slider->setAccessibleName(accText);
}

void VolControl::MonitorStateChange(int state)
{
	QString type = nullptr;
	switch (state) {
	case Qt::Checked:
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
		monitor->setToolTip(QTStr("audio.mixer.monitor.off"));
		type = "monitor and output";
		break;

	case Qt::PartiallyChecked:
		monitor->setToolTip(QTStr("audio.mixer.monitor.off"));
		type = "monitor only";
		break;
	case Qt::Unchecked:
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_NONE);
		monitor->setToolTip(QTStr("audio.mixer.monitor.on"));
		type = "none";
		break;

	default:
		break;
	}

	PLSBasic::instance()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY_BACK);

	QString log = QString("[%1 : %2] monitoring control: %3")
			      .arg(obs_source_get_id(source))
			      .arg(obs_source_get_name(source))
			      .arg(type);
	blog(LOG_INFO, "%s", qUtf8Printable(log));
}

void VolControl::LogVolumeChanged()
{
	if (slider->maximum() > slider->minimum()) {
		auto persent = (double)slider->value() / (double)(slider->maximum() - slider->minimum());
		blog(LOG_INFO, "Audio source:'%s'(%p) volume changed: %d%%", obs_source_get_name(source),
		     (void *)source.Get(), static_cast<int>(persent * 100.0));
	}
}

void VolControl::EmitConfigClicked()
{
	emit ConfigClicked();
}

void VolControl::SetMeterDecayRate(qreal q)
{
	volMeter->setPeakDecayRate(q);
}

void VolControl::setPeakMeterType(enum obs_peak_meter_type peakMeterType)
{
	volMeter->setPeakMeterType(peakMeterType);
}

VolControl::VolControl(OBSSource source_, bool showConfig, bool vertical)
	: source(std::move(source_)),
	  levelTotal(0.0f),
	  levelCount(0.0f),
	  obs_fader(obs_fader_create(OBS_FADER_LOG)),
	  obs_volmeter(obs_volmeter_create(OBS_FADER_LOG)),
	  vertical(vertical),
	  contextMenu(nullptr)
{
	nameLabel = new QLabel();
	volLabel = new QLabel();
	mute = new MuteCheckBox();

	volLabel->setObjectName("volLabel");
	volLabel->setAlignment(Qt::AlignCenter);

	monitor = new PLSSwitchButton();
	nameLabel->setObjectName("nameLabel");
	nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	volLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	installEventFilter(this);
	setAttribute(Qt::WA_StyledBackground);
	pls_set_css(this, {"PLSAudioMixer"});

	pls_frontend_add_event_callback(VolControl::MonitorChange, this);

#ifdef __APPLE__
	mute->setAttribute(Qt::WA_LayoutUsesWidgetRect);
#endif

	QString sourceName = obs_source_get_name(source);
	setObjectName(sourceName);

	if (showConfig) {
		config = new QPushButton(this);
		config->setProperty("themeID", "menuIconSmall");
		config->setAutoDefault(false);

		config->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

		config->setAccessibleName(QTStr("VolControl.Properties").arg(sourceName));
		config->setObjectName("volumeMenuButton");

		connect(config, &QAbstractButton::clicked, this, &VolControl::EmitConfigClicked);
	}

	QVBoxLayout *mainLayout = new QVBoxLayout;
	if (vertical) {
		mainLayout->setContentsMargins(0, 6, 0, 10);
		mainLayout->setSpacing(10);
		QVBoxLayout *nameVolLayout = new QVBoxLayout;
		QHBoxLayout *controlLayout = new QHBoxLayout;
		QHBoxLayout *volLayout = new QHBoxLayout;
		QHBoxLayout *meterLayout = new QHBoxLayout;
		QHBoxLayout *monitorLayout = new QHBoxLayout;

		volMeter = new VolumeMeter(nullptr, obs_volmeter, true);
		slider = new VolumeSlider(obs_fader, Qt::Vertical);
		slider->setLayoutDirection(Qt::LeftToRight);
		slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
		volMeter->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

		controlLayout->setAlignment(Qt::AlignCenter);
		meterLayout->setAlignment(Qt::AlignCenter);
		volLayout->setAlignment(Qt::AlignCenter);
		monitorLayout->setAlignment(Qt::AlignCenter);

		nameLabel->setAlignment(Qt::AlignCenter);
		volLabel->setAlignment(Qt::AlignCenter);
		nameVolLayout->setContentsMargins(0, 0, 0, 0);
		nameVolLayout->setSpacing(2);
		nameVolLayout->addWidget(nameLabel);
		nameVolLayout->addWidget(volLabel);

		controlLayout->setContentsMargins(0, 0, 0, 0);
		controlLayout->setSpacing(13);

		// Add Headphone (audio monitoring) widget here
		controlLayout->addWidget(mute);

		controlLayout->addItem(new QSpacerItem(3, 0));

		if (showConfig) {
			controlLayout->addItem(new QSpacerItem(3, 0));
			controlLayout->addWidget(config);
		}

		meterLayout->setContentsMargins(0, 0, 0, 0);
		meterLayout->setSpacing(6);
		meterLayout->addWidget(volMeter);
		meterLayout->addWidget(slider);

		monitorLayout->setContentsMargins(0, 0, 0, 0);
		monitorLayout->setSpacing(0);
		monitorLayout->addWidget(monitor);

		mainLayout->addItem(nameVolLayout);
		mainLayout->addItem(monitorLayout);
		mainLayout->addItem(meterLayout);
		mainLayout->addItem(controlLayout);

		volMeter->setFocusProxy(slider);

		// Default size can cause clipping of long names in vertical layout.
		QFont font = nameLabel->font();
		QFontInfo info(font);
		font.setPointSizeF(0.8 * info.pointSizeF());
		nameLabel->setFont(font);

		setMinimumHeight(246);
		setFixedWidth(120);
	} else {
		mainLayout->setContentsMargins(12, 4, 0, 13);
		mainLayout->setSpacing(0);

		QHBoxLayout *textLayout = new QHBoxLayout;
		QHBoxLayout *botLayout = new QHBoxLayout;
		QHBoxLayout *textLeftLayout = pls_new<QHBoxLayout>();
		QHBoxLayout *textRightLayout = pls_new<QHBoxLayout>();

		volMeter = new VolumeMeter(nullptr, obs_volmeter, false);
		slider = new VolumeSlider(obs_fader, Qt::Horizontal);
		slider->setLayoutDirection(Qt::LeftToRight);

		auto spacer = pls_new<QLabel>();
		spacer->setObjectName("volumeSperatorLabel");

		textLayout->setContentsMargins(0, 0, 12, 8);
		textLayout->setSpacing(0);

		botLayout->setContentsMargins(0, 3, 0, 0);
		botLayout->setSpacing(10);
		botLayout->addWidget(slider);
		botLayout->addWidget(mute);
		//botLayout->setAlignment(slider, Qt::AlignVCenter);
		//botLayout->setAlignment(mute, Qt::AlignVCenter);

		textLeftLayout->setSpacing(5);
		textLeftLayout->setContentsMargins(0, 0, 0, 0);
		textLeftLayout->setAlignment(Qt::AlignLeft);

		textLeftLayout->addWidget(nameLabel);
		textLeftLayout->addWidget(spacer);
		textLeftLayout->addWidget(volLabel);

		textRightLayout->setSpacing(0);
		textRightLayout->setContentsMargins(15, 0, 0, 2);
		textRightLayout->setAlignment(Qt::AlignRight);
		textRightLayout->addWidget(monitor);

		if (showConfig) {
			botLayout->addWidget(config);
			//botLayout->setAlignment(config, Qt::AlignVCenter);
		}

		textLayout->addLayout(textLeftLayout);
		textLayout->addLayout(textRightLayout);
		textLayout->setAlignment(textLeftLayout, Qt::AlignLeft);
		textLayout->setAlignment(textRightLayout, Qt::AlignRight);

		mainLayout->addItem(textLayout);
		mainLayout->addWidget(volMeter);
		mainLayout->addItem(botLayout);

		volMeter->setFocusProxy(slider);
	}

	setLayout(mainLayout);

	SetName(sourceName);

	slider->setMinimum(0);
	slider->setMaximum(int(FADER_PRECISION));

	bool muted = obs_source_muted(source);
	bool unassigned = IsSourceUnassigned(source);
	mute->setCheckState(GetCheckState(muted, unassigned));
	volMeter->muted = muted || unassigned;
	mute->setAccessibleName(QTStr("VolControl.Mute").arg(sourceName));
	obs_fader_add_callback(obs_fader, OBSVolumeChanged, this);
	obs_volmeter_add_callback(obs_volmeter, OBSVolumeLevel, this);

	sigs.emplace_back(obs_source_get_signal_handler(source), "mute", OBSVolumeMuted, this);
	sigs.emplace_back(obs_source_get_signal_handler(source), "audio_mixers", OBSMixersOrMonitoringChanged, this);
	sigs.emplace_back(obs_source_get_signal_handler(source), "audio_monitoring", OBSMixersOrMonitoringChanged,
			  this);
	sigs.emplace_back(obs_source_get_signal_handler(source), "rename", OBSVolumeRename, this);

	QWidget::connect(slider, &VolumeSlider::valueChanged, this, &VolControl::SliderChanged);
	QWidget::connect(mute, &MuteCheckBox::clicked, this, &VolControl::SetMuted);

	MonitorStateChangeFromAdv(static_cast<Qt::CheckState>(obs_source_get_monitoring_type(source)));
	monitor->setObjectName("volumeMonitorCheck");
	connect(monitor, &PLSSwitchButton::stateChanged, this, &VolControl::MonitorStateChange);

	obs_fader_attach_source(obs_fader, source);
	obs_volmeter_attach_source(obs_volmeter, source);

	/* Call volume changed once to init the slider position and label */
	VolumeChanged();

	setProperty("vertical", vertical);
	pls_flush_style(this);

	timerDelay.setSingleShot(true);
	connect(&timerDelay, &QTimer::timeout, this, &VolControl::LogVolumeChanged);
}

void VolControl::EnableSlider(bool enable)
{
	slider->setEnabled(enable);
	pls_flush_style(this);
}

VolControl::~VolControl()
{
	timerDelay.stop();
	obs_fader_remove_callback(obs_fader, OBSVolumeChanged, this);
	obs_volmeter_remove_callback(obs_volmeter, OBSVolumeLevel, this);
	pls_frontend_remove_event_callback(VolControl::MonitorChange, this);

	sigs.clear();

	if (contextMenu)
		contextMenu->close();
}

static inline QColor color_from_int(long long val)
{
	QColor color(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff, (val >> 24) & 0xff);
	color.setAlpha(255);

	return color;
}

QColor VolumeMeter::getBackgroundNominalColor() const
{
	return p_backgroundNominalColor;
}

QColor VolumeMeter::getBackgroundNominalColorDisabled() const
{
	return backgroundNominalColorDisabled;
}

void VolumeMeter::setBackgroundNominalColor(QColor c)
{
	p_backgroundNominalColor = std::move(c);

	if (config_get_bool(App()->GetUserConfig(), "Accessibility", "OverrideColors")) {
		backgroundNominalColor =
			color_from_int(config_get_int(App()->GetUserConfig(), "Accessibility", "MixerGreen"));
	} else {
		backgroundNominalColor = p_backgroundNominalColor;
	}
}

void VolumeMeter::setBackgroundNominalColorDisabled(QColor c)
{
	backgroundNominalColorDisabled = std::move(c);
}

QColor VolumeMeter::getBackgroundWarningColor() const
{
	return p_backgroundWarningColor;
}

QColor VolumeMeter::getBackgroundWarningColorDisabled() const
{
	return backgroundWarningColorDisabled;
}

void VolumeMeter::setBackgroundWarningColor(QColor c)
{
	p_backgroundWarningColor = std::move(c);

	if (config_get_bool(App()->GetUserConfig(), "Accessibility", "OverrideColors")) {
		backgroundWarningColor =
			color_from_int(config_get_int(App()->GetUserConfig(), "Accessibility", "MixerYellow"));
	} else {
		backgroundWarningColor = p_backgroundWarningColor;
	}
}

void VolumeMeter::setBackgroundWarningColorDisabled(QColor c)
{
	backgroundWarningColorDisabled = std::move(c);
}

QColor VolumeMeter::getBackgroundErrorColor() const
{
	return p_backgroundErrorColor;
}

QColor VolumeMeter::getBackgroundErrorColorDisabled() const
{
	return backgroundErrorColorDisabled;
}

void VolumeMeter::setBackgroundErrorColor(QColor c)
{
	p_backgroundErrorColor = std::move(c);

	if (config_get_bool(App()->GetUserConfig(), "Accessibility", "OverrideColors")) {
		backgroundErrorColor =
			color_from_int(config_get_int(App()->GetUserConfig(), "Accessibility", "MixerRed"));
	} else {
		backgroundErrorColor = p_backgroundErrorColor;
	}
}

void VolumeMeter::setBackgroundErrorColorDisabled(QColor c)
{
	backgroundErrorColorDisabled = std::move(c);
}

QColor VolumeMeter::getForegroundNominalColor() const
{
	return p_foregroundNominalColor;
}

QColor VolumeMeter::getForegroundNominalColorDisabled() const
{
	return foregroundNominalColorDisabled;
}

void VolumeMeter::setForegroundNominalColor(QColor c)
{
	p_foregroundNominalColor = std::move(c);

	if (config_get_bool(App()->GetUserConfig(), "Accessibility", "OverrideColors")) {
		foregroundNominalColor =
			color_from_int(config_get_int(App()->GetUserConfig(), "Accessibility", "MixerGreenActive"));
	} else {
		foregroundNominalColor = p_foregroundNominalColor;
	}
}

void VolumeMeter::setForegroundNominalColorDisabled(QColor c)
{
	foregroundNominalColorDisabled = std::move(c);
}

QColor VolumeMeter::getForegroundWarningColor() const
{
	return p_foregroundWarningColor;
}

QColor VolumeMeter::getForegroundWarningColorDisabled() const
{
	return foregroundWarningColorDisabled;
}

void VolumeMeter::setForegroundWarningColor(QColor c)
{
	p_foregroundWarningColor = std::move(c);

	if (config_get_bool(App()->GetUserConfig(), "Accessibility", "OverrideColors")) {
		foregroundWarningColor =
			color_from_int(config_get_int(App()->GetUserConfig(), "Accessibility", "MixerYellowActive"));
	} else {
		foregroundWarningColor = p_foregroundWarningColor;
	}
}

void VolumeMeter::setForegroundWarningColorDisabled(QColor c)
{
	foregroundWarningColorDisabled = std::move(c);
}

QColor VolumeMeter::getForegroundErrorColor() const
{
	return p_foregroundErrorColor;
}

QColor VolumeMeter::getForegroundErrorColorDisabled() const
{
	return foregroundErrorColorDisabled;
}

void VolumeMeter::setForegroundErrorColor(QColor c)
{
	p_foregroundErrorColor = std::move(c);

	if (config_get_bool(App()->GetUserConfig(), "Accessibility", "OverrideColors")) {
		foregroundErrorColor =
			color_from_int(config_get_int(App()->GetUserConfig(), "Accessibility", "MixerRedActive"));
	} else {
		foregroundErrorColor = p_foregroundErrorColor;
	}
}

void VolumeMeter::setForegroundErrorColorDisabled(QColor c)
{
	foregroundErrorColorDisabled = std::move(c);
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

int VolumeMeter::getMeterThickness() const
{
	return meterThickness;
}

void VolumeMeter::setMeterThickness(int v)
{
	meterThickness = v;
	recalculateLayout = true;
}

qreal VolumeMeter::getMeterFontScaling() const
{
	return meterFontScaling;
}

void VolumeMeter::setMeterFontScaling(qreal v)
{
	meterFontScaling = v;
	recalculateLayout = true;
}

void VolControl::refreshColors()
{
	volMeter->setBackgroundNominalColor(volMeter->getBackgroundNominalColor());
	volMeter->setBackgroundWarningColor(volMeter->getBackgroundWarningColor());
	volMeter->setBackgroundErrorColor(volMeter->getBackgroundErrorColor());
	volMeter->setForegroundNominalColor(volMeter->getForegroundNominalColor());
	volMeter->setForegroundWarningColor(volMeter->getForegroundWarningColor());
	volMeter->setForegroundErrorColor(volMeter->getForegroundErrorColor());
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

VolumeMeter::VolumeMeter(QWidget *parent, obs_volmeter_t *obs_volmeter, bool vertical)
	: QWidget(parent),
	  obs_volmeter(obs_volmeter),
	  vertical(vertical)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);

#if defined(Q_OS_MACOS)
	setAttribute(Qt::WA_NativeWindow, true);
#endif

	// Default meter settings, they only show if
	// there is no stylesheet, do not remove.
	backgroundNominalColor.setRgb(0x26, 0x7f, 0x26); // Dark green
	backgroundWarningColor.setRgb(0x7f, 0x7f, 0x26); // Dark yellow
	backgroundErrorColor.setRgb(0x7f, 0x26, 0x26);   // Dark red
	foregroundNominalColor.setRgb(0x4c, 0xff, 0x4c); // Bright green
	foregroundWarningColor.setRgb(0xff, 0xff, 0x4c); // Bright yellow
	foregroundErrorColor.setRgb(0xff, 0x4c, 0x4c);   // Bright red

	backgroundNominalColorDisabled.setRgb(90, 90, 90);
	backgroundWarningColorDisabled.setRgb(117, 117, 117);
	backgroundErrorColorDisabled.setRgb(65, 65, 65);
	foregroundNominalColorDisabled.setRgb(163, 163, 163);
	foregroundWarningColorDisabled.setRgb(217, 217, 217);
	foregroundErrorColorDisabled.setRgb(113, 113, 113);

	clipColor.setRgb(0xff, 0xff, 0xff);      // Bright white
	magnitudeColor.setRgb(0x00, 0x00, 0x00); // Black
	majorTickColor.setRgb(0x00, 0x00, 0x00); // Black
	minorTickColor.setRgb(0x32, 0x32, 0x32); // Dark gray
	minimumLevel = -60.0;                    // -60 dB
	warningLevel = -20.0;                    // -20 dB
	errorLevel = -9.0;                       //  -9 dB
	clipLevel = -0.5;                        //  -0.5 dB
	minimumInputLevel = -50.0;               // -50 dB
	peakDecayRate = 11.76;                   //  20 dB / 1.7 sec
	magnitudeIntegrationTime = 0.3;          //  99% in 300 ms
	peakHoldDuration = 20.0;                 //  20 seconds
	inputPeakHoldDuration = 1.0;             //  1 second
	meterThickness = 2;                      // Bar thickness in pixels
	meterFontScaling = 0.7;                  // Font size for numbers is 70% of Widget's font size
	channels = (int)audio_output_get_channels(obs_get_audio());

	doLayout();
	updateTimerRef = updateTimer.lock();
	if (!updateTimerRef) {
		updateTimerRef = std::make_shared<VolumeMeterTimer>();
		updateTimerRef->setTimerType(Qt::PreciseTimer);
		updateTimerRef->start(16);
		updateTimer = updateTimerRef;
	}

	updateTimerRef->AddVolControl(this);
}

VolumeMeter::~VolumeMeter()
{
	updateTimerRef->RemoveVolControl(this);
}

void VolumeMeter::setLevels(const float magnitude[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS],
			    const float inputPeak[MAX_AUDIO_CHANNELS])
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

bool VolumeMeter::needLayoutChange()
{
	if (pls_get_app_exiting())
		return false;

	int currentNrAudioChannels = obs_volmeter_get_nr_channels(obs_volmeter);

	if (!currentNrAudioChannels) {
		struct obs_audio_info oai;
		obs_get_audio_info(&oai);
		currentNrAudioChannels = (oai.speakers == SPEAKERS_MONO) ? 1 : 2;
	}

	if (displayNrAudioChannels != currentNrAudioChannels) {
		displayNrAudioChannels = currentNrAudioChannels;
		recalculateLayout = true;
	}

	return recalculateLayout;
}

// When this is called from the constructor, obs_volmeter_get_nr_channels has not
// yet been called and Q_PROPERTY settings have not yet been read from the
// stylesheet.
inline void VolumeMeter::doLayout()
{
	QMutexLocker locker(&dataMutex);

	recalculateLayout = false;

	tickFont = font();
	QFontInfo info(tickFont);
	tickFont.setPointSizeF(info.pointSizeF() * meterFontScaling);
	QFontMetrics metrics(tickFont);
	if (vertical) {
		// Each meter channel is meterThickness pixels wide, plus one pixel
		// between channels, but not after the last.
		// Add 4 pixels for ticks, space to hold our longest label in this font,
		// and a few pixels before the fader.
		//QRect scaleBounds = metrics.boundingRect("-88");
		//setMinimumSize(displayNrAudioChannels * (meterThickness + 1) -
		//1 + 4 + scaleBounds.width() + 2,
		//130);
		setMinimumWidth((displayNrAudioChannels - 1) * 7 + 12);
	} else {
		// Each meter channel is meterThickness pixels high, plus one pixel
		// between channels, but not after the last.
		// Add 4 pixels for ticks, and space high enough to hold our label in
		// this font, presuming that digits don't have descenders.
		setMinimumSize(130, (displayNrAudioChannels - 1) * 5 + displayNrAudioChannels * meterThickness + 2);
	}

	resetLevels();
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
		displayPeak[channelNr] =
			std::clamp(displayPeak[channelNr] - decay, std::min(currentPeak[channelNr], 0.f), 0.f);
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

	if (currentInputPeak[channelNr] >= displayInputPeakHold[channelNr] ||
	    !isfinite(displayInputPeakHold[channelNr])) {
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
		float attack = float((currentMagnitude[channelNr] - displayMagnitude[channelNr]) *
				     (timeSinceLastRedraw / magnitudeIntegrationTime) * 0.99);
		displayMagnitude[channelNr] =
			std::clamp(displayMagnitude[channelNr] + attack, (float)minimumLevel, 0.f);
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

void VolumeMeter::paintHTicks(QPainter &painter, int x, int y, int width)
{
	qreal scale = width / minimumLevel;

	painter.setFont(tickFont);
	QFontMetrics metrics(tickFont);
	painter.setPen(majorTickColor);

	// Draw major tick lines and numeric indicators.
	for (int i = 0; i >= minimumLevel; i -= 5) {
		int position = int(x + width - (i * scale) - 1);
		QString str = QString::number(i);

		// Center the number on the tick, but don't overflow
		QRect textBounds = metrics.boundingRect(str);
		int pos;
		if (i == 0) {
			pos = position - textBounds.width();
		} else {
			pos = position - (textBounds.width() / 2);
			if (pos < 0)
				pos = 0;
		}
		painter.drawText(pos, y + 4 + metrics.capHeight(), str);

		painter.drawLine(position, y, position, y + 2);
	}
}

void VolumeMeter::paintVTicks(QPainter &painter, int x, int y, int height)
{
	qreal scale = height / minimumLevel;

	painter.setFont(tickFont);
	QFontMetrics metrics(tickFont);
	painter.setPen(majorTickColor);

	// Draw major tick lines and numeric indicators.
	for (int i = 0; i >= minimumLevel; i -= 5) {
		int position = y + int(i * scale) + METER_PADDING;
		QString str = QString::number(i);

		// Center the number on the tick, but don't overflow
		if (i == 0) {
			painter.drawText(x + 6, position + metrics.capHeight(), str);
		} else {
			painter.drawText(x + 4, position + (metrics.capHeight() / 2), str);
		}

		painter.drawLine(x, position, x + 2, position);
	}
}

#define CLIP_FLASH_DURATION_MS 1000

inline int VolumeMeter::convertToInt(float number)
{
	constexpr int min = std::numeric_limits<int>::min();
	constexpr int max = std::numeric_limits<int>::max();

	// NOTE: Conversion from 'const int' to 'float' changes max value from 2147483647 to 2147483648
	if (number >= (float)max)
		return max;
	else if (number < min)
		return min;
	else
		return int(number);
}

void VolumeMeter::paintHMeter(QPainter &painter, int x, int y, int width, int height, float magnitude, float peak,
			      float peakHold)
{
	qreal scale = width / minimumLevel;

	QMutexLocker locker(&dataMutex);
	int minimumPosition = x + 0;
	int maximumPosition = x + width;
	int magnitudePosition = x + width - convertToInt(magnitude * scale);
	int peakPosition = x + width - convertToInt(peak * scale);
	int peakHoldPosition = x + width - convertToInt(peakHold * scale);
	int warningPosition = x + width - convertToInt(warningLevel * scale);
	int errorPosition = x + width - convertToInt(errorLevel * scale);

	int nominalLength = warningPosition - minimumPosition;
	int warningLength = errorPosition - warningPosition;
	int errorLength = maximumPosition - errorPosition;
	locker.unlock();

	if (clipping) {
		peakPosition = maximumPosition;
	}

	if (peakPosition < minimumPosition) {
		painter.fillRect(minimumPosition, y, nominalLength, height,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < warningPosition) {
		painter.fillRect(minimumPosition, y, peakPosition - minimumPosition, height,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(peakPosition, y, warningPosition - peakPosition, height,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < errorPosition) {
		painter.fillRect(minimumPosition, y, nominalLength, height,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(warningPosition, y, peakPosition - warningPosition, height,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
		painter.fillRect(peakPosition, y, errorPosition - peakPosition, height,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < maximumPosition) {
		painter.fillRect(minimumPosition, y, nominalLength, height,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
		painter.fillRect(errorPosition, y, peakPosition - errorPosition, height,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);
		painter.fillRect(peakPosition, y, maximumPosition - peakPosition, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else {
		if (!clipping) {
			QTimer::singleShot(CLIP_FLASH_DURATION_MS, this, [&]() { clipping = false; });
			clipping = true;
		}

		int end = errorLength + warningLength + nominalLength;
		painter.fillRect(minimumPosition, y, end, height,
				 QBrush(muted ? foregroundErrorColorDisabled : foregroundErrorColor));
	}

	if (peakHoldPosition - 3 < minimumPosition)
		; // Peak-hold below minimum, no drawing.
	else if (peakHoldPosition < warningPosition)
		painter.fillRect(peakHoldPosition - 3, y, 3, height,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
	else if (peakHoldPosition < errorPosition)
		painter.fillRect(peakHoldPosition - 3, y, 3, height,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
	else
		painter.fillRect(peakHoldPosition - 3, y, 3, height,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);

	if (magnitudePosition - 3 >= minimumPosition)
		painter.fillRect(magnitudePosition - 3, y, 3, height, magnitudeColor);
}

void VolumeMeter::paintVMeter(QPainter &painter, int x, int y, int width, int height, float magnitude, float peak,
			      float peakHold)
{
	qreal scale = height / minimumLevel;

	QMutexLocker locker(&dataMutex);
	int minimumPosition = y + 0;
	int maximumPosition = y + height;
	int magnitudePosition = y + height - convertToInt(magnitude * scale);
	int peakPosition = y + height - convertToInt(peak * scale);
	int peakHoldPosition = y + height - convertToInt(peakHold * scale);
	int warningPosition = y + height - convertToInt(warningLevel * scale);
	int errorPosition = y + height - convertToInt(errorLevel * scale);

	int nominalLength = warningPosition - minimumPosition;
	int warningLength = errorPosition - warningPosition;
	int errorLength = maximumPosition - errorPosition;
	locker.unlock();

	if (clipping) {
		peakPosition = maximumPosition;
	}

	if (peakPosition < minimumPosition) {
		painter.fillRect(x, minimumPosition, width, nominalLength,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < warningPosition) {
		painter.fillRect(x, minimumPosition, width, peakPosition - minimumPosition,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(x, peakPosition, width, warningPosition - peakPosition,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < errorPosition) {
		painter.fillRect(x, minimumPosition, width, nominalLength,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(x, warningPosition, width, peakPosition - warningPosition,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
		painter.fillRect(x, peakPosition, width, errorPosition - peakPosition,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < maximumPosition) {
		painter.fillRect(x, minimumPosition, width, nominalLength,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
		painter.fillRect(x, errorPosition, width, peakPosition - errorPosition,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);
		painter.fillRect(x, peakPosition, width, maximumPosition - peakPosition,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else {
		if (!clipping) {
			QTimer::singleShot(CLIP_FLASH_DURATION_MS, this, [&]() { clipping = false; });
			clipping = true;
		}

		int end = errorLength + warningLength + nominalLength;
		painter.fillRect(x, minimumPosition, width, end,
				 QBrush(muted ? foregroundErrorColorDisabled : foregroundErrorColor));
	}

	if (peakHoldPosition - 3 < minimumPosition)
		; // Peak-hold below minimum, no drawing.
	else if (peakHoldPosition < warningPosition)
		painter.fillRect(x, peakHoldPosition - 3, width, 3,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
	else if (peakHoldPosition < errorPosition)
		painter.fillRect(x, peakHoldPosition - 3, width, 3,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
	else
		painter.fillRect(x, peakHoldPosition - 3, width, 3,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);

	if (magnitudePosition - 3 >= minimumPosition)
		painter.fillRect(x, magnitudePosition - 3, width, 3, magnitudeColor);
}

void VolumeMeter::paintEvent(QPaintEvent *event)
{
	if (pls_get_app_exiting())
		return;

	uint64_t ts = os_gettime_ns();
	qreal timeSinceLastRedraw = (ts - lastRedrawTime) * 0.000000001;
	calculateBallistics(ts, timeSinceLastRedraw);
	bool idle = detectIdle(ts);

	QRect widgetRect = rect();
	int width = widgetRect.width();
	int height = widgetRect.height();

	QPainter painter(this);

	// Paint window background color (as widget is opaque)
	QColor background = palette().color(QPalette::ColorRole::Window);
	painter.fillRect(widgetRect, background);

	//if (vertical)
	//height -= METER_PADDING * 2;

	// timerEvent requests update of the bar(s) only, so we can avoid the
	// overhead of repainting the scale and labels.
	if (event->region().boundingRect() != getBarRect()) {
		if (needLayoutChange())
			doLayout();

#if 0
		if (vertical) {
			paintVTicks(painter,
				    displayNrAudioChannels *
						    (meterThickness + 1) -
					    1,
				    0, height - (INDICATOR_THICKNESS + 3));
		} else {
			paintHTicks(painter, INDICATOR_THICKNESS + 3,
				    displayNrAudioChannels *
						    (meterThickness + 1) -
					    1,
				    width - (INDICATOR_THICKNESS + 3));
		}
#endif
	}

	if (vertical) {
		// Invert the Y axis to ease the math
		painter.translate(0, height);
		painter.scale(1, -1);
	}

	for (int channelNr = 0; channelNr < displayNrAudioChannels; channelNr++) {

		int channelNrFixed = (displayNrAudioChannels == 1 && channels > 2) ? 2 : channelNr;

		if (vertical) {
			auto bar_height = idle ? (height + METER_PADDING * 2) : height - (INDICATOR_THICKNESS + 1);
			paintVMeter(painter, channelNr * 7 + 8, idle ? 0 : (INDICATOR_THICKNESS + 2), meterThickness,
				    bar_height, displayMagnitude[channelNrFixed], displayPeak[channelNrFixed],
				    displayPeakHold[channelNrFixed]);
		} else {
			auto bar_width = width - (INDICATOR_THICKNESS + 2) - 10;
			paintHMeter(painter, idle ? 0 : INDICATOR_THICKNESS + 2, (meterThickness + 5) * channelNr,
				    bar_width, meterThickness, displayMagnitude[channelNrFixed],
				    displayPeak[channelNrFixed], displayPeakHold[channelNrFixed]);
		}
		if (idle)
			continue;

		// By not drawing the input meter boxes the user can
		// see that the audio stream has been stopped, without
		// having too much visual impact.
		if (vertical)
			paintInputMeter(painter, channelNr * 7 + 8, 0, meterThickness, INDICATOR_THICKNESS,
					displayInputPeakHold[channelNrFixed]);
		else
			paintInputMeter(painter, 0, (meterThickness + 5) * channelNr, INDICATOR_THICKNESS,
					meterThickness, displayInputPeakHold[channelNrFixed]);
	}

	lastRedrawTime = ts;
}

QRect VolumeMeter::getBarRect() const
{
	QRect rec = rect();
	if (vertical)
		rec.setWidth(displayNrAudioChannels * 7 + 10);
	else
		rec.setHeight((displayNrAudioChannels - 1) * 5 + displayNrAudioChannels * meterThickness);

	return rec;
}

void VolumeMeter::changeEvent(QEvent *e)
{
	if (e->type() == QEvent::StyleChange)
		recalculateLayout = true;

	QWidget::changeEvent(e);
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
	for (VolumeMeter *meter : volumeMeters) {
		if (meter->needLayoutChange()) {
			// Tell paintEvent to update layout and paint everything
			if (!meter->visibleRegion().boundingRect().isValid())
				meter->doLayout();
			meter->update();
		} else {
			// Tell paintEvent to paint only the bars
			meter->update(meter->getBarRect());
		}
	}
}

VolumeSlider::VolumeSlider(obs_fader_t *fader, QWidget *parent) : AbsoluteSlider(parent)
{
	fad = fader;
}

VolumeSlider::VolumeSlider(obs_fader_t *fader, Qt::Orientation orientation, QWidget *parent)
	: AbsoluteSlider(orientation, parent)
{
	fad = fader;
}

bool VolumeSlider::getDisplayTicks() const
{
	return displayTicks;
}

void VolumeSlider::setDisplayTicks(bool display)
{
	displayTicks = display;
}

void VolumeSlider::paintEvent(QPaintEvent *event)
{
	if (!getDisplayTicks()) {
		QSlider::paintEvent(event);
		return;
	}

	QPainter painter(this);
	QColor tickColor(91, 98, 115, 255);

	obs_fader_conversion_t fader_db_to_def = obs_fader_db_to_def(fad);

	QStyleOptionSlider opt;
	initStyleOption(&opt);

	QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
	QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

	if (orientation() == Qt::Horizontal) {
		const int sliderWidth = groove.width() - handle.width();

		float tickLength = groove.height() * 1.5;
		tickLength = std::max((int)tickLength + groove.height(), 8 + groove.height());

		float yPos = groove.center().y() - (tickLength / 2) + 1;

		for (int db = -10; db >= -90; db -= 10) {
			float tickValue = fader_db_to_def(db);

			float xPos = groove.left() + (tickValue * sliderWidth) + (handle.width() / 2);
			painter.fillRect(xPos, yPos, 1, tickLength, tickColor);
		}
	}

	if (orientation() == Qt::Vertical) {
		const int sliderHeight = groove.height() - handle.height();

		float tickLength = groove.width() * 1.5;
		tickLength = std::max((int)tickLength + groove.width(), 8 + groove.width());

		float xPos = groove.center().x() - (tickLength / 2) + 1;

		for (int db = -10; db >= -96; db -= 10) {
			float tickValue = fader_db_to_def(db);

			float yPos =
				groove.height() + groove.top() - (tickValue * sliderHeight) - (handle.height() / 2);
			painter.fillRect(xPos, yPos, tickLength, 1, tickColor);
		}
	}

	QSlider::paintEvent(event);
}

VolumeAccessibleInterface::VolumeAccessibleInterface(QWidget *w) : QAccessibleWidget(w) {}

VolumeSlider *VolumeAccessibleInterface::slider() const
{
	return qobject_cast<VolumeSlider *>(object());
}

QString VolumeAccessibleInterface::text(QAccessible::Text t) const
{
	if (slider()->isVisible()) {
		switch (t) {
		case QAccessible::Text::Value:
			return currentValue().toString();
		default:
			break;
		}
	}
	return QAccessibleWidget::text(t);
}

QVariant VolumeAccessibleInterface::currentValue() const
{
	QString text;
	float db = obs_fader_get_db(slider()->fad);

	if (db < -96.0f)
		text = "-inf dB";
	else
		text = QString::number(db, 'f', 1).append(" dB");

	return text;
}

void VolumeAccessibleInterface::setCurrentValue(const QVariant &value)
{
	slider()->setValue(value.toInt());
}

QVariant VolumeAccessibleInterface::maximumValue() const
{
	return slider()->maximum();
}

QVariant VolumeAccessibleInterface::minimumValue() const
{
	return slider()->minimum();
}

QVariant VolumeAccessibleInterface::minimumStepSize() const
{
	return slider()->singleStep();
}

QAccessible::Role VolumeAccessibleInterface::role() const
{
	return QAccessible::Role::Slider;
}

void VolControl::MonitorStateChangeFromAdv(Qt::CheckState state)
{
	QSignalBlocker blocker(monitor);
	switch (state) {
	case Qt::Checked:
		monitor->setChecked(true);
		monitor->setToolTip(QTStr("audio.mixer.monitor.off"));

		break;
	case Qt::Unchecked:
		monitor->setChecked(false);
		monitor->setToolTip(QTStr("audio.mixer.monitor.on"));
		break;
	case Qt::PartiallyChecked:
		monitor->setChecked(true);
		monitor->setToolTip(QTStr("audio.mixer.monitor.off"));
		break;
	default:
		break;
	}
}

void VolControl::setClickState(bool clicked)
{
	setProperty("pressed", clicked);
	pls_flush_style(nameLabel);
	pls_flush_style(volLabel);
}

void VolControl::updateMouseState(bool hover)
{
	setProperty("hover", hover);
	pls_flush_style(this);
	pls_flush_style(volMeter);
}

bool VolControl::eventFilter(QObject *watched, QEvent *e)
{
	if (e->type() == QEvent::Resize) {
		if (watched == this) {
			pls_async_call(this, [this]() {
				pls_check_app_exiting();
				SetName(currentDisplayName);
			});
			return true;
		}
	} else if (e->type() == QEvent::MouseMove) {
		updateMouseState(true);
	} else if (e->type() == QEvent::Leave) {
		updateMouseState(false);
	} else if (e->type() == QEvent::Enter) {
		updateMouseState(true);
	}
	return QWidget::eventFilter(watched, e);
}

QString VolControl::GetName() const
{
	return currentDisplayName;
}

void VolControl::SetName(const QString &newName)
{
	currentDisplayName = newName;
	QFontMetrics fontWidth(nameLabel->font());
	int space = vertical ? 10 * 2 : 148;
	if (fontWidth.horizontalAdvance(currentDisplayName) > width() - space) {
		nameLabel->setText(fontWidth.elidedText(newName, Qt::ElideRight, width() - space));
		nameLabel->setToolTip(currentDisplayName);
	} else {
		nameLabel->setText(newName);
		nameLabel->setToolTip("");
	}
}