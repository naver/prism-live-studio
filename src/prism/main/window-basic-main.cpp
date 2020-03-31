/******************************************************************************
    Copyright (C) 2013-2015 by Hugh Bailey <obs.jim@gmail.com>
                               Zachary Lund <admin@computerquip.com>
                               Philippe Groarke <philippe.groarke@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <ctime>
#include <obs.hpp>
#include <QGuiApplication>
#include <QShowEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QDesktopWidget>
#include <QScreen>
#include <QSizePolicy>
#include <QUrl>
#include <QDir>
#include <QFile>

#include <util/dstr.h>
#include <util/util.hpp>
#include <util/platform.h>
#include <util/profiler.hpp>
#include <util/dstr.hpp>

#include "pls-app.hpp"
#include "platform.hpp"
#include "visibility-item-widget.hpp"
#include "item-widget-helpers.hpp"
#include "window-basic-settings.hpp"
#include "window-namedialog.hpp"
#include "window-basic-auto-config.hpp"
#include "window-basic-source-select.hpp"
#include "window-basic-main.hpp"
#include "window-basic-stats.hpp"
#include "window-basic-main-outputs.hpp"
#include "window-log-reply.hpp"
#include "window-projector.hpp"
#include "window-remux.hpp"
#include "qt-wrappers.hpp"
#include "display-helpers.hpp"
#include "volume-control.hpp"
#include "remote-text.hpp"
#include "dialog-view.hpp"
#include "main-view.hpp"
#include "pls-net-url.hpp"
#include "alert-view.hpp"
#include "toast-view.hpp"
#include "color-dialog-view.hpp"
#include "PLSSceneDataMgr.h"
#include "PLSSceneListView.h"
#include "window-basic-status-bar.hpp"
#include "channels/ChannelsDataApi/ChannelCommonFunctions.h"
#include "pls-common-language.hpp"
#include "pls-net-url.hpp"
#include "PLSChannelsEntrance.h"
#include "PLSAudioMixerScrollBar.h"
#include < fstream >
#include <sstream>

#include "ui_PLSBasic.h"
#include "ui_ColorSelect.h"

#include <fstream>
#include <sstream>

#include <QScreen>
#include <QWindow>
#include <QList>

#include <json11.hpp>

#include "pls-common-define.hpp"
#include "about-view.hpp"
#include "notice-view.hpp"
#include "pls-complex-header-icon.hpp"
#include "pls-notice-handler.hpp"

#include "log.h"
#include "action.h"
#include "log/module_names.h"
#include "PLSAddSourceMenuStyle.hpp"
#include "PLSAction.h"
#include "PLSMenu.hpp"
#include "PLSContactView.hpp"
#include "PLSOpenSourceView.h"

#include "PLSPlatformApi.h"

#define CONFIG_BASIC_WINDOW_MODULE "BasicWindow"
#define CONFIG_PREVIEW_MODE_MODULE "PreviewProgramMode"
#define MAIN_FRAME "MainFrame"

using namespace json11;
using namespace std;

#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
#endif

#include "ui-config.h"

#define RESTARTAPP 1024
struct QCef;
struct QCefCookieManager;

QCef *cef = nullptr;
QCefCookieManager *panel_cookies = nullptr;

void DestroyPanelCookieManager();
void restartApp();
namespace {

QDataStream &operator<<(QDataStream &out, const SignalContainer<OBSScene> &sc)
{
	return out << QString(obs_source_get_name(obs_scene_get_source(sc.ref))) << sc.handlers;
}

QDataStream &operator>>(QDataStream &in, SignalContainer<OBSScene> &sc)
{
	QString sceneName;

	in >> sceneName;

	obs_source_t *source = obs_get_source_by_name(QT_TO_UTF8(sceneName));
	sc.ref = obs_scene_from_source(source);

	in >> sc.handlers;

	return in;
}
}

extern volatile long insideEventLoop;
extern bool file_exists(const char *path);

Q_DECLARE_METATYPE(OBSScene);
Q_DECLARE_METATYPE(OBSSceneItem);
Q_DECLARE_METATYPE(OBSSource);
Q_DECLARE_METATYPE(obs_order_movement);
Q_DECLARE_METATYPE(SignalContainer<OBSScene>);

template<typename T> static T GetPLSRef(QListWidgetItem *item)
{
	return item->data(static_cast<int>(QtDataRole::PLSRef)).value<T>();
}

template<typename T> static void SetPLSRef(QListWidgetItem *item, T &&val)
{
	item->setData(static_cast<int>(QtDataRole::PLSRef), QVariant::fromValue(val));
}

static void AddExtraModulePaths()
{
	char base_module_dir[512];
#if defined(_WIN32) || defined(__APPLE__)
	int ret = GetProgramDataPath(base_module_dir, sizeof(base_module_dir), "PRISMLiveStudio/plugins/%module%");
#else
	int ret = GetConfigPath(base_module_dir, sizeof(base_module_dir), "PRISMLiveStudio/plugins/%module%");
#endif

	if (ret <= 0)
		return;

	string path = base_module_dir;
#if defined(__APPLE__)
	obs_add_module_path((path + "/bin").c_str(), (path + "/data").c_str());

	BPtr<char> config_bin = os_get_config_path_ptr("PRISMLiveStudio/plugins/%module%/bin");
	BPtr<char> config_data = os_get_config_path_ptr("PRISMLiveStudio/plugins/%module%/data");
	obs_add_module_path(config_bin, config_data);

#elif ARCH_BITS == 64
	obs_add_module_path((path + "/bin/64bit").c_str(), (path + "/data").c_str());
#else
	obs_add_module_path((path + "/bin/32bit").c_str(), (path + "/data").c_str());
#endif
}

extern pls_frontend_callbacks *InitializeAPIInterface(PLSBasic *main);

static int CountVideoSources()
{
	int count = 0;

	auto countSources = [](void *param, obs_source_t *source) {
		if (!source)
			return true;

		uint32_t flags = obs_source_get_output_flags(source);
		if ((flags & OBS_SOURCE_VIDEO) != 0)
			(*reinterpret_cast<int *>(param))++;

		return true;
	};

	obs_enum_sources(countSources, &count);
	return count;
}

void assignDockToggle(PLSBasic *basic, PLSDock *dock, QAction *action, const char *dockName)
{
	action->setText(dock->isFloating() ? QObject::tr("Basic.MainMenu.View.Attach") : QObject::tr("Basic.MainMenu.View.Detach"));
	action->setChecked(dock->isFloating());

	QObject::connect(action, &QAction::triggered, [basic, dock, action, dockName]() {
		PLS_UI_STEP(MAINMENU_MODULE, dockName, ACTION_CLICK);
		if (dock->isFloating()) {
			dock->setFloating(false);
		} else {
			QMetaObject::invokeMethod(basic, "SetDocksMovePolicy", Q_ARG(PLSDock *, dock));
		}
	});
	QObject::connect(dock, &QDockWidget::topLevelChanged, [action, dockName](bool isFloating) {
		PLS_INFO(MAINMENU_MODULE, "%s %s", dockName, isFloating ? "Floating" : "Docking");
		action->setText(isFloating ? QObject::tr("Basic.MainMenu.View.Attach") : QObject::tr("Basic.MainMenu.View.Detach"));
		action->setChecked(isFloating);
	});
}

extern void RegisterTwitchAuth();
extern void RegisterMixerAuth();
extern void RegisterRestreamAuth();

PLSBasic::PLSBasic(PLSMainView *mainView) : PLSMainWindow(mainView), ui(new Ui::PLSBasic)
{
	m_isUpdateLanguage = false;
	m_isSessionExpired = false;
	this->mainView = mainView;
	this->mainMenu = nullptr;
	setWindowFlags(Qt::SubWindow);
	setCursor(Qt::ArrowCursor);

	setAttribute(Qt::WA_NativeWindow);
	mainView->installEventFilter(this);

#if TWITCH_ENABLED
	RegisterTwitchAuth();
#endif
#if MIXER_ENABLED
	RegisterMixerAuth();
#endif
#if RESTREAM_ENABLED
	RegisterRestreamAuth();
#endif

	setAcceptDrops(true);

	api = InitializeAPIInterface(this);

	ui->setupUi(this);
	ui->previewDisabledWidget->setVisible(false);

	startingDockLayout = saveState();

	copyActionsDynamicProperties();

	char styleSheetPath[512];
	int ret = GetProfilePath(styleSheetPath, sizeof(styleSheetPath), "stylesheet.qss");
	if (ret > 0) {
		if (QFile::exists(styleSheetPath)) {
			QString path = QString("file:///") + QT_UTF8(styleSheetPath);
			App()->setStyleSheet(path);
		}
	}

	qRegisterMetaType<OBSScene>("OBSScene");
	qRegisterMetaType<OBSSceneItem>("OBSSceneItem");
	qRegisterMetaType<OBSSource>("OBSSource");
	qRegisterMetaType<obs_hotkey_id>("obs_hotkey_id");
	qRegisterMetaType<SignalContainer<OBSScene>>("SignalContainer<OBSScene>");

	qRegisterMetaTypeStreamOperators<std::vector<std::shared_ptr<OBSSignal>>>("std::vector<std::shared_ptr<OBSSignal>>");
	qRegisterMetaTypeStreamOperators<OBSScene>("OBSScene");
	qRegisterMetaTypeStreamOperators<OBSSceneItem>("OBSSceneItem");
	qRegisterMetaTypeStreamOperators<SignalContainer<OBSScene>>("SignalContainer<OBSScene>");

	//ui->scenes->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->sources->setAttribute(Qt::WA_MacShowFocusRect, false);
	auto displayResize = [this]() {
		struct obs_video_info ovi;
		if (obs_get_video_info(&ovi))
			ResizePreview(ovi.base_width, ovi.base_height);
	};

	auto adjustResize = [this](QLabel *scr, QLabel *view, bool &handled) {
		struct obs_video_info ovi;
		if (obs_get_video_info(&ovi)) {
			QSize size = GetPixelSize(scr);
			float scale = 0.0;
			int x = 0;
			int y = 0;

			if (ui->preview->IsFixedScaling()) {
				scale = ui->preview->GetScalingAmount();
				GetCenterPosFromFixedScale(int(ovi.base_width), int(ovi.base_height), size.width() - PREVIEW_EDGE_SIZE * 2, size.height() - PREVIEW_EDGE_SIZE * 2, x, y, scale);
				x += ui->preview->GetScrollX();
				y += ui->preview->GetScrollY();

			} else {
				GetScaleAndCenterPos(int(ovi.base_width), int(ovi.base_height), size.width() - PREVIEW_EDGE_SIZE * 2, size.height() - PREVIEW_EDGE_SIZE * 2, x, y, scale);
			}

			x += float(PREVIEW_EDGE_SIZE);
			y += float(PREVIEW_EDGE_SIZE);

			int cx = int(scale * float(ovi.base_width));
			int cy = int(scale * float(ovi.base_height));

			view->setGeometry(x, y, cx, cy);
			handled = true;
		}
	};

	connect(windowHandle(), &QWindow::screenChanged, displayResize);
	connect(ui->preview, &PLSQTDisplay::DisplayResized, displayResize);
	connect(ui->preview, &PLSQTDisplay::AdjustResizeView, adjustResize);
	connect(ui->previewTitle->transApply, &QAbstractButton::clicked, this, &PLSBasic::TransitionClicked);

	delete shortcutFilter;
	shortcutFilter = CreateShortcutFilter();
	mainView->installEventFilter(shortcutFilter);
	installEventFilter(shortcutFilter);

	stringstream name;
	name << "PLS " << App()->GetVersionString();
	blog(LOG_INFO, "%s", name.str().c_str());
	blog(LOG_INFO, "---------------------------------");

	UpdateTitleBar();

	cpuUsageInfo = os_cpu_usage_info_start();
	cpuUsageTimer = new QTimer(this);
	connect(cpuUsageTimer.data(), SIGNAL(timeout()), mainView->statusBar(), SLOT(UpdateCPUUsage()));
	cpuUsageTimer->start(1000);

	diskFullTimer = new QTimer(this);
	connect(diskFullTimer, SIGNAL(timeout()), this, SLOT(CheckDiskSpaceRemaining()));

	QAction *renameScene = new QAction(ui->scenesDock);
	renameScene->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(renameScene, SIGNAL(triggered()), this, SLOT(EditSceneName()));
	ui->scenesDock->addAction(renameScene);

	QAction *renameSource = new QAction(ui->sourcesDock);
	renameSource->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(renameSource, SIGNAL(triggered()), this, SLOT(EditSceneItemName()));
	ui->sourcesDock->addAction(renameSource);

#ifdef __APPLE__
	renameScene->setShortcut({Qt::Key_Return});
	renameSource->setShortcut({Qt::Key_Return});

	ui->actionRemoveSource->setShortcuts({Qt::Key_Backspace});
	ui->actionRemoveScene->setShortcuts({Qt::Key_Backspace});

	ui->action_Settings->setMenuRole(QAction::PreferencesRole);
	ui->actionExit->setMenuRole(QAction::QuitRole);
#else
	renameScene->setShortcut({Qt::Key_F2});
	renameSource->setShortcut({Qt::Key_F2});
#endif

	auto addNudge = [this](const QKeySequence &seq, const char *s) {
		QAction *nudge = new QAction(ui->preview);
		nudge->setShortcut(seq);
		nudge->setShortcutContext(Qt::WidgetShortcut);
		ui->preview->addAction(nudge);
		connect(nudge, SIGNAL(triggered()), this, s);
	};

	addNudge(Qt::Key_Up, SLOT(NudgeUp()));
	addNudge(Qt::Key_Down, SLOT(NudgeDown()));
	addNudge(Qt::Key_Left, SLOT(NudgeLeft()));
	addNudge(Qt::Key_Right, SLOT(NudgeRight()));

	//restore parent window geometry
	const char *geometry = config_get_string(App()->GlobalConfig(), "BasicWindow", "geometry");
	if (geometry != NULL) {
		QByteArray byteArray = QByteArray::fromBase64(QByteArray(geometry));
		mainView->restoreGeometry(byteArray);

		QRect windowGeometry = mainView->normalGeometry();
		if (!WindowPositionValid(windowGeometry)) {
			QRect rect = App()->desktop()->geometry();
			mainView->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), rect));
		}
	}

	mainView->geometryOfNormal = mainView->geometry();
	if (config_get_bool(App()->GlobalConfig(), "BasicWindow", "FullScreenState")) {
		mainView->isMaxState = config_get_bool(App()->GlobalConfig(), "BasicWindow", "MaximizedState");
		mainView->showFullScreen();
	} else if (config_get_bool(App()->GlobalConfig(), "BasicWindow", "MaximizedState")) {
		mainView->showMaximized();
	}

	ui->previewDisabledWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->previewDisabledWidget, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(PreviewDisabledMenu(const QPoint &)));
	connect(ui->enablePreviewButton, SIGNAL(clicked()), this, SLOT(TogglePreview()));

	connect(ui->actionExit, &QAction::triggered, this, [mainView]() {
		PLS_UI_STEP(MAINMENU_MODULE, "Main Menu File Exit", ACTION_CLICK);
		mainView->close();
	});

	connect(mainView, &PLSMainView::studioModeChanged, this, &PLSBasic::on_actionStudioMode_triggered);

	// process main menu shortcut
	connect(
		this, &PLSBasic::mainMenuPopupSubmenu, this,
		[this](PLSMenu *submenu) {
			PLSPopupMenu::setSelectedAction(submenu->menuAction());
			QList<QAction *> actions = submenu->actions();
			if (!actions.isEmpty()) {
				PLSPopupMenu::setSelectedAction(actions.first());
			}
		},
		Qt::QueuedConnection);

	auto addMainMenuShortcut = [mainView, this](const QString &shortcut, PLSMenu *submenu) {
		QAction *action = new QAction(this);
		action->setShortcut(QKeySequence(shortcut));
		action->setShortcutContext(Qt::ApplicationShortcut);
		mainView->addAction(action);
		connect(action, &QAction::triggered, this, [mainView, this, submenu]() {
			emit mainMenuPopupSubmenu(submenu);
			mainMenu->popup(mainView->menuButton(), QPoint(0, 1));
		});
	};

	addMainMenuShortcut("Alt+F", ui->menuFile);
	addMainMenuShortcut("Alt+V", ui->viewMenu);
	addMainMenuShortcut("Alt+E", ui->menuEdit);
	addMainMenuShortcut("Alt+S", ui->sceneCollectionMenu);
	addMainMenuShortcut("Alt+P", ui->profileMenu);
	addMainMenuShortcut("Alt+T", ui->menuTools);

	obs_frontend_add_event_callback(&frontendEventHandler, this);
	pls_frontend_add_event_callback(&frontendEventHandler, this);
	pls_frontend_add_event_callback(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGOUT, &PLSBasic::LogoutCallback, this);

	ui->actionRemoveSource->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	ui->actionRemoveScene->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	ui->scenesFrame->addAction(ui->actionRemoveScene);
	ui->sources->addAction(ui->actionRemoveSource);

	mainView->addActions({ui->action_Settings, ui->actionExit, ui->actionFullscreenInterface, ui->actionCopySource, ui->actionPasteRef, ui->actionShowAbout, ui->actionPrismWebsite,
			      ui->actionPrismFAQ, ui->actionContactUs, ui->actionEditTransform, ui->actionFitToScreen, ui->actionStretchToScreen, ui->actionResetTransform, ui->actionCenterToScreen,
			      ui->actionMoveUp, ui->actionMoveDown, ui->actionMoveToTop, ui->actionMoveToBottom});

	ui->previewTitle->OnStudioModeStatus(IsPreviewProgramMode());
	ui->previewTitle->OnLiveStatus(false);
	ui->previewTitle->OnRecordStatus(false);

	ui->hMixerScrollArea->setVerticalScrollBar(new PLSAudioMixerScrollBar());
	connect(qobject_cast<PLSAudioMixerScrollBar *>(ui->hMixerScrollArea->verticalScrollBar()), &PLSAudioMixerScrollBar::isShowScrollBar, [=](bool isShow) {
		if (isShow) {
			ui->hVolControlLayout->setContentsMargins(0, 0, 0, 0);
		} else {
			ui->hVolControlLayout->setContentsMargins(0, 0, 10, 0);
		}
		ui->hMixerScrollArea->update();
	});

	connect(PLS_PLATFORM_API, &PLSPlatformApi::enterLivePrepareState, this, [=](bool disable) { ui->action_Settings->setEnabled(!disable); });
}

static void SaveAudioDevice(const char *name, int channel, obs_data_t *parent, vector<OBSSource> &audioSources)
{
	obs_source_t *source = obs_get_output_source(channel);
	if (!source)
		return;

	audioSources.push_back(source);

	obs_data_t *data = obs_save_source(source);

	obs_data_set_obj(parent, name, data);

	obs_data_release(data);
	obs_source_release(source);
}

static obs_data_t *GenerateSaveData(obs_data_array_t *sceneOrder, int transitionDuration, obs_data_array_t *transitions, OBSScene &scene, OBSSource &curProgramScene,
				    obs_data_array_t *savedProjectorList)
{
	obs_data_t *saveData = obs_data_create();

	vector<OBSSource> audioSources;
	audioSources.reserve(5);

	SaveAudioDevice(DESKTOP_AUDIO_1, 1, saveData, audioSources);
	SaveAudioDevice(DESKTOP_AUDIO_2, 2, saveData, audioSources);
	SaveAudioDevice(AUX_AUDIO_1, 3, saveData, audioSources);
	SaveAudioDevice(AUX_AUDIO_2, 4, saveData, audioSources);
	SaveAudioDevice(AUX_AUDIO_3, 5, saveData, audioSources);
	SaveAudioDevice(AUX_AUDIO_4, 6, saveData, audioSources);

	/* -------------------------------- */
	/* save non-group sources           */

	auto FilterAudioSources = [&](obs_source_t *source) {
		if (obs_source_is_group(source))
			return false;

		return find(begin(audioSources), end(audioSources), source) == end(audioSources);
	};
	using FilterAudioSources_t = decltype(FilterAudioSources);

	obs_data_array_t *sourcesArray =
		obs_save_sources_filtered([](void *data, obs_source_t *source) { return (*static_cast<FilterAudioSources_t *>(data))(source); }, static_cast<void *>(&FilterAudioSources));

	/* -------------------------------- */
	/* save group sources separately    */

	/* saving separately ensures they won't be loaded in older versions */
	obs_data_array_t *groupsArray = obs_save_sources_filtered([](void *, obs_source_t *source) { return obs_source_is_group(source); }, nullptr);

	/* -------------------------------- */

	obs_source_t *transition = obs_get_output_source(0);
	obs_source_t *currentScene = obs_scene_get_source(scene);
	const char *sceneName = obs_source_get_name(currentScene);
	const char *programName = obs_source_get_name(curProgramScene);

	const char *sceneCollection = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");

	obs_data_set_string(saveData, "current_scene", sceneName);
	obs_data_set_string(saveData, "current_program_scene", programName);
	obs_data_set_array(saveData, "scene_order", sceneOrder);
	obs_data_set_string(saveData, "name", sceneCollection);
	obs_data_set_array(saveData, "sources", sourcesArray);
	obs_data_set_array(saveData, "groups", groupsArray);
	obs_data_set_array(saveData, "transitions", transitions);
	obs_data_set_array(saveData, "saved_projectors", savedProjectorList);
	obs_data_array_release(sourcesArray);
	obs_data_array_release(groupsArray);

	obs_data_set_string(saveData, "current_transition", obs_source_get_name(transition));
	obs_data_set_int(saveData, "transition_duration", transitionDuration);
	obs_source_release(transition);

	return saveData;
}

void PLSBasic::copyActionsDynamicProperties()
{
	// Themes need the QAction dynamic properties
	/*for (QAction *x : ui->scenesToolbar->actions()) {
		QWidget *temp = ui->scenesToolbar->widgetForAction(x);

		for (QByteArray &y : x->dynamicPropertyNames()) {
			temp->setProperty(y, x->property(y));
		}
	}*/
	//TODO
	/*for (QAction *x : ui->sourcesToolbar->actions()) {
		QWidget *temp = ui->sourcesToolbar->widgetForAction(x);

		for (QByteArray &y : x->dynamicPropertyNames()) {
			temp->setProperty(y, x->property(y));
		}
	}*/
}

void PLSBasic::UpdateVolumeControlsDecayRate()
{
	double meterDecayRate = config_get_double(basicConfig, "Audio", "MeterDecayRate");

	for (size_t i = 0; i < volumes.size(); i++) {
		volumes[i]->SetMeterDecayRate(meterDecayRate);
	}
}

void PLSBasic::UpdateVolumeControlsPeakMeterType()
{
	uint32_t peakMeterTypeIdx = config_get_uint(basicConfig, "Audio", "PeakMeterType");

	enum obs_peak_meter_type peakMeterType;
	switch (peakMeterTypeIdx) {
	case 0:
		peakMeterType = SAMPLE_PEAK_METER;
		break;
	case 1:
		peakMeterType = TRUE_PEAK_METER;
		break;
	default:
		peakMeterType = SAMPLE_PEAK_METER;
		break;
	}

	for (size_t i = 0; i < volumes.size(); i++) {
		volumes[i]->setPeakMeterType(peakMeterType);
	}
}

void PLSBasic::ClearVolumeControls()
{
	for (VolControl *vol : volumes)
		delete vol;

	volumes.clear();
}

obs_data_array_t *PLSBasic::SaveSceneListOrder()
{
	obs_data_array_t *sceneOrder = obs_data_array_create();
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();

	for (auto iter = data.begin(); iter != data.end(); ++iter) {
		obs_data_t *data = obs_data_create();
		obs_data_set_string(data, "name", iter->first.toStdString().c_str());
		obs_data_array_push_back(sceneOrder, data);
		obs_data_release(data);
	}

	return sceneOrder;
}

obs_data_array_t *PLSBasic::SaveProjectors()
{
	obs_data_array_t *savedProjectors = obs_data_array_create();

	auto saveProjector = [savedProjectors](PLSDialogView *dialogView, PLSProjector *projector) {
		if (!projector)
			return;

		obs_data_t *data = obs_data_create();
		ProjectorType type = projector->GetProjectorType();
		switch (type) {
		case ProjectorType::Scene:
		case ProjectorType::Source: {
			obs_source_t *source = projector->GetSource();
			const char *name = obs_source_get_name(source);
			obs_data_set_string(data, "name", name);
			break;
		}
		default:
			break;
		}
		obs_data_set_int(data, "monitor", projector->GetMonitor());
		obs_data_set_int(data, "type", static_cast<int>(type));
		obs_data_set_string(data, "geometry", dialogView->saveGeometry().toBase64().constData());
		obs_data_array_push_back(savedProjectors, data);
		obs_data_release(data);
	};

	for (auto &proj : projectors)
		saveProjector(proj.first, proj.second);

	for (auto &proj : windowProjectors)
		saveProjector(proj.first, proj.second);

	return savedProjectors;
}

void PLSBasic::Save(const char *file)
{
	OBSScene scene = GetCurrentScene();
	OBSSource curProgramScene = OBSGetStrongRef(programScene);
	if (!curProgramScene)
		curProgramScene = obs_scene_get_source(scene);

	obs_data_array_t *sceneOrder = SaveSceneListOrder();
	obs_data_array_t *transitions = ui->scenesFrame->SaveTransitions();
	obs_data_array_t *savedProjectorList = SaveProjectors();
	obs_data_t *saveData = GenerateSaveData(sceneOrder, ui->scenesFrame->GetTransitionDurationValue(), transitions, scene, curProgramScene, savedProjectorList);

	obs_data_set_bool(saveData, "preview_locked", ui->preview->Locked());
	obs_data_set_bool(saveData, "scaling_enabled", ui->preview->IsFixedScaling());
	obs_data_set_int(saveData, "scaling_level", ui->preview->GetScalingLevel());
	obs_data_set_double(saveData, "scaling_off_x", ui->preview->GetScrollX());
	obs_data_set_double(saveData, "scaling_off_y", ui->preview->GetScrollY());

	if (api) {
		obs_data_t *moduleObj = obs_data_create();
		api->on_save(moduleObj);
		obs_data_set_obj(saveData, "modules", moduleObj);
		obs_data_release(moduleObj);
	}

	if (!obs_data_save_json_safe(saveData, file, "tmp", "bak"))
		blog(LOG_ERROR, "Could not save scene data to %s", file);

	obs_data_release(saveData);
	obs_data_array_release(sceneOrder);
	obs_data_array_release(transitions);
	obs_data_array_release(savedProjectorList);
}

void PLSBasic::DeferSaveBegin()
{
	os_atomic_inc_long(&disableSaving);
}

void PLSBasic::DeferSaveEnd()
{
	long result = os_atomic_dec_long(&disableSaving);
	if (result == 0) {
		SaveProject();
	}
}

static void LoadAudioDevice(const char *name, int channel, obs_data_t *parent)
{
	obs_data_t *data = obs_data_get_obj(parent, name);
	if (!data)
		return;

	obs_source_t *source = obs_load_source(data);
	if (source) {
		obs_set_output_source(channel, source);
		obs_source_release(source);
	}

	obs_data_release(data);
}

static inline bool HasAudioDevices(const char *source_id)
{
	const char *output_id = source_id;
	obs_properties_t *props = obs_get_source_properties(output_id);
	size_t count = 0;

	if (!props)
		return false;

	obs_property_t *devices = obs_properties_get(props, "device_id");
	if (devices)
		count = obs_property_list_item_count(devices);

	obs_properties_destroy(props);

	return count != 0;
}

void PLSBasic::CreateFirstRunSources()
{
	bool hasDesktopAudio = HasAudioDevices(App()->OutputAudioSource());
	bool hasInputAudio = HasAudioDevices(App()->InputAudioSource());

	if (hasDesktopAudio)
		ResetAudioDevice(App()->OutputAudioSource(), "default", Str("Basic.DesktopDevice1"), 1);
	if (hasInputAudio)
		ResetAudioDevice(App()->InputAudioSource(), "default", Str("Basic.AuxDevice1"), 3);
}

void PLSBasic::CreateDefaultScene(bool firstStart)
{
	disableSaving++;

	ClearSceneData();
	InitDefaultTransitions();
	CreateDefaultQuickTransitions();
	ui->scenesFrame->SetTransitionDurationValue(SCENE_TRANSITION_DEFAULT_DURATION_VALUE);
	ui->scenesFrame->SetTransition(fadeTransition);

	obs_scene_t *scene = obs_scene_create(Str("Basic.Scene.first"));

	if (firstStart)
		CreateFirstRunSources();

	SetCurrentScene(scene, true);
	obs_scene_release(scene);

	disableSaving--;
}

static void ReorderSceneDisplayVecByName(const char *file, const char *name, SceneDisplayVector &reorderVector)
{
	SceneDisplayVector dataVec = PLSSceneDataMgr::Instance()->GetDisplayVector(file);
	for (auto iter = dataVec.begin(); iter != dataVec.end(); ++iter) {
		if (strcmp(name, iter->first.toStdString().c_str()) == 0) {
			reorderVector.emplace_back(SceneDisplayVector::value_type(name, iter->second));
			break;
		}
	}
}

void PLSBasic::LoadSceneListOrder(obs_data_array_t *array, const char *file)
{
	std::string file_base = strrchr(file, '/') + 1;
	file_base.erase(file_base.size() - 5, 5);

	size_t num = obs_data_array_count(array);

	SceneDisplayVector reorderVector;
	for (size_t i = 0; i < num; i++) {
		obs_data_t *data = obs_data_array_item(array, i);
		const char *name = obs_data_get_string(data, "name");

		ReorderSceneDisplayVecByName(file_base.c_str(), name, reorderVector);
		obs_data_release(data);
	}
	if (reorderVector.size() > 0 && reorderVector.size() == PLSSceneDataMgr::Instance()->GetSceneSize(file_base.c_str()))
		PLSSceneDataMgr::Instance()->SetDisplayVector(reorderVector, file_base.c_str());
	ui->scenesFrame->RefreshScene();
}

void PLSBasic::LoadSavedProjectors(obs_data_array_t *array)
{
	for (SavedProjectorInfo *info : savedProjectorsArray) {
		delete info;
	}
	savedProjectorsArray.clear();

	size_t num = obs_data_array_count(array);

	for (size_t i = 0; i < num; i++) {
		obs_data_t *data = obs_data_array_item(array, i);

		SavedProjectorInfo *info = new SavedProjectorInfo();
		info->monitor = obs_data_get_int(data, "monitor");
		info->type = static_cast<ProjectorType>(obs_data_get_int(data, "type"));
		info->geometry = std::string(obs_data_get_string(data, "geometry"));
		info->name = std::string(obs_data_get_string(data, "name"));
		savedProjectorsArray.emplace_back(info);

		obs_data_release(data);
	}
}

static void LogFilter(obs_source_t *, obs_source_t *filter, void *v_val)
{
	const char *name = obs_source_get_name(filter);
	const char *id = obs_source_get_id(filter);
	int val = (int)(intptr_t)v_val;
	string indent;

	for (int i = 0; i < val; i++)
		indent += "    ";

	blog(LOG_INFO, "%s- filter: '%s' (%s)", indent.c_str(), name, id);
}

static bool LogSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *v_val)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_id(source);
	int indent_count = (int)(intptr_t)v_val;
	string indent;

	for (int i = 0; i < indent_count; i++)
		indent += "    ";

	blog(LOG_INFO, "%s- source: '%s' (%s)", indent.c_str(), name, id);

	obs_monitoring_type monitoring_type = obs_source_get_monitoring_type(source);

	if (monitoring_type != OBS_MONITORING_TYPE_NONE) {
		const char *type = (monitoring_type == OBS_MONITORING_TYPE_MONITOR_ONLY) ? "monitor only" : "monitor and output";

		blog(LOG_INFO, "    %s- monitoring: %s", indent.c_str(), type);
	}
	int child_indent = 1 + indent_count;
	obs_source_enum_filters(source, LogFilter, (void *)(intptr_t)child_indent);
	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, LogSceneItem, (void *)(intptr_t)child_indent);
	return true;
}

