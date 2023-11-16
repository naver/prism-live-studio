#pragma once

#include "window-basic-main.hpp"
#include "PLSSceneCollectionView.h"
#include "PLSSceneCollectionManagement.h"
#include "giphy/PLSGiphyStickerView.h"
#include "prism-sticker/PLSPrismSticker.h"
#include "PLSDrawPen/PLSDrawPenView.h"
#include "PLSBasicStatusBar.hpp"
#include "PLSBasicStatusPanel.hpp"
#include "PLSRegionCapture.h"

#include <set>

class HookNativeEvent;
class PLSBackgroundMusicView;
struct pls_frontend_callbacks;
struct QCefCookieManager;

extern const std::vector<QString> presetColorListWithOpacity;

using BgmSourceVecType = std::vector<std::pair<QString, quint64>>;

enum class PlatformType {
	Empty = 0,

	Others = 0x0001,
	Band = 0x0002,
	CustomRTMP = 0x0004,
	BandAndCustomRTMP = Band | CustomRTMP,

	MultiPlatform = 0x2000,
	MpOthers = MultiPlatform | Others,
	MpBand = MultiPlatform | Band,
	MpCustomRTMP = MultiPlatform | CustomRTMP,
	MpBandAndCustomRTMP = MultiPlatform | Band | CustomRTMP
};
enum class RestartAppType { Direct = 0, Logout, ChangeLang, Update };

class CheckCamProcessWorker : public QObject {
	Q_OBJECT
public:
	using QObject::QObject;
	~CheckCamProcessWorker();

private slots:
	void CheckCamProcessIsExisted(const QString &processName);
	void Check();
signals:
	void checkFinished(bool isExisted);

private:
	QPointer<QTimer> checkTimer;
	bool processIsExisted{false};
	QString processName;
};

class PLSBasic : public OBSBasic {
	Q_OBJECT
	Q_PROPERTY(QIcon viewerCountIcon READ GetViewerCountIcon WRITE SetViewerCountIcon DESIGNABLE true)
	Q_PROPERTY(QIcon ndiIcon READ GetNdiIcon WRITE SetNdiIcon DESIGNABLE true)
	Q_PROPERTY(QIcon bgmIcon READ GetBgmIcon WRITE SetBgmIcon DESIGNABLE true)
	Q_PROPERTY(QIcon stickerIcon READ GetStickerIcon WRITE SetStickerIcon DESIGNABLE true)
	Q_PROPERTY(QIcon prismStickerIcon READ GetPrismStickerIcon WRITE SetPrismStickerIcon DESIGNABLE true)
	Q_PROPERTY(QIcon textMotionIcon READ GetTextMotionIcon WRITE SetTextMotionIcon DESIGNABLE true)
	Q_PROPERTY(QIcon chatIcon READ GetChatIcon WRITE SetChatIcon DESIGNABLE true)
	Q_PROPERTY(QIcon audiovIcon READ GetAudiovIcon WRITE SetAudiovIcon DESIGNABLE true)
	Q_PROPERTY(QIcon virtualBackgroundIcon READ GetVirtualBackgroundIcon WRITE SetVirtualBackgroundIcon DESIGNABLE true)
	Q_PROPERTY(QIcon prismMobileIcon READ GetPrismMobileIcon WRITE SetPrismMobileIcon DESIGNABLE true)
	Q_PROPERTY(QIcon timerIcon READ GetTimerIcon WRITE SetTimerIcon DESIGNABLE true)
	Q_PROPERTY(QIcon appAudioIcon READ GetAppAudioIcon WRITE SetAppAudioIcon DESIGNABLE true)
	Q_PROPERTY(QIcon decklinkInputIcon READ GetDecklinkInputIcon WRITE SetDecklinkInputIcon DESIGNABLE true)
	Q_PROPERTY(QIcon regionIcon READ GetRegionIcon WRITE SetRegionIcon DESIGNABLE true)
	friend class HookNativeEvent;

public:
	explicit PLSBasic(PLSMainView *mainView);
	~PLSBasic();

public:
	static PLSBasic *instance();

	PLSMainView *getMainView();
	pls_frontend_callbacks *getApi();
	bool willshow();
	static void removeConfig(const char *config);
	void backupSceneCollection(QString &secneCollectionName, QString &sceneCollectionFile);
	static void clearPrismConfigInfo();

public:
	void PLSInit();
#if defined(Q_OS_WIN)
	void onShowMainMenu();
#elif defined(Q_OS_MACOS)
	//TODO
#endif

	void addActionPasteMenu();
	PLSSceneItemView *GetCurrentSceneItemView();

