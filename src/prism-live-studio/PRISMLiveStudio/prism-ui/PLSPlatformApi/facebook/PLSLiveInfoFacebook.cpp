#include "PLSLiveInfoFacebook.h"
#include <QBitmap>
#include <QPainter>
#include <QPainterPath>
#include <QDesktopServices>
#include <QPointer>
#include <utility>
#include "log/log.h"
#include "ui_PLSLiveInfoFacebook.h"
#include "PLSPlatformApi.h"
#include "PLSChannelDataAPI.h"
#include "frontend-api.h"
#include "PLSAlertView.h"

constexpr auto liveInfoMoudule = "PLSLiveInfoFacebook";

const int GAMELIST_ITEM_HEIGHT = 40;
const int GAMELIST_BORDER = 1;
const int GAME_ITEM_MAX_SIZE = 5;
const int TITLE_MAX_LENGTH = 250;

#define IsTimelineObject ui->shareFirstObject->getComboBoxTitle() == TimelineObjectFlags
#define IsGroupObjectFlags ui->shareFirstObject->getComboBoxTitle() == GroupObjectFlags
#define IsPageObjectFlags ui->shareFirstObject->getComboBoxTitle() == PageObjectFlags
#define IsLiving PLSCHANNELS_API->isLiving()
using namespace std;
using namespace common;

void PLSLiveInfoFacebook::handleRequestFunctionType(PLSErrorHandler::RetData retData)
{
	if (PLSErrorHandler::CHANNEL_FACEBOOK_INVALIDACCESSTOKEN == retData.prismCode) {
		handleFacebookIncalidAccessToken(retData);
	} else {
		PLSErrorHandler::directShowAlert(retData, nullptr);
	}
}

int PLSLiveInfoFacebook::loadingMaskHeight() const
{
	if (!m_isFromGoLive) {
		return -1;
	}
	return ui->horizontalLayout_5->geometry().y();
}

void PLSLiveInfoFacebook::handleFacebookIncalidAccessToken(PLSErrorHandler::RetData retData)
{
	if (m_showTokenAlert) {
		return;
	}
	m_showTokenAlert = true;
	PLSAlertView::Button button = PLSAlertView::Button::NoButton;
	if (!retData.alertMsg.isEmpty()) {
		showLoading(content(), loadingMaskHeight());
		button = PLSErrorHandler::directShowAlert(retData, nullptr);
		hideLoading();
	}
	reject();
	if (button == PLSAlertView::Button::Ok) {
		PLSCHANNELS_API->channelExpired(platform->getChannelUUID(), false);
	}
	m_showTokenAlert = false;
}

PLSLiveInfoFacebook::PLSLiveInfoFacebook(PLSPlatformBase *pPlatformBase, QWidget *parent, bool isFromGoLive)
	: PLSLiveInfoBase(pPlatformBase, parent), platform(dynamic_cast<PLSPlatformFacebook *>(pPlatformBase)), m_isFromGoLive(isFromGoLive)
{
	ui = pls_new<Ui::PLSLiveInfoFacebook>();
	PLS_INFO(liveInfoMoudule, "Facebook liveinfo Will show");
	pls_add_css(this, {"PLSLiveInfoFacebook"});
	setupUi(ui);
	ui->horizontalLayout_6->addWidget(createResolutionButtonsFrame());
	ui->labelOpen->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("facebook.liveinfo.Public.Title")));
	setWindowTitle(tr("LiveInfo.liveinformation"));

	setFocusPolicy(Qt::StrongFocus);
	setFocus();

	platform->insertParentPointer(this);

	ui->dualWidget->setText(tr("Facebook"))->setUUID(platform->getChannelUUID());

	//set first class menu and second class menu
	initComboBoxList();

	//window active or deactive
	m_gameListWidget = pls_new<QListWidget>(this);
	m_gameListWidget->setObjectName("gameListWidget");
	m_gameListWidget->setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
	m_gameListWidget->setVisible(false);
	m_gameListWidget->setAttribute(Qt::WA_ShowWithoutActivating, true);
	m_gameListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

	//set the gamelineEditWidget list
	connect(ui->gameLineEdit, &PLSFacebookGameLineEdit::searchKeyword, this, &PLSLiveInfoFacebook::searchGameKeyword);
	connect(ui->gameLineEdit, &PLSFacebookGameLineEdit::clearText, this, [this] {
		m_gameListWidget->clear();
		m_gameListWidget->setVisible(false);
	});
	connect(m_gameListWidget, &QListWidget::itemClicked, this, [this](const QListWidgetItem *item) {
		PLSLoadingComboxItemData data = item->data(Qt::UserRole).value<PLSLoadingComboxItemData>();
		if (data.type != PLSLoadingComboxItemType::Fa_NormalTitle) {
			return;
		}
		ui->gameLineEdit->setText(data.title);
		hideSearchGameList();
		this->setFocus();
	});

	//set title line edit
	initLineEdit();

	//set the ok button state
	connect(
		ui->titleField, &QLineEdit::textChanged, this,
		[this](const QString &inputText) {
			if (QByteArray output = inputText.toUtf8(); output.size() > TITLE_MAX_LENGTH) {
				int truncateAt = 0;
				for (int i = TITLE_MAX_LENGTH; i > 0; i--) {
					if ((output[i] & 0xC0) != 0x80) {
						truncateAt = i;
						break;
					}
				}
				output.truncate(truncateAt);
				QSignalBlocker signalBlocker(ui->titleField);
				ui->titleField->setText(QString::fromUtf8(output));
				PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_FACEBOOK_TITLE_MAX_LENGTH, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData("PLSLiveInfoFacebook"));
				return;
			}
			doUpdateOkState();
		},
		Qt::QueuedConnection);
	connect(ui->descriptionTitle, &QTextEdit::textChanged, this, [this] { doUpdateOkState(); });
	connect(ui->gameLineEdit, &PLSFacebookGameLineEdit::textChanged, this, [this] { doUpdateOkState(); });

	//connect qApp focusChaned
	connect(qApp, &QApplication::focusChanged, m_gameListWidget, [this](const QWidget *, const QWidget *now) {
		if (!pls_object_is_valid(this) || !pls_object_is_valid(m_gameListWidget) || !pls_object_is_valid(ui->gameLineEdit))
			return;

		if (ui->gameLineEdit == now) {
			QString text = ui->gameLineEdit->text();
			if (m_gameListWidget->isHidden() && text.size() > 0) {
				showSearchGameList();
			}
		} else {
			hideSearchGameList();
		}
	});

	//update ok title logic
	updateStepTitle(ui->okButton);