void PLSBasic::LogScenes()
{
	blog(LOG_INFO, "------------------------------------------------");
	blog(LOG_INFO, "Loaded scenes:");

	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	for (auto iter = data.begin(); iter != data.end(); ++iter) {
		PLSSceneItemView *item = iter->second;
		if (!item) {
			continue;
		}
		OBSScene scene = item->GetData();

		obs_source_t *source = obs_scene_get_source(scene);
		const char *name = obs_source_get_name(source);

		blog(LOG_INFO, "- scene '%s':", name);
		obs_scene_enum_items(scene, LogSceneItem, (void *)(intptr_t)1);
		obs_source_enum_filters(source, LogFilter, (void *)(intptr_t)1);
	}

	blog(LOG_INFO, "------------------------------------------------");
}

bool obs_load_pld_callback(void *private_data)
{
	QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, FEED_UI_MAX_TIME);
	return true;
}

void PLSBasic::Load(const char *file)
{
	disableSaving++;

	obs_data_t *data = obs_data_create_from_json_file_safe(file, "bak");
	if (!data) {
		disableSaving--;
		blog(LOG_INFO, "No scene file found, creating default scene");
		CreateDefaultScene(true);
		SaveProject();
		return;
	}

	ClearSceneData();
	InitDefaultTransitions();

	obs_data_t *modulesObj = obs_data_get_obj(data, "modules");
	if (api)
		api->on_preload(modulesObj);

	obs_data_array_t *sceneOrder = obs_data_get_array(data, "scene_order");
	obs_data_array_t *sources = obs_data_get_array(data, "sources");
	obs_data_array_t *groups = obs_data_get_array(data, "groups");
	obs_data_array_t *transitions = obs_data_get_array(data, "transitions");
	const char *sceneName = obs_data_get_string(data, "current_scene");
	const char *programSceneName = obs_data_get_string(data, "current_program_scene");
	const char *transitionName = obs_data_get_string(data, "current_transition");

	if (!opt_starting_scene.empty()) {
		programSceneName = opt_starting_scene.c_str();
		if (!IsPreviewProgramMode())
			sceneName = opt_starting_scene.c_str();
	}

	int newDuration = obs_data_get_int(data, "transition_duration");
	if (!newDuration)
		newDuration = 300;

	if (!transitionName)
		transitionName = obs_source_get_name(fadeTransition);

	ui->scenesFrame->SetLoadTransitionsData(transitions, fadeTransition, newDuration, transitionName);
	obs_data_array_release(transitions);

	const char *curSceneCollection = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");

	obs_data_set_default_string(data, "name", curSceneCollection);

	const char *name = obs_data_get_string(data, "name");
	obs_source_t *curScene{};
	obs_source_t *curProgramScene{};
	//obs_source_t *curTransition;

	if (!name || !*name)
		name = curSceneCollection;

	std::string file_base = strrchr(file, '/') + 1;
	file_base.erase(file_base.size() - 5, 5);

	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollection", name);
	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile", file_base.c_str());

	LoadAudioDevice(DESKTOP_AUDIO_1, 1, data);
	LoadAudioDevice(DESKTOP_AUDIO_2, 2, data);
	LoadAudioDevice(AUX_AUDIO_1, 3, data);
	LoadAudioDevice(AUX_AUDIO_2, 4, data);
	LoadAudioDevice(AUX_AUDIO_3, 5, data);
	LoadAudioDevice(AUX_AUDIO_4, 6, data);

	if (!sources) {
		sources = groups;
		groups = nullptr;
	} else {
		obs_data_array_push_back_array(sources, groups);
	}

	obs_load_sources(sources, nullptr, obs_load_pld_callback, nullptr);

	if (sceneOrder) {
		LoadSceneListOrder(sceneOrder, file);
	}

retryScene:
	// set current scene
	if (sceneName && 0 != strcmp(sceneName, "")) {
		ui->scenesFrame->SetCurrentItem(sceneName);
		curScene = obs_get_source_by_name(sceneName);
	} else {
		PLSSceneItemView *currentItem = ui->scenesFrame->GetCurrentItem();
		if (currentItem) {
			curScene = obs_get_source_by_name(currentItem->GetName().toStdString().c_str());
		}
	}

	curProgramScene = obs_get_source_by_name(programSceneName);

	/* if the starting scene command line parameter is bad at all,
	 * fall back to original settings */
	if (!opt_starting_scene.empty() && (!curScene || !curProgramScene)) {
		sceneName = obs_data_get_string(data, "current_scene");
		programSceneName = obs_data_get_string(data, "current_program_scene");
		obs_source_release(curScene);
		obs_source_release(curProgramScene);
		opt_starting_scene.clear();
		goto retryScene;
	}

	if (!curProgramScene) {
		curProgramScene = curScene;
		obs_source_addref(curScene);
	}

	SetCurrentScene(curScene, true);
	if (IsPreviewProgramMode())
		TransitionToScene(curProgramScene, true);
	obs_source_release(curScene);
	obs_source_release(curProgramScene);

	obs_data_array_release(sources);
	obs_data_array_release(groups);
	obs_data_array_release(sceneOrder);

	/* ------------------- */

	bool projectorSave = config_get_bool(GetGlobalConfig(), "BasicWindow", "SaveProjectors");

	if (projectorSave) {
		obs_data_array_t *savedProjectors = obs_data_get_array(data, "saved_projectors");

		if (savedProjectors) {
			LoadSavedProjectors(savedProjectors);
			OpenSavedProjectors();
			activateWindow();
		}

		obs_data_array_release(savedProjectors);
	}

	/* ------------------- */

	bool previewLocked = obs_data_get_bool(data, "preview_locked");
	ui->preview->SetLocked(previewLocked);
	ui->actionLockPreview->setChecked(previewLocked);

	/* ---------------------- */

	bool fixedScaling = obs_data_get_bool(data, "scaling_enabled");
	int scalingLevel = (int)obs_data_get_int(data, "scaling_level");
	float scrollOffX = (float)obs_data_get_double(data, "scaling_off_x");
	float scrollOffY = (float)obs_data_get_double(data, "scaling_off_y");

	if (fixedScaling) {
		ui->preview->SetScalingLevel(scalingLevel);
		ui->preview->SetScrollingOffset(scrollOffX, scrollOffY);
	}
	ui->preview->SetFixedScaling(fixedScaling);

	/* ---------------------- */
	if (api)
		api->on_load(modulesObj);

	obs_data_release(modulesObj);
	obs_data_release(data);

	if (!opt_starting_scene.empty())
		opt_starting_scene.clear();

	if (opt_start_streaming) {
		blog(LOG_INFO, "Starting stream due to command line parameter");
		QMetaObject::invokeMethod(this, "StartStreaming", Qt::QueuedConnection);
		opt_start_streaming = false;
	}

	if (opt_start_recording) {
		blog(LOG_INFO, "Starting recording due to command line parameter");
		QMetaObject::invokeMethod(this, "StartRecording", Qt::QueuedConnection);
		opt_start_recording = false;
	}

	if (opt_start_replaybuffer) {
		QMetaObject::invokeMethod(this, "StartReplayBuffer", Qt::QueuedConnection);
		opt_start_replaybuffer = false;
	}

	copyString = nullptr;
	copyFiltersString = nullptr;

	LogScenes();

	disableSaving--;

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED);
	}
}

#define SERVICE_PATH "service.json"

void PLSBasic::SaveService()
{
	if (!service)
		return;

	char serviceJsonPath[512];
	int ret = GetProfilePath(serviceJsonPath, sizeof(serviceJsonPath), SERVICE_PATH);
	if (ret <= 0)
		return;

	obs_data_t *data = obs_data_create();
	obs_data_t *settings = obs_service_get_settings(service);

	obs_data_set_string(data, "type", obs_service_get_type(service));
	obs_data_set_obj(data, "settings", settings);

	if (!obs_data_save_json_safe(data, serviceJsonPath, "tmp", "bak"))
		blog(LOG_WARNING, "Failed to save service");

	obs_data_release(settings);
	obs_data_release(data);
}

bool PLSBasic::LoadService()
{
	const char *type;

	char serviceJsonPath[512];
	int ret = GetProfilePath(serviceJsonPath, sizeof(serviceJsonPath), SERVICE_PATH);
	if (ret <= 0)
		return false;

	obs_data_t *data = obs_data_create_from_json_file_safe(serviceJsonPath, "bak");

	obs_data_set_default_string(data, "type", "rtmp_common");
	type = obs_data_get_string(data, "type");

	obs_data_t *settings = obs_data_get_obj(data, "settings");
	obs_data_t *hotkey_data = obs_data_get_obj(data, "hotkeys");

	service = obs_service_create(type, "default_service", settings, hotkey_data);
	obs_service_release(service);

	obs_data_release(hotkey_data);
	obs_data_release(settings);
	obs_data_release(data);

	return !!service;
}

bool PLSBasic::InitService()
{
	ProfileScope("PLSBasic::InitService");

	if (LoadService())
		return true;

	service = obs_service_create("rtmp_common", "default_service", nullptr, nullptr);
	if (!service)
		return false;
	obs_service_release(service);

	return true;
}

static const double scaled_vals[] = {1.0, 1.25, (1.0 / 0.75), 1.5, (1.0 / 0.6), 1.75, 2.0, 2.25, 2.5, 2.75, 3.0, 0.0};

extern void CheckExistingCookieId();

bool PLSBasic::InitBasicConfigDefaults()
{
	QList<QScreen *> screens = QGuiApplication::screens();

	if (!screens.size()) {
		PLSErrorBox(NULL, "There appears to be no monitors.  Er, this "
				  "technically shouldn't be possible.");
		return false;
	}

	QScreen *primaryScreen = QGuiApplication::primaryScreen();

	uint32_t cx = primaryScreen->size().width();
	uint32_t cy = primaryScreen->size().height();

	bool oldResolutionDefaults = config_get_bool(App()->GlobalConfig(), "General", "Pre19Defaults");

	/* use 1920x1080 for new default base res if main monitor is above
	 * 1920x1080, but don't apply for people from older builds -- only to
	 * new users */
	if (!oldResolutionDefaults && (cx * cy) > (1920 * 1080)) {
		cx = 1920;
		cy = 1080;
	}

	bool changed = false;

	/* ----------------------------------------------------- */
	/* move over old FFmpeg track settings                   */
	if (config_has_user_value(basicConfig, "AdvOut", "FFAudioTrack") && !config_has_user_value(basicConfig, "AdvOut", "Pre22.1Settings")) {

		int track = (int)config_get_int(basicConfig, "AdvOut", "FFAudioTrack");
		config_set_int(basicConfig, "AdvOut", "FFAudioMixes", 1LL << (track - 1));
		config_set_bool(basicConfig, "AdvOut", "Pre22.1Settings", true);
		changed = true;
	}

	/* ----------------------------------------------------- */
	/* move over mixer values in advanced if older config */
	if (config_has_user_value(basicConfig, "AdvOut", "RecTrackIndex") && !config_has_user_value(basicConfig, "AdvOut", "RecTracks")) {

		uint64_t track = config_get_uint(basicConfig, "AdvOut", "RecTrackIndex");
		track = 1ULL << (track - 1);
		config_set_uint(basicConfig, "AdvOut", "RecTracks", track);
		config_remove_value(basicConfig, "AdvOut", "RecTrackIndex");
		changed = true;
	}

	/* ----------------------------------------------------- */
	/* set twitch chat extensions to "both" if prev version  */
	/* is under 24.1                                         */
	if (config_get_bool(GetGlobalConfig(), "General", "Pre24.1Defaults") && !config_has_user_value(basicConfig, "Twitch", "AddonChoice")) {
		config_set_int(basicConfig, "Twitch", "AddonChoice", 3);
		changed = true;
	}

	/* ----------------------------------------------------- */

	if (changed)
		config_save_safe(basicConfig, "tmp", nullptr);

	/* ----------------------------------------------------- */

	config_set_default_string(basicConfig, "Output", "Mode", "Simple");

	config_set_default_string(basicConfig, "SimpleOutput", "FilePath", GetDefaultVideoSavePath().c_str());
	config_set_default_string(basicConfig, "SimpleOutput", "RecFormat", "flv");
	config_set_default_uint(basicConfig, "SimpleOutput", "VBitrate", 6000);
	config_set_default_uint(basicConfig, "SimpleOutput", "ABitrate", 160);
	config_set_default_bool(basicConfig, "SimpleOutput", "UseAdvanced", false);
	config_set_default_bool(basicConfig, "SimpleOutput", "EnforceBitrate", true);
	config_set_default_string(basicConfig, "SimpleOutput", "Preset", "veryfast");
	config_set_default_string(basicConfig, "SimpleOutput", "NVENCPreset", "hq");
	config_set_default_string(basicConfig, "SimpleOutput", "RecQuality", "Small");
	config_set_default_bool(basicConfig, "SimpleOutput", "RecRB", false);
	config_set_default_int(basicConfig, "SimpleOutput", "RecRBTime", 20);
	config_set_default_int(basicConfig, "SimpleOutput", "RecRBSize", 512);
	config_set_default_string(basicConfig, "SimpleOutput", "RecRBPrefix", "Replay");

	config_set_default_bool(basicConfig, "AdvOut", "ApplyServiceSettings", true);
	config_set_default_bool(basicConfig, "AdvOut", "UseRescale", false);
	config_set_default_uint(basicConfig, "AdvOut", "TrackIndex", 1);
	config_set_default_string(basicConfig, "AdvOut", "Encoder", "obs_x264");

	config_set_default_string(basicConfig, "AdvOut", "RecType", "Standard");

	config_set_default_string(basicConfig, "AdvOut", "RecFilePath", GetDefaultVideoSavePath().c_str());
	config_set_default_string(basicConfig, "AdvOut", "RecFormat", "flv");
	config_set_default_bool(basicConfig, "AdvOut", "RecUseRescale", false);
	config_set_default_uint(basicConfig, "AdvOut", "RecTracks", (1 << 0));
	config_set_default_string(basicConfig, "AdvOut", "RecEncoder", "none");

	config_set_default_bool(basicConfig, "AdvOut", "FFOutputToFile", true);
	config_set_default_string(basicConfig, "AdvOut", "FFFilePath", GetDefaultVideoSavePath().c_str());
	config_set_default_string(basicConfig, "AdvOut", "FFExtension", "mp4");
	config_set_default_uint(basicConfig, "AdvOut", "FFVBitrate", 2500);
	config_set_default_uint(basicConfig, "AdvOut", "FFVGOPSize", 250);
	config_set_default_bool(basicConfig, "AdvOut", "FFUseRescale", false);
	config_set_default_bool(basicConfig, "AdvOut", "FFIgnoreCompat", false);
	config_set_default_uint(basicConfig, "AdvOut", "FFABitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "FFAudioMixes", 1);

	config_set_default_uint(basicConfig, "AdvOut", "Track1Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track2Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track3Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track4Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track5Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track6Bitrate", 160);

	config_set_default_bool(basicConfig, "AdvOut", "RecRB", false);
	config_set_default_uint(basicConfig, "AdvOut", "RecRBTime", 20);
	config_set_default_int(basicConfig, "AdvOut", "RecRBSize", 512);

	config_set_default_uint(basicConfig, "Video", "BaseCX", cx);
	config_set_default_uint(basicConfig, "Video", "BaseCY", cy);

	/* don't allow BaseCX/BaseCY to be susceptible to defaults changing */
	if (!config_has_user_value(basicConfig, "Video", "BaseCX") || !config_has_user_value(basicConfig, "Video", "BaseCY")) {
		config_set_uint(basicConfig, "Video", "BaseCX", cx);
		config_set_uint(basicConfig, "Video", "BaseCY", cy);
		config_save_safe(basicConfig, "tmp", nullptr);
	}

	config_set_default_string(basicConfig, "Output", "FilenameFormatting", "%CCYY-%MM-%DD %hh-%mm-%ss");

	config_set_default_bool(basicConfig, "Output", "DelayEnable", false);
	config_set_default_uint(basicConfig, "Output", "DelaySec", 20);
	config_set_default_bool(basicConfig, "Output", "DelayPreserve", true);

	config_set_default_bool(basicConfig, "Output", "Reconnect", true);
	config_set_default_uint(basicConfig, "Output", "RetryDelay", 10);
	config_set_default_uint(basicConfig, "Output", "MaxRetries", 20);

	config_set_default_string(basicConfig, "Output", "BindIP", "default");
	config_set_default_bool(basicConfig, "Output", "NewSocketLoopEnable", false);
	config_set_default_bool(basicConfig, "Output", "LowLatencyEnable", false);

	int i = 0;
	uint32_t scale_cx = cx;
	uint32_t scale_cy = cy;

	/* use a default scaled resolution that has a pixel count no higher
	 * than 1280x720 */
	while (((scale_cx * scale_cy) > (1280 * 720)) && scaled_vals[i] > 0.0) {
		double scale = scaled_vals[i++];
		scale_cx = uint32_t(double(cx) / scale);
		scale_cy = uint32_t(double(cy) / scale);
	}

	config_set_default_uint(basicConfig, "Video", "OutputCX", scale_cx);
	config_set_default_uint(basicConfig, "Video", "OutputCY", scale_cy);

	/* don't allow OutputCX/OutputCY to be susceptible to defaults
	 * changing */
	if (!config_has_user_value(basicConfig, "Video", "OutputCX") || !config_has_user_value(basicConfig, "Video", "OutputCY")) {
		if ((cx * cy) >= (1920 * 1080)) {
			config_set_uint(basicConfig, "Video", "OutputCX", 1920);
			config_set_uint(basicConfig, "Video", "OutputCY", 1080);
		} else {
			config_set_uint(basicConfig, "Video", "OutputCX", scale_cx);
			config_set_uint(basicConfig, "Video", "OutputCY", scale_cy);
		}
		config_save_safe(basicConfig, "tmp", nullptr);
	}

	config_set_default_uint(basicConfig, "Video", "FPSType", 0);
	config_set_default_string(basicConfig, "Video", "FPSCommon", "30");
	config_set_default_uint(basicConfig, "Video", "FPSInt", 30);
	config_set_default_uint(basicConfig, "Video", "FPSNum", 30);
	config_set_default_uint(basicConfig, "Video", "FPSDen", 1);
	config_set_default_string(basicConfig, "Video", "ScaleType", "bicubic");
	config_set_default_string(basicConfig, "Video", "ColorFormat", "NV12");
	config_set_default_string(basicConfig, "Video", "ColorSpace", "601");
	config_set_default_string(basicConfig, "Video", "ColorRange", "Partial");

	config_set_default_string(basicConfig, "Audio", "MonitoringDeviceId", "default");
	config_set_default_string(basicConfig, "Audio", "MonitoringDeviceName",
				  Str("Basic.Settings.Advanced.Audio.MonitoringDevice"
				      ".Default"));
	config_set_default_uint(basicConfig, "Audio", "SampleRate", 44100);
	config_set_default_string(basicConfig, "Audio", "ChannelSetup", "Stereo");
	config_set_default_double(basicConfig, "Audio", "MeterDecayRate", VOLUME_METER_DECAY_FAST);
	config_set_default_uint(basicConfig, "Audio", "PeakMeterType", 0);

	config_set_default_string(basicConfig, "Hotkeys", "ReplayBuffer", "{\"ReplayBuffer.Save\":[{\"alt\":true,\"key\":\"OBS_KEY_R\"}]}");

	config_set_default_string(basicConfig, "Others", "Hotkeys.ReplayBuffer", "Alt+R");

	CheckExistingCookieId();

	return true;
}

extern bool EncoderAvailable(const char *encoder);

void PLSBasic::InitBasicConfigDefaults2()
{
	bool oldEncDefaults = config_get_bool(App()->GlobalConfig(), "General", "Pre23Defaults");
	bool useNV = EncoderAvailable("ffmpeg_nvenc") && !oldEncDefaults;

	config_set_default_string(basicConfig, "SimpleOutput", "StreamEncoder", useNV ? SIMPLE_ENCODER_NVENC : SIMPLE_ENCODER_X264);
	config_set_default_string(basicConfig, "SimpleOutput", "RecEncoder", useNV ? SIMPLE_ENCODER_NVENC : SIMPLE_ENCODER_X264);
}

bool PLSBasic::InitBasicConfig()
{
	ProfileScope("PLSBasic::InitBasicConfig");

	char configPath[512];

	int ret = GetProfilePath(configPath, sizeof(configPath), "");
	if (ret <= 0) {
		PLSErrorBox(nullptr, "Failed to get profile path");
		return false;
	}

	if (os_mkdir(configPath) == MKDIR_ERROR) {
		PLSErrorBox(nullptr, "Failed to create profile path");
		return false;
	}

	ret = GetProfilePath(configPath, sizeof(configPath), "basic.ini");
	if (ret <= 0) {
		PLSErrorBox(nullptr, "Failed to get base.ini path");
		return false;
	}

	int code = basicConfig.Open(configPath, CONFIG_OPEN_ALWAYS);
	if (code != CONFIG_SUCCESS) {
		PLSErrorBox(NULL, "Failed to open basic.ini: %d", code);
		return false;
	}

	if (config_get_string(basicConfig, "General", "Name") == nullptr) {
		const char *curName = config_get_string(App()->GlobalConfig(), "Basic", "Profile");

		config_set_string(basicConfig, "General", "Name", curName);
		basicConfig.SaveSafe("tmp");
	}

	return InitBasicConfigDefaults();
}

void PLSBasic::InitPLSCallbacks()
{
	ProfileScope("PLSBasic::InitPLSCallbacks");

	signalHandlers.reserve(signalHandlers.size() + 6);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_create", PLSBasic::SourceCreated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_remove", PLSBasic::SourceRemoved, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_activate", PLSBasic::SourceActivated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_deactivate", PLSBasic::SourceDeactivated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_audio_activate", PLSBasic::SourceAudioActivated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_audio_deactivate", PLSBasic::SourceAudioDeactivated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_rename", PLSBasic::SourceRenamed, this);
}

void PLSBasic::InitPrimitives()
{
	ProfileScope("PLSBasic::InitPrimitives");

	obs_enter_graphics();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(1.0f, 1.0f);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(0.0f, 0.0f);
	box = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	boxLeft = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(1.0f, 0.0f);
	boxTop = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(1.0f, 1.0f);
	boxRight = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(1.0f, 1.0f);
	boxBottom = gs_render_save();

	gs_render_start(true);
	for (int i = 0; i <= 360; i += (360 / 20)) {
		float pos = RAD(float(i));
		gs_vertex2f(cosf(pos), sinf(pos));
	}
	circle = gs_render_save();

	obs_leave_graphics();
}

void PLSBasic::initMainMenu()
{
	connect(mainMenu, &PLSPopupMenu::shown, this, [this]() {
		bool hasSelectedSource = GetTopSelectedSourceItem() >= 0;
		ui->actionCopySource->setEnabled(hasSelectedSource);
		ui->actionCopyFilters->setEnabled(hasSelectedSource);
	});
}

void PLSBasic::ReplayBufferClicked()
{
	if (outputHandler->ReplayBufferActive())
		StopReplayBuffer();
	else
		StartReplayBuffer();
};

void PLSBasic::ResetOutputs()
{
	ProfileScope("PLSBasic::ResetOutputs");

	const char *mode = config_get_string(basicConfig, "Output", "Mode");
	bool advOut = astrcmpi(mode, "Advanced") == 0;

	if (!outputHandler || !outputHandler->Active()) {
		outputHandler.reset();
		outputHandler.reset(advOut ? CreateAdvancedOutputHandler(this) : CreateSimpleOutputHandler(this));

		//delete replayBufferButton;

		//if (outputHandler->replayBuffer) {
		//replayBufferButton = new QPushButton(QTStr("Basic.Main.StartReplayBuffer"), this);
		//replayBufferButton->setCheckable(true);
		//connect(replayBufferButton.data(), &QPushButton::clicked, this, &PLSBasic::ReplayBufferClicked);

		//replayBufferButton->setProperty("themeID", "replayBufferButton");
		//ui->buttonsVLayout->insertWidget(2, replayBufferButton);
		//}

		/*if (sysTrayReplayBuffer)
			sysTrayReplayBuffer->setEnabled(!!outputHandler->replayBuffer);*/
	} else {
		outputHandler->Update();
	}
}

static void AddProjectorMenuMonitors(QMenu *parent, QObject *target, const char *slot);

#define STARTUP_SEPARATOR "==== Startup complete ==============================================="
#define SHUTDOWN_SEPARATOR "==== Shutting down =================================================="

#define UNSUPPORTED_ERROR                                                     \
	"Failed to initialize video:\n\nRequired graphics API functionality " \
	"not found.  Your GPU may not be supported."

#define UNKNOWN_ERROR                                                  \
	"Failed to initialize video.  Your GPU may not be supported, " \
	"or your graphics drivers may need to be updated."

bool PLSBasic::PLSInit()
{
	ProfileScope("PLSBasic::PLSInit");

	const char *sceneCollection = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");
	char savePath[512];
	char fileName[512];
	int ret;

	if (!sceneCollection)
		throw "Failed to get scene collection name";

	ret = snprintf(fileName, 512, "PRISMLiveStudio/basic/scenes/%s.json", sceneCollection);
	if (ret <= 0)
		throw "Failed to create scene collection file name";

	ret = GetConfigPath(savePath, sizeof(savePath), fileName);
	if (ret <= 0)
		throw "Failed to get scene collection json file path";

	if (!InitBasicConfig())
		throw "Failed to load basic.ini";
	if (!ResetAudio())
		throw "Failed to initialize audio";

	ret = ResetVideo();

	switch (ret) {
	case OBS_VIDEO_MODULE_NOT_FOUND:
		throw "Failed to initialize video:  Graphics module not found";
	case OBS_VIDEO_NOT_SUPPORTED:
		throw UNSUPPORTED_ERROR;
	case OBS_VIDEO_INVALID_PARAM:
		throw "Failed to initialize video:  Invalid parameters";
	default:
		if (ret != OBS_VIDEO_SUCCESS)
			throw UNKNOWN_ERROR;
	}

	/* load audio monitoring */
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	const char *device_name = config_get_string(basicConfig, "Audio", "MonitoringDeviceName");
	const char *device_id = config_get_string(basicConfig, "Audio", "MonitoringDeviceId");

	obs_set_audio_monitoring_device(device_name, device_id);

	blog(LOG_INFO, "Audio monitoring device:\n\tname: %s\n\tid: %s", device_name, device_id);
#endif

	InitPLSCallbacks();

	/**************************/
	InitHotkeys();

	AddExtraModulePaths();
	obs_add_module_path("prism-plugins/", "data/prism-plugins/%module%");
	blog(LOG_INFO, "---------------------------------");
	obs_load_all_modules();
	blog(LOG_INFO, "---------------------------------");
	obs_log_loaded_modules();
	blog(LOG_INFO, "---------------------------------");
	obs_post_load_modules();

	//ui->controlsDock->hide();
#ifdef BROWSER_AVAILABLE
	cef = obs_browser_init_panel();
#endif

	InitBasicConfigDefaults2();

	CheckForSimpleModeX264Fallback();

	blog(LOG_INFO, STARTUP_SEPARATOR);

	ResetOutputs();
	CreateHotkeys();

	if (!InitService())
		throw "Failed to initialize service";

	InitPrimitives();

	/*
	Because we are using fixed values, so here we ignore the logic about setting values.
	sceneDuplicationMode = config_get_bool(App()->GlobalConfig(), "BasicWindow", "SceneDuplicationMode");
	swapScenesMode = config_get_bool(App()->GlobalConfig(), "BasicWindow", "SwapScenesMode");
	editPropertiesMode = config_get_bool(App()->GlobalConfig(), "BasicWindow", "EditPropertiesMode");
	*/

	if (!opt_studio_mode) {
		bool isValue = config_has_user_value(App()->GlobalConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_PREVIEW_MODE_MODULE);
		bool enabled = true;
		if (isValue) {
			enabled = config_get_bool(App()->GlobalConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_PREVIEW_MODE_MODULE);
		} else {
			config_set_bool(App()->GlobalConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_PREVIEW_MODE_MODULE, enabled);
		}
		SetPreviewProgramMode(enabled);
	} else {
		SetPreviewProgramMode(true);
		opt_studio_mode = false;
	}

#define SET_VISIBILITY(name, control)                                                               \
	do {                                                                                        \
		if (config_has_user_value(App()->GlobalConfig(), "BasicWindow", name)) {            \
			bool visible = config_get_bool(App()->GlobalConfig(), "BasicWindow", name); \
			ui->control->setChecked(visible);                                           \
		}                                                                                   \
	} while (false)

	SET_VISIBILITY("ShowListboxToolbars", toggleListboxToolbars);
	SET_VISIBILITY("ShowStatusBar", toggleStatusBar);
#undef SET_VISIBILITY

	{
		ProfileScope("PLSBasic::Load");
		disableSaving--;
		Load(savePath);
		disableSaving++;
	}

	loaded = true;

	previewEnabled = config_get_bool(App()->GlobalConfig(), "BasicWindow", "PreviewEnabled");

	if (!previewEnabled && !IsPreviewProgramMode())
		QMetaObject::invokeMethod(this, "EnablePreviewDisplay", Qt::QueuedConnection, Q_ARG(bool, previewEnabled));

#ifdef _WIN32
	uint32_t winVer = GetWindowsVersion();
	if (winVer > 0 && winVer < 0x602) {
		bool disableAero = config_get_bool(basicConfig, "Video", "DisableAero");
		SetAeroEnabled(!disableAero);
	}
#endif

	RefreshSceneCollections();
	RefreshProfiles();
	disableSaving--;

	auto addDisplay = [this](PLSQTDisplay *window) {
		obs_display_add_draw_callback(window->GetDisplay(), PLSBasic::RenderMain, this);

		struct obs_video_info ovi;
		if (obs_get_video_info(&ovi))
			ResizePreview(ovi.base_width, ovi.base_height);
	};

	connect(ui->preview, &PLSQTDisplay::DisplayCreated, addDisplay);
	connect(ui->scenesFrame, &PLSSceneListView::LogoutEvent, this, &PLSBasic::OnLogoutEvent);

	// scene action
	QAction *actionAdd = new QAction();
	actionAdd->setObjectName(OBJECT_NMAE_ADD_BUTTON);
	QAction *actionSwitchEffect = new QAction();
	actionSwitchEffect->setObjectName(OBJECT_NMAE_SWITCH_EFFECT_BUTTON);
	actionSeperateScene = new QAction(ui->scenesFrame);
	SetAttachWindowBtnText(actionSeperateScene, ui->scenesDock->isFloating());

	connect(actionAdd, &QAction::triggered, ui->scenesFrame, &PLSSceneListView::OnAddSceneButtonClicked);
	connect(actionSwitchEffect, &QAction::triggered, ui->scenesFrame, &PLSSceneListView::OnSceneSwitchEffectBtnClicked);

	ui->scenesDock->titleWidget()->setAdvButtonActions({actionSeperateScene});
	ui->scenesDock->titleWidget()->setButtonActions({actionAdd, actionSwitchEffect});
	connect(ui->scenesDock, &QDockWidget::topLevelChanged, this, &PLSBasic::OnSceneDockTopLevelChanged);
	connect(actionSeperateScene, &QAction::triggered, this, &PLSBasic::OnSceneDockSeperatedBtnClicked);

	// source action
	QAction *actionAddSource = new QAction();
	actionAddSource->setObjectName(OBJECT_NMAE_ADD_SOURCE_BUTTON);
	actionSeperateSource = new QAction();
	SetAttachWindowBtnText(actionSeperateSource, ui->sourcesDock->isFloating());

	connect(actionAddSource, &QAction::triggered, this, &PLSBasic::on_actionAddSource_triggered);
	connect(actionSeperateSource, &QAction::triggered, this, &PLSBasic::OnSourceDockSeperatedBtnClicked);

	ui->sourcesDock->titleWidget()->setAdvButtonActions({actionSeperateSource});
	ui->sourcesDock->titleWidget()->setButtonActions({actionAddSource});
	connect(ui->sourcesDock, &QDockWidget::topLevelChanged, this, &PLSBasic::OnSourceDockTopLevelChanged);

	//audio mixer
	QList<QAction *> l;
	QAction *actionAudioAdvance = new QAction(QTStr("Basic.MainMenu.Edit.AdvAudio"));
	actionAudioAdvance->setObjectName("audioAdvanced");
	connect(actionAudioAdvance, &QAction::triggered, this, &PLSBasic::on_actionAdvAudioProperties_triggered, Qt::DirectConnection);
	l.push_back(actionAudioAdvance);

	actionSperateMixer = new QAction();
	SetAttachWindowBtnText(actionSperateMixer, ui->mixerDock->isFloating());

	connect(actionSperateMixer, &QAction::triggered, this, &PLSBasic::onMixerDockSeperateBtnClicked);
	connect(ui->mixerDock, &QDockWidget::topLevelChanged, this, &PLSBasic::onMixerDockLocationChanged);

	ui->mixerDock->titleWidget()->setAdvButtonActions({actionSperateMixer});
	ui->mixerDock->titleWidget()->setButtonActions(l);
	// add by zzc
	initChannelUI();
#ifdef _WIN32
	SetWin32DropStyle(this);
	bool willShow = this->willShow();

#endif

	connect(mainView, &PLSMainView::isshowSignal, mainView, [this](bool isShow) {
		if (isShow) {
			bool alwaysOnTop = config_get_bool(App()->GlobalConfig(), "BasicWindow", "AlwaysOnTop");
			if (alwaysOnTop || opt_always_on_top) {
				SetAlwaysOnTop(mainView, MAIN_FRAME, true);
				ui->actionAlwaysOnTop->setChecked(true);
			}
		}
	});

#ifndef _WIN32
	bool willShow = willShow();
#endif

	const char *dockStateStr = config_get_string(App()->GlobalConfig(), "BasicWindow", "DockState");
	if (!dockStateStr) {
		on_resetUI_triggered();
	} else {
		QByteArray dockState = QByteArray::fromBase64(QByteArray(dockStateStr));
		if (!restoreState(dockState))
			on_resetUI_triggered();
	}

	bool pre23Defaults = config_get_bool(App()->GlobalConfig(), "General", "Pre23Defaults");
	if (pre23Defaults) {
		bool resetDockLock23 = config_get_bool(App()->GlobalConfig(), "General", "ResetDockLock23");
		if (!resetDockLock23) {
			config_set_bool(App()->GlobalConfig(), "General", "ResetDockLock23", true);
			config_remove_value(App()->GlobalConfig(), "BasicWindow", "DocksLocked");
			config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
		}
	}

	bool docksLocked = config_get_bool(App()->GlobalConfig(), "BasicWindow", "DocksLocked");
	on_lockUI_toggled(docksLocked);
	ui->lockUI->blockSignals(true);
	ui->lockUI->setChecked(docksLocked);
	ui->lockUI->blockSignals(false);

	// main menu process dock attach/detach
	assignDockToggle(this, ui->scenesDock, ui->actionScenesAttachOrDetach, "Scenes Dock");
	assignDockToggle(this, ui->sourcesDock, ui->actionSourcesAttachOrDetach, "Sources Dock");
	assignDockToggle(this, ui->mixerDock, ui->actionMixerAttachOrDetach, "Audio Mixer Dock");

	if (willShow) {
		mainView->show();
	}

#ifndef __APPLE__
	SystemTray(true);
#endif

	bool has_last_version = config_has_user_value(App()->GlobalConfig(), "General", "LastVersion");
	bool first_run = config_get_bool(App()->GlobalConfig(), "General", "FirstRun");

	if (!first_run) {
		config_set_bool(App()->GlobalConfig(), "General", "FirstRun", true);
		config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
	}

	// remove first start show config wizard
	// if (!first_run && !has_last_version && !Active()) {
	// 	QString msg;
	// 	msg = QTStr("Basic.FirstStartup.RunWizard");
	// 	if (PLSMessageBox::question(this, QTStr("Basic.AutoConfig"), msg) == PLSAlertView::Button::Yes) {
	// 		QMetaObject::invokeMethod(this, "on_autoConfigure_triggered", Qt::QueuedConnection);
	// 	} else {
	// 		msg = QTStr("Basic.FirstStartup.RunWizard.NoClicked");
	// 		PLSMessageBox::information(this, QTStr("Basic.AutoConfig"), msg);
	// 	}
	// }

	PLSBasicStats::InitializeValues();

	/* ----------------------- */
	/* Add multiview menu      */
	ui->menuMultiview->addAction(QTStr("Windowed"), this, SLOT(OpenMultiviewWindow()));
	multiviewProjectorMenu = new QMenu(QTStr("Fullscreen"));
	ui->menuMultiview->addMenu(multiviewProjectorMenu);
	AddProjectorMenuMonitors(multiviewProjectorMenu, this, SLOT(OpenMultiviewProjector()));
	connect(ui->menuMultiview->menuAction(), &QAction::hovered, this, &PLSBasic::UpdateMultiviewProjectorMenu);

	ui->sources->UpdateIcons();

#if !defined(_WIN32) && !defined(__APPLE__)
	delete ui->actionShowCrashLogs;
	delete ui->actionUploadLastCrashLog;
	delete ui->menuCrashLogs;
	delete ui->actionCheckForUpdates;
	ui->actionShowCrashLogs = nullptr;
	ui->actionUploadLastCrashLog = nullptr;
	ui->menuCrashLogs = nullptr;
	ui->actionCheckForUpdates = nullptr;
#endif

	OnFirstLoad();

#ifdef __APPLE__
	QMetaObject::invokeMethod(this, "DeferredSysTrayLoad", Qt::QueuedConnection, Q_ARG(int, 10));
#endif
	return willShow;
}

void PLSBasic::OnFirstLoad()
{
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_FINISHED_LOADING);

	Auth::Load();
}

