#include "PLSNoticeUpdateCoordinator.hpp"

#include "PLSMainView.hpp"
#include "PLSNoticePopupDialog.hpp"
#include "PLSNoticeUpdateCenterDialog.hpp"
#include "frontend-api.h"
#include "libui.h"
#include "libutils-api.h"

PLSNoticeUpdateCoordinator *PLSNoticeUpdateCoordinator::instance()
{
	static QPointer<PLSNoticeUpdateCoordinator> coordinator;
	if (!coordinator) {
		coordinator = new PLSNoticeUpdateCoordinator(pls_get_main_view());
	}
	return coordinator;
}

PLSNoticeUpdateCoordinator::PLSNoticeUpdateCoordinator(QObject *parent) : QObject(parent) {}

void PLSNoticeUpdateCoordinator::handleStartupNoticeResult(const QList<PLSNoticeUpdateItem> &noticeInfos)
{
	if (noticeInfos.isEmpty() || pls_is_main_window_closing())
		return;

	armBadgeFromUnread(true);
	if (auto mainView = PLSMainView::instance())
		mainView->setNoticeTips(true);

	if (isNoticeUiOpen() || m_pendingNoticeRefreshInFlight) {
		deferNoticeRefresh();
		return;
	}

	showPopupQueue(noticeInfos);
}

void PLSNoticeUpdateCoordinator::handlePollingNoticeResult(const QList<PLSNoticeUpdateItem> &noticeInfos)
{
	if (noticeInfos.isEmpty() || pls_is_main_window_closing())
		return;

	armBadgeFromUnread(true);
	if (auto mainView = PLSMainView::instance())
		mainView->setNoticeTips(true);

	if (isNoticeUiOpen() || m_pendingNoticeRefreshInFlight) {
		deferNoticeRefresh();
		return;
	}

	showPopupQueue(noticeInfos);
}

void PLSNoticeUpdateCoordinator::showPopupQueueDirect(const QList<PLSNoticeUpdateItem> &noticeInfos, QWidget *parentWidget, PLSMainView *mainView)
{
	showPopupQueue(noticeInfos, parentWidget, mainView);
}

void PLSNoticeUpdateCoordinator::openCenterFromMenu()
{
	if (pls_is_main_window_closing())
		return;

	onCenterOpened();

	auto *parent = pls_get_toplevel_view(pls_get_main_view().data());
	PLSNoticeUpdateCenterDialog dialog(parent);
	dialog.openWithState();
	dialog.exec();

	onCenterClosed();
}

void PLSNoticeUpdateCoordinator::openCenterFromPopup(PLSNoticeFilter initialFilter, PLSNoticeCategory initialCategory, bool isApiRequest, bool prefetchPeerCache,
						     QWidget *parentWidget)
{
	if (pls_is_main_window_closing())
		return;

	showCenterDialog(initialFilter, initialCategory, isApiRequest, prefetchPeerCache, parentWidget);
}

void PLSNoticeUpdateCoordinator::processPendingNoticeAfterUiClosed()
{
	if (pls_is_main_window_closing() || m_uiState != UiState::Idle || !m_pendingNoticeRefresh || m_pendingNoticeRefreshInFlight)
		return;

	refetchPendingNotices();
}

bool PLSNoticeUpdateCoordinator::isNoticeUiOpen() const
{
	return m_uiState != UiState::Idle;
}

void PLSNoticeUpdateCoordinator::syncBadgeStateFromUnread(bool hasUnread)
{
	if (!hasUnread) {
		m_sidebarBadgeVisible = false;
		m_helpMenuBadgeVisible = false;
	}
}

void PLSNoticeUpdateCoordinator::armBadgeFromUnread(bool hasUnread)
{
	if (!hasUnread) {
		syncBadgeStateFromUnread(false);
		return;
	}

	m_sidebarBadgeVisible = true;
	m_helpMenuBadgeVisible = false;
}

void PLSNoticeUpdateCoordinator::onHelpSidebarOpened(bool hasUnread)
{
	if (m_sidebarBadgeVisible) {
		m_sidebarBadgeVisible = false;
		m_helpMenuBadgeVisible = hasUnread;
	} else if (!hasUnread) {
		m_helpMenuBadgeVisible = false;
	}
}

