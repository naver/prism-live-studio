#include "PLSRtmpChannelView.h"
#include <QComboBox>
#include <QListView>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QUrl>
#include <QValidator>
#include "ChannelCommonFunctions.h"
#include "LogPredefine.h"
#include "pls-channel-const.h"

#include "PLSChannelDataAPI.h"

#include "PLSComboBox.h"
#include "ResolutionGuidePage.h"

#include "PLSAlertView.h"
#include "obs-app.hpp"
#include "obs.h"
#include "pls-gpop-data.hpp"
#include "pls-net-url.hpp"
#include "ui_PLSRtmpChannelView.h"

using namespace ChannelData;

PLSRtmpChannelView::PLSRtmpChannelView(const QVariantMap &oldData, QWidget *parent) : PLSDialogView(parent), ui(new Ui::RtmpChannelView), mOldData(oldData)
{

	pls_add_css(this, {"PLSRTMPChannelView", "PLSLiveInfoBase"});
	initUi();
	loadFromData(oldData);
	auto flushEdit = [this](bool firstShow) {
		if (firstShow) {
			QMetaObject::invokeMethod(
				this,
				[this] {
					pls_flush_style(ui->UserIDEdit);
					pls_flush_style(ui->UserPasswordEdit);
				},
				Qt::QueuedConnection);
		}
	};
	flushEdit(true);
	UpdateTwitchServerList();
	setTwitchUI(ui->PlatformCombbox->currentData().toString());
	updateSaveBtnAvailable();
}

PLSRtmpChannelView::~PLSRtmpChannelView()
{
	delete ui;
}

void PLSRtmpChannelView::initUi()
{
	this->setupUi(ui);
	ui->MenuFrame->hide();
	this->setHasCloseButton(false);
	setFixedSize(720, 710);
	auto btnsWidget = ResolutionGuidePage::createResolutionButtonsFrame(this);
	ui->horizontalLayout_8->addWidget(btnsWidget);
	ui->horizontalLayout_8->setAlignment(btnsWidget, Qt::AlignRight);

	languageChange();
	updateRtmpInfos();
	initCommbox();

	ui->UserPasswordEdit->installEventFilter(this);
	ui->StreamKeyEdit->installEventFilter(this);

#if defined(Q_OS_MACOS)
	ui->horizontalLayout->addWidget(ui->SaveBtn);
#endif
	connect(ui->NameEdit, &QLineEdit::textEdited, this, &PLSRtmpChannelView::updateSaveBtnAvailable, Qt::QueuedConnection);
	connect(ui->StreamKeyEdit, &QLineEdit::textEdited, this, &PLSRtmpChannelView::updateSaveBtnAvailable, Qt::QueuedConnection);
	connect(ui->RTMPUrlEdit, &QLineEdit::textEdited, this, &PLSRtmpChannelView::updateSaveBtnAvailable, Qt::QueuedConnection);
	connect(ui->UserIDEdit, &QLineEdit::textEdited, this, &PLSRtmpChannelView::updateSaveBtnAvailable, Qt::QueuedConnection);
	connect(ui->UserPasswordEdit, &QLineEdit::textEdited, this, &PLSRtmpChannelView::updateSaveBtnAvailable, Qt::QueuedConnection);
}