void PLSBasic::DeferredSysTrayLoad(int requeueCount)
{
	if (--requeueCount > 0) {
		QMetaObject::invokeMethod(this, "DeferredSysTrayLoad", Qt::QueuedConnection, Q_ARG(int, requeueCount));
		return;
	}

	/* Minimizng to tray on initial startup does not work on mac
	 * unless it is done in the deferred load */
	SystemTray(true);
}

void PLSBasic::UpdateMultiviewProjectorMenu()
{
	multiviewProjectorMenu->clear();
	AddProjectorMenuMonitors(multiviewProjectorMenu, this, SLOT(OpenMultiviewProjector()));
}

void PLSBasic::InitHotkeys()
{
	ProfileScope("PLSBasic::InitHotkeys");

	struct obs_hotkeys_translations t = {};
	t.insert = Str("Hotkeys.Insert");
	t.del = Str("Hotkeys.Delete");
	t.home = Str("Hotkeys.Home");
	t.end = Str("Hotkeys.End");
	t.page_up = Str("Hotkeys.PageUp");
	t.page_down = Str("Hotkeys.PageDown");
	t.num_lock = Str("Hotkeys.NumLock");
	t.scroll_lock = Str("Hotkeys.ScrollLock");
	t.caps_lock = Str("Hotkeys.CapsLock");
	t.backspace = Str("Hotkeys.Backspace");
	t.tab = Str("Hotkeys.Tab");
	t.print = Str("Hotkeys.Print");
	t.pause = Str("Hotkeys.Pause");
	t.left = Str("Hotkeys.Left");
	t.right = Str("Hotkeys.Right");
	t.up = Str("Hotkeys.Up");
	t.down = Str("Hotkeys.Down");
#ifdef _WIN32
	t.meta = Str("Hotkeys.Windows");
#else
	t.meta = Str("Hotkeys.Super");
#endif
	t.menu = Str("Hotkeys.Menu");
	t.space = Str("Hotkeys.Space");
	t.numpad_num = Str("Hotkeys.NumpadNum");
	t.numpad_multiply = Str("Hotkeys.NumpadMultiply");
	t.numpad_divide = Str("Hotkeys.NumpadDivide");
	t.numpad_plus = Str("Hotkeys.NumpadAdd");
	t.numpad_minus = Str("Hotkeys.NumpadSubtract");
	t.numpad_decimal = Str("Hotkeys.NumpadDecimal");
	t.apple_keypad_num = Str("Hotkeys.AppleKeypadNum");
	t.apple_keypad_multiply = Str("Hotkeys.AppleKeypadMultiply");
	t.apple_keypad_divide = Str("Hotkeys.AppleKeypadDivide");
	t.apple_keypad_plus = Str("Hotkeys.AppleKeypadAdd");
	t.apple_keypad_minus = Str("Hotkeys.AppleKeypadSubtract");
	t.apple_keypad_decimal = Str("Hotkeys.AppleKeypadDecimal");
	t.apple_keypad_equal = Str("Hotkeys.AppleKeypadEqual");
	t.mouse_num = Str("Hotkeys.MouseButton");
	t.escape = Str("Hotkeys.Escape");
	obs_hotkeys_set_translations(&t);

	obs_hotkeys_set_audio_hotkeys_translations(Str("Mute"), Str("Unmute"), Str("Push-to-mute"), Str("Push-to-talk"));

	obs_hotkeys_set_sceneitem_hotkeys_translations(Str("SceneItemShow"), Str("SceneItemHide"));

	obs_hotkey_enable_callback_rerouting(true);
	obs_hotkey_set_callback_routing_func(PLSBasic::HotkeyTriggered, this);
}

void PLSBasic::ProcessHotkey(obs_hotkey_id id, bool pressed)
{
	obs_hotkey_trigger_routed_callback(id, pressed);
}

void PLSBasic::HotkeyTriggered(void *data, obs_hotkey_id id, bool pressed)
{
	PLSBasic &basic = *static_cast<PLSBasic *>(data);
	QMetaObject::invokeMethod(&basic, "ProcessHotkey", Q_ARG(obs_hotkey_id, id), Q_ARG(bool, pressed));
}

void PLSBasic::CreateHotkeys()
{
	ProfileScope("PLSBasic::CreateHotkeys");

	auto LoadHotkeyData = [&](const char *name) -> OBSData {
		const char *info = config_get_string(basicConfig, "Hotkeys", name);
		if (!info)
			return {};

		obs_data_t *data = obs_data_create_from_json(info);
		if (!data)
			return {};

		OBSData res = data;
		obs_data_release(data);
		return res;
	};

	auto LoadHotkey = [&](obs_hotkey_id id, const char *name) {
		obs_data_array_t *array = obs_data_get_array(LoadHotkeyData(name), "bindings");

		obs_hotkey_load(id, array);
		obs_data_array_release(array);
	};

	auto LoadHotkeyPair = [&](obs_hotkey_pair_id id, const char *name0, const char *name1) {
		obs_data_array_t *array0 = obs_data_get_array(LoadHotkeyData(name0), "bindings");
		obs_data_array_t *array1 = obs_data_get_array(LoadHotkeyData(name1), "bindings");

		obs_hotkey_pair_load(id, array0, array1);
		obs_data_array_release(array0);
		obs_data_array_release(array1);
	};

#define MAKE_CALLBACK(pred, method, log_action)                            \
	[](void *data, obs_hotkey_pair_id, obs_hotkey_t *, bool pressed) { \
		PLSBasic &basic = *static_cast<PLSBasic *>(data);          \
		if ((pred) && pressed) {                                   \
			blog(LOG_INFO, log_action " due to hotkey");       \
			method();                                          \
			return true;                                       \
		}                                                          \
		return false;                                              \
	}

	streamingHotkeys = obs_hotkey_pair_register_frontend(
		"PLSBasic.StartStreaming", Str("Basic.Main.StartStreaming"), "PLSBasic.StopStreaming", Str("Basic.Main.StopStreaming"),
		MAKE_CALLBACK(!basic.outputHandler->StreamingActive() && !PLSCHANNELS_API->isLiving(), PLSCHANNELS_API->toStartBroadcast, "Starting stream"),
		MAKE_CALLBACK(basic.outputHandler->StreamingActive() && PLSCHANNELS_API->isLiving(), PLSCHANNELS_API->toStopBroadcast, "Stopping stream"), this, this);
	LoadHotkeyPair(streamingHotkeys, "PLSBasic.StartStreaming", "PLSBasic.StopStreaming");

	auto cb = [](void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
		PLSBasic &basic = *static_cast<PLSBasic *>(data);
		if (basic.outputHandler->StreamingActive() && pressed) {
			basic.ForceStopStreaming();
		}
	};

	forceStreamingStopHotkey = obs_hotkey_register_frontend("PLSBasic.ForceStopStreaming", Str("Basic.Main.ForceStopStreaming"), cb, this);
	LoadHotkey(forceStreamingStopHotkey, "PLSBasic.ForceStopStreaming");

	recordingHotkeys = obs_hotkey_pair_register_frontend(
		"PLSBasic.StartRecording", Str("Basic.Main.StartRecording"), "PLSBasic.StopRecording", Str("Basic.Main.StopRecording"),
		MAKE_CALLBACK(!basic.outputHandler->RecordingActive() && !PLSCHANNELS_API->isRecording(), PLSCHANNELS_API->toStartRecord, "Starting recording"),
		MAKE_CALLBACK(basic.outputHandler->RecordingActive() && PLSCHANNELS_API->isRecording(), PLSCHANNELS_API->toStopRecord, "Stopping recording"), this, this);
	LoadHotkeyPair(recordingHotkeys, "PLSBasic.StartRecording", "PLSBasic.StopRecording");

	// pauseHotkeys = obs_hotkey_pair_register_frontend("PLSBasic.PauseRecording", Str("Basic.Main.PauseRecording"), "PLSBasic.UnpauseRecording", Str("Basic.Main.UnpauseRecording"),
	// 						 MAKE_CALLBACK(basic.pause && !basic.pause->isChecked(), basic.PauseRecording, "Pausing recording"),
	// 						 MAKE_CALLBACK(basic.pause && basic.pause->isChecked(), basic.UnpauseRecording, "Unpausing recording"), this, this);
	// LoadHotkeyPair(pauseHotkeys, "PLSBasic.PauseRecording", "PLSBasic.UnpauseRecording");

	togglePreviewHotkeys = obs_hotkey_pair_register_frontend("PLSBasic.EnablePreview", Str("Basic.Main.PreviewConextMenu.Enable"), "PLSBasic.DisablePreview", Str("Basic.Main.Preview.Disable"),
								 MAKE_CALLBACK(!basic.previewEnabled, basic.EnablePreview, "Enabling preview"),
								 MAKE_CALLBACK(basic.previewEnabled, basic.DisablePreview, "Disabling preview"), this, this);
	LoadHotkeyPair(togglePreviewHotkeys, "PLSBasic.EnablePreview", "PLSBasic.DisablePreview");

#undef MAKE_CALLBACK

	auto togglePreviewProgram = [](void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
		if (pressed)
			QMetaObject::invokeMethod(static_cast<PLSBasic *>(data), "on_actionStudioMode_triggered", Qt::QueuedConnection);
	};

	togglePreviewProgramHotkey = obs_hotkey_register_frontend("PLSBasic.TogglePreviewProgram", Str("Basic.TogglePreviewProgramMode"), togglePreviewProgram, this);
	LoadHotkey(togglePreviewProgramHotkey, "PLSBasic.TogglePreviewProgram");

	auto transition = [](void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
		if (pressed)
			QMetaObject::invokeMethod(static_cast<PLSBasic *>(data), "TransitionClicked", Qt::QueuedConnection);
	};

	transitionHotkey = obs_hotkey_register_frontend("PLSBasic.Transition", Str("Transition"), transition, this);
	LoadHotkey(transitionHotkey, "PLSBasic.Transition");

	auto resetStats = [](void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
		if (pressed)
			QMetaObject::invokeMethod(static_cast<PLSBasic *>(data), "ResetStatsHotkey", Qt::QueuedConnection);
	};

	statsHotkey = obs_hotkey_register_frontend("PLSBasic.ResetStats", Str("Basic.Stats.ResetStats"), resetStats, this);
	LoadHotkey(statsHotkey, "PLSBasic.ResetStats");
}

void PLSBasic::ClearHotkeys()
{
	obs_hotkey_pair_unregister(streamingHotkeys);
	obs_hotkey_pair_unregister(recordingHotkeys);
	obs_hotkey_pair_unregister(pauseHotkeys);
	obs_hotkey_pair_unregister(replayBufHotkeys);
	obs_hotkey_pair_unregister(togglePreviewHotkeys);
	obs_hotkey_unregister(forceStreamingStopHotkey);
	obs_hotkey_unregister(togglePreviewProgramHotkey);
	obs_hotkey_unregister(transitionHotkey);
	obs_hotkey_unregister(statsHotkey);
}

PLSBasic::~PLSBasic()
{
	delete multiviewProjectorMenu;
	delete previewProjector;
	delete studioProgramProjector;
	delete previewProjectorSource;
	delete previewProjectorMain;
	delete sourceProjector;
	delete sceneProjectorMenu;
	delete scaleFilteringMenu;
	delete colorMenu;
	delete colorWidgetAction;
	delete colorSelect;
	delete deinterlaceMenu;
	delete perSceneTransitionMenu;
	delete shortcutFilter;
	delete trayMenu;
	delete program;

	/* XXX: any obs data must be released before calling obs_shutdown.
	 * currently, we can't automate this with C++ RAII because of the
	 * delicate nature of obs_shutdown needing to be freed before the UI
	 * can be freed, and we have no control over the destruction order of
	 * the Qt UI stuff, so we have to manually clear any references to
	 * libobs. */
	delete cpuUsageTimer;
	os_cpu_usage_info_destroy(cpuUsageInfo);

	obs_hotkey_set_callback_routing_func(nullptr, nullptr);
	ClearHotkeys();

	service = nullptr;
	outputHandler.reset();

	if (interaction)
		delete interaction;

	if (properties)
		delete properties;

	if (filters)
		delete filters;

	if (transformWindow)
		delete transformWindow;

	if (advAudioWindow)
		delete advAudioWindow;

	if (about)
		delete about;

	obs_display_remove_draw_callback(ui->preview->GetDisplay(), PLSBasic::RenderMain, this);

	obs_enter_graphics();
	gs_vertexbuffer_destroy(box);
	gs_vertexbuffer_destroy(boxLeft);
	gs_vertexbuffer_destroy(boxTop);
	gs_vertexbuffer_destroy(boxRight);
	gs_vertexbuffer_destroy(boxBottom);
	gs_vertexbuffer_destroy(circle);
	obs_leave_graphics();

	/* When shutting down, sometimes source references can get in to the
	 * event queue, and if we don't forcibly process those events they
	 * won't get processed until after obs_shutdown has been called.  I
	 * really wish there were a more elegant way to deal with this via C++,
	 * but Qt doesn't use C++ in a normal way, so you can't really rely on
	 * normal C++ behavior for your data to be freed in the order that you
	 * expect or want it to. */
	QApplication::sendPostedEvents(this);

	config_set_int(App()->GlobalConfig(), "General", "LastVersion", LIBOBS_API_VER);
#if PLS_RELEASE_CANDIDATE > 0
	config_set_int(App()->GlobalConfig(), "General", "LastRCVersion", PLS_RELEASE_CANDIDATE_VER);
#endif

	bool alwaysOnTop = IsAlwaysOnTop(mainView, MAIN_FRAME);

	config_set_bool(App()->GlobalConfig(), "BasicWindow", "PreviewEnabled", previewEnabled);
	config_set_bool(App()->GlobalConfig(), "BasicWindow", "AlwaysOnTop", alwaysOnTop);
	config_set_bool(App()->GlobalConfig(), "BasicWindow", "SceneDuplicationMode", sceneDuplicationMode);
	config_set_bool(App()->GlobalConfig(), "BasicWindow", "SwapScenesMode", swapScenesMode);
	config_set_bool(App()->GlobalConfig(), "BasicWindow", "EditPropertiesMode", editPropertiesMode);
	config_set_bool(App()->GlobalConfig(), "BasicWindow", "PreviewProgramMode", IsPreviewProgramMode());
	config_set_bool(App()->GlobalConfig(), "BasicWindow", "DocksLocked", ui->lockUI->isChecked());
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);

#ifdef _WIN32
	uint32_t winVer = GetWindowsVersion();
	if (winVer > 0 && winVer < 0x602) {
		bool disableAero = config_get_bool(basicConfig, "Video", "DisableAero");
		if (disableAero) {
			SetAeroEnabled(true);
		}
	}
#endif

#ifdef BROWSER_AVAILABLE
	DestroyPanelCookieManager();
	delete cef;
	cef = nullptr;
#endif

	obs_frontend_remove_event_callback(&frontendEventHandler, this);
	pls_frontend_remove_event_callback(&frontendEventHandler, this);
}

void PLSBasic::SaveProjectNow()
{
	if (disableSaving)
		return;

	projectChanged = true;
	SaveProjectDeferred();
}

void PLSBasic::SaveProject()
{
	if (disableSaving)
		return;

	projectChanged = true;
	QMetaObject::invokeMethod(this, "SaveProjectDeferred", Qt::QueuedConnection);
}

void PLSBasic::SaveProjectDeferred()
{
	if (disableSaving)
		return;

	if (!projectChanged)
		return;

	projectChanged = false;

	const char *sceneCollection = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");
	char savePath[512];
	char fileName[512];
	int ret;

	if (!sceneCollection)
		return;

	ret = snprintf(fileName, 512, "PRISMLiveStudio/basic/scenes/%s.json", sceneCollection);
	if (ret <= 0)
		return;

	ret = GetConfigPath(savePath, sizeof(savePath), fileName);
	if (ret <= 0)
		return;

	Save(savePath);
}

void PLSBasic::resetWindowGeometry(const QPoint prismLoginViewCenterPoint)
{
	uint width = 1310;
	uint height = 802;
	uint chatWidth = 300;
	config_set_string(App()->GlobalConfig(), "BasicWindow", "geometry", "");
	config_set_string(App()->GlobalConfig(), "BasicWindow", "DockState", "");
	config_save(App()->GlobalConfig());
	for (QScreen *screen : QGuiApplication::screens()) {
		if (screen->availableGeometry().contains(prismLoginViewCenterPoint)) {
			QPoint center = screen->availableGeometry().center();
			pls_get_main_view()->setGeometry(center.x() - width / 2 - chatWidth / 2, center.y() - height / 2, width, height);
			break;
		}
	}
}

OBSSource PLSBasic::GetProgramSource()
{
	return OBSGetStrongRef(programScene);
}

OBSScene PLSBasic::GetCurrentScene()
{
	PLSSceneItemView *item = ui->scenesFrame->GetCurrentItem();

	return item ? item->GetData() : nullptr;
}

PLSSceneItemView *PLSBasic::GetCurrentSceneItem()
{
	return ui->scenesFrame->GetCurrentItem();
}

OBSSceneItem PLSBasic::GetSceneItem(QListWidgetItem *item)
{
	return item ? GetPLSRef<OBSSceneItem>(item) : nullptr;
}

OBSSceneItem PLSBasic::GetCurrentSceneItemData()
{
	return ui->sources->Get(GetTopSelectedSourceItem());
}

void PLSBasic::UpdatePreviewScalingMenu()
{
	bool fixedScaling = ui->preview->IsFixedScaling();
	float scalingAmount = ui->preview->GetScalingAmount();
	if (!fixedScaling) {
		ui->actionScaleWindow->setChecked(true);
		ui->actionScaleCanvas->setChecked(false);
		ui->actionScaleOutput->setChecked(false);
		return;
	}

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	ui->actionScaleWindow->setChecked(false);
	ui->actionScaleCanvas->setChecked(scalingAmount == 1.0f);
	ui->actionScaleOutput->setChecked(scalingAmount == float(ovi.output_width) / float(ovi.base_width));
}

void PLSBasic::CreateInteractionWindow(obs_source_t *source)
{
	if (interaction)
		interaction->close();

	interaction = new PLSBasicInteraction(this, source);
	interaction->Init();
	interaction->setAttribute(Qt::WA_DeleteOnClose, true);
}

void PLSBasic::DeletePropertiesWindow(obs_source_t *source)
{
	if (!properties) {
		return;
	}

	if (source == properties->GetSource()) {
		properties->OnButtonBoxCancelClicked(source);
	}
}

void PLSBasic::CreatePropertiesWindow(obs_source_t *source, unsigned flags, QWidget *parent)
{
	if (properties)
		properties->close();

	properties = new PLSBasicProperties(parent, source, flags);
	properties->Init();
	properties->setAttribute(Qt::WA_DeleteOnClose, true);

	connect(properties, &PLSBasicProperties::OpenFilters, this, &PLSBasic::OpenFilters);
	connect(properties, &PLSBasicProperties::propertiesChanged, this, &PLSBasic::onPropertyChanged);
}

void PLSBasic::CreateFiltersWindow(obs_source_t *source)
{
	if (filters)
		filters->close();

	if (!source) {
		OBSScene scene = GetCurrentScene();
		source = obs_scene_get_source(scene);
	}

	if (!source) {
		PLS_ERROR(MAINSCENE_MODULE, "no valid source.");
		return;
	}

	filters = new PLSBasicFilters(this, source);

	filters->resize(FILTERS_VIEW_DEFAULT_WIDTH, FILTERS_VIEW_DEFAULT_HEIGHT);
	filters->Init();
	filters->setAttribute(Qt::WA_DeleteOnClose, true);

	connect(properties, &PLSBasicProperties::propertiesChanged, this, &PLSBasic::onPropertyChanged);
}

/* Qt callbacks for invokeMethod */

void PLSBasic::AddScene(OBSSource source)
{
	const char *name = obs_source_get_name(source);
	obs_scene_t *scene = obs_scene_from_source(source);

	obs_hotkey_register_source(
		source, "PLSBasic.SelectScene", Str("Basic.Hotkeys.SelectScene"),
		[](void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
			PLSBasic *main = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());

			auto potential_source = static_cast<obs_source_t *>(data);
			auto source = obs_source_get_ref(potential_source);
			if (source && pressed)
				main->SetCurrentScene(source);
			obs_source_release(source);
		},
		static_cast<obs_source_t *>(source));

	signal_handler_t *handler = obs_source_get_signal_handler(source);

	SignalContainer<OBSScene> container;
	container.ref = scene;
	container.handlers.assign({
		std::make_shared<OBSSignal>(handler, "item_add", PLSBasic::SceneItemAdded, this),
		std::make_shared<OBSSignal>(handler, "item_select", PLSBasic::SceneItemSelected, this),
		std::make_shared<OBSSignal>(handler, "item_deselect", PLSBasic::SceneItemDeselected, this),
		std::make_shared<OBSSignal>(handler, "reorder", PLSBasic::SceneReordered, this),
	});

	ui->scenesFrame->AddScene(name, scene, container);
	ui->scenesFrame->RefreshScene();

	/* if the scene already has items (a duplicated scene) add them */
	auto addSceneItem = [this](obs_sceneitem_t *item) { AddSceneItem(item); };

	using addSceneItem_t = decltype(addSceneItem);

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			addSceneItem_t *func;
			func = reinterpret_cast<addSceneItem_t *>(param);
			(*func)(item);
			return true;
		},
		&addSceneItem);

	SaveProject();

	if (!disableSaving) {
		obs_source_t *source = obs_scene_get_source(scene);
		blog(LOG_INFO, "User added scene '%s'", obs_source_get_name(source));

		PLSProjector::UpdateMultiviewProjectors();
	}

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED);
	}
}

void PLSBasic::RemoveScene(OBSSource source)
{
	obs_scene_t *scene = obs_scene_from_source(source);

	PLSSceneItemView *sel = nullptr;
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	for (auto iter = data.begin(); iter != data.end(); ++iter) {
		PLSSceneItemView *item = iter->second;
		if (!item) {
			continue;
		}
		auto cur_scene = item->GetData();
		if (cur_scene != scene)
			continue;

		sel = item;
		break;
	}

	if (sel != nullptr) {
		if (sel == ui->scenesFrame->GetCurrentItem())
			ui->sources->Clear();
		ui->scenesFrame->DeleteScene(sel->GetName());
	}

	SaveProject();

	if (!disableSaving) {
		blog(LOG_INFO, "User Removed scene '%s'", obs_source_get_name(source));

		PLSProjector::UpdateMultiviewProjectors();
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED);
}

static bool select_one(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	obs_sceneitem_t *selectedItem = reinterpret_cast<obs_sceneitem_t *>(param);
	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, select_one, param);

	obs_sceneitem_select(item, (selectedItem == item));

	UNUSED_PARAMETER(scene);
	return true;
}

void PLSBasic::AddSceneItem(OBSSceneItem item)
{
	obs_scene_t *scene = obs_sceneitem_get_scene(item);

	if (GetCurrentScene() == scene)
		ui->sources->Add(item);

	SaveProject();

	if (!disableSaving) {
		obs_source_t *sceneSource = obs_scene_get_source(scene);
		obs_source_t *itemSource = obs_sceneitem_get_source(item);
		blog(LOG_INFO, "User added source '%s' (%s) to scene '%s'", obs_source_get_name(itemSource), obs_source_get_id(itemSource), obs_source_get_name(sceneSource));

		obs_scene_enum_items(scene, select_one, (obs_sceneitem_t *)item);
	}
}

void PLSBasic::UpdateSceneSelection(OBSSource source)
{
	if (source) {
		obs_scene_t *scene = obs_scene_from_source(source);
		const char *name = obs_source_get_name(source);

		if (!scene)
			return;
		QList<PLSSceneItemView *> items = ui->scenesFrame->FindItems(name);

		if (items.count()) {
			sceneChanging = true;
			ui->scenesFrame->SetCurrentItem(items.first());
			sceneChanging = false;

			PLSSceneItemView *item = ui->scenesFrame->GetCurrentItem();
			if (item) {
				OBSScene curScene = ui->scenesFrame->GetCurrentItem()->GetData();
				if (api && scene != curScene)
					api->on_event(OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED);
			}
		}
	}
}

static void RenameListValues(PLSSceneListView *listWidget, const QString &newName, const QString &prevName)
{
	QList<PLSSceneItemView *> items = listWidget->FindItems(prevName);

	for (int i = 0; i < items.count(); i++)
		items[i]->SetName(newName);
}

void PLSBasic::RenameSources(OBSSource source, QString newName, QString prevName)
{
	RenameListValues(ui->scenesFrame, newName, prevName);

	for (size_t i = 0; i < volumes.size(); i++) {
		if (volumes[i]->GetName().compare(prevName) == 0)
			volumes[i]->SetName(newName);
	}

	PLSProjector::RenameProjector(prevName, newName);

	SaveProject();

	obs_scene_t *scene = obs_scene_from_source(source);
	if (scene)
		PLSProjector::UpdateMultiviewProjectors();
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED);
}

void PLSBasic::SelectSceneItem(OBSScene scene, OBSSceneItem item, bool select)
{
	SignalBlocker sourcesSignalBlocker(ui->sources);

	if (scene != GetCurrentScene() || ignoreSelectionUpdate)
		return;

	ui->sources->SelectItem(item, select);
}

static inline bool SourceMixerHidden(obs_source_t *source)
{
	PLS_UI_STEP(AUDIO_MIXER, "SourceMixerHidden", ACTION_CLICK);

	obs_data_t *priv_settings = obs_source_get_private_settings(source);
	bool hidden = obs_data_get_bool(priv_settings, "mixer_hidden");
	obs_data_release(priv_settings);

	return hidden;
}

static inline void SetSourceMixerHidden(obs_source_t *source, bool hidden)
{

	obs_data_t *priv_settings = obs_source_get_private_settings(source);
	obs_data_set_bool(priv_settings, "mixer_hidden", hidden);
	obs_data_release(priv_settings);
}

void PLSBasic::GetAudioSourceFilters()
{
	PLS_UI_STEP(AUDIO_MIXER, "audio source filters", ACTION_CLICK);

	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	obs_source_t *source = vol->GetSource();

	CreateFiltersWindow(source);
}

void PLSBasic::GetAudioSourceProperties()
{
	PLS_UI_STEP(AUDIO_MIXER, "audio source properties", ACTION_CLICK);

	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	obs_source_t *source = vol->GetSource();

	CreatePropertiesWindow(source, OPERATION_NONE, this);
}

void PLSBasic::HideAudioControl()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	PLS_UI_STEP(AUDIO_MIXER, QString("hideAudioControl AudioControl name:" + vol->GetName()).toUtf8().data(), ACTION_CLICK);
	obs_source_t *source = vol->GetSource();

	if (!SourceMixerHidden(source)) {
		SetSourceMixerHidden(source, true);
		DeactivateAudioSource(source);
	}
}

