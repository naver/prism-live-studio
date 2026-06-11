#pragma once

#include "window-dock-browser.hpp"
#include "youtube-api-wrappers.hpp"

class QAction;
class QCefWidget;

class YouTubeAppDock : public BrowserDock {
	Q_OBJECT

public:
	YouTubeAppDock(const QString &title);

	enum streaming_mode_t { YTSM_ACCOUNT, YTSM_STREAM_KEY };

	void AccountConnected();
	void AccountDisconnected();
	void Update();

	void BroadcastCreated(const char *stream_id);
	void BroadcastSelected(const char *stream_id);
	void IngestionStarted();
	void IngestionStopped();

	static bool IsYTServiceSelected();
	static YoutubeApiWrappers *GetYTApi();
	static void CleanupYouTubeUrls();

	static bool IsYouTubeService();
	bool IsUserSignedIntoYT();

public slots:
	void SettingsUpdated(bool cleanup = false);

protected:
	void IngestionStarted(const char *stream_id, streaming_mode_t mode);
	void IngestionStopped(const char *stream_id, streaming_mode_t mode);

private:
	std::string InitYTUserUrl();
	void SetVisibleYTAppDockInMenu(bool visible);
	void AddYouTubeAppDock();
	void CreateBrowserWidget(const std::string &url);
	virtual void showEvent(QShowEvent *event) override;
	virtual void closeEvent(QCloseEvent *event) override;
	void DispatchYTEvent(const char *event, const char *video_id, streaming_mode_t mode);
	void UpdateChannelId();
	void UpdateVideoId();
	void ReloadChatDock();
	void SetInitEvent(streaming_mode_t mode, const char *event = nullptr, const char *video_id = nullptr,
			  const char *channelId = nullptr);

	void setVideoId(bool bNewCreate, QString sVideoId);

	QString channelId;
	QString videoId;
	QPointer<QCefWidget> dockBrowser;
};