QVariantMap PLSRtmpChannelView::SaveResult() const
{
	if (m_type == SRT || m_type == RIST) {
		auto tmpData = mOldData;
		tmpData[g_channelRtmpUrl] = ui->RTMPUrlEdit->text().trimmed();
		tmpData[g_streamKey] = ui->StreamKeyEdit->text();
		if (m_type == SRT) {
			tmpData[g_data_type] = SRTType;
			tmpData[g_channelName] = CUSTOM_SRT;
		} else {
			tmpData[g_data_type] = RISTType;
			tmpData[g_channelName] = CUSTOM_RIST;
		}
		tmpData[g_nickName] = ui->NameEdit->text();
		QString userID = ui->UserIDEdit->text();
		tmpData[g_rtmpUserID] = userID;
		QString password = ui->UserPasswordEdit->text();
		tmpData[g_password] = password;
		return tmpData;
	}
	auto tmpData = mOldData;
	tmpData[g_nickName] = ui->NameEdit->text();
	tmpData[g_channelRtmpUrl] = ui->RTMPUrlEdit->text().trimmed();
	tmpData[g_streamKey] = ui->StreamKeyEdit->text();
	QString platfromName;

	if (ui->PlatformCombbox->currentIndex() == 0 || ui->PlatformCombbox->currentData().toString() == CHANNELS_TR(UserInputRTMP)) {
		platfromName = CUSTOM_RTMP;
	} else {
		platfromName = ui->PlatformCombbox->currentData().toString();
	}

	tmpData[g_channelName] = platfromName;

	QString userID = ui->UserIDEdit->text();
	tmpData[g_rtmpUserID] = userID;
	QString password = ui->UserPasswordEdit->text();
	tmpData[g_password] = password;

	if (ui->PlatformCombbox->currentData().toString() == TWITCH) {
		QString text = QTStr("setting.output.server.auto");
		if (ui->ServerComboBox->currentText() == text) {
			tmpData[g_isTwitchRtmpServerAuto] = true;
		} else {
			tmpData[g_isTwitchRtmpServerAuto] = false;
		}
	}

	return tmpData;
}

void PLSRtmpChannelView::updatePlatform(const QVariantMap &oldData)
{
	QString platform = getInfo(oldData, g_channelName);
	int index = ui->PlatformCombbox->findData(platform, Qt::DisplayRole, Qt::MatchContains);
	ui->PlatformCombbox->setDisabled(true);
	if (index != -1) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentIndex(index);
	}

	auto type = getInfo(oldData, g_data_type, RTMPType);
	if (type == RTMPType && platform == CUSTOM_RTMP) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		int index = ui->PlatformCombbox->findData(CHANNELS_TR(UserInputRTMP));
		ui->PlatformCombbox->setCurrentIndex(index);
		m_type = RTMP;
	} else if (type == SRTType) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		int index = ui->PlatformCombbox->findData(CHANNELS_TR(UserInputSRT));
		ui->PlatformCombbox->setCurrentIndex(index);
		m_type = SRT;
	} else if (type == RISTType) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		int index = ui->PlatformCombbox->findData(CHANNELS_TR(UserInputRIST));
		ui->PlatformCombbox->setCurrentIndex(index);
		m_type = RIST;
	}
}

void PLSRtmpChannelView::loadFromData(const QVariantMap &oldData)
{
	auto uuid = getInfo(oldData, g_channelUUID);
	ui->CategoryLabel->setText(tr("Channels.RTMadd.Catogry"))->setUUID(uuid);
	isEdit = getInfo(oldData, g_isUpdated, false);
	this->setWindowTitle(isEdit ? CHANNELS_TR(RTMPEdit) : CHANNELS_TR(RTMPadd.titlebar));
	if (!isEdit) {
		return;
	}

	updatePlatform(oldData);
	if (m_type == SRT || m_type == RIST) {
		IsHideSomeFrame(true);
	} else {
		IsHideSomeFrame(false);
	}

	UpdateTwitchServerList();
	QString displayName = getInfo(oldData, g_nickName);
	ui->TitleLabel->setText(CHANNELS_TR(RTMPEdit));
	ui->NameEdit->setText(displayName);
	ui->NameEdit->setModified(true);

	QSignalBlocker block(ui->RTMPUrlEdit);
	ui->RTMPUrlEdit->setEnabled(false);
	QString rtmpUrl = getInfo(oldData, g_channelRtmpUrl);
	ui->RTMPUrlEdit->setText(rtmpUrl);
	if (ui->PlatformCombbox->currentData().toString() == TWITCH) {
		bool bServerAuto = getInfo(oldData, g_isTwitchRtmpServerAuto, false);
		if (bServerAuto) {
			ui->RTMPUrlEdit->setText(ui->ServerComboBox->currentData().toString());
		}
	}
	ui->RTMPUrlEdit->setModified(false);
	QString streamKey = getInfo(oldData, g_streamKey);
	ui->StreamKeyEdit->setText(streamKey);

	QString userID = getInfo(oldData, g_rtmpUserID);
	ui->UserIDEdit->setText(userID);
	QString password = getInfo(oldData, g_password);
	ui->UserPasswordEdit->setText(password);
	pls_async_call_mt([this, oldData]() { ResolutionGuidePage::checkResolution(this, getInfo(oldData, g_channelUUID)); });
}

