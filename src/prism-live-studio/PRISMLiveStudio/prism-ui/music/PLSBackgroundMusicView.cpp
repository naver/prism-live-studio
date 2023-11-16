#include "PLSBackgroundMusicView.h"
#include "PLSBgmItemCoverView.h"
#include "PLSBgmItemView.h"
#include "PLSBgmLibraryView.h"
#include "ui_PLSBackgroundMusicView.h"

#include "action.h"
#include "frontend-api.h"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSMainView.hpp"
#include "obs-app.hpp"
#include "pls-common-define.hpp"
#include "PLSAction.h"
#include "PLSPlatformApi.h"
#include "pls/pls-source.h"

#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QGraphicsBlurEffect>
#include <QMenu>
#include <QMimeData>
#include <QRadioButton>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <ctime>
#include <sstream>

using namespace common;
using namespace action;

static constexpr auto GEOMETRY_BGM_DATA = "geometryBgm"; //key of the bgm window geometry in global ini
static constexpr auto MAXIMIZED_STATE = "isMaxState";    //key of the bgm window is maximized in global ini
static constexpr auto SHOW_MADE = "showMode";            //key of the bgm window is shown in global ini
static constexpr auto DEFAULT_COVER_IMAGE = ":/resource/images/bgm/bgm-default.png";

static const int COVER_MAX_NUMBER = 15;
static const int FLOW_LAYOUT_MARGIN_LEFT_RIGHT = 19;
static const int FLOW_LAYOUT_MARGIN_TOP_BOTTOM = 19;
static const int FLOW_LAYOUT_H_SPACING = 19;
static const int FLOW_LAYOUT_V_SPACING = 22;
static const int LoadingTimeoutMS = 10000;
const static int COVER_WIDTH = 130;

void SendThread::DoWork(const PLSBgmItemData &data) const
{
	SendMusicMetaData(data);
}

PLSBackgroundMusicView::PLSBackgroundMusicView(DialogInfo info, QWidget *parent) : PLSSideBarDialogView(info, parent)
{
	ui = pls_new<Ui::PLSBackgroundMusicView>();
	pls_add_css(this, {"PLSBackgroundMusicView", "PLSToastMsgFrame"});
	qRegisterMetaType<obs_media_state>("obs_media_state");

	initUI();

	sliderTimer = pls_new<QTimer>(this);
	connect(sliderTimer, &QTimer::timeout, this, &PLSBackgroundMusicView::SetSliderPos, Qt::QueuedConnection);
	connect(&seekTimer, &QTimer::timeout, this, &PLSBackgroundMusicView::SeekTimerCallback);
	connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, &PLSBackgroundMusicView::OnCurrentPageChanged);
	connect(ui->playBtn, &QPushButton::clicked, this, &PLSBackgroundMusicView::OnPlayButtonClicked);
	connect(ui->preBtn, &QPushButton::clicked, this, &PLSBackgroundMusicView::OnPreButtonClicked);
	connect(ui->nextBtn, &QPushButton::clicked, this, &PLSBackgroundMusicView::OnNextButtonClicked);
	connect(ui->refreshBtn, &QPushButton::clicked, this, &PLSBackgroundMusicView::OnRefreshButtonClicked);

	connect(ui->loopBtn, &QRadioButton::clicked, this, [this](bool checked) { OnLoopBtnClicked(currentSceneItem, checked); });
	connect(ui->addSourceBtn, &QPushButton::clicked, this, &PLSBackgroundMusicView::OnAddSourceBtnClicked);
	connect(ui->addMusicBtn, &QPushButton::clicked, this, &PLSBackgroundMusicView::OnAddMusicBtnClicked);
	connect(ui->coverLabel, &PLSBgmItemCoverView::CoverPressed, this, [this](const QPoint &point) { move(this->frameGeometry().topLeft() + point); });
	connect(ui->noSongLabel, &DragLabel::CoverPressed, this, [this](const QPoint &point) { move(this->frameGeometry().topLeft() + point); });

	connect(ui->playListWidget, &PLSBgmDragView::MouseDoublePressedSignal, this, &PLSBackgroundMusicView::OnPlayListItemDoublePressed);
	connect(ui->playListWidget, &PLSBgmDragView::RowChanged, this, &PLSBackgroundMusicView::OnPlayListItemRowChanged);
	connect(ui->playListWidget, &PLSBgmDragView::AudioFileDraggedIn, this, &PLSBackgroundMusicView::OnAudioFileDraggedIn);
	connect(ui->playListWidget, &PLSBgmDragView::DelButtonClickedSignal, this, &PLSBackgroundMusicView::OnDelButtonClicked);
	connect(ui->playListWidget, &PLSBgmDragView::LoadingFailed, this, &PLSBackgroundMusicView::OnLoadFailed);
	connect(ui->noPlayListFrame, &DragInFrame::AudioFileDraggedIn, this, &PLSBackgroundMusicView::OnAudioFileDraggedIn);

	connect(ui->playingSlider, SIGNAL(sliderPressed()), this, SLOT(SliderClicked()));
	connect(ui->playingSlider, SIGNAL(sliderReleased()), this, SLOT(SliderReleased()));
	connect(ui->playingSlider, SIGNAL(sliderMoved(int)), this, SLOT(SliderMoved(int)));

	obs_frontend_add_event_callback(PLSFrontendEvent, this);

	pls_network_state_monitor([this](bool accsible) { networkAvailable = accsible; });
}

PLSBackgroundMusicView::~PLSBackgroundMusicView()
{
	obs_frontend_remove_event_callback(PLSFrontendEvent, this);

	PLSBgmDataViewManager::Instance()->DeleteGroupButton();
	PLSBgmDataViewManager::Instance()->ClearCachePlayList();

	if (nullptr != sendThreadObj) {
		sendThreadObj->deleteLater();
	}
	sendThread.quit();
	sendThread.wait();

	if (nullptr != coverThreadObj) {
		coverThreadObj->deleteLater();
	}
	coverThread.quit();
	coverThread.wait();

	if (nullptr != checkThreadObj) {
		checkThreadObj->deleteLater();
	}
	checkThread.quit();
	checkThread.wait();
	pls_delete(ui);
}

void PLSBackgroundMusicView::AddSource(const QString &sourceName, quint64 sceneItem, bool createNew)
{
	InitSourceSettingsData(sourceName, sceneItem, createNew);
}

void PLSBackgroundMusicView::AddSourceAndRefresh(const QString &sourceName, quint64 sceneItem)
{
	AddSource(sourceName, sceneItem, true);
}

void PLSBackgroundMusicView::RemoveSourceAndRefresh(const QString &sourceName, quint64 sceneItem)
{
	RemoveSource(sourceName, sceneItem);
	UpdateSourceSelectUI();
}

void PLSBackgroundMusicView::RemoveSource(const QString &, quint64 sceneItem)
{
	//default
	clearUI(sceneItem);

	if (SameWithCurrentSource(sceneItem)) {
		ui->playListWidget->Clear();
		currentSceneItem = 0;
		currentSourceName = "";
	}
}

void PLSBackgroundMusicView::RemoveBgmSourceList(const BgmSourceVecType &sourceList)
{
	for (auto &bgm : sourceList) {
		RemoveSource(bgm.first, bgm.second);
	}
}

void PLSBackgroundMusicView::InitSourceSettingsData(const QString &sourceName, const quint64 &sceneItem, bool createNew, bool)
{
	OBSSource source = pls_get_source_by_name(sourceName.toStdString().c_str());
	if (!source) {
		return;
	}
	OBSData settings = obs_data_create();
	pls_source_get_private_data(source, settings);
	if (createNew) {
		currentSceneItem = sceneItem;
		currentSourceName = sourceName;
	}
	UpdateUIBySourceSettings(settings, source, sceneItem, createNew);
	obs_data_release(settings);
}

void PLSBackgroundMusicView::OnCurrentPageChanged(int index)
{
	const QWidget *currentPage = ui->stackedWidget->widget(index);
	if (currentPage == ui->playListPage) {
		UpdatePlayListUI(currentSceneItem);
		if (0 == ui->playListWidget->Count()) {
			AddPlayListUI(currentSceneItem);
		}
		ui->addSourceBtn->setVisible(false);
		ui->addMusicBtn->setVisible(true);
		ui->tabWidget->setVisible(true);

		int row = ui->playListWidget->GetCurrentRow();
		if (-1 != row && isVisible()) {
			ui->playListWidget->scrollTo(ui->playListWidget->GetModelIndex(row));
		}

	} else if (currentPage == ui->noSourcePage) {
		ResetControlView();
		ui->playListWidget->Clear();
		ui->sourceNameLabel->SetText("");
		ui->addSourceBtn->setVisible(true);
		ui->addMusicBtn->setVisible(false);
		ui->tabWidget->setVisible(false);
	}
}

void PLSBackgroundMusicView::SliderClicked()
{
	OBSSource source = pls_get_source_by_name(currentSourceName.toStdString().c_str());
	if (!source) {
		return;
	}
	obs_media_state state = obs_source_media_get_state(source);
	if (state == OBS_MEDIA_STATE_PLAYING) {
		prevPaused = false;
		obs_source_media_play_pause(source, true);
		StopSliderPlayingTimer();
	} else if (state == OBS_MEDIA_STATE_PAUSED) {
		prevPaused = true;
	}

	seek = ui->playingSlider->value();
	seeking = true;
	seekTimer.start(100);
}

void PLSBackgroundMusicView::SliderReleased()
{
	OBSSource source = pls_get_source_by_name(currentSourceName.toStdString().c_str());
	if (!source) {
		return;
	}
	if (seekTimer.isActive()) {
		seeking = false;
		seekTimer.stop();
		if (lastSeek != seek) {
			obs_source_media_set_time(source, GetSliderTime(seek));
		}

		seek = lastSeek = -1;
	}

	if (!prevPaused) {
		obs_source_media_play_pause(source, false);
		StartSliderPlayingTimer();
	}
}

void PLSBackgroundMusicView::SliderMoved(int val)
{
	if (seekTimer.isActive()) {
		seek = val;
	}
	ui->playingSlider->setValue(val);
}

