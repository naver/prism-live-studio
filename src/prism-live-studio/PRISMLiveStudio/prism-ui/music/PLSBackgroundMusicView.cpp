#include "PLSBackgroundMusicView.h"
#include "PLSBgmItemCoverView.h"
#include "PLSBgmItemView.h"
#include "PLSBgmLibraryView.h"
#include "PLSBgmDataManager.h"
#include "ui_PLSBackgroundMusicView.h"

#include "action.h"
#include "frontend-api.h"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSMainView.hpp"
#include "obs-app.hpp"
#include "pls-common-define.hpp"
#include "PLSPlatformApi.h"
#include "pls/pls-source.h"
#include "PLSPushButton.h"
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
#include <QWindow>
#include <QMetaEnum>
#include <ctime>
#include <sstream>
#include "libui.h"

using namespace common;

extern std::vector<QString> musicFormat;

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

PLSBackgroundMusicView::PLSBackgroundMusicView(QWidget *parent) : QWidget(parent)
{
	ui = pls_new<Ui::PLSBackgroundMusicView>();
	ui->setupUi(this);
	pls_add_css(this, {"PLSBackgroundMusicView", "PLSToastMsgFrame"});
	qRegisterMetaType<obs_media_state>("obs_media_state");

	initUI();
	pls_uistep_v2_set_name(ui->loopBtn, "Loop Button");
	pls_uistep_v2_set_name(ui->refreshBtn, "Mode Button");

	sliderTimer = pls_new<QTimer>(this);
	connect(sliderTimer, &QTimer::timeout, this, &PLSBackgroundMusicView::SetSliderPos, Qt::QueuedConnection);
	connect(&seekTimer, &QTimer::timeout, this, &PLSBackgroundMusicView::SeekTimerCallback);
	connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, &PLSBackgroundMusicView::OnCurrentPageChanged);
	connect(ui->playBtn, &QPushButton::clicked, this, &PLSBackgroundMusicView::OnPlayButtonClicked);
	connect(ui->preBtn, &PLSDelayResponseButton::buttonClicked, this, &PLSBackgroundMusicView::OnPreButtonClicked);
	connect(ui->nextBtn, &PLSDelayResponseButton::buttonClicked, this, &PLSBackgroundMusicView::OnNextButtonClicked);
	connect(ui->refreshBtn, &QPushButton::clicked, this, &PLSBackgroundMusicView::OnRefreshButtonClicked);

	connect(ui->loopBtn, &QRadioButton::clicked, this, [this](bool) { OnLoopBtnClicked(currentSceneItem); });
	connect(ui->addSourceBtn, &QPushButton::clicked, this, &PLSBackgroundMusicView::OnAddSourceBtnClicked);
	connect(ui->addMusicBtn, &QPushButton::clicked, this, &PLSBackgroundMusicView::OnAddMusicBtnClicked);
	connect(ui->coverLabel, &PLSBgmItemCoverView::CoverPressed, this, [this](const QPoint &point) { move(this->frameGeometry().topLeft() + point); });
	connect(ui->playListWidget, &PLSBgmDragView::MousePressedSignal, this, &PLSBackgroundMusicView::OnPlayListItemPressed);
	connect(ui->playListWidget, &PLSBgmDragView::RowChanged, this, &PLSBackgroundMusicView::OnPlayListItemRowChanged);
	connect(ui->playListWidget, &PLSBgmDragView::AudioFileDraggedIn, this, &PLSBackgroundMusicView::OnAudioFileDraggedIn);
	connect(ui->playListWidget, &PLSBgmDragView::DelButtonClickedSignal, this, &PLSBackgroundMusicView::OnDelButtonClicked);
	connect(ui->playListWidget, &PLSBgmDragView::LoadingFailed, this, &PLSBackgroundMusicView::OnLoadFailed);
	connect(ui->noPlayListFrame, &DragInFrame::AudioFileDraggedIn, this, &PLSBackgroundMusicView::OnAudioFileDraggedIn);

	connect(ui->playingSlider, SIGNAL(sliderPressed()), this, SLOT(SliderClicked()));
	connect(ui->playingSlider, SIGNAL(sliderReleased()), this, SLOT(SliderReleased()));
	connect(ui->playingSlider, SIGNAL(sliderMoved(int)), this, SLOT(SliderMoved(int)));

	connect(
		this->window()->windowHandle(), &QWindow::screenChanged, this,
		[this](QScreen *) {
			pls_check_app_exiting();

			// force resize
			this->resize(this->width() - 1, this->height() - 1);
			this->resize(this->width() + 1, this->height() + 1);
		},
		Qt::QueuedConnection);

	obs_frontend_add_event_callback(PLSFrontendEvent, this);

	pls_network_state_monitor([pthis = QPointer<PLSBackgroundMusicView>(this)](bool available) {
		if (!pls_object_is_valid(pthis)) {
			return;
		}
		pthis->onNetworkChanged(available);
	});
}

