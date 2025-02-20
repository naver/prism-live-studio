#include <obs-frontend-internal.hpp>
#include "PLSApp.h"
#include "qt-wrappers.hpp"
#include "window-basic-main.hpp"
#include "window-basic-main-outputs.hpp"
#include "PLSSceneDataMgr.h"
#include "channel-login-view.hpp"
#include "frontend-api/frontend-internal.hpp"
#include <functional>
#include "pls-common-define.hpp"
#include "libhttp-client.h"
#include "liblog.h"
#include "libutils-api.h"
#include "pls-gpop-data.hpp"
#include "pls-notice-handler.hpp"
#include "libutils-api.h"
#include <util/windows/win-version.h>
#include "PLSBasic.h"
#include "PLSMainView.hpp"
#include "network-state.h"
#include "../log/module_names.h"
#include "prism-version.h"
#include <QUrlQuery>
#include "PLSUpdateView.hpp"
#include "PLSMotionImageListView.h"
#include "login-user-info.hpp"
#include "browser-panel.hpp"
#include "PLSBrowserView.hpp"
#include "PLSPlatformApi.h"
#include "PLSPlatformNaverShoppingLIVE.h"
#include "PLSMotionFileManager.h"
#include "TextMotionTemplateDataHelper.h"
#include "PLSChatTemplateDataHelper.h"
#include "PLSChannelDataAPI.h"
#include "PLSPlatformApi.h"
#include "PLSPlatformPrism.h"
#include "ChannelCommonFunctions.h"
#include "ResolutionGuidePage.h"
#include "PLSPropertyModel.hpp"
#include "PLSTemplateButton.h"
#include <pls/pls-properties.h>
#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSAudioControl.h"
#include "PLSLaboratory.h"
#include "RemoteChat/PLSRemoteChatView.h"
#include "RemoteChat/PLSRemoteChatManage.h"
#include "PLSLaboratoryManage.h"
#include "PLSLaunchWizardView.h"
#include "PLSSyncServerManager.hpp"
#include "PLSLiveInfoChzzk.h"
#include <pls/pls-dual-output.h>
#include "PLSLoginDataHandler.h"

using namespace std;
using namespace common;

Q_DECLARE_METATYPE(OBSScene);
Q_DECLARE_METATYPE(OBSSource);

#ifdef Q_OS_WIN64
#define PLATFORM_TYPE QStringLiteral("WIN64")
#else
#define PLATFORM_TYPE QStringLiteral("MAC")
#endif

constexpr auto VERSION_COMPARE_COUNT = 3;
#define PARAM_REPLY_EMAIL_ADDRESS QStringLiteral("replyEmailAddress")
#define PARAM_QUESTION_TYPE QStringLiteral("questionType")
#define PARAM_SERVICE_TYPE QStringLiteral("serviceType")
#define PARAM_REPLY_QUESTION QStringLiteral("question")
#define PARAM_REPLY_ATTACHED_FILES QStringLiteral("attachedFiles")
#define PARAM_PLATFORM_TYPE QStringLiteral("platformType")
#define PARAM_VERSION QStringLiteral("version")
#define PARAM_APP_TYPE_KEY QStringLiteral("appType")
#define PARAM_APP_TYPE_VALUE QStringLiteral("LIVE_STUDIO")

extern PLSLaboratory *g_laboratoryDialog;
extern PLSRemoteChatView *g_remoteChatDialog;
extern QString getOsVersion();
extern void httpRequestHead(QVariantMap &headMap, bool hasGacc);
extern OBSOutputSet obsOutputSet;
extern OBSOutputSet obsOutputSet_v;

template<typename T> static T GetOBSRef(QListWidgetItem *item)
{
	return item->data(static_cast<int>(QtDataRole::OBSRef)).value<T>();
}

void EnumProfiles(function<bool(const char *, const char *)> &&cb);
void EnumSceneCollections(const std::function<bool(const char *, const char *)> &cb);

int getActivedChatChannelCount();

extern volatile bool streaming_active;
extern volatile bool recording_active;
extern volatile bool recording_paused;
extern volatile bool replaybuf_active;
extern volatile bool virtualcam_active;
extern QCef *cef;

/* ------------------------------------------------------------------------- */

template<typename T> struct OBSStudioCallback {
	T callback;
	void *private_data;

	inline OBSStudioCallback(T cb, void *p) : callback(cb), private_data(p) {}
};

template<typename T> inline size_t GetCallbackIdx(vector<OBSStudioCallback<T>> &callbacks, T callback, void *private_data)
{
	for (size_t i = 0; i < callbacks.size(); i++) {
		OBSStudioCallback<T> curCB = callbacks[i];
		if (curCB.callback == callback && curCB.private_data == private_data)
			return i;
	}

	return (size_t)-1;
}

class EventCallbacks {
	std::list<std::pair<pls_frontend_event_cb, void *>> ecbs, adds, removes;

	void add(std::list<std::pair<pls_frontend_event_cb, void *>> &dst, std::list<std::pair<pls_frontend_event_cb, void *>> &src)
	{
		while (!src.empty()) {
			dst.push_back(src.front());
			src.pop_front();
		}
	}
	void remove(std::list<std::pair<pls_frontend_event_cb, void *>> &dst, std::list<std::pair<pls_frontend_event_cb, void *>> &src)
	{
		while (!src.empty()) {
			dst.remove(src.front());
			src.pop_front();
		}
	}

public:
	void add(pls_frontend_event_cb callback, void *context) { adds.push_back(std::make_pair(callback, context)); }
	void remove(pls_frontend_event_cb callback, void *context) { removes.push_back(std::make_pair(callback, context)); }
	void trigger(pls_frontend_event event, const QVariantList &params)
	{
		add(ecbs, adds);
		remove(ecbs, removes);

		for (auto iter = ecbs.begin(); iter != ecbs.end(); ++iter) {
			auto ecb = *iter;
			if (std::find(removes.begin(), removes.end(), ecb) == removes.end())
				ecb.first(event, params, ecb.second);
		}

		add(ecbs, adds);
		remove(ecbs, removes);
	}
};

class BrowserDock;
struct OBSStudioAPI : pls_frontend_callbacks {
	OBSBasic *main;
	vector<OBSStudioCallback<obs_frontend_event_cb>> callbacks;
	vector<OBSStudioCallback<obs_frontend_save_cb>> saveCallbacks;
	vector<OBSStudioCallback<obs_frontend_save_cb>> preloadCallbacks;
	QList<PLSLoginInfo *> loginInfos;
	EventCallbacks eventCallbacks;
	QMap<QString, QSharedPointer<BrowserDock>> chatDocks;
	inline OBSStudioAPI(OBSBasic *main_) : main(main_) {}

	inline PLSBasic *basic() { return static_cast<PLSBasic *>(main); }
	inline PLSMainView *mainView() { return basic()->getMainView(); }

	void *obs_frontend_get_main_window(void) override { return (void *)main; }

	void *obs_frontend_get_main_window_handle(void) override { return (void *)main->winId(); }

	void *obs_frontend_get_system_tray(void) override { return (void *)main->trayIcon.data(); }

	void obs_frontend_get_scenes(struct obs_frontend_source_list *sources) override
	{
		SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
		for (auto iter = data.begin(); iter != data.end(); ++iter) {
			OBSScene scene = iter->second->GetData();
			if (obs_source_t *source = obs_source_get_ref(obs_scene_get_source(scene)); source) {
				da_push_back(sources->sources, &source);
			}
		}
	}

	obs_source_t *obs_frontend_get_current_scene(void) override
	{
		if (pls_is_app_exiting()) {
			return nullptr;
		}
		if (main->IsPreviewProgramMode() && main->programScene) {
			return obs_weak_source_get_source(main->programScene);
		} else {
			OBSSource source = main->GetCurrentSceneSource();
			return obs_source_get_ref(source);
		}
	}

