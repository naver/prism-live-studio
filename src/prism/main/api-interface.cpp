#include <frontend-internal.hpp>
#include <util/windows/win-version.h>

#include "PLSNetworkMonitor.h"
#include "pls-app.hpp"
#include "qt-wrappers.hpp"
#include "window-basic-main.hpp"
#include "window-basic-main-outputs.hpp"
#include "browser-view.hpp"

#include "channel-login-view.hpp"
#include "login-web-handler.hpp"
#include "network-environment.hpp"
#include "main-view.hpp"
#include <functional>
#include "window-dock-browser.hpp"
#include "pls-common-define.hpp"
#include "pls-gpop-data.hpp"
#include "pls-notice-handler.hpp"
#include "notice-view.hpp"
#include "PLSSceneDataMgr.h"
#include "window-dock-browser.hpp"
#include "PLSPlatformApi.h"
#include "PLSChannelDataAPI.h"
#include "TextMotionTemplateDataHelper.h"
#include "PLSPlatformApi/prism/PLSPlatformPrism.h"
#include "PLSMotionImageListView.h"
#include "PLSStickerDataHandler.h"
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QPointer>
using namespace std;

Q_DECLARE_METATYPE(OBSScene);
Q_DECLARE_METATYPE(OBSSource);

template<typename T> static T GetPLSRef(QListWidgetItem *item)
{
	return item->data(static_cast<int>(QtDataRole::PLSRef)).value<T>();
}

void EnumProfiles(function<bool(const char *, const char *)> &&cb);
void EnumSceneCollections(function<bool(const char *, const char *)> &&cb);

pls_check_update_result_t checkUpdate(QString &gcc, bool &isForceUpdate, QString &version, QString &fileUrl, QString &updateInfoUrl);
bool checkLastestVersion(QString &update_info_url);
pls_upload_file_result_t upload_contactus_files(const QString &email, const QString &question, const QList<QFileInfo> files);
bool showUpdateView(bool manualUpdate, bool isForceUpdate, const QString &version, const QString &fileUrl, const QString &updateInfoUrl, QWidget *parent);
bool downloadUpdate(QString &localFilePath, const QString &fileUrl, PLSCancel &cancel, const std::function<void(qint64 download_bytes, qint64 total_bytes)> &progress);
bool installUpdate(const QString &filePath);
void restartApp();
int getActivedChatChannelCount();

extern volatile bool streaming_active;
extern volatile bool recording_active;
extern volatile bool recording_paused;
extern volatile bool replaybuf_active;
extern volatile bool g_mainViewValid;

/* ------------------------------------------------------------------------- */

template<typename T> struct PLSStudioCallback {
	T callback;
	void *private_data;

	inline PLSStudioCallback(T cb, void *p) : callback(cb), private_data(p) {}
};

template<typename T> inline size_t GetCallbackIdx(vector<PLSStudioCallback<T>> &callbacks, T callback, void *private_data)
{
	for (size_t i = 0; i < callbacks.size(); i++) {
		PLSStudioCallback<T> curCB = callbacks[i];
		if (curCB.callback == callback && curCB.private_data == private_data)
			return i;
	}

	return (size_t)-1;
}

class BrowserDock;
struct PLSStudioAPI : pls_frontend_callbacks {
	QPointer<PLSBasic> main;
	vector<PLSStudioCallback<obs_frontend_event_cb>> callbacks;
	vector<PLSStudioCallback<obs_frontend_save_cb>> saveCallbacks;
	vector<PLSStudioCallback<obs_frontend_save_cb>> preloadCallbacks;
	QList<PLSLoginInfo *> loginInfos;
	QList<std::tuple<QList<pls_frontend_event>, pls_frontend_event_cb, void *>> eventCallbacks;
	QMap<QString, QSharedPointer<BrowserDock>> chatDocks;

	explicit inline PLSStudioAPI(PLSBasic *main_) : main(main_) {}

	void *obs_frontend_get_main_window(void) override { return (void *)main; }

	void *obs_frontend_get_main_window_handle(void) override { return (void *)main->winId(); }

	void *obs_frontend_get_system_tray(void) override { return (void *)main->trayIcon.data(); }

	void obs_frontend_get_scenes(struct obs_frontend_source_list *sources) override
	{
		SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
		for (auto iter = data.begin(); iter != data.end(); ++iter) {
			PLSSceneItemView *item = iter->second;
			OBSScene scene = item->GetData();
			obs_source_t *source = obs_scene_get_source(scene);

			obs_source_addref(source);
			da_push_back(sources->sources, &source);
		}
	}