void PLSBackgroundMusicView::SetSliderPos()
{
	pls_check_app_exiting();
	OBSSource source = pls_get_source_by_name(currentSourceName.toStdString().c_str());
	if (!source) {
		return;
	}

	obs_media_state state = obs_source_media_get_state(source);
	bool visible = obs_sceneitem_visible(pls_get_sceneitem_by_pointer_address((void *)currentSceneItem));
	if ((state != OBS_MEDIA_STATE_PLAYING && state != OBS_MEDIA_STATE_PAUSED) || !visible) {
		return;
	}

	auto time = (float)obs_source_media_get_time(source);
	if (time < 1) {
		return;
	}
	auto duration = (float)obs_source_media_get_duration(source);
	float sliderPosition = 0.0f;

	sliderPosition = (time / duration) * (float)ui->playingSlider->maximum();
	ui->currentTimeLabel->setText(PLSBgmDataViewManager::Instance()->ConvertIntToTimeString((int)(time / 1000.0f)));
	ui->durationLabel->setText(PLSBgmDataViewManager::Instance()->ConvertIntToTimeString((int)(duration / 1000.0f)));
	ui->playingSlider->setValue((int)(sliderPosition));
}

void PLSBackgroundMusicView::SeekTimerCallback()
{
	if (lastSeek != seek) {
		OBSSource source = pls_get_source_by_name(currentSourceName.toStdString().c_str());
		if (source) {
			obs_source_media_set_time(source, GetSliderTime(seek));
		}
		lastSeek = seek;
	}
}

void PLSBackgroundMusicView::UpdateUIBySourceSettings(obs_data_t *settings, OBSSource, const quint64 &sceneItem, bool createNew)
{
	if (!settings) {
		return;
	}

	if (!SameWithCurrentSource(sceneItem)) {
		return;
	}

	bool playInOrder = obs_data_get_bool(settings, PLAY_IN_ORDER);
	if (playInOrder) {
		if (mode != PlayMode::InOrderMode) {
			SetCurrentPlayMode(PlayMode::InOrderMode);
		}
	}

	bool random = obs_data_get_bool(settings, RANDOM_PLAY);
	if (random) {
		if (mode != PlayMode::RandomMode) {
			SetCurrentPlayMode(PlayMode::RandomMode);
		}
	}

	if (!playInOrder && !random) {
		SetCurrentPlayMode(PlayMode::InOrderMode);
	}

	bool isLoop = true;
	if (!createNew) {
		isLoop = obs_data_get_bool(settings, IS_LOOP);
	}

	ui->loopBtn->setChecked(isLoop);
	SetLoop(sceneItem, isLoop);
	AddPlayListUI(sceneItem);
}

void PLSBackgroundMusicView::ClearUrlInfo()
{
	const PLSBasic *main = PLSBasic::instance();
	BgmSourceVecType playList = main->EnumAllBgmSource();
	for (const auto &iter : playList) {
		obs_source_t *source = GetSource(iter.second);
		SetUrlInfo(source, PLSBgmItemData());
	}
}

void PLSBackgroundMusicView::DisconnectSignalsWhenAppClose() const
{
	auto main = static_cast<PLSBasic *>(App()->GetMainWindow());
	if (!main) {
		return;
	}

	auto enumBgmSource = [](void *param, obs_source_t *source) {
		if (!source)
			return true;
		QVector<OBSSource> &bgmSource = *static_cast<QVector<OBSSource> *>(param);
		if (0 == strcmp(obs_source_get_id(source), BGM_SOURCE_ID)) {
			bgmSource.push_back(source);
		}
		return true;
	};

	QVector<OBSSource> bgmSourcesList;
	obs_enum_sources(enumBgmSource, &bgmSourcesList);

	for (auto item : bgmSourcesList) {
		signal_handler_disconnect(obs_source_get_signal_handler(item), "media_restart", PLSBasic::MediaRestarted, nullptr);
	}
}

void PLSBackgroundMusicView::OnSceneChanged()
{
	QString name{};
	quint64 item{};
	auto main = static_cast<PLSBasic *>(App()->GetMainWindow());
	if (!main) {
		return;
	}
	main->GetSelectBgmSourceName(name, item);
	BgmSourceVecType sourceList = main->GetCurrentSceneBgmSourceList();
	UpdateSourceList(name, item, sourceList);

	ui->sourceNameLabel->SetText(currentSourceName);
	UpdateSourceSelectUI();
}

void PLSBackgroundMusicView::UpdateLoadingStartState(const QString &)
{
	if (!CheckNetwork()) {
		return;
	}
	int row = ui->playListWidget->GetCurrentRow();
	if (-1 != row) {
		StopLoadingTimer();
		PLSBgmItemDelegate::totalFrame(8);
		PLSBgmItemDelegate::setCurrentFrame(1);
		indexLoading = row;
		ui->playListWidget->SetMediaStatus(row, MediaStatus::stateLoading);
		StartLoadingTimer();
	}
	isLoading = true;
	SetPlayerControllerStatus(currentSceneItem);
}

void PLSBackgroundMusicView::UpdateLoadingEndState(const QString &sourceName)
{
	if (!SameWithCurrentSource(sourceName)) {
		return;
	}

	if (!CheckNetwork()) {
		return;
	}

	int row = ui->playListWidget->GetCurrentRow();
	if (-1 != row) {
		StopLoadingTimer();

		obs_source_t *source = pls_get_source_by_name(sourceName.toStdString().c_str());
		obs_media_state state = obs_source_media_get_state(source);
		if (state == OBS_MEDIA_STATE_PLAYING) {
			indexLoading = row;
			PLSBgmItemDelegate::totalFrame(21);
			PLSBgmItemDelegate::setCurrentFrame(1);
			ui->playListWidget->SetMediaStatus(row, MediaStatus::statePlaying);
			StartLoadingTimer(100);
		} else if (state == OBS_MEDIA_STATE_PAUSED) {
			ui->playListWidget->SetMediaStatus(row, MediaStatus::statePause);
		}
	}
	SetPlayerControllerStatus(currentSceneItem);
}

void PLSBackgroundMusicView::OnMediaStateChanged(const QString &name, obs_media_state state)
{
	OBSSource source = pls_get_source_by_name(name.toStdString().c_str());
	if (!source) {
		return;
	}

	switch (state) {
	case OBS_MEDIA_STATE_STOPPED:
	case OBS_MEDIA_STATE_ENDED:
		UpdateStopUIState(name);
		break;
	case OBS_MEDIA_STATE_OPENING:
		UpdateOpeningUIState(name);
		break;
	case OBS_MEDIA_STATE_PLAYING: {
		UpdateOpeningUIState(name);
		UpdateStatuPlayling(name);
	} break;
	case OBS_MEDIA_STATE_PAUSED:
		UpdatePauseUIState(name);
		break;
	case OBS_MEDIA_STATE_ERROR:
		UpdateErrorUIState(name, true);
		break;
	default:
		break;
	}
	last_state = state;
}

void PLSBackgroundMusicView::OnMediaRestarted(const QString &name) const
{

	obs_source_t *source = pls_get_source_by_name(name.toStdString().c_str());
	if (!source) {
		return;
	}
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "method", "get_select_url");
	pls_source_get_private_data(source, settings);

	PLSBgmItemData bgmData;
	bool isCurrent = obs_data_get_bool(settings, BGM_IS_CURRENT);
	if (!isCurrent) {
		obs_data_release(settings);
		return;
	}

	const char *categoryId = obs_data_get_string(settings, BGM_GROUP);
	const char *musicId = obs_data_get_string(settings, BGM_URL);
	const char *duration = obs_data_get_string(settings, BGM_DURATION);
	bool isLocalFile = obs_data_get_bool(settings, BGM_IS_LOCAL_FILE);
	const char *type = isLocalFile ? "user" : "prism";

	pls_send_analog(AnalogType::ANALOG_PLAY_BGM, {{ANALOG_BGM_CATEGORY_KEY, categoryId}, {ANALOG_BGM_TYPE_KEY, type}, {ANALOG_BGM_ID_KEY, musicId}, {ANALOG_BGM_DURATION_KEY, duration}});

	obs_data_release(settings);
}

void PLSBackgroundMusicView::OnLoopStateChanged(const QString &name)
{
	OBSSource source = pls_get_source_by_name(name.toStdString().c_str());
	if (!source) {
		return;
	}

	if (!SameWithCurrentSource(name)) {
		return;
	}

	OBSData settings = obs_source_get_private_settings(source);
	bool loop = obs_data_get_bool(settings, IS_LOOP);
	if (ui->loopBtn->isChecked() == loop) {
		return;
	}

	ui->loopBtn->setChecked(loop);
	pls_flush_style(ui->loopBtn, STATUS_PRESSED, loop);
}

void PLSBackgroundMusicView::UpdatePlayingUIState(const QString &name)
{
	if (!SameWithCurrentSource(name)) {
		return;
	}

	int row = ui->playListWidget->GetCurrentRow();
	if (-1 != row) {
		StopLoadingTimer();
		indexLoading = row;
		PLSBgmItemDelegate::totalFrame(21);
		PLSBgmItemDelegate::setCurrentFrame(1);
		ui->playListWidget->SetMediaStatus(row, MediaStatus::statePlaying);
		StartLoadingTimer(100);
		ui->playingSlider->setEnabled(true);
	}

	SetPlayerControllerStatus(currentSceneItem);
	ShowCoverGif(true);
}

void PLSBackgroundMusicView::UpdatePauseUIState(const QString &name)
{
	if (!SameWithCurrentSource(name)) {
		return;
	}

	if (seeking) {
		return;
	}

	StopLoadingTimer();
	int row = ui->playListWidget->GetCurrentRow();
	if (-1 != row) {
		ui->playListWidget->SetMediaStatus(row, MediaStatus::statePause);
	}
	SetPlayerControllerStatus(currentSceneItem);
	ShowCoverGif(false);
}

