#include "PLSPreviewTitle.h"
#include "window-basic-main.hpp"
#include "PLSChannelDataAPI.h"
#include "PLSBasic.h"
#include "liblog.h"

#include <QHBoxLayout>

const auto TRANSITION_APPLY_BUTTON_SIZE = 34;

const auto TIMER_REFRESH_INTERVAL = 500; // in milliseconds

const auto PREVIEW_TITLE_LABEL = "previewTitleLabel";
const auto PREVIEW_TITLE_LABEL_EDIT = "previewTitleLabelEdit";
const auto PREVIEW_TITLE_LABEL_DRAWPEN = "previewTitleLabelDrawPen";
const auto PREVIEW_LIVEREC_TIME = "previewLiveRecTime";
const auto PREVIEW_LIVEREC_SPACEICON = "previewLiveRecSpace";

const auto PREVIEW_TITLE_STATUS = "status";
const auto PREVIEW_TITLE_STATUS_START = "true";
const auto PREVIEW_TITLE_STATUS_END = "false";
const auto PREVIEW_TIMER_ACTIVATE = "activate";

const auto STUDIO_MODE = "studio_mode";
const auto RECORD_STATUS = "record";
const auto LIVE_STATUS = "live";

constexpr auto PREVIEW_TITLE_TIMERTYPE = "timerType";

using namespace common;

PLSTimerDisplay::PLSTimerDisplay(TimerType type, bool showTime_, QWidget *parent) : QWidget(parent), showTime(showTime_)
{
	title = pls_new<QLabel>(this);
	title->setObjectName(PREVIEW_TITLE_LABEL);
	SetTimerType(type);

	auto layout = pls_new<QHBoxLayout>(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(5);
	layout->addWidget(title);

	if (showTime) {
		time = pls_new<QLabel>(this);
		time->setText("00:00:00");
		time->setObjectName(PREVIEW_LIVEREC_TIME);
		layout->addWidget(time);
	}

	OnStatus(false);
}

void PLSTimerDisplay::SetTimerType(TimerType type)
{
	timerType = type;
	switch (type) {
	case PLSTimerDisplay::TimerType::TimerRecord:
		title->setText("REC");
		title->setProperty(PREVIEW_TITLE_TIMERTYPE, "TimerRecord");
		break;
	case PLSTimerDisplay::TimerType::TimerRehearsal:
		title->setText("Rehearsal");
		title->setProperty(PREVIEW_TITLE_TIMERTYPE, "TimerRehearsal");
		break;
	case PLSTimerDisplay::TimerType::TimerLive:
		title->setText("LIVE");
		title->setProperty(PREVIEW_TITLE_TIMERTYPE, "TimerLive");
		break;
	default:
		assert(false);
		title->setText("");
		title->setProperty(PREVIEW_TITLE_TIMERTYPE, "");
		break;
	}
}

void PLSTimerDisplay::OnStatus(bool isStarted)
{
	UpdateProperty(isStarted);

	started = isStarted;

	if (!showTime || !time) {
		return;
	}
	OnTimerActivate(false);

	time->setText("00:00:00");
	startTime = (uint)QDateTime::currentDateTime().toSecsSinceEpoch();

	if (isStarted) {
		if (timerType == PLSTimerDisplay::TimerType::TimerLive || timerType == PLSTimerDisplay::TimerType::TimerRehearsal) {
			PLSBasic::instance()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_STREAMING_TIME_READY);
		}
		timerID = this->startTimer(TIMER_REFRESH_INTERVAL);
		OnTimerActivate(true);
	} else {
		if (INTERNAL_ERROR != timerID) {
			killTimer(timerID);
			timerID = INTERNAL_ERROR;
		}
		totalRecordSeconds = 0;
		paused = false;
	}
}

void PLSTimerDisplay::OnEvents(EventType eventType, bool on)
{
	switch (eventType) {
	case PLSTimerDisplay::EventType::LiveStatusChanged:
		title->setProperty(LIVE_STATUS, on ? STATUS_ON : STATUS_OFF);
		break;
	case PLSTimerDisplay::EventType::RecordStatusChanged:
		title->setProperty(RECORD_STATUS, on ? STATUS_ON : STATUS_OFF);
		break;
	case PLSTimerDisplay::EventType::StudioModeSatusChanged:
		title->setProperty(STUDIO_MODE, on ? STATUS_ON : STATUS_OFF);
		break;
	default:
		title->setProperty(LIVE_STATUS, STATUS_OFF);
		title->setProperty(RECORD_STATUS, STATUS_OFF);
		title->setProperty(STUDIO_MODE, STATUS_OFF);
		break;
	}

	pls_flush_style(title);
}

