#ifndef PLSNOTICEUPDATECOORDINATOR_H
#define PLSNOTICEUPDATECOORDINATOR_H

#include <QObject>
#include "PLSNoticeUpdateTypes.hpp"

class PLSMainView;

class PLSNoticeUpdateCoordinator : public QObject {
public:
	static PLSNoticeUpdateCoordinator *instance();

	void handleStartupNoticeResult(const QList<PLSNoticeUpdateItem> &noticeInfos);
	void handlePollingNoticeResult(const QList<PLSNoticeUpdateItem> &noticeInfos);
	void showPopupQueueDirect(const QList<PLSNoticeUpdateItem> &noticeInfos, QWidget *parentWidget = nullptr, PLSMainView *mainView = nullptr);

	void openCenterFromMenu();
	void openCenterFromPopup(PLSNoticeFilter initialFilter, PLSNoticeCategory initialCategory = PLSNoticeCategory::Notice, bool isApiRequest = false,
				 bool prefetchPeerCache = false, QWidget *parentWidget = nullptr);

	void processPendingNoticeAfterUiClosed();
	bool isNoticeUiOpen() const;
	void syncBadgeStateFromUnread(bool hasUnread);
	void armBadgeFromUnread(bool hasUnread);
	void onHelpSidebarOpened(bool hasUnread);
	void consumeMenuBadge();
	void consumeAllBadges();
	bool sidebarBadgeVisible() const;
	bool helpMenuBadgeVisible() const;

private:
	enum class UiState {
		Idle,
		PopupQueueShowing,
		CenterOpen,
	};

	explicit PLSNoticeUpdateCoordinator(QObject *parent = nullptr);

	void showPopupQueue(const QList<PLSNoticeUpdateItem> &noticeInfos, QWidget *parentWidget = nullptr, PLSMainView *mainView = nullptr);
	void showCenterDialog(PLSNoticeFilter initialFilter, PLSNoticeCategory initialCategory, bool isApiRequest, bool prefetchPeerCache, QWidget *parentWidget = nullptr);

	void onPopupQueueStarted();
	void onPopupQueueFinished();
	void onCenterOpened();
	void onCenterClosed();

	void deferNoticeRefresh();
	void refetchPendingNotices();

private:
	UiState m_uiState{UiState::Idle};
	bool m_pendingNoticeRefresh{false};
	bool m_pendingNoticeRefreshInFlight{false};
	bool m_sidebarBadgeVisible{false};
	bool m_helpMenuBadgeVisible{false};
};

#endif // PLSNOTICEUPDATECOORDINATOR_H