void PLSBackgroundMusicView::UpdateStopUIState(const QString &name)
{
	if (!SameWithCurrentSource(name)) {
		OBSSource source = pls_get_source_by_name(name.toStdString().c_str());
		if (!source) {
			return;
		}
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "method", "get_current_url");
		pls_source_get_private_data(source, settings);
		PLSBgmItemData bgmData = GetPlayListDataBySettings(settings);
		if (!CheckNetwork() || !CheckValidLocalAudioFile(bgmData.GetUrl(bgmData.id))) {
			OnInvalidSongs(name, true);
		} else {
			obs_source_media_next(source);
		}
		return;
	}

	PLSBgmItemData data = GetCurrentPlayListData();
	if (!CheckNetwork() && !data.isLocalFile) {
		OnNoNetwork(QTStr("Bgm.No.Network.Toast"), data);
	}

	if (data.isLocalFile) {
		if (bool valid = CheckValidLocalAudioFile(data.GetUrl(data.id)); !valid) {
			UpdateErrorUIState(name, true);
		} else {
			obs_source_media_next(GetSource(currentSceneItem));
		}
	}

	StopLoadingTimer();
	StopSliderPlayingTimer();
	pls_flush_style(ui->playBtn, STATUS_STATE, STATUS_PLAY);
	ui->playBtn->setToolTip(QTStr("Bgm.Play"));

	int row = ui->playListWidget->GetCurrentRow();
	if (-1 != row) {
		ui->playListWidget->SetMediaStatus(row, MediaStatus::stateNormal);
	}

	SetPlayerControllerStatus(currentSceneItem);
	ShowCoverGif(false);
}

void PLSBackgroundMusicView::UpdateOpeningUIState(const QString &name)
{
	OnUpdateOpeningUIState(name);

	OBSOutput streamingOutput = obs_frontend_get_streaming_output();
	obs_output_release(streamingOutput);
	if (obs_output_active(streamingOutput)) {
		if (!sendThreadObj) {
			sendThreadObj = pls_new<SendThread>();
			sendThreadObj->moveToThread(&sendThread);
			sendThread.start();
		}
		PLSBgmItemData data = GetCurrentPlayListDataBySettings(name);
		QMetaObject::invokeMethod(sendThreadObj, "DoWork", Qt::QueuedConnection, Q_ARG(PLSBgmItemData, data));
	}
}

void PLSBackgroundMusicView::UpdateErrorUIState(const QString &name, bool gotoNext)
{
	if (!SameWithCurrentSource(name)) {
		OnInvalidSongs(name);
		return;
	}

	PLSBgmItemData data = GetCurrentPlayListData();
	QString toast = CheckNetwork() ? QTStr("Bgm.Songs.Invalid.Toast") : QTStr("Bgm.No.Network.Toast");
	OnNoNetwork(toast, data, gotoNext);
	ShowCoverGif(false);
}

void PLSBackgroundMusicView::OnUpdateOpeningUIState(const QString &name)
{
	OBSSource source = pls_get_source_by_name(name.toStdString().c_str());
	if (source) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "method", "bgm_get_opening");
		pls_source_set_private_data(source, settings);
		obs_data_release(settings);
	}

	if (!SameWithCurrentSource(name)) {
		return;
	}

	if (!coverThreadObj) {
		coverThreadObj = pls_new<GetCoverThread>();
		coverThreadObj->moveToThread(&coverThread);
		connect(coverThreadObj, &GetCoverThread::Finished, this, &PLSBackgroundMusicView::UpdateBgmCoverPath, Qt::QueuedConnection);
		connect(
			coverThreadObj, &GetCoverThread::GetPreviewImage, this,
			[this](const QImage &image, const PLSBgmItemData &data) {
				PLSBgmItemData curData = GetCurrentPlayListData();
				if (curData.id != data.id || curData.GetUrl(curData.id) != data.GetUrl(data.id)) {
					return;
				}
				if (image.isNull()) {
					PLSBgmItemData temp = data;
					temp.coverPath = DEFAULT_COVER_IMAGE;
					ui->coverLabel->SetCoverPath(temp.coverPath, !temp.isLocalFile);
					UpdateBgmCoverPath(temp);
				} else {
					ui->coverLabel->SetImage(image);
				}
			},
			Qt::QueuedConnection);
		coverThread.start();
	}

	if (IsSameState(source, OBS_MEDIA_STATE_PLAYING) || IsSameState(source, OBS_MEDIA_STATE_PAUSED)) {
		return;
	}

	int row = ui->playListWidget->GetCurrentRow();
	if (-1 != row) {
		ui->playListWidget->SetMediaStatus(row, MediaStatus::stateNormal);
	}

	PLSBgmItemData data = GetCurrentPlayListDataBySettings();

	ui->playingSlider->setEnabled(false);
	ui->playingSlider->setValue(0);
	ui->currentTimeLabel->setText("00:00");
	ui->coverLabel->SetMusicInfo(data.title, data.producer);
	ui->playListWidget->SetCurrentRow(data);

	ShowCoverGif(false);
	ShowCoverImage(data);
	SetPlayerControllerStatus(currentSceneItem);
}

void PLSBackgroundMusicView::UpdateStatuPlayling(const QString &name)
{
	UpdatePlayingUIState(name);
	PLSBgmItemData data = GetCurrentPlayListDataBySettings(name);
	if (!data.title.isEmpty()) {
		action::SendActionLog(action::ActionInfo(EVENT_PLAY, EVENT_PRISM_MUSIC, EVENT_PLAYED, data.title));
		action::SendPropToNelo(BGM_SOURCE_ID, "played", qUtf8Printable(data.title));
	}
}

void PLSBackgroundMusicView::UpdateLoadUIState(const QString &name, bool load, bool)
{
	if (!CheckNetwork()) {
		return;
	}
	if (!SameWithCurrentSource(name)) {
		if (!CheckNetwork()) {
			OnLoadFailed(name);
		}
		isLoading = false;
		return;
	}

	if (load) {
		UpdateLoadingStartState(name);
		return;
	}

	UpdateLoadingEndState(name);
}

void PLSBackgroundMusicView::SetCurrentPlayMode(PlayMode mode_)
{
	this->mode = mode_;
	if (mode == PlayMode::InOrderMode) {
		ui->refreshBtn->setToolTip(QTStr("Bgm.PlayInOrder"));
		pls_flush_style(ui->refreshBtn, "playMode", "inOrder");
	} else {
		ui->refreshBtn->setToolTip(QTStr("Bgm.Shuffle"));
		pls_flush_style(ui->refreshBtn, "playMode", "random");
	}

	OBSSource source = pls_get_source_by_name(currentSourceName.toStdString().c_str());
	if (!source) {
		return;
	}
	OBSData settings = obs_data_create();
	obs_data_set_string(settings, "method", "bgm_play_mode");
	obs_data_set_bool(settings, PLAY_IN_ORDER, mode == PlayMode::InOrderMode);
	obs_data_set_bool(settings, RANDOM_PLAY, mode == PlayMode::RandomMode);
	pls_source_set_private_data(source, settings);
	obs_data_release(settings);
}

void PLSBackgroundMusicView::SetSourceSelect(const QString &sourceName, quint64 sceneItem, bool)
{
	PLSBasic *main = PLSBasic::instance();
	if (!main) {
		return;
	}
	const obs_sceneitem_t *sceneitem = pls_get_sceneitem_by_pointer_address(main->GetCurrentScene(), (void *)sceneItem);
	if (!sceneitem || !obs_sceneitem_visible(sceneitem)) {
		return;
	}

	if (!SameWithCurrentSource(sourceName)) {
		if (isLoading && !CheckNetwork()) {
			OnLoadFailed(currentSourceName);
		}
		currentSceneItem = sceneItem;
		currentSourceName = sourceName;
		isLoading = false;
		InitSourceSettingsData(currentSourceName, currentSceneItem, false);
	}

	currentSceneItem = sceneItem;
	ui->sourceNameLabel->SetText(currentSourceName);
	ui->stackedWidget->setCurrentWidget(ui->playListPage);
	UpdatePlayListUI(sceneItem);
	SetPlayerControllerStatus(sceneItem);
	PLSBgmItemData data = GetCurrentPlayListDataBySettings(sourceName);
	ShowCoverImage(data);
	SetPlayListStatus(data);
}

void PLSBackgroundMusicView::SetSourceVisible(const QString &, quint64, bool)
{
	UpdateSourceSelectUI();
}

void PLSBackgroundMusicView::UpdateSourceList(const QString &sourceName, quint64 sceneItem, const BgmSourceVecType &sourceList)
{
	for (auto &bgm : sourceList) {
		if (!obs_sceneitem_visible(pls_get_sceneitem_by_pointer_address((void *)sceneItem))) {
			continue;
		}
		AddSource(bgm.first, bgm.second, false);
	}

	if (sourceName.isEmpty()) {
		SetPlayerControllerStatus(sceneItem);
		UpdatePlayListUI(sceneItem);
	}
}

void PLSBackgroundMusicView::RenameSourceName(const quint64 &, const QString &newName, const QString &prevName)
{
	if (prevName == currentSourceName) {
		currentSourceName = newName;
		ui->sourceNameLabel->SetText(currentSourceName);
	}
}

void PLSBackgroundMusicView::closeEvent(QCloseEvent *event)
{
	hide();
	event->ignore();
}

void PLSBackgroundMusicView::showEvent(QShowEvent *event)
{
	PLSSideBarDialogView::showEvent(event);
	OnRetryNetwork();
	UpdateSourceSelectUI();
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::BgmConfig, true);
}

void PLSBackgroundMusicView::hideEvent(QHideEvent *event)
{
	PLSSideBarDialogView::hideEvent(event);
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::BgmConfig, false);
}

void PLSBackgroundMusicView::resizeEvent(QResizeEvent *event)
{
	PLSSideBarDialogView::resizeEvent(event);
	if (toastView.isVisible()) {
		QTimer::singleShot(0, this, [this]() { ResizeToastView(); });
	}
}

void PLSBackgroundMusicView::OnDelButtonClicked(const PLSBgmItemData &data)
{
	if (data.title.isEmpty()) {
		return;
	}

	obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		return;
	}

	ui->playListWidget->Remove(data);
	auto index = ui->playListWidget->indexAt(ui->playListWidget->mapFromGlobal(QCursor::pos()));
	ui->playListWidget->UpdataData(index.row(), QVariant::fromValue(RowStatus::stateHover), CustomDataRole::RowStatusRole);
	ui->playListWidget->update(index);
	obs_data_t *delData = obs_data_create();
	obs_data_set_string(delData, "method", "bgm_remove");
	obs_data_set_string(delData, "remove_url", data.GetUrl(data.id).toStdString().c_str());
	obs_data_set_string(delData, BGM_DURATION_TYPE, QString::number(data.id).toStdString().c_str());

	pls_source_set_private_data(source, delData);
	obs_data_release(delData);

	UpdateCurrentPlayStatus(currentSourceName);
	UpdatePlayListUI(currentSceneItem);
	Save();
	RefreshPropertyWindow();
}