#if defined(Q_OS_WIN)
	if (!PLS_PLATFORM_API->isPrepareLive()) {
		ui->horizontalLayout_5->addWidget(ui->cancelButton);
	}
#endif

	bool gameTagHidden = true;
	ui->gameLabel->setHidden(gameTagHidden);
	ui->gameLineEdit->setHidden(gameTagHidden);

	//update ok state logic
	doUpdateOkState();

	pls_uistep_v2_set_title(this, QStringLiteral("Live Information: %1").arg(platform->getNameForChannelType()));
	pls_uistep_v2_set_name(ui->shareFirstObject, QStringLiteral("Public1st"));
	pls_uistep_v2_set_value(ui->shareFirstObject, [=] { return ui->shareFirstObject->getComboBoxTitle(); });
	pls_uistep_v2_set_name(ui->shareSecondObject, QStringLiteral("Public2nd"));
	pls_uistep_v2_set_value(ui->shareSecondObject, [=] { return ui->shareSecondObject->getComboBoxTitle(); });
	pls_uistep_v2_auto_bind(this);
}

void PLSLiveInfoFacebook::initComboBoxList()
{
	//the first shareobject comboBox
	ui->shareFirstObject->setDisabled(IsLiving);
	PLSAPIFacebook::FacebookPrepareLiveInfo prepareInfo = platform->getPrepareInfo();
	ui->shareFirstObject->setComboBoxTitleData(prepareInfo.firstObjectName);
	ui->shareSecondObject->setComboBoxTitleData(prepareInfo.secondObjectName, prepareInfo.secondObjectId);
	connect(ui->shareFirstObject, &PLSLoadingCombox::pressed, this, [this] {
		PLS_UI_ACTION("Public shareFirst pressed");
		ui->shareFirstObject->showTitlesView(platform->getShareObjectList());
	});
	connect(ui->shareFirstObject, &PLSLoadingCombox::clickItemIndex, this, [this](int showIndex) {
		QString title = platform->getShareObjectList().at(showIndex);
		if (title == ui->shareFirstObject->getComboBoxTitle()) {
			return;
		}
		// Switching away from whichever Timeline/Group/Page is currently selected interrupts any
		// permission check still in flight for it, instead of leaving it to finish in the
		// background: cancelInFlightPermissionRequest() must run before setComboBoxTitleData()
		// below, since IsTimelineObject/IsGroupObjectFlags/IsPageObjectFlags (and thus which
		// completion callback is holding the shared browser-consent slot) are keyed off the
		// combo's title text.
		cancelInFlightPermissionRequest();
		// Invalidate any in-flight request keyed off the previous selection (e.g.
		// onClickPageComboBox()'s page-list fetch) that isn't covered by
		// cancelInFlightPermissionRequest() above, so its completion can recognize itself as stale.
		++m_shareFirstObjectGeneration;
		ui->shareFirstObject->setComboBoxTitleData(title);
		if (IsTimelineObject) {
			ui->shareSecondObject->setComboBoxTitleData(TimelinePublicName, TimelinePublicId);
		} else if (IsGroupObjectFlags) {
			ui->shareSecondObject->setComboBoxTitleData(GROUP_COMBOX_DEFAULT_TEXT);
		} else if (IsPageObjectFlags) {
			ui->shareSecondObject->setComboBoxTitleData(PAGE_COMBOX_DEFAULT_TEXT);
		}
		QString log = QString("Facebook liveinfo shareFirstObject title is %1,update shareSecondObject title is %2").arg(title).arg(ui->shareSecondObject->getComboBoxTitle());
		PLS_UI_STEP(liveInfoMoudule, log.toUtf8().constData(), ACTION_CLICK);
		doUpdateOkState();
	});

	//the second ComboBox
	connect(ui->shareSecondObject, &PLSLoadingCombox::pressed, this, [this] {
		PLS_UI_ACTION("Public shareSecond pressed");
		if (IsTimelineObject) {
			// Fire the same permission check the OK button runs later, as early as possible, so
			// Facebook's consent screen (if needed) appears now instead of at go-live time. The
			// alert/UI outcome is still handled solely by the OK-button path, so this can't race
			// or duplicate that alert. checkTimelineLivingPermission() itself joins an
			// already-in-flight check instead of starting a competing one, so repeatedly opening
			// this dropdown is safe. Started before showTitlesView() (like onClickPageComboBox()'s
			// equivalent fix): showTitlesView() -> showMenuView() -> QMenu::exec() blocks in its
			// own event loop until the popup closes, so starting the check after it would delay
			// "as early as possible" until the user has already dismissed the dropdown, defeating
			// the point of this early trigger.
			// doUpdateOkState() is safe to call unconditionally here even if the OK button joined
			// this request while it was in flight: m_okButtonDisabledForAction (set by
			// checkTimelineLivingPermission()'s onRequestingPermission hook if the consent browser
			// actually opens) keeps doUpdateOkState() forcing okButton disabled until
			// checkLivePermissionFinished (running right after this callback, as the pending
			// callback) resolves the session and clears it.
			checkTimelineLivingPermission([this](const PLSErrorHandler::RetData &) {
				// This early trigger never itself leads to going live (that only happens through
				// on_okButton_clicked()'s own flow) - whether the consent browser ended up granted
				// or declined, this session is over and must not leave okButton stuck disabled.
				m_okButtonDisabledForAction = false;
				doUpdateOkState();
			});
			ui->shareSecondObject->showTitlesView(platform->getItemNameList(FacebookPrivacyItemType));
		} else if (IsGroupObjectFlags) {
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_FACEBOOK_GROUP_DISABLED, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData("PLSLiveInfoFacebook"));
		} else if (IsPageObjectFlags) {
			onClickPageComboBox();
		}
	});
	connect(ui->shareSecondObject, &PLSLoadingCombox::clickItemIndex, this, [this](int showIndex) {
		QString name;
		QString idString;
		QString itemType;
		if (IsTimelineObject) {
			itemType = FacebookPrivacyItemType;
		} else if (IsGroupObjectFlags) {
			itemType = FacebookGroupItemType;
		} else if (IsPageObjectFlags) {
			itemType = FacebookPageItemType;
		}
		platform->getItemInfo(itemType, showIndex, name, idString);
		ui->shareSecondObject->setComboBoxTitleData(name, idString);
		QString log = QString("Facebook liveinfo shareSecondObject type is %1 and title is %2").arg(itemType).arg(name);
		PLS_UI_STEP(liveInfoMoudule, log.toUtf8().constData(), ACTION_CLICK);
		doUpdateOkState();
	});

	//when living, group and page second list is disabled
	if (IsLiving && (IsGroupObjectFlags || IsPageObjectFlags)) {
		ui->shareSecondObject->setDisabled(true);
	}
}