PLSBackgroundMusicView::~PLSBackgroundMusicView()
{
	obs_frontend_remove_event_callback(PLSFrontendEvent, this);

	PLSBgmDataViewManager::Instance()->DeleteGroupButton();
	PLSBgmDataViewManager::Instance()->ClearCachePlayList();

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
	UpdateSourceSelectUI();
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
	OBSSource source = pls_get_source_by_name(sourceName.toUtf8().constData());
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
	OBSSource source = pls_get_source_by_name(currentSourceName.toUtf8().constData());
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
	OBSSource source = pls_get_source_by_name(currentSourceName.toUtf8().constData());
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
	OBSSource source = pls_get_source_by_name(currentSourceName.toUtf8().constData());
	if (!source) {
		return;
	}

	obs_media_state state = obs_source_media_get_state(source);
	bool visible = isSceneitemVisible(currentSceneItem);
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
		OBSSource source = pls_get_source_by_name(currentSourceName.toUtf8().constData());
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

	LoopMode loopMode = LoopMode::LoopAll;
	if (!createNew) {
		if (auto isLoop = obs_data_get_bool(settings, IS_LOOP); isLoop) {
			if (auto loopModeStr = obs_data_get_string(settings, "loopMode"); !pls_is_empty(loopModeStr)) {
				loopMode = static_cast<LoopMode>(QMetaEnum::fromType<LoopMode>().keyToValue(loopModeStr));
			}
		} else {
			loopMode = LoopMode::NoLoop;
		}
	}

	SetLoop(sceneItem, loopMode);
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

		obs_source_t *source = pls_get_source_by_name(sourceName.toUtf8().constData());
		obs_media_state state = obs_source_media_get_state(source);
		if (state == OBS_MEDIA_STATE_PLAYING) {
			indexLoading = row;
			PLSBgmItemDelegate::totalFrame(21);
			PLSBgmItemDelegate::setCurrentFrame(1);
			ui->playListWidget->SetMediaStatus(row, isSceneitemVisible(currentSceneItem) ? MediaStatus::statePlaying : MediaStatus::stateCurrentInvisible);
			StartLoadingTimer(100);
		} else if (state == OBS_MEDIA_STATE_PAUSED) {
			ui->playListWidget->SetMediaStatus(row, isSceneitemVisible(currentSceneItem) ? MediaStatus::statePause : MediaStatus::stateCurrentInvisible);
		}
	}
	SetPlayerControllerStatus(currentSceneItem);
}

void PLSBackgroundMusicView::OnMediaStateChanged(const QString &name, obs_media_state state)
{
	OBSSource source = pls_get_source_by_name(name.toUtf8().constData());
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

void PLSBackgroundMusicView::OnLoopStateChanged(const QString &name)
{
	OBSSource source = pls_get_source_by_name(name.toUtf8().constData());
	if (!source) {
		return;
	}

	if (!SameWithCurrentSource(name)) {
		return;
	}

	OBSDataAutoRelease settings = obs_source_get_private_settings(source);
	auto loopStr = obs_data_get_string(settings, "loopMode");
	auto loop = static_cast<LoopMode>(QMetaEnum::fromType<LoopMode>().keyToValue(loopStr));
	if (m_loopMode == loop) {
		return;
	}
	m_loopMode = loop;
	SetLoop(source, loop);
}

void PLSBackgroundMusicView::OnModeStateChanged(const QString &name)
{
	OBSSource source = pls_get_source_by_name(name.toUtf8().constData());
	if (!source) {
		return;
	}

	if (!SameWithCurrentSource(name)) {
		return;
	}

	OBSDataAutoRelease settings = obs_source_get_private_settings(source);
	bool is_play_in_order = obs_data_get_bool(settings, PLAY_IN_ORDER);
	if (is_play_in_order) {
		this->mode = PlayMode::InOrderMode;
		ui->refreshBtn->setToolTip(QTStr("Bgm.PlayInOrder"));
		pls_flush_style(ui->refreshBtn, "playMode", "inOrder");
	} else {
		this->mode = PlayMode::RandomMode;
		ui->refreshBtn->setToolTip(QTStr("Bgm.Shuffle"));
		pls_flush_style(ui->refreshBtn, "playMode", "random");
	}
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
		ui->playListWidget->SetMediaStatus(row, isSceneitemVisible(currentSceneItem) ? MediaStatus::statePlaying : MediaStatus::stateCurrentInvisible);
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
		ui->playListWidget->SetMediaStatus(row, isSceneitemVisible(currentSceneItem) ? MediaStatus::statePause : MediaStatus::stateCurrentInvisible);
	}
	SetPlayerControllerStatus(currentSceneItem);
	ShowCoverGif(false);
}