void PLSBackgroundMusicView::OnNoNetwork(const QString &toast, const PLSBgmItemData &data_, bool gotoNext)
{
	obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		return;
	}

	obs_media_state state = obs_source_media_get_state(source);
	if (state != OBS_MEDIA_STATE_PLAYING && state != OBS_MEDIA_STATE_PAUSED && state != OBS_MEDIA_STATE_OPENING && state != OBS_MEDIA_STATE_ERROR && state != OBS_MEDIA_STATE_STOPPED &&
	    state != OBS_MEDIA_STATE_ENDED) {
		return;
	}

	int row = ui->playListWidget->GetCurrentRow();
	if (-1 != row) {
		PLSBgmItemData data = data_;
		data.isDisable = true;
		data.isCurrent = false;
		ui->playListWidget->UpdataData(row, data);
		ui->playListWidget->SetMediaStatus(row, MediaStatus::stateInvalid);
		data.isLocalFile ? ShowToastView(QTStr("Bgm.Songs.Invalid.Toast").arg(data.title)) : ShowToastView(toast.arg(data.title));
		StopSliderPlayingTimer();
		StopLoadingTimer();
		OnInvalidSongs(currentSourceName, true);
	} else {
		ResetControlView();
	}
}

void PLSBackgroundMusicView::OnInvalidSongs(const QString &name, bool gotoNext) const
{
	OBSSource source = pls_get_source_by_name(name.toStdString().c_str());
	if (!source) {
		return;
	}

	PLSBgmItemData data = GetCurrentPlayListDataBySettings(name);
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "method", "bgm_disable");
	obs_data_set_string(settings, BGM_URL, data.GetUrl(data.id).toStdString().c_str());
	obs_data_set_string(settings, BGM_DURATION_TYPE, QString::number(data.id).toStdString().c_str());
	obs_data_set_bool(settings, "goto_next_songs", gotoNext);

	pls_source_set_private_data(source, settings);
	obs_data_release(settings);
}

void PLSBackgroundMusicView::OnRetryNetwork()
{
	if (!CheckNetwork()) {
		return;
	}

	const obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		return;
	}

	QVector<PLSBgmItemData> availableDatas;
	bool needUpdateDisableConfig = false;
	for (int i = 0; i < ui->playListWidget->Count(); i++) {
		PLSBgmItemData data = ui->playListWidget->Get(i);
		if (data.isDisable) {
			if (data.isLocalFile) {
				CreateCheckValidThread();
				QMetaObject::invokeMethod(checkThreadObj, "CheckUrlAvailable", Qt::QueuedConnection, Q_ARG(PLSBgmItemData, data));
				continue;
			}

			needUpdateDisableConfig = true;
			ui->playListWidget->UpdataData(i, QVariant::fromValue(MediaStatus::stateNormal), CustomDataRole::MediaStatusRole);
			availableDatas.push_back(data);
		}
	}
	if (needUpdateDisableConfig) {
		RefreshMulicEnabledPlayList(currentSceneItem, availableDatas);
	}
	SetPlayListStatus(ui->playListWidget->GetCurrent());
	Save();
}

void PLSBackgroundMusicView::OnPlayListItemDoublePressed(const QModelIndex &index)
{
	PLSBgmItemData data = ui->playListWidget->GetData(index);
	obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		return;
	}

	if (data.isDisable) {
		return;
	}

	PLS_INFO(MAIN_BGM_MODULE, "switch to next music: %s", data.title.toStdString().c_str());
	SetUrlInfo(source, data);
}

void PLSBackgroundMusicView::OnPlayListItemRowChanged(const int &srcIndex, const int &destIndex)
{
	if (srcIndex == destIndex) {
		return;
	}

	obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		return;
	}

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "method", "bgm_row_changed");
	obs_data_set_int(settings, "src_index", srcIndex);
	obs_data_set_int(settings, "dest_index", destIndex);
	pls_source_set_private_data(source, settings);

	pls_source_get_private_data(source, settings);
	QVector<PLSBgmItemData> datas = GetPlayListData(settings);
	obs_data_release(settings);

	ui->playListWidget->UpdateWidget(datas);

	PLSBgmItemData data = GetCurrentPlayListData();
	SetPlayListStatus(data);
	SetPlayerControllerStatus(currentSceneItem);
	UpdateCurrentPlayStatus(currentSourceName);
	Save();
	RefreshPropertyWindow();
}

static QString GetTempImageFilePath(const QString &suffix, const int &index)
{
	QDir temp = QDir::temp();
	temp.mkdir("musicImages");
	temp.cd("musicImages");
	QString tempImageFilePath = temp.absoluteFilePath(QString("musicImages-%1").arg(QDateTime::currentMSecsSinceEpoch() + index) + suffix);
	return tempImageFilePath;
}

static QImage CaptureImage(uint32_t width, uint32_t height, const char *data, int size, const int &)
{
	if (!data) {
		return QImage();
	}

	std::vector<uchar> buffer(size);

	if (width * height * 4 != unsigned(size)) {
		PLS_WARN(MAIN_BGM_MODULE, "cover image size was not correct, width=[%d], height=[%d], size=[%d].", width, height, size);
	}

	memset(buffer.data(), 0, size);
	memmove(buffer.data(), data, size);
	QImage image(buffer.data(), width, height, QImage::Format_RGBA8888);
	QImage imageCopy = image.copy();
	return imageCopy;
}

void PLSBackgroundMusicView::OnAudioFileDraggedIn(const QStringList &paths)
{
	if (paths.empty()) {
		return;
	}
	PLS_INFO(MAIN_BGM_MODULE, QString("Add %1 Local Songs").arg(paths.size()).toStdString().c_str());

	bool existedSameUrl = false;
	QVector<PLSBgmItemData> datas;
	for (auto &path : paths) {
		if (path.isEmpty()) {
			continue;
		}
		PLSBgmItemData data;
		data.id = 0;
		if (ui->playListWidget->Existed(path)) {
			existedSameUrl = true;
			continue;
		}
		data.SetUrl(path, data.id);

		media_info_t media_info;
		memset(&media_info, 0, sizeof(media_info_t));
		bool open = mi_open(&media_info, path.toStdString().c_str(), MI_OPEN_DIRECTLY);
		if (open && 0 == path.right(3).toLower().compare("mp3")) {
			data.title = mi_get_string(&media_info, "title");
			data.producer = mi_get_string(&media_info, "artist");
			data.SetDuration(data.id, (int)mi_get_int(&media_info, "duration") / 1000);
			data.haveCover = mi_get_bool(&media_info, "has_cover");
		}
		if (data.title.isEmpty()) {
			data.title = path.mid(path.lastIndexOf('/') + 1);
		}
		if (data.producer.isEmpty()) {
			data.producer = "Unknown";
		}
		if (0 == data.GetDuration(data.id)) {
			data.SetDuration(data.id, (int)mi_get_int(&media_info, "duration") / 1000);
		}
		data.isLocalFile = true;

		datas.push_back(data);
		mi_free(&media_info);
	}

	if (existedSameUrl) {
		ShowToastView(QTStr("Bgm.Existed.Same.Url.Tips"));
	}

	OnAddCachePlayList(datas);
}

void PLSBackgroundMusicView::OnAddCachePlayList(const QVector<PLSBgmItemData> &datas_)
{
	QVector<PLSBgmItemData> datas = datas_;

	obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		return;
	}

	obs_data_t *settings = obs_data_create();
	obs_data_array_t *playLists = obs_data_array_create();

	for (auto &data : datas) {
		obs_data_t *playList = obs_data_create();
		if (!data.group.isEmpty()) {
			data.coverPath = QString(":/resource/images/bgm/group-%1.png").arg(data.group.toLower());
		}

		SetUrlInfoToSettings(playList, data);
		obs_data_array_insert(playLists, 0, playList);
		obs_data_release(playList);
	}
	obs_data_set_array(settings, PLAY_LIST, playLists);
	obs_data_set_string(settings, "method", "bgm_insert_playlist");
	pls_source_set_private_data(source, settings);

	obs_data_array_release(playLists);
	obs_data_release(settings);

	ui->playListWidget->InsertWidget(datas);
	UpdateCurrentPlayStatus(currentSourceName);
	UpdatePlayListUI(currentSceneItem);
	Save();
	RefreshPropertyWindow();
}

void PLSBackgroundMusicView::OnPlayButtonClicked() const
{
	OBSSource source = pls_get_source_by_name(currentSourceName.toStdString().c_str());
	if (!source) {
		return;
	}
	obs_media_state state = obs_source_media_get_state(source);
	switch (state) {
	case OBS_MEDIA_STATE_STOPPED:
	case OBS_MEDIA_STATE_ENDED:
	case OBS_MEDIA_STATE_NONE:
		obs_source_media_restart(source);
		break;
	case OBS_MEDIA_STATE_PLAYING:
		obs_source_media_play_pause(source, true);
		break;
	case OBS_MEDIA_STATE_PAUSED:
		obs_source_media_play_pause(source, false);
		break;
	default:
		break;
	}
}

void PLSBackgroundMusicView::OnPreButtonClicked()
{
	obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		return;
	}

	obs_source_media_previous(source);
}

void PLSBackgroundMusicView::OnNextButtonClicked()
{
	obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		return;
	}

	obs_source_media_next(source);
}

void PLSBackgroundMusicView::OnLoopBtnClicked(const quint64 &sceneItem, bool checked)
{
	SetLoop(sceneItem, checked);
}