void PLSLiveInfoFacebook::on_cancelButton_clicked()
{
	string log = "Facebook liveinfo cancel button";
	PLS_UI_STEP(liveInfoMoudule, log.c_str(), ACTION_CLICK);
	cancelInFlightPermissionRequest();
	for (const QString &expiredId : m_expiredObjectList) {
		QString dashboardItemId = platform->getPrepareInfo().secondObjectId;
		if (dashboardItemId == expiredId) {
			platform->setPrivacyToTimeline();
		}
	}
	reject();
}

void PLSLiveInfoFacebook::on_okButton_clicked()
{
	// okButton is no longer force-disabled the instant a go-live/update-live permission check
	// starts (see below): it only gets disabled once a permission check actually needs to open
	// Facebook's consent browser. This guard replaces that disabling as the re-entrancy check, so a
	// second click landing while the (now still-enabled) button is mid fast permission check can't
	// re-enter this function and start a second go-live/update-live session.
	if (m_liveActionCheckInFlight) {
		return;
	}

	// Before the live broadcast, click the OK button to save the LiveInfo information to memory.
	saveLiveInfo(m_oldPrepareInfo);

	// If it is before live broadcast and it is timeline or group, return directly
	bool isPrepareLive = PLS_PLATFORM_API->isPrepareLive();
	bool isLivingProcess = IsLiving;
	if (isLivingProcess) {
		PLS_UI_STEP(liveInfoMoudule, "Facebbok liveinfo update living ok button", ACTION_CLICK);
	} else if (isPrepareLive) {
		PLS_UI_STEP(liveInfoMoudule, "Facebook liveinfo goLive button", ACTION_CLICK);
	} else {
		PLS_UI_STEP(liveInfoMoudule, "Facebook liveinfo ok button", ACTION_CLICK);
		if (IsTimelineObject) {
			getTimelineOrGroupOrPageInfoRequest();
			return;
		}
	}

	// Start checking permissions
	QStringList permissionList;
	PLSAPIFacebook::PLSAPI apiType;
	if (isPrepareLive || isLivingProcess) {
		// Marks the whole go-live/update-live session as active, purely to block a second click of
		// okButton from re-entering this function (see the guard at the top) while this session is
		// still running - it does NOT by itself keep okButton visually disabled; that's
		// m_okButtonDisabledForAction's job (see below and doUpdateOkState()).
		m_liveActionCheckInFlight = true;

		auto checkLivePermissionFinished = [this, isPrepareLive, isLivingProcess](const PLSErrorHandler::RetData &retData) {
			hideLoading();
			// Restore, not force-enable: initComboBoxList() permanently disables this control for
			// Group/Page while already living, and this shared callback must not override that.
			ui->shareSecondObject->setEnabled(!(IsLiving && (IsGroupObjectFlags || IsPageObjectFlags)));
			ui->shareFirstObject->setDisabled(IsLiving);

			bool willStartOrUpdateLiving = retData.prismCode == PLSErrorHandler::SUCCESS && (isPrepareLive || isLivingProcess);
			if (!willStartOrUpdateLiving) {
				// The session ends here (cancelled, declined, or failed permission check): let
				// doUpdateOkState() evaluate the real enabled state again. Clears
				// m_okButtonDisabledForAction too, in case this permission check did open the
				// consent browser (declined/failed) - otherwise okButton would stay stuck disabled.
				m_liveActionCheckInFlight = false;
				m_okButtonDisabledForAction = false;
				// doUpdateOkState() uses repaint() (immediate, synchronous), not update(): calling it
				// here unconditionally - including when willStartOrUpdateLiving is true and permission
				// was already granted (fast path, m_okButtonDisabledForAction still false at this
				// point) - would flash okButton visibly enabled for one frame before
				// startLivingRequest()/updateLivingRequest() force it disabled again a few lines below.
				// Only call it for the "session really ends here" case; the going-live branch leaves
				// okButton exactly as doUpdateOkState() already left it after checkPermission's own
				// onRequestingPermission hook (still true) or never disabled it at all (fast path),
				// and startLivingRequest()/updateLivingRequest() take care of disabling it themselves.
				doUpdateOkState();
			}

			if (m_permissionCancelledByUser) {
				// This completion was synthesized by our own cancelInFlightPermissionRequest()
				// call (Cancel button, or switching Timeline/Group/Page away mid-request), not a
				// real declined response; the user's action already explains what happened, so
				// skip the generic declined-alert path here.
				m_permissionCancelledByUser = false;
				return;
			}

			if (willStartOrUpdateLiving) {
				if (isPrepareLive) {
					startLivingRequest();
				} else if (isLivingProcess) {
					updateLivingRequest();
				}
				return;
			}

			if (isPrepareLive) {
				switch (retData.prismCode) {
				case PLSErrorHandler::CHANNEL_FACEBOOK_INVALIDACCESSTOKEN:
					PLS_LOGEX(PLS_LOG_ERROR, liveInfoMoudule,
						  {{"platformName", "facebook"}, {"startLiveStatus", "Failed"}, {"startLiveFailed", "facebook check living permission api invalid access token"}},
						  "facebook start live failed");
					break;
				case PLSErrorHandler::CHANNEL_FACEBOOK_DECLINED:
					PLS_LOGEX(PLS_LOG_ERROR, liveInfoMoudule,
						  {{"platformName", "facebook"}, {"startLiveStatus", "Failed"}, {"startLiveFailed", "user decline the facebook living permission"}},
						  "facebook start live failed");

					break;
				default:
					PLS_LOGEX(PLS_LOG_ERROR, liveInfoMoudule,
						  {{"platformName", "facebook"}, {"startLiveStatus", "Failed"}, {"startLiveFailed", "facebook check living permission failed"}},
						  "facebook start live failed");
					break;
				}
			}

			handleRequestFunctionType(retData);
		};

		showLoading(content(), loadingMaskHeight());
		// showLoading()'s overlay blocks mouse input but not a keyboard-focused button's Space/Enter
		// activation; shareSecondObject is disabled explicitly so a repeat activation can't re-enter
		// the dropdown's early-trigger branch while this check is in flight and race with it over the
		// shared PLSAPICheckTimelineLivingPermission request slot. shareFirstObject is intentionally
		// left enabled: switching Timeline/Group/Page away mid-request now interrupts the in-flight
		// permission check (see clickItemIndex's cancelInFlightPermissionRequest() call) instead of
		// being blocked. okButton is NOT disabled here: it only gets disabled once a permission check
		// below actually needs to open the Facebook consent browser (missing-permission path), not for
		// the fast common-case round trip where everything is already granted; the top-of-function
		// m_liveActionCheckInFlight guard covers re-entrancy in the meantime.
		ui->shareSecondObject->setEnabled(false);

		if (IsTimelineObject) {
			// If the dropdown's prefetch is still in flight, checkTimelineLivingPermission() joins
			// it (registers checkLivePermissionFinished as the pending callback) instead of issuing
			// a second goFacebookRequestPermission call: PLSAPIFacebook's browser-consent channel
			// is a single shared slot, not keyed by request type, so a second concurrent call would
			// open a second browser tab and silently orphan whichever one the user doesn't finish.
			checkTimelineLivingPermission(checkLivePermissionFinished);
			return;
		} else if (IsGroupObjectFlags) {
			permissionList << timeline_living_permission;
			permissionList << group_living_permission;
			apiType = PLSAPIFacebook::PLSAPICheckGroupLivingPermission;
			PLSFaceBookRquest->checkPermission(apiType, permissionList, checkLivePermissionFinished, this, [this] {
				m_okButtonDisabledForAction = true;
				doUpdateOkState();
			});
		} else if (IsPageObjectFlags) {
			// Routed through the same in-flight/join mechanism as onClickPageComboBox()'s own
			// permission check: both call sites can otherwise trigger PLSAPIFacebook's shared
			// browser-consent slot independently (different PLSAPI request types, so nothing else
			// would coordinate them), letting one tear the other down and synthesize a spurious
			// "declined" result for whichever request actually held the slot.
			checkPageLivingPermission(checkLivePermissionFinished);
			return;
		}

	} else if (IsPageObjectFlags || IsGroupObjectFlags) {

		auto pageGetInfoPermissionFinished = [this](const PLSErrorHandler::RetData &retData) {
			hideLoading();

			if (m_permissionCancelledByUser) {
				// This completion was synthesized by our own cancelInFlightPermissionRequest()
				// call (Cancel button, or switching Timeline/Group/Page away mid-request), not a
				// real declined response; the user's action already explains what happened, so
				// skip the generic declined-alert path here.
				m_permissionCancelledByUser = false;
				return;
			}

			if (retData.prismCode == PLSErrorHandler::SUCCESS) {
				getTimelineOrGroupOrPageInfoRequest();
				return;
			}

			handleRequestFunctionType(retData);
		};

		showLoading(content(), loadingMaskHeight());

		if (IsPageObjectFlags) {
			permissionList << pages_read_engagement_permission;
			apiType = PLSAPIFacebook::PLSAPICheckPageGetInfoPermission;
		} else if (IsGroupObjectFlags) {
			permissionList << group_living_permission;
			apiType = PLSAPIFacebook::PLSAPICheckGroupGetInfoPermission;
		}
		PLSFaceBookRquest->checkPermission(apiType, permissionList, pageGetInfoPermissionFinished, this);
	}
}