void PLSRtmpChannelView::showResolutionGuide()
{
	ResolutionGuidePage::showResolutionGuideCloseAfterChange(this);
}

void PLSRtmpChannelView::setPlatformCombboxIndex(const QString &channleName)
{
	int index = ui->PlatformCombbox->findData(channleName);
	ui->PlatformCombbox->setCurrentIndex(index);
}

bool PLSRtmpChannelView::eventFilter(QObject *watched, QEvent *event)
{
	auto parent = dynamic_cast<QWidget *>(watched)->parentWidget();
	switch (event->type()) {
	case QEvent::FocusIn:
		parent->setProperty("isFocus", true);
		refreshStyle(parent);
		break;
	case QEvent::FocusOut:
		parent->setProperty("isFocus", false);
		refreshStyle(parent);
		break;
	default:
		break;
	}
	return false;
}
void PLSRtmpChannelView::on_SaveBtn_clicked()
{
	PRE_LOG_UI_MSG("save clicked", PLSRtmpChannelView)
	QSignalBlocker blocker(ui->NameEdit);
	if (!verifyRename()) {
		return;
	}
	if (checkIsModified()) {
		this->accept();
		return;
	}
	this->reject();
}

void PLSRtmpChannelView::on_CancelBtn_clicked()
{
	PRE_LOG_UI_MSG("cancle clicked", PLSRtmpChannelView)
	this->reject();
}
void PLSRtmpChannelView::on_StreamKeyVisible_toggled(bool isCheck)
{
	PRE_LOG_UI_MSG("StreamKeyVisible clicked", PLSRtmpChannelView)
	ui->StreamKeyEdit->setEchoMode(isCheck ? QLineEdit::Password : QLineEdit::Normal);
}
void PLSRtmpChannelView::on_PasswordVisible_toggled(bool isCheck)
{
	PRE_LOG_UI_MSG("PasswordVisible clicked", PLSRtmpChannelView)
	ui->UserPasswordEdit->setEchoMode(isCheck ? QLineEdit::Password : QLineEdit::Normal);
}

