#include "PLSRtmpChannelView.h"
#include <QComboBox>
#include <QListView>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QUrl>
#include <QValidator>
#include "ChannelCommonFunctions.h"
#include "ChannelConst.h"
#include "LogPredefine.h"

#include "PLSChannelDataAPI.h"
#include "PLSDpiHelper.h"
#include "ResolutionGuidePage.h"
#include "combobox.hpp"
#include "pls-app.hpp"
#include "pls-gpop-data.hpp"
#include "pls-net-url.hpp"
#include "ui_PLSRtmpChannelView.h"

using namespace ChannelData;

PLSRtmpChannelView::PLSRtmpChannelView(QVariantMap &oldData, QWidget *parent) : PLSDialogView(parent), ui(new Ui::RtmpChannelView), mOldData(oldData), isEdit(false)
{
	PLSDpiHelper dpiHelper;
	dpiHelper.setCss(this, {PLSCssIndex::PLSRTMPChannelView});

	initUi();
	loadFromData(oldData);

	dpiHelper.notifyDpiChanged(this, [this](double, double, bool firstShow) {
		if (firstShow) {
			QMetaObject::invokeMethod(
				this,
				[this] {
					pls_flush_style(ui->UserIDEdit);
					pls_flush_style(ui->UserPasswordEdit);
				},
				Qt::QueuedConnection);
		}
	});

	updateSaveBtnAvailable();
}

PLSRtmpChannelView::~PLSRtmpChannelView()
{
	delete ui;
}