void PLSLiveInfoFacebook::cancelInFlightPermissionRequest()
{
	// cancelFacebookRequestPermission() synchronously re-enters whichever completion callback
	// currently holds PLSAPIFacebook's shared browser-consent slot (checkLivePermissionFinished,
	// pageGetInfoPermissionFinished, or onClickPageComboBox()'s dropdown-prefetch callback) with a
	// synthesized declined result; the flag suppresses that callback's generic declined-alert path
	// since a user-initiated cancel/switch isn't an error. It's a no-op (flag stays false) when no
	// request is currently in flight.
	m_permissionCancelledByUser = true;
	// Also clear the forced-disable state before tearing down the request: if the consent browser
	// was actually open (onRequestingPermission already fired), the dropdown early-trigger's own
	// completion callback is just doUpdateOkState() - with nothing else to clear this flag, okButton
	// would otherwise stay stuck disabled after switching away from Timeline/Group/Page. The
	// OK-button-initiated flow's checkLivePermissionFinished also clears it on this same declined
	// completion, making this redundant-but-harmless for that path.
	m_okButtonDisabledForAction = false;
	PLSFaceBookRquest->cancelFacebookRequestPermission();
	// Not dead code: if a real callback already consumed the pending request a moment before this
	// call, cancelFacebookRequestPermission() is a no-op and the guard above never runs, so this
	// reset is what prevents the flag from staying stuck true.
	m_permissionCancelledByUser = false;
}

void PLSLiveInfoFacebook::initLineEdit()
{
	ui->titleField->setText(platform->getPrepareInfo().title);
	ui->descriptionTitle->setText(platform->getPrepareInfo().description);
	ui->gameLineEdit->setText(platform->getPrepareInfo().gameName);
	initPlaceTextHorderColor(ui->titleField);
	initPlaceTextHorderColor(ui->descriptionTitle);
	initPlaceTextHorderColor(ui->gameLineEdit);
}