	obs_source_t *obs_frontend_get_current_scene(void) override
	{
		OBSSource source;

		if (main->IsPreviewProgramMode()) {
			source = obs_weak_source_get_source(main->programScene);
		} else {
			source = main->GetCurrentSceneSource();
			obs_source_addref(source);
		}
		return source;
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
		QComboBox *box = main->GetTransitionCombobox();
		for (int i = 0; i < box->count(); i++) {
			OBSSource tr = box->itemData(i).value<OBSSource>();

			obs_source_addref(tr);
			da_push_back(sources->sources, &tr);
		}
	}

	obs_source_t *obs_frontend_get_current_transition(void) override
	{
		OBSSource tr = main->GetCurrentTransition();

		obs_source_addref(tr);
		return tr;
	}

	void obs_frontend_set_current_transition(obs_source_t *transition) override { QMetaObject::invokeMethod(main, "SetTransition", Q_ARG(OBSSource, OBSSource(transition))); }

	int obs_frontend_get_transition_duration(void) override { return main->GetTransitionDuration(); }

	void obs_frontend_set_transition_duration(int duration) override { QMetaObject::invokeMethod(main->GetTransitionDurationSpinBox(), "setValue", Q_ARG(int, duration)); }

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

	void obs_frontend_set_current_scene_collection(const char *collection) override
	{
		QList<QAction *> menuActions = main->ui->sceneCollectionMenu->actions();
		QString qstrCollection = QT_UTF8(collection);

		for (int i = 0; i < menuActions.count(); i++) {
			QAction *action = menuActions[i];
			QVariant v = action->property("file_name");

			if (v.typeName() != nullptr) {
				if (action->text() == qstrCollection) {
					auto menu = action->menu();
					auto actions = menu->actions();
					if (actions.size() > 0 && actions[0]) {
						actions[0]->triggered();
						break;
					}
				}
			}
		}
	}

	bool obs_frontend_add_scene_collection(const char *name) override
	{
		bool success = false;
		QMetaObject::invokeMethod(main, "AddSceneCollection", WaitConnection(), Q_RETURN_ARG(bool, success), Q_ARG(bool, true), Q_ARG(QString, QT_UTF8(name)), Q_ARG(QString, QT_UTF8("")),
					  Q_ARG(QString, QT_UTF8("")));
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

	void obs_frontend_set_current_profile(const char *profile) override
	{
		QList<QAction *> menuActions = main->ui->profileMenu->actions();
		QString qstrProfile = QT_UTF8(profile);

		for (int i = 0; i < menuActions.count(); i++) {
			QAction *action = menuActions[i];
			QVariant v = action->property("file_name");

			if (v.typeName() != nullptr) {
				if (action->text() == qstrProfile) {
					auto menu = action->menu();
					auto actions = menu->actions();
					if (actions.size() > 0 && actions[0]) {
						actions[0]->triggered();
						break;
					}
				}
			}
		}
	}

	void obs_frontend_streaming_start(void) override { QMetaObject::invokeMethod(main, "StartStreaming"); }

	void obs_frontend_streaming_stop(void) override { QMetaObject::invokeMethod(main, "StopStreaming"); }

	bool obs_frontend_streaming_active(void) override { return os_atomic_load_bool(&streaming_active); }

	void obs_frontend_recording_start(void) override { QMetaObject::invokeMethod(main, "StartRecording"); }

	void obs_frontend_recording_stop(void) override { QMetaObject::invokeMethod(main, "StopRecording"); }

	bool obs_frontend_recording_active(void) override { return os_atomic_load_bool(&recording_active); }

	void obs_frontend_recording_pause(bool pause) override { QMetaObject::invokeMethod(main, pause ? "PauseRecording" : "UnpauseRecording"); }

	bool obs_frontend_recording_paused(void) override { return os_atomic_load_bool(&recording_paused); }

	void obs_frontend_replay_buffer_start(void) override { QMetaObject::invokeMethod(main, "StartReplayBuffer"); }

	void obs_frontend_replay_buffer_save(void) override { QMetaObject::invokeMethod(main, "ReplayBufferSave"); }

	void obs_frontend_replay_buffer_stop(void) override { QMetaObject::invokeMethod(main, "StopReplayBuffer"); }

	bool obs_frontend_replay_buffer_active(void) override { return os_atomic_load_bool(&replaybuf_active); }

	void set_third_party_params_to_action(QAction *action, const char *name)
	{
		if (!action || !name) {
			return;
		}

		bool isThirdPartyPlugins = !obs_is_internal_loading_module();
		if (isThirdPartyPlugins) {
			obs_add_external_module_dll_info(obs_get_external_loading_module_dll_name(), "null", name);
		}
		action->setProperty(IS_THIRD_PARTY_PLUGINS, isThirdPartyPlugins);
	}

	void *obs_frontend_add_tools_menu_qaction(const char *name) override
	{
		main->ui->menuTools->setEnabled(true);

		QAction *action = main->ui->menuTools->addAction(QT_UTF8(name));
		set_third_party_params_to_action(action, name);

		return (void *)action;
	}

	void obs_frontend_add_tools_menu_item(const char *name, obs_frontend_cb callback, void *private_data) override
	{
		main->ui->menuTools->setEnabled(true);

		auto func = [private_data, callback]() { callback(private_data); };

		QAction *action = main->ui->menuTools->addAction(QT_UTF8(name));
		set_third_party_params_to_action(action, name);

		QObject::connect(action, &QAction::triggered, func);
	}

	void *obs_frontend_add_dock(void *dock) override { return (void *)main->AddDockWidget((QDockWidget *)dock); }

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
		if (!main->outputHandler) {
			return nullptr;
		}
		OBSOutput output = main->outputHandler->streamOutput;
		obs_output_addref(output);
		return output;
	}