void PLSRtmpChannelView::on_RTMPUrlEdit_textChanged(const QString &rtmpUrl)
{
	if (m_type == SRT || m_type == RIST) {
		updateSaveBtnAvailable();
		return;
	}

	if (ui->PlatformCombbox->currentIndex() == 0) {
		QSignalBlocker bloker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentText(CHANNELS_TR(UserInputRTMP));
	}

	auto platformStr = guessPlatformFromRTMP(rtmpUrl.trimmed());
	if (platformStr != BAND && platformStr != NOW && platformStr != CUSTOM_RTMP && platformStr != CHZZK && platformStr != NAVER_SHOPPING_LIVE) {
		if (platformStr == AFREECATV) {
			platformStr = TR_AFREECATV;
		}
		QSignalBlocker bloker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentText(platformStr);
		m_type = OTHER;
		IsHideSomeFrame(false);
		setTwitchUI(platformStr);
	} else if (platformStr == CUSTOM_RTMP) {
		QSignalBlocker bloker(ui->PlatformCombbox);
		int index = 0;
		if (isUrlRight("^srt?://\\w+", rtmpUrl.trimmed())) {
			index = ui->PlatformCombbox->findData(CHANNELS_TR(UserInputSRT));
			m_type = SRT;
			platformStr = CUSTOM_SRT;
			IsHideSomeFrame(true);
		} else if (isUrlRight("^rist?://\\w+", rtmpUrl.trimmed())) {
			index = ui->PlatformCombbox->findData(CHANNELS_TR(UserInputRIST));
			m_type = RIST;
			platformStr = CUSTOM_RIST;
			IsHideSomeFrame(true);
		} else {
			index = ui->PlatformCombbox->findData(CHANNELS_TR(UserInputRTMP));
			m_type = RTMP;
			IsHideSomeFrame(false);
		}
		ui->PlatformCombbox->setCurrentIndex(index);
	} else {
		platformStr = CUSTOM_RTMP;
		m_type = RTMP;
		IsHideSomeFrame(false);
	}
	if (ui->NameEdit->text().isEmpty() || !ui->NameEdit->isModified()) {
		QSignalBlocker bloker(ui->NameEdit);
		ui->NameEdit->setText(platformStr);
		ui->NameEdit->setModified(false);
	}

	updateSaveBtnAvailable();
}

void PLSRtmpChannelView::on_PlatformCombbox_currentTextChanged(const QString &showText)
{
	QString platForm = ui->PlatformCombbox->currentData().toString();
	PRE_LOG_UI_MSG(QString("PlatformCombbox clicked to:" + platForm).toUtf8().constData(), PLSRtmpChannelView)
	ResolutionGuidePage::checkResolutionForPlatform(this, platForm, channel_data::ChannelDataType::RTMPType);
	if (platForm == CHANNELS_TR(UserInputRTMP) || platForm == CHANNELS_TR(UserInputSRT) || ui->PlatformCombbox->currentIndex() == 0 || platForm == CHANNELS_TR(UserInputRIST)) {

		ui->RTMPUrlEdit->setEnabled(true);

		QSignalBlocker block(ui->RTMPUrlEdit);
		ui->RTMPUrlEdit->clear();
		ui->StreamKeyEdit->clear();

		bool bmodify = ui->NameEdit->isModified();
		QSignalBlocker blockName(ui->NameEdit);
		if (platForm == CHANNELS_TR(UserInputRTMP)) {
			if (!bmodify) {
				ui->NameEdit->setText(CUSTOM_RTMP);
			}
			m_type = RTMP;
			IsHideSomeFrame(false);
		} else if (platForm == CHANNELS_TR(UserInputSRT)) {
			if (!bmodify) {
				ui->NameEdit->setText(CUSTOM_SRT);
			}
			m_type = SRT;
			IsHideSomeFrame(true);
		} else if (platForm == CHANNELS_TR(UserInputRIST)) {
			if (!bmodify) {
				ui->NameEdit->setText(CUSTOM_RIST);
			}
			m_type = RIST;
			IsHideSomeFrame(true);
		} else {
			m_type = OTHER;
			ui->NameEdit->clear();
			IsHideSomeFrame(false);
		}
		ui->ServerFrame->hide();
		ui->ServerComboBox->hide();
		updateSaveBtnAvailable();
		return;
	}
	m_type = OTHER;
	IsHideSomeFrame(false);
	ui->RTMPUrlEdit->setEnabled(false);
	if (ui->NameEdit->text().isEmpty() || !ui->NameEdit->isModified()) {
		QSignalBlocker block(ui->NameEdit);
		ui->NameEdit->setText(showText);
		ui->NameEdit->setModified(false);
	}

	if (auto retIte = mRtmps.find(platForm); retIte != mRtmps.end()) {
		QSignalBlocker block(ui->RTMPUrlEdit);
		ui->RTMPUrlEdit->setText(retIte.value());
		ui->RTMPUrlEdit->setModified(false);
	}
	if (platForm == TWITCH) {
		ui->ServerFrame->show();
		ui->ServerComboBox->show();
		if (GlobalVars::g_bUseAPIServer) {
			QSignalBlocker block(ui->RTMPUrlEdit);
			ui->RTMPUrlEdit->setText(ui->ServerComboBox->currentData().toString());
			ui->RTMPUrlEdit->setModified(false);
		}
	} else {
		ui->ServerFrame->hide();
		ui->ServerComboBox->hide();
	}
	updateSaveBtnAvailable();
}