void PLSLiveInfoFacebook::onClickGroupComboBox()
{
	PLS_UI_STEP(liveInfoMoudule, "Facebook group comboBox", ACTION_CLICK);

	auto onFinish = [this](const PLSErrorHandler::RetData &retData) {
		ui->shareFirstObject->setDisabled(IsLiving);
		if (retData.prismCode != PLSErrorHandler::SUCCESS) {
			handleRequestFunctionType(retData);
			return;
		}
		getMyGroupListRequestSuccess();
		doUpdateOkState();
	};
	platform->getMyGroupListRequestAndCheckPermission(onFinish, this);
	ui->shareSecondObject->showLoadingView();
}

void PLSLiveInfoFacebook::getMyGroupListRequestSuccess()
{
	if (QList<QString> nameList = platform->getItemNameList(FacebookGroupItemType); !nameList.isEmpty()) {
		QList<QString> idList = platform->getItemIdList(FacebookGroupItemType);
		if (QString groupId = ui->shareSecondObject->getComboBoxId(); !idList.contains(groupId)) {
			ui->shareSecondObject->setComboBoxTitleData(GROUP_COMBOX_DEFAULT_TEXT);
		} else {
			QString title = platform->getItemName(groupId, FacebookGroupItemType);
			ui->shareSecondObject->setComboBoxTitleData(title, groupId);
		}
		ui->shareSecondObject->updateTitleIdsView(nameList, idList);
	} else {
		ui->shareSecondObject->setComboBoxTitleData(GROUP_COMBOX_DEFAULT_TEXT);
		ui->shareSecondObject->refreshGuideView(tr("facebook.empty.grouplist.tip"));
	}
}

void PLSLiveInfoFacebook::onClickPageComboBox()
{
	PLS_UI_STEP(liveInfoMoudule, "Facebook page comboBox", ACTION_CLICK);
	// Run the same permission check the OK button's Page branch runs, through the shared
	// checkPageLivingPermission() join mechanism, before fetching the page list: without this,
	// this dropdown's own permission check and the OK button's PLSAPICheckPageLivingPermission
	// check are two independent PLSAPIFacebook::checkPermission() calls (different PLSAPI request
	// types, so nothing else coordinates them) that can each open a browser consent tab and tear
	// the other down via the shared browser-consent slot, synthesizing a spurious "declined"
	// result for whichever one actually held it. Started before showLoadingView() (like
	// onClickGroupComboBox() and the pre-existing Page code both do): showLoadingView() ->
	// showMenuView() -> QMenu::exec() blocks in its own event loop until the popup closes, so
	// starting the async chain first lets its eventual completion still live-update the open
	// menu via updateTitleIdsView(); starting it after exec() would delay it until the popup
	// is already dismissed.
	// Captured before the permission-check/list-fetch chain starts: if the user switches
	// shareFirstObject away from Page before the chain below completes, this call's result is no
	// longer relevant and must not overwrite shareSecondObject's now-unrelated Timeline/Group data.
	const int requestGeneration = m_shareFirstObjectGeneration;
	checkPageLivingPermission([this, requestGeneration](const PLSErrorHandler::RetData &retData) {
		if (m_permissionCancelledByUser) {
			// This completion was synthesized by cancelInFlightPermissionRequest() (Cancel
			// button, or switching Timeline/Group/Page away mid-request), which means
			// checkLivePermissionFinished() is also part of this same completion chain (either
			// as the request it joined, or joined onto this one) and is responsible for
			// resetting the flag; don't reset it here too, and skip this callback's alert path
			// since the user's own action already explains what happened.
			return;
		}
		// This early trigger never itself leads to going live (that only happens through
		// on_okButton_clicked()'s own flow) - whether the consent browser (if
		// checkPageLivingPermission()'s onRequestingPermission hook opened one) ended up granted or
		// declined, this session is over and must not leave okButton stuck disabled.
		m_okButtonDisabledForAction = false;
		if (retData.prismCode != PLSErrorHandler::SUCCESS) {
			doUpdateOkState();
			handleRequestFunctionType(retData);
			return;
		}
		// Permission is already confirmed at this point; getMyPageListRequestAndCheckPermission()'s
		// own me/permissions pre-check will see everything granted and short-circuit straight to
		// fetching the page list, with no second consent popup.
		auto onFinish = [this, requestGeneration](const PLSErrorHandler::RetData &retData2) {
			if (requestGeneration != m_shareFirstObjectGeneration) {
				// shareFirstObject switched away from Page while this page-list fetch was in
				// flight (not covered by cancelInFlightPermissionRequest(), which only tears down
				// the permission-check channel above). This response is stale; applying it now
				// would overwrite shareSecondObject with Page data while shareFirstObject already
				// shows Timeline/Group.
				return;
			}
			if (retData2.prismCode != PLSErrorHandler::SUCCESS) {
				doUpdateOkState();
				handleRequestFunctionType(retData2);
				return;
			}
			getMyPageListRequestSuccess();
			doUpdateOkState();
		};
		platform->getMyPageListRequestAndCheckPermission(onFinish, this);
	});
	ui->shareSecondObject->showLoadingView();
}

void PLSLiveInfoFacebook::getMyPageListRequestSuccess()
{
	if (QList<QString> nameList = platform->getItemNameList(FacebookPageItemType); !nameList.isEmpty()) {
		QList<QString> idList = platform->getItemIdList(FacebookPageItemType);
		if (QString pageId = ui->shareSecondObject->getComboBoxId(); !idList.contains(pageId)) {
			ui->shareSecondObject->setComboBoxTitleData(PAGE_COMBOX_DEFAULT_TEXT);
		} else {
			QString title = platform->getItemName(pageId, FacebookPageItemType);
			ui->shareSecondObject->setComboBoxTitleData(title, pageId);
		}
		ui->shareSecondObject->updateTitleIdsView(nameList, idList);
	} else {
		ui->shareSecondObject->setComboBoxTitleData(PAGE_COMBOX_DEFAULT_TEXT);
		ui->shareSecondObject->refreshGuideView(tr("facebook.empty.pagelist.tip"));
	}
}