void PLSBasic::UnhideAllAudioControls()
{
	PLS_UI_STEP(AUDIO_MIXER, "show all audio controls", ACTION_CLICK);

	auto UnhideAudioMixer = [this](obs_source_t *source) /* -- */
	{
		if (!obs_source_active(source))
			return true;
		if (!SourceMixerHidden(source))
			return true;

		SetSourceMixerHidden(source, false);
		ActivateAudioSource(source);
		return true;
	};

	using UnhideAudioMixer_t = decltype(UnhideAudioMixer);

	auto PreEnum = [](void *data, obs_source_t *source) -> bool /* -- */
	{ return (*reinterpret_cast<UnhideAudioMixer_t *>(data))(source); };

	obs_enum_sources(PreEnum, &UnhideAudioMixer);
}

void PLSBasic::ToggleHideMixer()
{
	OBSSceneItem item = GetCurrentSceneItemData();
	OBSSource source = obs_sceneitem_get_source(item);

	if (!SourceMixerHidden(source)) {
		SetSourceMixerHidden(source, true);
		DeactivateAudioSource(source);
	} else {
		SetSourceMixerHidden(source, false);
		ActivateAudioSource(source);
	}
}

void PLSBasic::MixerRenameSource()
{
	PLS_UI_STEP(AUDIO_MIXER, "mixer rename source", ACTION_CLICK);
	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	OBSSource source = vol->GetSource();

	const char *prevName = obs_source_get_name(source);

	for (;;) {
		string name;
		bool accepted = NameDialog::AskForName(this, QTStr("Basic.Main.MixerRename.Title"), QTStr("Basic.Main.MixerRename.Text"), name, QT_UTF8(prevName));
		if (!accepted)
			return;

		if (name.empty()) {
			PLSMessageBox::warning(this, QTStr("NoNameEntered.Title"), QTStr("NoNameEntered.Text"));
			continue;
		}

		OBSSource sourceTest = obs_get_source_by_name(name.c_str());
		obs_source_release(sourceTest);

		if (sourceTest) {
			PLSMessageBox::warning(this, QTStr("NameExists.Title"), QTStr("NameExists.Text"));
			continue;
		}

		obs_source_set_name(source, name.c_str());
		break;
	}
}

void PLSBasic::VolControlContextMenu()
{
	VolControl *vol = nullptr;
	vol = qobject_cast<VolControl *>(sender());

	/* ------------------- */

	QAction hideAction(QTStr("Hide"), this);
	QAction unhideAllAction(QTStr("UnhideAll"), this);
	QAction mixerRenameAction(QTStr("Rename"), this);

	QAction copyFiltersAction(QTStr("Copy.Filters"), this);
	QAction pasteFiltersAction(QTStr("Paste.Filters"), this);
	if (QString(copyFiltersString).isEmpty()) {
		pasteFiltersAction.setEnabled(false);
	} else {
		pasteFiltersAction.setEnabled(true);
	}
	QAction filtersAction(QTStr("Filters"), this);
	QAction propertiesAction(QTStr("Properties"), this);
	QAction advPropAction(QTStr("Basic.MainMenu.Edit.AdvAudio"), this);

	/* ------------------- */

	connect(&hideAction, &QAction::triggered, this, &PLSBasic::HideAudioControl, Qt::DirectConnection);
	connect(&unhideAllAction, &QAction::triggered, this, &PLSBasic::UnhideAllAudioControls, Qt::DirectConnection);
	connect(&mixerRenameAction, &QAction::triggered, this, &PLSBasic::MixerRenameSource, Qt::DirectConnection);

	connect(&copyFiltersAction, &QAction::triggered, this, &PLSBasic::AudioMixerCopyFilters, Qt::DirectConnection);
	connect(&pasteFiltersAction, &QAction::triggered, this, &PLSBasic::AudioMixerPasteFilters, Qt::DirectConnection);

	connect(&filtersAction, &QAction::triggered, this, &PLSBasic::GetAudioSourceFilters, Qt::DirectConnection);
	connect(&propertiesAction, &QAction::triggered, this, &PLSBasic::GetAudioSourceProperties, Qt::DirectConnection);
	connect(&advPropAction, &QAction::triggered, this, &PLSBasic::on_actionAdvAudioProperties_triggered, Qt::DirectConnection);

	/* ------------------- */

	hideAction.setProperty("volControl", QVariant::fromValue<VolControl *>(vol));
	mixerRenameAction.setProperty("volControl", QVariant::fromValue<VolControl *>(vol));

	copyFiltersAction.setProperty("volControl", QVariant::fromValue<VolControl *>(vol));
	pasteFiltersAction.setProperty("volControl", QVariant::fromValue<VolControl *>(vol));

	filtersAction.setProperty("volControl", QVariant::fromValue<VolControl *>(vol));
	propertiesAction.setProperty("volControl", QVariant::fromValue<VolControl *>(vol));

	/* ------------------- */

	if (copyFiltersString == nullptr)
		pasteFiltersAction.setEnabled(false);
	else
		pasteFiltersAction.setEnabled(true);

	PLSPopupMenu popup(this);
	popup.addAction(&unhideAllAction);
	popup.addAction(&hideAction);
	popup.addAction(&mixerRenameAction);
	popup.addSeparator();
	popup.addAction(&copyFiltersAction);
	popup.addAction(&pasteFiltersAction);
	popup.addSeparator();

	popup.addAction(&filtersAction);
	popup.addAction(&propertiesAction);
	if (!vol) {
		popup.addAction(&advPropAction);
	}
	popup.exec(QCursor::pos());
}

void PLSBasic::on_hMixerScrollArea_customContextMenuRequested()
{
	StackedMixerAreaContextMenuRequested();
}

void PLSBasic::on_vMixerScrollArea_customContextMenuRequested()
{
	StackedMixerAreaContextMenuRequested();
}

void PLSBasic::StackedMixerAreaContextMenuRequested()
{
	QAction unhideAllAction(QTStr("UnhideAll"), this);

	QAction advPropAction(QTStr("Basic.MainMenu.Edit.AdvAudio"), this);

	/* ------------------- */

	connect(&unhideAllAction, &QAction::triggered, this, &PLSBasic::UnhideAllAudioControls, Qt::DirectConnection);

	connect(&advPropAction, &QAction::triggered, this, &PLSBasic::on_actionAdvAudioProperties_triggered, Qt::DirectConnection);

	/* ------------------- */

	/* ------------------- */

	PLSPopupMenu popup(this);
	popup.addAction(&unhideAllAction);

	popup.addAction(&advPropAction);
	popup.exec(QCursor::pos());
}

void PLSBasic::ToggleMixerLayout(bool vertical)
{
	if (vertical) {
		ui->stackedMixerArea->setMinimumSize(180, 220);
		ui->stackedMixerArea->setCurrentIndex(1);
	} else {
		ui->stackedMixerArea->setMinimumSize(220, 0);
		ui->stackedMixerArea->setCurrentIndex(0);
	}
}

void PLSBasic::ToggleVolControlLayout()
{
	bool vertical = !config_get_bool(GetGlobalConfig(), "BasicWindow", "VerticalVolControl");
	config_set_bool(GetGlobalConfig(), "BasicWindow", "VerticalVolControl", vertical);
	ToggleMixerLayout(vertical);

	// We need to store it so we can delete current and then add
	// at the right order
	vector<OBSSource> sources;
	for (size_t i = 0; i != volumes.size(); i++)
		sources.emplace_back(volumes[i]->GetSource());

	ClearVolumeControls();

	for (const auto &source : sources)
		ActivateAudioSource(source);
}

void PLSBasic::ActivateAudioSource(OBSSource source)
{
	if (SourceMixerHidden(source))
		return;
	if (!obs_source_audio_active(source))
		return;

	bool vertical = config_get_bool(GetGlobalConfig(), "BasicWindow", "VerticalVolControl");
	VolControl *vol = new VolControl(source, true, vertical);

	double meterDecayRate = config_get_double(basicConfig, "Audio", "MeterDecayRate");
	vol->SetMeterDecayRate(meterDecayRate);

	uint32_t peakMeterTypeIdx = config_get_uint(basicConfig, "Audio", "PeakMeterType");

	enum obs_peak_meter_type peakMeterType;
	switch (peakMeterTypeIdx) {
	case 0:
		peakMeterType = SAMPLE_PEAK_METER;
		break;
	case 1:
		peakMeterType = TRUE_PEAK_METER;
		break;
	default:
		peakMeterType = SAMPLE_PEAK_METER;
		break;
	}

	vol->setPeakMeterType(peakMeterType);

	vol->setContextMenuPolicy(Qt::CustomContextMenu);
	//right menu show by configClicked; remove right menu
	connect(vol, &QWidget::customContextMenuRequested, this, &PLSBasic::VolControlContextMenu);
	connect(vol, &VolControl::ConfigClicked, this, &PLSBasic::VolControlContextMenu);

	InsertQObjectByName(volumes, vol);

	for (auto volume : volumes) {
		if (vertical)
			ui->vVolControlLayout->addWidget(volume);
		else
			ui->hVolControlLayout->addWidget(volume);
	}
}

void PLSBasic::DeactivateAudioSource(OBSSource source)
{
	for (size_t i = 0; i < volumes.size(); i++) {
		if (volumes[i]->GetSource() == source) {
			delete volumes[i];
			volumes.erase(volumes.begin() + i);
			break;
		}
	}
}

void PLSBasic::DuplicateSelectedScene()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Duplicate Selected Scene", ACTION_CLICK);

	OBSScene curScene = GetCurrentScene();

	if (!curScene)
		return;

	OBSSource curSceneSource = obs_scene_get_source(curScene);
	QString format{obs_source_get_name(curSceneSource)};
	format += " %1";

	int i = 2;
	QString placeHolderText = format.arg(i);
	obs_source_t *source = nullptr;
	while ((source = obs_get_source_by_name(QT_TO_UTF8(placeHolderText)))) {
		obs_source_release(source);
		placeHolderText = format.arg(++i);
	}

	for (;;) {
		string name;
		bool accepted = NameDialog::AskForName(this, QTStr("Basic.Main.AddSceneDlg.Title"), QTStr("Basic.Main.AddSceneDlg.Text"), name, placeHolderText);
		if (!accepted)
			return;

		if (name.empty()) {
			PLSMessageBox::warning(this, QTStr("NoNameEntered.Title"), QTStr("NoNameEntered.Text"));
			continue;
		}

		obs_source_t *source = obs_get_source_by_name(name.c_str());
		if (source) {
			PLSMessageBox::warning(this, QTStr("NameExists.Title"), QTStr("NameExists.Text"));

			obs_source_release(source);
			continue;
		}

		obs_scene_t *scene = obs_scene_duplicate(curScene, name.c_str(), OBS_SCENE_DUP_REFS);
		source = obs_scene_get_source(scene);
		SetCurrentScene(source, true);
		obs_scene_release(scene);

		break;
	}
}

void PLSBasic::RemoveSelectedScene()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Remove Selected Scene", ACTION_CLICK);

	OBSScene scene = GetCurrentScene();
	if (scene) {
		obs_source_t *source = obs_scene_get_source(scene);
		if (QueryRemoveSource(source)) {
			obs_source_remove(source);
		}
	}
}

void PLSBasic::RemoveSelectedSceneItem()
{
	OBSSceneItem item = GetCurrentSceneItemData();
	if (item) {
		obs_source_t *source = obs_sceneitem_get_source(item);
		if (QueryRemoveSource(source))
			obs_sceneitem_remove(item);
	}
}

void PLSBasic::ReorderSources(OBSScene scene)
{
	if (scene != GetCurrentScene() || ui->sources->IgnoreReorder())
		return;

	ui->sources->ReorderItems();
	SaveProject();
}

/* PLS Callbacks */

void PLSBasic::SceneReordered(void *data, calldata_t *params)
{
	PLSBasic *window = static_cast<PLSBasic *>(data);

	obs_scene_t *scene = (obs_scene_t *)calldata_ptr(params, "scene");

	QMetaObject::invokeMethod(window, "ReorderSources", Q_ARG(OBSScene, OBSScene(scene)));
}

void PLSBasic::SceneItemAdded(void *data, calldata_t *params)
{
	PLSBasic *window = static_cast<PLSBasic *>(data);

	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(params, "item");

	QMetaObject::invokeMethod(window, "AddSceneItem", Q_ARG(OBSSceneItem, OBSSceneItem(item)));
}

void PLSBasic::SceneItemSelected(void *data, calldata_t *params)
{
	PLSBasic *window = static_cast<PLSBasic *>(data);

	obs_scene_t *scene = (obs_scene_t *)calldata_ptr(params, "scene");
	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(params, "item");

	QMetaObject::invokeMethod(window, "SelectSceneItem", Q_ARG(OBSScene, scene), Q_ARG(OBSSceneItem, item), Q_ARG(bool, true));
}

void PLSBasic::SceneItemDeselected(void *data, calldata_t *params)
{
	PLSBasic *window = static_cast<PLSBasic *>(data);

	obs_scene_t *scene = (obs_scene_t *)calldata_ptr(params, "scene");
	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(params, "item");

	QMetaObject::invokeMethod(window, "SelectSceneItem", Q_ARG(OBSScene, scene), Q_ARG(OBSSceneItem, item), Q_ARG(bool, false));
}

void PLSBasic::SourceCreated(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");
	if (!source)
		return;

	if (obs_scene_from_source(source) != NULL)
		QMetaObject::invokeMethod(static_cast<PLSBasic *>(data), "AddScene", WaitConnection(), Q_ARG(OBSSource, OBSSource(source)));
}

void PLSBasic::SourceRemoved(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");

	if (obs_scene_from_source(source) != NULL)
		QMetaObject::invokeMethod(static_cast<PLSBasic *>(data), "RemoveScene", Q_ARG(OBSSource, OBSSource(source)));
}

void PLSBasic::SourceActivated(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");
	uint32_t flags = obs_source_get_output_flags(source);
	if (flags & OBS_SOURCE_AUDIO) {
		QMetaObject::invokeMethod(static_cast<PLSBasic *>(data), "ActivateAudioSource", Q_ARG(OBSSource, OBSSource(source)));
	}
}

void PLSBasic::SourceDeactivated(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");
	uint32_t flags = obs_source_get_output_flags(source);

	if (flags & OBS_SOURCE_AUDIO)
		QMetaObject::invokeMethod(static_cast<PLSBasic *>(data), "DeactivateAudioSource", Q_ARG(OBSSource, OBSSource(source)));
}

void PLSBasic::SourceAudioActivated(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");

	if (obs_source_active(source))
		QMetaObject::invokeMethod(static_cast<PLSBasic *>(data), "ActivateAudioSource", Q_ARG(OBSSource, OBSSource(source)));
}

void PLSBasic::SourceAudioDeactivated(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");
	QMetaObject::invokeMethod(static_cast<PLSBasic *>(data), "DeactivateAudioSource", Q_ARG(OBSSource, OBSSource(source)));
}

void PLSBasic::SourceRenamed(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");
	const char *newName = calldata_string(params, "new_name");
	const char *prevName = calldata_string(params, "prev_name");

	QMetaObject::invokeMethod(static_cast<PLSBasic *>(data), "RenameSources", Q_ARG(OBSSource, source), Q_ARG(QString, QT_UTF8(newName)), Q_ARG(QString, QT_UTF8(prevName)));

	blog(LOG_INFO, "Source '%s' renamed to '%s'", prevName, newName);
}

void PLSBasic::DrawBackdrop(float cx, float cy)
{
	if (!box)
		return;

	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_DEFAULT, "DrawBackdrop");

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	vec4 colorVal;
	vec4_set(&colorVal, 0.0f, 0.0f, 0.0f, 1.0f);
	gs_effect_set_vec4(color, &colorVal);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_scale3f(float(cx), float(cy), 1.0f);

	gs_load_vertexbuffer(box);
	gs_draw(GS_TRISTRIP, 0, 0);

	gs_matrix_pop();
	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_load_vertexbuffer(nullptr);

	GS_DEBUG_MARKER_END();
}

void PLSBasic::RenderMain(void *data, uint32_t cx, uint32_t cy)
{
	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_DEFAULT, "RenderMain");

	PLSBasic *window = static_cast<PLSBasic *>(data);
	obs_video_info ovi;

	obs_get_video_info(&ovi);

	window->previewCX = int(window->previewScale * float(ovi.base_width));
	window->previewCY = int(window->previewScale * float(ovi.base_height));

	gs_viewport_push();
	gs_projection_push();

	obs_display_t *display = window->ui->preview->GetDisplay();
	uint32_t width, height;
	obs_display_size(display, &width, &height);
	float right = float(width) - window->previewX;
	float bottom = float(height) - window->previewY;

	gs_ortho(-window->previewX, right, -window->previewY, bottom, -100.0f, 100.0f);

	window->ui->preview->DrawOverflow();

	/* --------------------------------------- */

	gs_ortho(0.0f, float(ovi.base_width), 0.0f, float(ovi.base_height), -100.0f, 100.0f);
	gs_set_viewport(window->previewX, window->previewY, window->previewCX, window->previewCY);

	if (window->IsPreviewProgramMode()) {
		window->DrawBackdrop(float(ovi.base_width), float(ovi.base_height));

		OBSScene scene = window->GetCurrentScene();
		obs_source_t *source = obs_scene_get_source(scene);
		if (source)
			obs_source_video_render(source);
	} else {
		obs_render_main_texture_src_color_only();
	}
	gs_load_vertexbuffer(nullptr);

	/* --------------------------------------- */

	gs_ortho(-window->previewX, right, -window->previewY, bottom, -100.0f, 100.0f);
	gs_reset_viewport();

	window->ui->preview->DrawSceneEditing();

	/* --------------------------------------- */

	gs_projection_pop();
	gs_viewport_pop();

	GS_DEBUG_MARKER_END();

	UNUSED_PARAMETER(cx);
	UNUSED_PARAMETER(cy);
}

void PLSBasic::frontendEventHandler(enum obs_frontend_event event, void *private_data)
{
	PLSBasic *main = (PLSBasic *)private_data;
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		main->showEncodingInStatusBar();
		break;
	default:
		break;
	}
}

void PLSBasic::frontendEventHandler(pls_frontend_event event, const QVariantList &params, void *context)
{
	PLSBasic *main = (PLSBasic *)context;
	switch (event) {
	case pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_START:
		main->ui->profileMenu->menuAction()->setEnabled(false);
		main->mainView->statusBar()->setEncodingEnabled(false);
		main->ui->actionLogout->setEnabled(false);

		if (!main->outputHandler->ReplayBufferActive()) {
			PLS_INFO(HOTKEYS_MODULE, "Auto start replay buffer on stream or record started");
			main->StartReplayBufferWithNoCheck();
		}
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_END:
		main->ui->profileMenu->menuAction()->setEnabled(true);
		main->mainView->statusBar()->setEncodingEnabled(true);
		main->ui->actionLogout->setEnabled(true);

		if (!pls_is_living_or_recording()) {
			if (main->outputHandler->ReplayBufferActive()) {
				PLS_INFO(HOTKEYS_MODULE, "Auto save replay buffer on stream or record stopped");
				main->StopReplayBuffer();
			}
		}
		break;
	default:
		break;
	}
}

void PLSBasic::LogoutCallback(pls_frontend_event event, const QVariantList &params, void *context)
{
	bool defaultPreviewMode = true;
	config_set_bool(App()->GlobalConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_PREVIEW_MODE_MODULE, defaultPreviewMode);
	reinterpret_cast<PLSBasic *>(App()->GetMainWindow())->SetPreviewProgramMode(defaultPreviewMode);
}

/* Main class functions */

obs_service_t *PLSBasic::GetService()
{
	if (!service) {
		service = obs_service_create("rtmp_common", NULL, NULL, nullptr);
		obs_service_release(service);
	}
	return service;
}

void PLSBasic::SetService(obs_service_t *newService)
{
	if (newService)
		service = newService;
}

bool PLSBasic::StreamingActive() const
{
	if (!outputHandler)
		return false;
	return outputHandler->StreamingActive();
}

bool PLSBasic::Active() const
{
	if (!outputHandler)
		return false;
	return outputHandler->Active();
}

#ifdef _WIN32
#define IS_WIN32 1
#else
#define IS_WIN32 0
#endif

static inline int AttemptToResetVideo(struct obs_video_info *ovi)
{
	return obs_reset_video(ovi);
}

static inline enum obs_scale_type GetScaleType(ConfigFile &basicConfig)
{
	const char *scaleTypeStr = config_get_string(basicConfig, "Video", "ScaleType");

	if (astrcmpi(scaleTypeStr, "bilinear") == 0)
		return OBS_SCALE_BILINEAR;
	else if (astrcmpi(scaleTypeStr, "lanczos") == 0)
		return OBS_SCALE_LANCZOS;
	else if (astrcmpi(scaleTypeStr, "area") == 0)
		return OBS_SCALE_AREA;
	else
		return OBS_SCALE_BICUBIC;
}

static inline enum video_format GetVideoFormatFromName(const char *name)
{
	if (astrcmpi(name, "I420") == 0)
		return VIDEO_FORMAT_I420;
	else if (astrcmpi(name, "NV12") == 0)
		return VIDEO_FORMAT_NV12;
	else if (astrcmpi(name, "I444") == 0)
		return VIDEO_FORMAT_I444;
#if 0 //currently unsupported
	else if (astrcmpi(name, "YVYU") == 0)
		return VIDEO_FORMAT_YVYU;
	else if (astrcmpi(name, "YUY2") == 0)
		return VIDEO_FORMAT_YUY2;
	else if (astrcmpi(name, "UYVY") == 0)
		return VIDEO_FORMAT_UYVY;
#endif
	else
		return VIDEO_FORMAT_RGBA;
}

void PLSBasic::ResetUI()
{
	bool studioPortraitLayout = config_get_bool(GetGlobalConfig(), "BasicWindow", "StudioPortraitLayout");

	bool labels = config_get_bool(GetGlobalConfig(), "BasicWindow", "StudioModeLabels");

	if (studioPortraitLayout)
		ui->previewLayout->setDirection(QBoxLayout::TopToBottom);
	else
		ui->previewLayout->setDirection(QBoxLayout::LeftToRight);
}

int PLSBasic::ResetVideo()
{
	if (outputHandler && outputHandler->Active())
		return OBS_VIDEO_CURRENTLY_ACTIVE;

	ProfileScope("PLSBasic::ResetVideo");

	struct obs_video_info ovi;
	int ret;

	GetConfigFPS(ovi.fps_num, ovi.fps_den);

	const char *colorFormat = config_get_string(basicConfig, "Video", "ColorFormat");
	const char *colorSpace = config_get_string(basicConfig, "Video", "ColorSpace");
	const char *colorRange = config_get_string(basicConfig, "Video", "ColorRange");

	ovi.graphics_module = App()->GetRenderModule();
	ovi.base_width = (uint32_t)config_get_uint(basicConfig, "Video", "BaseCX");
	ovi.base_height = (uint32_t)config_get_uint(basicConfig, "Video", "BaseCY");
	ovi.output_width = (uint32_t)config_get_uint(basicConfig, "Video", "OutputCX");
	ovi.output_height = (uint32_t)config_get_uint(basicConfig, "Video", "OutputCY");
	ovi.output_format = GetVideoFormatFromName(colorFormat);
	ovi.colorspace = astrcmpi(colorSpace, "601") == 0 ? VIDEO_CS_601 : VIDEO_CS_709;
	ovi.range = astrcmpi(colorRange, "Full") == 0 ? VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL;
	ovi.adapter = config_get_uint(App()->GlobalConfig(), "Video", "AdapterIdx");
	ovi.gpu_conversion = true;
	ovi.scale_type = GetScaleType(basicConfig);

	if (ovi.base_width == 0 || ovi.base_height == 0) {
		ovi.base_width = 1920;
		ovi.base_height = 1080;
		config_set_uint(basicConfig, "Video", "BaseCX", 1920);
		config_set_uint(basicConfig, "Video", "BaseCY", 1080);
	}

	if (ovi.output_width == 0 || ovi.output_height == 0) {
		ovi.output_width = ovi.base_width;
		ovi.output_height = ovi.base_height;
		config_set_uint(basicConfig, "Video", "OutputCX", ovi.base_width);
		config_set_uint(basicConfig, "Video", "OutputCY", ovi.base_height);
	}

	ret = AttemptToResetVideo(&ovi);
	if (IS_WIN32 && ret != OBS_VIDEO_SUCCESS) {
		if (ret == OBS_VIDEO_CURRENTLY_ACTIVE) {
			blog(LOG_WARNING, "Tried to reset when "
					  "already active");
			return ret;
		}

		/* Try OpenGL if DirectX fails on windows */
		if (astrcmpi(ovi.graphics_module, DL_OPENGL) != 0) {
			blog(LOG_WARNING,
			     "Failed to initialize obs video (%d) "
			     "with graphics_module='%s', retrying "
			     "with graphics_module='%s'",
			     ret, ovi.graphics_module, DL_OPENGL);
			ovi.graphics_module = DL_OPENGL;
			ret = AttemptToResetVideo(&ovi);
		}
	} else if (ret == OBS_VIDEO_SUCCESS) {
		ResizePreview(ovi.base_width, ovi.base_height);
		if (program)
			ResizeProgram(ovi.base_width, ovi.base_height);
	}

	if (ret == OBS_VIDEO_SUCCESS) {
		PLSBasicStats::InitializeValues();
		PLSProjector::UpdateMultiviewProjectors();
		App()->InitWatermark();
	}

	return ret;
}

bool PLSBasic::ResetAudio()
{
	ProfileScope("PLSBasic::ResetAudio");

	struct obs_audio_info ai;
	ai.samples_per_sec = config_get_uint(basicConfig, "Audio", "SampleRate");

	const char *channelSetupStr = config_get_string(basicConfig, "Audio", "ChannelSetup");

	if (strcmp(channelSetupStr, "Mono") == 0)
		ai.speakers = SPEAKERS_MONO;
	else if (strcmp(channelSetupStr, "2.1") == 0)
		ai.speakers = SPEAKERS_2POINT1;
	else if (strcmp(channelSetupStr, "4.0") == 0)
		ai.speakers = SPEAKERS_4POINT0;
	else if (strcmp(channelSetupStr, "4.1") == 0)
		ai.speakers = SPEAKERS_4POINT1;
	else if (strcmp(channelSetupStr, "5.1") == 0)
		ai.speakers = SPEAKERS_5POINT1;
	else if (strcmp(channelSetupStr, "7.1") == 0)
		ai.speakers = SPEAKERS_7POINT1;
	else
		ai.speakers = SPEAKERS_STEREO;

	return obs_reset_audio(&ai);
}

void PLSBasic::ResetAudioDevice(const char *sourceId, const char *deviceId, const char *deviceDesc, int channel)
{
	bool disable = deviceId && strcmp(deviceId, "disabled") == 0;
	obs_source_t *source;
	obs_data_t *settings;

	source = obs_get_output_source(channel);
	if (source) {
		if (disable) {
			obs_set_output_source(channel, nullptr);
		} else {
			settings = obs_source_get_settings(source);
			const char *oldId = obs_data_get_string(settings, "device_id");
			if (strcmp(oldId, deviceId) != 0) {
				obs_data_set_string(settings, "device_id", deviceId);
				obs_source_update(source, settings);
			}
			obs_data_release(settings);
		}

		obs_source_release(source);

	} else if (!disable) {
		settings = obs_data_create();
		obs_data_set_string(settings, "device_id", deviceId);
		source = obs_source_create(sourceId, deviceDesc, settings, nullptr);
		obs_data_release(settings);

		obs_set_output_source(channel, source);
		obs_source_release(source);
	}
}

void PLSBasic::ResizePreview(uint32_t cx, uint32_t cy)
{
	QSize targetSize;
	bool isFixedScaling;
	obs_video_info ovi;

	/* resize preview panel to fix to the top section of the window */
	targetSize = GetPixelSize(ui->preview);

	isFixedScaling = ui->preview->IsFixedScaling();
	obs_get_video_info(&ovi);

	if (isFixedScaling) {
		previewScale = ui->preview->GetScalingAmount();
		GetCenterPosFromFixedScale(int(cx), int(cy), targetSize.width() - PREVIEW_EDGE_SIZE * 2, targetSize.height() - PREVIEW_EDGE_SIZE * 2, previewX, previewY, previewScale);
		previewX += ui->preview->GetScrollX();
		previewY += ui->preview->GetScrollY();

	} else {
		GetScaleAndCenterPos(int(cx), int(cy), targetSize.width() - PREVIEW_EDGE_SIZE * 2, targetSize.height() - PREVIEW_EDGE_SIZE * 2, previewX, previewY, previewScale);
	}

	previewX += float(PREVIEW_EDGE_SIZE);
	previewY += float(PREVIEW_EDGE_SIZE);
}

void PLSBasic::CloseDialogs()
{
	QList<QDialog *> childDialogs = this->findChildren<QDialog *>();
	if (!childDialogs.isEmpty()) {
		for (int i = 0; i < childDialogs.size(); ++i) {
			childDialogs.at(i)->close();
		}
	}

	for (auto &proj : windowProjectors) {
		delete proj.first;
		proj.first.clear();
		proj.second = nullptr;
	}
	for (auto &proj : projectors) {
		delete proj.first;
		proj.first.clear();
		proj.second = nullptr;
	}

	if (!stats.isNull())
		stats->close(); //call close to save Stats geometry
	if (!remux.isNull())
		remux->close();
}

void PLSBasic::EnumDialogs()
{
	visDialogs.clear();
	modalDialogs.clear();
	visMsgBoxes.clear();

	/* fill list of Visible dialogs and Modal dialogs */
	for (QWidget *widget : QApplication::topLevelWidgets()) {
		if (PLSAlertView *msgbox = dynamic_cast<PLSAlertView *>(widget); msgbox) {
			if (msgbox->isVisible())
				visMsgBoxes.append(msgbox);
		} else if (QDialog *dialog = dynamic_cast<QDialog *>(widget); dialog) {
			if (dialog->isVisible())
				visDialogs.append(dialog);
			if (dialog->isModal())
				modalDialogs.append(dialog);
		}
	}
}

void PLSBasic::ClearSceneData()
{
	disableSaving++;

	CloseDialogs();

	ClearVolumeControls();
	PLSSceneDataMgr::Instance()->DeleteAllData();
	ui->sources->Clear();
	ui->scenesFrame->ClearTransition();
	obs_set_output_source(0, nullptr);
	obs_set_output_source(1, nullptr);
	obs_set_output_source(2, nullptr);
	obs_set_output_source(3, nullptr);
	obs_set_output_source(4, nullptr);
	obs_set_output_source(5, nullptr);
	lastScene = nullptr;
	swapScene = nullptr;
	programScene = nullptr;

	auto cb = [](void *unused, obs_source_t *source) {
		obs_source_remove(source);
		UNUSED_PARAMETER(unused);
		return true;
	};

	obs_enum_sources(cb, nullptr);

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP);

	disableSaving--;

	blog(LOG_INFO, "All scene data cleared");
	blog(LOG_INFO, "------------------------------------------------");
}

void PLSBasic::mainViewClose(QCloseEvent *event)
{
	m_bFastStop = true;

	/* Do not close window if inside of a temporary event loop because we
	 * could be inside of an Auth::LoadUI call.  Keep trying once per
	 * second until we've exit any known sub-loops. */
	if (os_atomic_load_long(&insideEventLoop) != 0) {
		QTimer::singleShot(1000, mainView, SLOT(close()));
		event->ignore();
		mainView->closing = false;
		return;
	}

	if (mainView->isVisible() && !mainView->getMaxState() && !mainView->getFullScreenState()) {
		config_set_string(App()->GlobalConfig(), "BasicWindow", "geometry", mainView->saveGeometry().toBase64().constData());
	}

	if (pls_is_living_or_recording()) {
		SetShowing(true);
		// close main view in living or recording need show alert
		if (!m_isUpdateLanguage && !m_isSessionExpired &&
		    PLSAlertView::question(getMainView(), tr("notice.bottom.confirm.button.text"), tr("main.message.exit_broadcasting_alert"),
					   PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel) != PLSAlertView::Button::Ok) {
			event->ignore();
			mainView->closing = false;
			return;
		}
	}

	mainView->callBaseCloseEvent(event);
	if (!event->isAccepted()) {
		mainView->closing = false;
		return;
	}

	blog(LOG_INFO, SHUTDOWN_SEPARATOR);

	if (logUploadThread)
		logUploadThread->wait();

	signalHandlers.clear();

	Auth::Save();
	SaveProjectNow();
	auth.reset();

	config_set_string(App()->GlobalConfig(), "BasicWindow", "DockState", saveState().toBase64().constData());

#ifdef BROWSER_AVAILABLE
	SaveExtraBrowserDocks();
	ClearExtraBrowserDocks();
#endif

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_EXIT);

	disableSaving++;

	/* Clear all scene data (dialogs, widgets, widget sub-items, scenes,
	 * sources, etc) so that all references are released before shutdown */
	ClearSceneData();

	App()->quit();
}

