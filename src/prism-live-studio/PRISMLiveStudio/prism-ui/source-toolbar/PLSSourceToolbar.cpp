#include "PLSSourceToolbar.hpp"
#include "PLSRegionCapture.h"
#include <QPushButton>
#include <QHBoxLayout>
#include <libutils-api.h>
#include <pls/pls-obs-api.h>
#include <libui.h>
#include "pls/pls-source.h"
#include <QPushButton>

RegionCaptureToolbar::RegionCaptureToolbar(QWidget *parent, OBSSource source) : SourceToolbar(parent, source)
{
#ifdef Q_OS_WIN
	auto mainlayout = pls_new<QHBoxLayout>();
	mainlayout->setContentsMargins(0, 0, 0, 0);
	mainlayout->setSpacing(0);

	auto *btn = pls_new<QPushButton>(this);
	btn->setObjectName("regionCaptureToolbarBtn");
	btn->setText(tr("Basic.PropertiesView.Capture.selectRegion"));
	connect(btn, &QPushButton::clicked, this, &RegionCaptureToolbar::OnSelectRegionClicked);
	mainlayout->addWidget(btn);
	mainlayout->addStretch();
	setLayout(mainlayout);
#endif //Q_OS_WIN

	pls_add_css(this, {"PLSSourceToolbar"});
}

void RegionCaptureToolbar::OnSelectRegionClicked()
{
	uint64_t max_size = pls_texture_get_max_size();
	PLSRegionCapture *regionCapture = pls_new<PLSRegionCapture>(this);
	connect(regionCapture, &PLSRegionCapture::selectedRegion, this, [this, regionCapture](const QRect &selectedRect) {
		OBSDataAutoRelease settings = obs_source_get_settings(GetSource());
		qInfo() << "user selected a new region=" << selectedRect;
		if (!selectedRect.isValid()) {
			regionCapture->deleteLater();
			return;
		}

		obs_data_t *region_obj = obs_data_create();
		obs_data_set_int(region_obj, "left", selectedRect.left());
		obs_data_set_int(region_obj, "top", selectedRect.top());
		obs_data_set_int(region_obj, "width", selectedRect.width());
		obs_data_set_int(region_obj, "height", selectedRect.height());
		obs_data_set_obj(settings, "region_select", region_obj);
		obs_data_release(region_obj);

		obs_source_update(GetSource(), settings);
		regionCapture->deleteLater();
	});
	regionCapture->StartCapture(max_size, max_size);
}

TimerSourceToolbar::TimerSourceToolbar(QWidget *parent, OBSSource source) : SourceToolbar(parent, source), m_source(source)
{
	auto mainlayout = pls_new<QHBoxLayout>();
	mainlayout->setContentsMargins(0, 0, 0, 0);
	mainlayout->setSpacing(5);
	m_startBtn = pls_new<QPushButton>(this);
	m_startBtn->setObjectName("toolBarBtn");

	m_startBtn->setText(tr("timer.btn.start"));
	m_startBtn->setToolTip(tr("timer.btn.start"));
	connect(m_startBtn, &QPushButton::clicked, this, &TimerSourceToolbar::OnStartClicked);
	mainlayout->addWidget(m_startBtn);

	m_stopBtn = pls_new<QPushButton>(this);
	m_stopBtn->setObjectName("toolBarBtn");
	m_stopBtn->setText(tr("timer.btn.cancel"));
	m_stopBtn->setToolTip(tr("timer.btn.cancel"));
	connect(m_stopBtn, &QPushButton::clicked, this, &TimerSourceToolbar::OnStopClicked);
	mainlayout->addWidget(m_stopBtn);

	mainlayout->addStretch();
	setLayout(mainlayout);
	pls_set_css(this, {"PLSSourceToolbar"});
	updateBtnState();

	signal_handler_connect_ref(obs_get_signal_handler(), "source_notify", TimerSourceToolbar::updatePropertiesButtonState, this);
}

TimerSourceToolbar::~TimerSourceToolbar()
{
	signal_handler_disconnect(obs_get_signal_handler(), "source_notify", TimerSourceToolbar::updatePropertiesButtonState, this);
}

void TimerSourceToolbar::updateBtnState()
{
	OBSData settings = obs_data_create();
	obs_data_set_string(settings, "method", "get_timer_type");
	obs_data_set_string(settings, "method", "get_timer_state");
	pls_source_get_private_data(m_source, settings);
	const char *startText = obs_data_get_string(settings, "start_text");
	const char *cancelText = obs_data_get_string(settings, "cancel_text");
	bool startSate = obs_data_get_bool(settings, "start_state");
	bool cancelSate = obs_data_get_bool(settings, "cancel_state");
	bool startHighlight = obs_data_get_bool(settings, "start_highlight");
	bool cancelHighlight = obs_data_get_bool(settings, "cancel_highlight");
	obs_data_release(settings);

	m_startBtn->setText(startText);
	m_startBtn->setToolTip(startText);
	m_startBtn->setEnabled(startSate);
	m_startBtn->setCheckable(true);
	m_startBtn->setChecked(startHighlight);
	m_stopBtn->setEnabled(cancelSate);
	m_stopBtn->setText(cancelText);
	m_stopBtn->setToolTip(cancelText);
	m_stopBtn->setCheckable(true);
	m_stopBtn->setChecked(cancelHighlight);
}

bool TimerSourceToolbar::isSameSource(const obs_source_t *rSource) const
{
	return rSource == m_source;
}

void TimerSourceToolbar::updatePropertiesButtonState(void *data, calldata_t *params)
{
	pls_used(params);
	auto type = (int)calldata_int(params, "message");
	if (type != OBS_SOURCE_TIMER_BUTTON_STATE_CHANGED) {
		return;
	}

	auto *timerBar = static_cast<TimerSourceToolbar *>(data);
	if (!timerBar) {
		return;
	}
	auto source = (obs_source_t *)calldata_ptr(params, "source");
	if (!source || !timerBar->isSameSource(source)) {
		return;
	}

	QMetaObject::invokeMethod(timerBar, &TimerSourceToolbar::updateBtnState, Qt::QueuedConnection);
}

void TimerSourceToolbar::OnStartClicked()
{

	obs_data_t *settings_ = obs_data_create();
	obs_data_set_string(settings_, "method", "start_clicked");
	pls_source_set_private_data(m_source, settings_);
	obs_data_release(settings_);
	updateBtnState();
}

void TimerSourceToolbar::OnStopClicked()
{
	obs_data_t *settings_ = obs_data_create();
	obs_data_set_string(settings_, "method", "cancel_clicked");
	pls_source_set_private_data(m_source, settings_);
	obs_data_release(settings_);
	updateBtnState();
}
