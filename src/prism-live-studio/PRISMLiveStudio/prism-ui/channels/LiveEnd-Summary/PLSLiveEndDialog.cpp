#include "PLSLiveEndDialog.h"
#include <frontend-api.h>
#include <util/config-file.h>
#include <QMap>
#include <QVariant>
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "PLSEndTipView.h"
#include "PLSLiveEndItem.h"
#include "PLSPlatformApi.h"
#include "QDebug"
#include "QDesktopServices"
#include "QGridLayout"
#include "QImage"
#include "QPushButton"
#include "QUrl"
#include "log/log.h"
#include "platform.hpp"

#include "pls-channel-const.h"
#include "qfile.h"
#include "ui_PLSLiveEndDialog.h"

PLSLiveEndDialog::PLSLiveEndDialog(PLSEndPageType pageType, QWidget *parent) : PLSDialogView(parent), m_pageType(pageType)
{
	ui = pls_new<Ui::PLSLiveEndDialog>();

	pls_add_css(this, {"PLSLiveEndDialog"});
	setupUi(ui);
	setWindowTitle(QString());
	setHasMinButton(false);
	setHasCloseButton(true);
	setHasHLine(false);
	setResizeEnabled(false);
	this->setAttribute(Qt::WA_AlwaysShowToolTips, true);
	addMacTopMargin();

	PLS_INFO(END_MODULE, "PLSEnd Dialog Show");

	this->setupFirstUI();
	ui->titleLabel->adjustSize();
	connect(ui->okButton, &QPushButton::clicked, this, &PLSLiveEndDialog::okButtonClicked);
	connect(ui->savePushButton, &QPushButton::clicked, this, &PLSLiveEndDialog::openFileSavePath);
}

PLSLiveEndDialog::~PLSLiveEndDialog()
{
	PLS_INFO(END_MODULE, "PLSEnd Dialog is uniniting");
	pls_delete(ui);
}

void PLSLiveEndDialog::setupFirstUI()
{
	const static QString placeholderString = "       ";
	bool recordWhenStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	bool saveWidgetHidden = true;
	if (recordWhenStreaming) {
		saveWidgetHidden = false;
	} else if (m_pageType == PLSEndPageType::PLSRecordPage) {
		saveWidgetHidden = false;
	}
	ui->saveWidget->setHidden(saveWidgetHidden);

	bool rehearsalWidgetHidden = true;
	if (m_pageType == PLSEndPageType::PLSRehearsalPage) {
		rehearsalWidgetHidden = false;
	}
	QSizePolicy sizePolicy = ui->Rehearsalwidget->sizePolicy();
	sizePolicy.setRetainSizeWhenHidden(false);
	ui->Rehearsalwidget->setSizePolicy(sizePolicy);
	ui->rehearsalLabel->setText(tr("navershopping.liveinfo.rehearsal.endpage.content"));
	ui->Rehearsalwidget->setHidden(rehearsalWidgetHidden);

	auto notifyDpiChanged = [this]() {
		QString appendString = getRecordPath();
		appendString = appendString.append(placeholderString);
		QFontMetrics fontWidth(ui->savePushButton->font());
		QString pathString = fontWidth.elidedText(appendString, Qt::ElideRight, 550);
		if (!pathString.endsWith(placeholderString)) {
			pathString = pathString.append(placeholderString);
		}
		ui->savePushButton->setText(pathString);
		ui->savePushButton->setToolTip(getRecordPath());
	};

	notifyDpiChanged();
	if (m_pageType == PLSEndPageType::PLSRecordPage) {
		ui->titleLabel->setText(tr("broadcast.end.record"));
	} else if (m_pageType == PLSEndPageType::PLSRehearsalPage) {
		ui->titleLabel->setText(tr("broadcast.endpage.rehearsal.title"));
	} else if (m_pageType == PLSEndPageType::PLSLivingPage) {
		ui->titleLabel->setText(tr("broadcast.end.live"));
	}

#if defined(Q_OS_MACOS)
	if (m_pageType == PLSEndPageType::PLSRecordPage) {
		setWindowTitle(tr("broadcast.end.record.mac.title"));
	} else if (m_pageType == PLSEndPageType::PLSRehearsalPage) {
		setWindowTitle(tr("broadcast.endpage.rehearsal.mac.title"));
	} else if (m_pageType == PLSEndPageType::PLSLivingPage) {
		setWindowTitle(tr("broadcast.end.live.mac.title"));
	}
#endif

	ui->okButton->setText(tr("OK"));
	setupScrollData();
}