void PLSBasic::showEncodingInStatusBar()
{
	mainView->statusBar()->setEncoding(config_get_uint(basicConfig, "Video", "OutputCX"), config_get_uint(basicConfig, "Video", "OutputCY"));
	uint32_t fpsNum = 0, fpsDen = 0;
	GetConfigFPS(fpsNum, fpsDen);
	mainView->statusBar()->setFps(QString("%3fps").arg(fpsNum / fpsDen));
	emit statusBarDataChanged();
}

void PLSBasic::showEvent(QShowEvent *event)
{
	showEncodingInStatusBar();
	PLSMainWindow::showEvent(event);
}

bool PLSBasic::eventFilter(QObject *watcher, QEvent *event)
{
	if (watcher == mainView) {
		if (event->type() == QEvent::WindowStateChange && mainView->isMinimized() && trayIcon && trayIcon->isVisible() && sysTrayMinimizeToTray()) {
			ToggleShowHide();
		}
	}

	return PLSMainWindow::eventFilter(watcher, event);
}

void PLSBasic::on_actionShowVideosFolder_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu File Show Videos Folder", ACTION_CLICK);

	const char *mode = config_get_string(basicConfig, "Output", "Mode");
	const char *type = config_get_string(basicConfig, "AdvOut", "RecType");
	const char *adv_path = strcmp(type, "Standard") ? config_get_string(basicConfig, "AdvOut", "FFFilePath") : config_get_string(basicConfig, "AdvOut", "RecFilePath");
	const char *path = strcmp(mode, "Advanced") ? config_get_string(basicConfig, "SimpleOutput", "FilePath") : adv_path;
	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void PLSBasic::on_actionRemux_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Remux", ACTION_CLICK);

	if (!remux.isNull()) {
		remux->show();
		remux->raise();
		return;
	}

	const char *mode = config_get_string(basicConfig, "Output", "Mode");
	const char *path = strcmp(mode, "Advanced") ? config_get_string(basicConfig, "SimpleOutput", "FilePath") : config_get_string(basicConfig, "AdvOut", "RecFilePath");

	PLSRemux *remuxDlg;
	remuxDlg = new PLSRemux(path, this);
	remuxDlg->show();
	remux = remuxDlg;
}

void PLSBasic::on_action_Settings_triggered()
{
	onPopupSettingView(QStringLiteral("General"), QStringLiteral(""));
}

void PLSBasic::on_actionAdvAudioProperties_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Advance Audio Properties", ACTION_CLICK);

	if (advAudioWindow != nullptr) {
		advAudioWindow->raise();
		return;
	}

	advAudioWindow = new PLSBasicAdvAudio(this);
	advAudioWindow->show();
	advAudioWindow->setAttribute(Qt::WA_DeleteOnClose, true);

	connect(advAudioWindow, SIGNAL(destroyed()), this, SLOT(on_advAudioProps_destroyed()));
}

void PLSBasic::on_advAudioProps_clicked()
{
	on_actionAdvAudioProperties_triggered();
}

void PLSBasic::on_advAudioProps_destroyed()
{
	advAudioWindow = nullptr;
}

void PLSBasic::OnScenesCurrentItemChanged()
{
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED);
}

void PLSBasic::EditSceneName()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Edit Scene Name", ACTION_CLICK);

	PLSSceneItemView *item = ui->scenesFrame->GetCurrentItem();
	if (item) {
		item->OnRenameOperation();
	}
}

static void AddProjectorMenuMonitors(QMenu *parent, QObject *target, const char *slot)
{
	QAction *action;
	QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); i++) {
		QRect screenGeometry = screens[i]->geometry();
		QString str = QString("%1 %2: %3x%4 @ %5,%6")
				      .arg(QTStr("Display"), QString::number(i + 1), QString::number(screenGeometry.width()), QString::number(screenGeometry.height()),
					   QString::number(screenGeometry.x()), QString::number(screenGeometry.y()));

		action = parent->addAction(str, target, slot);
		action->setProperty("monitor", i);
	}
}

void PLSBasic::OnScenesCustomContextMenuRequested(PLSSceneItemView *item)
{
	QMenu popup(this);

	if (item) {

		popup.addAction(QTStr("Copy.Scene"), this, SLOT(DuplicateSelectedScene()));
		popup.addAction(QTStr("Rename"), this, SLOT(EditSceneName()));
		popup.addAction(QTStr("Delete"), this, SLOT(RemoveSelectedScene()));
		popup.addSeparator();

		popup.addAction(QTStr("Filters"), this, SLOT(OpenSceneFilters()));
		QAction *pasteFilters = new QAction(QTStr("Paste.Filters"), this);
		pasteFilters->setEnabled(copyFiltersString);
		connect(pasteFilters, SIGNAL(triggered()), this, SLOT(ScenePasteFilters()));

		popup.addAction(QTStr("Copy.Filters"), this, SLOT(SceneCopyFilters()));
		popup.addAction(pasteFilters);
		popup.addSeparator();

		QMenu *projector = new QMenu(QTStr("Projector"), this);

		popup.addSeparator();

		delete sceneProjectorMenu;
		sceneProjectorMenu = new QMenu(QTStr("SceneProjector"), this);
		AddProjectorMenuMonitors(sceneProjectorMenu, this, SLOT(OpenSceneProjector()));
		projector->addMenu(sceneProjectorMenu);

		QAction *sceneWindow = projector->addAction(QTStr("SceneWindow"), this, SLOT(OpenSceneWindow()));

		popup.addMenu(projector);
		popup.addSeparator();

		/* ---------------------- */

		QMenu *multiviewMenu = new QMenu(QTStr("ShowInMultiview"), this);
		popup.addMenu(multiviewMenu);

		QAction *multiviewShow = new QAction(QTStr("Show"), this);
		multiviewShow->setCheckable(true);

		QAction *multiviewHide = new QAction(QTStr("Hide"), this);
		multiviewHide->setCheckable(true);

		multiviewMenu->addAction(multiviewShow);

		multiviewMenu->addSeparator();
		multiviewMenu->addAction(multiviewHide);

		OBSSource source = GetCurrentSceneSource();
		OBSData data = obs_source_get_private_settings(source);
		obs_data_release(data);

		obs_data_set_default_bool(data, "show_in_multiview", true);
		bool show = obs_data_get_bool(data, "show_in_multiview");

		multiviewShow->setChecked(show);
		multiviewHide->setChecked(!show);

		connect(multiviewShow, &QAction::triggered, this, [this, multiviewHide](bool checked) {
			multiviewHide->setChecked(!checked);
			OnMultiviewShowTriggered(checked);
		});

		connect(multiviewHide, &QAction::triggered, this, [this, multiviewShow](bool checked) {
			multiviewShow->setChecked(!checked);
			OnMultiviewHideTriggered(checked);
		});

		delete perSceneTransitionMenu;
		perSceneTransitionMenu = CreatePerSceneTransitionMenu();
		popup.addMenu(perSceneTransitionMenu);
	}

	popup.exec(QCursor::pos());
}

void PLSBasic::OnMultiviewShowTriggered(bool checked)
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Multiview Show", ACTION_CLICK);

	OBSSource source = GetCurrentSceneSource();
	OBSData data = obs_source_get_private_settings(source);
	obs_data_release(data);

	obs_data_set_bool(data, "show_in_multiview", checked);
	PLSProjector::UpdateMultiviewProjectors();
}

void PLSBasic::OnMultiviewHideTriggered(bool checked)
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Multiview Hide", ACTION_CLICK);

	OBSSource source = GetCurrentSceneSource();
	OBSData data = obs_source_get_private_settings(source);
	obs_data_release(data);

	obs_data_set_bool(data, "show_in_multiview", !checked);
	PLSProjector::UpdateMultiviewProjectors();
}

void PLSBasic::EditSceneItemName()
{
	int idx = GetTopSelectedSourceItem();
	ui->sources->Edit(idx);
	PLS_UI_STEP(MAINFRAME_MODULE, QT_TO_UTF8(QTStr("Rename Source")), ACTION_CLICK);
}

void PLSBasic::SetDeinterlacingMode()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	obs_deinterlace_mode mode = (obs_deinterlace_mode)action->property("mode").toInt();
	OBSSceneItem sceneItem = GetCurrentSceneItemData();
	obs_source_t *source = obs_sceneitem_get_source(sceneItem);

	obs_source_set_deinterlace_mode(source, mode);
}

void PLSBasic::SetDeinterlacingOrder()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	obs_deinterlace_field_order order = (obs_deinterlace_field_order)action->property("order").toInt();
	OBSSceneItem sceneItem = GetCurrentSceneItemData();
	obs_source_t *source = obs_sceneitem_get_source(sceneItem);

	obs_source_set_deinterlace_field_order(source, order);
}

QMenu *PLSBasic::AddDeinterlacingMenu(QMenu *menu, obs_source_t *source)
{
	obs_deinterlace_mode deinterlaceMode = obs_source_get_deinterlace_mode(source);
	obs_deinterlace_field_order deinterlaceOrder = obs_source_get_deinterlace_field_order(source);
	QAction *action;

#define ADD_MODE(name, mode)                                                          \
	action = menu->addAction(QTStr("" name), this, SLOT(SetDeinterlacingMode())); \
	action->setProperty("mode", (int)mode);                                       \
	action->setCheckable(true);                                                   \
	action->setChecked(deinterlaceMode == mode);

	ADD_MODE("Disable", OBS_DEINTERLACE_MODE_DISABLE);
	ADD_MODE("Deinterlacing.Discard", OBS_DEINTERLACE_MODE_DISCARD);
	ADD_MODE("Deinterlacing.Retro", OBS_DEINTERLACE_MODE_RETRO);
	ADD_MODE("Deinterlacing.Blend", OBS_DEINTERLACE_MODE_BLEND);
	ADD_MODE("Deinterlacing.Blend2x", OBS_DEINTERLACE_MODE_BLEND_2X);
	ADD_MODE("Deinterlacing.Linear", OBS_DEINTERLACE_MODE_LINEAR);
	ADD_MODE("Deinterlacing.Linear2x", OBS_DEINTERLACE_MODE_LINEAR_2X);
	ADD_MODE("Deinterlacing.Yadif", OBS_DEINTERLACE_MODE_YADIF);
	ADD_MODE("Deinterlacing.Yadif2x", OBS_DEINTERLACE_MODE_YADIF_2X);
#undef ADD_MODE

	menu->addSeparator();

#define ADD_ORDER(name, order)                                                                       \
	action = menu->addAction(QTStr("Deinterlacing." name), this, SLOT(SetDeinterlacingOrder())); \
	action->setProperty("order", (int)order);                                                    \
	action->setCheckable(true);                                                                  \
	action->setChecked(deinterlaceOrder == order);

	ADD_ORDER("TopFieldFirst", OBS_DEINTERLACE_FIELD_ORDER_TOP);
	ADD_ORDER("BottomFieldFirst", OBS_DEINTERLACE_FIELD_ORDER_BOTTOM);
#undef ADD_ORDER

	return menu;
}

void PLSBasic::SetScaleFilter()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	obs_scale_type mode = (obs_scale_type)action->property("mode").toInt();
	OBSSceneItem sceneItem = GetCurrentSceneItemData();

	obs_sceneitem_set_scale_filter(sceneItem, mode);
}

QMenu *PLSBasic::AddScaleFilteringMenu(QMenu *menu, obs_sceneitem_t *item)
{
	obs_scale_type scaleFilter = obs_sceneitem_get_scale_filter(item);
	QAction *action;

#define ADD_MODE(name, mode)                                                    \
	action = menu->addAction(QTStr("" name), this, SLOT(SetScaleFilter())); \
	action->setProperty("mode", (int)mode);                                 \
	action->setCheckable(true);                                             \
	action->setChecked(scaleFilter == mode);

	ADD_MODE("Disable", OBS_SCALE_DISABLE);
	ADD_MODE("ScaleFiltering.Point", OBS_SCALE_POINT);
	ADD_MODE("ScaleFiltering.Bilinear", OBS_SCALE_BILINEAR);
	ADD_MODE("ScaleFiltering.Bicubic", OBS_SCALE_BICUBIC);
	ADD_MODE("ScaleFiltering.Lanczos", OBS_SCALE_LANCZOS);
	ADD_MODE("ScaleFiltering.Area", OBS_SCALE_AREA);
#undef ADD_MODE

	return menu;
}

std::vector<QString> presetColorList = {"rgba(255,68,68,55%)", "rgba(255,255,68,55%)", "rgba(68,255,68,55%)", "rgba(68,255,255,55%)",
					"rgba(68,68,255,55%)", "rgba(255,68,255,55%)", "rgba(68,68,68,55%)",  "rgba(255,255,255,55%)"};

QMenu *PLSBasic::AddBackgroundColorMenu(QMenu *menu, QWidgetAction *widgetAction, ColorSelect *select, obs_sceneitem_t *item)
{
	QAction *action;

	menu->setStyleSheet(QString("*[bgColor=\"1\"]{background-color:rgba(255,68,68,33%);}"
				    "*[bgColor=\"2\"]{background-color:rgba(255,255,68,33%);}"
				    "*[bgColor=\"3\"]{background-color:rgba(68,255,68,33%);}"
				    "*[bgColor=\"4\"]{background-color:rgba(68,255,255,33%);}"
				    "*[bgColor=\"5\"]{background-color:rgba(68,68,255,33%);}"
				    "*[bgColor=\"6\"]{background-color:rgba(255,68,255,33%);}"
				    "*[bgColor=\"7\"]{background-color:rgba(68,68,68,33%);}"
				    "*[bgColor=\"8\"]{background-color:rgba(255,255,255,33%);}"));

	obs_data_t *privData = obs_sceneitem_get_private_settings(item);
	obs_data_release(privData);

	obs_data_set_default_int(privData, "color-preset", 0);
	int preset = obs_data_get_int(privData, "color-preset");

	action = menu->addAction(QTStr("Clear"), this, +SLOT(ColorChange()));
	action->setCheckable(true);
	action->setProperty("bgColor", 0);
	action->setChecked(preset == 0);

	action = menu->addAction(QTStr("CustomColor"), this, +SLOT(ColorChange()));
	action->setCheckable(true);
	action->setProperty("bgColor", 1);
	action->setChecked(preset == 1);

	menu->addSeparator();

	select->setMouseTracking(true);
	widgetAction->setDefaultWidget(select);

	for (int i = 1; i < 9; i++) {
		stringstream button;
		button << "preset" << i;
		QPushButton *colorButton = select->findChild<QPushButton *>(button.str().c_str());
		if (preset == i + 1)
			colorButton->setStyleSheet("border: 2px solid black");
		else
			colorButton->setStyleSheet("border: 2px solid transparent");

		colorButton->setProperty("bgColor", i);
		select->connect(colorButton, SIGNAL(released()), this, SLOT(ColorChange()));
	}

	menu->addAction(widgetAction);

	return menu;
}

ColorSelect::ColorSelect(QWidget *parent) : QWidget(parent), ui(new Ui::ColorSelect)
{
	ui->setupUi(this);
}

void PLSBasic::CreateSourcePopupMenu(int idx, bool preview)
{
	QMenu popup(this);
	delete previewProjectorSource;
	delete sourceProjector;
	delete scaleFilteringMenu;
	delete colorMenu;
	delete colorWidgetAction;
	delete colorSelect;
	delete deinterlaceMenu;

	if (preview) {
		QMenu *previewMenu = popup.addMenu(QTStr("Basic.Main.Preview"));

		QAction *action = previewMenu->addAction(QTStr("Basic.Main.PreviewConextMenu.Enable"), this, SLOT(TogglePreview()));
		action->setCheckable(true);
		action->setChecked(obs_display_enabled(ui->preview->GetDisplay()));
		if (IsPreviewProgramMode())
			action->setEnabled(false);

		previewMenu->addAction(ui->actionLockPreview);

		QMenu *scalingMenu = previewMenu->addMenu(tr("Basic.MainMenu.Edit.Scale"));
		connect(scalingMenu, &QMenu::aboutToShow, this, &PLSBasic::on_scalingMenu_aboutToShow);

		scalingMenu->addAction(ui->actionScaleWindow);
		scalingMenu->addAction(ui->actionScaleCanvas);
		scalingMenu->addAction(ui->actionScaleOutput);

		QMenu *previewProjector = previewMenu->addMenu(QTStr("Basic.MainMenu.PreviewProjector"));

		previewProjectorSource = new QMenu(QTStr("Basic.MainMenu.PreviewProjector.Fullscreen"));
		AddProjectorMenuMonitors(previewProjectorSource, this, SLOT(OpenPreviewProjector()));

		previewProjector->addMenu(previewProjectorSource);

		QAction *previewWindow = previewProjector->addAction(QTStr("Basic.MainMenu.PreviewProjector.Window"), this, SLOT(OpenPreviewWindow()));

		previewProjector->addAction(previewWindow);

		previewMenu->addSeparator();
	}

	QPointer<QMenu> addSourceMenu = CreateAddSourcePopupMenu();
	if (addSourceMenu)
		popup.addMenu(addSourceMenu);

	ui->actionCopyFilters->setEnabled(false);
	ui->actionCopySource->setEnabled(false);

	if (ui->sources->MultipleBaseSelected()) {
		popup.addSeparator();
		popup.addAction(QTStr("Basic.Main.GroupItems"), ui->sources, SLOT(GroupSelectedItems()));

	} else if (ui->sources->GroupsSelected()) {
		popup.addSeparator();
		popup.addAction(QTStr("Basic.Main.Ungroup"), ui->sources, SLOT(UngroupSelectedGroups()));
	}

	popup.addSeparator();
	popup.addAction(ui->actionCopySource);
	popup.addAction(ui->actionPasteRef);
	popup.addAction(ui->actionPasteDup);
	popup.addSeparator();

	// Advance
	QMenu *advanceMenu = new QMenu(tr("Basic.MainMenu.Advance"));
	advanceMenu->addAction(ui->actionCopyFilters);
	advanceMenu->addAction(ui->actionPasteFilters);

	if (idx != -1) {
		if (addSourceMenu)
			popup.addSeparator();

		// recover source context menu
		QMenu *orderMenu = popup.addMenu(tr("Basic.MainMenu.Edit.Order"));
		orderMenu->addAction(ui->actionMoveUp);
		orderMenu->addAction(ui->actionMoveDown);
		orderMenu->addSeparator();
		orderMenu->addAction(ui->actionMoveToTop);
		orderMenu->addAction(ui->actionMoveToBottom);

		// Transform
		QMenu *transformMenu = popup.addMenu(tr("Basic.MainMenu.Edit.Transform"));
		transformMenu->addAction(ui->actionEditTransform);
		transformMenu->addAction(ui->actionCopyTransform);
		transformMenu->addAction(ui->actionPasteTransform);
		transformMenu->addAction(ui->actionResetTransform);
		transformMenu->addSeparator();
		transformMenu->addAction(ui->actionRotate90CW);
		transformMenu->addAction(ui->actionRotate90CCW);
		transformMenu->addAction(ui->actionRotate180);
		transformMenu->addSeparator();
		transformMenu->addAction(ui->actionFlipHorizontal);
		transformMenu->addAction(ui->actionFlipVertical);
		transformMenu->addSeparator();
		transformMenu->addAction(ui->actionFitToScreen);
		transformMenu->addAction(ui->actionStretchToScreen);
		transformMenu->addAction(ui->actionCenterToScreen);
		transformMenu->addAction(ui->actionVerticalCenter);
		transformMenu->addAction(ui->actionHorizontalCenter);
		popup.addSeparator();

		// Advance
		popup.addMenu(advanceMenu);
		popup.addSeparator();

		// Set Color
		OBSSceneItem sceneItem = ui->sources->Get(idx);
		colorMenu = new QMenu(QTStr("ChangeBG"));
		colorWidgetAction = new QWidgetAction(colorMenu);
		colorWidgetAction->setObjectName("colorSelect");
		colorSelect = new ColorSelect(colorMenu);
		advanceMenu->addMenu(AddBackgroundColorMenu(colorMenu, colorWidgetAction, colorSelect, sceneItem));
		advanceMenu->addSeparator();

		// Hide in Mixer
		obs_source_t *source = obs_sceneitem_get_source(sceneItem);
		uint32_t flags = obs_source_get_output_flags(source);
		bool hasAudio = (flags & OBS_SOURCE_AUDIO) == OBS_SOURCE_AUDIO;
		if (hasAudio) {
			QAction *actionHideMixer = advanceMenu->addAction(QTStr("HideMixer"), this, SLOT(ToggleHideMixer()));
			actionHideMixer->setCheckable(true);
			actionHideMixer->setChecked(SourceMixerHidden(source));
		}

		// Deinterlacing
		bool isAsyncVideo = (flags & OBS_SOURCE_ASYNC_VIDEO) == OBS_SOURCE_ASYNC_VIDEO;
		if (isAsyncVideo) {
			deinterlaceMenu = new QMenu(QTStr("Deinterlacing"));
			advanceMenu->addMenu(AddDeinterlacingMenu(deinterlaceMenu, source));
		}
		advanceMenu->addSeparator();

		// Resize output
		QAction *resizeOutput = advanceMenu->addAction(QTStr("ResizeOutputSizeOfSource"), this, SLOT(ResizeOutputSizeOfSource()));

		int width = obs_source_get_width(source);
		int height = obs_source_get_height(source);

		resizeOutput->setEnabled(!obs_video_active());

		if (width == 0 || height == 0)
			resizeOutput->setEnabled(false);

		// Scale Filtering
		scaleFilteringMenu = new QMenu(QTStr("ScaleFiltering"));
		advanceMenu->addMenu(AddScaleFilteringMenu(scaleFilteringMenu, sceneItem));
		advanceMenu->addSeparator();

		// Source Projector
		QMenu *sourceMenu = popup.addMenu(QTStr("Basic.MainMenu.SourceProjector"));
		sourceProjector = new QMenu(QTStr("Basic.MainMenu.SourceProjector.Fullscreen"));
		AddProjectorMenuMonitors(sourceProjector, this, SLOT(OpenSourceProjector()));

		QAction *sourceWindow = sourceMenu->addAction(QTStr("Basic.MainMenu.SourceProjector.Window"), this, SLOT(OpenSourceWindow()));
		sourceMenu->addMenu(sourceProjector);
		sourceMenu->addAction(sourceWindow);
		popup.addSeparator();

		// Interact
		QAction *action = popup.addAction(QTStr("Interact"), this, SLOT(on_actionInteract_triggered()));
		action->setEnabled(obs_source_get_output_flags(source) & OBS_SOURCE_INTERACTION);

		// Filters Properties
		popup.addAction(QTStr("Filters"), this, SLOT(OpenFilters()));
		popup.addAction(QTStr("Properties"), this, SLOT(on_actionSourceProperties_triggered()));
		popup.addSeparator();

		// Rename Delete
		popup.addAction(QTStr("Rename"), this, SLOT(EditSceneItemName()));
		popup.addAction(QTStr("Delete"), this, SLOT(on_actionRemoveSource_triggered()));

		ui->actionCopyFilters->setEnabled(true);
		ui->actionCopySource->setEnabled(true);
	} else {
		popup.addMenu(advanceMenu);
		ui->actionPasteFilters->setEnabled(false);
	}

	popup.exec(QCursor::pos());
}

void PLSBasic::on_sources_customContextMenuRequested(const QPoint &pos)
{
	if (PLSSceneDataMgr::Instance()->GetSceneSize()) {
		QModelIndex idx = ui->sources->indexAt(pos);
		CreateSourcePopupMenu(idx.row(), false);
	}
}

void PLSBasic::OnScenesItemDoubleClicked()
{
	if (IsPreviewProgramMode()) {
		bool doubleClickSwitch = config_get_bool(App()->GlobalConfig(), "BasicWindow", "TransitionOnDoubleClick");

		if (doubleClickSwitch)
			TransitionClicked();
	}
}

void PLSBasic::AddSource(const char *id)
{
	if (id && *id) {
		PLS_UI_STEP(SOURCE_MODULE, "addSourceMenu", ACTION_CLICK);
		PLSBasicSourceSelect sourceSelect(this, id);
		if (sourceSelect.exec() == QDialog::Accepted)
			action::SendActionLog(action::ActionInfo(EVENT_MAIN_ADD, EVENT_SUB_SOURCE_ADDED, EVENT_TYPE_CONFIRMED, action::GetActionSourceID(id)));
		if (sourceSelect.newSource && strcmp(id, "group") != 0)
			CreatePropertiesWindow(sourceSelect.newSource, OPERATION_ADD_SOURCE, this);
	}
}

void PLSBasic::BindActionData(QAction *popupItem, const char *type)
{
	popupItem->setData(QT_UTF8(type));
	connect(popupItem, SIGNAL(triggered(bool)), this, SLOT(AddSourceFromAction()));

	QIcon icon;

	if (strcmp(type, SCENE_SOURCE_ID) == 0)
		icon = GetSceneIcon();
	else
		icon = GetSourceIcon(type);

	popupItem->setIcon(icon);
}

QMenu *PLSBasic::CreateAddSourcePopupMenu()
{
	QMenu *popup = new QMenu(QTStr("Add"), this);

	PLSAddSourceMenuStyle *style = new PLSAddSourceMenuStyle;
	popup->setStyle(style);
	popup->setStyleSheet("QMenu::item{padding-left:11px;}");

	bool foundInputSource = false;
	std::vector<std::vector<QString>> presetTypeList;
	std::vector<QString> otherTypeList;
	GetSourceTypeList(presetTypeList, otherTypeList);

	// add preset source type
	for (auto iter = presetTypeList.begin(); iter != presetTypeList.end(); ++iter) {
		std::vector<QString> &subList = *iter;
		for (unsigned i = 0; i < subList.size(); ++i) {
			QString id = subList[i];
			QString displayName;
			if (strcmp(id.toStdString().c_str(), SCENE_SOURCE_ID) == 0) {
				displayName = Str("Basic.Scene");
			} else {
				foundInputSource = true;
				displayName = obs_source_get_display_name(id.toStdString().c_str());
			}

			QAction *action = new QAction(displayName, this);
			BindActionData(action, id.toStdString().c_str());
			popup->addAction(action);
		}

		popup->addSeparator();
	}

	//add other source type
	if (!otherTypeList.empty()) {
		for (int i = 0; i < otherTypeList.size(); i++) {
			QAction *action = new QAction(obs_source_get_display_name(otherTypeList[i].toStdString().c_str()), this);
			BindActionData(action, otherTypeList[i].toStdString().c_str());
			popup->addAction(action);
		}
		popup->addSeparator();
		foundInputSource = true;
	}

	QAction *addGroup = new QAction(QTStr("Group"), this);
	addGroup->setData(QT_UTF8("group"));
	addGroup->setIcon(GetGroupIcon());
	connect(addGroup, SIGNAL(triggered(bool)), this, SLOT(AddSourceFromAction()));
	popup->addAction(addGroup);

	if (!foundInputSource) {
		PLS_WARN(SOURCE_MODULE, "Cann't find any input source and won't popup add menu");
		delete popup;
		popup = nullptr;
	}
	return popup;
}

void PLSBasic::AddSourceFromAction()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action)
		return;

	AddSource(QT_TO_UTF8(action->data().toString()));
}

void PLSBasic::AddSourcePopupMenu(const QPoint &pos)
{
	if (!GetCurrentScene()) {
		// Tell the user he needs a scene first (help beginners).
		PLSMessageBox::information(this, QTStr("Basic.Main.AddSourceHelp.Title"), QTStr("Basic.Main.AddSourceHelp.Text"));
		return;
	}

	QScopedPointer<QMenu> popup(CreateAddSourcePopupMenu());
	if (popup)
		popup->exec(pos);
}

void PLSBasic::on_actionAddSource_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Add Source", ACTION_CLICK);
	AddSourcePopupMenu(QCursor::pos());
}

void PLSBasic::on_actionRemoveScene_triggered()
{
	ui->scenesFrame->OnDeleteSceneButtonClicked(this->GetCurrentSceneItem());
}

static bool remove_items(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	vector<OBSSceneItem> &items = *reinterpret_cast<vector<OBSSceneItem> *>(param);

	if (obs_sceneitem_selected(item)) {
		items.emplace_back(item);
	} else if (obs_sceneitem_is_group(item)) {
		obs_sceneitem_group_enum_items(item, remove_items, &items);
	}
	return true;
};

void PLSBasic::on_actionRemoveSource_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, QT_TO_UTF8(QTStr("Delete")), ACTION_CLICK);
	vector<OBSSceneItem> items;

	obs_scene_enum_items(GetCurrentScene(), remove_items, &items);

	if (!items.size())
		return;

	auto removeMultiple = [this](size_t count) {
		QString text = QTStr("ConfirmRemove.TextMultiple").arg(QString::number(count));
		return PLSAlertView::Button::Ok == PLSMessageBox::question(getMainView(), QTStr("ConfirmRemove.Title"), text, PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel);
	};

	if (items.size() == 1) {
		OBSSceneItem &item = items[0];
		obs_source_t *source = obs_sceneitem_get_source(item);

		if (source && QueryRemoveSource(source))
			obs_sceneitem_remove(item);
	} else {
		if (removeMultiple(items.size())) {
			for (auto &item : items)
				obs_sceneitem_remove(item);
		}
	}
}

void PLSBasic::on_actionInteract_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Interact", ACTION_CLICK);

	OBSSceneItem item = GetCurrentSceneItemData();
	OBSSource source = obs_sceneitem_get_source(item);

	if (source)
		CreateInteractionWindow(source);
}

void PLSBasic::on_actionSourceProperties_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Source Properties", ACTION_CLICK);

	OBSSceneItem item = GetCurrentSceneItemData();
	OBSSource source = obs_sceneitem_get_source(item);

	if (source)
		CreatePropertiesWindow(source, OPERATION_NONE, this);
}

void PLSBasic::on_actionSourceUp_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Source Up", ACTION_CLICK);

	OBSSceneItem item = GetCurrentSceneItemData();
	obs_sceneitem_set_order(item, OBS_ORDER_MOVE_UP);
}

void PLSBasic::on_actionSourceDown_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Source Down", ACTION_CLICK);

	OBSSceneItem item = GetCurrentSceneItemData();
	obs_sceneitem_set_order(item, OBS_ORDER_MOVE_DOWN);
}

void PLSBasic::on_actionMoveUp_triggered()
{
	OBSSceneItem item = GetCurrentSceneItemData();
	obs_sceneitem_set_order(item, OBS_ORDER_MOVE_UP);
	PLS_UI_STEP(MAINFRAME_MODULE, "Move Up", ACTION_CLICK);
}

void PLSBasic::on_actionMoveDown_triggered()
{
	OBSSceneItem item = GetCurrentSceneItemData();
	obs_sceneitem_set_order(item, OBS_ORDER_MOVE_DOWN);
	PLS_UI_STEP(MAINFRAME_MODULE, "Move Down", ACTION_CLICK);
}

void PLSBasic::on_actionMoveToTop_triggered()
{
	OBSSceneItem item = GetCurrentSceneItemData();
	obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
	PLS_UI_STEP(MAINFRAME_MODULE, "Move To Top", ACTION_CLICK);
}

void PLSBasic::on_actionMoveToBottom_triggered()
{
	OBSSceneItem item = GetCurrentSceneItemData();
	obs_sceneitem_set_order(item, OBS_ORDER_MOVE_BOTTOM);
	PLS_UI_STEP(MAINFRAME_MODULE, "Move To Bottom", ACTION_CLICK);
}

void PLSBasic::OpenFilters()
{
	OBSSceneItem item = GetCurrentSceneItemData();
	OBSSource source = obs_sceneitem_get_source(item);

	PLS_UI_STEP(MAINFRAME_MODULE, "Menu Open Source Filter", ACTION_CLICK);

	CreateFiltersWindow(source);
}