	void obs_frontend_set_current_scene(obs_source_t *scene) override
	{
		if (main->IsPreviewProgramMode()) {
			QMetaObject::invokeMethod(main, "TransitionToScene", WaitConnection(), Q_ARG(OBSSource, OBSSource(scene)));
		} else {
			QMetaObject::invokeMethod(main, "SetCurrentScene", WaitConnection(), Q_ARG(OBSSource, OBSSource(scene)), Q_ARG(bool, false));
		}
	}

	void obs_frontend_get_transitions(struct obs_frontend_source_list *sources) override
	{
		const QComboBox *box = main->GetTransitionCombobox();
		for (int i = 0; i < box->count(); i++) {
			if (auto source = obs_source_get_ref(box->itemData(i).value<OBSSource>()); source) {
				da_push_back(sources->sources, &source);
			}
		}
	}

	obs_source_t *obs_frontend_get_current_transition(void) override
	{
		OBSSource tr = main->GetCurrentTransition();
		return obs_source_get_ref(tr);
	}

	void obs_frontend_set_current_transition(obs_source_t *transition) override { QMetaObject::invokeMethod(main, "SetTransition", Q_ARG(OBSSource, OBSSource(transition))); }

	int obs_frontend_get_transition_duration(void) override { return main->GetTransitionDuration(); }

	void obs_frontend_set_transition_duration(int duration) override { QMetaObject::invokeMethod(main->GetTransitionDurationSpinBox(), "setValue", Q_ARG(int, duration)); }

	void obs_frontend_release_tbar(void) override { QMetaObject::invokeMethod(main, "TBarReleased"); }

	void obs_frontend_set_tbar_position(int position) override { QMetaObject::invokeMethod(main, "TBarChanged", Q_ARG(int, position)); }

	int obs_frontend_get_tbar_position(void) override { return 0; }

	void obs_frontend_get_scene_collections(std::vector<std::string> &strings) override
	{
		auto addCollection = [&](const char *name, const char *) {
			strings.emplace_back(name);
			return true;
		};

		EnumSceneCollections(addCollection);
	}

	char *obs_frontend_get_current_scene_collection(void) override
	{
		const char *cur_name = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
		return bstrdup(cur_name);
	}

	void obs_frontend_set_current_scene_collection(const char *collection) override { main->SetCurrentSceneCollection(collection); }

	bool obs_frontend_add_scene_collection(const char *name) override
	{
		bool success = false;
		QMetaObject::invokeMethod(main, "AddSceneCollection", WaitConnection(), Q_RETURN_ARG(bool, success), Q_ARG(bool, true), Q_ARG(QWidget *, main), Q_ARG(QString, QT_UTF8(name)),
					  Q_ARG(QString, QT_UTF8("")), Q_ARG(QString, QT_UTF8("")));
		return success;
	}

	void obs_frontend_get_profiles(std::vector<std::string> &strings) override
	{
		auto addProfile = [&](const char *name, const char *) {
			strings.emplace_back(name);
			return true;
		};

		EnumProfiles(addProfile);
	}

	char *obs_frontend_get_current_profile(void) override
	{
		const char *name = config_get_string(App()->GlobalConfig(), "Basic", "Profile");
		return bstrdup(name);
	}

	char *obs_frontend_get_current_profile_path(void) override
	{
		char profilePath[512];
		int ret = GetProfilePath(profilePath, sizeof(profilePath), "");
		if (ret <= 0)
			return nullptr;

		return bstrdup(profilePath);
	}

	void obs_frontend_set_current_profile(const char *profile) override
	{
		QList<QAction *> menuActions = main->ui->profileMenu->actions();
		QString qstrProfile = QT_UTF8(profile);

		for (int i = 0; i < menuActions.count(); i++) {
			QAction *action = menuActions[i];
			QVariant v = action->property("file_name");

			if (v.typeName() != nullptr && action->text() == qstrProfile) {
				auto menu = action->menu();
				auto actions = menu->actions();
				if (!actions.empty() && actions[0]) {
					actions[0]->triggered();
					break;
				}
			}
		}
	}

	void obs_frontend_create_profile(const char *name) override { QMetaObject::invokeMethod(main, "NewProfile", Q_ARG(QString, name)); }

	void obs_frontend_duplicate_profile(const char *name) override { QMetaObject::invokeMethod(main, "DuplicateProfile", Q_ARG(QString, name)); }

	void obs_frontend_delete_profile(const char *profile) override { QMetaObject::invokeMethod(main, "DeleteProfile", Q_ARG(const QString &, profile)); }

	void obs_frontend_streaming_start(void) override
	{
		if (!pls_get_hotkey_enable())
			return;
		// PRISM_PC-1009 All the streamings MUST start by PRISM APIs
		// since there are many other logics before streamings.
		pls_start_broadcast(true);
	}

	void obs_frontend_streaming_stop(void) override
	{
		if (!pls_get_hotkey_enable())
			return;
		// PRISM_PC-1009 All the streamings MUST start by PRISM APIs
		// since there are many other logics before streamings.
		pls_start_broadcast(false);
	}

	bool obs_frontend_streaming_active(void) override { return os_atomic_load_bool(&streaming_active); }

	void obs_frontend_recording_start(void) override { QMetaObject::invokeMethod(main, "StartRecording"); }

	void obs_frontend_recording_stop(void) override { QMetaObject::invokeMethod(main, "StopRecording"); }

	bool obs_frontend_recording_active(void) override { return os_atomic_load_bool(&recording_active); }

	void obs_frontend_recording_pause(bool pause) override { QMetaObject::invokeMethod(main, pause ? "PauseRecording" : "UnpauseRecording"); }

	bool obs_frontend_recording_paused(void) override { return os_atomic_load_bool(&recording_paused); }

	bool obs_frontend_recording_split_file(void) override
	{
		if (os_atomic_load_bool(&recording_active) && !os_atomic_load_bool(&recording_paused)) {
			proc_handler_t *ph = obs_output_get_proc_handler(main->outputHandler->fileOutput);
			uint8_t stack[128];
			calldata cd;
			calldata_init_fixed(&cd, stack, sizeof(stack));
			proc_handler_call(ph, "split_file", &cd);
			bool result = calldata_bool(&cd, "split_file_enabled");
			return result;
		} else {
			return false;
		}
	}

	bool obs_frontend_recording_add_chapter(const char *name) override
	{
		if (!os_atomic_load_bool(&recording_active) || os_atomic_load_bool(&recording_paused))
			return false;

		proc_handler_t *ph = obs_output_get_proc_handler(main->outputHandler->fileOutput);

		calldata cd;
		calldata_init(&cd);
		calldata_set_string(&cd, "chapter_name", name);
		bool result = proc_handler_call(ph, "add_chapter", &cd);
		calldata_free(&cd);
		return result;
	}

	void obs_frontend_replay_buffer_start(void) override { QMetaObject::invokeMethod(main, "StartReplayBuffer"); }

	void obs_frontend_replay_buffer_save(void) override { QMetaObject::invokeMethod(main, "ReplayBufferSave"); }

	void obs_frontend_replay_buffer_stop(void) override { QMetaObject::invokeMethod(main, "StopReplayBuffer"); }

	bool obs_frontend_replay_buffer_active(void) override { return os_atomic_load_bool(&replaybuf_active); }

	void *obs_frontend_add_tools_menu_qaction(const char *name) override
	{
		main->ui->menuTools->setEnabled(true);
		return (void *)main->ui->menuTools->addAction(QT_UTF8(name));
	}

	void obs_frontend_add_tools_menu_item(const char *name, obs_frontend_cb callback, void *private_data) override
	{
		main->ui->menuTools->setEnabled(true);

		auto func = [private_data, callback]() { callback(private_data); };

		QAction *action = main->ui->menuTools->addAction(QT_UTF8(name));
		QObject::connect(action, &QAction::triggered, func);
	}