void PLSTimerDisplay::timerEvent(QTimerEvent *e)
{
	if (!showTime || !time) {
		return;
	}
	if (e->timerId() == timerID) {
		totalRecordSeconds += (float)TIMER_REFRESH_INTERVAL / 1000;
		QString text = FormatTimeString((int)totalRecordSeconds);
		time->setText(text);
	}
}

QString PLSTimerDisplay::FormatTimeString(uint sec) const
{
	int hours = sec / 3600;
	int minutes = (sec % 3600) / 60;
	int seconds = (sec % 60);

	QString strH;
	QString strM;
	QString strS;

	strM = QString::asprintf("%02d", minutes);
	strS = QString::asprintf("%02d", seconds);
	if (hours > 99) {
		strH = QString::asprintf("%d", hours);
	} else {
		strH = QString::asprintf("%02d", hours);
	}

	QString ret = strH + QString(":") + strM + QString(":") + strS;
	return ret;
}

void PLSTimerDisplay::UpdateProperty(bool isStarted)
{
	title->setProperty(PREVIEW_TITLE_STATUS, isStarted ? PREVIEW_TITLE_STATUS_START : PREVIEW_TITLE_STATUS_END);
	title->style()->unpolish(title);
	title->style()->polish(title);
}

void PLSTimerDisplay::OnTimerActivate(bool activate)
{
	if (!time) {
		return;
	}
	time->setProperty(PREVIEW_TIMER_ACTIVATE, activate ? STATUS_TRUE : STATUS_FALSE);
	pls_flush_style(time);
}

void PLSTimerDisplay::Pause(bool pause)
{
	if (pause) {
		if (INTERNAL_ERROR != timerID) {
			killTimer(timerID);
			timerID = INTERNAL_ERROR;
			QString text = FormatTimeString((int)totalRecordSeconds);
			text.append(" (Paused)");
			time->setText(text);
			paused = true;
		}
	} else {
		timerID = this->startTimer(TIMER_REFRESH_INTERVAL);
		paused = false;
	}
}

PLSPreviewProgramTitle::PLSPreviewProgramTitle(bool studioPortraitLayout_, QWidget *parent_) : QFrame(parent_), studioPortraitLayout(studioPortraitLayout_)
{
	parent = parent_;
	CreateUI();
	setStudioPortraitLayoutUI();
	OnLiveStatus(false);
	OnRecordStatus(false);
}

void PLSPreviewProgramTitle::SetStudioPortraitLayout(bool studioPortraitLayout_)
{
	studioPortraitLayout = studioPortraitLayout_;
	setStudioPortraitLayoutUI();
}

void PLSPreviewProgramTitle::setEditTitle(const QString &sceneName)
{
	int maxWidth = (parent->width() - horApplyBtn->width()) / 2;

	horEditArea->SetSceneName(sceneName);
	porEditArea->SetSceneName(sceneName);
	horEditArea->SetDisplayText(maxWidth);
	porEditArea->SetDisplayText(parent->width());
}

void PLSPreviewProgramTitle::setLiveTitle(const QString &sceneName)
{
	int maxWidth = (parent->width() - horApplyBtn->width()) / 2;

	horLiveArea->SetSceneName(sceneName);
	porLiveArea->SetSceneName(sceneName);

	horLiveArea->SetDisplayText(maxWidth);
	porLiveArea->SetDisplayText(maxWidth);
}

void PLSPreviewProgramTitle::setApplyEnabled(bool enable)
{
	porApplyBtn->setEnabled(enable);
	horApplyBtn->setEnabled(enable);
}

void PLSPreviewProgramTitle::toggleShowHide(bool visible)
{
	this->setVisible(visible);
	porEditArea->setVisible(visible);
	porLiveArea->setVisible(visible);
	porApplyBtn->setVisible(visible);
	horizontalArea->setVisible(visible);
}

void PLSPreviewProgramTitle::OnPreviewSceneChanged(const QString &sceneName)
{
	this->setLiveTitle(sceneName);
}

QFrame *PLSPreviewProgramTitle::GetEditArea()
{
	return porEditArea;
}

QFrame *PLSPreviewProgramTitle::GetLiveArea()
{
	return porLiveArea;
}