void PLSBackgroundMusicView::OnLocalFileBtnClicked()
{
	QString filter("(*.mp3 *.aac *.ogg *.wav)");
	QStringList paths;

	if (localFilePath.isEmpty()) {
		localFilePath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
		paths = QFileDialog::getOpenFileNames(this, QString(), localFilePath, filter);
	} else {
		paths = QFileDialog::getOpenFileNames(this, QString(), "", filter);
	}

	OnAudioFileDraggedIn(paths);
}

void PLSBackgroundMusicView::OnLibraryBtnClicked()
{
	auto libraryView_ = pls_new<PLSBgmLibraryView>(this);

#if defined(Q_OS_MACOS)
	libraryView_->initSize(720, 610);
#elif defined(Q_OS_WIN)
	libraryView_->initSize(720, 650);
#endif
	libraryView_->setAttribute(Qt::WA_DeleteOnClose);
	connect(libraryView_, &PLSBgmLibraryView::AddCachePlayList, this, &PLSBackgroundMusicView::OnAddCachePlayList);
	libraryView_->exec();
}

void PLSBackgroundMusicView::OnAddSourceBtnClicked() const
{
	PLSBasic *main = PLSBasic::instance();
	if (!main) {
		return;
	}

	main->AddSource(BGM_SOURCE_ID);
}

void PLSBackgroundMusicView::OnAddMusicBtnClicked()
{
	QMenu popup(ui->addMusicBtn);
	popup.setObjectName("addMusicMenu");

	auto addLocalAction = pls_new<QAction>(QTStr("Bgm.Add.Local.File"), &popup);
	auto addLibraryAction = pls_new<QAction>(QTStr("Bgm.Add.Free.Music"), &popup);
	connect(addLocalAction, &QAction::triggered, this, &PLSBackgroundMusicView::OnLocalFileBtnClicked);
	connect(addLibraryAction, &QAction::triggered, this, &PLSBackgroundMusicView::OnLibraryBtnClicked);

	popup.addAction(addLocalAction);
	popup.addAction(addLibraryAction);

	popup.exec(QCursor::pos());
}

void PLSBackgroundMusicView::OnLoadFailed(const QString &name)
{
	UpdateLoadingEndState(name.isEmpty() ? currentSourceName : name);
	UpdateErrorUIState(name.isEmpty() ? currentSourceName : name, true);
}

void PLSBackgroundMusicView::OnRefreshButtonClicked()
{
	if (mode == PlayMode::InOrderMode) {
		SetCurrentPlayMode(PlayMode::RandomMode);
	} else if (mode == PlayMode::RandomMode) {
		SetCurrentPlayMode(PlayMode::InOrderMode);
	}
}

void PLSBackgroundMusicView::initUI()
{
	setupUi(ui);
	ui->currentTimeLabel->setText("00:00");
	ui->stackedWidget->setCurrentWidget(ui->noSourcePage);
	ui->addSourceBtn->setVisible(true);
	ui->addMusicBtn->setVisible(false);
	ui->playListFrame->setVisible(false);
	ui->tabWidget->setVisible(false);
	ui->verticalLayout->setAlignment(Qt::AlignTop);
	ui->loadingBtn->hide();
	ui->infoframe->setCursor(Qt::ArrowCursor);
	ui->noticeLabel->setContentsMargins(20, 0, 20, 0);
	ui->preBtn->setToolTip(QTStr("Bgm.Previous"));
	ui->nextBtn->setToolTip(QTStr("Bgm.Next"));
	ui->loopBtn->setToolTip(QTStr("Bgm.Repeat"));
	SetCurrentPlayMode(PlayMode::InOrderMode);
	ResetControlView();
	InitToast();
	pls_flush_style(ui->playBtn, STATUS_STATE, STATUS_PLAY);
	ui->playBtn->setToolTip(QTStr("Bgm.Play"));
	this->setAttribute(Qt::WA_AlwaysShowToolTips, true);

	setHasMaxResButton(true);
	setWindowTitle(QTStr("Bgm.Title"));

	connect(&timerLoading, &QTimer::timeout, [this]() {
		if (indexLoading != -1) {
			PLSBgmItemDelegate::nextLoadFrame();
			auto index = ui->playListWidget->model()->index(indexLoading, 0);
			auto data = ui->playListWidget->model()->data(index, (int)CustomDataRole::MediaStatusRole).value<MediaStatus>();
			ui->playListWidget->update(index);
			if (data == MediaStatus::stateLoading && ((os_gettime_ns() - loadingTimeout) / 1000000 >= LoadingTimeoutMS)) {
				PLSBgmItemData data_ = ui->playListWidget->Get(indexLoading);
				PLS_INFO(MAIN_BGM_MODULE, "Loading [%s] timeout.", data_.title.toStdString().c_str());
				OnLoadFailed(currentSourceName);
			}
		}
	});

	networkAvailable = pls_get_network_state();
	pls_network_state_monitor([this](bool accessible) { networkAvailable = accessible; });
}

void PLSBackgroundMusicView::clearUI(quint64 sceneItem)
{
	SetPlayerControllerStatus(sceneItem);
	SetCurrentPlayMode(PlayMode::InOrderMode);

	UpdatePlayListUI(sceneItem);
	ui->loadingBtn->setVisible(false); //for play btn
	loadingEvent.stopLoadingTimer();
}

void PLSBackgroundMusicView::Save() const
{
	const PLSBasic *main = PLSBasic::instance();
	if (main) {
		PLSBasic::Get()->SaveProject();
	}
}

void PLSBackgroundMusicView::SetLoop(const quint64 &sceneItem, bool checked)
{
	pls_flush_style(ui->loopBtn, STATUS_PRESSED, checked);

	OBSData settings = obs_data_create();
	obs_source_t *source = GetSource(sceneItem);
	if (!source) {
		return;
	}
	obs_data_set_string(settings, "method", "bgm_loop");
	obs_data_set_bool(settings, IS_LOOP, checked);
	pls_source_set_private_data(source, settings);
	obs_data_release(settings);
}

void PLSBackgroundMusicView::CreateCheckValidThread()
{
	if (!checkThreadObj) {
		checkThreadObj = pls_new<CheckValidThread>();
		connect(
			checkThreadObj, &CheckValidThread::checkFinished, this,
			[this](const PLSBgmItemData &data, bool result) {
				if (!result) {
					return;
				}
				ui->playListWidget->UpdataData(ui->playListWidget->GetRow(data), QVariant::fromValue(MediaStatus::stateNormal), CustomDataRole::MediaStatusRole);
				QVector<PLSBgmItemData> vecs;
				vecs.push_back(data);
				RefreshMulicEnabledPlayList(currentSceneItem, vecs);
			},
			Qt::QueuedConnection);
		checkThreadObj->moveToThread(&checkThread);
		checkThread.start();
	}
}

obs_source_t *PLSBackgroundMusicView::GetSource(const quint64 &sceneItem)
{
	PLSBasic *main = PLSBasic::instance();
	if (!main) {
		return nullptr;
	}

	return obs_sceneitem_get_source(pls_get_sceneitem_by_pointer_address(main->GetCurrentScene(), (void *)sceneItem));
}

bool PLSBackgroundMusicView::SameWithCurrentSource(const QString &sourceName)
{
	return pls_get_source_by_name(sourceName.toStdString().c_str()) == GetSource(currentSceneItem);
}

bool PLSBackgroundMusicView::SameWithCurrentSource(const quint64 &sceneItem)
{
	return GetSource(sceneItem) == GetSource(currentSceneItem);
}

bool PLSBackgroundMusicView::IsSameState(OBSSource source, obs_media_state state)
{
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "method", "get_select_url");
	pls_source_get_private_data(source, settings);

	const char *musicId = obs_data_get_string(settings, BGM_URL);
	const char *durationType = obs_data_get_string(settings, BGM_DURATION_TYPE);
	if (!musicId || !durationType) {
		return false;
	}

	PLSBgmItemData data = ui->playListWidget->GetCurrent();
	if ((0 == data.GetUrl(data.id).compare(musicId)) && atoi(durationType) == data.id && state == last_state) {
		return true;
	}
	return false;
}

QVector<PLSBgmItemData> PLSBackgroundMusicView::GetPlayListData(obs_data_t *settings) const
{
	QVector<PLSBgmItemData> datas{};
	if (!settings) {
		return datas;
	}

	obs_data_array_t *playListArray = obs_data_get_array(settings, PLAY_LIST);
	size_t item_count = obs_data_array_count(playListArray);
	if (0 == item_count) {
		return datas;
	}

	for (int i = 0; i < item_count; i++) {
		obs_data_t *playList = obs_data_array_item(playListArray, i);
		QString playlistType = obs_data_get_string(playList, BGM_DURATION_TYPE);

		PLSBgmItemData bgmData = GetPlayListDataBySettings(playList);
		datas.push_back(bgmData);
		obs_data_release(playList);
	}

	obs_data_array_release(playListArray);
	return datas;
}

PLSBgmItemData PLSBackgroundMusicView::GetPlayListDataBySettings(obs_data_t *settings) const
{
	if (!settings) {
		return PLSBgmItemData();
	}

	PLSBgmItemData bgmData;
	bgmData.isCurrent = obs_data_get_bool(settings, BGM_IS_CURRENT);
	bgmData.title = obs_data_get_string(settings, BGM_TITLE);
	bgmData.producer = obs_data_get_string(settings, BGM_PRODUCER);
	bgmData.id = atoi(obs_data_get_string(settings, BGM_DURATION_TYPE));
	bgmData.SetUrl(obs_data_get_string(settings, BGM_URL), bgmData.id);
	bgmData.SetDuration(bgmData.id, atoi(obs_data_get_string(settings, BGM_DURATION)));
	bgmData.isLocalFile = obs_data_get_bool(settings, BGM_IS_LOCAL_FILE);
	bgmData.isDisable = obs_data_get_bool(settings, BGM_IS_DISABLE);
	bgmData.haveCover = obs_data_get_bool(settings, BGM_HAVE_COVER);
	bgmData.coverPath = obs_data_get_string(settings, BGM_COVER_PATH);

	return bgmData;
}

PLSBgmItemData PLSBackgroundMusicView::GetCurrentPlayListData() const
{
	return ui->playListWidget->GetCurrent();
}