void PLSBasic::OpenSceneFilters()
{
	OBSScene scene = GetCurrentScene();
	OBSSource source = obs_scene_get_source(scene);

	PLS_UI_STEP(MAINFRAME_MODULE, "Menu Open Scene Filter", ACTION_CLICK);

	CreateFiltersWindow(source);
}

#define RECORDING_START "==== Recording Start ==============================================="
#define RECORDING_STOP "==== Recording Stop ================================================"
#define REPLAY_BUFFER_START "==== Replay Buffer Start ==========================================="
#define REPLAY_BUFFER_STOP "==== Replay Buffer Stop ============================================"
#define STREAMING_START "==== Streaming Start ==============================================="
#define STREAMING_STOP "==== Streaming Stop ================================================"

void PLSBasic::StartStreaming()
{
	PLS_INFO("main", "main: %s", "StartStreaming");
	if (outputHandler->StreamingActive()) {
		PLS_PLATFORM_API->onLiveStarted();
		return;
	}
	if (disableOutputsRef) {
		PLS_PLATFORM_API->onLiveEnded();
		return;
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_STREAMING_STARTING);

	SaveProject();

	if (sysTrayStream) {
		sysTrayStream->setEnabled(false);
		sysTrayStream->setText(QTStr("Basic.Main.Connecting"));
	}

	if (!outputHandler->StartStreaming(service)) {
		QString message = !outputHandler->lastError.empty() ? QTStr(outputHandler->lastError.c_str()) : QTStr("Output.StartFailedGeneric");
		if (sysTrayStream) {
			sysTrayStream->setText(QTStr("Basic.Main.StartStreaming"));
			sysTrayStream->setEnabled(true);
		}

		PLSAlertView::critical(this, QTStr("Output.StartStreamFailed"), message);
		PLS_PLATFORM_API->onLiveEnded();
		return;
	}

	bool recordWhenStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	if (recordWhenStreaming) {
		StartRecording();
	}
	bool replayBufferWhileStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "ReplayBufferWhileStreaming");
	if (replayBufferWhileStreaming) {
		StartReplayBufferWithNoCheck();
	}
}

#ifdef _WIN32
static inline void UpdateProcessPriority()
{
	const char *priority = config_get_string(App()->GlobalConfig(), "General", "ProcessPriority");
	if (priority && strcmp(priority, "Normal") != 0)
		SetProcessPriority(priority);
}

static inline void ClearProcessPriority()
{
	const char *priority = config_get_string(App()->GlobalConfig(), "General", "ProcessPriority");
	if (priority && strcmp(priority, "Normal") != 0)
		SetProcessPriority("Normal");
}
#else
#define UpdateProcessPriority() \
	do {                    \
	} while (false)
#define ClearProcessPriority() \
	do {                   \
	} while (false)
#endif

inline void PLSBasic::OnActivate()
{
	if (ui->profileMenu->isEnabled()) {
		ui->profileMenu->setEnabled(false);
		ui->autoConfigure->setEnabled(false);
		App()->IncrementSleepInhibition();
		UpdateProcessPriority();

		if (trayIcon)
			trayIcon->setIcon(QIcon::fromTheme("obs-tray-active", QIcon(":/res/images/PRISMLiveStudio.ico")));
	}
}

inline void PLSBasic::OnDeactivate()
{
	if (!outputHandler->Active() && !ui->profileMenu->isEnabled()) {
		ui->profileMenu->setEnabled(true);
		ui->autoConfigure->setEnabled(true);
		App()->DecrementSleepInhibition();
		ClearProcessPriority();

		if (trayIcon)
			trayIcon->setIcon(QIcon::fromTheme("obs-tray", QIcon(":/res/images/PRISMLiveStudio.ico")));
	} else {
		if (trayIcon)
			trayIcon->setIcon(QIcon(":/res/images/PRISMLiveStudio.ico"));
	}
}

void PLSBasic::StopStreaming()
{
	PLS_INFO("main", "main: %s", "StopStreaming");
	SaveProject();

	if (outputHandler->StreamingActive())
		outputHandler->StopStreaming(m_bFastStop || streamingStopping);

	OnDeactivate();

	bool recordWhenStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	bool keepRecordingWhenStreamStops = config_get_bool(GetGlobalConfig(), "BasicWindow", "KeepRecordingWhenStreamStops");
	if (recordWhenStreaming && !keepRecordingWhenStreamStops)
		StopRecording();

	bool replayBufferWhileStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "ReplayBufferWhileStreaming");
	bool keepReplayBufferStreamStops = config_get_bool(GetGlobalConfig(), "BasicWindow", "KeepReplayBufferStreamStops");
	if (replayBufferWhileStreaming && !keepReplayBufferStreamStops)
		StopReplayBuffer();
}

void PLSBasic::ForceStopStreaming()
{
	SaveProject();

	if (outputHandler->StreamingActive())
		outputHandler->StopStreaming(true);

	OnDeactivate();

	bool recordWhenStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	bool keepRecordingWhenStreamStops = config_get_bool(GetGlobalConfig(), "BasicWindow", "KeepRecordingWhenStreamStops");
	if (recordWhenStreaming && !keepRecordingWhenStreamStops)
		StopRecording();

	bool replayBufferWhileStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "ReplayBufferWhileStreaming");
	bool keepReplayBufferStreamStops = config_get_bool(GetGlobalConfig(), "BasicWindow", "KeepReplayBufferStreamStops");
	if (replayBufferWhileStreaming && !keepReplayBufferStreamStops)
		StopReplayBuffer();
}

void PLSBasic::StreamDelayStarting(int sec)
{
	//ui->streamButton->setText(QTStr("Basic.Main.StopStreaming"));
	//ui->streamButton->setEnabled(true);
	//ui->streamButton->setChecked(true);

	if (sysTrayStream) {
		sysTrayStream->setText(QTStr("Basic.Main.StopStreaming"));
		sysTrayStream->setEnabled(true);
	}

	if (!startStreamMenu.isNull())
		startStreamMenu->deleteLater();

	startStreamMenu = new QMenu();
	startStreamMenu->addAction(QTStr("Basic.Main.StopStreaming"), this, SLOT(StopStreaming()));
	startStreamMenu->addAction(QTStr("Basic.Main.ForceStopStreaming"), this, SLOT(ForceStopStreaming()));
	//ui->streamButton->setMenu(startStreamMenu);

	mainView->statusBar()->StreamDelayStarting(sec);

	OnActivate();
}

void PLSBasic::StreamDelayStopping(int sec)
{
	//ui->streamButton->setText(QTStr("Basic.Main.StartStreaming"));
	//ui->streamButton->setEnabled(true);
	//ui->streamButton->setChecked(false);

	if (sysTrayStream) {
		sysTrayStream->setText(QTStr("Basic.Main.StartStreaming"));
		sysTrayStream->setEnabled(true);
	}

	if (!startStreamMenu.isNull())
		startStreamMenu->deleteLater();

	startStreamMenu = new QMenu();
	startStreamMenu->addAction(QTStr("Basic.Main.StartStreaming"), this, SLOT(StartStreaming()));
	startStreamMenu->addAction(QTStr("Basic.Main.ForceStopStreaming"), this, SLOT(ForceStopStreaming()));
	//ui->streamButton->setMenu(startStreamMenu);

	mainView->statusBar()->StreamDelayStopping(sec);

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_STREAMING_STOPPING);
}

void PLSBasic::StreamingStart()
{
	PLS_INFO("main", "main: %s", "StreamingStart");
	mainView->statusBar()->StreamStarted(outputHandler->streamOutput);

	if (sysTrayStream) {
		sysTrayStream->setText(QTStr("Basic.Main.StopStreaming"));
		sysTrayStream->setEnabled(true);
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_STREAMING_STARTED);

	ui->previewTitle->OnLiveStatus(true);

	OnActivate();

	blog(LOG_INFO, STREAMING_START);
}

void PLSBasic::StreamStopping()
{
	PLS_INFO("main", "main: %s", "StreamStopping");
	ui->previewTitle->OnLiveStatus(false);

	if (sysTrayStream)
		sysTrayStream->setText(QTStr("Basic.Main.StoppingStreaming"));

	streamingStopping = true;
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_STREAMING_STOPPING);
}

void PLSBasic::StreamingStop(int code, QString last_error)
{
	PLS_INFO("main", "main: %s", "StreamingStop");
	const char *errorDescription = "";
	DStr errorMessage;
	bool use_last_error = false;
	bool encode_error = false;

	switch (code) {
	case OBS_OUTPUT_BAD_PATH:
		errorDescription = Str("Output.ConnectFail.BadPath");
		break;

	case OBS_OUTPUT_CONNECT_FAILED:
		use_last_error = true;
		errorDescription = Str("Output.ConnectFail.ConnectFailed");
		break;

	case OBS_OUTPUT_INVALID_STREAM:
		errorDescription = Str("Output.ConnectFail.InvalidStream");
		break;

	case OBS_OUTPUT_ENCODE_ERROR:
		encode_error = true;
		break;

	default:
	case OBS_OUTPUT_ERROR:
		use_last_error = true;
		errorDescription = Str("Output.ConnectFail.Error");
		break;

	case OBS_OUTPUT_DISCONNECTED:
		/* doesn't happen if output is set to reconnect.  note that
		 * reconnects are handled in the output, not in the UI */
		use_last_error = true;
		errorDescription = Str("Output.ConnectFail.Disconnected");
	}

	if (use_last_error && !last_error.isEmpty())
		dstr_printf(errorMessage, "%s\n\n%s", errorDescription, QT_TO_UTF8(last_error));
	else
		dstr_copy(errorMessage, errorDescription);

	mainView->statusBar()->StreamStopped();

	ui->previewTitle->OnLiveStatus(false);

	//ui->streamButton->setText(QTStr("Basic.Main.StartStreaming"));
	//ui->streamButton->setEnabled(true);
	//ui->streamButton->setChecked(false);

	if (sysTrayStream) {
		sysTrayStream->setText(QTStr("Basic.Main.StartStreaming"));
		sysTrayStream->setEnabled(true);
	}

	streamingStopping = false;
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_STREAMING_STOPPED);

	OnDeactivate();

	blog(LOG_INFO, STREAMING_STOP);

	if (encode_error) {
		PLSMessageBox::information(this, QTStr("Output.StreamEncodeError.Title"), QTStr("Output.StreamEncodeError.Msg"));

	} else if (code != OBS_OUTPUT_SUCCESS && mainView->isVisible()) {
		PLSMessageBox::information(this, QTStr("Output.ConnectFail.Title"), QT_UTF8(errorMessage));

	} else if (code != OBS_OUTPUT_SUCCESS && !mainView->isVisible()) {
		SysTrayNotify(QT_UTF8(errorDescription), QSystemTrayIcon::Warning);
	}

	if (!startStreamMenu.isNull()) {
		//ui->streamButton->setMenu(nullptr);
		startStreamMenu->deleteLater();
		startStreamMenu = nullptr;
	}
}

void PLSBasic::AutoRemux()
{
	const char *mode = config_get_string(basicConfig, "Output", "Mode");
	bool advanced = astrcmpi(mode, "Advanced") == 0;

	const char *path = !advanced ? config_get_string(basicConfig, "SimpleOutput", "FilePath") : config_get_string(basicConfig, "AdvOut", "RecFilePath");

	/* do not save if using FFmpeg output in advanced output mode */
	if (advanced) {
		const char *type = config_get_string(basicConfig, "AdvOut", "RecType");
		if (astrcmpi(type, "FFmpeg") == 0) {
			return;
		}
	}

	QString input;
	input += path;
	input += "/";
	input += remuxFilename.c_str();

	QFileInfo fi(remuxFilename.c_str());

	/* do not remux if lossless */
	if (fi.suffix().compare("avi", Qt::CaseInsensitive) == 0) {
		return;
	}

	QString output;
	output += path;
	output += "/";
	output += fi.completeBaseName();
	output += ".mp4";

	PLSRemux *remux = new PLSRemux(path, this, true);
	//remux->show();
	remux->AutoRemux(input, output);
}

bool PLSBasic::StartRecording()
{
	PLS_INFO("main", "main: %s", "StartRecording");
	if (outputHandler->RecordingActive()) {
		return false;
	}

	if (disableOutputsRef) {
		return false;
	}

	if (LowDiskSpace()) {
		DiskSpaceMessage();
		return false;
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_RECORDING_STARTING);

	SaveProject();

	if (!outputHandler->StartRecording() && api) {
		api->on_event(OBS_FRONTEND_EVENT_RECORDING_STOPPED);
		return false;
	}
	return true;
}

void PLSBasic::RecordStopping()
{
	PLS_INFO("main", "main: %s", "RecordStopping");
	ui->previewTitle->OnRecordStatus(false);

	if (sysTrayRecord)
		sysTrayRecord->setText(QTStr("Basic.Main.StoppingRecording"));

	recordingStopping = true;
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_RECORDING_STOPPING);
}

bool PLSBasic::StopRecording()
{
	PLS_INFO("main", "main: %s", "StopRecording");
	SaveProject();

	if (outputHandler->RecordingActive())
		outputHandler->StopRecording(m_bFastStop || recordingStopping);

	OnDeactivate();
	return true;
}

void PLSBasic::RecordingStart()
{
	PLS_INFO("main", "main: %s", "RecordingStart");
	mainView->statusBar()->RecordingStarted(outputHandler->fileOutput);

	if (sysTrayRecord)
		sysTrayRecord->setText(QTStr("Basic.Main.StopRecording"));

	recordingStopping = false;
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_RECORDING_STARTED);

	if (!diskFullTimer->isActive())
		diskFullTimer->start(1000);

	ui->previewTitle->OnRecordStatus(true);

	OnActivate();
	UpdatePause();

	blog(LOG_INFO, RECORDING_START);
}

void PLSBasic::RecordingStop(int code, QString last_error)
{
	PLS_INFO("main", "main: %s", "RecordingStop");
	mainView->statusBar()->RecordingStopped();

	ui->previewTitle->OnRecordStatus(false);

	if (sysTrayRecord)
		sysTrayRecord->setText(QTStr("Basic.Main.StartRecording"));

	blog(LOG_INFO, RECORDING_STOP);

	if (code == OBS_OUTPUT_UNSUPPORTED && mainView->isVisible()) {
		PLSMessageBox::critical(this, QTStr("Output.RecordFail.Title"), QTStr("Output.RecordFail.Unsupported"));

	} else if (code == OBS_OUTPUT_ENCODE_ERROR && mainView->isVisible()) {
		PLSMessageBox::warning(this, QTStr("Output.RecordError.Title"), QTStr("Output.RecordError.EncodeErrorMsg"));

	} else if (code == OBS_OUTPUT_NO_SPACE && mainView->isVisible()) {
		PLSMessageBox::warning(this, QTStr("Output.RecordNoSpace.Title"), QTStr("Output.RecordNoSpace.Msg"));

	} else if (code != OBS_OUTPUT_SUCCESS && mainView->isVisible()) {

		const char *errorDescription;
		DStr errorMessage;
		bool use_last_error = true;

		errorDescription = Str("Output.RecordError.Msg");

		if (use_last_error && !last_error.isEmpty())
			dstr_printf(errorMessage, "%s\n\n%s", errorDescription, QT_TO_UTF8(last_error));
		else
			dstr_copy(errorMessage, errorDescription);

		PLSMessageBox::critical(this, QTStr("Output.RecordError.Title"), QT_UTF8(errorMessage));

	} else if (code == OBS_OUTPUT_UNSUPPORTED && !mainView->isVisible()) {
		SysTrayNotify(QTStr("Output.RecordFail.Unsupported"), QSystemTrayIcon::Warning);

	} else if (code == OBS_OUTPUT_NO_SPACE && !mainView->isVisible()) {
		SysTrayNotify(QTStr("Output.RecordNoSpace.Msg"), QSystemTrayIcon::Warning);

	} else if (code != OBS_OUTPUT_SUCCESS && !mainView->isVisible()) {
		SysTrayNotify(QTStr("Output.RecordError.Msg"), QSystemTrayIcon::Warning);
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_RECORDING_STOPPED);

	if (diskFullTimer->isActive())
		diskFullTimer->stop();

	if (remuxAfterRecord)
		AutoRemux();

	OnDeactivate();
	UpdatePause(false);
}

#define RP_NO_HOTKEY_TITLE QTStr("Output.ReplayBuffer.NoHotkey.Title")
#define RP_NO_HOTKEY_TEXT QTStr("Output.ReplayBuffer.NoHotkey.Msg")

extern volatile bool recording_paused;
extern volatile bool replaybuf_active;

void PLSBasic::ShowReplayBufferPauseWarning()
{
	auto msgBox = []() {
		auto result = PLSAlertView::information(App()->getMainView(), QTStr("Output.ReplayBuffer.PauseWarning.Title"), QTStr("Output.ReplayBuffer.PauseWarning.Text"), QTStr("DoNotShowAgain"));
		if (result.isChecked) {
			config_set_bool(App()->GlobalConfig(), "General", "WarnedAboutReplayBufferPausing", true);
			config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
		}
	};

	bool warned = config_get_bool(App()->GlobalConfig(), "General", "WarnedAboutReplayBufferPausing");
	if (!warned) {
		QMetaObject::invokeMethod(App(), "Exec", Qt::QueuedConnection, Q_ARG(VoidFunc, msgBox));
	}
}

// add by zzc
bool PLSBasic::ReplayBufferPreCheck()
{
	if (!outputHandler || !outputHandler->replayBuffer)
		return false;
	if (outputHandler->ReplayBufferActive())
		return false;
	if (disableOutputsRef)
		return false;

	if (!NoSourcesConfirmation()) {
		return false;
	}

	if (LowDiskSpace()) {
		DiskSpaceMessage();
		return false;
	}

	return true;
}

//add by zzc
bool PLSBasic::StartReplayBufferWithNoCheck()
{
	if (!outputHandler || !outputHandler->replayBuffer)
		return false;
	if (outputHandler->ReplayBufferActive())
		return false;
	if (disableOutputsRef)
		return false;

	obs_output_t *output = outputHandler->replayBuffer;
	obs_data_t *hotkeys = obs_hotkeys_save_output(output);
	obs_data_array_t *bindings = obs_data_get_array(hotkeys, "ReplayBuffer.Save");
	size_t count = obs_data_array_count(bindings);
	obs_data_array_release(bindings);
	obs_data_release(hotkeys);

	if (!count) {
		return false;
	}

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING);
	}

	SaveProject();

	if (!outputHandler->StartReplayBuffer()) {
		return true;
	} else if (os_atomic_load_bool(&recording_paused)) {
		ShowReplayBufferPauseWarning();
	}

	return true;
}

void PLSBasic::StartReplayBuffer()
{
	PLS_INFO("main", "main: %s", "StartReplayBuffer");
	if (!outputHandler || !outputHandler->replayBuffer)
		return;
	if (outputHandler->ReplayBufferActive())
		return;
	if (disableOutputsRef)
		return;

	if (!NoSourcesConfirmation()) {
		return;
	}

	if (LowDiskSpace()) {
		DiskSpaceMessage();
		return;
	}

	obs_output_t *output = outputHandler->replayBuffer;
	obs_data_t *hotkeys = obs_hotkeys_save_output(output);
	obs_data_array_t *bindings = obs_data_get_array(hotkeys, "ReplayBuffer.Save");
	size_t count = obs_data_array_count(bindings);
	obs_data_array_release(bindings);
	obs_data_release(hotkeys);

	if (!count) {
		PLSMessageBox::information(this, RP_NO_HOTKEY_TITLE, RP_NO_HOTKEY_TEXT);
		//replayBufferButton->setChecked(false);
		return;
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING);

	SaveProject();

	if (!outputHandler->StartReplayBuffer()) {
		//replayBufferButton->setChecked(false);
	} else if (os_atomic_load_bool(&recording_paused)) {
		ShowReplayBufferPauseWarning();
	}
}

void PLSBasic::ReplayBufferStopping()
{
	PLS_INFO("main", "main: %s", "ReplayBufferStopping");
	if (!outputHandler || !outputHandler->replayBuffer)
		return;

	replayBufferStopping = true;
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING);
}

void PLSBasic::StopReplayBuffer()
{
	PLS_INFO("main", "main: %s", "StopReplayBuffer");
	if (!outputHandler || !outputHandler->replayBuffer)
		return;

	SaveProject();

	if (outputHandler->ReplayBufferActive())
		outputHandler->StopReplayBuffer(replayBufferStopping);

	OnDeactivate();
}

void PLSBasic::ReplayBufferStart()
{
	PLS_INFO("main", "main: %s", "ReplayBufferStart");
	if (!outputHandler || !outputHandler->replayBuffer)
		return;

	replayBufferStopping = false;
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED);

	OnActivate();

	blog(LOG_INFO, REPLAY_BUFFER_START);
}

void PLSBasic::ReplayBufferSave()
{
	if (!outputHandler || !outputHandler->replayBuffer)
		return;
	if (!outputHandler->ReplayBufferActive())
		return;

	calldata_t cd = {0};
	proc_handler_t *ph = obs_output_get_proc_handler(outputHandler->replayBuffer);
	proc_handler_call(ph, "save", &cd);
	calldata_free(&cd);
}

void PLSBasic::ReplayBufferStop(int code)
{
	PLS_INFO("main", "main: %s", "ReplayBufferStop");
	if (!outputHandler || !outputHandler->replayBuffer)
		return;

	blog(LOG_INFO, REPLAY_BUFFER_STOP);

	if (code == OBS_OUTPUT_UNSUPPORTED && mainView->isVisible()) {
		PLSMessageBox::critical(this, QTStr("Output.RecordFail.Title"), QTStr("Output.RecordFail.Unsupported"));

	} else if (code == OBS_OUTPUT_NO_SPACE && mainView->isVisible()) {
		PLSMessageBox::warning(this, QTStr("Output.RecordNoSpace.Title"), QTStr("Output.RecordNoSpace.Msg"));

	} else if (code != OBS_OUTPUT_SUCCESS && mainView->isVisible()) {
		PLSMessageBox::critical(this, QTStr("Output.RecordError.Title"), QTStr("Output.RecordError.Msg"));

	} else if (code == OBS_OUTPUT_UNSUPPORTED && !mainView->isVisible()) {
		SysTrayNotify(QTStr("Output.RecordFail.Unsupported"), QSystemTrayIcon::Warning);

	} else if (code == OBS_OUTPUT_NO_SPACE && !mainView->isVisible()) {
		SysTrayNotify(QTStr("Output.RecordNoSpace.Msg"), QSystemTrayIcon::Warning);

	} else if (code != OBS_OUTPUT_SUCCESS && !mainView->isVisible()) {
		SysTrayNotify(QTStr("Output.RecordError.Msg"), QSystemTrayIcon::Warning);
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);

	OnDeactivate();
}

void PLSBasic::ReplayBufferSaved(int code, const QString &errorStr)
{
	if (code == 0) { // save ok
		pls_toast_message(pls_toast_info_type::PLS_TOAST_REPLY_BUFFER, tr("ReplayBuffer.Video.SaveOk"));
	} else { // save failed
		 // pls_toast_message(PLS_TOAST_ERROR, tr("ReplayBuffer.Video.SaveFailed").arg(errorStr));
	}
}

bool PLSBasic::NoSourcesConfirmation()
{
	if (CountVideoSources() == 0 && mainView->isVisible()) {
		QString msg;
		msg = QTStr("NoSources.Text");
		msg += "\n\n";
		msg += QTStr("NoSources.Text.AddSource");
		if (PLSAlertView::Button::Ok != PLSMessageBox::question(getMainView(), QTStr("NoSources.Title"), msg, PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel)) {
			return false;
		}
	}

	return true;
}

void PLSBasic::on_streamButton_clicked()
{
	if (outputHandler->StreamingActive()) {
		bool confirm = config_get_bool(GetGlobalConfig(), "BasicWindow", "WarnBeforeStoppingStream");

		if (confirm && mainView->isVisible()) {
			if (PLSMessageBox::question(this, QTStr("ConfirmStop.Title"), QTStr("ConfirmStop.Text")) != PLSAlertView::Button::Yes) {
				//ui->streamButton->setChecked(true);
				return;
			}
		}

		StopStreaming();
	} else {
		if (!NoSourcesConfirmation()) {
			//ui->streamButton->setChecked(false);
			return;
		}

		bool confirm = config_get_bool(GetGlobalConfig(), "BasicWindow", "WarnBeforeStartingStream");

		obs_data_t *settings = obs_service_get_settings(service);
		bool bwtest = obs_data_get_bool(settings, "bwtest");
		obs_data_release(settings);

		if (bwtest && mainView->isVisible()) {
			if (PLSMessageBox::question(this, QTStr("ConfirmBWTest.Title"), QTStr("ConfirmBWTest.Text")) != PLSAlertView::Button::Yes) {
				//ui->streamButton->setChecked(false);
				return;
			}
		} else if (confirm && mainView->isVisible()) {
			if (PLSMessageBox::question(this, QTStr("ConfirmStart.Title"), QTStr("ConfirmStart.Text")) != PLSAlertView::Button::Yes) {
				//ui->streamButton->setChecked(false);
				return;
			}
		}

		StartStreaming();
	}
}

bool PLSBasic::startStreamingCheck()
{
	if (!NoSourcesConfirmation()) {
		//ui->streamButton->setChecked(false);
		return false;
	}

	bool confirm = config_get_bool(GetGlobalConfig(), "BasicWindow", "WarnBeforeStartingStream");

	obs_data_t *settings = obs_service_get_settings(service);
	bool bwtest = obs_data_get_bool(settings, "bwtest");
	obs_data_release(settings);

	if (bwtest && mainView->isVisible()) {
		if (PLSMessageBox::question(this, QTStr("ConfirmBWTest.Title"), QTStr("ConfirmBWTest.Text")) != PLSAlertView::Button::Yes) {
			//ui->streamButton->setChecked(false);
			return false;
		}
	} else if (confirm && mainView->isVisible()) {
		if (PLSMessageBox::question(this, QTStr("ConfirmStart.Title"), QTStr("ConfirmStart.Text")) != PLSAlertView::Button::Yes) {
			//ui->streamButton->setChecked(false);
			return false;
		}
	}

	return true;
}

bool PLSBasic::stopStreamingCheck()
{
	if (outputHandler->StreamingActive()) {
		bool confirm = config_get_bool(GetGlobalConfig(), "BasicWindow", "WarnBeforeStoppingStream");

		if (confirm && mainView->isVisible()) {
			if (PLSMessageBox::question(this, QTStr("ConfirmStop.Title"), QTStr("ConfirmStop.Text")) != PLSAlertView::Button::Yes) {
				return false;
			}
		}
	}
	return true;
}

bool PLSBasic::startRecordCheck()
{
	if (!NoSourcesConfirmation()) {
		//ui->recordButton->setChecked(false);
		return false;
	}
	return true;
}

bool PLSBasic::stopRecordCheck()
{
	if (outputHandler->RecordingActive()) {

		bool confirm = config_get_bool(GetGlobalConfig(), "BasicWindow", "WarnBeforeStoppingRecord");

		if (confirm && isVisible()) {
			if (PLSMessageBox::question(this, QTStr("ConfirmStopRecord.Title"), QTStr("ConfirmStopRecord.Text")) != PLSAlertView::Button::Yes) {
				return false;
			}
		}
	}
	return true;
}

void PLSBasic::on_recordButton_clicked()
{
	if (outputHandler->RecordingActive()) {
		bool confirm = config_get_bool(GetGlobalConfig(), "BasicWindow", "WarnBeforeStoppingRecord");

		if (confirm && mainView->isVisible()) {
			if (PLSMessageBox::question(this, QTStr("ConfirmStopRecord.Title"), QTStr("ConfirmStopRecord.Text")) != PLSAlertView::Button::Yes) {
				//ui->recordButton->setChecked(true);
				return;
			}
		}
		StopRecording();
	} else {
		if (!NoSourcesConfirmation()) {
			//ui->recordButton->setChecked(false);
			return;
		}

		StartRecording();
	}
}

void PLSBasic::on_settingsButton_clicked()
{
	on_action_Settings_triggered();
}


void PLSBasic::on_actionShowSettingsFolder_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu File Show Settings Folder", ACTION_CLICK);

	char path[512];
	int ret = GetConfigPath(path, 512, "PRISMLiveStudio");
	if (ret <= 0)
		return;

	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void PLSBasic::on_actionShowProfileFolder_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Profile Show Profile Folder", ACTION_CLICK);

	char path[512];
	int ret = GetProfilePath(path, 512, "");
	if (ret <= 0)
		return;

	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

int PLSBasic::GetTopSelectedSourceItem()
{
	QModelIndexList selectedItems = ui->sources->selectionModel()->selectedIndexes();
	return selectedItems.count() ? selectedItems[0].row() : -1;
}

void PLSBasic::on_preview_customContextMenuRequested(const QPoint &pos)
{
	CreateSourcePopupMenu(GetTopSelectedSourceItem(), true);

	UNUSED_PARAMETER(pos);
}

void PLSBasic::on_program_customContextMenuRequested(const QPoint &)
{
	QMenu popup(this);
	QPointer<QMenu> studioProgramProjector;

	studioProgramProjector = new QMenu(QTStr("StudioProgramProjector"));
	AddProjectorMenuMonitors(studioProgramProjector, this, SLOT(OpenStudioProgramProjector()));

	popup.addMenu(studioProgramProjector);

	QAction *studioProgramWindow = popup.addAction(QTStr("StudioProgramWindow"), this, SLOT(OpenStudioProgramWindow()));

	popup.addAction(studioProgramWindow);

	popup.exec(QCursor::pos());
}

void PLSBasic::PreviewDisabledMenu(const QPoint &pos)
{
	QMenu popup(this);
	delete previewProjectorMain;

	QAction *action = popup.addAction(QTStr("Basic.Main.PreviewConextMenu.Enable"), this, SLOT(TogglePreview()));
	action->setCheckable(true);
	action->setChecked(obs_display_enabled(ui->preview->GetDisplay()));

	previewProjectorMain = new QMenu(QTStr("PreviewProjector"));
	AddProjectorMenuMonitors(previewProjectorMain, this, SLOT(OpenPreviewProjector()));

	QAction *previewWindow = popup.addAction(QTStr("PreviewWindow"), this, SLOT(OpenPreviewWindow()));

	popup.addMenu(previewProjectorMain);
	popup.addAction(previewWindow);
	popup.exec(QCursor::pos());

	UNUSED_PARAMETER(pos);
}

void PLSBasic::on_actionAlwaysOnTop_triggered()
{
#ifndef _WIN32
	/* Make sure all dialogs are safely and successfully closed before
	 * switching the always on top mode due to the fact that windows all
	 * have to be recreated, so queue the actual toggle to happen after
	 * all events related to closing the dialogs have finished */
	CloseDialogs();
#endif
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu View Always On Top", ACTION_CLICK);

	QMetaObject::invokeMethod(this, "ToggleAlwaysOnTop", Qt::QueuedConnection);
}

void PLSBasic::ToggleAlwaysOnTop()
{
	bool isAlwaysOnTop = IsAlwaysOnTop(mainView, MAIN_FRAME);

	ui->actionAlwaysOnTop->setChecked(!isAlwaysOnTop);
	SetAlwaysOnTop(mainView, MAIN_FRAME, !isAlwaysOnTop);

	mainView->show();
}

void PLSBasic::GetFPSCommon(uint32_t &num, uint32_t &den) const
{
	const char *val = config_get_string(basicConfig, "Video", "FPSCommon");

	if (strcmp(val, "10") == 0) {
		num = 10;
		den = 1;
	} else if (strcmp(val, "20") == 0) {
		num = 20;
		den = 1;
	} else if (strcmp(val, "24 NTSC") == 0) {
		num = 24000;
		den = 1001;
	} else if (strcmp(val, "25 PAL") == 0) {
		num = 25;
		den = 1;
	} else if (strcmp(val, "29.97") == 0) {
		num = 30000;
		den = 1001;
	} else if (strcmp(val, "48") == 0) {
		num = 48;
		den = 1;
	} else if (strcmp(val, "50 PAL") == 0) {
		num = 50;
		den = 1;
	} else if (strcmp(val, "59.94") == 0) {
		num = 60000;
		den = 1001;
	} else if (strcmp(val, "60") == 0) {
		num = 60;
		den = 1;
	} else {
		num = 30;
		den = 1;
	}
}