	// bgm
	BgmSourceVecType GetCurrentSceneBgmSourceList();
	obs_sceneitem_t *GetCurrentSceneItemBySourceName(const QString &name);
	obs_sceneitem_t *GetSceneItemBySourceName(const QString &name);
	bool GetSelectBgmSourceName(QString &selectSourceName, quint64 &item) const;
	bool isBgmSourceBySourceId(const obs_scene_item *item) const;
	BgmSourceVecType EnumAllBgmSource() const;
	void ClearMusicResource();

	static void GetSourceTypeList(std::vector<std::vector<QString>> &preset, std::vector<QString> &other, std::map<QString, bool> &monitors);
	static void loadSourceTypeList(std::map<QString, bool> &monitors, std::vector<QString> &otherList);
	QString getSupportLanguage() const;
	void replaceMenuActionWithWidgetAction(QMenuBar *menuBar, QAction *originAction, QWidgetAction *replaceAction);
	void replaceMenuActionWithWidgetAction(QMenu *menu, QAction *originAction, QWidgetAction *replaceAction);
	void CheckAppUpdate();
	void CheckAppUpdateFinished();
	int compareVersion(const QString &v1, const QString &v2) const;
	int getUpdateResult() const;
	bool ShowUpdateView(QWidget *parent);
	void restartPrismApp(bool isUpdated = false);
	void initSideBarWindowVisible();
	bool CheckStreamEncoder() const;
	void restartAppDirect(bool isUpdate);
	void restartApp(const RestartAppType &restartType = RestartAppType::Direct);

	//browser interaction
	bool CheckHideInteraction(OBSSceneItem items);
	void HideAllInteraction(obs_source_t *except_source);
	void ShowInteractionUI(obs_source_t *source, bool show) const;
	void OnSourceInteractButtonClick(OBSSource source);
	void OnRemoveSceneItem(OBSSceneItem item);

	void ShowLoadSceneCollectionError();
	bool checkMainViewClose(QCloseEvent *event);
	virtual void closeMainBegin() override;
	virtual void closeMainFinished() override;

	void CreateToolArea();
	void OnToolAreaVisible();
	inline bool IsDrawPenMode() const { return drawPenView && drawPenView->IsDrawPenMode(); }
	void UpdateDrawPenView(OBSSource scene, bool collectionChanging) const;
	void UpdateDrawPenVisible(bool locked);
	void ResizeDrawPenCursorPixmap() const;
	bool ShowStickerView(const char *id);
	void OnSourceCreated(const char *id);

	bool GetSourceIcon(QIcon &icon, int type) const override;
	QIcon GetViewerCountIcon() const;
	void SetViewerCountIcon(const QIcon &icon);
	QIcon GetNdiIcon() const;
	void SetNdiIcon(const QIcon &icon);

	QIcon GetStickerIcon() const;
	QIcon GetPrismMobileIcon() const;
	QIcon GetBgmIcon() const;
	QIcon GetTextMotionIcon() const;
	QIcon GetRegionIcon() const;
	QIcon GetChatIcon() const;
	QIcon GetAudiovIcon() const;
	QIcon GetVirtualBackgroundIcon() const;
	QIcon GetPrismStickerIcon() const;
	QIcon GetTimerIcon() const;
	QIcon GetAppAudioIcon() const;
	QIcon GetDecklinkInputIcon() const;

	void SetStickerIcon(const QIcon &icon);
	void SetPrismMobileIcon(const QIcon &icon);
	void SetBgmIcon(const QIcon &icon);
	void SetTextMotionIcon(const QIcon &icon);
	void SetRegionIcon(const QIcon &icon);
	void SetChatIcon(const QIcon &icon);
	void SetAudiovIcon(const QIcon &icon);
	void SetVirtualBackgroundIcon(const QIcon &icon);
	void SetPrismStickerIcon(const QIcon &icon);
	void SetTimerIcon(const QIcon &icon);
	void SetAppAudioIcon(const QIcon &icon);
	void SetDecklinkInputIcon(const QIcon &icon);

	static void InitBrowserPanelSafeBlock();
	static QCefCookieManager *getBrowserPannelCookieMgr(const QString &pannelName);
	static void delPannelCookies(const QString &pannelName);
	static void setManualPannelCookies(const QString &pannelName);
	static QString cookiePath(const QString &pannelName);
	// scene display method
	void SetSceneDisplayMethod(int method) const;
	int GetSceneDisplayMethod() const;
	QMenu *CreateSceneDisplayMenu();

	QString getStreamState() const;
	QString getRecordState() const;

