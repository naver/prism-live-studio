#ifndef PLSBACKGROUNDMUSICVIEW_H
#define PLSBACKGROUNDMUSICVIEW_H

#include "PLSBgmDataManager.h"

#include "PLSSideBarDialogView.h"
#include "loading-event.hpp"
#include "PLSToastMsgFrame.h"
#include "obs.hpp"
#include "window-basic-main.hpp"
#include "PLSBasic.h"
#include "PLSBgmControlsBase.h"
#include "pls/media-info.h"

#include <QLabel>
#include <qnetworkaccessmanager.h>
#include <QThread>
#include <QQueue>

namespace Ui {
class PLSBackgroundMusicView;
}

class QRadioButton;
class QMediaPlayer;
class PLSBgmItemView;
class PLSBgmItemCoverView;
class PLSBgmLibraryView;

class SendThread : public QObject {

	Q_OBJECT
public:
	using QObject::QObject;
	~SendThread() override = default;

private:
	void SendMusicMetaData(const PLSBgmItemData &data) const;

public slots:
	void DoWork(const PLSBgmItemData &data) const;
};

class CheckValidThread : public QObject {
	Q_OBJECT
public:
	using QObject::QObject;
	~CheckValidThread() override = default;

private slots:
	void CheckUrlAvailable(const PLSBgmItemData &data);

signals:
	void checkFinished(const PLSBgmItemData &data, bool result);
};

class GetCoverThread : public QObject {
	Q_OBJECT
public:
	using QObject::QObject;
	~GetCoverThread() override = default;

private:
	void PrintThumbInfo(const QString &url, media_info_t *mi) const;

private slots:
	void SaveCoverToLocalPath(const PLSBgmItemData &data, const QImage &image);
	void GetCoverImage(const PLSBgmItemData &data);
	void NextTask();
signals:
	void Finished(const PLSBgmItemData &data);
	void GetPreviewImage(const QImage &image, const PLSBgmItemData &data);

private:
	QQueue<PLSBgmItemData> taskQueue;
};

class DragLabel : public QLabel {
	Q_OBJECT
public:
	explicit DragLabel(QWidget *parent = nullptr) : QLabel(parent)
	{
		installEventFilter(this);
		setCursor(Qt::ArrowCursor);
	}

protected:
	bool eventFilter(QObject *object, QEvent *event) override;
signals:
	void CoverPressed(const QPoint &point);

private:
	QPoint startPoint{};
	bool mousePressed{false};
};

class DragInFrame : public QFrame {

	Q_OBJECT
public:
	explicit DragInFrame(QWidget *parent = nullptr) : QFrame(parent) { setAcceptDrops(true); }

protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;

signals:
	void AudioFileDraggedIn(const QStringList &paths);
};

class PLSBackgroundMusicView : public PLSSideBarDialogView {
	Q_OBJECT

public:
	enum class PlayMode { InOrderMode, RandomMode, UnknownMode };
	Q_ENUM(PlayMode)

	explicit PLSBackgroundMusicView(DialogInfo info, QWidget *parent = nullptr);
	~PLSBackgroundMusicView() override;

	PLSBackgroundMusicView(const PLSBackgroundMusicView &) = delete;
	PLSBackgroundMusicView &operator=(const PLSBackgroundMusicView &) = delete;
	PLSBackgroundMusicView(PLSBackgroundMusicView &&) = delete;
	PLSBackgroundMusicView &operator=(PLSBackgroundMusicView &&) = delete;

	void UpdateSourceList(const QString &sourceName, quint64 sceneItem, const BgmSourceVecType &sourceList);
	void RenameSourceName(const quint64 &item, const QString &newName, const QString &prevName);
	void ClearUrlInfo();
	void DisconnectSignalsWhenAppClose() const;
	bool CurrentPlayListBgmDataExisted(const QString &url) const;
	int GetCurrentPlayListDataSize() const;
	int GetCurrentBgmSourceSize() const;
	QVector<PLSBgmItemData> GetPlayListDatas() const;
	void RefreshPropertyWindow();
	void UpdateSourceSelectUI();
	PLSBgmItemData GetCurrentPlayListDataBySettings(const QString &name) const;

public slots:
	void UpdateLoadUIState(const QString &name, bool load, bool isOpen);
	void AddSourceAndRefresh(const QString &sourceName, quint64 sceneItem);
	void RemoveSourceAndRefresh(const QString &sourceName, quint64 sceneItem);
	void RemoveBgmSourceList(const BgmSourceVecType &sourceList);
	void InitSourceSettingsData(const QString &sourceName, const quint64 &sceneItem, bool createNew, bool setCurrent = true);
	void SetSourceSelect(const QString &sourceName, quint64 sceneItem, bool selected);
	void SetSourceVisible(const QString &sourceName, quint64 sceneItem, bool visible);
	void OnSceneChanged();
	void UpdateLoadingStartState(const QString &sourceName);
	void UpdateLoadingEndState(const QString &sourceName);