PLSBgmItemData PLSBackgroundMusicView::GetCurrentPlayListDataBySettings()
{
	obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		return PLSBgmItemData();
	}
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "method", "get_current_url");
	pls_source_get_private_data(source, settings);

	PLSBgmItemData bgmData = GetPlayListDataBySettings(settings);
	obs_data_release(settings);

	return bgmData;
}

PLSBgmItemData PLSBackgroundMusicView::GetCurrentPlayListDataBySettings(const QString &name) const
{
	OBSSource source = pls_get_source_by_name(name.toStdString().c_str());
	if (!source) {
		return PLSBgmItemData();
	}
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "method", "get_current_url");
	pls_source_get_private_data(source, settings);

	PLSBgmItemData bgmData = GetPlayListDataBySettings(settings);
	obs_data_release(settings);

	return bgmData;
}

int PLSBackgroundMusicView::GetCurrentPlayListDataSize() const
{
	return ui->playListWidget->Count();
}

int PLSBackgroundMusicView::GetCurrentBgmSourceSize() const
{
	auto main = static_cast<PLSBasic *>(App()->GetMainWindow());
	if (!main) {
		return 0;
	}
	BgmSourceVecType sourceList = main->GetCurrentSceneBgmSourceList();
	return int(sourceList.size());
}

QVector<PLSBgmItemData> PLSBackgroundMusicView::GetPlayListDatas() const
{
	return ui->playListWidget->GetData();
}

void PLSBackgroundMusicView::RefreshPropertyWindow()
{
	if (currentSourceName.isEmpty()) {
		return;
	}

	emit RefreshSourceProperty(currentSourceName, this->isVisible());
}

bool PLSBackgroundMusicView::CurrentPlayListBgmDataExisted(const QString &url) const
{
	return ui->playListWidget->Existed(url);
}

void PLSBackgroundMusicView::UpdateSourceSelectUI()
{
	PLSBasic *main = PLSBasic::instance();
	if (!main) {
		return;
	}
	QString name{};
	quint64 item{};
	bool selectBgm = false;
	selectBgm = main->GetSelectBgmSourceName(name, item);
	selectBgm = selectBgm && obs_sceneitem_visible(pls_get_sceneitem_by_pointer_address(main->GetCurrentScene(), (void *)item));

	BgmSourceVecType sourceList = main->GetCurrentSceneBgmSourceList();
	if (0 == sourceList.size()) {
		currentSceneItem = 0;
		currentSourceName = "";
		ui->stackedWidget->setCurrentWidget(ui->noSourcePage);
	} else if (selectBgm) {
		SetSourceSelect(name, item, true);
	} else {
		for (const auto &iter : sourceList) {
			const obs_sceneitem_t *sceneitem = pls_get_sceneitem_by_pointer_address(main->GetCurrentScene(), (void *)iter.second);
			if (!sceneitem) {
				continue;
			}
			bool visible = obs_sceneitem_visible(sceneitem);
			if (!visible) {
				continue;
			}
			SetSourceSelect(iter.first, iter.second, true);
			return;
		}
		currentSceneItem = 0;
		currentSourceName = "";
		ui->stackedWidget->setCurrentWidget(ui->noSourcePage);
	}
}

void PLSBackgroundMusicView::UpdatePlayListUI(const quint64 &sceneItem)
{
	if (!SameWithCurrentSource(sceneItem)) {
		return;
	}

	obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		ui->tabWidget->hide();
		ui->noPlayListFrame->show();
		ui->playListFrame->hide();
		ResetControlView();
		return;
	}

	OBSData settings = obs_data_create();
	pls_source_get_private_data(source, settings);
	obs_data_array_t *playListArray = obs_data_get_array(settings, PLAY_LIST);
	size_t item_count = obs_data_array_count(playListArray);
	if (0 == item_count) {
		ResetControlView();
		ui->noPlayListFrame->show();
		ui->playListFrame->hide();
	} else {
		ui->noPlayListFrame->hide();
		ui->playListFrame->show();
	}
	ui->tabWidget->show();
	obs_data_release(settings);
}

void PLSBackgroundMusicView::AddPlayListUI(const quint64 &sceneItem)
{
	obs_source_t *source = GetSource(sceneItem);
	if (!source) {
		return;
	}
	OBSData settings = obs_data_create();
	pls_source_get_private_data(source, settings);
	obs_data_release(settings);
	obs_data_array_t *playListArray = obs_data_get_array(settings, PLAY_LIST);
	size_t item_count = obs_data_array_count(playListArray);
	if (0 == item_count) {
		ui->playListWidget->Clear();
		ResetControlView();
		return;
	}

	QVector<PLSBgmItemData> datas;
	QVector<PLSBgmItemData> availableDatas;
	bool needUpdateDisableConfig = false;
	PLSBgmItemData currentBgmData;
	obs_media_state state = obs_source_media_get_state(source);
	for (int i = 0; i < item_count; i++) {
		obs_data_t *playList = obs_data_array_item(playListArray, i);
		PLSBgmItemData bgmData = GetPlayListDataBySettings(playList);
		if (bgmData.isCurrent && state != OBS_MEDIA_STATE_NONE) {
			bgmData.isCurrent = true;
			currentBgmData = bgmData;
		} else {
			bgmData.isCurrent = false;
		}
		if (bgmData.isDisable) {
			if (bgmData.isLocalFile) {
				CreateCheckValidThread();
				QMetaObject::invokeMethod(checkThreadObj, "CheckUrlAvailable", Qt::QueuedConnection, Q_ARG(PLSBgmItemData, bgmData));
			} else if (CheckNetwork()) {
				availableDatas.push_back(bgmData);
				bgmData.isDisable = false;
				needUpdateDisableConfig = true;
			}
		}
		datas.push_back(bgmData);
		obs_data_release(playList);
	}

	if (needUpdateDisableConfig) {
		RefreshMulicEnabledPlayList(sceneItem, availableDatas);
	}

	ui->playListWidget->UpdateWidget(datas);
	SetPlayListStatus(currentBgmData);

	int currentRow = ui->playListWidget->GetCurrentRow();
	if (!currentBgmData.isCurrent || -1 == currentRow) {
		StopLoadingTimer();
		return;
	}
	if (isVisible()) {
		ui->playListWidget->scrollTo(ui->playListWidget->GetModelIndex(currentRow));
	}
	indexLoading = currentRow;
	if (state == OBS_MEDIA_STATE_PLAYING || state == OBS_MEDIA_STATE_OPENING) {
		//if (obs_source_media_is_update_done(source)) {
		isLoading = false;
		PLSBgmItemDelegate::totalFrame(21);
		PLSBgmItemDelegate::setCurrentFrame(1);
		ui->playListWidget->SetMediaStatus(currentRow, MediaStatus::statePlaying);
		StartLoadingTimer();
		//} else {
		//isLoading = true;
		//PLSBgmItemDelegate::totalFrame(8);
		//PLSBgmItemDelegate::setCurrentFrame(1);
		//ui->playListWidget->SetMediaStatus(currentRow, MediaStatus::stateLoading);
		//StartLoadingTimer();
		//	}
	} else {
		StopLoadingTimer();
	}
	SetPlayerControllerStatus(sceneItem, true);
}

void PLSBackgroundMusicView::SetPlayerControllerStatus(const quint64 &sceneItem, bool listChanged)
{
	if (!SameWithCurrentSource(sceneItem)) {
		return;
	}

	obs_source_t *source = GetSource(sceneItem);
	if (!source) {
		ResetControlView();
		return;
	}

	bool visible = obs_sceneitem_visible(pls_get_sceneitem_by_pointer_address((void *)sceneItem));
	obs_media_state state = obs_source_media_get_state(source);
	if ((state == OBS_MEDIA_STATE_NONE || state == OBS_MEDIA_STATE_ENDED) || !visible) {
		ResetControlView();
		return;
	}

	PLSBgmItemData data = GetCurrentPlayListDataBySettings();
	if (data.title.isEmpty()) {
		ResetControlView();
		return;
	}

	if (state == OBS_MEDIA_STATE_PLAYING) {
		if (listChanged) {
			auto time = (float)obs_source_media_get_time(source);
			auto duration = (float)obs_source_media_get_duration(source);
			float sliderPosition = 0.0f;
			sliderPosition = (time / duration) * (float)ui->playingSlider->maximum();
			ui->currentTimeLabel->setText(PLSBgmDataViewManager::Instance()->ConvertIntToTimeString((int)(time / 1000.0f)));
			ui->durationLabel->setText(PLSBgmDataViewManager::Instance()->ConvertIntToTimeString((int)(duration / 1000.0f)));
			ui->playingSlider->setValue((int)(sliderPosition));
		}
		ui->playingSlider->setEnabled(true);
		StartSliderPlayingTimer();
		pls_flush_style(ui->playBtn, STATUS_STATE, STATUS_PAUSE);
		ui->playBtn->setToolTip(QTStr("Bgm.Pause"));
	} else if (state == OBS_MEDIA_STATE_OPENING) {
		ui->playingSlider->setEnabled(false);
		ui->currentTimeLabel->setText("00:00");
		ui->durationLabel->setText(PLSBgmDataViewManager::Instance()->ConvertIntToTimeString(data.GetDuration(data.id)));
		ui->playingSlider->setValue(0);
	} else if (state == OBS_MEDIA_STATE_PAUSED) {
		ui->playingSlider->setEnabled(true);
		StopSliderPlayingTimer();
		SetSliderPos();
		pls_flush_style(ui->playBtn, STATUS_STATE, STATUS_PLAY);
		ui->playBtn->setToolTip(QTStr("Bgm.Play"));
	}

	isLoading ? loadingEvent.startLoadingTimer(ui->loadingBtn) : loadingEvent.stopLoadingTimer();
	if (isLoading) {
		ui->playBtn->setVisible(!isLoading);
		ui->loadingBtn->setVisible(isLoading);
	} else {
		ui->loadingBtn->setVisible(isLoading);
		ui->playBtn->setVisible(!isLoading);
	}

	ui->coverLabel->SetMusicInfo(data.title, data.producer);
	DisablePlayerControlUI(false);
	ShowCoverGif(state == OBS_MEDIA_STATE_PLAYING);

	ui->playingSlider->setMinimum(0);
}

