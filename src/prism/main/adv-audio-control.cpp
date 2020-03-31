#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include "qt-wrappers.hpp"
#include "pls-app.hpp"
#include "adv-audio-control.hpp"
#include "window-basic-main.hpp"
#include "spinbox.hpp"
#include "combobox.hpp"
#include "frontend-api.h"
#include "window-basic-main.hpp"
#include "login-common-helper.hpp"

#ifndef NSEC_PER_MSEC
#define NSEC_PER_MSEC 1000000
#endif

#define NAME_LABEL_WIDTH 100
#define MONITOR_TYPE_WIDTH 200
#define VOLUME_SPINBOX_WIDTH 110
#define MONO_CONTAINER_WIDTH 80
#define BALANCE_WIDTH 159
#define TRACKS_MAX_CONTAINER_WIDTH 268
#define TRACKS_MIN_CONTAINER_WIDTH 258
#define NAME_LABEL_HEIGHT 40

#define MIN_DB -96.0
#define MAX_DB 26.0

PLSAdvAudioCtrl::PLSAdvAudioCtrl(QGridLayout *, obs_source_t *source_) : source(source_)
{

	QHBoxLayout *hlayout;
	signal_handler_t *handler = obs_source_get_signal_handler(source);

	//get source volumn
	float vol = obs_source_get_volume(source);

	//get source flags(indicate the source type)
	uint32_t flags = obs_source_get_flags(source);

	//get audio mixer count
	uint32_t mixers = obs_source_get_audio_mixers(source);

	volChangedSignal.Connect(handler, "volume", OBSSourceVolumeChanged, this);
	syncOffsetSignal.Connect(handler, "audio_sync", OBSSourceSyncChanged, this);
	flagsSignal.Connect(handler, "update_flags", OBSSourceFlagsChanged, this);
	mixersSignal.Connect(handler, "audio_mixers", OBSSourceMixersChanged, this);

	pls_frontend_add_event_callback(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY_BACK, PLSAdvAudioCtrl::monitorControlChange, this);

	// source name label
	nameLabel = new QLabel();
	nameLabel->setObjectName("sourceNameLabel");
	const char *sourceName = obs_source_get_name(source);
	nameLabel->setToolTip(QT_UTF8(sourceName));
	nameLabel->setStyleSheet("#sourceNameLabel{ min-width:100px; min-height:40px; font-size:14px; font-weight:bold; color:#bababa; padding:0px; }");
	nameLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

	// audio monitor combox
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	int idx;
	monitoringType = new PLSComboBox();
	monitoringType->setObjectName(QTStr("advMonitoringType"));
	monitoringType->addItem(QTStr("Basic.AdvAudio.Monitoring.None"), (int)OBS_MONITORING_TYPE_NONE);
	monitoringType->addItem(QTStr("Basic.AdvAudio.Monitoring.MonitorOnly"), (int)OBS_MONITORING_TYPE_MONITOR_ONLY);
	monitoringType->addItem(QTStr("Basic.AdvAudio.Monitoring.Both"), (int)OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
	int mt = (int)obs_source_get_monitoring_type(source);
	idx = monitoringType->findData(mt);
	monitoringType->setCurrentIndex(idx);
	monitoringType->setFixedSize(MONITOR_TYPE_WIDTH, NAME_LABEL_HEIGHT);
	QWidget::connect(monitoringType, SIGNAL(currentIndexChanged(int)), this, SLOT(monitoringTypeChanged(int)));
#endif

	// audio volume spin box
	volume = new PLSDoubleSpinBox();
	volume->setObjectName(QTStr("advVolumnSpinBox"));
	volume->setMinimum(MIN_DB - 0.1);
	volume->setMaximum(MAX_DB);
	volume->setSingleStep(0.1);
	volume->setDecimals(1);
	volume->setSuffix(" dB");
	volume->setValue(obs_mul_to_db(vol));
	volume->setMinimumSize(VOLUME_SPINBOX_WIDTH, NAME_LABEL_HEIGHT);
	volume->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	if (volume->value() < MIN_DB)
		volume->setSpecialValueText("-inf dB");
	QWidget::connect(volume, SIGNAL(valueChanged(double)), this, SLOT(volumeChanged(double)));

	// balance slider
	balanceContainer = new QWidget();

	hlayout = new QHBoxLayout();
	hlayout->setContentsMargins(0, 0, 0, 0);
	hlayout->setSpacing(10);

	balanceContainer->setLayout(hlayout);
	balanceContainer->setFixedWidth(BALANCE_WIDTH);

	labelL = new QLabel();
	labelL->setObjectName("advLeftBalanceLabel");
	labelL->setText("L");

	labelR = new QLabel();
	labelR->setObjectName("advRightBalanceLabel");
	labelR->setText("R");

	balance = new BalanceSlider();
	balance->setObjectName("advBalanceSlider");
	balance->setOrientation(Qt::Horizontal);
	balance->setMinimum(0);
	balance->setMaximum(100);
	balance->setTickPosition(QSlider::TicksAbove);
	balance->setTickInterval(50);
	PLSBasic *main = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());
	const char *speakers = config_get_string(main->Config(), "Audio", "ChannelSetup");
	if (strcmp(speakers, "Mono") == 0)
		balance->setEnabled(false);
	else
		balance->setEnabled(true);
	float bal = obs_source_get_balance_value(source) * 100.0f;
	balance->setValue((int)bal);

	speaker_layout sl = obs_source_get_speaker_layout(source);
	if (sl == SPEAKERS_STEREO) {
		balanceContainer->layout()->addWidget(labelL);
		balanceContainer->layout()->addWidget(balance);
		balanceContainer->layout()->addWidget(labelR);
	}
	QWidget::connect(balance, SIGNAL(valueChanged(int)), this, SLOT(balanceChanged(int)));
	QWidget::connect(balance, SIGNAL(doubleClicked()), this, SLOT(ResetBalance()));

	// mono checkbox control
	forceMonoContainer = new QWidget();

	hlayout = new QHBoxLayout();
	hlayout->setContentsMargins(0, 0, 0, 0);

	forceMonoContainer->setLayout(hlayout);

	forceMono = new QCheckBox();
	forceMono->setChecked((flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0);

	forceMonoContainer->layout()->addWidget(forceMono);
	forceMonoContainer->layout()->setAlignment(forceMono, Qt::AlignVCenter | Qt::AlignHCenter);
	forceMonoContainer->setFixedWidth(MONO_CONTAINER_WIDTH);
	QWidget::connect(forceMono, SIGNAL(clicked(bool)), this, SLOT(downmixMonoChanged(bool)));

	// sync offset spin box
	syncOffset = new PLSSpinBox();
	int64_t cur_sync = obs_source_get_sync_offset(source);
	syncOffset->setMinimum(-950);
	syncOffset->setMaximum(20000);
	syncOffset->setSuffix(" ms");
	syncOffset->setValue(int(cur_sync / NSEC_PER_MSEC));
	syncOffset->setMinimumSize(VOLUME_SPINBOX_WIDTH, NAME_LABEL_HEIGHT);
	syncOffset->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	QWidget::connect(syncOffset, SIGNAL(valueChanged(int)), this, SLOT(syncOffsetChanged(int)));

	// audio mixer tracks container
	mixerContainer = new QWidget();
	//mixerContainer->setStyleSheet("background-color:red;");

	hlayout = new QHBoxLayout();
	hlayout->setContentsMargins(0, 0, 0, 0);
	hlayout->setSpacing(0);
	hlayout->setAlignment(Qt::AlignLeft);
	mixerContainer->setLayout(hlayout);
	mixerContainer->setMinimumSize(TRACKS_MIN_CONTAINER_WIDTH, NAME_LABEL_HEIGHT);
	mixerContainer->setMaximumSize(TRACKS_MAX_CONTAINER_WIDTH, NAME_LABEL_HEIGHT);
	mixerContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

	QString checkBoxSheet = "font-size:14px; spacing:5px; min-width:43px; max-width:43px;";
	mixer1 = new QCheckBox();
	mixer1->setStyleSheet(checkBoxSheet);
	mixer2 = new QCheckBox();
	mixer2->setStyleSheet(checkBoxSheet);
	mixer3 = new QCheckBox();
	mixer3->setStyleSheet(checkBoxSheet);
	mixer4 = new QCheckBox();
	mixer4->setStyleSheet(checkBoxSheet);
	mixer5 = new QCheckBox();
	mixer5->setStyleSheet(checkBoxSheet);
	mixer6 = new QCheckBox();
	mixer6->setStyleSheet(checkBoxSheet);
	mixer1->setText("1");
	mixer1->setChecked(mixers & (1 << 0));
	mixer2->setText("2");
	mixer2->setChecked(mixers & (1 << 1));
	mixer3->setText("3");
	mixer3->setChecked(mixers & (1 << 2));
	mixer4->setText("4");
	mixer4->setChecked(mixers & (1 << 3));
	mixer5->setText("5");
	mixer5->setChecked(mixers & (1 << 4));
	mixer6->setText("6");
	mixer6->setChecked(mixers & (1 << 5));

	mixerContainer->layout()->addWidget(mixer1);
	mixerContainer->layout()->addWidget(mixer2);
	mixerContainer->layout()->addWidget(mixer3);
	mixerContainer->layout()->addWidget(mixer4);
	mixerContainer->layout()->addWidget(mixer5);
	mixerContainer->layout()->addWidget(mixer6);

	QWidget::connect(mixer1, SIGNAL(clicked(bool)), this, SLOT(mixer1Changed(bool)));
	QWidget::connect(mixer2, SIGNAL(clicked(bool)), this, SLOT(mixer2Changed(bool)));
	QWidget::connect(mixer3, SIGNAL(clicked(bool)), this, SLOT(mixer3Changed(bool)));
	QWidget::connect(mixer4, SIGNAL(clicked(bool)), this, SLOT(mixer4Changed(bool)));
	QWidget::connect(mixer5, SIGNAL(clicked(bool)), this, SLOT(mixer5Changed(bool)));
	QWidget::connect(mixer6, SIGNAL(clicked(bool)), this, SLOT(mixer6Changed(bool)));

	setObjectName(sourceName);
}

PLSAdvAudioCtrl::~PLSAdvAudioCtrl()
{

	pls_frontend_remove_event_callback(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY_BACK, PLSAdvAudioCtrl::monitorControlChange, this);

	nameLabel->deleteLater();
	volume->deleteLater();
	forceMonoContainer->deleteLater();
	balanceContainer->deleteLater();
	syncOffset->deleteLater();
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	monitoringType->deleteLater();
#endif
	mixerContainer->deleteLater();
}

void PLSAdvAudioCtrl::ShowAudioControl(QGridLayout *layout)
{
	int lastRow = layout->rowCount();
	m_row = lastRow;
	int idx = 0;

	// the first column is the source name
	layout->addWidget(nameLabel, lastRow, idx++);
	idx++;

	// the second column is the monitor
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	layout->addWidget(monitoringType, lastRow, idx++);
	idx++;
#endif

	// the third column is the volumn
	layout->addWidget(volume, lastRow, idx++);
	idx++;

	// the four column is balance
	layout->addWidget(balanceContainer, lastRow, idx++);
	idx++;

	// the fifth column is mono
	layout->addWidget(forceMonoContainer, lastRow, idx++);
	idx++;

	//the six column is sync offset
	layout->addWidget(syncOffset, lastRow, idx++);
	idx++;

	//the seven column is track
	layout->addWidget(mixerContainer, lastRow, idx++);

	layout->layout()->setAlignment(mixerContainer, Qt::AlignVCenter);
}

void PLSAdvAudioCtrl::setSourceName()
{
	QFontMetrics fontWidth(nameLabel->font());
	const char *sourceName = obs_source_get_name(source);
	QString elideNote = fontWidth.elidedText(QT_UTF8(sourceName), Qt::ElideRight, nameLabel->rect().width());
	nameLabel->setText(elideNote);
}

/* ------------------------------------------------------------------------- */
/* PLS source callbacks */

void PLSAdvAudioCtrl::OBSSourceFlagsChanged(void *param, calldata_t *calldata)
{
	uint32_t flags = (uint32_t)calldata_int(calldata, "flags");
	QMetaObject::invokeMethod(reinterpret_cast<PLSAdvAudioCtrl *>(param), "SourceFlagsChanged", Q_ARG(uint32_t, flags));
}

void PLSAdvAudioCtrl::OBSSourceVolumeChanged(void *param, calldata_t *calldata)
{
	float volume = (float)calldata_float(calldata, "volume");
	QMetaObject::invokeMethod(reinterpret_cast<PLSAdvAudioCtrl *>(param), "SourceVolumeChanged", Q_ARG(float, volume));
}

void PLSAdvAudioCtrl::OBSSourceSyncChanged(void *param, calldata_t *calldata)
{
	int64_t offset = calldata_int(calldata, "offset");
	QMetaObject::invokeMethod(reinterpret_cast<PLSAdvAudioCtrl *>(param), "SourceSyncChanged", Q_ARG(int64_t, offset));
}

void PLSAdvAudioCtrl::OBSSourceMixersChanged(void *param, calldata_t *calldata)
{
	uint32_t mixers = (uint32_t)calldata_int(calldata, "mixers");
	QMetaObject::invokeMethod(reinterpret_cast<PLSAdvAudioCtrl *>(param), "SourceMixersChanged", Q_ARG(uint32_t, mixers));
}

void PLSAdvAudioCtrl::monitorControlChange(pls_frontend_event event, const QVariantList &params, void *context)
{
	if (pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY_BACK == event) {
		PLSAdvAudioCtrl *control = (PLSAdvAudioCtrl *)context;
		for (int index = 0; index != control->monitoringType->count(); ++index) {
			if (control->monitoringType->itemData(index) == obs_source_get_monitoring_type(control->source)) {
				control->monitoringType->setCurrentIndex(index);
				break;
			}
		}
	}
}

/* ------------------------------------------------------------------------- */
/* Qt event queue source callbacks */

static inline void setCheckboxState(QCheckBox *checkbox, bool checked)
{
	checkbox->blockSignals(true);
	checkbox->setChecked(checked);
	checkbox->blockSignals(false);
}

void PLSAdvAudioCtrl::SourceFlagsChanged(uint32_t flags)
{
	bool forceMonoVal = (flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0;
	setCheckboxState(forceMono, forceMonoVal);
}

void PLSAdvAudioCtrl::SourceVolumeChanged(float value)
{
	volume->blockSignals(true);
	volume->setValue(obs_mul_to_db(value));
	volume->blockSignals(false);
}

void PLSAdvAudioCtrl::SourceSyncChanged(int64_t offset)
{
	syncOffset->setValue(offset / NSEC_PER_MSEC);
}

void PLSAdvAudioCtrl::SourceMixersChanged(uint32_t mixers)
{
	setCheckboxState(mixer1, mixers & (1 << 0));
	setCheckboxState(mixer2, mixers & (1 << 1));
	setCheckboxState(mixer3, mixers & (1 << 2));
	setCheckboxState(mixer4, mixers & (1 << 3));
	setCheckboxState(mixer5, mixers & (1 << 4));
	setCheckboxState(mixer6, mixers & (1 << 5));
}

/* ------------------------------------------------------------------------- */
/* Qt control callbacks */

void PLSAdvAudioCtrl::volumeChanged(double db)
{
	QString strText = volume->text();
	if (db < MIN_DB) {
		volume->setSpecialValueText("-inf dB");
		db = -INFINITY;
	}
	float val = obs_db_to_mul(db);
	obs_source_set_volume(source, val);
	qDebug() << "volumeChanged: " << db << "suffix: " << volume->text();
}

void PLSAdvAudioCtrl::downmixMonoChanged(bool checked)
{
	uint32_t flags = obs_source_get_flags(source);
	bool forceMonoActive = (flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0;

	if (forceMonoActive != checked) {
		if (checked)
			flags |= OBS_SOURCE_FLAG_FORCE_MONO;
		else
			flags &= ~OBS_SOURCE_FLAG_FORCE_MONO;

		obs_source_set_flags(source, flags);
	}
	PLS_UI_STEP(AUDIO_MIXER_ADV_MODULE, "Mono checkBox", ACTION_CLICK);
}

void PLSAdvAudioCtrl::balanceChanged(int val)
{
	float bal = (float)val / 100.0f;

	if (abs(50 - val) < 10) {
		balance->blockSignals(true);
		balance->setValue(50);
		bal = 0.5f;
		balance->blockSignals(false);
	}

	obs_source_set_balance_value(source, bal);
}

void PLSAdvAudioCtrl::ResetBalance()
{
	balance->setValue(50);
}

void PLSAdvAudioCtrl::syncOffsetChanged(int milliseconds)
{
	int64_t cur_val = obs_source_get_sync_offset(source);

	if (cur_val / NSEC_PER_MSEC != milliseconds)
		obs_source_set_sync_offset(source, int64_t(milliseconds) * NSEC_PER_MSEC);
}

void PLSAdvAudioCtrl::monitoringTypeChanged(int index)
{
	int mt = monitoringType->itemData(index).toInt();
	obs_source_set_monitoring_type(source, (obs_monitoring_type)mt);

	const char *type = nullptr;

	switch (mt) {
	case OBS_MONITORING_TYPE_NONE:
		type = "none";
		break;
	case OBS_MONITORING_TYPE_MONITOR_ONLY:
		type = "monitor only";
		break;
	case OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT:
		type = "monitor and output";
		break;
	}

	PLS_UI_STEP(AUDIO_MIXER_ADV_MODULE, "Audio Monitoring ComboBox", ACTION_CLICK);
	PLSBasic ::Get()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY);
}

static inline void setMixer(obs_source_t *source, const int mixerIdx, const bool checked)
{
	uint32_t mixers = obs_source_get_audio_mixers(source);
	uint32_t new_mixers = mixers;

	if (checked)
		new_mixers |= (1 << mixerIdx);
	else
		new_mixers &= ~(1 << mixerIdx);

	obs_source_set_audio_mixers(source, new_mixers);
}

void PLSAdvAudioCtrl::mixer1Changed(bool checked)
{
	setMixer(source, 0, checked);
	PLS_UI_STEP(AUDIO_MIXER_ADV_MODULE, "Track1 CheckBox", ACTION_CLICK);
}

void PLSAdvAudioCtrl::mixer2Changed(bool checked)
{
	setMixer(source, 1, checked);
	PLS_UI_STEP(AUDIO_MIXER_ADV_MODULE, "Track2 CheckBox", ACTION_CLICK);
}

void PLSAdvAudioCtrl::mixer3Changed(bool checked)
{
	setMixer(source, 2, checked);
	PLS_UI_STEP(AUDIO_MIXER_ADV_MODULE, "Track3 CheckBox", ACTION_CLICK);
}

void PLSAdvAudioCtrl::mixer4Changed(bool checked)
{
	setMixer(source, 3, checked);
	PLS_UI_STEP(AUDIO_MIXER_ADV_MODULE, "Track4 CheckBox", ACTION_CLICK);
}

void PLSAdvAudioCtrl::mixer5Changed(bool checked)
{
	setMixer(source, 4, checked);
	PLS_UI_STEP(AUDIO_MIXER_ADV_MODULE, "Track5 CheckBox", ACTION_CLICK);
}

void PLSAdvAudioCtrl::mixer6Changed(bool checked)
{
	setMixer(source, 5, checked);
	PLS_UI_STEP(AUDIO_MIXER_ADV_MODULE, "Track6 CheckBox", ACTION_CLICK);
}