void PLSBasic::GetFPSInteger(uint32_t &num, uint32_t &den) const
{
	num = (uint32_t)config_get_uint(basicConfig, "Video", "FPSInt");
	den = 1;
}

void PLSBasic::GetFPSFraction(uint32_t &num, uint32_t &den) const
{
	num = (uint32_t)config_get_uint(basicConfig, "Video", "FPSNum");
	den = (uint32_t)config_get_uint(basicConfig, "Video", "FPSDen");
}

void PLSBasic::GetFPSNanoseconds(uint32_t &num, uint32_t &den) const
{
	num = 1000000000;
	den = (uint32_t)config_get_uint(basicConfig, "Video", "FPSNS");
}

void PLSBasic::GetConfigFPS(uint32_t &num, uint32_t &den) const
{
	uint32_t type = config_get_uint(basicConfig, "Video", "FPSType");

	if (type == 1) //"Integer"
		GetFPSInteger(num, den);
	else if (type == 2) //"Fraction"
		GetFPSFraction(num, den);
	else if (false) //"Nanoseconds", currently not implemented
		GetFPSNanoseconds(num, den);
	else
		GetFPSCommon(num, den);
}

config_t *PLSBasic::Config() const
{
	return basicConfig;
}

void PLSBasic::on_actionEditTransform_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Edit Transform", ACTION_CLICK);
	if (transformWindow)
		transformWindow->close();

	transformWindow = new PLSBasicTransform(this);
	transformWindow->show();
	transformWindow->setAttribute(Qt::WA_DeleteOnClose, true);
}

static obs_transform_info copiedTransformInfo;
static obs_sceneitem_crop copiedCropInfo;

void PLSBasic::on_actionCopyTransform_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Copy Transform", ACTION_CLICK);
	auto func = [](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
		if (!obs_sceneitem_selected(item))
			return true;

		obs_sceneitem_defer_update_begin(item);
		obs_sceneitem_get_info(item, &copiedTransformInfo);
		obs_sceneitem_get_crop(item, &copiedCropInfo);
		obs_sceneitem_defer_update_end(item);

		UNUSED_PARAMETER(scene);
		UNUSED_PARAMETER(param);
		return true;
	};

	obs_scene_enum_items(GetCurrentScene(), func, nullptr);
	ui->actionPasteTransform->setEnabled(true);
}

void PLSBasic::on_actionPasteTransform_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Paste Transform", ACTION_CLICK);
	auto func = [](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
		if (!obs_sceneitem_selected(item))
			return true;

		obs_sceneitem_defer_update_begin(item);
		obs_sceneitem_set_info(item, &copiedTransformInfo);
		obs_sceneitem_set_crop(item, &copiedCropInfo);
		obs_sceneitem_defer_update_end(item);

		UNUSED_PARAMETER(scene);
		UNUSED_PARAMETER(param);
		return true;
	};

	obs_scene_enum_items(GetCurrentScene(), func, nullptr);
}

static bool reset_tr(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, reset_tr, nullptr);
	if (!obs_sceneitem_selected(item))
		return true;

	obs_sceneitem_defer_update_begin(item);

	obs_transform_info info;
	vec2_set(&info.pos, 0.0f, 0.0f);
	vec2_set(&info.scale, 1.0f, 1.0f);
	info.rot = 0.0f;
	info.alignment = OBS_ALIGN_TOP | OBS_ALIGN_LEFT;
	info.bounds_type = OBS_BOUNDS_NONE;
	info.bounds_alignment = OBS_ALIGN_CENTER;
	vec2_set(&info.bounds, 0.0f, 0.0f);
	obs_sceneitem_set_info(item, &info);

	obs_sceneitem_crop crop = {};
	obs_sceneitem_set_crop(item, &crop);

	obs_sceneitem_defer_update_end(item);

	UNUSED_PARAMETER(scene);
	UNUSED_PARAMETER(param);
	return true;
}

void PLSBasic::on_actionResetTransform_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Reset Transform", ACTION_CLICK);
	obs_scene_enum_items(GetCurrentScene(), reset_tr, nullptr);
}

static void GetItemBox(obs_sceneitem_t *item, vec3 &tl, vec3 &br)
{
	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

	vec3_set(&tl, M_INFINITE, M_INFINITE, 0.0f);
	vec3_set(&br, -M_INFINITE, -M_INFINITE, 0.0f);

	auto GetMinPos = [&](float x, float y) {
		vec3 pos;
		vec3_set(&pos, x, y, 0.0f);
		vec3_transform(&pos, &pos, &boxTransform);
		vec3_min(&tl, &tl, &pos);
		vec3_max(&br, &br, &pos);
	};

	GetMinPos(0.0f, 0.0f);
	GetMinPos(1.0f, 0.0f);
	GetMinPos(0.0f, 1.0f);
	GetMinPos(1.0f, 1.0f);
}

static vec3 GetItemTL(obs_sceneitem_t *item)
{
	vec3 tl, br;
	GetItemBox(item, tl, br);
	return tl;
}

static void SetItemTL(obs_sceneitem_t *item, const vec3 &tl)
{
	vec3 newTL;
	vec2 pos;

	obs_sceneitem_get_pos(item, &pos);
	newTL = GetItemTL(item);
	pos.x += tl.x - newTL.x;
	pos.y += tl.y - newTL.y;
	obs_sceneitem_set_pos(item, &pos);
}

static bool RotateSelectedSources(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, RotateSelectedSources, param);
	if (!obs_sceneitem_selected(item))
		return true;

	float rot = *reinterpret_cast<float *>(param);

	vec3 tl = GetItemTL(item);

	rot += obs_sceneitem_get_rot(item);
	if (rot >= 360.0f)
		rot -= 360.0f;
	else if (rot <= -360.0f)
		rot += 360.0f;
	obs_sceneitem_set_rot(item, rot);

	obs_sceneitem_force_update_transform(item);

	SetItemTL(item, tl);

	UNUSED_PARAMETER(scene);
	return true;
};

void PLSBasic::on_actionRotate90CW_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Rotate90CW", ACTION_CLICK);
	float f90CW = 90.0f;
	obs_scene_enum_items(GetCurrentScene(), RotateSelectedSources, &f90CW);
}

void PLSBasic::on_actionRotate90CCW_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Rotate90CCW", ACTION_CLICK);
	float f90CCW = -90.0f;
	obs_scene_enum_items(GetCurrentScene(), RotateSelectedSources, &f90CCW);
}

void PLSBasic::on_actionRotate180_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Rotate180", ACTION_CLICK);
	float f180 = 180.0f;
	obs_scene_enum_items(GetCurrentScene(), RotateSelectedSources, &f180);
}

static bool MultiplySelectedItemScale(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	vec2 &mul = *reinterpret_cast<vec2 *>(param);

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, MultiplySelectedItemScale, param);
	if (!obs_sceneitem_selected(item))
		return true;

	vec3 tl = GetItemTL(item);

	vec2 scale;
	obs_sceneitem_get_scale(item, &scale);
	vec2_mul(&scale, &scale, &mul);
	obs_sceneitem_set_scale(item, &scale);

	obs_sceneitem_force_update_transform(item);

	SetItemTL(item, tl);

	UNUSED_PARAMETER(scene);
	return true;
}

void PLSBasic::on_actionFlipHorizontal_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Flip Horizontal", ACTION_CLICK);

	vec2 scale;
	vec2_set(&scale, -1.0f, 1.0f);
	obs_scene_enum_items(GetCurrentScene(), MultiplySelectedItemScale, &scale);
}

void PLSBasic::on_actionFlipVertical_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Flip Vertical", ACTION_CLICK);

	vec2 scale;
	vec2_set(&scale, 1.0f, -1.0f);
	obs_scene_enum_items(GetCurrentScene(), MultiplySelectedItemScale, &scale);
}

static bool CenterAlignSelectedItems(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	obs_bounds_type boundsType = *reinterpret_cast<obs_bounds_type *>(param);

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, CenterAlignSelectedItems, param);
	if (!obs_sceneitem_selected(item))
		return true;

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	obs_transform_info itemInfo;
	vec2_set(&itemInfo.pos, 0.0f, 0.0f);
	vec2_set(&itemInfo.scale, 1.0f, 1.0f);
	itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	itemInfo.rot = 0.0f;

	vec2_set(&itemInfo.bounds, float(ovi.base_width), float(ovi.base_height));
	itemInfo.bounds_type = boundsType;
	itemInfo.bounds_alignment = OBS_ALIGN_CENTER;

	obs_sceneitem_set_info(item, &itemInfo);

	UNUSED_PARAMETER(scene);
	return true;
}

void PLSBasic::on_actionFitToScreen_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Fit To Sceen", ACTION_CLICK);

	obs_bounds_type boundsType = OBS_BOUNDS_SCALE_INNER;
	obs_scene_enum_items(GetCurrentScene(), CenterAlignSelectedItems, &boundsType);
}

void PLSBasic::on_actionStretchToScreen_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Stretch To Sceen", ACTION_CLICK);

	obs_bounds_type boundsType = OBS_BOUNDS_STRETCH;
	obs_scene_enum_items(GetCurrentScene(), CenterAlignSelectedItems, &boundsType);
}

enum class CenterType {
	Scene,
	Vertical,
	Horizontal,
};

static bool center_to_scene(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	CenterType centerType = *reinterpret_cast<CenterType *>(param);

	vec3 tl, br, itemCenter, screenCenter, offset;
	obs_video_info ovi;
	obs_transform_info oti;

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, center_to_scene, &centerType);
	if (!obs_sceneitem_selected(item))
		return true;

	obs_get_video_info(&ovi);
	obs_sceneitem_get_info(item, &oti);

	if (centerType == CenterType::Scene)
		vec3_set(&screenCenter, float(ovi.base_width), float(ovi.base_height), 0.0f);
	else if (centerType == CenterType::Vertical)
		vec3_set(&screenCenter, float(oti.bounds.x), float(ovi.base_height), 0.0f);
	else if (centerType == CenterType::Horizontal)
		vec3_set(&screenCenter, float(ovi.base_width), float(oti.bounds.y), 0.0f);

	vec3_mulf(&screenCenter, &screenCenter, 0.5f);

	GetItemBox(item, tl, br);

	vec3_sub(&itemCenter, &br, &tl);
	vec3_mulf(&itemCenter, &itemCenter, 0.5f);
	vec3_add(&itemCenter, &itemCenter, &tl);

	vec3_sub(&offset, &screenCenter, &itemCenter);
	vec3_add(&tl, &tl, &offset);

	if (centerType == CenterType::Vertical)
		tl.x = oti.pos.x;
	else if (centerType == CenterType::Horizontal)
		tl.y = oti.pos.y;

	SetItemTL(item, tl);
	return true;
};

void PLSBasic::on_actionCenterToScreen_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Center To Sceen", ACTION_CLICK);

	CenterType centerType = CenterType::Scene;
	obs_scene_enum_items(GetCurrentScene(), center_to_scene, &centerType);
}

void PLSBasic::on_actionVerticalCenter_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Vertical To Sceen", ACTION_CLICK);

	CenterType centerType = CenterType::Vertical;
	obs_scene_enum_items(GetCurrentScene(), center_to_scene, &centerType);
}

void PLSBasic::on_actionHorizontalCenter_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Horizontal Center", ACTION_CLICK);

	CenterType centerType = CenterType::Horizontal;
	obs_scene_enum_items(GetCurrentScene(), center_to_scene, &centerType);
}

void PLSBasic::EnablePreviewDisplay(bool enable)
{
	obs_display_set_enabled(ui->preview->GetDisplay(), enable);

	// NOTE : To avoid refresh problem, here we should hide current page firstly.
	if (enable) {
		ui->previewDisabledWidget->setVisible(false);
		ui->previewContainer->setVisible(true);
	} else {
		ui->previewContainer->setVisible(false);
		ui->previewDisabledWidget->setVisible(true);
	}
}

void PLSBasic::TogglePreview()
{
	previewEnabled = !previewEnabled;
	if (previewEnabled) {
		PLS_UI_STEP(MAINFRAME_MODULE, "Enable preview", ACTION_CLICK);
	} else {
		PLS_UI_STEP(MAINFRAME_MODULE, "Disable preview", ACTION_CLICK);
	}

	EnablePreviewDisplay(previewEnabled);
}

void PLSBasic::EnablePreview()
{
	if (previewProgramMode)
		return;

	previewEnabled = true;
	EnablePreviewDisplay(true);
}

void PLSBasic::DisablePreview()
{
	if (previewProgramMode)
		return;

	previewEnabled = false;
	EnablePreviewDisplay(false);
}

static bool nudge_callback(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_locked(item))
		return true;

	struct vec2 &offset = *reinterpret_cast<struct vec2 *>(param);
	struct vec2 pos;

	if (!obs_sceneitem_selected(item)) {
		if (obs_sceneitem_is_group(item)) {
			struct vec3 offset3;
			vec3_set(&offset3, offset.x, offset.y, 0.0f);

			struct matrix4 matrix;
			obs_sceneitem_get_draw_transform(item, &matrix);
			vec4_set(&matrix.t, 0.0f, 0.0f, 0.0f, 1.0f);
			matrix4_inv(&matrix, &matrix);
			vec3_transform(&offset3, &offset3, &matrix);

			struct vec2 new_offset;
			vec2_set(&new_offset, offset3.x, offset3.y);
			obs_sceneitem_group_enum_items(item, nudge_callback, &new_offset);
		}

		return true;
	}

	obs_sceneitem_get_pos(item, &pos);
	vec2_add(&pos, &pos, &offset);
	obs_sceneitem_set_pos(item, &pos);
	return true;
}

void PLSBasic::Nudge(int dist, MoveDir dir)
{
	if (ui->preview->Locked())
		return;

	struct vec2 offset;
	vec2_set(&offset, 0.0f, 0.0f);

	switch (dir) {
	case MoveDir::Up:
		offset.y = (float)-dist;
		break;
	case MoveDir::Down:
		offset.y = (float)dist;
		break;
	case MoveDir::Left:
		offset.x = (float)-dist;
		break;
	case MoveDir::Right:
		offset.x = (float)dist;
		break;
	}

	obs_scene_enum_items(GetCurrentScene(), nudge_callback, &offset);
}

void PLSBasic::NudgeUp()
{
	Nudge(1, MoveDir::Up);
}
void PLSBasic::NudgeDown()
{
	Nudge(1, MoveDir::Down);
}
void PLSBasic::NudgeLeft()
{
	Nudge(1, MoveDir::Left);
}
void PLSBasic::NudgeRight()
{
	Nudge(1, MoveDir::Right);
}

PLSDialogView *PLSBasic::OpenProjector(obs_source_t *source, int monitor, QString title, ProjectorType type)
{
	/* seriously?  10 monitors? */
	if (monitor > 9 || monitor > QGuiApplication::screens().size() - 1)
		return nullptr;

	PLSDialogView *dialogView = new PLSDialogView(nullptr);
	dialogView->setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);

	PLSProjector *projector = new PLSProjector(nullptr, source, monitor, title, type);
	dialogView->setWidget(projector);
	dialogView->content()->setContentsMargins(4, 0, 4, 4);
	dialogView->content()->setStyleSheet("background-color: #000000");

	QWidget *toplevelView = pls_get_toplevel_view(projector);
	connect(toplevelView, SIGNAL(beginResizeSignal()), projector, SLOT(beginResizeSlot()));
	connect(toplevelView, SIGNAL(endResizeSignal()), projector, SLOT(endResizeSlot()));

	if (monitor < 0) {
		dialogView->setWindowIcon(QIcon::fromTheme("obs", QIcon(":/res/images/PRISMLiveStudio.ico")));
		dialogView->setHasMinButton(true);
		dialogView->setHasMaxResButton(true);
		dialogView->resize(480, 320);
	} else {
		dialogView->setEscapeCloseEnabled(true);
		dialogView->setHasCaption(false);
		dialogView->setHasBorder(false);

		QScreen *screen = QGuiApplication::screens()[monitor];
		dialogView->setGeometry(screen->geometry());
	}

	connect(dialogView, &PLSDialogView::shown, dialogView, [=]() { SetAlwaysOnTop(dialogView, "Projector", config_get_bool(GetGlobalConfig(), "BasicWindow", "ProjectorAlwaysOnTop")); });

	dialogView->setAttribute(Qt::WA_DeleteOnClose, true);
	dialogView->setAttribute(Qt::WA_QuitOnClose, false);

	dialogView->installEventFilter(CreateShortcutFilter());

	dialogView->show();

	connect(dialogView, &PLSDialogView::destroyed, this, [dialogView, monitor, this]() {
		if (monitor < 0) {
			for (auto &proj : windowProjectors) {
				if (proj.first == dialogView) {
					proj.first.clear();
					proj.second = nullptr;
				}
			}
		} else {
			auto &proj = projectors[monitor];
			if (proj.first == dialogView) {
				proj.first.clear();
				proj.second = nullptr;
			}
		}
	});

	if (monitor < 0) {
		bool inserted = false;
		for (auto &proj : windowProjectors) {
			if (!proj.first) {
				proj.first = dialogView;
				proj.second = projector;
				inserted = true;
			}
		}

		if (!inserted) {
			windowProjectors.push_back(QPair<QPointer<PLSDialogView>, PLSProjector *>(dialogView, projector));
		}
	} else {
		auto &proj = projectors[monitor];
		delete proj.first;
		proj.first.clear();
		proj.second = nullptr;

		proj = QPair<QPointer<PLSDialogView>, PLSProjector *>(dialogView, projector);
	}

	return dialogView;
}

void PLSBasic::OpenStudioProgramProjector()
{
	int monitor = sender()->property("monitor").toInt();
	OpenProjector(nullptr, monitor, nullptr, ProjectorType::StudioProgram);
	PLS_UI_STEP(MAINMENU_MODULE, "Studio Program Projector", ACTION_CLICK);
}

void PLSBasic::OpenPreviewProjector()
{
	int monitor = sender()->property("monitor").toInt();
	OpenProjector(nullptr, monitor, nullptr, ProjectorType::Preview);
	PLS_UI_STEP(MAINMENU_MODULE, "Fullscreen Projector (Preview)", ACTION_CLICK);
}

void PLSBasic::OpenSourceProjector()
{
	int monitor = sender()->property("monitor").toInt();
	OBSSceneItem item = GetCurrentSceneItemData();
	if (!item)
		return;

	PLS_UI_STEP(MAINMENU_MODULE, "Fullscreen Projector (Source)", ACTION_CLICK);
	OpenProjector(obs_sceneitem_get_source(item), monitor, nullptr, ProjectorType::Source);
}

void PLSBasic::OpenMultiviewProjector()
{
	int monitor = sender()->property("monitor").toInt();
	OpenProjector(nullptr, monitor, nullptr, ProjectorType::Multiview);
	PLS_UI_STEP(MAINMENU_MODULE, "Multiview Projector", ACTION_CLICK);
}

void PLSBasic::OpenSceneProjector()
{
	int monitor = sender()->property("monitor").toInt();
	OBSScene scene = GetCurrentScene();
	if (!scene)
		return;

	PLS_UI_STEP(MAINMENU_MODULE, "Fullscreen Projector(Scene)", ACTION_CLICK);
	OpenProjector(obs_scene_get_source(scene), monitor, nullptr, ProjectorType::Scene);
}

void PLSBasic::OpenStudioProgramWindow()
{
	OpenProjector(nullptr, -1, QTStr("StudioProgramWindow"), ProjectorType::StudioProgram);
	PLS_UI_STEP(MAINFRAME_MODULE, QT_TO_UTF8(QTStr("StudioProgramWindow")), ACTION_CLICK);
}

void PLSBasic::OpenPreviewWindow()
{
	OpenProjector(nullptr, -1, QTStr("PreviewWindow"), ProjectorType::Preview);
	PLS_UI_STEP(MAINFRAME_MODULE, QT_TO_UTF8(QTStr("PreviewWindow")), ACTION_CLICK);
}

void PLSBasic::OpenSourceWindow()
{
	OBSSceneItem item = GetCurrentSceneItemData();
	if (!item)
		return;

	OBSSource source = obs_sceneitem_get_source(item);
	QString title = QString::fromUtf8(obs_source_get_name(source));

	OpenProjector(obs_sceneitem_get_source(item), -1, title, ProjectorType::Source);
	PLS_UI_STEP(MAINFRAME_MODULE, QT_TO_UTF8(QTStr("SourceWindow")), ACTION_CLICK);
}

void PLSBasic::OpenMultiviewWindow()
{
	OpenProjector(nullptr, -1, QTStr("MultiviewWindowed"), ProjectorType::Multiview);
	PLS_UI_STEP(MAINFRAME_MODULE, QT_TO_UTF8(QTStr("MultiviewWindowed")), ACTION_CLICK);
}

void PLSBasic::OpenSceneWindow()
{
	OBSScene scene = GetCurrentScene();
	if (!scene)
		return;

	OBSSource source = obs_scene_get_source(scene);
	QString title = QString::fromUtf8(obs_source_get_name(source));

	OpenProjector(obs_scene_get_source(scene), -1, title, ProjectorType::Scene);
	PLS_UI_STEP(MAINFRAME_MODULE, QT_TO_UTF8(QTStr("SceneWindow")), ACTION_CLICK);
}

void PLSBasic::OpenSavedProjectors()
{
	for (SavedProjectorInfo *info : savedProjectorsArray) {
		PLSDialogView *dialogView = nullptr;
		switch (info->type) {
		case ProjectorType::Source:
		case ProjectorType::Scene: {
			OBSSource source = obs_get_source_by_name(info->name.c_str());
			if (!source)
				continue;

			QString title = nullptr;
			if (info->monitor < 0)
				title = QString::fromUtf8(obs_source_get_name(source));

			dialogView = OpenProjector(source, info->monitor, title, info->type);

			obs_source_release(source);
			break;
		}
		case ProjectorType::Preview: {
			dialogView = OpenProjector(nullptr, info->monitor, QTStr("PreviewWindow"), ProjectorType::Preview);
			break;
		}
		case ProjectorType::StudioProgram: {
			dialogView = OpenProjector(nullptr, info->monitor, QTStr("StudioProgramWindow"), ProjectorType::StudioProgram);
			break;
		}
		case ProjectorType::Multiview: {
			dialogView = OpenProjector(nullptr, info->monitor, QTStr("MultiviewWindowed"), ProjectorType::Multiview);
			break;
		}
		}

		if (dialogView && !info->geometry.empty() && info->monitor < 0) {
			QByteArray byteArray = QByteArray::fromBase64(QByteArray(info->geometry.c_str()));
			dialogView->restoreGeometry(byteArray);

			if (!WindowPositionValid(dialogView->normalGeometry())) {
				QRect rect = App()->desktop()->geometry();
				dialogView->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), rect));
			}
		}
	}
}

void PLSBasic::on_actionFullscreenInterface_triggered()
{
	if (!mainView->getFullScreenState()) {
		mainView->showFullScreen();
		PLS_UI_STEP(MAINFRAME_MODULE, "Fullscreen", ACTION_CLICK);
	} else {
		mainView->showNormal();
		PLS_UI_STEP(MAINFRAME_MODULE, "Fullscreen Normal", ACTION_CLICK);
	}
}

void PLSBasic::UpdateTitleBar()
{
	stringstream name;

	const char *profile = config_get_string(App()->GlobalConfig(), "Basic", "Profile");
	const char *sceneCollection = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");

	name << "PRISMLiveStudio ";
	// if (previewProgramMode)
	// 	name << "Studio ";

	name << App()->GetVersionString();
	if (App()->IsPortableMode())
		name << " - Portable Mode";

	name << " - " << Str("TitleBar.Profile") << ": " << profile;
	name << " - " << Str("TitleBar.Scenes") << ": " << sceneCollection;

	setWindowTitle(QT_UTF8(name.str().c_str()));
}

int PLSBasic::GetProfilePath(char *path, size_t size, const char *file) const
{
	char profiles_path[512];
	const char *profile = config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");
	int ret;

	if (!profile)
		return -1;
	if (!path)
		return -1;
	if (!file)
		file = "";

	ret = GetConfigPath(profiles_path, 512, "PRISMLiveStudio/basic/profiles");
	if (ret <= 0)
		return ret;

	if (!*file)
		return snprintf(path, size, "%s/%s", profiles_path, profile);

	return snprintf(path, size, "%s/%s/%s", profiles_path, profile, file);
}

PLSMainView *PLSBasic::getMainView() const
{
	return mainView;
}

void PLSBasic::on_resetUI_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Reset UI", ACTION_CLICK);

	/* prune deleted extra docks */
	for (int i = extraDocks.size() - 1; i >= 0; i--) {
		if (!extraDocks[i]) {
			extraDocks.removeAt(i);
		}
	}

	if (extraDocks.size()) {
		if (PLSMessageBox::question(this, QTStr("ResetUIWarning.Title"), QTStr("ResetUIWarning.Text")) != PLSAlertView::Button::Yes) {
			return;
		}
	}

	/* undock/hide/center extra docks */
	for (int i = extraDocks.size() - 1; i >= 0; i--) {
		if (extraDocks[i]) {
			extraDocks[i]->setVisible(true);
			extraDocks[i]->setFloating(true);
			extraDocks[i]->move(frameGeometry().topLeft() + rect().center() - extraDocks[i]->rect().center());
			extraDocks[i]->setVisible(false);
		}
	}

	restoreState(startingDockLayout);

	QList<QDockWidget *> docks{ui->scenesDock, ui->sourcesDock, ui->mixerDock};

	ui->scenesDock->setVisible(true);
	ui->sourcesDock->setVisible(true);
	ui->mixerDock->setVisible(true);
	//ui->controlsDock->setVisible(false);

	// resize dock with UI design
	resizeDocks(docks, {218, 218, 218}, Qt::Vertical);
	resizeDocks(docks, {410, 410, 410}, Qt::Horizontal);
}

void PLSBasic::on_lockUI_toggled(bool lock)
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Lock UI", ACTION_CLICK);

	QDockWidget::DockWidgetFeatures features = lock ? QDockWidget::NoDockWidgetFeatures : QDockWidget::AllDockWidgetFeatures;

	QDockWidget::DockWidgetFeatures mainFeatures = features;
	mainFeatures &= ~QDockWidget::QDockWidget::DockWidgetClosable;

	// limit dock to bottom area
	ui->scenesDock->setFeatures(mainFeatures);
	ui->scenesDock->setAllowedAreas(Qt::BottomDockWidgetArea);
	ui->sourcesDock->setFeatures(mainFeatures);
	ui->sourcesDock->setAllowedAreas(Qt::BottomDockWidgetArea);
	ui->mixerDock->setFeatures(mainFeatures);
	ui->mixerDock->setAllowedAreas(Qt::BottomDockWidgetArea);
	//ui->controlsDock->setFeatures(mainFeatures);

	for (int i = extraDocks.size() - 1; i >= 0; i--) {
		if (!extraDocks[i]) {
			extraDocks.removeAt(i);
		} else {
			extraDocks[i]->setFeatures(features);
		}
	}
}

void PLSBasic::on_toggleListboxToolbars_toggled(bool visible)
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Toggle Listbox Toolbars", ACTION_CLICK);

	config_set_bool(App()->GlobalConfig(), "BasicWindow", "ShowListboxToolbars", visible);
}

void PLSBasic::on_toggleStatusBar_toggled(bool visible)
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Toggle Status Bar", ACTION_CLICK);

	mainView->statusBar()->setVisible(visible);

	config_set_bool(App()->GlobalConfig(), "BasicWindow", "ShowStatusBar", visible);
}

void PLSBasic::on_actionLockPreview_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Lock Preview", ACTION_CLICK);

	ui->preview->ToggleLocked();
	ui->actionLockPreview->setChecked(ui->preview->Locked());
}

void PLSBasic::on_scalingMenu_aboutToShow()
{
	obs_video_info ovi;
	obs_get_video_info(&ovi);

	QAction *action = ui->actionScaleCanvas;
	QString text = QTStr("Basic.MainMenu.Edit.Scale.Canvas");
	text = text.arg(QString::number(ovi.base_width), QString::number(ovi.base_height));
	action->setText(text);

	action = ui->actionScaleOutput;
	text = QTStr("Basic.MainMenu.Edit.Scale.Output");
	text = text.arg(QString::number(ovi.output_width), QString::number(ovi.output_height));
	action->setText(text);
	action->setVisible(!(ovi.output_width == ovi.base_width && ovi.output_height == ovi.base_height));

	UpdatePreviewScalingMenu();
}

void PLSBasic::on_actionScaleWindow_triggered()
{
	ui->preview->SetFixedScaling(false);
	ui->preview->ResetScrollingOffset();
	emit ui->preview->DisplayResized();
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Scale To Window", ACTION_CLICK);
}

void PLSBasic::on_actionScaleCanvas_triggered()
{
	ui->preview->SetFixedScaling(true);
	ui->preview->SetScalingLevel(0);
	emit ui->preview->DisplayResized();
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Scale To Canvas", ACTION_CLICK);
}

void PLSBasic::on_actionScaleOutput_triggered()
{
	obs_video_info ovi;
	obs_get_video_info(&ovi);

	ui->preview->SetFixedScaling(true);
	float scalingAmount = float(ovi.output_width) / float(ovi.base_width);
	// log base ZOOM_SENSITIVITY of x = log(x) / log(ZOOM_SENSITIVITY)
	int32_t approxScalingLevel = int32_t(round(log(scalingAmount) / log(ZOOM_SENSITIVITY)));
	ui->preview->SetScalingLevel(approxScalingLevel);
	ui->preview->SetScalingAmount(scalingAmount);
	emit ui->preview->DisplayResized();
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Scale To Output", ACTION_CLICK);
}

void PLSBasic::on_actionStudioMode_triggered()
{
	if (sender() == ui->actionStudioMode) {
		PLS_UI_STEP(MAINMENU_MODULE, "Main Menu View Studio Mode", ACTION_CLICK);
	}

	bool isStudioMode = IsPreviewProgramMode();
	if (isStudioMode) {
		PLS_UI_STEP(MAINFRAME_MODULE, "Exit studio mode", ACTION_CLICK);
	} else {
		PLS_UI_STEP(MAINFRAME_MODULE, "Enter studio mode", ACTION_CLICK);
	}
	SetPreviewProgramMode(!isStudioMode);
}

void PLSBasic::SetShowing(bool showing)
{
	if (!showing && mainView->isVisible()) {
		PLS_INFO(MAINFRAME_MODULE, "hide main window");

		if (!mainView->getMaxState() && !mainView->getFullScreenState()) {
			config_set_string(App()->GlobalConfig(), "BasicWindow", "geometry", saveGeometry().toBase64().constData());
		}

		/* hide all visible child dialogs */
		visDlgPositions.clear();
		if (!visDialogs.isEmpty()) {
			for (QDialog *dlg : visDialogs) {
				visDlgPositions.append(dlg->pos());
				dlg->hide();
			}
		}

		if (showHide)
			showHide->setText(QTStr("Basic.SystemTray.Show"));
		QTimer::singleShot(250, mainView, SLOT(hide()));

		if (previewEnabled)
			EnablePreviewDisplay(false);
		if (ui->scenesDock->isFloating())
			ui->scenesDock->hide();
		if (ui->sourcesDock->isFloating())
			ui->sourcesDock->hide();
		if (ui->mixerDock->isFloating())
			ui->mixerDock->hide();

		mainView->setVisible(false);

#ifdef __APPLE__
		EnableOSXDockIcon(false);
#endif

	} else if (showing && !mainView->isVisible()) {
		PLS_INFO(MAINFRAME_MODULE, "show and activate main window");

		if (showHide)
			showHide->setText(QTStr("Basic.SystemTray.Hide"));
		QTimer::singleShot(250, mainView, SLOT(show()));

		if (previewEnabled)
			EnablePreviewDisplay(true);
		if (ui->scenesDock->isFloating())
			ui->scenesDock->show();
		if (ui->sourcesDock->isFloating())
			ui->sourcesDock->show();
		if (ui->mixerDock->isFloating())
			ui->mixerDock->show();

		mainView->setVisible(true);

#ifdef __APPLE__
		EnableOSXDockIcon(true);
#endif

		/* raise and activate window to ensure it is on top */
		mainView->raise();
		mainView->activateWindow();

		/* show all child dialogs that was visible earlier */
		if (!visDialogs.isEmpty()) {
			for (int i = 0; i < visDialogs.size(); ++i) {
				QDialog *dlg = visDialogs[i];
				if (visDlgPositions.size() > i)
					dlg->move(visDlgPositions[i]);
				dlg->show();
			}
		}

		/* Unminimize window if it was hidden to tray instead of task
		 * bar. */
		if (sysTrayMinimizeToTray() || mainView->isMinimized()) {
			Qt::WindowStates state;
			state = mainView->windowState() & ~Qt::WindowMinimized;
			state |= Qt::WindowActive;
			mainView->setWindowState(state);
		}
	} else if (showing) {
		PLS_INFO(MAINFRAME_MODULE, "activate main window");

		mainView->raise();
		mainView->activateWindow();

		if (mainView->isMinimized()) {
			Qt::WindowStates state;
			state = mainView->windowState() & ~Qt::WindowMinimized;
			state |= Qt::WindowActive;
			mainView->setWindowState(state);
		}
	}
}

