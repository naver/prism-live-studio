#ifndef PLSLIVEINFOFACEBOOK_H
#define PLSLIVEINFOFACEBOOK_H

#include "../PLSLiveInfoBase.h"
#include "PLSAPIFacebook.h"
#include "PLSPlatformFacebook.h"

enum class PLSLiveInfoFacebookErrorType {
	PLSLiveInfoFacebookGroupError,
	PLSLiveInfoFacebookPageError,
	PLSLiveInfoFacebookSearchGameError,
};

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSLiveInfoFacebook;
}
QT_END_NAMESPACE

class PLSLiveInfoFacebook : public PLSLiveInfoBase {

	Q_OBJECT

public:
	explicit PLSLiveInfoFacebook(PLSPlatformBase *pPlatformBase, QWidget *parent = nullptr, bool isFromGoLive = false);
	~PLSLiveInfoFacebook() override;
	void handleRequestFunctionType(PLSErrorHandler::RetData retData);
	void handleFacebookIncalidAccessToken(PLSErrorHandler::RetData retData);

private slots:
	void on_cancelButton_clicked();
	void on_okButton_clicked();

private:
	void initComboBoxList();
	void initLineEdit();
	void onClickGroupComboBox();
	void getMyGroupListRequestSuccess();
	void onClickPageComboBox();
	void getMyPageListRequestSuccess();
	void checkTimelineLivingPermission(const MyRequestTypeFunction &onFinished);
	void checkPageLivingPermission(const MyRequestTypeFunction &onFinished);
	void searchGameKeyword(const QString keyword);
	void gameTagListRequestSuccess();
	void hideSearchGameList();
	void showSearchGameList();
	void doUpdateOkState();
	void cancelInFlightPermissionRequest();
	void startLivingRequest();
	void updateLivingRequest();
	void getTimelineOrGroupOrPageInfoRequest();
	void saveLiveInfo(PLSAPIFacebook::FacebookPrepareLiveInfo &oldPrepareInfo);
	bool isModified();
	void initPlaceTextHorderColor(QWidget *widget) const;
	void setupGameListCornerRadius();
	void showNetworkErrorAlert(PLSLiveInfoFacebookErrorType errorType);
	void getLivingTitleDescRequest();
	void getLivingTimelinePrivacy();
	int loadingMaskHeight() const;

protected:
	void showEvent(QShowEvent *event) override;

private:
	Ui::PLSLiveInfoFacebook *ui;
	QFrame *m_frame;
	PLSPlatformFacebook *platform;
	QListWidget *m_gameListWidget;
	QList<QString> m_expiredObjectList;
	bool m_showTokenAlert{false};
	// True only when this dialog was opened via the GoLive flow (PLSPlatformFacebook::onPrepareLive);
	// false for the "edit live info" entry point. Gates whether loadingMaskHeight() shrinks the
	// loading mask to expose Cancel/OK, since only the GoLive path needs the buttons reachable
	// while loading.
	bool m_isFromGoLive{false};
	PLSAPIFacebook::FacebookPrepareLiveInfo m_oldPrepareInfo;
	// True while a checkLivePermissionFinished/pageGetInfoPermissionFinished/onClickPageComboBox
	// completion is being synchronously re-entered by our own cancelInFlightPermissionRequest()
	// call (Cancel button, or switching Timeline/Group/Page away while its permission check is
	// still in flight). Suppresses the generic declined-permission alert for that synthesized
	// completion, since the user's own cancel/switch is not an error.
	bool m_permissionCancelledByUser{false};
	// True while a PLSAPICheckTimelineLivingPermission request (from either the dropdown
	// prefetch or the OK button) is outstanding. PLSAPIFacebook's browser-consent channel
	// (m_permissionCallback/m_permissionServer) is a single shared slot, not keyed by request
	// type, so a second concurrent call would silently orphan the first browser tab; this flag
	// makes a second caller join the in-flight request instead of starting a competing one.
	bool m_timelinePermissionCheckInFlight{false};
	// Callback registered by a caller that arrived while a check was already in flight; invoked
	// (once) alongside the in-flight caller's own callback when that request completes.
	MyRequestTypeFunction m_timelinePermissionPendingCallback;
	// Same pair as above, for Page: true while a PLSAPICheckPageLivingPermission request (from
	// either onClickPageComboBox()'s list-fetch or the OK button) is outstanding.
	bool m_pagePermissionCheckInFlight{false};
	MyRequestTypeFunction m_pagePermissionPendingCallback;
	// True for the whole OK-button-triggered go-live/update-live session: from the moment OK is
	// clicked (before its permission check starts) until that session is truly over (cancelled,
	// failed, or handed off to startLivingRequest()/updateLivingRequest() through to their own
	// completion). Purely a re-entrancy guard against a second OK click starting a second session
	// (checked at the top of on_okButton_clicked()) - it does NOT by itself keep okButton visually
	// disabled; see m_okButtonDisabledForAction below for that.
	bool m_liveActionCheckInFlight{false};
	// True only while okButton must be force-disabled for a real reason: the Facebook consent
	// browser is open (permission genuinely missing), or startLivingRequest()/updateLivingRequest()
	// is actually running. doUpdateOkState() disables okButton whenever this is true regardless of
	// what triggered the call, and otherwise evaluates the normal enabled state - a plain
	// in-flight me/permissions check (already-granted fast path) does NOT set this, so the button
	// stays clickable while only that quick round trip is happening (PRISM_PC-6431).
	bool m_okButtonDisabledForAction{false};
	// Bumped every time shareFirstObject's selection changes (Timeline/Group/Page). Captured by
	// onClickPageComboBox() before starting its page-list fetch and compared again when that fetch
	// completes, so a page-list response that arrives after the user has already switched away from
	// Page is recognized as stale and discarded instead of overwriting shareSecondObject with Page
	// data while shareFirstObject now reads Timeline/Group.
	int m_shareFirstObjectGeneration{0};
};
#endif // PLSLIVEINFOFACEBOOK_H