void PLSBackgroundMusicView::UpdateStopUIState(const QString &name)
{
	if (!SameWithCurrentSource(name)) {
		OBSSource source = pls_get_source_by_name(name.toUtf8().constData());
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

	setCurrentRow(pls_get_source_by_name(name.toUtf8().constData()));
	SetPlayerControllerStatus(currentSceneItem);
	ShowCoverGif(false);
}

void PLSBackgroundMusicView::UpdateOpeningUIState(const QString &name)
{
	OnUpdateOpeningUIState(name);
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
	OBSSource source = pls_get_source_by_name(name.toUtf8().constData());
	if (source) {
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "method", "bgm_get_opening");
		pls_source_set_private_data(source, settings);
		obs_data_release(settings);
	}

	if (!SameWithCurrentSource(name)) {
		return;
	}

	createGetCoverThread();

	if (IsSameState(source, OBS_MEDIA_STATE_PLAYING) || IsSameState(source, OBS_MEDIA_STATE_PAUSED)) {
		return;
	}

	int row = ui->playListWidget->GetCurrentRow();
	if (-1 != row) {
		ui->playListWidget->SetMediaStatus(row, MediaStatus::stateNormal);
	}

	PLSBgmItemData data = GetCurrentPlayListDataBySettings();
	if (!data.title.isEmpty()) {
		ui->playingSlider->setEnabled(false);
		ui->playingSlider->setValue(0);
		ui->currentTimeLabel->setText("00:00");
		ui->coverLabel->SetMusicInfo(data.title, data.producer);
		ui->playListWidget->SetCurrentRow(data);

		ShowCoverGif(false);
		ShowCoverImage(data);
		SetPlayerControllerStatus(currentSceneItem);
	}
}

void PLSBackgroundMusicView::UpdateStatuPlayling(const QString &name)
{
	UpdatePlayingUIState(name);
}

int PLSBackgroundMusicView::GetDelayResponseIntervalMs()
{
	// for test
	if (!config_has_user_value(App()->GetUserConfig(), "General", "DelayIntervalMs")) {
		return PUSHBUTTON_DELAY_RESPONSE_MS;
	}

	return config_get_int(App()->GetUserConfig(), "General", "DelayIntervalMs");
}

void PLSBackgroundMusicView::onNetworkChanged(bool available)
{
	networkAvailable = available;
	OnRetryNetwork();
	UpdateCurrentPlayStatus(currentSourceName);
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

	OBSSource source = pls_get_source_by_name(currentSourceName.toUtf8().constData());
	if (!source) {
		return;
	}
	OBSData settings = obs_data_create();
	obs_data_set_string(settings, "method", "bgm_play_mode");
	obs_data_set_bool(settings, PLAY_IN_ORDER, mode == PlayMode::InOrderMode);
	obs_data_set_bool(settings, RANDOM_PLAY, mode == PlayMode::RandomMode);
	pls_source_set_private_data(source, settings);
	obs_data_release(settings);
	PLS_UI_ACTION("In Background Music, the play mode has been changed: %s.", QMetaEnum::fromType<PlayMode>().valueToKey(static_cast<int>(mode)));
}

void PLSBackgroundMusicView::SetSourceSelect(const QString &sourceName, quint64 sceneItem, bool selectd)
{
	PLSBasic *main = PLSBasic::instance();
	if (!main) {
		return;
	}
	obs_sceneitem_t *sceneitem = pls_get_sceneitem_by_pointer_address(main->GetCurrentScene(), (void *)sceneItem);
	if (!sceneitem) {
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

	if (selectd && !obs_sceneitem_visible(sceneitem)) {
		checkSceneitemEnableToastDisplayed(sceneitem);
	}

	UpdatePlayListUI(sceneItem);
	SetPlayerControllerStatus(sceneItem);
	PLSBgmItemData data = GetCurrentPlayListDataBySettings(sourceName);
	ShowCoverImage(data);
	SetPlayListStatus(data);
}

void PLSBackgroundMusicView::SetSourceVisible(const QString &sourceName, quint64 sceneitem, bool visible)
{
	UpdateSourceSelectUI();
	UpdateCurrentPlayStatus(currentSourceName);
	if (sceneitem == currentSceneItem && !visible) {
		checkSceneitemEnableToastDisplayed(sceneitem, true);
	}
	OBSSource source = pls_get_source_by_name(sourceName.toUtf8().constData());
	if (!source) {
		return;
	}
	pls_on_source_property_changed(source, "loop");
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "method", "bgm_visible");
	obs_data_set_bool(data, "visible", visible);
	pls_source_set_private_data(source, data);
}

void PLSBackgroundMusicView::UpdateSourceList(const QString &sourceName, quint64 sceneItem, const BgmSourceVecType &sourceList)
{
	for (auto &bgm : sourceList) {
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
	QWidget::showEvent(event);
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::BgmConfig, true);
	// Real show logic runs in OnDockReallyShown() when the dock is truly shown (connected from PLSBasic).
}