void PLSBasic::ToggleShowHide()
{
	bool showing = mainView->isVisible();
	if (showing) {
		/* check for modal dialogs */
		EnumDialogs();
		if (!modalDialogs.isEmpty() || !visMsgBoxes.isEmpty())
			return;
	}
	SetShowing(!showing);
}

void PLSBasic::SystemTrayInit()
{
	trayIcon.reset(new QSystemTrayIcon(QIcon::fromTheme("obs-tray", QIcon(":/res/images/PRISMLiveStudio.ico")), this));
	trayIcon->setToolTip("PRISMLiveStudio");

	showHide = new QAction(QTStr("Basic.SystemTray.Show"), trayIcon.data());
	sysTrayStream = new QAction(QTStr("Basic.Main.StartStreaming"), trayIcon.data());
	sysTrayRecord = new QAction(QTStr("Basic.Main.StartRecording"), trayIcon.data());

	exit = new QAction(QTStr("Exit"), trayIcon.data());

	trayMenu = new QMenu();
	previewProjector = new QMenu(QTStr("PreviewProjector"));
	studioProgramProjector = new QMenu(QTStr("StudioProgramProjector"));
	AddProjectorMenuMonitors(previewProjector, this, SLOT(OpenPreviewProjector()));
	AddProjectorMenuMonitors(studioProgramProjector, this, SLOT(OpenStudioProgramProjector()));
	trayMenu->addAction(showHide);
	trayMenu->addMenu(previewProjector);
	trayMenu->addMenu(studioProgramProjector);
	trayMenu->addAction(sysTrayStream);
	trayMenu->addAction(sysTrayRecord);

	trayMenu->addAction(exit);
	trayIcon->setContextMenu(trayMenu);
	trayIcon->show();

	connect(trayIcon.data(), SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(IconActivated(QSystemTrayIcon::ActivationReason)));
	connect(showHide, SIGNAL(triggered()), this, SLOT(ToggleShowHide()));
	connect(sysTrayStream, &QAction::triggered, PLSCHANNELS_API, &PLSChannelDataAPI::toStartBroadcast);
	connect(sysTrayRecord, &QAction::triggered, PLSCHANNELS_API, &PLSChannelDataAPI::toStartRecord);

	connect(exit, &QAction::triggered, this, [this]() {
		trayIcon->hide();
		mainView->close();
	});
	connect(qApp, &QCoreApplication::aboutToQuit, trayIcon.data(), &QSystemTrayIcon::hide);
}

void PLSBasic::IconActivated(QSystemTrayIcon::ActivationReason reason)
{
	// Refresh projector list
	previewProjector->clear();
	studioProgramProjector->clear();
	AddProjectorMenuMonitors(previewProjector, this, SLOT(OpenPreviewProjector()));
	AddProjectorMenuMonitors(studioProgramProjector, this, SLOT(OpenStudioProgramProjector()));

	if (reason == QSystemTrayIcon::Trigger)
		ToggleShowHide();
}

void PLSBasic::SysTrayNotify(const QString &text, QSystemTrayIcon::MessageIcon n)
{
	if (trayIcon && QSystemTrayIcon::supportsMessages()) {
		QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::MessageIcon(n);
		trayIcon->showMessage("PRISM Live Studio", text, icon, 10000);
	}
}

void PLSBasic::SystemTray(bool firstStarted)
{
	if (!QSystemTrayIcon::isSystemTrayAvailable())
		return;
	if (!trayIcon && !firstStarted)
		return;

	bool sysTrayWhenStarted = config_get_bool(GetGlobalConfig(), "BasicWindow", "SysTrayWhenStarted");
	bool sysTrayEnabled = config_get_bool(GetGlobalConfig(), "BasicWindow", "SysTrayEnabled");

	if (firstStarted)
		SystemTrayInit();

	if (!sysTrayWhenStarted && !sysTrayEnabled) {
		trayIcon->hide();
	} else if ((sysTrayWhenStarted && sysTrayEnabled) || opt_minimize_tray) {
		trayIcon->show();
		if (firstStarted) {
			QTimer::singleShot(50, this, SLOT(mainView->hide()));
			EnablePreviewDisplay(false);
			mainView->setVisible(false);
#ifdef __APPLE__
			EnableOSXDockIcon(false);
#endif
			opt_minimize_tray = false;
		}
	} else if (sysTrayEnabled) {
		trayIcon->show();
	} else if (!sysTrayEnabled) {
		trayIcon->hide();
	} else if (!sysTrayWhenStarted && sysTrayEnabled) {
		trayIcon->hide();
	}

	if (mainView->isVisible())
		showHide->setText(QTStr("Basic.SystemTray.Hide"));
	else
		showHide->setText(QTStr("Basic.SystemTray.Show"));
}

bool PLSBasic::sysTrayMinimizeToTray()
{
	return config_get_bool(GetGlobalConfig(), "BasicWindow", "SysTrayMinimizeToTray");
}

void PLSBasic::on_actionCopySource_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Edit Copy Source", ACTION_CLICK);

	OBSSceneItem item = GetCurrentSceneItemData();
	if (!item)
		return;

	on_actionCopyTransform_triggered();

	OBSSource source = obs_sceneitem_get_source(item);

	copyString = obs_source_get_name(source);
	copyVisible = obs_sceneitem_visible(item);

	ui->actionPasteRef->setEnabled(true);

	uint32_t output_flags = obs_source_get_output_flags(source);
	if ((output_flags & OBS_SOURCE_DO_NOT_DUPLICATE) == 0)
		ui->actionPasteDup->setEnabled(true);
	else
		ui->actionPasteDup->setEnabled(false);
}

void PLSBasic::on_actionPasteRef_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Edit Paste Reference", ACTION_CLICK);

	/* do not allow duplicate refs of the same group in the same scene */
	OBSScene scene = GetCurrentScene();
	if (!!obs_scene_get_group(scene, copyString))
		return;

	PLSBasicSourceSelect::SourcePaste(copyString, copyVisible, false);
	on_actionPasteTransform_triggered();
}

void PLSBasic::on_actionPasteDup_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Edit Paste Duplicate", ACTION_CLICK);

	PLSBasicSourceSelect::SourcePaste(copyString, copyVisible, true);
	on_actionPasteTransform_triggered();
}

void PLSBasic::AudioMixerCopyFilters()
{
	PLS_UI_STEP(AUDIO_MIXER, "AudioMixerCopyFilters", ACTION_CLICK);

	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	obs_source_t *source = vol->GetSource();

	copyFiltersString = obs_source_get_name(source);
}

void PLSBasic::AudioMixerPasteFilters()
{
	PLS_UI_STEP(AUDIO_MIXER, "AudioMixerPasteFilters", ACTION_CLICK);

	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	obs_source_t *dstSource = vol->GetSource();

	OBSSource source = obs_get_source_by_name(copyFiltersString);
	obs_source_release(source);

	if (source == dstSource)
		return;

	obs_source_copy_filters(dstSource, source);
}

void PLSBasic::SceneCopyFilters()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Scene Copy Filters", ACTION_CLICK);

	copyFiltersString = obs_source_get_name(GetCurrentSceneSource());
}

void PLSBasic::ScenePasteFilters()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Scene Paste Filters", ACTION_CLICK);

	OBSSource source = obs_get_source_by_name(copyFiltersString);
	obs_source_release(source);

	OBSSource dstSource = GetCurrentSceneSource();

	if (source == dstSource)
		return;

	obs_source_copy_filters(dstSource, source);
}

void PLSBasic::on_actionCopyFilters_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Edit Copy Filters", ACTION_CLICK);

	OBSSceneItem item = GetCurrentSceneItemData();

	if (!item)
		return;

	OBSSource source = obs_sceneitem_get_source(item);

	copyFiltersString = obs_source_get_name(source);

	ui->actionPasteFilters->setEnabled(true);
}

void PLSBasic::on_actionPasteFilters_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Edit Paste Filters", ACTION_CLICK);

	OBSSource source = obs_get_source_by_name(copyFiltersString);
	obs_source_release(source);

	OBSSceneItem sceneItem = GetCurrentSceneItemData();
	OBSSource dstSource = obs_sceneitem_get_source(sceneItem);

	if (source == dstSource)
		return;

	obs_source_copy_filters(dstSource, source);
}

void PLSBasic::OnSourceDockSeperatedBtnClicked()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Source Dock Attach", ACTION_CLICK);

	bool isFloating = ui->sourcesDock->isFloating();
	SetAttachWindowBtnText(actionSeperateSource, !isFloating);

	if (isFloating) {
		ui->sourcesDock->setFloating(!isFloating);
		return;
	}

	SetDocksMovePolicy(ui->sourcesDock);
}

void PLSBasic::OnSceneDockSeperatedBtnClicked()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Scene Dock Attach", ACTION_CLICK);

	bool isFloating = ui->scenesDock->isFloating();
	SetAttachWindowBtnText(actionSeperateScene, !isFloating);
	if (isFloating) {
		ui->scenesDock->setFloating(!isFloating);
		return;
	}

	SetDocksMovePolicy(ui->scenesDock);
}

void PLSBasic::onMixerDockSeperateBtnClicked()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Mixer Dock Attach", ACTION_CLICK);

	bool isFloating = ui->mixerDock->isFloating();
	SetAttachWindowBtnText(actionSperateMixer, !isFloating);
	if (isFloating) {
		ui->mixerDock->setFloating(!isFloating);
		return;
	}

	SetDocksMovePolicy(ui->mixerDock);
}

void PLSBasic::SetDocksMovePolicy(PLSDock *dock)
{
	bool isFloating = dock->isFloating();
	if (!isFloating) {
		QPoint pre = mapToGlobal(QPoint(dock->x(), dock->y()));
		dock->setFloating(!isFloating);
		docksMovePolicy(dock, pre);
	}
}

void PLSBasic::docksMovePolicy(PLSDock *dock, const QPoint &pre)
{
	if (qAbs(dock->y() - pre.y()) < DOCK_DEATTACH_MIN_SIZE && qAbs(dock->x() - pre.x()) < DOCK_DEATTACH_MIN_SIZE) {
		dock->move(pre.x() + DOCK_DEATTACH_MIN_SIZE, pre.y() + DOCK_DEATTACH_MIN_SIZE);
	}
}

static void ConfirmColor(SourceTree *sources, const QColor &color, QModelIndexList selectedItems)
{
	QString strColor = color.name(QColor::HexArgb);
	for (int x = 0; x < selectedItems.count(); x++) {
		SourceTreeItem *treeItem = sources->GetItemWidget(selectedItems[x].row());
		treeItem->SetBgColor(SourceTreeItem::BgCustom, (void *)strColor.toStdString().c_str());

		OBSSceneItem sceneItem = sources->Get(selectedItems[x].row());
		obs_data_t *privData = obs_sceneitem_get_private_settings(sceneItem);
		obs_data_set_int(privData, "color-preset", 1);
		obs_data_set_string(privData, "color", strColor.toUtf8());
		obs_data_release(privData);
	}
}

void PLSBasic::ColorChange()
{
	QModelIndexList selectedItems = ui->sources->selectionModel()->selectedIndexes();
	QAction *action = qobject_cast<QAction *>(sender());
	QPushButton *colorButton = qobject_cast<QPushButton *>(sender());

	if (selectedItems.count() == 0)
		return;

	if (colorButton) {
		int preset = colorButton->property("bgColor").value<int>();
		for (int x = 0; x < selectedItems.count(); x++) {
			SourceTreeItem *treeItem = ui->sources->GetItemWidget(selectedItems[x].row());
			treeItem->SetBgColor(SourceTreeItem::BgPreset, (void *)(preset - 1)); // (preset - 1) index of preset is started with 1

			OBSSceneItem sceneItem = ui->sources->Get(selectedItems[x].row());
			obs_data_t *privData = obs_sceneitem_get_private_settings(sceneItem);
			obs_data_set_int(privData, "color-preset", preset + 1);
			obs_data_set_string(privData, "color", "");
			obs_data_release(privData);
		}

		for (int i = 1; i < 9; i++) {
			stringstream button;
			button << "preset" << i;
			QPushButton *cButton = colorButton->parentWidget()->findChild<QPushButton *>(button.str().c_str());
			cButton->setStyleSheet("border: 2px solid transparent");
		}

		colorButton->setStyleSheet("border: 2px solid black");
	} else if (action) {
		int preset = action->property("bgColor").value<int>();
		if (preset == 1) {
			OBSSceneItem curSceneItem = GetCurrentSceneItemData();
			SourceTreeItem *curTreeItem = GetItemWidgetFromSceneItem(curSceneItem);
			obs_data_t *curPrivData = obs_sceneitem_get_private_settings(curSceneItem);

			int oldPreset = obs_data_get_int(curPrivData, "color-preset");
			const QString oldSheet = curTreeItem->styleSheet();

			auto liveChangeColor = [=](const QColor &color) {
				if (color.isValid()) {
					QString strColor = color.name(QColor::HexArgb);
					curTreeItem->SetBgColor(SourceTreeItem::BgCustom, (void *)strColor.toStdString().c_str());
				}
			};

			auto changedColor = [=](const QColor &color) {
				if (color.isValid()) {
					ConfirmColor(ui->sources, color, selectedItems);
				}
			};

			auto rejected = [=]() {
				curTreeItem->setStyleSheet(oldSheet);
				curTreeItem->OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_NORMAL);
			};

			QColorDialog::ColorDialogOptions options = QColorDialog::ShowAlphaChannel;

			const char *oldColor = obs_data_get_string(curPrivData, "color");
			const char *customColor = *oldColor != 0 ? oldColor : "#55FF0000";
#ifdef __APPLE__
			options |= QColorDialog::DontUseNativeDialog;
#endif

			PLSColorDialogView *colorDialog = new PLSColorDialogView(this);
			colorDialog->setAttribute(Qt::WA_DeleteOnClose, true);
			colorDialog->setOptions(options);
			colorDialog->setCurrentColor(QColor(customColor));
			connect(colorDialog, &PLSColorDialogView::currentColorChanged, liveChangeColor);
			connect(colorDialog, &PLSColorDialogView::colorSelected, changedColor);
			connect(colorDialog, &PLSColorDialogView::rejected, rejected);
			colorDialog->open();

			obs_data_release(curPrivData);
		} else {
			for (int x = 0; x < selectedItems.count(); x++) {
				SourceTreeItem *treeItem = ui->sources->GetItemWidget(selectedItems[x].row());
				treeItem->SetBgColor(SourceTreeItem::BgDefault, NULL);

				OBSSceneItem sceneItem = ui->sources->Get(selectedItems[x].row());
				obs_data_t *privData = obs_sceneitem_get_private_settings(sceneItem);
				obs_data_set_int(privData, "color-preset", preset);
				obs_data_set_string(privData, "color", "");
				obs_data_release(privData);
			}
		}
	}
}

SourceTreeItem *PLSBasic::GetItemWidgetFromSceneItem(obs_sceneitem_t *sceneItem)
{
	int i = 0;
	SourceTreeItem *treeItem = ui->sources->GetItemWidget(i);
	OBSSceneItem item = ui->sources->Get(i);
	int64_t id = obs_sceneitem_get_id(sceneItem);
	while (treeItem && obs_sceneitem_get_id(item) != id) {
		i++;
		treeItem = ui->sources->GetItemWidget(i);
		item = ui->sources->Get(i);
	}
	if (treeItem)
		return treeItem;

	return nullptr;
}

void PLSBasic::on_autoConfigure_triggered()
{
	PLS_UI_STEP(MAINFRAME_MODULE, "Auto Configure", ACTION_CLICK);

	AutoConfig test(this);
	test.setModal(true);
	test.show();
	test.exec();
}

void PLSBasic::popupStats(bool show, const QPoint &pos)
{
	if (!stats.isNull()) {
		if (show) {
			QSize size = stats->size();
			stats->move(pos.x(), pos.y() - size.height());
			stats->show();
			stats->raise();
		} else {
			stats->hide();
		}
		return;
	} else if (!show) {
		return;
	}

	PLSDialogView *statsDlgView = new PLSDialogView(this);
	stats = statsDlgView;
	statsDlgView->setResizeEnabled(false);
	statsDlgView->setHasCaption(false);
	statsDlgView->setHasHLine(true);
	statsDlgView->setFixedSize(875, 184);

	PLSBasicStats *statsDlg = new PLSBasicStats(statsDlgView, statsDlgView->content());
	statsDlgView->setWidget(statsDlg);

	connect(statsDlg, &PLSBasicStats::showSignal, mainView->statusBar(), [this]() { mainView->statusBar()->setStatsOpen(true); });
	connect(statsDlg, &PLSBasicStats::hideSignal, mainView->statusBar(), [this]() { mainView->statusBar()->setStatsOpen(false); });

	QSize size = statsDlgView->size();
	statsDlgView->move(pos.x(), pos.y() - size.height());
	statsDlgView->show();
}

QWidget *PLSBasic::getChannelsContainer() const
{
	return mainView->channelsArea();
}

void PLSBasic::on_actionShowAbout_triggered()
{
	if (sender() == ui->actionShowAbout) {
		PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Help About", ACTION_CLICK);
	}

}

void PLSBasic::on_actionOpenSourceLicense_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Help Open Source License", ACTION_CLICK);
	PLSOpenSourceView view(this);
	view.exec();
}

void PLSBasic::ResizeOutputSizeOfSource()
{
	if (obs_video_active())
		return;

	PLSAlertView::Button button = PLSAlertView::warning(getMainView(), QTStr("ResizeOutputSizeOfSource"),
							    QTStr("ResizeOutputSizeOfSource.Text") + "\n\n" + QTStr("ResizeOutputSizeOfSource.Continue"),
							    {{PLSAlertView::Button::Yes, QTStr("Yes")}, {PLSAlertView::Button::No, QTStr("No")}});
	if (button != PLSAlertView::Button::Yes)
		return;

	OBSSource source = obs_sceneitem_get_source(GetCurrentSceneItemData());

	int width = obs_source_get_width(source);
	int height = obs_source_get_height(source);

	config_set_uint(basicConfig, "Video", "BaseCX", width);
	config_set_uint(basicConfig, "Video", "BaseCY", height);
	config_set_uint(basicConfig, "Video", "OutputCX", width);
	config_set_uint(basicConfig, "Video", "OutputCY", height);

	ResetVideo();
	on_actionFitToScreen_triggered();
}

void PLSBasic::singletonWakeup()
{
	PLS_INFO(MAINFRAME_MODULE, "singleton instance wakeup");
	if (App()->isAppRunning()) {
		PLS_INFO(MAINFRAME_MODULE, "wakeup main window");
		SetShowing(true);

		// bring window to top
		bringWindowToTop(mainView);
	} else {
		PLS_INFO(MAINFRAME_MODULE, "application is starting, don't wakeup");
	}
}

QAction *PLSBasic::AddDockWidget(QDockWidget *dock)
{
	assert(false);
	return nullptr;
}

PLSBasic *PLSBasic::Get()
{
	return reinterpret_cast<PLSBasic *>(App()->GetMainWindow());
}

bool PLSBasic::StreamingActive()
{
	if (!outputHandler)
		return false;
	return outputHandler->StreamingActive();
}

bool PLSBasic::RecordingActive()
{
	if (!outputHandler)
		return false;
	return outputHandler->RecordingActive();
}

bool PLSBasic::ReplayBufferActive()
{
	if (!outputHandler)
		return false;
	return outputHandler->ReplayBufferActive();
}

SceneRenameDelegate::SceneRenameDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void SceneRenameDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
	QStyledItemDelegate::setEditorData(editor, index);
	QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
	if (lineEdit)
		lineEdit->selectAll();
}

bool SceneRenameDelegate::eventFilter(QObject *editor, QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent->key() == Qt::Key_Escape) {
			QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
			if (lineEdit)
				lineEdit->undo();
		}
	}

	return QStyledItemDelegate::eventFilter(editor, event);
}

void PLSBasic::UpdatePatronJson(const QString &text, const QString &error)
{
	if (!error.isEmpty())
		return;

	patronJson = QT_TO_UTF8(text);
}

void PLSBasic::PauseRecording()
{
	if (!pause || !outputHandler || !outputHandler->fileOutput)
		return;

	obs_output_t *output = outputHandler->fileOutput;

	if (obs_output_pause(output, true)) {
		pause->setAccessibleName(QTStr("Basic.Main.UnpauseRecording"));
		pause->setToolTip(QTStr("Basic.Main.UnpauseRecording"));
		pause->blockSignals(true);
		pause->setChecked(true);
		pause->blockSignals(false);

		if (trayIcon)
			trayIcon->setIcon(QIcon(":/res/images/PRISMLiveStudio.ico"));

		os_atomic_set_bool(&recording_paused, true);

		if (api)
			api->on_event(OBS_FRONTEND_EVENT_RECORDING_PAUSED);

		if (os_atomic_load_bool(&replaybuf_active))
			ShowReplayBufferPauseWarning();
	}
}

void PLSBasic::UnpauseRecording()
{
	if (!pause || !outputHandler || !outputHandler->fileOutput)
		return;

	obs_output_t *output = outputHandler->fileOutput;

	if (obs_output_pause(output, false)) {
		pause->setAccessibleName(QTStr("Basic.Main.PauseRecording"));
		pause->setToolTip(QTStr("Basic.Main.PauseRecording"));
		pause->blockSignals(true);
		pause->setChecked(false);
		pause->blockSignals(false);

		if (trayIcon)
			trayIcon->setIcon(QIcon(":/res/images/PRISMLiveStudio.ico"));

		os_atomic_set_bool(&recording_paused, false);

		if (api)
			api->on_event(OBS_FRONTEND_EVENT_RECORDING_UNPAUSED);
	}
}

void PLSBasic::SetSceneDockSeperated(bool state)
{
	ui->scenesDock->setFloating(state);
}

bool PLSBasic::GetSceneDockSeperated()
{
	return ui->scenesDock->isFloating();
}

void PLSBasic::SetSourceDockSeperated(bool state)
{
	ui->sourcesDock->setFloating(state);
}

bool PLSBasic::GetSourceDockSeperated()
{
	return ui->sourcesDock->isFloating();
}

void PLSBasic::OnMultiviewLayoutChanged(int layout)
{
	ui->scenesFrame->RefreshMultiviewLayout(layout);
}

void PLSBasic::toastMessage(pls_toast_info_type type, const QString &message, int autoClose)
{
	mainView->toastMessage(type, message, autoClose);
}

void PLSBasic::toastClear()
{
	mainView->toastClear();
}

void PLSBasic::OnSceneDockTopLevelChanged()
{
	SetAttachWindowBtnText(actionSeperateScene, ui->scenesDock->isFloating());
}

void PLSBasic::OnSourceDockTopLevelChanged()
{
	SetAttachWindowBtnText(actionSeperateSource, ui->sourcesDock->isFloating());
}

void PLSBasic::onMixerDockLocationChanged()
{
	SetAttachWindowBtnText(actionSperateMixer, ui->mixerDock->isFloating());
}

void PLSBasic::onUpdateChatActionText(bool shown)
{
	ui->actionChat->setText(shown ? tr("Basic.MainMenu.View.ChatHide") : tr("Basic.MainMenu.View.ChatShow"));
}

void PLSBasic::PauseToggled()
{
	if (!pause || !outputHandler || !outputHandler->fileOutput)
		return;

	obs_output_t *output = outputHandler->fileOutput;
	bool enable = !obs_output_paused(output);

	if (enable)
		PauseRecording();
	else
		UnpauseRecording();
}

void PLSBasic::UpdatePause(bool activate)
{
	if (!activate || !outputHandler || !outputHandler->RecordingActive()) {
		pause.reset();
		return;
	}

	const char *mode = config_get_string(basicConfig, "Output", "Mode");
	bool adv = astrcmpi(mode, "Advanced") == 0;
	bool shared;

	if (adv) {
		const char *recType = config_get_string(basicConfig, "AdvOut", "RecType");

		if (astrcmpi(recType, "FFmpeg") == 0) {
			shared = config_get_bool(basicConfig, "AdvOut", "FFOutputToFile");
		} else {
			const char *recordEncoder = config_get_string(basicConfig, "AdvOut", "RecEncoder");
			shared = astrcmpi(recordEncoder, "none") == 0;
		}
	} else {
		const char *quality = config_get_string(basicConfig, "SimpleOutput", "RecQuality");
		shared = strcmp(quality, "Stream") == 0;
	}

	if (!shared) {
		pause.reset(new QPushButton());
		pause->setAccessibleName(QTStr("Basic.Main.PauseRecording"));
		pause->setToolTip(QTStr("Basic.Main.PauseRecording"));
		pause->setCheckable(true);
		pause->setChecked(false);
		pause->setProperty("themeID", QVariant(QStringLiteral("pauseIconSmall")));
		connect(pause.data(), &QAbstractButton::clicked, this, &PLSBasic::PauseToggled);
		//ui->recordingLayout->addWidget(pause.data());
	} else {
		pause.reset();
	}
}

#define MBYTE (1024ULL * 1024ULL)
#define MBYTES_LEFT_STOP_REC 50ULL
#define MAX_BYTES_LEFT (MBYTES_LEFT_STOP_REC * MBYTE)

void PLSBasic::delAllCookie()
{
	if (!panel_cookies) {
		InitBrowserPanelSafeBlock();
	}
	panel_cookies->DeleteCookies("", "");

	QString del_file = QString(pls_get_user_path("PRISMLiveStudio/plugin_config/obs-browser"));
	QDir dir;
	dir.setPath(del_file);
	dir.removeRecursively();
}

void PLSBasic::delSpecificUrlCookie(const QString &url)
{
	if (!panel_cookies) {
		InitBrowserPanelSafeBlock();
	}
	panel_cookies->DeleteCookies(url.toStdString(), "");
}

const char *PLSBasic::GetCurrentOutputPath()
{
	const char *path = nullptr;
	const char *mode = config_get_string(Config(), "Output", "Mode");

	if (strcmp(mode, "Advanced") == 0) {
		const char *advanced_mode = config_get_string(Config(), "AdvOut", "RecType");

		if (strcmp(advanced_mode, "FFmpeg") == 0) {
			path = config_get_string(Config(), "AdvOut", "FFFilePath");
		} else {
			path = config_get_string(Config(), "AdvOut", "RecFilePath");
		}
	} else {
		path = config_get_string(Config(), "SimpleOutput", "FilePath");
	}

	return path;
}

bool PLSBasic::QueryRemoveSourceWithNoNotifier(obs_source_t *source)
{
	if (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE && !obs_source_is_group(source)) {
		int count = PLSSceneDataMgr::Instance()->GetSceneSize();
		if (count == 1) {
			if (ui->sources->Count() >= 1) {
				QVector<OBSSceneItem> items = ui->sources->GetItems();
				for (auto &item : items) {
					obs_sceneitem_remove(item);
				}
				return false;
			}
			return false;
		}
	}
	return true;
}

bool PLSBasic::QueryRemoveSource(obs_source_t *source)
{
	if (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE && !obs_source_is_group(source)) {
		int count = PLSSceneDataMgr::Instance()->GetSceneSize();
		if (count == 1) {
			if (ui->sources->Count() >= 1) {
				QVector<OBSSceneItem> items = ui->sources->GetItems();
				for (auto &item : items) {
					obs_sceneitem_remove(item);
				}
				return false;
			}

			PLSMessageBox::information(this, QTStr("FinalScene.Title"), QTStr("FinalScene.Text"));
			return false;
		}
	}

	const char *name = obs_source_get_name(source);

	QString text = QTStr("ConfirmRemove.Text.title");

	if (0 == strcmp(App()->GetLocale(), "ko-KR")) {
		return PLSAlertView::Button::Ok == PLSMessageBox::question(getMainView(), QTStr("ConfirmRemove.Title"), name, text, PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel);

	} else {
		return PLSAlertView::Button::Ok == PLSMessageBox::question(getMainView(), QTStr("ConfirmRemove.Title"), text, name, PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel);
	}
}

int PLSBasic::GetTransitionDuration()
{
	return ui->scenesFrame->GetTransitionDurationValue();
}

QSpinBox *PLSBasic::GetTransitionDurationSpinBox()
{
	return ui->scenesFrame->GetTransitionDurationSpinBox();
}

QComboBox *PLSBasic::GetTransitionCombobox()
{
	return ui->scenesFrame->GetTransitionCombobox();
}

OBSSource PLSBasic::GetCurrentTransition()
{
	return ui->scenesFrame->GetCurrentTransition();
}

void PLSBasic::DiskSpaceMessage()
{
	blog(LOG_ERROR, "Recording stopped because of low disk space");

	PLSMessageBox::critical(this, QTStr("Output.RecordNoSpace.Title"), QTStr("Output.RecordNoSpace.Msg"));
}

void PLSBasic::onPropertyChanged(OBSSource source)
{
	emit propertiesChaned(source);
}

void PLSBasic::SetAttachWindowBtnText(QAction *action, bool isFloating)
{
	if (!action) {
		return;
	}
	if (isFloating) {
		action->setText(QTStr(MAIN_MAINFRAME_TOOLTIP_REATTACH));
	} else {
		action->setText(QTStr(MAIN_MAINFRAME_TOOLTIP_DETACH));
	}
}

bool PLSBasic::LowDiskSpace()
{
	const char *path;

	path = GetCurrentOutputPath();
	if (!path)
		return false;

	uint64_t num_bytes = os_get_free_disk_space(path);

	if (num_bytes < (MAX_BYTES_LEFT))
		return true;
	else
		return false;
}

void PLSBasic::CheckDiskSpaceRemaining()
{
	if (LowDiskSpace()) {
		StopRecording();
		StopReplayBuffer();

		DiskSpaceMessage();
	}
}

void PLSBasic::ScenesReordered(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row)
{
	UNUSED_PARAMETER(parent);
	UNUSED_PARAMETER(start);
	UNUSED_PARAMETER(end);
	UNUSED_PARAMETER(destination);
	UNUSED_PARAMETER(row);

	PLSProjector::UpdateMultiviewProjectors();
}

void PLSBasic::ResetStatsHotkey()
{
	QList<PLSBasicStats *> list = findChildren<PLSBasicStats *>();

	foreach(PLSBasicStats * s, list) s->Reset();
}

void PLSBasic::onPopupSettingView(const QString &tab, const QString &group)
{
	static bool settings_already_executing = false;

	if (sender() == ui->action_Settings) {
		PLS_UI_STEP(MAINMENU_MODULE, "Main Menu File Settings", ACTION_CLICK);
	}

	int result = -1;
	bool isLanguageChange = false;
	{
		/* Do not load settings window if inside of a temporary event loop
		 * because we could be inside of an Auth::LoadUI call.  Keep trying
		 * once per second until we've exit any known sub-loops. */
		if (os_atomic_load_long(&insideEventLoop) != 0) {
			QTimer::singleShot(1000, this, [tab, group, this]() { onPopupSettingView(tab, group); });
			return;
		}

		if (settings_already_executing) {
			return;
		}

		settings_already_executing = true;

		PLSBasicSettings settings(this);
		settings.switchTo(tab, group);
		result = settings.exec();
		SystemTray(false);

		settings_already_executing = false;
	}
}

bool PLSBasic::willShow()
{
	//add login event
	PLSBasic ::Get()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGIN);
	return true;
}

void PLSBasic::showSettingVideo()
{
	onPopupSettingView(QStringLiteral("Output"), QString());
}

void PLSBasic::OnLogoutEvent()
{
	CreateDefaultScene(true);
	SaveProject();
}