QFrame *PLSPreviewProgramTitle::GetTotalArea()
{
	return horizontalArea;
}

void PLSPreviewProgramTitle::OnLiveStatus(bool isStarted)
{
	pls_async_call_mt([this, isStarted]() {
		horLiveArea->SetLiveStatus(isStarted);
		porLiveArea->SetLiveStatus(isStarted);
	});
}

void PLSPreviewProgramTitle::OnRecordStatus(bool isStarted)
{
	pls_async_call_mt([this, isStarted]() {
		horLiveArea->SetRecordStatus(isStarted);
		porLiveArea->SetRecordStatus(isStarted);
	});
}

void PLSPreviewProgramTitle::OnStudioModeStatus(bool isStudioMode)
{
	horEditArea->SetStudioModeStatus(isStudioMode);
	porEditArea->SetStudioModeStatus(isStudioMode);

	horLiveArea->SetStudioModeStatus(isStudioMode);
	porLiveArea->SetStudioModeStatus(isStudioMode);
}

void PLSPreviewProgramTitle::OnDrawPenModeStatus(bool isDrawPen, bool isStudioMode)
{
	horEditArea->SetDrawPenModeStatus(isDrawPen, isStudioMode);
	porEditArea->SetDrawPenModeStatus(isDrawPen, isStudioMode);
	CustomResize();
}

uint PLSPreviewProgramTitle::getStartTime() const
{
	if (studioPortraitLayout) {
		return porLiveArea->GetStartTime();
	}
	return horLiveArea->GetStartTime();
}

void PLSPreviewProgramTitle::CustomResize()
{
	int maxWidth = (parent->width() - horApplyBtn->width()) / 2;

	if (studioPortraitLayout) {
		QTimer::singleShot(100, this, [this]() {
			PLS_INFO("PLSPreviewProgramTitle", "single shot timer triggered for move preview UI.");
			porApplyBtn->move((porLiveArea->width() - porApplyBtn->width()) / 2, 3);
		});
		porEditArea->setFixedWidth(parent->width());
		porEditArea->SetDisplayText(parent->width());
		porLiveArea->SetDisplayText(maxWidth);
	} else {
		horEditArea->setFixedWidth(maxWidth);
		horLiveArea->setFixedWidth(maxWidth);
		horEditArea->SetDisplayText(maxWidth);
		horLiveArea->SetDisplayText(maxWidth);
	}
}

void PLSPreviewProgramTitle::CustomResizeAsync()
{
	pls_async_call(this, [this]() { CustomResize(); });
}

void PLSPreviewProgramTitle::ShowSceneNameLabel(bool show)
{
	horEditArea->SetSceneNameVisible(show);
	porEditArea->SetSceneNameVisible(show);

	horLiveArea->SetSceneNameVisible(show);
	porLiveArea->SetSceneNameVisible(show);
}

void PLSPreviewProgramTitle::setStudioPortraitLayoutUI()
{
	if (studioPortraitLayout) {
		horizontalArea->setVisible(false);
		this->setVisible(false);
		porEditArea->setVisible(true);
		porLiveArea->setVisible(true);
		porApplyBtn->setVisible(true);
		porEditArea->setFixedHeight(35);
		porLiveArea->setFixedHeight(40);
		CustomResizeAsync();
	} else {
		porEditArea->setVisible(false);
		porLiveArea->setVisible(false);
		porApplyBtn->setVisible(false);
		this->setVisible(true);
		horizontalArea->setVisible(true);
		horizontalArea->setFixedHeight(40);
	}
}

QPushButton *PLSPreviewProgramTitle::CreateApplyBtn(const QString &objName, QWidget *parent)
{
	QPushButton *applyBtn = pls_new<QPushButton>(parent);
	applyBtn->setObjectName(objName);
	applyBtn->setToolTip(tr("StudioMode.Apply.Tooltips"));
	return applyBtn;
}