void PLSLiveEndDialog::setupScrollData()
{

	if (m_pageType == PLSEndPageType::PLSRecordPage) {
		ui->channelScroll->setHidden(true);
		initSize(windowWidth, pls_get_platform_window_height_by_windows_height(recordWidnowHeight));
		ui->placehoderLabel->setFixedHeight(0);
		return;
	}

	if (m_pageType == PLSEndPageType::PLSRehearsalPage) {
		ui->channelScroll->setHidden(true);
		bool recordWhenStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
		int rehearsalHeight = 235;
		if (recordWhenStreaming) {
			rehearsalHeight = 261;
		}
		initSize(windowWidth, pls_get_platform_window_height_by_windows_height(rehearsalHeight));
		ui->placehoderLabel->setFixedHeight(0);
		return;
	}

	mChannelVBoxLayout = static_cast<QVBoxLayout *>(ui->channelScroll->widget()->layout());
	ui->channelScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

#if 0
	std::list<QString> uuids;
	const auto &cMap =  PLSCHANNELS_API->getCurrentSelectedChannels();
	for (const auto &map : cMap) {
		uuids.emplace_back(map[ChannelData::g_channelUUID].toString());
	}
#else
	const auto &uuids = PLS_PLATFORM_API->getUuidOnStarted();
#endif

	for (const auto &uid : uuids) {

		auto endItem = pls_new<PLSLiveEndItem>(uid, this);
		connect(endItem, &PLSLiveEndItem::tipButtonClick, this, &PLSLiveEndDialog::openTipView);
		this->mChannelVBoxLayout->addWidget(endItem);
	}

	auto tmpLiveCount = int(uuids.size());
	bool isExceendMax = tmpLiveCount > maxCurrentShowCount;
	ui->channelScroll->setVerticalScrollBarPolicy(isExceendMax ? Qt::ScrollBarPolicy::ScrollBarAlwaysOn : Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
	ui->channelScroll->verticalScrollBar()->setEnabled(isExceendMax ? true : false);

	updateOnDPIChanged(tmpLiveCount, 1, isExceendMax);
}

void PLSLiveEndDialog::updateOnDPIChanged(int tmpLiveCount, double dpi, bool isExceendMax)
{
	int liveCount = tmpLiveCount;

	int windowHeight = recordWidnowHeight;
	bool isExeend = false;
	if (isExceendMax) {
		liveCount = maxCurrentShowCount;
		isExeend = true;
	}

	if (liveCount != 0) {
		int scrolHeight = liveCount * endItemHeight;
		int dpiLiveModeOffest = isExeend ? 0 : liveModeOffest;
		windowHeight += scrolHeight;
		ui->channelScroll->setFixedHeight(scrolHeight - dpiLiveModeOffest);
		ui->placehoderLabel->setFixedHeight(dpiLiveModeOffest);
	} else {
		ui->channelScroll->setHidden(true);
	}
	initSize(windowWidth, pls_get_platform_window_height_by_windows_height(windowHeight));

	QMetaObject::invokeMethod(
		this,
		[this] {
			for (QObject *child : ui->channelScroll->widget()->children()) {
				if (!child->isWidgetType()) {
					continue;
				}
				if (auto *childWidget = dynamic_cast<PLSLiveEndItem *>(child); childWidget) {
					childWidget->setNameElideString();
				}
			}
		},
		Qt::QueuedConnection);
}

void PLSLiveEndDialog::okButtonClicked()
{
	PLS_UI_STEP(END_MODULE, "PLSEnd Dialog OK Button Click", ACTION_CLICK);
	this->close();
}

void PLSLiveEndDialog::openFileSavePath() const
{
	QUrl videoPath = QUrl::fromLocalFile(getRecordPath());
	PLS_UI_STEP(END_MODULE, "PLSEnd Dialog openFileSavePath Click", ACTION_CLICK);
	if (!QDesktopServices::openUrl(videoPath.toString())) {
		PLS_WARN(END_MODULE, "openFileSavePath failed");
	}
}

void PLSLiveEndDialog::openTipView()
{
	PLSEndTipView tipView(this);
	tipView.exec();
}

QString PLSLiveEndDialog::getRecordPath() const
{

	const char *mode = pls_basic_config_get_string("Output", "Mode");
	bool adv = astrcmpi(mode, "Advanced") == 0;

	const char *path = pls_basic_config_get_string("SimpleOutput", "FilePath");
	if (adv) {
		path = pls_basic_config_get_string("AdvOut", "RecFilePath");
	}

	return QString(path);
}