void PLSLiveInfoFacebook::checkTimelineLivingPermission(const MyRequestTypeFunction &onFinished)
{
	// okButton is intentionally NOT disabled here: this fast me/permissions round trip is the
	// common case (permission already granted), and PRISM_PC-6431 wants GoLive clickable during it.
	// It only gets disabled below, via onRequestingPermission, if this check actually needs to open
	// Facebook's consent browser - whether this call starts that request or joins one already
	// running (here), the dropdown's early prefetch (see shareSecondObject's pressed handler) can
	// start it before OK is even clicked.
	if (m_timelinePermissionCheckInFlight) {
		// Join the request already in flight instead of starting a second
		// goFacebookRequestPermission call for the same permission set: PLSAPIFacebook keeps its
		// browser-consent state (m_permissionCallback/m_permissionServer) in a single shared slot,
		// not one per request type, so a second concurrent call would open a second browser tab and
		// silently orphan whichever tab the user doesn't finish, potentially losing a real grant.
		m_timelinePermissionPendingCallback = onFinished;
		return;
	}
	m_timelinePermissionCheckInFlight = true;
	QPointer<PLSLiveInfoFacebook> guard(this);
	// Timeline permission stays isolated from Page permission except for Public visibility:
	// Facebook only needs pages_read_user_content_permission/pages_read_engagement_permission
	// granted to allow EVERYONE-visibility on a Timeline live; Friends-only Timeline doesn't
	// need them, so they're only requested when Public is the current selection.
	QStringList permissionList;
	permissionList << timeline_living_permission;
	if (ui->shareSecondObject->getComboBoxId() == TimelinePublicId) {
		permissionList << pages_read_user_content_permission;
		permissionList << pages_read_engagement_permission;
	}
	PLSFaceBookRquest->checkPermission(
		PLSAPIFacebook::PLSAPICheckTimelineLivingPermission, permissionList,
		[guard, onFinished](const PLSErrorHandler::RetData &retData) {
			if (!guard)
				return;
			guard->m_timelinePermissionCheckInFlight = false;
			// Restore, not force-enable: initComboBoxList() permanently disables this control
			// while already living, regardless of object type, and this must not override that.
			guard->ui->shareFirstObject->setDisabled(IsLiving);
			onFinished(retData);
			if (auto pending = std::exchange(guard->m_timelinePermissionPendingCallback, nullptr)) {
				pending(retData);
			}
		},
		this, [guard] {
			if (guard) {
				guard->m_okButtonDisabledForAction = true;
				guard->doUpdateOkState();
			}
		});
}

void PLSLiveInfoFacebook::checkPageLivingPermission(const MyRequestTypeFunction &onFinished)
{
	// Mirrors checkTimelineLivingPermission(): onClickPageComboBox()'s own permission check and
	// the OK button's Page branch are otherwise two independent PLSAPIFacebook::checkPermission()
	// calls (different PLSAPI request types) that can each open a browser consent tab and tear
	// the other down via the shared browser-consent slot. Joining is valid here because this
	// permission list is a strict superset of what PLSAPICheckPageLivingPermission alone needs
	// (see getMyPageListRequestAndCheckPermission()'s comment for the same union rationale): a
	// SUCCESS/decline result for this list is a correct answer for the narrower request too.
	// See checkTimelineLivingPermission(): okButton is intentionally NOT disabled here, only via
	// onRequestingPermission below if this check actually needs to open Facebook's consent
	// browser, whether started here or joined below (onClickPageComboBox() can start this before OK
	// is clicked).
	if (m_pagePermissionCheckInFlight) {
		m_pagePermissionPendingCallback = onFinished;
		return;
	}
	m_pagePermissionCheckInFlight = true;
	QPointer<PLSLiveInfoFacebook> guard(this);
	QStringList permissionList;
	permissionList << page_show_list_permission;
	permissionList << business_management_permission;
	permissionList << pages_manage_posts_permission;
	permissionList << pages_read_engagement_permission;
	permissionList << pages_read_user_content_permission;
	PLSFaceBookRquest->checkPermission(
		PLSAPIFacebook::PLSAPICheckPageLivingPermission, permissionList,
		[guard, onFinished](const PLSErrorHandler::RetData &retData) {
			if (!guard)
				return;
			guard->m_pagePermissionCheckInFlight = false;
			guard->ui->shareFirstObject->setDisabled(IsLiving);
			onFinished(retData);
			if (auto pending = std::exchange(guard->m_pagePermissionPendingCallback, nullptr)) {
				pending(retData);
			}
		},
		this, [guard] {
			if (guard) {
				guard->m_okButtonDisabledForAction = true;
				guard->doUpdateOkState();
			}
		});
}

PLSLiveInfoFacebook::~PLSLiveInfoFacebook()
{
	pls_object_remove(this);
	pls_delete(ui);
}

void PLSLiveInfoFacebook::searchGameKeyword(const QString keyword)
{
	PLS_UI_STEP(liveInfoMoudule, "Facebook liveinfo search game tag", ACTION_CLICK);
	QPoint p = this->content()->mapToGlobal(QPoint(ui->gameLineEdit->pos().x(), ui->gameLineEdit->pos().y() + ui->gameLineEdit->height()));
	m_gameListWidget->move(p);
	int count = m_gameListWidget->count();
	int gameItemHeight = GAMELIST_ITEM_HEIGHT;
	int border = 1;
	int padding = 1;
	if (count > GAME_ITEM_MAX_SIZE || count == 0) {
		count = GAME_ITEM_MAX_SIZE;
	}
	m_gameListWidget->setFixedSize(ui->gameLineEdit->width(), gameItemHeight * count + (border + padding) * 2);
	setupGameListCornerRadius();
	m_gameListWidget->setVisible(true);
	m_gameListWidget->clear();
	auto onFinish = [this](const PLSErrorHandler::RetData &retData) {
		if (retData.prismCode != PLSErrorHandler::SUCCESS) {
			handleRequestFunctionType(retData);
			return;
		}
		gameTagListRequestSuccess();
		setupGameListCornerRadius();
	};
	platform->getGameTagListByKeyword(onFinish, keyword);
}