	OBSDock *GetChatDock() const { return ui->chatDock; }
	void InitChatDockGeometry();
	void SetAttachWindowBtnText(QAction *action, bool isFloating) const;

	void OnBrowserDockSeperatedBtnClicked();
	void OnBrowserDockTopLevelChanged();
	void CreateAdvancedButtonForBrowserDock(OBSDock *dock, const QString &uuid);
	void setDockDetachEnabled(bool dockLocked);

	static void frontendEventHandler(enum obs_frontend_event event, void *private_data);
	static void frontendEventHandler(pls_frontend_event event, const QVariantList &params, void *context);
	static void LogoutCallback(pls_frontend_event event, const QVariantList &params, void *context);

	void graphicsCardNotice() const;
	void UpdateAudioMixerMenuTxt();

	bool _isSupportHEVC(const QVariantMap &info) const;
	bool _isSupportHEVC();
	bool CheckHEVC();
	void showSettingVideo();
	bool isAppUpadting() const;

public slots:
	void OnSideBarButtonClicked(int buttonId);

	void OnTransitionAdded();
	void onTransitionRemoved(OBSSource source);
	void OnTransitionRenamed();
	void OnTransitionSet();
	void OnTransitionDurationValueChanged(int value);

	void OnActionAudioMasterControlTriggered() const;
	void OnActionAdvAudioPropertiesTriggered();
	void OnMixerDockSeperateBtnClicked();
	void OnMixerDockLocationChanged();
	void OnMixerLayoutTriggerd();

	void on_actionShowAbout_triggered();
	void on_actionHelpPortal_triggered();
	void on_actionHelpOpenSource_triggered();
	void on_actionDiscord_triggered() const;
	void on_actionWebsite_triggered() const;
	void on_actionContactUs_triggered(const QString &message = QString(), const QString &additionalMessage = QString());
	void on_actionRepair_triggered() const;
	void on_actionBlog_triggered() const;
	void on_stats_triggered();
	void on_actionStudioMode_triggered();
	void on_actionShowVideosFolder_triggered() const;

	void on_checkUpdate_triggered();
	void actionLaboratory_triggered();
	void OnScenesCurrentItemChanged();
	void OnStickerClicked();
	void OnPrismStickerClicked();
	void OnStickerApply(const QString &fileName, const GiphyData &taskData);
	void AddStickerSource(const char *id, const QString &file, const GiphyData &giphyData);
	void CheckStickerSource() const;
	void OnDrawPenClicked() const;
	void OnCamStudioClicked(QStringList arguments, QWidget *parent);
	bool CheckCamStudioInstalled(QString &program);
	void ShowInstallCamStudioTips(QWidget *parent, QString title, QString content, QString okTip, QString cancelTip);

	void OnPRISMStickerApplied(const StickerHandleResult &stickerData);
	void CreateStickerDownloader(obs_source_t *source);
	void OnStickerDownloadCallback(const TaskResponData &responData);
	void InitGiphyStickerViewVisible();
	void InitPrismStickerViewVisible();

	void OnSetBgmViewVisible(bool state);
	void RenameBgmSourceName(obs_sceneitem_t *sceneItem, const QString &newName, const QString &prevName);
	void SetBgmItemSelected(obs_scene_item *item, bool isSelected);
	void SetBgmItemVisible(obs_scene_item *item, bool isVisible);
	void AddBgmItem(obs_scene_item *item);
	void RemoveBgmItem(obs_scene_item *item);
	void RemoveBgmList(obs_scene_t *scene);

	void onPopupSettingView(const QString &tab, const QString &group);
	int showSettingView(const QString &tab, const QString &group);

	void showEncodingInStatusBar() const;
	bool toggleStatusPanel(int iSwitch);
	void moveStatusPanel();
	void updateStatusPanel(PLSBasicStatusData &dataStatus) const;

	void singletonWakeup();
	QList<QAction *> adjustedMenu();
	void adjustFileMenu();
	void initVirtualCamMenu();
	void initLabMenu();
	void rehearsalSwitchedToLive() const;
	void showMainViewAfter(QWidget *parentWidget);
	void startDownloading();
	void OpenRegionCapture();
	void AddSelectRegionSource(const char *id, const QRect &rectSelected);

	void UpdateStudioPortraitLayoutUI(bool studioMode, bool studioPortraitLayout);

	//get the rehearsal or live start time stamp
	uint getStartTimeStamp() const;

public:
	static void MediaStateChanged(void *data, calldata_t *calldata);
	static void MediaLoad(void *data, calldata_t *calldata);
	static void PropertiesChanged(void *param, calldata_t *data);
	static void MediaRestarted(void *param, calldata_t *data);