void PLSBackgroundMusicView::OnDockReallyShown()
{
	OnRetryNetwork();
	UpdateSourceSelectUI();
	UpdateCurrentPlayStatus(currentSourceName);
	if (currentSceneItem && !isSceneitemVisible(currentSceneItem)) {
		pls_async_call(this, [this]() { checkSceneitemEnableToastDisplayed(currentSceneItem, true); });
	}
}

void PLSBackgroundMusicView::hideEvent(QHideEvent *event)
{
	QWidget::hideEvent(event);
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::BgmConfig, false);
}

void PLSBackgroundMusicView::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if (toastView.isVisible()) {
		pls_async_call(this, [this]() { ResizeToastView(); });
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
	obs_data_set_string(delData, "remove_url", data.GetUrl(data.id).toUtf8().constData());
	obs_data_set_string(delData, BGM_DURATION_TYPE, QString::number(data.id).toUtf8().constData());
	pls_source_set_private_data(source, delData);
	obs_data_release(delData);

	UpdateCurrentPlayStatus(currentSourceName);
	UpdatePlayListUI(currentSceneItem);
	Save();
	RefreshPropertyWindow();
	PLS_UI_ACTION("In Music Playlist, the music play list has been changed when deleted.");
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
		data.isLocalFile ? ShowToastView(QTStr("Bgm.Songs.Invalid.Toast").arg(data.title)) : ShowToastView(toast);
		StopSliderPlayingTimer();
		StopLoadingTimer();
		OnInvalidSongs(currentSourceName, true);
	} else {
		setCurrentRow(source);
	}
}

void PLSBackgroundMusicView::OnInvalidSongs(const QString &name, bool gotoNext) const
{
	OBSSource source = pls_get_source_by_name(name.toUtf8().constData());
	if (!source) {
		return;
	}

	PLSBgmItemData data = GetCurrentPlayListDataBySettings(name);
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "method", "bgm_disable");
	obs_data_set_string(settings, BGM_URL, data.GetUrl(data.id).toUtf8().constData());
	obs_data_set_string(settings, BGM_DURATION_TYPE, QString::number(data.id).toUtf8().constData());
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

void PLSBackgroundMusicView::OnPlayListItemPressed(const QModelIndex &index)
{
	PLS_INFO(MAIN_BGM_MODULE, "music play list clicked.");

	PLSBgmItemData data = ui->playListWidget->GetData(index);
	obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		return;
	}
	pls_on_source_property_changed(source, "play");

	if (data.isDisable) {
		return;
	}

	if (!isSceneitemVisible(currentSceneItem)) {
		setCurrentRow(source, data);
		return;
	}

	PLS_INFO(MAIN_BGM_MODULE, "switch to next music: %s", data.title.toUtf8().constData());
	pls_uistep_v2(this, "Double click", "Button", data.title);
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

	PLSBgmItemData data = GetCurrentPlayListData();
	ui->playListWidget->UpdateWidget(datas);
	ui->playListWidget->SetCurrentRow(data);

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
	PLS_INFO(MAIN_BGM_MODULE, QString("Add %1 Local Songs").arg(paths.size()).toUtf8().constData());

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
		bool open = mi_open(&media_info, path.toUtf8().constData(), MI_OPEN_DIRECTLY);
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

	if (!datas.isEmpty()) {
		auto data = GetCurrentPlayListDataBySettings();
		if (data.title.isEmpty()) {
			setCurrentRow(source, datas.first());
		} else {
			SetPlayListStatus(data);
		}
	}
	UpdateCurrentPlayStatus(currentSourceName);
	UpdatePlayListUI(currentSceneItem);
	Save();
	RefreshPropertyWindow();
}