	void *obs_frontend_add_dock(void *dock) override
	{
		QDockWidget *d = reinterpret_cast<QDockWidget *>(dock);

		QString name = d->objectName();
		if (name.isEmpty() || main->IsDockObjectNameUsed(name)) {
			blog(LOG_WARNING, "The object name of the added dock is empty or already used,"
					  " a temporary one will be set to avoid conflicts");

			char *uuid = os_generate_uuid();
			name = QT_UTF8(uuid);
			bfree(uuid);
			name.append("_oldExtraDock");

			d->setObjectName(name);
		}

		return (void *)main->AddDockWidget(d);
	}

	bool obs_frontend_add_dock_by_id(const char *id, const char *title, void *widget) override
	{
		if (main->IsDockObjectNameUsed(QT_UTF8(id))) {
			blog(LOG_WARNING,
			     "Dock id '%s' already used!  "
			     "Duplicate library?",
			     id);
			return false;
		}

		OBSDockOri *dock = new OBSDockOri(main);
		dock->setWidget((QWidget *)widget);
		dock->setWindowTitle(QT_UTF8(title));
		dock->setObjectName(QT_UTF8(id));

		main->AddDockWidget(dock, Qt::RightDockWidgetArea);

		dock->setVisible(false);
		dock->setFloating(true);

		return true;
	}

	void obs_frontend_remove_dock(const char *id) override { main->RemoveDockWidget(QT_UTF8(id)); }

	bool obs_frontend_add_custom_qdock(const char *id, void *dock) override
	{
		if (main->IsDockObjectNameUsed(QT_UTF8(id))) {
			blog(LOG_WARNING,
			     "Dock id '%s' already used!  "
			     "Duplicate library?",
			     id);
			return false;
		}

		QDockWidget *d = reinterpret_cast<QDockWidget *>(dock);
		d->setObjectName(QT_UTF8(id));

		main->AddCustomDockWidget(d);

		return true;
	}

	void obs_frontend_add_event_callback(obs_frontend_event_cb callback, void *private_data) override
	{
		size_t idx = GetCallbackIdx(callbacks, callback, private_data);
		if (idx == (size_t)-1)
			callbacks.emplace_back(callback, private_data);
	}

	void obs_frontend_remove_event_callback(obs_frontend_event_cb callback, void *private_data) override
	{
		size_t idx = GetCallbackIdx(callbacks, callback, private_data);
		if (idx == (size_t)-1)
			return;

		callbacks.erase(callbacks.begin() + idx);
	}

	obs_output_t *obs_frontend_get_streaming_output(void) override
	{
		std::lock_guard locker(obsOutputSet.mutex);
		auto multitrackVideo = main->outputHandler->multitrackVideo.get();
		auto mtvOutput = multitrackVideo ? obs_output_get_ref(multitrackVideo->StreamingOutput()) : nullptr;
		if (mtvOutput)
			return mtvOutput;

		if (obsOutputSet.streamOutput) {
			OBSOutput output = obsOutputSet.streamOutput;
			return obs_output_get_ref(output);
		}

		return NULL;
	}

	obs_output_t *obs_frontend_get_recording_output(void) override
	{
		std::lock_guard locker(obsOutputSet.mutex);
		if (obsOutputSet.fileOutput) {
			OBSOutput output = obsOutputSet.fileOutput;
			return obs_output_get_ref(output);
		}

		return NULL;
	}

	obs_output_t *obs_frontend_get_replay_buffer_output(void) override
	{
		std::lock_guard locker(obsOutputSet.mutex);
		if (obsOutputSet.replayBuffer) {
			OBSOutput output = obsOutputSet.replayBuffer;
			return obs_output_get_ref(output);
		}

		return NULL;
	}

	config_t *obs_frontend_get_profile_config(void) override { return main->basicConfig; }

	config_t *obs_frontend_get_global_config(void) override { return App()->GlobalConfig(); }

	void obs_frontend_open_projector(const char *type, int monitor, const char *geometry, const char *name) override
	{
		SavedProjectorInfo proj = {
			ProjectorType::Preview,
			monitor,
			geometry ? geometry : "",
			name ? name : "",
		};
		if (type) {
			if (astrcmpi(type, "Source") == 0)
				proj.type = ProjectorType::Source;
			else if (astrcmpi(type, "Scene") == 0)
				proj.type = ProjectorType::Scene;
			else if (astrcmpi(type, "StudioProgram") == 0)
				proj.type = ProjectorType::StudioProgram;
			else if (astrcmpi(type, "Multiview") == 0)
				proj.type = ProjectorType::Multiview;
		}
		QMetaObject::invokeMethod(main, "OpenSavedProjector", WaitConnection(), Q_ARG(SavedProjectorInfo *, &proj));
	}

	void obs_frontend_save(void) override { main->SaveProject(); }

	void obs_frontend_defer_save_begin(void) override { QMetaObject::invokeMethod(main, "DeferSaveBegin"); }

	void obs_frontend_defer_save_end(void) override { QMetaObject::invokeMethod(main, "DeferSaveEnd"); }

	void obs_frontend_add_save_callback(obs_frontend_save_cb callback, void *private_data) override
	{
		size_t idx = GetCallbackIdx(saveCallbacks, callback, private_data);
		if (idx == (size_t)-1)
			saveCallbacks.emplace_back(callback, private_data);
	}

	void obs_frontend_remove_save_callback(obs_frontend_save_cb callback, void *private_data) override
	{
		size_t idx = GetCallbackIdx(saveCallbacks, callback, private_data);
		if (idx == (size_t)-1)
			return;

		saveCallbacks.erase(saveCallbacks.begin() + idx);
	}

	void obs_frontend_add_preload_callback(obs_frontend_save_cb callback, void *private_data) override
	{
		size_t idx = GetCallbackIdx(preloadCallbacks, callback, private_data);
		if (idx == (size_t)-1)
			preloadCallbacks.emplace_back(callback, private_data);
	}

	void obs_frontend_remove_preload_callback(obs_frontend_save_cb callback, void *private_data) override
	{
		size_t idx = GetCallbackIdx(preloadCallbacks, callback, private_data);
		if (idx == (size_t)-1)
			return;

		preloadCallbacks.erase(preloadCallbacks.begin() + idx);
	}

	void obs_frontend_push_ui_translation(obs_frontend_translate_ui_cb translate) override { App()->PushUITranslation(translate); }

	void obs_frontend_pop_ui_translation(void) override { App()->PopUITranslation(); }

	void obs_frontend_set_streaming_service(obs_service_t *service) override { main->SetService(service); }

	obs_service_t *obs_frontend_get_streaming_service(void) override { return main->GetService(); }

	void obs_frontend_save_streaming_service(void) override { main->SaveService(); }

	bool obs_frontend_preview_program_mode_active(void) override { return main->IsPreviewProgramMode(); }

	void obs_frontend_set_preview_program_mode(bool enable) override { main->SetPreviewProgramMode(enable); }

	void obs_frontend_preview_program_trigger_transition(void) override { QMetaObject::invokeMethod(main, "TransitionClicked"); }

	bool obs_frontend_preview_enabled(void) override { return main->previewEnabled; }

	void obs_frontend_set_preview_enabled(bool enable) override
	{
		if (main->previewEnabled != enable)
			main->EnablePreviewDisplay(enable);
	}

	obs_source_t *obs_frontend_get_current_preview_scene(void) override
	{
		if (main->IsPreviewProgramMode()) {
			OBSSource source = main->GetCurrentSceneSource();
			return obs_source_get_ref(source);
		}

		return nullptr;
	}

