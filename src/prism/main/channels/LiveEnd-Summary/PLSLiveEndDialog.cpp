#include "PLSLiveEndDialog.h"
#include <QVariant>
#include <QMap>
#include "qfile.h"
#include "ui_PLSLiveEndDialog.h"
#include "QImage"
#include "PLSLiveEndItem.h"
#include "QDebug"
#include "QPushButton"
#include "QDesktopServices"
#include "QUrl"
#include "QGridLayout"
#include <frontend-api.h>
#include "log/log.h"
#include "PLSChannelDataAPI.h"
#include "ChannelConst.h"
#include <util/config-file.h>
#include "pls-app.hpp"
#include "PLSPlatformApi.h"

PLSLiveEndDialog::PLSLiveEndDialog(bool isRecord, QWidget *parent) : PLSDialogView(parent), ui(new Ui::PLSLiveEndDialog), m_bRecord(isRecord)
{
	ui->setupUi(this->content());
	setWindowTitle(QString());
	setIsMoveInContent(true);
	setHasMinButton(false);
	setHasCloseButton(true);
	setCaptionHeight(40);
	setHasHLine(false);
	setResizeEnabled(false);

	this->setAttribute(Qt::WA_AlwaysShowToolTips, true);

	PLS_INFO(END_MODULE, "PLSEnd Dialog Show");

	this->setupFirstUI();
	ui->titleLabel->adjustSize();
	connect(ui->okButton, &QPushButton::clicked, this, &PLSLiveEndDialog::okButtonClicked);
	connect(ui->savePushButton, &QPushButton::clicked, this, &PLSLiveEndDialog::openFileSavePath);
}

PLSLiveEndDialog::~PLSLiveEndDialog()
{
	delete ui;
}

void PLSLiveEndDialog::setupFirstUI()
{
	const static QString placeholderString = "       ";
	bool recordWhenStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	ui->saveFrame->setHidden(!recordWhenStreaming && !m_bRecord);

	QString appendString = getRecordPath();
	appendString = appendString.append(placeholderString);
	QFontMetrics fontWidth(ui->savePushButton->font());
	QString pathString = fontWidth.elidedText(appendString, Qt::ElideRight, 550);
	if (!pathString.endsWith(placeholderString)) {
		pathString = pathString.append(placeholderString);
	}
	ui->savePushButton->setText(pathString);
	ui->savePushButton->setToolTip(getRecordPath());

	ui->titleLabel->setText(tr(m_bRecord ? "broadcast.end.record" : "broadcast.end.live"));

	ui->okButton->setText(tr("OK"));

	setupScrollData();
}

void PLSLiveEndDialog::setupScrollData()
{

	const static int endItemHeight = 75;
	const static int windowWidth = 720;
	const static int maxCurrentShowCount = 3;
	const static int recordWidnowHeight = 235;
	const static int liveModeOffest = 5;

	if (m_bRecord) {
		ui->channelScroll->setHidden(true);
		this->setFixedSize(windowWidth, recordWidnowHeight);
		return;
	}

	mChannelVBoxLayout = static_cast<QVBoxLayout *>(ui->channelScroll->widget()->layout());
	ui->channelScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	int liveCount = PLS_PLATFORM_API->getUuidOnStarted().size();
	for (const auto &uid : PLS_PLATFORM_API->getUuidOnStarted()) {
		PLSLiveEndItem *endItem = new PLSLiveEndItem(uid, this);
		this->mChannelVBoxLayout->addWidget(endItem);
	}
	int windowHeight = recordWidnowHeight + liveModeOffest;
	if (liveCount > maxCurrentShowCount) {
		liveCount = maxCurrentShowCount;
	}

	if (liveCount != 0) {
		windowHeight += liveCount * (endItemHeight);
	} else {
		ui->channelScroll->setHidden(true);
	}

	this->resize(windowWidth, windowHeight);
}

void PLSLiveEndDialog::okButtonClicked()
{
	PLS_UI_STEP(END_MODULE, "PLSEnd Dialog OK Button Click", ACTION_CLICK);
	this->close();
}

void PLSLiveEndDialog::openFileSavePath()
{
	QUrl videoPath = QUrl::fromLocalFile(getRecordPath());
	QDesktopServices::openUrl(videoPath);
}

const QString PLSLiveEndDialog::getRecordPath()
{

	const char *mode = pls_basic_config_get_string("Output", "Mode");
	bool adv = astrcmpi(mode, "Advanced") == 0;

	const char *path = pls_basic_config_get_string("SimpleOutput", "FilePath");
	if (adv) {
		path = pls_basic_config_get_string("AdvOut", "RecFilePath");
	}

	return QString(path);
}