void PLSBackgroundMusicView::OnPlayButtonClicked() const
{
	OBSSource source = pls_get_source_by_name(currentSourceName.toUtf8().constData());
	if (!source) {
		return;
	}
	obs_media_state state = obs_source_media_get_state(source);
	switch (state) {
	case OBS_MEDIA_STATE_STOPPED:
	case OBS_MEDIA_STATE_ENDED:
	case OBS_MEDIA_STATE_NONE:
		pls_on_source_property_changed(source, "play");
		obs_source_media_restart(source);
		break;
	case OBS_MEDIA_STATE_PLAYING:
		pls_on_source_property_changed(source, "pause");
		obs_source_media_play_pause(source, true);
		break;
	case OBS_MEDIA_STATE_PAUSED:
		pls_on_source_property_changed(source, "play");
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
	pls_on_source_property_changed(source, "previous");
	obs_source_media_previous(source);
}

void PLSBackgroundMusicView::OnNextButtonClicked()
{
	obs_source_t *source = GetSource(currentSceneItem);
	if (!source) {
		return;
	}
	pls_on_source_property_changed(source, "next");
	obs_source_media_next(source);
}

void PLSBackgroundMusicView::OnLoopBtnClicked(const quint64 &sceneItem)
{
	LoopMode mode;
	if (m_loopMode == LoopMode::LoopAll) {
		mode = LoopMode::LoopOne;
	} else if (m_loopMode == LoopMode::LoopOne) {
		mode = LoopMode::NoLoop;
	} else {
		mode = LoopMode::LoopAll;
	}
	SetLoop(sceneItem, mode);
}

void PLSBackgroundMusicView::OnLocalFileBtnClicked()
{
	QString filter("Audio Files (");
	for (auto iter = musicFormat.begin(); iter != musicFormat.end(); ++iter) {
		filter.append(*iter).append(" ");
	}
	filter.replace(filter.lastIndexOf(" "), strlen(" "), ")");
	QStringList paths;
	pls::HotKeyLocker locker;
	if (localFilePath.isEmpty()) {
		localFilePath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
		paths = QFileDialog::getOpenFileNames(this, QTStr("Bgm.Free.Music.Local.File"), localFilePath, filter);
	} else {
		paths = QFileDialog::getOpenFileNames(this, QTStr("Bgm.Free.Music.Local.File"), "", filter);
	}

	OnAudioFileDraggedIn(paths);
}

void PLSBackgroundMusicView::OnLibraryBtnClicked()
{
	PLS_PERFORMANCE_GLOBAL_START("showsPLSBgmLibraryView");
	PLS_PERFORMANCE_GLOBAL_START("Bulid PLSBgmLibraryView", "showsPLSBgmLibraryView");
	auto libraryView_ = pls_new<PLSBgmLibraryView>(this);
	PLS_PERFORMANCE_GLOBAL_END("Bulid PLSBgmLibraryView");

#if defined(Q_OS_MACOS)
	libraryView_->initSize(720, 650 - PLS_TITLE_BAR_HEIGHT);
#elif defined(Q_OS_WIN)
	libraryView_->initSize(720, 650);
#endif
	libraryView_->setAttribute(Qt::WA_DeleteOnClose);
	connect(libraryView_, &PLSBgmLibraryView::AddCachePlayList, this, &PLSBackgroundMusicView::OnAddCachePlayList);
	PLS_PERFORMANCE_GLOBAL_START("PLSBgmLibraryView Exec", "showsPLSBgmLibraryView");
	PLS_PERFORMANCE_GLOBAL_END_WHEN_WIDGET_SHOW(libraryView_, PLS_PERFORMANCE_GLOBAL_END("PLSBgmLibraryView Exec"); PLS_PERFORMANCE_GLOBAL_END("showsPLSBgmLibraryView"));
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

	PLS_UI_ACTION("In Background Music, the menu has been displayed when clicked add music button.");

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
	SetCurrentPlayMode(PlayMode::InOrderMode);
	ResetControlView();
	InitToast();
	pls_flush_style(ui->playBtn, STATUS_STATE, STATUS_PLAY);
	ui->playBtn->setToolTip(QTStr("Bgm.Play"));
	pls_uistep_v2_set_custom_enter_leave_name(ui->playBtn, "Play/Pause Button");
	pls_uistep_v2_set_custom_enter_leave_name(ui->loopBtn, "Loop Mode Button");
	pls_uistep_v2_set_custom_enter_leave_name(ui->preBtn, "Pre Button");
	pls_uistep_v2_set_custom_enter_leave_name(ui->nextBtn, "Next Button");
	pls_uistep_v2_set_custom_enter_leave_name(ui->refreshBtn, "Play Mode Button");

	ui->preBtn->setDelayRespInterval(GetDelayResponseIntervalMs());
	ui->nextBtn->setDelayRespInterval(GetDelayResponseIntervalMs());
	setWindowTitle(QTStr("Bgm.Title"));

	connect(&timerLoading, &QTimer::timeout, [this]() {
		if (indexLoading != -1) {
			PLSBgmItemDelegate::nextLoadFrame();
			auto index = ui->playListWidget->model()->index(indexLoading, 0);
			auto data = ui->playListWidget->model()->data(index, (int)CustomDataRole::MediaStatusRole).value<MediaStatus>();
			ui->playListWidget->update(index);
			if (data == MediaStatus::stateLoading && ((os_gettime_ns() - loadingTimeout) / 1000000 >= LoadingTimeoutMS)) {
				PLSBgmItemData data_ = ui->playListWidget->Get(indexLoading);
				PLS_INFO(MAIN_BGM_MODULE, "Loading [%s] timeout.", data_.title.toUtf8().constData());
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

void PLSBackgroundMusicView::SetLoop(const quint64 &sceneItem, LoopMode mode)
{
	this->m_loopMode = mode;

	obs_source_t *source = GetSource(sceneItem);
	if (!source) {
		return;
	}
	SetLoop(source, mode);
}

void PLSBackgroundMusicView::SetLoop(OBSSource source, LoopMode mode)
{
	if (!source) {
		return;
	}
	OBSData settings = obs_data_create();
	obs_data_set_string(settings, "method", "bgm_loop");
	obs_data_set_bool(settings, IS_LOOP, m_loopMode == LoopMode::LoopAll || m_loopMode == LoopMode::LoopOne);
	obs_data_set_string(settings, "loopMode", QMetaEnum::fromType<LoopMode>().valueToKey(static_cast<int>(m_loopMode)));
	pls_source_set_private_data(source, settings);
	obs_data_release(settings);

	refreshLoopUi();
	PLS_UI_ACTION("In Background Music, the loop mode has been changed: %s.", QMetaEnum::fromType<LoopMode>().valueToKey(static_cast<int>(m_loopMode)));
}

void PLSBackgroundMusicView::refreshLoopUi()
{
	if (m_loopMode == LoopMode::LoopAll) {
		ui->loopBtn->setToolTip(QTStr("Bgm.Repeat"));
		pls_flush_style(ui->loopBtn, "loopMode", QMetaEnum::fromType<LoopMode>().valueToKey(static_cast<int>(LoopMode::LoopAll)));
	} else if (m_loopMode == LoopMode::LoopOne) {
		ui->loopBtn->setToolTip(QTStr("Bgm.Repeat.One"));
		pls_flush_style(ui->loopBtn, "loopMode", QMetaEnum::fromType<LoopMode>().valueToKey(static_cast<int>(LoopMode::LoopOne)));
	} else {
		ui->loopBtn->setToolTip(QTStr("Bgm.No.Repeat"));
		pls_flush_style(ui->loopBtn, "loopMode", QMetaEnum::fromType<LoopMode>().valueToKey(static_cast<int>(LoopMode::NoLoop)));
	}
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

void PLSBackgroundMusicView::createGetCoverThread()
{
	if (coverThreadObj) {
		return;
	}
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
	return pls_get_source_by_name(sourceName.toUtf8().constData()) == GetSource(currentSceneItem);
}

bool PLSBackgroundMusicView::SameWithCurrentSource(const quint64 &sceneItem)
{
	return GetSource(sceneItem) == GetSource(currentSceneItem);
}

bool PLSBackgroundMusicView::isSceneitemVisible(const quint64 &sceneItem)
{
	return obs_sceneitem_visible(pls_get_sceneitem_by_pointer_address((void *)sceneItem));
}

void PLSBackgroundMusicView::checkSceneitemEnableToastDisplayed(const quint64 &sceneitem, bool alwaysShowToast)
{
	auto item = pls_get_sceneitem_by_pointer_address(PLSBasic::instance()->GetCurrentScene(), (void *)sceneitem);
	if (!item) {
		return;
	}
	return checkSceneitemEnableToastDisplayed(item, alwaysShowToast);
}

void PLSBackgroundMusicView::checkSceneitemEnableToastDisplayed(OBSSceneItem sceneitem, bool alwaysShowToast)
{
	OBSDataAutoRelease data = obs_sceneitem_get_private_settings(sceneitem);
	bool needShowToast = alwaysShowToast || !obs_data_get_bool(data, "enableToastDisplayed");
	if (needShowToast) {
		obs_data_set_bool(data, "enableToastDisplayed", true);
		ShowToastView(networkAvailable ? tr("Bgm.Enable.Playlist") : tr("Bgm.No.Network.Toast"));
	}
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
	OBSSource source = pls_get_source_by_name(name.toUtf8().constData());
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

PLSBgmItemData PLSBackgroundMusicView::getCurrentRow() const
{
	return ui->playListWidget->GetCurrent();
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
	bool selectBgm = main->GetSelectBgmSourceName(name, item);

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
		setCurrentRow(source);
		return;
	}
	if (isVisible()) {
		ui->playListWidget->scrollTo(ui->playListWidget->GetModelIndex(currentRow));
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

	bool visible = isSceneitemVisible(sceneItem);
	obs_media_state state = obs_source_media_get_state(source);
	PLSBgmItemData data;
	if (!visible) {
		data = GetCurrentPlayListData();
	} else if (state == OBS_MEDIA_STATE_ENDED || state == OBS_MEDIA_STATE_STOPPED || state == OBS_MEDIA_STATE_NONE) {
		data = GetCurrentPlayListData();
	} else {
		data = GetCurrentPlayListDataBySettings();
	}

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
			ui->playingSlider->setValue(visible ? (int)(sliderPosition) : 0);
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
	} else if (state == OBS_MEDIA_STATE_NONE || state == OBS_MEDIA_STATE_STOPPED || state == OBS_MEDIA_STATE_ENDED) {
		ui->playingSlider->setEnabled(false);
		ui->playingSlider->setValue(0);
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
	DisablePlayerControlUI(!visible, true);
	ShowCoverImage(data);
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

	if (auto main = PLSBasic::instance(); main) {
		main->getApi()->on_event(pls_frontend_event::PLS_FRONT_EVENT_MUSIC_PLAYLIST_SELECT_CHANGED);
	}
}

void PLSBackgroundMusicView::DisablePlayerControlUI(bool disable, bool needShowCover)
{
	QString status = disable ? STATUS_DISABLE : STATUS_ENABLE;
	pls_flush_style(ui->refreshBtn, STATUS, status);
	pls_flush_style(ui->playBtn, STATUS, status);
	pls_flush_style(ui->preBtn, STATUS, status);
	pls_flush_style(ui->nextBtn, STATUS, status);
	pls_flush_style(ui->loopBtn, STATUS, status);
	ui->noSongFrame->setVisible(!needShowCover);
	ui->coverLabel->setVisible(needShowCover);
	if (!needShowCover)
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

void PLSBackgroundMusicView::SetUrlInfo(OBSSource source, const PLSBgmItemData &data, bool playImmediately) const
{
	if (!source) {
		return;
	}

	OBSData settings = obs_data_create();

	obs_data_set_string(settings, "method", "bgm_play");
	SetUrlInfoToSettings(settings, data);

	pls_source_set_private_data(source, settings);
	obs_data_release(settings);
	if (playImmediately) {
		obs_source_media_restart(source);
	}
}

void PLSBackgroundMusicView::SetUrlInfoToSettings(obs_data_t *settings, const PLSBgmItemData &data) const
{
	if (!settings) {
		return;
	}

	obs_data_set_string(settings, BGM_TITLE, data.title.toUtf8().constData());
	obs_data_set_string(settings, BGM_PRODUCER, data.producer.toUtf8().constData());
	obs_data_set_string(settings, BGM_URL, data.GetUrl(data.id).toUtf8().constData());
	obs_data_set_string(settings, BGM_DURATION_TYPE, QString::number(data.id).toUtf8().constData());
	obs_data_set_string(settings, BGM_DURATION, QString::number(data.GetDuration(data.id)).toUtf8().constData());
	obs_data_set_string(settings, BGM_GROUP, data.group.toUtf8().constData());
	obs_data_set_string(settings, BGM_COVER_PATH, data.coverPath.toUtf8().constData());
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

	bool open = mi_open(&media_info, url.toUtf8().constData(), static_cast<mi_open_mode>(MI_OPEN_DIRECTLY | MI_OPEN_TRY_DECODER));
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
		obs_data_set_string(url, BGM_URL, data.GetUrl(data.id).toUtf8().constData());
		obs_data_set_string(url, BGM_DURATION_TYPE, QString::number(data.id).toUtf8().constData());
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

	OBSSource source = pls_get_source_by_name(currentSourceName.toUtf8().constData());
	if (!source) {
		return;
	}

	int currentRow = ui->playListWidget->GetCurrentRow();
	if (-1 == currentRow) {
		setCurrentRow(source);
		return;
	}
	if (!isSceneitemVisible(currentSceneItem)) {
		ui->playListWidget->SetMediaStatus(currentRow, MediaStatus::stateCurrentInvisible);
		return;
	}
	obs_media_state state = obs_source_media_get_state(source);
	if (state == OBS_MEDIA_STATE_PLAYING) {
		indexLoading = currentRow;
		ui->playListWidget->SetMediaStatus(currentRow, isLoading ? MediaStatus::stateLoading : MediaStatus::statePlaying);
		StartLoadingTimer(100);
	} else if (state == OBS_MEDIA_STATE_OPENING) {
		indexLoading = currentRow;
		ui->playListWidget->SetMediaStatus(currentRow, MediaStatus::stateLoading);
	} else if (state == OBS_MEDIA_STATE_ENDED || state == OBS_MEDIA_STATE_STOPPED || state == OBS_MEDIA_STATE_NONE) {
		if (-1 == currentRow) {
			setCurrentRow(source);
		}
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
	OBSSource source = pls_get_source_by_name(currentSourceName.toUtf8().constData());
	if (!source) {
		ui->playListWidget->SetMediaStatus(index, MediaStatus::stateNormal);
		return;
	}

	if (data_.isDisable) {
		ui->playListWidget->SetMediaStatus(index, MediaStatus::stateInvalid);
		return;
	}

	if (!isSceneitemVisible(currentSceneItem)) {
		current ? ui->playListWidget->SetMediaStatus(index, MediaStatus::stateCurrentInvisible) : ui->playListWidget->SetMediaStatus(index, MediaStatus::stateInvisible);
		return;
	}

	if (current) {
		obs_media_state state = obs_source_media_get_state(source);
		if (state == OBS_MEDIA_STATE_PLAYING) {
			if (!isLoading) {
				ui->playListWidget->SetMediaStatus(index, MediaStatus::statePlaying);
				indexLoading = index;
				StartLoadingTimer(100);
			}
		} else if (state == OBS_MEDIA_STATE_OPENING) {
			if (!isLoading) {
				ui->playListWidget->SetMediaStatus(index, MediaStatus::stateLoading);
			}
		} else if (state == OBS_MEDIA_STATE_PAUSED) {
			ui->playListWidget->SetMediaStatus(index, MediaStatus::statePause);
		} else if (state == OBS_MEDIA_STATE_ERROR) {
			ui->playListWidget->SetMediaStatus(index, MediaStatus::stateInvalid);
		} else if (state == OBS_MEDIA_STATE_NONE || state == OBS_MEDIA_STATE_ENDED || state == OBS_MEDIA_STATE_STOPPED) {
			ui->playListWidget->SetMediaStatus(index, MediaStatus::stateSelected);
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
		ui->playingSlider->setValue(0);
		ui->currentTimeLabel->setText("00:00");
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
	if (toastView.isVisible() && toastView.GetMessageContent() == tr("Bgm.No.Network.Toast")) {
		return;
	}
	toastView.SetMessage(text);
	ResizeToastView();
	toastView.ShowToast();
}

void PLSBackgroundMusicView::ResizeToastView()
{
	toastView.SetShowWidth(this->width() - 2 * 10);
	QPoint pos;
	if (ui->noSourcePage->isVisible()) {
		pos = ui->noSourcePage->mapTo(this, QPoint(10, 10));
	} else if (ui->playListPage->isVisible()) {
		pos = ui->playListPage->mapTo(this, QPoint(10, 10));
	}
	toastView.move(pos.x(), pos.y());
}

QImage PLSBackgroundMusicView::GetCoverImage(const QString &url) const
{
	QImage image{};
	media_info_t media_info;
	memset(&media_info, 0, sizeof(media_info_t));
	bool open = mi_open(&media_info, url.toUtf8().constData(), MI_OPEN_DIRECTLY);
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
	createGetCoverThread();
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
				obs_data_set_string(settings_, BGM_URL, data.GetUrl(data.id).toUtf8().constData());
				obs_data_set_string(settings_, BGM_DURATION_TYPE, QString::number(data.id).toUtf8().constData());
				obs_data_set_string(settings_, BGM_COVER_PATH, data.coverPath.toUtf8().constData());
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

	bool open = mi_open(&media_info, data.GetUrl(data.id).toUtf8().constData(), static_cast<mi_open_mode>(MI_OPEN_DIRECTLY | MI_OPEN_TRY_DECODER));
	mi_free(&media_info);

	emit checkFinished(data, open);
}

void PLSBackgroundMusicView::SeekTo(int val)
{
	OBSSource source = pls_get_source_by_name(currentSourceName.toUtf8().constData());
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
	OBSSource source = pls_get_source_by_name(currentSourceName.toUtf8().constData());
	if (!source)
		return 0;

	float percent = (float)val / (float)ui->playingSlider->maximum();
	float duration = (float)obs_source_media_get_duration(source);
	int64_t seekTo = (int64_t)(percent * duration);

	return seekTo;
}

void PLSBackgroundMusicView::setCurrentRow(OBSSource source)
{
	if (!source) {
		ResetControlView();
		return;
	}
	// get first invalid
	PLSBgmItemData data;
	bool valid = false;
	for (auto dataTmp : ui->playListWidget->GetData()) {
		if (dataTmp.title.isEmpty() || dataTmp.isDisable) {
			continue;
		}
		data = dataTmp;
		valid = true;
		break;
	}
	if (!valid) {
		ResetControlView();
		ui->playListWidget->SetCurrentRow(PLSBgmItemData());
		return;
	}

	setCurrentRow(source, data);
}

void PLSBackgroundMusicView::setCurrentRow(OBSSource source, const PLSBgmItemData &data)
{
	if (!source) {
		ResetControlView();
		return;
	}

	SetUrlInfo(source, data, false);
	ui->playingSlider->setEnabled(false);
	ui->playingSlider->setValue(0);
	ui->currentTimeLabel->setText("00:00");
	ui->durationLabel->setText(PLSBgmDataViewManager::Instance()->ConvertIntToTimeString(data.GetDuration(data.id)));
	ui->coverLabel->SetMusicInfo(data.title, data.producer);
	ui->playListWidget->SetCurrentRow(data);
	ShowCoverGif(false);
	ShowCoverImage(data);
	SetPlayListStatus(data);
	SetPlayerControllerStatus(currentSceneItem);
	if (auto main = PLSBasic::instance(); main) {
		main->getApi()->on_event(pls_frontend_event::PLS_FRONT_EVENT_MUSIC_PLAYLIST_SELECT_CHANGED);
	}
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
		pls_async_call(this, [this]() { NextTask(); });
	taskQueue.enqueue(data);
}

void GetCoverThread::PrintThumbInfo(const QString &url, media_info_t *mi) const
{
	auto width = mi_get_int(mi, "width");
	auto height = mi_get_int(mi, "height");
	auto vformat = mi_get_int(mi, "video_format");

	if (width > 0 && height > 0) {
		auto file_name = pls_get_path_file_name(url);
		PLS_INFO(MAIN_BGM_MODULE, "bgm thumbnail info. %lldx%lld, vformat:%lld, file:'%s'", width, height, vformat, file_name.toUtf8().constData());
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
	bool open = mi_open(&media_info, url.toUtf8().constData(), MI_OPEN_DIRECTLY);
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