void PLSLiveInfoFacebook::gameTagListRequestSuccess()
{
	QList<QString> list = platform->getItemNameList(FacebookGameItemType);
	m_gameListWidget->clear();
	int gameItemHeight = GAMELIST_ITEM_HEIGHT;
	int border = 1;
	int padding = 1;
	for (int i = 0; i < list.size(); i++) {
		QString title = list.at(i);
		QListWidgetItem *item = pls_new<QListWidgetItem>(title);
		auto itemData = PLSLoadingComboxItemData();
		itemData.showIndex = i;
		itemData.title = title;
		item->setData(Qt::UserRole, QVariant::fromValue<PLSLoadingComboxItemData>(itemData));
		item->setSizeHint(QSize(0, gameItemHeight));
		m_gameListWidget->addItem(item);
	}
	int size = list.size();
	if (size > GAME_ITEM_MAX_SIZE) {
		size = GAME_ITEM_MAX_SIZE;
	} else if (size == 0) {
		QString title = tr("facebook.liveinfo.game.empty.list");
		QListWidgetItem *item = pls_new<QListWidgetItem>(title);
		item->setFlags(Qt::ItemIsEnabled);
		auto itemData = PLSLoadingComboxItemData();
		itemData.showIndex = 0;
		itemData.title = title;
		itemData.type = PLSLoadingComboxItemType::Fa_Guide;
		item->setData(Qt::UserRole, QVariant::fromValue<PLSLoadingComboxItemData>(itemData));
		item->setSizeHint(QSize(0, gameItemHeight));
		m_gameListWidget->addItem(item);
		size = 1;
	}
	m_gameListWidget->setFixedSize(ui->gameLineEdit->width(), gameItemHeight * size + (border + padding) * 2);
	QString title = ui->gameLineEdit->text();
	int index = MENU_DONT_SELECTED_INDEX;
	if (list.contains(title)) {
		index = list.indexOf(title);
		m_gameListWidget->setCurrentRow(index);
	}
}

void PLSLiveInfoFacebook::hideSearchGameList()
{
	m_gameListWidget->setVisible(false);
	ui->gameLineEdit->clearFocus();
}

void PLSLiveInfoFacebook::showSearchGameList()
{
	int count = m_gameListWidget->count();
	int gameItemHeight = GAMELIST_ITEM_HEIGHT;
	int border = 1;
	int padding = 1;
	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = m_gameListWidget->item(i);
		item->setSizeHint(QSize(0, gameItemHeight));
	}
	QPoint p = this->content()->mapToGlobal(QPoint(ui->gameLineEdit->pos().x(), ui->gameLineEdit->pos().y() + ui->gameLineEdit->height()));
	m_gameListWidget->move(p);
	if (count > GAME_ITEM_MAX_SIZE || count == 0) {
		count = GAME_ITEM_MAX_SIZE;
	}
	m_gameListWidget->setFixedSize(ui->gameLineEdit->width(), gameItemHeight * count + (border + padding) * 2);
	setupGameListCornerRadius();
	m_gameListWidget->setVisible(true);
}

void PLSLiveInfoFacebook::doUpdateOkState()
{
	if (m_okButtonDisabledForAction) {
		// Facebook consent browser is open, or the actual go-live/update-live request is running;
		// okButton must stay disabled regardless of what triggered this call (title/description/game
		// text changing, combo selection changing, etc.) until that session itself resolves and
		// clears the flag. A plain in-flight permission check (m_timelinePermissionCheckInFlight /
		// m_pagePermissionCheckInFlight) does NOT set this flag, so it falls through to the normal
		// evaluation below instead of forcing a disable.
		ui->okButton->setEnabled(false);
		ui->okButton->parentWidget()->repaint();
		return;
	}
	QString newPrivacy = ui->shareSecondObject->getComboBoxTitle();
	if (newPrivacy == GROUP_COMBOX_DEFAULT_TEXT || newPrivacy == PAGE_COMBOX_DEFAULT_TEXT) {
		ui->okButton->setEnabled(false);
		ui->okButton->parentWidget()->repaint();
		return;
	}
	ui->okButton->setEnabled(true);
	ui->okButton->parentWidget()->repaint();
}

void PLSLiveInfoFacebook::getTimelineOrGroupOrPageInfoRequest()
{
	showLoading(this);

	// Obtain timeline, page, group avatar and name before live broadcast
	auto itemInfoFinished = [this](const PLSErrorHandler::RetData &retData) {
		// Remove the network loading box
		hideLoading();

		// If the user information is successfully obtained, LiveInfo will be hidden.
		if (retData.prismCode == PLSErrorHandler::SUCCESS) {
			accept();
			return;
		}

		handleRequestFunctionType(retData);
	};

	// Get avatar and name request
	platform->requestItemInfoRequest(itemInfoFinished);
}

void PLSLiveInfoFacebook::startLivingRequest()
{
	m_expiredObjectList.clear();

	// Unlike the permission check before it, actually creating the live video is not a fast/common
	// no-op round trip - it's the real go-live action, so okButton is force-disabled here
	// unconditionally (permission may have been already-granted, in which case it was never
	// disabled up to this point).
	m_okButtonDisabledForAction = true;
	doUpdateOkState();
	showLoading(content(), loadingMaskHeight());

	auto startLivingFinished = [this](const PLSErrorHandler::RetData &retData) {
		hideLoading();

		if (retData.prismCode == PLSErrorHandler::SUCCESS) {
			PLS_LOGEX(PLS_LOG_INFO, liveInfoMoudule, {{"platformName", "facebook"}, {"startLiveStatus", "Success"}}, "facebook start live success");
			accept();
			return;
		}

		if (retData.prismCode == PLSErrorHandler::CHANNEL_FACEBOOK_OBJECTNOTEXIST) {
			m_expiredObjectList.append(ui->shareSecondObject->getComboBoxId());
		}

		PLS_LOGEX(PLS_LOG_ERROR, liveInfoMoudule,
			  {{"platformName", "facebook"},
			   {"startLiveStatus", "Failed"},
			   {"startLiveFailed", qUtf8Printable(QString("facebook call create live api failed, prismCode=%1").arg(retData.prismCode))}},
			  "facebook start live failed");

		m_liveActionCheckInFlight = false;
		m_okButtonDisabledForAction = false;
		doUpdateOkState();
		handleRequestFunctionType(retData);
	};

	platform->startLiving(startLivingFinished);
}