void PLSRtmpChannelView::on_ServerComboBox_currentTextChanged(const QString &text)
{
	auto platform = ui->PlatformCombbox->currentData().toString();
	if (platform != TWITCH) {
		PLS_INFO("PLSRtmpChannelView", "current platform is %s", platform.toUtf8().constData());
		return;
	}
	ui->RTMPUrlEdit->setEnabled(false);
	auto url = ui->ServerComboBox->currentData().toString();
	if (!url.isEmpty()) {
		QSignalBlocker block(ui->RTMPUrlEdit);
		if (url == "auto" && !GlobalVars::g_bUseAPIServer) {
			url = mRtmps.value(platform);
		}
		PLS_INFO("PLSRtmpChannelView", "current twitch server is %s", url.toUtf8().constData());
		ui->RTMPUrlEdit->setText(url);
		ui->RTMPUrlEdit->setModified(false);
	}
	updateSaveBtnAvailable();
}

void PLSRtmpChannelView::on_OpenLink_clicked() const
{
	PRE_LOG_UI_MSG("open link clicked", PLSRtmpChannelView)
	QString urlStr;
	QString lang = pls_get_current_country_short_str().toUpper();
	if (lang == "KR") {
		urlStr = g_streamKeyPrismHelperKr;
	} else {
		urlStr = g_streamKeyPrismHelperEn;
	}

	if (!QDesktopServices::openUrl(urlStr)) {
		PRE_LOG(" error open url " + urlStr, ERROR)
	}
}

void PLSRtmpChannelView::updateSaveBtnAvailable()
{
	ui->SaveBtn->setEnabled(isInfoValid());
}
void PLSRtmpChannelView::languageChange()
{
	ui->UserIDEdit->setPlaceholderText(CHANNELS_TR(Optional));
	ui->UserPasswordEdit->setPlaceholderText(CHANNELS_TR(Optional));
}

void PLSRtmpChannelView::initCommbox()
{
	QStringList rtmpNames;
	rtmpNames += TR_SELECT;
	rtmpNames << mPlatforms << CHANNELS_TR(UserInputRTMP) << CHANNELS_TR(UserInputSRT) << CHANNELS_TR(UserInputRIST);
	for (QString &name : rtmpNames) {
		ui->PlatformCombbox->addItem(name == AFREECATV ? SOOP : name, name);
	}

	auto view = dynamic_cast<QListView *>(ui->PlatformCombbox->view());
	view->setRowHidden(0, true);
	ui->PlatformCombbox->setMaxVisibleItems(6);
	ui->PlatformCombbox->update();
}

bool PLSRtmpChannelView::verifyRename()
{
	const auto &infos = PLSCHANNELS_API->getAllChannelInfoReference();
	auto myName = ui->NameEdit->text();
	auto isSameName = [&](const QVariantMap &info) {
		auto name = getInfo(info, g_nickName);
		if (isEdit) {
			auto uuid = getInfo(info, g_channelUUID);
			auto oldUUid = getInfo(mOldData, g_channelUUID);
			return name == myName && uuid != oldUUid;
		}
		bool ret = (name == myName);
		return ret;
	};
	if (auto ite = std::find_if(infos.cbegin(), infos.cend(), isSameName); ite != infos.end()) {
		PLSAlertView::warning(this, tr("Alert.Title"), tr("setting.channel.rtmp.existname"));
		return false;
	}
	return true;
}