	obs_output_t *obs_frontend_get_recording_output(void) override
	{
		if (!main->outputHandler) {
			return nullptr;
		}
		OBSOutput out = main->outputHandler->fileOutput;
		obs_output_addref(out);
		return out;
	}

	obs_output_t *obs_frontend_get_replay_buffer_output(void) override
	{
		if (!main->outputHandler) {
			return nullptr;
		}
		OBSOutput out = main->outputHandler->replayBuffer;
		obs_output_addref(out);
		return out;
	}

	config_t *obs_frontend_get_profile_config(void) override { return main->basicConfig; }

	config_t *obs_frontend_get_global_config(void) override { return App()->GlobalConfig(); }

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
		OBSSource source = nullptr;

		if (main->IsPreviewProgramMode()) {
			source = main->GetCurrentSceneSource();
			obs_source_addref(source);
		}

		return source;
	}

	void obs_frontend_set_current_preview_scene(obs_source_t *scene) override
	{
		if (main->IsPreviewProgramMode()) {
			QMetaObject::invokeMethod(main, "SetCurrentScene", Q_ARG(OBSSource, OBSSource(scene)), Q_ARG(bool, false));
		}
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
		if (main->disableSaving)
			return;

		for (size_t i = callbacks.size(); i > 0; i--) {
			auto cb = callbacks[i - 1];
			cb.callback(event, cb.private_data);
		}
	}

	bool pls_register_login_info(PLSLoginInfo *login_info)
	{
		loginInfos.append(login_info);
		return true;
	}

	void pls_unregister_login_info(PLSLoginInfo *login_info) { loginInfos.removeOne(login_info); }

	int pls_get_login_info_count() { return loginInfos.size(); }

	PLSLoginInfo *pls_get_login_info(int index)
	{
		if ((index >= 0) && (index < loginInfos.size())) {
			return loginInfos.at(index);
		}
		return nullptr;
	}

	void del_pannel_cookies(const QString &pannelName) { PLSBasic::delPannelCookies(pannelName); }

	void pls_del_specific_url_cookie(const QString &url, const QString &cookieName) { main->delSpecificUrlCookie(url, cookieName); }

	QJsonObject pls_ssmap_to_json(const QMap<QString, QString> &ssmap)
	{
		QJsonObject result;
		for (auto iter = ssmap.begin(), endIter = ssmap.end(); iter != endIter; ++iter) {
			result.insert(iter.key(), iter.value());
		}
		return result;
	}

	bool pls_browser_view(QJsonObject &result, const QUrl &url, pls_result_checking_callback_t callback, QWidget *parent)
	{
		PLSBrowserView bv(&result, url, callback, parent);
		bv.update();
		return bv.exec() == QDialog::Accepted;
	}

	bool pls_browser_view(QJsonObject &result, const QUrl &url, const std::map<std::string, std::string> &headers, const QString &pannelName, const std::string &script,
			      pls_result_checking_callback_t callback, QWidget *parent)
	{
		PLSBrowserView bv(&result, url, headers, pannelName, script, callback, parent);
		// 2020.08.03 cheng.bing
		//disable hotkey,able hotkey

		HotKeyLocker locker;
		bool isOk = (bv.exec() == QDialog::Accepted);
		return isOk;
	}

	bool pls_rtmp_view(QJsonObject &, PLSLoginInfo *, QWidget *)
	{
		/*PLSRtmpChannelView rclv(login_info, result, parent);
		return rclv.exec() == QDialog::Accepted;*/
		return false;
	}

	bool pls_channel_login(QJsonObject &result, QWidget *parent)
	{
		PLSChannelLoginView clv(result, parent);
		return clv.exec() == QDialog::Accepted;
	}
	bool pls_sns_user_info(QJsonObject &result, const QList<QNetworkCookie> &cookies, const QString &urlStr)
	{
		LoginWebHandler webHandler(result);
		auto _url = urlStr;
		webHandler.initialize(cookies, _url.replace("/prism/", "/prism_pc/"), LoginInfoType::PrismLoginInfo);
		webHandler.requesetHttp();

		return webHandler.isSuccessLogin();
	}
	void google_get_cookie(const QString &token, std::function<void(bool ok, const QJsonObject &)> callback, QObject *receiver) { return; }
	void google_regeist_handler(QNetworkReply *reply, const QString &token, std::function<void(bool ok, const QJsonObject &)> callback, QObject *receiver)
	{
		PLSHmacNetworkReplyBuilder builder(PLS_GOOGLE_LOGIN_URL_TOKEN.arg(PRISM_SSL));
		builder.setCookie(reply->header(QNetworkRequest::SetCookieHeader));
		QJsonObject object;
		object["accessToken"] = token;
		object["snsCd"] = "google";
		object["agreement"] = "true";
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
		builder.setJsonObject(object);

		auto handleSuccess = [=](QNetworkReply *networkReply, int, QByteArray data, void *) {
			auto cookie = networkReply->header(QNetworkRequest::SetCookieHeader);
			PLSLoginUserInfo::getInstance()->setUserLoginInfo(data);
			PLSLoginUserInfo::getInstance()->setPrismCookieConfigInfo(cookie.value<QList<QNetworkCookie>>());
			callback(true, QJsonObject());
		};
		auto handleFail = [=](QNetworkReply *, int, QByteArray data, QNetworkReply::NetworkError, void *) { callback(false, QJsonObject()); };
		PLS_HTTP_HELPER->connectFinished(builder.post(), receiver, handleSuccess, handleFail);
	}
	bool pls_google_user_info(std::function<void(bool ok, const QJsonObject &)> callback, const QString &redirect_uri, const QString &code, QObject *receiver)
	{
		if (PLSLoginUserInfo::getInstance()->isPrismLogined()) {
			callback(false, QJsonObject());
			return false;
		}

		PLSNetworkReplyBuilder builder(YOUTUBE_API_TOKEN);
		builder.setQuerys({{HTTP_CODE, code},
				   {HTTP_CLIENT_ID, YOUTUBE_CLIENT_ID},
				   {HTTP_CLIENT_SECRET, YOUTUBE_CLIENT_KEY},
				   {HTTP_REDIRECT_URI, redirect_uri},
				   {HTTP_GRANT_TYPE, HTTP_AUTHORIZATION_CODE}});

		auto handleSuccess = [=](QNetworkReply *, int, QByteArray data, void *) { google_get_cookie(QJsonDocument::fromJson(data).object()["access_token"].toString(), callback, receiver); };

		auto handleFail = [=](QNetworkReply *, int, QByteArray data, QNetworkReply::NetworkError, void *) {
			auto obj = QJsonDocument::fromJson(data).object();
			QString errorInfo = QString("code = %1; prism login error;message:%2").arg(obj["code"].toInt()).arg(obj["error"].toString());
			PLS_ERROR(PLS_LOGIN_MODULE, errorInfo.toUtf8().data());
			PLSAlertView::warning(0, QTStr("Alert.Title"), QTStr("prism.login.default.error"));
			callback(false, QJsonObject());
		};
		PLS_HTTP_HELPER->connectFinished(builder.post(), receiver, handleSuccess, handleFail);
		return true;
	}
	bool pls_network_environment_reachable()
	{
		NetworkEnvironment environment;
		return environment.getNetWorkEnvironment();
	}

	bool pls_prism_token_is_vaild(const QString &urlStr)
	{
		QJsonObject result;
		LoginWebHandler webHandler(result);
		QVariantMap tokenInfos;
		tokenInfos.insert(COOKIE_NEO_SES, PLSLoginUserInfo::getInstance()->getToken());
		webHandler.initialize(pls_get_cookies(QJsonObject::fromVariantMap(tokenInfos)), urlStr, LoginInfoType::TokenInfo);
		webHandler.requesetHttp();
		bool isSuccess = webHandler.isSuccessLogin();
		return isSuccess;
	}
	void pls_prism_change_over_login_view()
	{
		//删头像
		QFile::remove(pls_get_user_path(QString(CONFIGS_USER_THUMBNAIL_PATH).arg(PLSLoginUserInfo::getInstance()->getAuthType())));
		QDir dir(pls_get_user_path(QString(GIPHY_STICKERS_USER_PATH)));
		if (dir.exists())
			dir.removeRecursively();
		// Delete Prism Sticker recent used json.
		PLSStickerDataHandler::ClearPrismStickerData();
		PLSLoginUserInfo::getInstance()->clearPrismLoginInfo();
		if (g_mainViewValid && main && !main->getMainView()->isClosing()) {
			main->getMainView()->close();
			restartApp();
		}
	}
	void pls_prism_logout(const QString &urlStr, LoginInfoType infoType)
	{
		//navershop api
		PLSPlatformNaverShoppingLIVE *platform = PLS_PLATFORM_API->getPlatformNaverShoppingLIVEActive();
		if (platform) {
			PLSNaverShoppingLIVEAPI::logoutNaverShopping(platform, platform->getAccessToken(), platform);
		}
		LoginWebHandler webHandler;
		QVariantMap tokenInfos;
		tokenInfos.insert(COOKIE_NEO_SES, PLSLoginUserInfo::getInstance()->getToken());
		webHandler.initialize(pls_get_cookies(QJsonObject::fromVariantMap(tokenInfos)), urlStr, infoType);
		webHandler.requesetHttp(QNetworkAccessManager::PostOperation);
		if (infoType == LoginInfoType::PrismSignoutInfo || infoType == LoginInfoType::PrismLogoutInfo) {

			pls_prism_change_over_login_view();
		}
	}
	QString pls_prism_user_thumbnail_path()
	{
		QString thumbnailPath;
		if (!PLSLoginUserInfo::getInstance()->getAuthType().isEmpty()) {
			thumbnailPath = pls_get_user_path(QString(CONFIGS_USER_THUMBNAIL_PATH).arg(PLSLoginUserInfo::getInstance()->getAuthType()));
		}
		return thumbnailPath;
	}
	Common pls_get_common() { return PLSGpopData::instance()->getCommon(); }
	VliveNotice pls_get_vlive_notice() { return PLSGpopData::instance()->getVliveNotice(); }
	QMap<QString, SnsCallbackUrl> pls_get_snscallback_urls() { return PLSGpopData::instance()->getSnscallbackUrls(); }
	Connection pls_get_connection() { return PLSGpopData::instance()->getConnection(); }
	QMap<int, RtmpDestination> pls_get_rtmpDestination() { return PLSGpopData::instance()->getRtmpDestination(); };

	QString pls_get_gcc_data() { return ""; }
	QString pls_get_prism_token() { return PLSLoginUserInfo::getInstance()->getToken(); }
	QString pls_get_prism_email() { return PLSLoginUserInfo::getInstance()->getEmail(); }
	QString pls_get_prism_thmbanilurl() { return PLSLoginUserInfo::getInstance()->getprofileThumbanilUrl(); }
	QString pls_get_prism_nickname() { return PLSLoginUserInfo::getInstance()->getNickname(); }
	QString pls_get_prism_usercode() { return PLSLoginUserInfo::getInstance()->getUserCode(); }

	QWidget *pls_get_main_view() { return main ? main->getMainView() : nullptr; }
	QWidget *pls_get_toplevel_view(QWidget *widget)
	{
		if (!widget) {
			return pls_get_main_view();
		} else if (dynamic_cast<PLSMainView *>(widget) || dynamic_cast<PLSDialogView *>(widget) || dynamic_cast<PLSBrowserView *>(widget)) {
			return widget;
		} else if (QDockWidget *dock = dynamic_cast<QDockWidget *>(widget); dock && dock->isFloating()) {
			return dock;
		}

		while (widget = widget->parentWidget()) {
			if (dynamic_cast<PLSMainView *>(widget) || dynamic_cast<PLSDialogView *>(widget) || dynamic_cast<PLSBrowserView *>(widget)) {
				return widget;
			} else if (QDockWidget *dock = dynamic_cast<QDockWidget *>(widget); dock && dock->isFloating()) {
				return dock;
			}
		}
		return pls_get_main_view();
	}

	QByteArray pls_get_prism_cookie() { return PLSLoginUserInfo::getInstance()->getPrismCookie(); }

	void pls_frontend_add_event_callback(pls_frontend_event_cb callback, void *context) { eventCallbacks.append(make_tuple(QList<pls_frontend_event>{}, callback, context)); }
	void pls_frontend_add_event_callback(pls_frontend_event event, pls_frontend_event_cb callback, void *context)
	{
		eventCallbacks.append(make_tuple(QList<pls_frontend_event>{event}, callback, context));
	}
	void pls_frontend_add_event_callback(QList<pls_frontend_event> events, pls_frontend_event_cb callback, void *context) { eventCallbacks.append(make_tuple(events, callback, context)); }
	void pls_frontend_remove_event_callback(pls_frontend_event_cb callback, void *context) { eventCallbacks.removeOne(make_tuple(QList<pls_frontend_event>{}, callback, context)); }
	void pls_frontend_remove_event_callback(pls_frontend_event event, pls_frontend_event_cb callback, void *context)
	{
		eventCallbacks.removeOne(make_tuple(QList<pls_frontend_event>{event}, callback, context));
	}
	void pls_frontend_remove_event_callback(QList<pls_frontend_event> events, pls_frontend_event_cb callback, void *context) { eventCallbacks.removeOne(make_tuple(events, callback, context)); }
	void on_event(pls_frontend_event event, const QVariantList &params)
	{
		for (auto iter = eventCallbacks.begin(); iter != eventCallbacks.end(); ++iter) {
			if (std::get<0>(*iter).isEmpty() || std::get<0>(*iter).contains(event)) {
				std::get<1> (*iter)(event, params, std::get<2>(*iter));
			}
		}
	}

	QString pls_get_theme_dir_path() { return "data/prism-studio/themes/Dark/"; }
	QString pls_get_color_filter_dir_path() { return pls_get_user_path("PRISMLiveStudio/color_filter/"); }

	void pls_toast_message(pls_toast_info_type type, const QString &message, int auto_close)
	{
		QMetaObject::invokeMethod(main, "toastMessage", Q_ARG(pls_toast_info_type, type), Q_ARG(QString, message), Q_ARG(int, auto_close));
	}
	void pls_toast_message(pls_toast_info_type type, const QString &message, const QString &url, const QString &replaceStr, int auto_close)
	{
		QMetaObject::invokeMethod(main, "toastMessage", Q_ARG(pls_toast_info_type, type), Q_ARG(QString, message), Q_ARG(QString, url), Q_ARG(QString, replaceStr), Q_ARG(int, auto_close));
	}
	void pls_toast_clear() { QMetaObject::invokeMethod(main, "toastClear"); }

	void pls_set_main_view_side_bar_user_button_icon(const QIcon &icon) { main->getMainView()->setUserButtonIcon(icon); }

	void pls_load_chat_dock(const QString &objectName, const std::string &url, const std::string &white_popup_url, const std::string &startup_script)
	{
		if (!cef) {
			return;
		}

		PLSBasic::InitBrowserPanelSafeBlock();

		QSharedPointer<BrowserDock> chat;
		if (chatDocks.contains(objectName)) {
			chat = chatDocks[objectName];
		} else {
			chat.reset(new BrowserDock());
			chatDocks.insert(objectName, chat);
		}
		chat->setObjectName(objectName);
		chat->resize(300, 600);
		chat->setMinimumSize(200, 300);
		chat->setWindowTitle(objectName);
		chat->setAllowedAreas(Qt::AllDockWidgetAreas);

		QCefWidget *browser = cef->create_widget(nullptr, url, startup_script, panel_cookies);
		chat->SetWidget(browser);
		if (!white_popup_url.empty()) {
			cef->add_force_popup_url(white_popup_url, chat.data());
		}

		main->addDockWidget(Qt::RightDockWidgetArea, chat.data());
		chat->setVisible(true);
	}

	void pls_unload_chat_dock(const QString &objectName)
	{
		if (chatDocks.contains(objectName)) {
			auto dock = chatDocks[objectName];

			chatDocks.remove(objectName);
		}
	}

	const char *pls_basic_config_get_string(const char *section, const char *name, const char *) { return config_get_string(main->Config(), section, name); }
	int64_t pls_basic_config_get_int(const char *section, const char *name, int64_t) { return config_get_int(main->Config(), section, name); }
	uint64_t pls_basic_config_get_uint(const char *section, const char *name, uint64_t) { return config_get_uint(main->Config(), section, name); }
	bool pls_basic_config_get_bool(const char *section, const char *name, bool) { return config_get_bool(main->Config(), section, name); }
	double pls_basic_config_get_double(const char *section, const char *name, double) { return config_get_double(main->Config(), section, name); }

	pls_check_update_result_t pls_check_update(QString &gcc, bool &is_force, QString &version, QString &file_url, QString &update_info_url)
	{
		return checkUpdate(gcc, is_force, version, file_url, update_info_url);
	}

	bool pls_check_lastest_version(QString &update_info_url) { return checkLastestVersion(update_info_url); }
	pls_upload_file_result_t pls_upload_contactus_files(const QString &email, const QString &question, const QList<QFileInfo> files) { return upload_contactus_files(email, question, files); }
	bool pls_show_update_info_view(bool is_force, const QString &version, const QString &file_url, const QString &update_info_url, bool is_manual, QWidget *parent)
	{
		return showUpdateView(is_manual, is_force, version, file_url, update_info_url, parent);
	}
	bool pls_download_update(QString &local_file_path, const QString &file_url, PLSCancel &cancel, const std::function<void(qint64 download_bytes, qint64 total_bytes)> &progress)
	{
		return downloadUpdate(local_file_path, file_url, cancel, progress);
	}
	bool pls_install_update(const QString &file_path) { return installUpdate(file_path); }

	QVariantMap pls_get_new_notice_Info()
	{
		LoginWebHandler loginWebHandler;
		loginWebHandler.initialize(PLS_NOTICE_URL.arg(PRISM_SSL), LoginInfoType::PrismNoticeInfo);
		loginWebHandler.requesetHttpForNotice(QNetworkAccessManager::GetOperation);
		if (loginWebHandler.isSuccessLogin()) {
			return PLSNoticeHandler::getInstance()->getNewNotice();
		} else {
			return QVariantMap();
		}
	}
	QString pls_get_win_os_version()
	{
		win_version_info wvi;
		get_win_ver(&wvi);
		return QString("Windows %1.%2.%3.%4").arg(wvi.major).arg(wvi.minor).arg(wvi.build).arg(wvi.revis);
	}

	bool pls_is_living_or_recording() { return PLS_PLATFORM_API->isGoLiveOrRecording(); }

	// add tools menu seperator
	void pls_add_tools_menu_seperator()
	{
		if (!main->ui->menuTools->isEmpty()) {
			main->ui->menuTools->addSeparator();
		}
	}
	void pls_start_broadcast(bool toStart = true)
	{
		if (toStart) {
			PLSCHANNELS_API->toStartBroadcast();
		} else {
			PLSCHANNELS_API->toStopBroadcast();
		}
	}

	void pls_start_record(bool toStart = true)
	{
		if (toStart) {
			PLSCHANNELS_API->toStartRecord();
		} else {
			PLSCHANNELS_API->toStopRecord();
		}
	}

	ITextMotionTemplateHelper *pls_get_text_motion_template_helper_instance() override { return TextMotionTemplateDataHelper::instance(); }

	virtual QString pls_get_current_language() override { return QString::fromUtf8(App()->GetLocale()); }

	virtual int pls_get_actived_chat_channel_count() override { return getActivedChatChannelCount(); }
	virtual int pls_get_prism_live_seq() override { return PLS_PLATFORM_PRSIM->getVideoSeq(); }
	virtual bool pls_is_create_souce_in_loading() override
	{
		if (main) {
			return main->isCreateSouceInLoading;
		}
		return false;
	}

	virtual void pls_network_state_monitor(std::function<void(bool)> &&callback) override { QObject::connect(PLSNetworkMonitor::Instance(), &PLSNetworkMonitor::OnNetWorkStateChanged, callback); }
	virtual bool pls_get_network_state() override { return PLSNetworkMonitor::Instance()->IsInternetAvailable(); }

	virtual void pls_show_virtual_background() override { main->OnVirtualBgClicked(true); }
	virtual QWidget *pls_create_virtual_background_resource_widget(QWidget *parent, std::function<void(QWidget *)> &&init, bool forProperty, const QString &itemId, bool checkBoxState,
								       bool) override
	{
		auto view = new PLSMotionImageListView(parent, forProperty ? PLSMotionViewType::PLSMotionPropertyView : PLSMotionViewType::PLSMotionDetailView, init);
		if (!itemId.isEmpty()) {
			view->switchToSelectItem(itemId);
		}
		view->setCheckState(checkBoxState);
		view->setFilterButtonVisible(forProperty);
		return view;
	}

	QPixmap pls_load_svg(const QString &path, const QSize &size)
	{
		QPixmap paintSvg(const QString &pixmapPath, const QSize &pixSize);
		return paintSvg(path, size);
	}

	void pls_show_mobile_source_help() { main->mainView->wifiHelperclicked(); }

	pls_blacklist_type pls_is_blacklist(QString value, pls_blacklist_type type)
	{
		BlackList blacklist = PLSGpopData::instance()->getBlackList();
		auto finder = [value](QString name) { return value.contains(name); };

		if (type == pls_blacklist_type::GPUModel) {
			if (std::find_if(blacklist.gpuModels.begin(), blacklist.gpuModels.end(), finder) != blacklist.gpuModels.end())
				return pls_blacklist_type::GPUModel;
		} else if (type == pls_blacklist_type::GraphicsDrivers) {
			if (std::find_if(blacklist.graphicsDrivers.begin(), blacklist.graphicsDrivers.end(), finder) != blacklist.graphicsDrivers.end())
				return pls_blacklist_type::GraphicsDrivers;
		} else if (type == pls_blacklist_type::DeviceDrivers) {
			if (std::find_if(blacklist.deviceDrivers.begin(), blacklist.deviceDrivers.end(), finder) != blacklist.deviceDrivers.end())
				return pls_blacklist_type::DeviceDrivers;
		} else if (type == pls_blacklist_type::ThirdPlugins) {
			if (std::find_if(blacklist.thirdPartyPlugins.begin(), blacklist.thirdPartyPlugins.end(), finder) != blacklist.thirdPartyPlugins.end())
				return pls_blacklist_type::ThirdPlugins;
		} else if (type == pls_blacklist_type::VSTPlugins) {
			if (std::find_if(blacklist.vstPlugins.begin(), blacklist.vstPlugins.end(), finder) != blacklist.vstPlugins.end())
				return pls_blacklist_type::VSTPlugins;
		} else if (type == pls_blacklist_type::ThirdPrograms) {
			if (std::find_if(blacklist.thirdPartyPrograms.begin(), blacklist.thirdPartyPrograms.end(), finder) != blacklist.thirdPartyPrograms.end())
				return pls_blacklist_type::ThirdPrograms;
		} else if (type == pls_blacklist_type::ExceptionType) {
			if (blacklist.exceptionTypes.contains(value)) {
				return pls_blacklist_type::ExceptionType;
			}
		}

		return pls_blacklist_type::OtherType;
	}

	void pls_alert_third_party_plugins(QString pluginName, QWidget *parent) { QMetaObject::invokeMethod(main, "OnUsingThirdPartyPlugins", Q_ARG(QString, pluginName), Q_ARG(QWidget *, parent)); }

	//PRISM/Xiewei/20210113/#/add apis for stream deck
	virtual void pls_set_side_window_visible(int key, bool visible) override { main->mainView->setSidebarWindowVisible(key, visible); }
	virtual void pls_mixer_mute_all(bool mute) { mute ? main->MuteAllAudio() : main->UnmuteAllAudio(); }
	virtual bool pls_mixer_is_all_mute() { return main->ConfigAllMuted(); }
	virtual QList<SideWindowInfo> pls_get_side_windows_info() { return main->mainView->getSideWindowInfo(); }
	virtual int pls_get_toast_message_count() { return main->mainView->getToastMessageCount(); }
	virtual QString pls_get_login_state() { return main->getLoginState(); }
	virtual QString pls_get_stream_state() { return main->getStreamState(); }
	virtual QString pls_get_record_state() { return main->getRecordState(); }
	virtual bool pls_get_live_record_available() { return App()->HotkeyEnable(); }

	QDialogButtonBox::StandardButton pls_alert_warning(const char *title, const char *message) { return PLSAlertView::warning(PLSBasic::Get(), QTStr(title), QTStr(message)); }

	void pls_singletonWakeup() { PLSBasic::Get()->singletonWakeup(); }

	uint pls_get_live_start_time() { return main->getStartTimeStamp(); }
};

pls_frontend_callbacks *InitializeAPIInterface(PLSBasic *main)
{
	pls_frontend_callbacks *api = new PLSStudioAPI(main);
	pls_frontend_set_callbacks_internal(api);
	return api;
}
