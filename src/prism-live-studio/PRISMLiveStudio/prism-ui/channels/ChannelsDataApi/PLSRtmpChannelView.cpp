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

#include "pls-gpop-data.hpp"
#include "pls-net-url.hpp"
#include "ui_PLSRtmpChannelView.h"

using namespace ChannelData;

PLSRtmpChannelView::PLSRtmpChannelView(const QVariantMap &oldData, QWidget *parent) : PLSDialogView(parent), ui(new Ui::RtmpChannelView), mOldData(oldData)
{

	pls_add_css(this, {"PLSRTMPChannelView"});
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
			tmpData[g_platformName] = CUSTOM_SRT;
		} else {
			tmpData[g_data_type] = RISTType;
			tmpData[g_platformName] = CUSTOM_RIST;
		}
		tmpData[g_nickName] = ui->NameEdit->text();
		return tmpData;
	}
	auto tmpData = mOldData;
	tmpData[g_nickName] = ui->NameEdit->text();
	tmpData[g_channelRtmpUrl] = ui->RTMPUrlEdit->text().trimmed();
	tmpData[g_streamKey] = ui->StreamKeyEdit->text();
	QString platfromName;

	if (ui->PlatformCombbox->currentIndex() == 0 || ui->PlatformCombbox->currentText() == CHANNELS_TR(UserInputRTMP)) {
		platfromName = CUSTOM_RTMP;
	} else {
		platfromName = ui->PlatformCombbox->currentText();
	}

	tmpData[g_platformName] = platfromName;

	QString userID = ui->UserIDEdit->text();
	tmpData[g_rtmpUserID] = userID;
	QString password = ui->UserPasswordEdit->text();
	tmpData[g_password] = password;

	return tmpData;
}

void PLSRtmpChannelView::updatePlatform(const QVariantMap &oldData)
{
	QString platform = getInfo(oldData, g_platformName);
	int index = ui->PlatformCombbox->findText(platform, Qt::MatchContains);
	ui->PlatformCombbox->setDisabled(true);
	if (index != -1) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentIndex(index);
	}

	auto type = getInfo(oldData, g_data_type, RTMPType);
	if (type == RTMPType && platform == CUSTOM_RTMP) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		int index = ui->PlatformCombbox->findText(CHANNELS_TR(UserInputRTMP));
		ui->PlatformCombbox->setCurrentIndex(index);
		m_type = RTMP;
	} else if (type == SRTType) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		int index = ui->PlatformCombbox->findText(CHANNELS_TR(UserInputSRT));
		ui->PlatformCombbox->setCurrentIndex(index);
		m_type = SRT;
	} else if (type == RISTType) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		int index = ui->PlatformCombbox->findText(CHANNELS_TR(UserInputRIST));
		ui->PlatformCombbox->setCurrentIndex(index);
		m_type = RIST;
	}
}

void PLSRtmpChannelView::loadFromData(const QVariantMap &oldData)
{
	isEdit = getInfo(oldData, g_isUpdated, false);
	this->setWindowTitle(isEdit ? CHANNELS_TR(RTMPEdit) : CHANNELS_TR(RTMPadd.titlebar));
	if (!isEdit) {
		return;
	}

	updatePlatform(oldData);
	if (m_type == SRT || m_type == RIST) {
		QString rtmpUrl = getInfo(oldData, g_channelRtmpUrl);
		ui->RTMPUrlEdit->setEnabled(false);
		ui->RTMPUrlEdit->setText(rtmpUrl);
		QString streamKey = getInfo(oldData, g_streamKey);
		ui->StreamKeyEdit->setText(streamKey);
		QString displayName = getInfo(oldData, g_nickName);
		ui->NameEdit->setText(displayName);
		ui->NameEdit->setModified(true);
		ResolutionGuidePage::checkResolution(this, getInfo(oldData, g_channelUUID));
		IsHideSomeFrame(true);
		return;
	}
	IsHideSomeFrame(false);
	QString displayName = getInfo(oldData, g_nickName);
	ui->TitleLabel->setText(CHANNELS_TR(RTMPEdit));
	ui->NameEdit->setText(displayName);
	ui->NameEdit->setModified(true);

	ui->RTMPUrlEdit->setEnabled(false);
	QString rtmpUrl = getInfo(oldData, g_channelRtmpUrl);
	ui->RTMPUrlEdit->setText(rtmpUrl);
	QString streamKey = getInfo(oldData, g_streamKey);
	ui->StreamKeyEdit->setText(streamKey);

	QString userID = getInfo(oldData, g_rtmpUserID);
	ui->UserIDEdit->setText(userID);
	QString password = getInfo(oldData, g_password);
	ui->UserPasswordEdit->setText(password);

	ResolutionGuidePage::checkResolution(this, getInfo(oldData, g_channelUUID));
}