void PLSPreviewProgramTitle::CreateUI()
{
	// horizontal layout
	horizontalArea = pls_new<QFrame>(this);
	QHBoxLayout *hLayout = pls_new<QHBoxLayout>(horizontalArea);
	hLayout->setAlignment(Qt::AlignCenter | Qt::AlignCenter);
	hLayout->setSpacing(10);
	hLayout->setContentsMargins(0, 0, 0, 0);
	horEditArea = pls_new<PLSPreviewEditLabel>(true, this);
	horLiveArea = pls_new<PLSPreviewLiveLabel>(true, false, this);
	connect(horLiveArea, &PLSPreviewLiveLabel::resizeSignal, this, [this]() { CustomResize(); });

	horApplyBtn = CreateApplyBtn("horApplyBtn", horizontalArea);
	hLayout->addWidget(horEditArea);
	hLayout->addWidget(horApplyBtn);
	hLayout->addWidget(horLiveArea);

	// portrait layout
	porEditArea = pls_new<PLSPreviewEditLabel>(false, this);
	porLiveArea = pls_new<PLSPreviewLiveLabel>(false, false, this);
	connect(porLiveArea, &PLSPreviewLiveLabel::resizeSignal, this, [this]() { CustomResize(); });
	porApplyBtn = CreateApplyBtn("porApplyBtn", porLiveArea);
	porApplyBtn->move((porLiveArea->width() - porApplyBtn->width()) / 2, 3);
}

PLSPreviewEditLabel::PLSPreviewEditLabel(bool center, QWidget *parent) : QFrame(parent)
{
	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	editLabel = pls_new<QLabel>(this);
	editLabel->setObjectName("editLabel");
	editLabel->setText("EDIT");

	drawPenLabel = pls_new<QLabel>(this);
	drawPenLabel->setObjectName(PREVIEW_TITLE_LABEL_DRAWPEN);
	drawPenLabel->hide();

	sceneNameLabel = pls_new<QLabel>(this);
	sceneNameLabel->setObjectName("editSceneLabel");
	QHBoxLayout *editLayout = pls_new<QHBoxLayout>(this);
	editLayout->setSpacing(7);
	editLayout->setContentsMargins(0, 0, 0, 0);
	if (center) {
		editLayout->setAlignment(Qt::AlignCenter | Qt::AlignCenter);
		editLayout->addWidget(drawPenLabel);
		editLayout->addWidget(editLabel);
		editLayout->addWidget(sceneNameLabel);
	} else {
		editLayout->setAlignment(Qt::AlignLeft | Qt::AlignCenter);
		editLayout->addWidget(drawPenLabel);
		editLayout->addWidget(editLabel);
		editLayout->addWidget(sceneNameLabel);
	}
}

void PLSPreviewEditLabel::SetSceneText(const QString &sceneName)
{
	sceneNameLabel->setText(sceneName);
}

void PLSPreviewEditLabel::SetSceneName(const QString &sceneName_)
{
	sceneName = sceneName_;
}

void PLSPreviewEditLabel::SetDisplayText(int maxWidth)
{
	if (!sceneNameLabel) {
		return;
	}

	int margin = editLabel->width() + 20;
	QFontMetrics fontWidth(sceneNameLabel->font());
	if (drawPenLabel->isVisible()) {
		margin += drawPenLabel->width() + 8;
	}
	if (fontWidth.horizontalAdvance(sceneName) > maxWidth - margin) {
		QString name = fontWidth.elidedText(sceneName, Qt::ElideRight, maxWidth - margin);
		sceneNameLabel->setText(name);
		sceneNameLabel->setToolTip(sceneName);
		return;
	}

	sceneNameLabel->setText(sceneName);
	sceneNameLabel->setToolTip("");
}

void PLSPreviewEditLabel::SetSceneNameVisible(bool visible)
{
	sceneNameLabel->setVisible(visible);
}

void PLSPreviewEditLabel::SetStudioModeStatus(bool isStudioMode)
{
	drawPenLabel->setProperty(STUDIO_MODE, isStudioMode ? STATUS_ON : STATUS_OFF);
}

void PLSPreviewEditLabel::SetDrawPenModeStatus(bool isDrawPen, bool isStudioMode)
{
	if (isDrawPen) {
		drawPenLabel->setProperty(STUDIO_MODE, isStudioMode ? STATUS_ON : STATUS_OFF);
		pls_flush_style(drawPenLabel);
		drawPenLabel->show();
	} else {
		drawPenLabel->setProperty(STUDIO_MODE, isStudioMode ? STATUS_ON : STATUS_OFF);
		pls_flush_style(drawPenLabel);
		drawPenLabel->hide();
	}
}