void PLSBackgroundMusicView::ResetControlView()
{
	ui->durationLabel->setText("00:00");
	ui->currentTimeLabel->setText("00:00");
	ui->playingSlider->setValue(0);
	pls_flush_style(ui->playingSlider, STATUS_ENTER, false);
	StopSliderPlayingTimer();
	StopLoadingTimer();
	ui->loadingBtn->setVisible(false);
	ui->playBtn->setVisible(true);
	pls_flush_style(ui->playBtn, STATUS_STATE, STATUS_PLAY);
	DisablePlayerControlUI(true);
}

void PLSBackgroundMusicView::DisablePlayerControlUI(bool disable)
{
	QString status = disable ? STATUS_DISABLE : STATUS_ENABLE;
	pls_flush_style(ui->refreshBtn, STATUS, status);
	pls_flush_style(ui->playBtn, STATUS, status);
	pls_flush_style(ui->preBtn, STATUS, status);
	pls_flush_style(ui->nextBtn, STATUS, status);
	pls_flush_style(ui->loopBtn, STATUS, status);
	ui->noSongFrame->setVisible(disable);
	ui->coverLabel->setVisible(!disable);
	if (disable)
		ui->coverLabel->SetCoverPath(DEFAULT_COVER_IMAGE, false);
	SetPlayerPaneEnabled(!disable);
}

void PLSBackgroundMusicView::PLSFrontendEvent(obs_frontend_event event, void *ptr)
{
	auto view = static_cast<PLSBackgroundMusicView *>(ptr);

	switch ((int)event) {
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
		QMetaObject::invokeMethod(view, "OnSceneChanged", Qt::QueuedConnection);
		break;
	default:
		break;
	}
}

int PLSBackgroundMusicView::GetPlayListItemIndexByKey(const QString &key) const
{
	for (int i = 0; i < ui->playListWidget->Count(); i++) {
		PLSBgmItemData data = ui->playListWidget->Get(i);
		QString targetKey = data.title.append(QString::number(data.GetDuration(data.id)));
		if (0 == key.compare(targetKey)) {
			return i;
		}
	}
	return -1;
}

void PLSBackgroundMusicView::SetUrlInfo(OBSSource source, const PLSBgmItemData &data) const
{
	if (!source) {
		return;
	}

	OBSData settings = obs_data_create();

	obs_data_set_string(settings, "method", "bgm_play");
	SetUrlInfoToSettings(settings, data);

	pls_source_set_private_data(source, settings);
	obs_data_release(settings);

	obs_source_media_restart(source);
}

void PLSBackgroundMusicView::SetUrlInfoToSettings(obs_data_t *settings, const PLSBgmItemData &data) const
{
	if (!settings) {
		return;
	}

	obs_data_set_string(settings, BGM_TITLE, data.title.toStdString().c_str());
	obs_data_set_string(settings, BGM_PRODUCER, data.producer.toStdString().c_str());
	obs_data_set_string(settings, BGM_URL, data.GetUrl(data.id).toStdString().c_str());
	obs_data_set_string(settings, BGM_DURATION_TYPE, QString::number(data.id).toStdString().c_str());
	obs_data_set_string(settings, BGM_DURATION, QString::number(data.GetDuration(data.id)).toStdString().c_str());
	obs_data_set_string(settings, BGM_GROUP, data.group.toStdString().c_str());
	obs_data_set_string(settings, BGM_COVER_PATH, data.coverPath.toStdString().c_str());
	obs_data_set_bool(settings, BGM_IS_LOCAL_FILE, data.isLocalFile);
	obs_data_set_bool(settings, BGM_HAVE_COVER, data.haveCover);
	obs_data_set_bool(settings, BGM_IS_CURRENT, data.isCurrent);
	obs_data_set_bool(settings, BGM_IS_DISABLE, data.isDisable);
}

QString PLSBackgroundMusicView::StrcatString(const QString &title, const QString &producer) const
{
	if (title.isEmpty() || producer.isEmpty()) {
		return "";
	}
	return title + QString("-") + producer;
}

bool PLSBackgroundMusicView::IsSourceAvailable(const QString &sourceName, quint64 sceneItem) const
{
	OBSSource source = pls_get_source_by_name(sourceName.toStdString().c_str());
	if (!source) {
		return false;
	}

	auto item = (obs_sceneitem_t *)sceneItem;
	return obs_sceneitem_visible(item);
}

void PLSBackgroundMusicView::StartSliderPlayingTimer()
{
	if (!sliderTimer) {
		return;
	}
	if (!sliderTimer->isActive()) {
		sliderTimer->start(1000);
	}
}

void PLSBackgroundMusicView::StopSliderPlayingTimer()
{
	if (!sliderTimer) {
		return;
	}
	if (sliderTimer->isActive()) {
		sliderTimer->stop();
	}
}

void PLSBackgroundMusicView::StartLoadingTimer(int timeOutMs)
{
	if (!timerLoading.isActive()) {
		timerLoading.start(timeOutMs);
		loadingTimeout = os_gettime_ns();
	}
}

void PLSBackgroundMusicView::StopLoadingTimer()
{
	isLoading = false;
	if (timerLoading.isActive()) {
		timerLoading.stop();
		indexLoading = -1;
		loadingTimeout = 0;
	}
}

bool PLSBackgroundMusicView::CheckNetwork() const
{
	return networkAvailable;
}

bool PLSBackgroundMusicView::CheckValidLocalAudioFile(const QString &url) const
{
	media_info_t media_info;
	memset(&media_info, 0, sizeof(media_info_t));

	bool open = mi_open(&media_info, url.toStdString().c_str(), static_cast<mi_open_mode>(MI_OPEN_DIRECTLY | MI_OPEN_TRY_DECODER));
	mi_free(&media_info);

	return open;
}

void PLSBackgroundMusicView::RefreshMulicEnabledPlayList(const quint64 &sceneItem, const QVector<PLSBgmItemData> &datas)
{
	obs_source_t *source = GetSource(sceneItem);
	if (!source) {
		return;
	}
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "method", "bgm_enable");

	obs_data_array_t *urlArray = obs_data_array_create();
	for (auto &data : datas) {
		obs_data_t *url = obs_data_create();
		obs_data_set_string(url, BGM_URL, data.GetUrl(data.id).toStdString().c_str());
		obs_data_set_string(url, BGM_DURATION_TYPE, QString::number(data.id).toStdString().c_str());
		obs_data_array_push_back(urlArray, url);
		obs_data_release(url);
	}
	obs_data_set_array(settings, BGM_URLS, urlArray);
	pls_source_set_private_data(source, settings);
	obs_data_array_release(urlArray);
	obs_data_release(settings);
}

void PLSBackgroundMusicView::UpdateCurrentPlayStatus(const QString &sourceName)
{
	if (sourceName != currentSourceName) {
		return;
	}

	OBSSource source = pls_get_source_by_name(currentSourceName.toStdString().c_str());
	if (!source) {
		return;
	}

	int currentRow = ui->playListWidget->GetCurrentRow();
	if (-1 == currentRow) {
		return;
	}

	obs_media_state state = obs_source_media_get_state(source);
	if (state == OBS_MEDIA_STATE_PLAYING) {
		indexLoading = currentRow;
		ui->playListWidget->SetMediaStatus(currentRow, isLoading ? MediaStatus::stateLoading : MediaStatus::statePlaying);
	} else if (state == OBS_MEDIA_STATE_OPENING) {
		indexLoading = currentRow;
		ui->playListWidget->SetMediaStatus(currentRow, MediaStatus::stateLoading);
	}
}

void PLSBackgroundMusicView::SetPlayListStatus(const PLSBgmItemData &)
{
	for (int i = 0; i < ui->playListWidget->Count(); i++) {
		PLSBgmItemData data_ = ui->playListWidget->Get(i);
		SetPlayListItemStatus(i, data_);
	}
}

void PLSBackgroundMusicView::SetPlayListItemStatus(const int &index, const PLSBgmItemData &data_)
{
	bool current = data_.isCurrent;
	OBSSource source = pls_get_source_by_name(currentSourceName.toStdString().c_str());
	if (!source) {
		ui->playListWidget->SetMediaStatus(index, MediaStatus::stateNormal);
		return;
	}

	if (data_.isDisable) {
		ui->playListWidget->SetMediaStatus(index, MediaStatus::stateInvalid);
		return;
	}

	if (current) {
		obs_media_state state = obs_source_media_get_state(source);
		if (state == OBS_MEDIA_STATE_PLAYING) {
			if (!isLoading) {
				ui->playListWidget->SetMediaStatus(index, MediaStatus::statePlaying);
			}
		} else if (state == OBS_MEDIA_STATE_OPENING) {
			if (!isLoading) {
				ui->playListWidget->SetMediaStatus(index, MediaStatus::stateLoading);
			}
		} else if (state == OBS_MEDIA_STATE_PAUSED) {
			ui->playListWidget->SetMediaStatus(index, MediaStatus::statePause);
		} else if (state == OBS_MEDIA_STATE_ERROR) {
			ui->playListWidget->SetMediaStatus(index, MediaStatus::stateInvalid);
		} else {
			data_.isDisable ? ui->playListWidget->SetMediaStatus(index, MediaStatus::stateInvalid) : ui->playListWidget->SetMediaStatus(index, MediaStatus::stateNormal);
		}
		return;
	}
	ui->playListWidget->SetMediaStatus(index, MediaStatus::stateNormal);
}

void PLSBackgroundMusicView::SetPlayerPaneEnabled(bool enabled)
{
	ui->loopBtn->setEnabled(enabled);
	ui->preBtn->setEnabled(enabled);
	ui->playBtn->setEnabled(enabled);
	ui->loadingBtn->setEnabled(enabled);
	ui->nextBtn->setEnabled(enabled);
	ui->refreshBtn->setEnabled(enabled);
	ui->currentTimeLabel->setEnabled(enabled);
	ui->durationLabel->setEnabled(enabled);

	if (!enabled) {
		ui->playingSlider->setEnabled(enabled);
	}
}

void PLSBackgroundMusicView::CreateLibraryView()
{
	if (!libraryView) {
		libraryView = pls_new<PLSBgmLibraryView>(this);
		connect(libraryView, &PLSBgmLibraryView::AddCachePlayList, this, &PLSBackgroundMusicView::OnAddCachePlayList);
		libraryView->setModal(true);
	}
}