void PLSRtmpChannelView::showResolutionGuide()
{
	ResolutionGuidePage::showResolutionGuideCloseAfterChange(this);
}

void PLSRtmpChannelView::changeEvent(QEvent *e)
{
	QDialog::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		languageChange();
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
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
	verifyRename();
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
	if (platformStr != BAND && platformStr != NOW && platformStr != CUSTOM_RTMP) {
		QSignalBlocker bloker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentText(platformStr);
	} else if (platformStr == CUSTOM_RTMP) {
		QSignalBlocker bloker(ui->PlatformCombbox);
		int index = ui->PlatformCombbox->findText(CHANNELS_TR(UserInputRTMP));
		ui->PlatformCombbox->setCurrentIndex(index);
	} else {
		platformStr = CUSTOM_RTMP;
	}
	if (ui->NameEdit->text().isEmpty() || !ui->NameEdit->isModified()) {
		QSignalBlocker bloker(ui->NameEdit);
		ui->NameEdit->setText(platformStr);
		ui->NameEdit->setModified(false);
	}

	updateSaveBtnAvailable();
}

void PLSRtmpChannelView::on_PlatformCombbox_currentTextChanged(const QString &platForm)
{
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

		updateSaveBtnAvailable();
		return;
	}
	m_type = OTHER;
	IsHideSomeFrame(false);
	ui->RTMPUrlEdit->setEnabled(false);
	if (ui->NameEdit->text().isEmpty() || !ui->NameEdit->isModified()) {
		QSignalBlocker block(ui->NameEdit);
		ui->NameEdit->setText(platForm);
		ui->NameEdit->setModified(false);
	}

	if (auto retIte = mRtmps.find(platForm); retIte != mRtmps.end()) {
		QSignalBlocker block(ui->RTMPUrlEdit);
		ui->RTMPUrlEdit->setText(retIte.value());
		ui->RTMPUrlEdit->setModified(false);
	}
	updateSaveBtnAvailable();
}

void PLSRtmpChannelView::on_OpenLink_clicked() const
{
	PRE_LOG_UI_MSG("open link clicked", PLSRtmpChannelView)
	QString urlStr;
	QString lang = pls_get_current_country_short_str().toUpper();
	if (lang == "ID") {
		urlStr = g_streamKeyPrismHelperId;
	} else if (lang == "KR") {
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
	ui->PlatformCombbox->addItems(rtmpNames << mPlatforms << CHANNELS_TR(UserInputRTMP) << CHANNELS_TR(UserInputSRT) << CHANNELS_TR(UserInputRIST));
	auto view = dynamic_cast<QListView *>(ui->PlatformCombbox->view());
	view->setRowHidden(0, true);
	ui->PlatformCombbox->setMaxVisibleItems(6);
	ui->PlatformCombbox->update();
}

void PLSRtmpChannelView::verifyRename()
{
	const auto &infos = PLSCHANNELS_API->getAllChannelInfoReference();
	auto myName = ui->NameEdit->text();
	QRegularExpression regx("(-\\d+$)");
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
	static int countN = 1;

	if (auto ite = std::find_if(infos.cbegin(), infos.cend(), isSameName); ite != infos.end()) {
		myName.remove(regx);
		QString newName = myName + "-" + QString::number(countN) + QString("");
		if (newName.count() > ui->NameEdit->maxLength()) {
			return;
		}
		ui->NameEdit->setText(newName);
		++countN;
		verifyRename();
	}
	countN = 1;
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
		ui->EditAreaFrame->hide();
		ui->MustBeLabelStreamKey->hide();
		ui->OpenLink->hide();
	} else {
		ui->EditAreaFrame->show();
		ui->MustBeLabelStreamKey->show();
		ui->OpenLink->show();
	}
}

bool PLSRtmpChannelView::isInfoValid()
{

	if (m_type == SRT || m_type == RIST) {
		if (ui->NameEdit->text().isEmpty() || ui->RTMPUrlEdit->text().isEmpty()) {
			return false;
		}
		return true;
	}

	ValidateNameEdit();
	if (ui->NameEdit->text().isEmpty() || ui->RTMPUrlEdit->text().isEmpty() || ui->StreamKeyEdit->text().isEmpty()) {
		return false;
	}

	if (!isRtmUrlRight()) {
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

bool PLSRtmpChannelView::isRtmUrlRight() const
{
	QRegularExpression reg("^rtmp[s]?://\\w+");
	reg.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
	auto matchRe = reg.match(ui->RTMPUrlEdit->text().trimmed());
	return matchRe.hasMatch();
}