void PLSRtmpChannelView::initUi()
{
	ui->setupUi(this->content());
	this->setHasCaption(false);
	this->setIsMoveInContent(true);
	QMetaObject::connectSlotsByName(this);

	auto btnsWidget = ResolutionGuidePage::createResolutionButtonsFrame(this);
	ui->horizontalLayout_8->addWidget(btnsWidget);
	ui->horizontalLayout_8->setAlignment(btnsWidget, Qt::AlignRight);
	setWindowFlag(Qt::FramelessWindowHint);
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

QVariantMap PLSRtmpChannelView::SaveResult()
{
	auto tmpData = mOldData;
	tmpData[g_nickName] = ui->NameEdit->text();
	tmpData[g_channelRtmpUrl] = ui->RTMPUrlEdit->text().trimmed();
	tmpData[g_streamKey] = ui->StreamKeyEdit->text();
	QString platfromName;

	if (ui->PlatformCombbox->currentIndex() == 0 || ui->PlatformCombbox->currentIndex() == ui->PlatformCombbox->count() - 1) {
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

void PLSRtmpChannelView::updatePlatform(const QString &platform)
{
	int index = ui->PlatformCombbox->findText(platform, Qt::MatchContains);
	ui->PlatformCombbox->setDisabled(true);
	if (index != -1) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentIndex(index);

	} else {
		QSignalBlocker blocker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentIndex(ui->PlatformCombbox->count() - 1);
	}
}

void PLSRtmpChannelView::loadFromData(const QVariantMap &oldData)
{
	isEdit = getInfo(oldData, g_isUpdated, false);
	if (!isEdit) {
		return;
	}

	QString platform = getInfo(oldData, g_platformName);
	updatePlatform(platform);

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
	PRE_LOG_UI_MSG("save clicked", PLSRtmpChannelView);
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
	PRE_LOG_UI_MSG("cancle clicked", PLSRtmpChannelView);
	this->reject();
}
void PLSRtmpChannelView::on_StreamKeyVisible_toggled(bool isCheck)
{
	PRE_LOG_UI_MSG("StreamKeyVisible clicked", PLSRtmpChannelView);
	ui->StreamKeyEdit->setEchoMode(isCheck ? QLineEdit::Password : QLineEdit::Normal);
}
void PLSRtmpChannelView::on_PasswordVisible_toggled(bool isCheck)
{
	PRE_LOG_UI_MSG("PasswordVisible clicked", PLSRtmpChannelView);
	ui->UserPasswordEdit->setEchoMode(isCheck ? QLineEdit::Password : QLineEdit::Normal);
}

void PLSRtmpChannelView::on_RTMPUrlEdit_textChanged(const QString &rtmpUrl)
{
	if (ui->PlatformCombbox->currentIndex() == 0) {
		QSignalBlocker bloker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentText(CHANNELS_TR(UserInput));
	}

	auto platformStr = guessPlatformFromRTMP(rtmpUrl.trimmed());
	if (platformStr != BAND && platformStr != NOW && platformStr != CUSTOM_RTMP) {
		QSignalBlocker bloker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentText(platformStr);
	} else if (platformStr == CUSTOM_RTMP) {
		QSignalBlocker bloker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentIndex(ui->PlatformCombbox->count() - 1);
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

void PLSRtmpChannelView::on_PlatformCombbox_currentIndexChanged(const QString &platForm)
{
	PRE_LOG_UI_MSG(QString("PlatformCombbox clicked to:" + platForm).toUtf8().constData(), PLSRtmpChannelView);
	if (ui->PlatformCombbox->currentIndex() == (ui->PlatformCombbox->count() - 1) || ui->PlatformCombbox->currentIndex() == 0) {

		ui->RTMPUrlEdit->setEnabled(true);

		if (!ui->RTMPUrlEdit->isModified()) {
			QSignalBlocker block(ui->RTMPUrlEdit);
			ui->RTMPUrlEdit->clear();
		}

		if (!ui->NameEdit->isModified()) {
			QSignalBlocker block(ui->NameEdit);
			if (ui->PlatformCombbox->currentIndex() == (ui->PlatformCombbox->count() - 1)) {
				ui->NameEdit->setText(CUSTOM_RTMP);
			} else {
				ui->NameEdit->clear();
			}
		}
		updateSaveBtnAvailable();
		return;
	}

	ui->RTMPUrlEdit->setEnabled(false);
	if (ui->NameEdit->text().isEmpty() || !ui->NameEdit->isModified()) {
		QSignalBlocker block(ui->NameEdit);
		ui->NameEdit->setText(platForm);
		ui->NameEdit->setModified(false);
	}

	auto retIte = mRtmps.find(platForm);
	if (retIte != mRtmps.end()) {
		QSignalBlocker block(ui->RTMPUrlEdit);
		ui->RTMPUrlEdit->setText(retIte.value());
		ui->RTMPUrlEdit->setModified(false);
	}
	updateSaveBtnAvailable();
}

void PLSRtmpChannelView::on_OpenLink_clicked()
{
	PRE_LOG_UI_MSG("open link clicked", PLSRtmpChannelView);
	QString urlStr;
	if (!IS_KR()) {
		urlStr = g_streamKeyPrismHelperEn;
	} else {
		urlStr = g_streamKeyPrismHelperKr;
	}

	if (!QDesktopServices::openUrl(urlStr)) {
		PRE_LOG(" error open url " + urlStr, ERROR);
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
	ui->PlatformCombbox->addItems(rtmpNames << mPlatforms << CHANNELS_TR(UserInput));
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
	auto ite = std::find_if(infos.cbegin(), infos.cend(), isSameName);
	if (ite != infos.end()) {
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

bool PLSRtmpChannelView::isInfoValid()
{
	ValidateNameEdit();
	if (ui->NameEdit->text().isEmpty() || ui->RTMPUrlEdit->text().isEmpty() || ui->StreamKeyEdit->text().isEmpty()) {
		return false;
	}
	if (!isRtmUrlRight()) {
		return false;
	}
	return true;
}

bool PLSRtmpChannelView::checkIsModified()
{
	auto tmp = SaveResult();
	return tmp != mOldData;
}

void PLSRtmpChannelView::updateRtmpInfos()
{
	mRtmps = PLSCHANNELS_API->getRTMPInfos();
	mPlatforms = PLSCHANNELS_API->getRTMPsName();
}

bool PLSRtmpChannelView::isRtmUrlRight()
{
	QRegularExpression reg("^rtmp[s]?://\\w+");
	reg.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
	auto matchRe = reg.match(ui->RTMPUrlEdit->text().trimmed());
	return matchRe.hasMatch();
}
