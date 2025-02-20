#include "PLSLiveInfoFacebook.h"
#include <QBitmap>
#include <QPainter>
#include <QPainterPath>
#include <QDesktopServices>
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

void PLSLiveInfoFacebook::handleFacebookIncalidAccessToken(PLSErrorHandler::RetData retData)
{
	if (m_showTokenAlert) {
		return;
	}
	m_showTokenAlert = true;
	PLSAlertView::Button button = PLSAlertView::Button::NoButton;
	if (!retData.alertMsg.isEmpty()) {
		showLoading(content());
		button = PLSErrorHandler::directShowAlert(retData, nullptr);
		hideLoading();
	}
	reject();
	if (button == PLSAlertView::Button::Ok) {
		PLSCHANNELS_API->channelExpired(platform->getChannelUUID(), false);
	}
	m_showTokenAlert = false;
}

PLSLiveInfoFacebook::PLSLiveInfoFacebook(PLSPlatformBase *pPlatformBase, QWidget *parent) : PLSLiveInfoBase(pPlatformBase, parent), platform(dynamic_cast<PLSPlatformFacebook *>(pPlatformBase))
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
				pls_alert_error_message(PLSBasic::Get(), QTStr("Alert.Title"), QTStr("facebook.liveinfo.title.max.length"));
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
}

void PLSLiveInfoFacebook::initComboBoxList()
{
	//the first shareobject comboBox
	ui->shareFirstObject->setDisabled(IsLiving);
	PLSAPIFacebook::FacebookPrepareLiveInfo prepareInfo = platform->getPrepareInfo();
	ui->shareFirstObject->setComboBoxTitleData(prepareInfo.firstObjectName);
	ui->shareSecondObject->setComboBoxTitleData(prepareInfo.secondObjectName, prepareInfo.secondObjectId);
	connect(ui->shareFirstObject, &PLSLoadingCombox::pressed, this, [this] { ui->shareFirstObject->showTitlesView(platform->getShareObjectList()); });
	connect(ui->shareFirstObject, &PLSLoadingCombox::clickItemIndex, this, [this](int showIndex) {
		QString title = platform->getShareObjectList().at(showIndex);
		if (title == ui->shareFirstObject->getComboBoxTitle()) {
			return;
		}
		ui->shareFirstObject->setComboBoxTitleData(title);
		if (IsTimelineObject) {
			ui->shareSecondObject->setComboBoxTitleData(TimelinePublicName, TimelinePublicId);
		} else if (IsGroupObjectFlags) {
			ui->shareSecondObject->setComboBoxTitleData(GROUP_COMBOX_DEFAULT_TEXT);
		} else if (IsPageObjectFlags) {
			ui->shareSecondObject->setComboBoxTitleData(PAGE_COMBOX_DEFAULT_TEXT);
		}
		QString log = QString("Facebook liveinfo shareFirstObject title is %1,update shareSecondObject title is %2").arg(title).arg(ui->shareSecondObject->getComboBoxTitle());
		PLS_UI_STEP(liveInfoMoudule, log.toStdString().c_str(), ACTION_CLICK);
		doUpdateOkState();
	});

	//the second ComboBox
	connect(ui->shareSecondObject, &PLSLoadingCombox::pressed, this, [this] {
		if (IsTimelineObject) {
			ui->shareSecondObject->showTitlesView(platform->getItemNameList(FacebookPrivacyItemType));
		} else if (IsGroupObjectFlags) {
			QString link = "<a href=\"https://developers.facebook.com/blog/post/2024/01/23/introducing-facebook-graph-and-marketing-api-v19/\">";
			link += QString("%1</a>").arg(tr("Facebook.Group.Disabled.Link"));
			QString content = QTStr("Facebook.Group.Disabled.Text").arg(link);
			PLSAlertView::warning(this, QTStr("Alert.Title"), content);
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
		PLS_UI_STEP(liveInfoMoudule, log.toStdString().c_str(), ACTION_CLICK);
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

		auto checkLivePermissionFinished = [this, isPrepareLive, isLivingProcess](const PLSErrorHandler::RetData &retData) {
			hideLoading();

			if (retData.prismCode == PLSErrorHandler::SUCCESS) {
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

		showLoading(content());

		if (IsTimelineObject) {
			permissionList << timeline_living_permission;
			apiType = PLSAPIFacebook::PLSAPICheckTimelineLivingPermission;
		} else if (IsGroupObjectFlags) {
			permissionList << timeline_living_permission;
			permissionList << group_living_permission;
			apiType = PLSAPIFacebook::PLSAPICheckGroupLivingPermission;
		} else if (IsPageObjectFlags) {
			permissionList << pages_manage_posts_permission;
			permissionList << pages_read_engagement_permission;
			permissionList << business_management_permission;
			permissionList << pages_read_user_content_permission;
			apiType = PLSAPIFacebook::PLSAPICheckPageLivingPermission;
		}
		PLSFaceBookRquest->checkPermission(apiType, permissionList, checkLivePermissionFinished, this);

	} else if (IsPageObjectFlags || IsGroupObjectFlags) {

		auto pageGetInfoPermissionFinished = [this](const PLSErrorHandler::RetData &retData) {
			hideLoading();

			if (retData.prismCode == PLSErrorHandler::SUCCESS) {
				getTimelineOrGroupOrPageInfoRequest();
				return;
			}

			handleRequestFunctionType(retData);
		};

		showLoading(content());

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
	auto onFinish = [this](const PLSErrorHandler::RetData &retData) {
		if (retData.prismCode != PLSErrorHandler::SUCCESS) {
			handleRequestFunctionType(retData);
			return;
		}
		getMyPageListRequestSuccess();
		doUpdateOkState();
	};
	platform->getMyPageListRequestAndCheckPermission(onFinish, this);
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
	QString newPrivacy = ui->shareSecondObject->getComboBoxTitle();
	if (newPrivacy == GROUP_COMBOX_DEFAULT_TEXT || newPrivacy == PAGE_COMBOX_DEFAULT_TEXT) {
		ui->okButton->setEnabled(false);
		return;
	}
	ui->okButton->setEnabled(true);
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

	showLoading(content());

	auto startLivingFinished = [this](const PLSErrorHandler::RetData &retData) {
		hideLoading();

		if (retData.prismCode == PLSErrorHandler::SUCCESS) {
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

		handleRequestFunctionType(retData);
	};

	platform->startLiving(startLivingFinished);
}

void PLSLiveInfoFacebook::updateLivingRequest()
{
	showLoading(content());

	auto updateLivingFinished = [this](const PLSErrorHandler::RetData &retData) {
		hideLoading();

		if (retData.prismCode == PLSErrorHandler::SUCCESS) {
			accept();
			return;
		}

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
	showLoading(content());
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
	showLoading(content());
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