void PLSLiveInfoFacebook::updateLivingRequest()
{
	// See startLivingRequest(): force-disabled here unconditionally for the same reason.
	m_okButtonDisabledForAction = true;
	doUpdateOkState();
	showLoading(content(), loadingMaskHeight());

	auto updateLivingFinished = [this](const PLSErrorHandler::RetData &retData) {
		hideLoading();

		if (retData.prismCode == PLSErrorHandler::SUCCESS) {
			accept();
			return;
		}

		m_liveActionCheckInFlight = false;
		m_okButtonDisabledForAction = false;
		doUpdateOkState();
		handleRequestFunctionType(retData);
	};
	platform->updateLiving(updateLivingFinished);
}

void PLSLiveInfoFacebook::saveLiveInfo(PLSAPIFacebook::FacebookPrepareLiveInfo &oldPrepareInfo)
{
	PLSAPIFacebook::FacebookPrepareLiveInfo prepareInfo = platform->getPrepareInfo();
	oldPrepareInfo = prepareInfo;
	prepareInfo.firstObjectName = ui->shareFirstObject->getComboBoxTitle();
	prepareInfo.secondObjectName = ui->shareSecondObject->getComboBoxTitle();
	prepareInfo.secondObjectId = ui->shareSecondObject->getComboBoxId();
	prepareInfo.title = ui->titleField->text();
	prepareInfo.description = ui->descriptionTitle->toPlainText();
	QString gameName = ui->gameLineEdit->text();
	prepareInfo.gameName = gameName;
	QString gameId = platform->getGameId(gameName);
	prepareInfo.gameId = gameId;
	platform->setPrepareInfo(prepareInfo);
}

bool PLSLiveInfoFacebook::isModified()
{
	//the first share object name
	PLSAPIFacebook::FacebookPrepareLiveInfo prepareInfo = platform->getPrepareInfo();
	QString oldShareObjectName = prepareInfo.firstObjectName;
	if (QString newShareObjectName = ui->shareFirstObject->getComboBoxTitle(); oldShareObjectName != newShareObjectName) {
		return true;
	}

	//the second share object name
	QString oldPrivacy = prepareInfo.secondObjectId;
	if (QString newPrivacy = ui->shareSecondObject->getComboBoxId(); newPrivacy != oldPrivacy) {
		return true;
	}

	//the title
	QString oldTitle = prepareInfo.title;
	if (QString newTitle = ui->titleField->text(); oldTitle != newTitle) {
		return true;
	}

	//the description
	QString oldDescription = prepareInfo.description;
	if (QString newDescription = ui->descriptionTitle->toPlainText(); oldDescription != newDescription) {
		return true;
	}

	//the game
	QString oldGame = prepareInfo.gameName;
	if (QString newGame = ui->gameLineEdit->text(); oldGame != newGame) {
		return true;
	}

	return false;
}

void PLSLiveInfoFacebook::initPlaceTextHorderColor(QWidget *widget) const
{
	QPalette palette = widget->palette();
	palette.setColor(QPalette::All, QPalette::PlaceholderText, Qt::white);
	widget->setPalette(palette);
}

void PLSLiveInfoFacebook::setupGameListCornerRadius()
{
	const int radius = 3;
	QPainterPath path;
	path.addRoundedRect(m_gameListWidget->rect(), radius, radius);
	auto mask = QRegion(path.toFillPolygon().toPolygon());
	m_gameListWidget->setMask(mask);
}

void PLSLiveInfoFacebook::showNetworkErrorAlert(PLSLiveInfoFacebookErrorType errorType)
{
	PLS_INFO(liveInfoMoudule, "Facebook liveinfo showNetworkErrorAlert");
	switch (errorType) {
	case PLSLiveInfoFacebookErrorType::PLSLiveInfoFacebookGroupError:
		if (IsGroupObjectFlags && ui->shareSecondObject->isChecked()) {
			PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("login.check.note.network"));
		}
		break;
	case PLSLiveInfoFacebookErrorType::PLSLiveInfoFacebookPageError:
		if (IsPageObjectFlags && ui->shareSecondObject->isChecked()) {
			PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("login.check.note.network"));
		}
		break;
	case PLSLiveInfoFacebookErrorType::PLSLiveInfoFacebookSearchGameError:
		if (ui->gameLineEdit->hasFocus()) {
			PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("login.check.note.network"));
		}
		break;
	default:
		break;
	}
}

void PLSLiveInfoFacebook::getLivingTitleDescRequest()
{
	showLoading(content(), loadingMaskHeight());
	auto onFinished = [this](const PLSErrorHandler::RetData &retData) {
		hideLoading();
		if (retData.prismCode != PLSErrorHandler::SUCCESS) {
			handleRequestFunctionType(retData);
			return;
		}
		getLivingTimelinePrivacy();
		ui->titleField->setText(platform->getPrepareInfo().title);
		ui->descriptionTitle->setText(platform->getPrepareInfo().description);
	};
	platform->getLivingVideoTitleDescRequest(onFinished);
}

void PLSLiveInfoFacebook::getLivingTimelinePrivacy()
{
	if (QString shareObjectName = platform->getPrepareInfo().firstObjectName; shareObjectName != TimelineObjectFlags) {
		return;
	}
	showLoading(content(), loadingMaskHeight());
	auto onFinished = [this](const PLSErrorHandler::RetData &retData, QString) {
		hideLoading();
		if (retData.prismCode != PLSErrorHandler::SUCCESS) {
			handleRequestFunctionType(retData);
			return;
		}
		ui->shareSecondObject->setComboBoxTitleData(platform->getPrepareInfo().secondObjectName, platform->getPrepareInfo().secondObjectId);
	};
	platform->getLivingTimelinePrivacyRequest(onFinished);
}

void PLSLiveInfoFacebook::showEvent(QShowEvent *event)
{
	if (IsLiving) {
		getLivingTitleDescRequest();
	}
	PLSLiveInfoBase::showEvent(event);
}