void PLSNoticeUpdateCoordinator::consumeMenuBadge()
{
	m_helpMenuBadgeVisible = false;
}

void PLSNoticeUpdateCoordinator::consumeAllBadges()
{
	m_sidebarBadgeVisible = false;
	m_helpMenuBadgeVisible = false;
}

bool PLSNoticeUpdateCoordinator::sidebarBadgeVisible() const
{
	return m_sidebarBadgeVisible;
}

bool PLSNoticeUpdateCoordinator::helpMenuBadgeVisible() const
{
	return m_helpMenuBadgeVisible;
}

void PLSNoticeUpdateCoordinator::showPopupQueue(const QList<PLSNoticeUpdateItem> &noticeInfos, QWidget *parentWidget, PLSMainView *mainView)
{
	if (noticeInfos.isEmpty() || pls_is_main_window_closing())
		return;

	auto *resolvedMainView = mainView ? mainView : PLSMainView::instance();
	QWidget *parent = parentWidget ? parentWidget : pls_get_toplevel_view(resolvedMainView ? static_cast<QWidget *>(resolvedMainView) : pls_get_main_view().data());
	PLSNoticeFilter requestedFilter = PLSNoticeFilter::PrismOnly;

	onPopupQueueStarted();
	const bool openCenterRequested = PLSNoticePopupDialog::showNoticePopupQueue(noticeInfos, parent, resolvedMainView, &requestedFilter);
	onPopupQueueFinished();

	if (openCenterRequested && !pls_is_main_window_closing()) {
		consumeAllBadges();
		if (resolvedMainView)
			resolvedMainView->setNoticeTips(false);
		openCenterFromPopup(requestedFilter, PLSNoticeCategory::Notice, false, false, parent);
	} else {
		processPendingNoticeAfterUiClosed();
	}
}

void PLSNoticeUpdateCoordinator::showCenterDialog(PLSNoticeFilter initialFilter, PLSNoticeCategory initialCategory, bool isApiRequest, bool prefetchPeerCache,
						  QWidget *parentWidget)
{
	if (pls_is_main_window_closing())
		return;

	onCenterOpened();

	auto *parent = parentWidget ? parentWidget : pls_get_toplevel_view(pls_get_main_view().data());
	PLSNoticeUpdateCenterDialog dialog(parent);
	dialog.openWithState(initialFilter, initialCategory, isApiRequest, prefetchPeerCache);
	dialog.exec();

	onCenterClosed();
}

void PLSNoticeUpdateCoordinator::onPopupQueueStarted()
{
	m_uiState = UiState::PopupQueueShowing;
}

void PLSNoticeUpdateCoordinator::onPopupQueueFinished()
{
	m_uiState = UiState::Idle;
}

void PLSNoticeUpdateCoordinator::onCenterOpened()
{
	m_uiState = UiState::CenterOpen;
}

void PLSNoticeUpdateCoordinator::onCenterClosed()
{
	m_uiState = UiState::Idle;
	processPendingNoticeAfterUiClosed();
}

void PLSNoticeUpdateCoordinator::deferNoticeRefresh()
{
	m_pendingNoticeRefresh = true;
}

void PLSNoticeUpdateCoordinator::refetchPendingNotices()
{
	m_pendingNoticeRefresh = false;
	m_pendingNoticeRefreshInFlight = true;

	pls_get_new_notice_Info([this](const QList<PLSNoticeUpdateItem> &noticeInfos) {
		pls_async_call(this, [this, noticeInfos]() {
			m_pendingNoticeRefreshInFlight = false;

			if (pls_is_main_window_closing())
				return;

			if (m_uiState != UiState::Idle) {
				if (!noticeInfos.isEmpty())
					m_pendingNoticeRefresh = true;
				return;
			}

			if (!noticeInfos.isEmpty()) {
				showPopupQueue(noticeInfos);
				return;
			}

			if (m_pendingNoticeRefresh)
				processPendingNoticeAfterUiClosed();
		});
	});
}