	// media callback
	void OnMediaStateChanged(const QString &name, obs_media_state state);
	void OnMediaRestarted(const QString &name) const;
	void OnLoopStateChanged(const QString &name);

protected:
	void closeEvent(QCloseEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void OnDelButtonClicked(const PLSBgmItemData &data);
	void OnNoNetwork(const QString &toast, const PLSBgmItemData &data, bool gotoNext = false);
	void OnInvalidSongs(const QString &name, bool gotoNext = false) const;
	void OnRetryNetwork();
	void OnPlayListItemDoublePressed(const QModelIndex &index);
	void OnPlayListItemRowChanged(const int &srcIndex, const int &destIndex);
	void OnAudioFileDraggedIn(const QStringList &paths);
	void OnAddCachePlayList(const QVector<PLSBgmItemData> &datas);

	void OnPlayButtonClicked() const;
	void OnPreButtonClicked();
	void OnNextButtonClicked();
	void OnLoopBtnClicked(const quint64 &sceneItem, bool checked);
	void OnLocalFileBtnClicked();
	void OnLibraryBtnClicked();
	void OnAddSourceBtnClicked() const;
	void OnAddMusicBtnClicked();
	void OnLoadFailed(const QString &name);
	void OnRefreshButtonClicked();

	void OnCurrentPageChanged(int index);
	// slider state
	void SliderClicked();
	void SliderReleased();
	void SliderMoved(int val);
	void SetSliderPos();
	void SeekTimerCallback();
	void UpdateUIBySourceSettings(obs_data_t *settings, OBSSource source, const quint64 &sceneItem, bool createNew);

	void UpdatePlayingUIState(const QString &name);
	void UpdatePauseUIState(const QString &name);
	void UpdateStopUIState(const QString &name);
	void UpdateOpeningUIState(const QString &name);
	void UpdateErrorUIState(const QString &name, bool gotoNextSongs);
	void OnUpdateOpeningUIState(const QString &name);
	void UpdateStatuPlayling(const QString &name);

private:
	void initUI();
	void clearUI(quint64 sceneItem);
	void Save() const;
	void SetLoop(const quint64 &sceneItem, bool checked);

	/* create thread */
	void CreateCheckValidThread();

	void AddSource(const QString &sourceName, quint64 sceneItem, bool createNew = true);
	void RemoveSource(const QString &sourceName, quint64 sceneItem);
	obs_source_t *GetSource(const quint64 &sceneItem);
	bool SameWithCurrentSource(const QString &sourceName);
	bool SameWithCurrentSource(const quint64 &sceneItem);
	bool IsSameState(OBSSource source, obs_media_state state);
	QVector<PLSBgmItemData> GetPlayListData(obs_data_t *settings) const;
	PLSBgmItemData GetPlayListDataBySettings(obs_data_t *settings) const;

	PLSBgmItemData GetCurrentPlayListData() const;
	PLSBgmItemData GetCurrentPlayListDataBySettings();

	void UpdatePlayListUI(const quint64 &sceneItem);
	void AddPlayListUI(const quint64 &sceneItem);
	void SetPlayerControllerStatus(const quint64 &sceneItem, bool listChanged = false);
	void ResetControlView();
	void DisablePlayerControlUI(bool disable);
	void SetCurrentPlayMode(PlayMode mode);
	static void PLSFrontendEvent(enum obs_frontend_event event, void *ptr);

	int GetPlayListItemIndexByKey(const QString &key) const;

	void SetUrlInfo(OBSSource source, const PLSBgmItemData &data) const;
	void SetUrlInfoToSettings(obs_data_t *settings, const PLSBgmItemData &data) const;

	QString StrcatString(const QString &title, const QString &producer) const;
	bool IsSourceAvailable(const QString &sourceName, quint64 sceneItem) const;

	void StartSliderPlayingTimer();
	void StopSliderPlayingTimer();

	void StartLoadingTimer(int timeOutMs = 200);
	void StopLoadingTimer();

	bool CheckNetwork() const;
	bool CheckValidLocalAudioFile(const QString &url) const;
	void RefreshMulicEnabledPlayList(const quint64 &sceneItem, const QVector<PLSBgmItemData> &datas);
	void UpdateCurrentPlayStatus(const QString &sourceName);
	void SetPlayListStatus(const PLSBgmItemData &data);
	void SetPlayListItemStatus(const int &index, const PLSBgmItemData &data);
	void SetPlayerPaneEnabled(bool enabled);

	// library
	void CreateLibraryView();

	// toast
	void InitToast();
	void ShowToastView(const QString &text);
	void ResizeToastView();

	QImage GetCoverImage(const QString &url) const;
	void SetCoverImage(const PLSBgmItemData &data);
	void ShowCoverImage(const PLSBgmItemData &data);
	void ShowCoverGif(bool show);
	void UpdateBgmCoverPath(const PLSBgmItemData &data);

	void SeekTo(int val);
	int64_t GetSliderTime(int val);

signals:
	void bgmViewVisibleChanged(bool isVisible);
	void RefreshSourceProperty(const QString &sourceName, bool disable = false);

private:
	Ui::PLSBackgroundMusicView *ui;
	QString currentSourceName{};
	quint64 currentSceneItem{};

	PlayMode mode = PlayMode::InOrderMode;
	PLSBgmLibraryView *libraryView{nullptr};

	QTimer *sliderTimer{};
	PLSLoadingEvent loadingEvent;

	bool networkAvailable{true};
	QString localFilePath{};
	bool isLoading{false};
	QTimer timerLoading;
	int indexLoading{-1};
	uint64_t loadingTimeout{0};

	PLSToastMsgFrame toastView;
	QThread sendThread;
	QThread coverThread;
	QThread checkThread;

	SendThread *sendThreadObj{};
	GetCoverThread *coverThreadObj{};
	CheckValidThread *checkThreadObj{};

	obs_media_state last_state = OBS_MEDIA_STATE_NONE;

	QTimer seekTimer;
	int seek = -1;
	int lastSeek = -1;
	bool seeking = false;
	bool prevPaused = false;
};

#endif // PLSBACKGROUNDMUSICVIEW_H