	static void OnSourceNotify(void *data, calldata_t *calldata);
	static void OnSourceMessage(void *data, calldata_t *calldata);

protected:
	void showEvent(QShowEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	QPushButton *CreateSourcePopupMenuCustomWidget(const char *id, const QString &display, const QSize &iconSize);
	void setUserIcon() const;
	void initConnect();

	// scene collection
	void OnExportSceneCollectionClicked();
	void StartSaveSceneCollectionTimer();

	void OnSceneDockTopLevelChanged();
	void OnSourceDockTopLevelChanged();
	void OnChatDockTopLevelChanged();
	void OnSourceDockSeperatedBtnClicked();
	void OnSceneDockSeperatedBtnClicked();
	void OnChatDockSeperatedBtnClicked();
	void changeDockState(PLSDock *dock, QAction *action) const;
	void SetDocksMovePolicy(PLSDock *dock) const;
	void docksMovePolicy(PLSDock *dock, const QPoint &pre) const;

	QStringList getRestartParams(bool isUpdate);
	void ClearStickerCache() const;
	void InitInteractData() const;
	bool hasSourceMonitor() const;
	void updateInfoViewShow();
	void getLocalUpdateResult();

	void InitCamStudioSidebarState();
	void CreateCheckCamProcessThread(const QString &processName);

private slots:
	void OnSaveSceneCollectionTimerTriggered();
	void CreateGiphyStickerView();
	void CreatePrismStickerView();
	void AddPRISMStickerSource(const StickerHandleResult &data);

	obs_source_t *CreateSource(const char *id, obs_data_t *settings = nullptr);

	// bgm
	void OnBgmClicked();
	void createBackgroundMusicView();
	void SetBgmViewVisible(bool visible);
	void ReorderBgmSourceList();
	bool GetBgmViewVisible() const;

	void OnSourceNotifyAsync(QString name, int msg, int code);
	void OnMainViewShow(bool isShow);

	void PrismLogout() const;

signals:
	void backgroundTemplateSourceError(int msg, int code);
	void outputStateChanged();

private:
#if defined(Q_OS_WIN)
	bool m_mainMenuShow = false;
#endif

	QWidgetAction *m_checkUpdateWidgetAction{nullptr};
	PLSNewIconActionWidget *m_checkUpdateWidget{nullptr};
	QPointer<PLSGiphyStickerView> giphyStickerView;
	QPointer<PLSPrismSticker> prismStickerView;
	QPointer<PLSBackgroundMusicView> backgroundMusicView;
	QPointer<PLSRegionCapture> regionCapture;
	void *interaction_sceneitem_pointer = nullptr;
	QPointer<PLSBasicStatusPanel> m_dialogStatusPanel;
	pls_process_t *camStudioProcess = nullptr;
	QString m_updateGcc;
	QString m_updateVersion;
	QString m_updateFileUrl;
	QString m_updateInfoUrl;
	bool m_updateForceUpdate;
	int m_checkUpdateResult = 0;
	bool m_requestUpdate = false;
	bool m_isLogout = false;
	bool m_isUpdateLanguage = false;
	bool isFirstShow = true;
	bool m_isSessionExpired = false;
	QPointer<PLSDrawPenView> drawPenView{nullptr};
	QIcon viewerCountIcon;
	QIcon ndiIcon;
	QIcon bgmIcon;
	QIcon textMotionIcon;
	QIcon chatIcon;
	QIcon regionIcon;
	QIcon audiovIcon;
	QIcon virtualBackgroundIcon;
	QIcon prismStickerIcon;
	QIcon stickerIcon;
	QIcon prismMobileIcon;
	QIcon timerIcon;
	QIcon appAudioIcon;
	QIcon decklinkInputIcon;
	QMenu *virtualCamMenu = nullptr;
	QAction *virtualCam = nullptr;
	QAction *virtualCamSetting = nullptr;
	QAction *prismLab = nullptr;
	QAction *logOut = nullptr;
	QAction *exitApp = nullptr;
	bool m_isStartCategoryRequest = false;
	bool gpopDataInited = false;
	bool isAudioMonitorToastDisplay = false;
	std::set<std::string> m_hook_failed_game_list;
	std::map<uint64_t, QString> stickerSourceMap;
	CheckCamProcessWorker *checkCamProcessWorker = nullptr;
	QThread checkCamThread;
	bool m_isAppUpdating = false;
	QTimer m_noticeTimer;

	QAction *actionRealTime = nullptr;
	QAction *actionThumbnail = nullptr;
	QAction *actionText = nullptr;
};