void PLSRtmpChannelView::ValidateNameEdit()
{
	static QRegularExpression regx("[\\r\\n]");
	auto txt = ui->NameEdit->text();
	if (txt.contains(regx)) {
		txt.remove(regx);
		QSignalBlocker bloker(ui->NameEdit);
		auto cursor = ui->NameEdit->cursorPosition();
		ui->NameEdit->setText(txt);
		ui->NameEdit->setCursorPosition(cursor);
	}
}

void PLSRtmpChannelView::IsHideSomeFrame(bool bShow)
{
	if ((m_type == SRT || m_type == RIST) && bShow) {
		ui->EditAreaFrame->show();
		ui->MustBeLabelStreamKey->hide();
		ui->OpenLink->hide();
	} else if (m_type == OTHER && !bShow) {
		ui->EditAreaFrame->hide();
		ui->MustBeLabelStreamKey->show();
		ui->OpenLink->show();
	} else {
		ui->EditAreaFrame->show();
		ui->MustBeLabelStreamKey->show();
		ui->OpenLink->show();
	}
}

bool PLSRtmpChannelView::isInfoValid()
{
	auto url = ui->RTMPUrlEdit->text().trimmed();
	if (m_type == SRT || m_type == RIST) {
		if (ui->NameEdit->text().isEmpty() || ui->RTMPUrlEdit->text().isEmpty()) {
			return false;
		}
		if (m_type == SRT) {
			return isUrlRight("^srt?://\\w+", url);
		} else {
			return isUrlRight("^rist?://\\w+", url);
		}
	}

	ValidateNameEdit();
	if (ui->NameEdit->text().isEmpty() || ui->RTMPUrlEdit->text().isEmpty() || ui->StreamKeyEdit->text().isEmpty()) {
		return false;
	}

	if (!isUrlRight("^rtmp[s]?://\\w+", url)) {
		return false;
	}
	return true;
}

bool PLSRtmpChannelView::checkIsModified() const
{
	auto tmp = SaveResult();
	return tmp != mOldData;
}

void PLSRtmpChannelView::updateRtmpInfos()
{
	mRtmps = PLSCHANNELS_API->getRTMPInfos();
	mPlatforms = PLSCHANNELS_API->getRTMPsName();
}

bool PLSRtmpChannelView::isUrlRight(const QString &regular, const QString &url) const
{
	QRegularExpression reg(regular);
	reg.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
	auto matchRe = reg.match(url);
	return matchRe.hasMatch();
}

void PLSRtmpChannelView::UpdateTwitchServerList()
{
	if (ui->ServerComboBox->count() != 0) {
		return;
	}
	auto serverList = initTwitchServer();
	QSignalBlocker block(ui->ServerComboBox);
	if (serverList.isEmpty()) {
		ui->ServerComboBox->addItem(QTStr("setting.output.server.auto"), mRtmps.value(TWITCH));
	} else {

		if (!GlobalVars::g_bUseAPIServer && serverList.at(0).second == "auto") {
			serverList.replace(0, QPair<QString, QString>(QTStr("setting.output.server.auto"), mRtmps.value(TWITCH)));
		}
		for (auto pair : serverList) {
			ui->ServerComboBox->addItem(pair.first, pair.second);
		}
	}
}

void PLSRtmpChannelView::setTwitchUI(const QString &channelName)
{
	if (channelName == TWITCH) {
		auto text = ui->RTMPUrlEdit->text();
		int index = ui->ServerComboBox->findData(text);
		bool bServerAuto = getInfo(mOldData, g_isTwitchRtmpServerAuto, false);
		if (index == 0 && !bServerAuto) {
			index = 1;
		}
		if (index == -1) {
			index = 0;
		}
		ui->ServerComboBox->setCurrentIndex(index);
		ui->ServerFrame->show();
		ui->ServerComboBox->show();
	} else {
		ui->ServerFrame->hide();
		ui->ServerComboBox->hide();
	}
}