	void obs_frontend_set_current_preview_scene(obs_source_t *scene) override
	{
		if (main->IsPreviewProgramMode()) {
			QMetaObject::invokeMethod(main, "SetCurrentScene", Q_ARG(OBSSource, OBSSource(scene)), Q_ARG(bool, false));
		}
	}

	void obs_frontend_take_screenshot(void) override
	{
		pls_check_app_exiting();
		QMetaObject::invokeMethod(main, "Screenshot");
	}

	void obs_frontend_take_source_screenshot(obs_source_t *source) override
	{
		pls_check_app_exiting();
		QMetaObject::invokeMethod(main, "Screenshot", Q_ARG(OBSSource, OBSSource(source)));
	}

	obs_output_t *obs_frontend_get_virtualcam_output(void) override
	{
		std::lock_guard locker(obsOutputSet.mutex);
		if (obsOutputSet.virtualCam) {
			OBSOutput output = obsOutputSet.virtualCam;
			return obs_output_get_ref(output);
		}

		return NULL;
	}

	void obs_frontend_start_virtualcam(void) override { QMetaObject::invokeMethod(main, "StartVirtualCam"); }

	void obs_frontend_stop_virtualcam(void) override { QMetaObject::invokeMethod(main, "StopVirtualCam"); }

	bool obs_frontend_virtualcam_active(void) override { return os_atomic_load_bool(&virtualcam_active); }

	void obs_frontend_reset_video(void) override { main->ResetVideo(); }

	void obs_frontend_open_source_properties(obs_source_t *source) override { QMetaObject::invokeMethod(main, "OpenProperties", Q_ARG(OBSSource, OBSSource(source))); }

	void obs_frontend_open_source_filters(obs_source_t *source) override { QMetaObject::invokeMethod(main, "OpenFilters", Q_ARG(OBSSource, OBSSource(source))); }

	void obs_frontend_open_source_interaction(obs_source_t *source) override { QMetaObject::invokeMethod(main, "OpenInteraction", Q_ARG(OBSSource, OBSSource(source))); }

	void obs_frontend_open_sceneitem_edit_transform(obs_sceneitem_t *item) override { QMetaObject::invokeMethod(main, "OpenEditTransform", Q_ARG(OBSSceneItem, OBSSceneItem(item))); }

	char *obs_frontend_get_current_record_output_path(void) override
	{
		const char *recordOutputPath = main->GetCurrentOutputPath();

		return bstrdup(recordOutputPath);
	}

	const char *obs_frontend_get_locale_string(const char *string) override { return Str(string); }

	bool obs_frontend_is_theme_dark(void) override { return App()->IsThemeDark(); }

	char *obs_frontend_get_last_recording(void) override { return bstrdup(main->outputHandler->lastRecordingPath.c_str()); }

	char *obs_frontend_get_last_screenshot(void) override { return bstrdup(main->lastScreenshot.c_str()); }

	char *obs_frontend_get_last_replay(void) override { return bstrdup(main->lastReplay.c_str()); }

	void obs_frontend_add_undo_redo_action(const char *name, const undo_redo_cb undo, const undo_redo_cb redo, const char *undo_data, const char *redo_data, bool repeatable) override
	{
		main->undo_s.add_action(
			name, [undo](const std::string &data) { undo(data.c_str()); }, [redo](const std::string &data) { redo(data.c_str()); }, undo_data, redo_data, repeatable);
	}

	void on_load(obs_data_t *settings) override
	{
		for (size_t i = saveCallbacks.size(); i > 0; i--) {
			auto cb = saveCallbacks[i - 1];
			cb.callback(settings, false, cb.private_data);
		}
	}

	void on_preload(obs_data_t *settings) override
	{
		for (size_t i = preloadCallbacks.size(); i > 0; i--) {
			auto cb = preloadCallbacks[i - 1];
			cb.callback(settings, false, cb.private_data);
		}
	}

	void on_save(obs_data_t *settings) override
	{
		for (size_t i = saveCallbacks.size(); i > 0; i--) {
			auto cb = saveCallbacks[i - 1];
			cb.callback(settings, true, cb.private_data);
		}
	}

	void on_event(enum obs_frontend_event event) override
	{
		if (main->disableSaving && event != OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP && event != OBS_FRONTEND_EVENT_EXIT)
			return;

		for (size_t i = callbacks.size(); i > 0; i--) {
			auto cb = callbacks[i - 1];
			cb.callback(event, cb.private_data);
		}
	}

	bool pls_register_login_info(PLSLoginInfo *login_info) override
	{
		loginInfos.append(login_info);
		return true;
	}

	void pls_unregister_login_info(PLSLoginInfo *login_info) override { loginInfos.removeOne(login_info); }

	int pls_get_login_info_count() override { return static_cast<int>(loginInfos.size()); }

	PLSLoginInfo *pls_get_login_info(int index) override
	{
		if ((index >= 0) && (index < loginInfos.size())) {
			return loginInfos.at(index);
		}
		return nullptr;
	}

	void del_pannel_cookies(const QString &pannelName) override { PLSBasic::delPannelCookies(pannelName); }

	void pls_set_manual_cookies(const QString &pannelName) override { PLSBasic::setManualPannelCookies(pannelName); }

	void pls_del_specific_url_cookie(const QString &url, const QString &cookieName) override
	{
		//main->delSpecificUrlCookie(url, cookieName);
	}

	QJsonObject pls_ssmap_to_json(const QMap<QString, QString> &ssmap) override
	{
		QJsonObject result;
		for (auto iter = ssmap.begin(), endIter = ssmap.end(); iter != endIter; ++iter) {
			result.insert(iter.key(), iter.value());
		}
		return result;
	}

	bool pls_browser_view(QVariantHash &result, const QUrl &url, const pls_result_checking_callback_t &callback, QWidget *parent, bool readCookies) override
	{
		if ((!cef) || (!cef->init_browser())) { // 2021.12.10 #10003 stack overflow
			return false;
		}

		PLSBrowserView bv(readCookies, &result, url, callback, parent);
		bv.update();
		pls::HotKeyLocker locker;
		return bv.exec() == QDialog::Accepted;
	}

	bool pls_browser_view(QVariantHash &result, const QUrl &url, const std_map<std::string, std::string> &headers, const QString &pannelName, const std::string &script,
			      const pls_result_checking_callback_t &callback, QWidget *parent, bool readCookies) override
	{
		if ((!cef) || (!cef->init_browser())) { // 2021.12.10 #10003 stack overflow
			return false;
		}

		PLSBrowserView bv(readCookies, &result, url, headers, pannelName, script, callback, parent);
		// 2020.08.03 cheng.bing
		//disable hotkey,able hotkey

		pls::HotKeyLocker locker;
		bool isOk = (bv.exec() == QDialog::Accepted);
		return isOk;
	}

	bool pls_rtmp_view(QJsonObject &, PLSLoginInfo *, QWidget *) override { return false; }

	bool pls_channel_login(QJsonObject &result, QWidget *parent) override
	{
		PLSChannelLoginView clv(result, parent);
		return clv.exec() == QDialog::Accepted;
	}