PLSPreviewLiveLabel::PLSPreviewLiveLabel(bool center, bool showTime, QWidget *parent) : QFrame(parent)
{
	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	liveUI = pls_new<PLSTimerDisplay>(PLSTimerDisplay::TimerType::TimerLive, showTime, this);
	recordUI = pls_new<PLSTimerDisplay>(PLSTimerDisplay::TimerType::TimerRecord, showTime, this);

	spaceIcon = pls_new<QLabel>(this);
	spaceIcon->setObjectName(PREVIEW_LIVEREC_SPACEICON);

	sceneNameLabel = pls_new<QLabel>(this);
	sceneNameLabel->setObjectName("liveSceneLabel");

	QHBoxLayout *liveLayout = pls_new<QHBoxLayout>(this);
	liveLayout->setSpacing(10);
	liveLayout->setContentsMargins(0, 0, 0, 0);
	if (center) {
		liveLayout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	} else {
		liveLayout->setAlignment(Qt::AlignLeft | Qt::AlignCenter);
	}
	liveLayout->addWidget(liveUI);
	liveLayout->addWidget(spaceIcon);
	liveLayout->addWidget(recordUI);
	liveLayout->addWidget(sceneNameLabel);
}

void PLSPreviewLiveLabel::SetSceneText(const QString &sceneName)
{
	sceneNameLabel->setText(sceneName);
}

void PLSPreviewLiveLabel::SetSceneName(const QString &sceneName_)
{
	sceneName = sceneName_;
}

void PLSPreviewLiveLabel::SetDisplayText(int maxWidth)
{
	if (!sceneNameLabel) {
		return;
	}

	int margin = liveUI->width() + 20;
	QFontMetrics fontWidth(sceneNameLabel->font());
	if (recordUI->isVisible()) {
		margin += recordUI->width() + 20;
	}
	if (fontWidth.horizontalAdvance(sceneName) > maxWidth - margin) {
		QString name = fontWidth.elidedText(sceneName, Qt::ElideRight, maxWidth - margin);
		sceneNameLabel->setText(name);
		sceneNameLabel->setToolTip(sceneName);
		return;
	}

	sceneNameLabel->setText(sceneName);
	sceneNameLabel->setToolTip("");
}

void PLSPreviewLiveLabel::SetSceneNameVisible(bool visible)
{
	sceneNameLabel->setVisible(visible);
}

void PLSPreviewLiveLabel::SetLiveStatus(bool isStarted)
{
	liveUI->SetTimerType(isStarted && PLSCHANNELS_API->isRehearsaling() ? PLSTimerDisplay::TimerType::TimerRehearsal : PLSTimerDisplay::TimerType::TimerLive);
	liveUI->OnStatus(isStarted);
	liveUI->OnEvents(PLSTimerDisplay::EventType::LiveStatusChanged, isStarted);
	recordUI->OnEvents(PLSTimerDisplay::EventType::LiveStatusChanged, isStarted);
	if (isStarted) {
		liveUI->show();
		spaceIcon->setVisible(PLSCHANNELS_API->isRecording());
	} else {
		if (PLSCHANNELS_API->isRecording()) {
			liveUI->hide();
			spaceIcon->hide();
		}
	}
}

void PLSPreviewLiveLabel::SetRecordStatus(bool isStarted)
{
	recordUI->OnStatus(isStarted);

	liveUI->OnEvents(PLSTimerDisplay::EventType::RecordStatusChanged, isStarted);
	recordUI->OnEvents(PLSTimerDisplay::EventType::RecordStatusChanged, isStarted);

	bool isLiving = pls_get_output_stream_dealy_active() || PLSCHANNELS_API->isLiving();
	if (isStarted) {
		recordUI->show();
		spaceIcon->setVisible(isLiving);
		if (!isLiving) {
			liveUI->hide();
		}
	} else {
		spaceIcon->hide();
		recordUI->hide();
		liveUI->show();
	}
}

void PLSPreviewLiveLabel::SetStudioModeStatus(bool isStudioMode)
{
	liveUI->OnEvents(PLSTimerDisplay::EventType::StudioModeSatusChanged, isStudioMode);
	recordUI->OnEvents(PLSTimerDisplay::EventType::StudioModeSatusChanged, isStudioMode);
}

uint PLSPreviewLiveLabel::GetStartTime() const
{
	return liveUI->getStartTime();
}

int PLSPreviewLiveLabel::GetRecordDuration() const
{
	return recordUI->getDuration();
}

void PLSPreviewLiveLabel::OnRecordPaused(bool paused)
{
	return recordUI->Pause(paused);
}

void PLSPreviewLiveLabel::resizeEvent(QResizeEvent *event)
{
	emit resizeSignal();
	QFrame::resizeEvent(event);
}