void PLSBackgroundMusicView::InitToast()
{
	toastView.setParent(this);
	toastView.hide();
}

void PLSBackgroundMusicView::ShowToastView(const QString &text)
{
	toastView.SetMessage(text);
	ResizeToastView();
	toastView.ShowToast();
}

void PLSBackgroundMusicView::ResizeToastView()
{
	toastView.SetShowWidth(this->width() - 2 * 10);
	toastView.move(10, 460);
}

QImage PLSBackgroundMusicView::GetCoverImage(const QString &url) const
{
	QImage image{};
	media_info_t media_info;
	memset(&media_info, 0, sizeof(media_info_t));
	bool open = mi_open(&media_info, url.toStdString().c_str(), MI_OPEN_DIRECTLY);
	if (!open) {
		return image;
	}
	auto cover = (mi_cover_t *)mi_get_obj(&media_info, "cover_obj");
	if (cover) {
		image = CaptureImage(cover->width, cover->height, cover->data, cover->size, 0);
	}
	mi_free(&media_info);
	return image;
}

void PLSBackgroundMusicView::SetCoverImage(const PLSBgmItemData &data)
{
	QMetaObject::invokeMethod(coverThreadObj, "GetCoverImage", Qt::QueuedConnection, Q_ARG(const PLSBgmItemData &, data));
}

void PLSBackgroundMusicView::ShowCoverImage(const PLSBgmItemData &data)
{
	if (data.title.isEmpty()) {
		return;
	}
	QString coverPath;
	if (!data.coverPath.isEmpty()) {
		coverPath = data.coverPath;
		QFileInfo fileinfo(coverPath);
		if (!fileinfo.exists() && !data.haveCover) {
			coverPath = DEFAULT_COVER_IMAGE;
			ui->coverLabel->SetCoverPath(coverPath, !data.isLocalFile);
			return;
		}

		if (!data.haveCover) {
			ui->coverLabel->SetCoverPath(coverPath, !data.isLocalFile);
			return;
		}
		if (fileinfo.exists()) {
			ui->coverLabel->SetCoverPath(coverPath, !data.isLocalFile);
			return;
		}

		SetCoverImage(data);
		return;
	} else {
		if (data.haveCover) {
			SetCoverImage(data);
			return;
		}
		coverPath = DEFAULT_COVER_IMAGE;
	}

	if (0 != data.coverPath.compare(coverPath)) {
		PLSBgmItemData data_ = data;
		data_.coverPath = coverPath;
		ui->coverLabel->SetCoverPath(data_.coverPath, !data.isLocalFile);
		UpdateBgmCoverPath(data_);
		return;
	}
}

void PLSBackgroundMusicView::ShowCoverGif(bool show)
{
	ui->coverLabel->ShowPlayingGif(show);
}

void PLSBackgroundMusicView::UpdateBgmCoverPath(const PLSBgmItemData &data)
{
	obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		return;
	}

	ui->playListWidget->UpdataData(ui->playListWidget->GetRow(data), data.coverPath, CustomDataRole::CoverPathRole);
	obs_data_t *settings = obs_data_create();
	pls_source_get_private_data(source, settings);
	QVector<PLSBgmItemData> datas = GetPlayListData(settings);
	for (auto const &data_ : datas) {
		if (data_.id == data.id && data_.GetUrl(data_.id) == data.GetUrl(data.id)) {
			QFileInfo fileinfo(data_.coverPath);
			bool existed = fileinfo.exists();
			if (data_.coverPath.isEmpty() || !existed) {
				OBSData settings_ = obs_data_create();
				obs_data_set_string(settings_, "method", "bgm_update_cover_path");
				obs_data_set_string(settings_, BGM_URL, data.GetUrl(data.id).toStdString().c_str());
				obs_data_set_string(settings_, BGM_DURATION_TYPE, QString::number(data.id).toStdString().c_str());
				obs_data_set_string(settings_, BGM_COVER_PATH, data.coverPath.toStdString().c_str());
				pls_source_set_private_data(source, settings_);
				obs_data_release(settings_);
			}
			break;
		}
	}
	obs_data_release(settings);
}

void CheckValidThread::CheckUrlAvailable(const PLSBgmItemData &data)
{
	media_info_t media_info;
	memset(&media_info, 0, sizeof(media_info_t));

	bool open = mi_open(&media_info, data.GetUrl(data.id).toStdString().c_str(), static_cast<mi_open_mode>(MI_OPEN_DIRECTLY | MI_OPEN_TRY_DECODER));
	mi_free(&media_info);

	emit checkFinished(data, open);
}

void PLSBackgroundMusicView::SeekTo(int val)
{
	OBSSource source = pls_get_source_by_name(currentSourceName.toStdString().c_str());
	if (!source)
		return;

	float percent = (float)val / float(ui->playingSlider->maximum());
	int64_t duration = obs_source_media_get_duration(source);

	if (duration > 0) {
		auto seekTo = static_cast<int64_t>(percent * static_cast<float>(duration));
		obs_source_media_set_time(source, seekTo);
	}
}

int64_t PLSBackgroundMusicView::GetSliderTime(int val)
{
	OBSSource source = pls_get_source_by_name(currentSourceName.toStdString().c_str());
	if (!source)
		return 0;

	float percent = (float)val / (float)ui->playingSlider->maximum();
	float duration = (float)obs_source_media_get_duration(source);
	int64_t seekTo = (int64_t)(percent * duration);

	return seekTo;
}

void GetCoverThread::SaveCoverToLocalPath(const PLSBgmItemData &data_, const QImage &image)
{
	PLSBgmItemData data = data_;
	data.coverPath = GetTempImageFilePath(".png", 0);
	image.save(data.coverPath);
	emit Finished(data);
}

void GetCoverThread::GetCoverImage(const PLSBgmItemData &data)
{
	if (taskQueue.isEmpty())
		QTimer::singleShot(0, this, SLOT(NextTask()));
	taskQueue.enqueue(data);
}

void GetCoverThread::PrintThumbInfo(const QString &url, media_info_t *mi) const
{
	auto width = mi_get_int(mi, "width");
	auto height = mi_get_int(mi, "height");
	auto vformat = mi_get_int(mi, "video_format");

	if (width > 0 && height > 0) {
		auto file_name = pls_get_path_file_name(url);
		PLS_INFO(MAIN_BGM_MODULE, "bgm thumbnail info. %lldx%lld, vformat:%lld, file:'%s'", width, height, vformat, file_name.toStdString().c_str());
	}
}

void GetCoverThread::NextTask()
{
	if (taskQueue.isEmpty())
		return;

	auto data = taskQueue.dequeue();

	QImage image{};
	QString url = data.GetUrl(data.id);
	if (url.isEmpty()) {
		emit GetPreviewImage(image, data);
		NextTask();
		return;
	}
	media_info_t media_info;
	memset(&media_info, 0, sizeof(media_info_t));
	bool open = mi_open(&media_info, url.toStdString().c_str(), MI_OPEN_DIRECTLY);
	if (!open) {
		emit GetPreviewImage(image, data);
		NextTask();
		return;
	}

	PrintThumbInfo(url, &media_info);

	auto cover = (mi_cover_t *)mi_get_obj(&media_info, "cover_obj");
	if (cover) {
		image = CaptureImage(cover->width, cover->height, cover->data, cover->size, 0);
	}
	mi_free(&media_info);
	bool widthLonger = (image.width() > image.height());
	if (image.width() > COVER_WIDTH * 3 || image.height() > COVER_WIDTH * 3) {
		image = widthLonger ? image.scaledToWidth(COVER_WIDTH * 3, Qt::SmoothTransformation) : image.scaledToHeight(COVER_WIDTH * 3, Qt::SmoothTransformation);
		emit GetPreviewImage(image, data);
	} else {
		emit GetPreviewImage(image, data);
	}
	// save image to local
	PLSBgmItemData temp = data;
	temp.coverPath = GetTempImageFilePath(".png", 0);
	image.save(temp.coverPath);
	emit Finished(temp);
	NextTask();
}

void SendThread::SendMusicMetaData(const PLSBgmItemData &data) const
{
	if (data.GetUrl(data.id).isEmpty()) {
		return;
	}

	media_info_t media_info;
	memset(&media_info, 0, sizeof(media_info_t));
	bool open = mi_open(&media_info, data.GetUrl(data.id).toStdString().c_str(), MI_OPEN_DEFER);
	if (!open) {
		return;
	}

	auto id3 = (mi_id3v2_t *)mi_get_obj(&media_info, "id3v2_obj");
	if (id3) {
		//todo mi_send_id3v2(id3);
	}
	mi_free(&media_info);
}

void DragInFrame::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls()) {
		if (PLSBgmDataViewManager::Instance()->IsSupportFormat(event->mimeData()->urls())) {
			event->acceptProposedAction();
			return;
		}
		event->ignore();
	}
}

void DragInFrame::dropEvent(QDropEvent *event)
{
	QFrame::dropEvent(event);

	if (event->mimeData()->hasUrls()) {
		QStringList paths;
		QList<QUrl> urls = event->mimeData()->urls();
		for (const auto &item : urls) {
			QString file = item.toLocalFile();
			QFileInfo fileInfo(file);
			if (!fileInfo.exists())
				continue;

			if (!PLSBgmDataViewManager::Instance()->IsSupportFormat(file)) {
				continue;
			}

			paths << file;
		}
		emit AudioFileDraggedIn(paths);
	}
}

bool DragLabel::eventFilter(QObject *object, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonPress) {
		auto mouseEvent = static_cast<QMouseEvent *>(event);
		if (mouseEvent->button() == Qt::LeftButton) {
			startPoint = mouseEvent->pos();
			mousePressed = true;
			return true;
		}
	} else if (event->type() == QEvent::MouseMove) {
		auto mouseEvent = static_cast<QMouseEvent *>(event);
		if (mousePressed) {
			emit CoverPressed(mouseEvent->pos() - startPoint);
			return true;
		}
	} else if (event->type() == QEvent::MouseButtonRelease) {
		mousePressed = false;
		return true;
	}

	return QLabel::eventFilter(object, event);
}