	void pls_prism_change_over_login_view() override
	{
		QFile::remove(pls_get_user_path(QString("PRISMLiveStudio/user/%1").arg(PLSLoginUserInfo::getInstance()->getAuthType())));
		bool isSuccess = QFile::remove(pls_get_user_path(QString(CONFIGS_USER_THUMBNAIL_PATH)));
		PLS_INFO("logout", "del user thumbnnail file is %s", isSuccess ? "success" : "failed");
		QDir dir(pls_get_user_path(QString(GIPHY_STICKERS_USER_PATH)));
		if (dir.exists())
			dir.removeRecursively();
		// Delete Prism Sticker recent used json.
		//
		PLSStickerDataHandler::ClearPrismStickerData();
		PLSLoginUserInfo::getInstance()->clearPrismLoginInfo();
		PLSBasic::instance()->ClearService();
		PLSBasic::instance()->removeChzzkSponsorSource();
		PLSCHANNELS_API->clearAll();
		PLSCHANNELS_API->saveData();
		if (main && !pls_is_main_window_closing()) {
			main->mainView->close();
			PLSBasic::instance()->restartApp(RestartAppType::Logout);
		}
	}
	void pls_prism_logout(const QString &urlStr) override
	{
		//hide mainwindow
		if (main) {
			main->EnumDialogs();
			main->SetShowing(false);
		}

		//navershop api
		if (const PLSPlatformNaverShoppingLIVE *platform = PLS_PLATFORM_API->getPlatformNaverShoppingLIVEActive(); platform) {
			PLSNaverShoppingLIVEAPI::logoutNaverShopping(platform, platform->getAccessToken(), platform);
		}

		PLSMotionFileManager::instance()->logoutClear();
		PLS_INFO("logout", "clear prism info success");

		if (!PLSLoginUserInfo::getInstance()->isSelf() && urlStr.contains(PLS_LOGOUT_URL.arg(PRISM_SSL))) {
			PLS_INFO("logout", "pirsm user info from prism cam, no call logout api");
			QString vcam_path = pls_get_user_path(QString(CONFIGS_VIRTUAL_CAMERA_PATH));
			QFile::remove(vcam_path);
			pls_prism_change_over_login_view();
			return;
		}
		QEventLoop loop;
		pls::http::request(pls::http::Request()
					   .method(pls::http::Method::Post)           //
					   .hmacUrl(urlStr, PLS_PC_HMAC_KEY.toUtf8()) //
					   .cookie({{"NEO_SES", PLSLoginUserInfo::getInstance()->getToken()}})
					   .withLog()
					   .jsonContentType()
					   .workInMainThread()
					   .receiver(&loop)
					   .timeout(PRISM_NET_REQUEST_TIMEOUT)
					   .okResult([this, &loop](const pls::http::Reply &) {
						   loop.quit();
						   PLS_INFO("logout", "prism logout/signout api success");
						   QString vcam_path = pls_get_user_path(QString(CONFIGS_VIRTUAL_CAMERA_PATH));
						   QFile::remove(vcam_path);
						   pls_prism_change_over_login_view();
					   })
					   .failResult([this, &loop](const pls::http::Reply &) {
						   loop.quit();
						   PLS_INFO("logout", "prism logout/signout api failed");
						   QString vcam_path = pls_get_user_path(QString(CONFIGS_VIRTUAL_CAMERA_PATH));
						   QFile::remove(vcam_path);
						   pls_prism_change_over_login_view();
					   }));
		loop.exec();
	}
	QString pls_prism_user_thumbnail_path() override
	{
		QString thumbnailPath;
		if (!PLSLoginUserInfo::getInstance()->getprofileThumbanilUrl().isEmpty()) {
			thumbnailPath = pls_get_user_path(CONFIGS_USER_THUMBNAIL_PATH);
		}
		return thumbnailPath;
	}
	Common pls_get_common() override { return PLSGpopData::instance()->getCommon(); }
	QMap<QString, SnsCallbackUrl> pls_get_snscallback_urls() override { return PLSGpopData::instance()->getSnscallbackUrls(); }
	Connection pls_get_connection() override { return PLSGpopData::instance()->getConnection(); }
	QMap<int, RtmpDestination> pls_get_rtmpDestination() override { return PLSSyncServerManager::instance()->getRtmpDestination(); };

	QString pls_get_gcc_data() override { return QString::fromStdString(GlobalVars::gcc); }
	QString pls_get_prism_token() override { return PLSLoginUserInfo::getInstance()->getToken(); }
	QString pls_get_prism_email() override { return PLSLoginUserInfo::getInstance()->getEmail(); }
	QString pls_get_prism_thmbanilurl() override { return PLSLoginUserInfo::getInstance()->getprofileThumbanilUrl(); }
	QString pls_get_prism_nickname() override { return PLSLoginUserInfo::getInstance()->getNickname(); }
	QString pls_get_prism_usercode() override { return PLSLoginUserInfo::getInstance()->getUserCode(); }

	QByteArray pls_get_prism_cookie() override { return PLSLoginUserInfo::getInstance()->getPrismCookie(); }
	QString pls_get_b2b_auth_url() override { return PLSLoginUserInfo::getInstance()->getNCPPlatformServiceAuthUrl(); }
	bool pls_get_b2b_acctoken(const QString &url) override { return PLSLoginDataHandler::instance()->getNCPAccessToken(url); }

	void pls_frontend_add_event_callback(pls_frontend_event_cb callback, void *context) override { eventCallbacks.add(callback, context); }
	void pls_frontend_remove_event_callback(pls_frontend_event_cb callback, void *context) override { eventCallbacks.remove(callback, context); }
	void on_event(pls_frontend_event event, const QVariantList &params) override { eventCallbacks.trigger(event, params); }

	QString pls_get_theme_dir_path() override { return "data/prism-studio/themes/Dark/"; }
	QString pls_get_color_filter_dir_path() override { return pls_get_user_path("PRISMLiveStudio/color_filter/"); }

	void pls_toast_message(pls_toast_info_type type, const QString &message, int auto_close) override
	{
		QMetaObject::invokeMethod(main->mainView, "toastMessage", Q_ARG(pls_toast_info_type, type), Q_ARG(QString, message), Q_ARG(int, auto_close));
	}
	void pls_toast_message(pls_toast_info_type type, const QString &message, const QString &url, const QString &replaceStr, int auto_close) override
	{
		QMetaObject::invokeMethod(main->mainView, "toastMessage", Q_ARG(pls_toast_info_type, type), Q_ARG(QString, message), Q_ARG(QString, url), Q_ARG(QString, replaceStr),
					  Q_ARG(int, auto_close));
	}
	void pls_toast_clear() override { QMetaObject::invokeMethod(main->mainView, "toastClear"); }

	void pls_set_main_view_side_bar_user_button_icon(const QIcon &icon) override { PLSBasic::instance()->getMainView()->setUserButtonIcon(icon); }

	void pls_unload_chat_dock(const QString &objectName) override
	{
		if (chatDocks.contains(objectName)) {
			auto dock = chatDocks[objectName];

			chatDocks.remove(objectName);
		}
	}

	const char *pls_basic_config_get_string(const char *section, const char *name, const char *) override { return config_get_string(main->Config(), section, name); }
	int64_t pls_basic_config_get_int(const char *section, const char *name, int64_t) override { return config_get_int(main->Config(), section, name); }
	uint64_t pls_basic_config_get_uint(const char *section, const char *name, uint64_t) override { return config_get_uint(main->Config(), section, name); }
	bool pls_basic_config_get_bool(const char *section, const char *name, bool) override { return config_get_bool(main->Config(), section, name); }
	double pls_basic_config_get_double(const char *section, const char *name, double) override { return config_get_double(main->Config(), section, name); }

	static QString getUpdateInfoUrl(const QJsonObject &updateInfoUrlList)
	{
		if (!strcmp(App()->GetLocale(), "ko-KR")) {
			return updateInfoUrlList.value(QStringLiteral("kr")).toString();
		} else {
			return updateInfoUrlList.value(QStringLiteral("en")).toString();
		}
	}

	void checkUpdateAppHandle(const QByteArray &data, const QString &verstr, bool &isForceUpdate, QString &version, QString &fileUrl, QString &updateInfoUrl,
				  pls_check_update_result_t &check_update_result)
	{
		return;
	}

	pls_check_update_result_t pls_check_app_update(bool &isForceUpdate, QString &version, QString &fileUrl, QString &updateInfoUrl, PLSErrorHandler::RetData &retData) override
	{

		PLS_INFO(UPDATE_MODULE, "UPDATE STATUS: check update appversion api request start");

		pls_check_update_result_t check_update_result = pls_check_update_result_t::Failed;
		return check_update_result;
	}

	pls_upload_file_result_t pls_upload_contactus_files(PLS_CONTACTUS_QUESTION_TYPE iType, const QString &email, const QString &question, const QList<QFileInfo> files) override
	{

		pls_upload_file_result_t upload_result = pls_upload_file_result_t::Ok;
		QEventLoop eventLoop;
		pls::http::Request request;

		QStringList fileList;
		for (const QFileInfo &fileInfo : files) {
			fileList.append(fileInfo.absoluteFilePath());
		}
		request.form(PARAM_REPLY_ATTACHED_FILES, fileList, true);
		//init the email form data
		request.form(PARAM_REPLY_EMAIL_ADDRESS, email);

		switch (iType) {
		case PLS_CONTACTUS_QUESTION_TYPE::Error:
			request.form(PARAM_QUESTION_TYPE, "ER");
			break;
		case PLS_CONTACTUS_QUESTION_TYPE::Advice:
			request.form(PARAM_QUESTION_TYPE, "FS");
			break;
		case PLS_CONTACTUS_QUESTION_TYPE::Consult:
			request.form(PARAM_QUESTION_TYPE, "UI");
			break;
		default:
			request.form(PARAM_QUESTION_TYPE, "ET");
			break;
		}

		bool isB2B = false;
		bool isIHS = false;
		auto platforms = PLS_PLATFORM_API->getAllPlatforms();
		for (auto platform : platforms) {
			if (isB2B) {
				break;
			}

			switch (platform->getServiceType()) {
			case PLSServiceType::ST_NCB2B:
				isB2B = true;
				break;

			case PLSServiceType::ST_CHZZK:
			case PLSServiceType::ST_NAVER_SHOPPING_LIVE:
			case PLSServiceType::ST_BAND:
			case PLSServiceType::ST_NAVERTV:
				isIHS = true;
				break;

			default:
				break;
			}
		}

		if (isB2B) {
			request.form(PARAM_SERVICE_TYPE, "B2B");
		} else if (isIHS) {
			request.form(PARAM_SERVICE_TYPE, "IHS");
		} else {
			request.form(PARAM_SERVICE_TYPE, "ETC");
		}

		//init the question forom data
		request.form(PARAM_REPLY_QUESTION, question);
		request.method(pls::http::Method::Post);
		request.withLog();
		request.hmacUrl(CONTACT_SEND_EMAIL_URL.arg(PRISM_SSL), PLS_PC_HMAC_KEY.toUtf8());
		request.workInMainThread();
		request.receiver(&eventLoop);

		//eventloop request
		pls::http::request(request.okResult([&eventLoop](const pls::http::Reply &) {
						  PLS_INFO(CONTACT_US_MODULE, "request contact us success");
						  eventLoop.quit();
					  })
					   .failResult([&eventLoop, &upload_result](const pls::http::Reply &reply) {
						   QByteArray root = reply.data();
						   QJsonObject obj;
						   pls_parse_json(obj, root);
						   QVariant codeVariant = pls_find_attr<QVariant>(obj, "code");
						   int code = codeVariant.toInt();
						   upload_result = pls_upload_file_result_t::NetworkError;
						   if (code == 1001) {
							   upload_result = pls_upload_file_result_t::EmailFormatError;
						   } else if (code == 1203) {
							   upload_result = pls_upload_file_result_t::FileFormatError;
						   } else if (code == 1200) {
							   upload_result = pls_upload_file_result_t::AttachUpToMaxFile;
						   }
						   PLS_ERROR(CONTACT_US_MODULE, "contact send email failed, code: %d, statusCode = %d", code, reply.statusCode());
						   eventLoop.quit();
					   }));
		eventLoop.exec();

		return upload_result;
	}

	bool pls_show_update_info_view(bool is_force, const QString &version, const QString &file_url, const QString &update_info_url, bool is_manual, QWidget *parent) override
	{
		PLS_INFO(UPDATE_MODULE, "UPDATE STATUS: show update view");
		PLSUpdateView updateDlg(is_manual, is_force, version, file_url, update_info_url, parent);
		return updateDlg.exec() == PLSUpdateView::Accepted;
	}
	void pls_get_new_notice_Info(const std::function<void(const QVariantMap &noticeInfo)> &noticeCallback) override
	{
		PLSNoticeHandler::getInstance()->getNoticeInfoFromRemote(pls_get_main_view(), PLS_NOTICE_URL.arg(PRISM_SSL), PLS_PC_HMAC_KEY, noticeCallback);
	}
	QString pls_get_win_os_version() override { return getOsVersion(); }

	bool pls_is_living_or_recording() override { return PLS_PLATFORM_API->isGoLiveOrRecording(); }

	bool pls_is_output_actived() override
	{
		auto state = ResolutionGuidePage::getCurrentOutputState();
		return (state != ResolutionGuidePage::OutputState::OuputIsOff) && (state != ResolutionGuidePage::OutputState::NoState);
	};

	// add tools menu seperator
	void pls_add_tools_menu_seperator() override
	{
		if (!main->ui->menuTools->isEmpty()) {
			main->ui->menuTools->addSeparator();
		}
	}
	void pls_start_broadcast(bool toStart = true) override
	{
		if (toStart) {
			PLSCHANNELS_API->toStartBroadcast();
		} else {
			PLSCHANNELS_API->toStopBroadcast();
		}
	}
	void pls_start_broadcast_in_info(bool toStart = true) override
	{

		if (toStart) {
			PLSCHANNELS_API->toStartBroadcastInInfoView();
		} else {
			PLSCHANNELS_API->toStopBroadcast();
		}
	}
	void pls_start_rehearsal(bool toStart = true) override
	{
		if (toStart) {
			PLSCHANNELS_API->toStartRehearsal();
		} else {
			PLSCHANNELS_API->toStopRehearsal();
		}
	}

	void pls_start_record(bool toStart = true) override
	{
		if (toStart) {
			PLSCHANNELS_API->toStartRecord();
		} else {
			PLSCHANNELS_API->toStopRecord();
		}
	}

	ITextMotionTemplateHelper *pls_get_text_motion_template_helper_instance() override { return TextMotionTemplateDataHelper::instance(); }
	ITextMotionTemplateHelper *pls_get_chat_template_helper_instance() override { return PLSChatTemplateDataHelper::instance(); }

	QString pls_get_current_language() override { return QString::fromUtf8(App()->GetLocale()); }

	int pls_get_actived_chat_channel_count() override { return getActivedChatChannelCount(); }
	void pls_get_prism_live_seq(int &seqHorizontal, int &seqVertical) override
	{
		seqHorizontal = PLS_PLATFORM_PRSIM->getVideoSeq(DualOutputType::Horizontal);
		seqVertical = PLS_PLATFORM_PRSIM->getVideoSeq(DualOutputType::Vertical);
	}
	bool pls_is_create_souce_in_loading() override
	{
		if (main) {
			return main->isCreateSouceInLoading;
		}
		return false;
	}

	void pls_network_state_monitor(const std::function<void(bool)> &callback) override { QObject::connect(pls::NetworkState::instance(), &pls::NetworkState::stateChanged, callback); }
	bool pls_get_network_state() override { return pls::NetworkState::instance()->isAvailable(); }

	void pls_show_virtual_background() override {}
	QWidget *pls_create_virtual_background_resource_widget(QWidget *parent, const std::function<void(QWidget *)> &init, bool forProperty, const QString &itemId, bool checkBoxState, bool) override
	{
		auto view = pls_new<PLSMotionImageListView>(parent, forProperty ? PLSMotionViewType::PLSMotionPropertyView : PLSMotionViewType::PLSMotionDetailView, init);
		if (!itemId.isEmpty()) {
			view->switchToSelectItem(itemId);
		}
		view->setCheckState(checkBoxState);
		view->setFilterButtonVisible(forProperty);
		return view;
	}

	QPixmap pls_load_svg(const QString &path, const QSize &size) override { return pls_shared_paint_svg(path, size); }

	void pls_set_bgm_visible(bool visible) override { PLSBasic::instance()->OnSetBgmViewVisible(visible); }

	//PRISM/Xiewei/20210113/#/add apis for stream deck
	bool pls_set_side_window_visible(int key, bool visible) override { return PLSBasic::instance()->getMainView()->setSidebarWindowVisible(key, visible); }
	void pls_mixer_mute_all(bool mute) override { PLSAudioControl::instance()->SetMuteState(mute); }
	bool pls_mixer_is_all_mute() override { return PLSAudioControl::instance()->GetMuteState(); }
	QList<SideWindowInfo> pls_get_side_windows_info() override { return PLSBasic::instance()->getMainView()->getSideWindowInfo(); }
	int pls_get_toast_message_count() override { return PLSBasic::instance()->getMainView()->getToastMessageCount(); }
	QString pls_get_stream_state() override { return PLSBasic::instance()->getStreamState(); }
	QString pls_get_record_state() override { return PLSBasic::instance()->getRecordState(); }
	int pls_get_record_duration() override { return PLSBasic::instance()->getRecordDuration(); }
	bool pls_get_hotkey_enable() override { return App()->HotkeyEnable(); }

	int pls_alert_warning(const char *title, const char *message) override { return PLSAlertView::warning(PLSBasic::Get(), QTStr(title), QTStr(message)); }

	void pls_singleton_wakeup() override { PLSBasic::instance()->singletonWakeup(); }

	uint pls_get_live_start_time() override { return PLSBasic::instance()->getStartTimeStamp(); }

	void pls_navershopping_get_store_login_url(QWidget *widget, const std::function<void(const QString &storeLoginUrl)> &ok, const std::function<void(const QByteArray &)> &fail) override
	{
		PLSNaverShoppingLIVEAPI::getStoreLoginUrl(widget, ok, fail);
	}

	void pls_navershopping_get_error_code_message(const QByteArray &data, QString &errorCode, QString &errorMessage) override
	{
		PLSNaverShoppingLIVEAPI::getErrorCodeOrErrorMessage(data, errorCode, errorMessage);
	}

	void pls_send_analog(AnalogType logType, const QVariantMap &info) override { PLS_PLATFORM_API->sendAnalog(logType, info); }

	void pls_get_scene_source_count(int &sceneCount, int &sourceCount) override
	{
		SceneDisplayVector displayVector = PLSSceneDataMgr::Instance()->GetDisplayVector();
		sceneCount = static_cast<int>(displayVector.size());

		auto enum_proc = [](void *param, obs_source_t *source) {
			if (!source)
				return true;

			auto count_p = static_cast<int *>(param);
			if (count_p) {
				const char *source_name = obs_source_get_name(source);
				PLS_DEBUG(MAIN_FRONTEND_API, "enumerating sources: %s", source_name);
				(*count_p)++;
			}
			return true;
		};
		int param = 0;
		obs_enum_sources(enum_proc, &param);
		sourceCount = param;
	}

	void pls_laboratory_click_open_button(const QString &laboratoryId, bool targetStatus) override
	{

		if (laboratoryId == LABORATORY_NEW_BEAUTY_EFFECT_ID) {
			if (!targetStatus) {
				//TODO
				//PLSBeautyPlugin::AboutToCloseFaceMakerLaboratory(g_laboratoryDialog);
			} else {
				pls_set_laboratory_status(LABORATORY_NEW_BEAUTY_EFFECT_ID, true);
			}
		} else if (laboratoryId == LABORATORY_REMOTECHAT_ID) {
			if (!targetStatus) {
				PLSRemoteChatManage::instance()->closeRemoteChat();
			}
		}
	}

	void pls_set_laboratory_status(const QString &laboratoryId, bool on) override
	{

		if (g_laboratoryDialog) {
			g_laboratoryDialog->changeCheckedState(laboratoryId, on);
		}
	}
	void pls_laboratory_detail_page_js_event(const QString &page, const QString &action, const QString &info) override
	{
		if (page == common::LABORATORY_JS_REMOTE_CHAT_PAGE) {
			if (action == common::LABORATORY_JS_CLICK_ACTION && info == common::LABORATORY_JS_REMOTE_CHAT_INFO) {
				PLSRemoteChatManage::instance()->openRemoteChat();
			}
		} /*
		  else if (page == common::LABORATORY_JS_FACE_MAKER_PAGE) {
			if (action == common::LABORATORY_JS_CLICK_ACTION) {
				PLSBeautyPlugin::OpenFaceMakerView();
			}
		}*/
	}

	bool pls_get_laboratory_status(const QString &laboratoryId) override { return LabManage->getLaboratoryUseState(laboratoryId); }

	QJsonObject pls_get_resource_statistics_data() override
	{
		QJsonObject result;
		//TODO:need nodify

		/*PLSPerf::PerfStats stats = {};
		if (!PLSPerf::PerfCounter::Instance().GetPerfStats(stats)) {
			result.insert("result", "fail");
			result.insert("reason", "GetPerfStats failed");
			return result;
		}

		result.insert("result", "OK");
		QJsonObject data;
		data.insert("CPU", stats.process.cpu.usage);
		data.insert("GPU", stats.process.gpu.gpuUsage);
		data.insert("memory", stats.process.memory.committedMB);
		result.insert("data", data);*/
		return result;
	}

	bool pls_click_alert_message() override
	{
		auto main = dynamic_cast<PLSBasic *>(App()->GetMainWindow());
		main->OnSideBarButtonClicked(ConfigId::LivingMsgView);
		return true;
	}

	bool pls_alert_message_visible() override
	{
		auto main = dynamic_cast<PLSBasic *>(App()->GetMainWindow());
		return main->getMainView()->alert_message_visible();
	}

	int pls_alert_message_count() override
	{
		auto main = dynamic_cast<PLSBasic *>(App()->GetMainWindow());
		return main->getMainView()->alert_message_count();
	}

	QList<std::tuple<QString, QString>> pls_get_user_active_channles_info() override
	{
		QList<std::tuple<QString, QString>> results;

		auto _activiedPlatforms = PLS_PLATFORM_ACTIVIED;
		QList<PLSPlatformBase *> activiedPlatforms(_activiedPlatforms.begin(), _activiedPlatforms.end());
		std::sort(activiedPlatforms.begin(), activiedPlatforms.end(),
			  [](auto p1, auto p2) { return p1->getInitData().value(ChannelData::g_displayOrder).toInt() < p2->getInitData().value(ChannelData::g_displayOrder).toInt(); });
		for (auto platform : activiedPlatforms) {
			QString platformName;

			auto info = platform->getInitData();
			if (auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt(); dataType >= ChannelData::CustomType) {
				platformName = QStringLiteral("Custom RTMP");
			} else if (dataType == ChannelData::ChannelType) {
				platformName = info.value(ChannelData::g_channelName, "").toString();
			}
			const auto sss = __func__;
			auto shareUrl = getInfo(info, ChannelData::g_shareUrlTemp);
			if (shareUrl.isEmpty())
				shareUrl = getInfo(info, ChannelData::g_shareUrl);

			results.append({platformName, shareUrl});
		}

		return results;
	}

	bool pls_is_rehearsal_info_display() override
	{

		auto platformInfos = PLSCHANNELS_API->getCurrentSelectedChannels(ChannelData::ChannelType);
		for (const auto &platformInfo : platformInfos) {
			QString platform = getInfo(platformInfo, ChannelData::g_channelName);
			if ((platform == VLIVE || platform == NAVER_TV || platform == NAVER_SHOPPING_LIVE) && pls_get_stream_state() == "broadcastGo") {
				return true;
			}
		}
		return false;
	}

	QString pls_get_remote_control_mobile_name(const QString &platformName) override { return PLSSyncServerManager::instance()->getRemoteControlMobilePlatform(platformName); }

	bool pls_is_rehearsaling() override { return PLSCHANNELS_API->isRehearsaling(); }

	bool pls_get_chat_info(QString &id, int &seqHorizontal, int &seqVertical, QString &cookie, bool &isSinglePlatform) override
	{
		int count = pls_get_actived_chat_channel_count();
		if (count <= 0)
			return false;
		auto cookieValue = QString::fromUtf8(pls_get_prism_cookie_value());
		if (cookieValue.isEmpty())
			return false;
		pls_get_prism_live_seq(seqHorizontal, seqVertical);
		id = QString::number(seqHorizontal);
		cookie = cookieValue;
		isSinglePlatform = count == 1;
		return true;
	}

	int pls_get_current_selected_channel_count() override { return PLSCHANNELS_API->currentSelectedCount(); }

	void pls_sys_tray_notify(const QString &text, QSystemTrayIcon::MessageIcon n, bool usePrismLogo = true) override { PLSBasic::Get()->SysTrayNotify(text, n); }

	QVector<QString> pls_get_scene_collections() override { return main->GetSceneCollections(); }

	pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, const QString &userId, pls::Buttons buttons,
					    pls::Button defaultButton, const std::optional<int> &timeout, const QMap<QString, QVariant> &properties) override
	{
		return alert_error_message(parent, title, message, errorCode, userId, buttons, defaultButton, timeout, properties);
	}
	pls::Button pls_alert_error_message(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, const QString &userId, const QMap<pls::Button, QString> &buttons,
					    pls::Button defaultButton, const std::optional<int> &timeout, const QMap<QString, QVariant> &properties) override
	{
		return alert_error_message(parent, title, message, errorCode, userId, buttons, defaultButton, timeout, properties);
	}
	template<typename Buttons>
	pls::Button alert_error_message(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, const QString &userId, const Buttons &buttons,
					pls::Button defaultButton, const std::optional<int> &timeout, const QMap<QString, QVariant> &properties) const
	{
		auto contactUsCb = [this](const QString &, const QString &msg, const QString &errCode, const QString &, const QString &time) {
			QString additionalMessage = msg + '\n' + QStringLiteral("SessionID: ") + QString::fromStdString(GlobalVars::prismSession) + '\n' + time;
			pls_async_call_mt(main, [this, additionalMessage]() {
				QMetaObject::invokeMethod(main, "on_actionContactUs_triggered", Qt::QueuedConnection, Q_ARG(QString, additionalMessage), Q_ARG(QString, QString()));
			});
		};
		return PLSAlertView::errorMessage(parent, title, message, errorCode, userId, contactUsCb, buttons, defaultButton, timeout, properties);
	}

	pls::IPropertyModel *pls_get_property_model(obs_source_t *source, obs_data_t *settings, obs_properties_t *props, obs_property_t *prop) override
	{
		if (!pls_current_is_main_thread()) {
			return nullptr;
		}

		switch ((int)obs_property_get_type(prop)) {
		case PLS_PROPERTY_TEMPLATE_LIST:
			return pls_get_template_list_property_model(source, settings, props, prop);
		default:
			break;
		}
		return nullptr;
	}
	pls::IPropertyModel *pls_get_template_list_property_model(const obs_source_t *source, obs_data_t *settings, obs_properties_t *props, obs_property_t *prop) const
	{
		auto id = obs_source_get_id(source);
		if (pls_is_empty(id)) {
			return nullptr;
		} else if (pls_is_equal(id, PRISM_VIEWER_COUNT_SOURCE_ID)) {
			return PLSViewerCountTemplateListPropertyModel::instance();
		}
		return nullptr;
	}
	void pls_template_button_refresh_gif_geometry(QAbstractButton *) const override {}
	config_t *pls_get_global_cookie_config(void) const override { return PLSApp::plsApp()->CookieConfig(); }
	QWidget *pls_get_banner_widget() const override { return PLSLaunchWizardView::instance(); }
	void pls_open_cam_studio(QStringList arguments, QWidget *parent) const override
	{
		QMetaObject::invokeMethod(main, "OnCamStudioClicked", Q_ARG(QStringList, arguments), Q_ARG(QWidget *, parent));
	}
	void pls_show_cam_studio_uninstall(QWidget *parent, QString title, QString content, QString okTip, QString cancelTip)
	{
		QMetaObject::invokeMethod(main, "ShowInstallCamStudioTips", Q_ARG(QWidget *, parent), Q_ARG(QString, title), Q_ARG(QString, content), Q_ARG(QString, okTip), Q_ARG(QString, cancelTip));
	}

	bool pls_is_install_cam_studio(QString &program) const override { return PLSBasic::instance()->CheckCamStudioInstalled(program); }

	const char *pls_source_get_display_name(const char *id)
	{
		if (pls_is_empty(id)) {
			return nullptr;
		}

		const char *displayName = obs_source_get_display_name(id);
		if (pls_is_equal(PRISM_LENS_SOURCE_ID, id) || pls_is_equal(PRISM_LENS_MOBILE_SOURCE_ID, id)) {
			return Str(displayName);
		}
		return displayName;
	}
	QVariantMap pls_http_request_head(bool hasGacc)
	{
		QVariantMap headMap;
		httpRequestHead(headMap, hasGacc);
		return headMap;
	}
	bool pls_install_scene_template(const SceneTemplateItem &item)
	{
		if (!main) {
			return false;
		}
		return main->importSceneTemplate(make_tuple(item.itemId(), item.title(), item.resourcePath(), item.versionLimit(), item.width(), item.height()));
	}

	bool pls_get_output_stream_dealy_active() override
	{
		if (main->outputHandler) {
			return main->outputHandler->delayActive;
		}
		return false;
	}

	QStringList getChannelWithChatList(bool bAddNCPPrefix) override { return getChatChannelNameList(bAddNCPPrefix); }

	bool pls_is_ncp(QString &channlName) override { return channlName == PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName(); }

	bool pls_is_ncp_first_login(QString &serviceName) override
	{
		auto ncpService = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
		if (!GlobalVars::isLogined && !ncpService.isEmpty()) {
			serviceName = ncpService;
			return true;
		}
		return false;
	};

	QString get_channel_cookie_path(const QString &channelLoginName) override { return PLSBasic::cookiePath(channelLoginName); }

	bool pls_is_chzzk_checked(bool forHorizontal)
	{
		auto bDualOutputOn = pls_is_dual_output_on();

		if (!bDualOutputOn && !forHorizontal) {
			return false;
		}

		auto platforms = PLS_PLATFORM_ACTIVIED;
		return any_of(platforms.begin(), platforms.end(), [=](PLSPlatformBase *platform) {
			return (PLSServiceType::ST_CHZZK == platform->getServiceType() || platform->getChannelName() == CHZZK) && (!bDualOutputOn || platform->isHorizontalOutput() == forHorizontal);
		});
	}

	obs_output_t *pls_frontend_get_streaming_output_v(void)
	{
		if (pls_is_dual_output_on()) {
			std::lock_guard locker(obsOutputSet_v.mutex);
			if (obsOutputSet_v.streamOutput) {
				OBSOutput output = obsOutputSet_v.streamOutput;
				return obs_output_get_ref(output);
			}
		}
		return NULL;
	}
};

pls_frontend_callbacks *InitializeAPIInterface(OBSBasic *main)
{
	pls_frontend_callbacks *api = new OBSStudioAPI(main);
	pls_frontend_set_callbacks_internal(api);
	return api;
}
