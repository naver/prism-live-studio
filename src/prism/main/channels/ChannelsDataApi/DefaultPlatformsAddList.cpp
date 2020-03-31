#include "DefaultPlatformsAddList.h"
#include "ui_DefaultPlatformsAddList.h"
#include "PLSChannelDataAPI.h"
#include "ChannelCommonFunctions.h"
#include "ChannelConst.h"
#include "PLSChannelsVirualAPI.h"
#include <QToolButton>

using namespace ChannelData;

DefaultPlatformsAddList::DefaultPlatformsAddList(QWidget *parent) : QFrame(parent), ui(new Ui::DefaultPlatformsAddList)
{
	ui->setupUi(this);
	initUi();
}

DefaultPlatformsAddList::~DefaultPlatformsAddList()
{
	delete ui;
}

void DefaultPlatformsAddList::runBtnCMD()
{
	auto btn = dynamic_cast<QToolButton *>(sender());
	auto cmdStr = getInfoOfObject(btn, g_channelName.toStdString().c_str(), QString("add"));
	runCMD(cmdStr);
}

void DefaultPlatformsAddList::changeEvent(QEvent *e)
{
	QFrame::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

bool DefaultPlatformsAddList::eventFilter(QObject *watched, QEvent *event)
{
	auto srcBtn = dynamic_cast<QToolButton *>(watched);
	if (srcBtn == nullptr) {
		return false;
	}
	auto hoverBtn = srcBtn->findChild<QToolButton *>();
	if (hoverBtn == nullptr) {
		return false;
	}
	if (event->type() == QEvent::HoverEnter) {
		hoverBtn->show();
		return true;
	}

	if (event->type() == QEvent::HoverLeave) {
		hoverBtn->hide();
		return true;
	}
	return false;
}

void DefaultPlatformsAddList::initUi()
{
	for (const QString &platformName : getDefaultPlatforms()) {
		auto btn = new QToolButton;

		btn->setObjectName(platformName);
		btn->setToolButtonStyle(Qt::ToolButtonIconOnly);

		QString iconPath;
		if (platformName.contains(CUSTOM_RTMP)) {
			iconPath = ":/Images/skin/btn-custom-rtmp-normal.png";

		} else {
			iconPath = getPlatformImageFromName(platformName, "btn.+-", "\\.png");
			btn->setDisabled(true);
		}
		btn->setIcon(QIcon(iconPath));
		if (btn->isEnabled()) {
			btn->installEventFilter(this);

			auto hoverBtn = new QToolButton(btn);
			hoverBtn->setObjectName("hoverBtn");
			hoverBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
			hoverBtn->setProperty(g_channelName.toStdString().c_str(), platformName);
			hoverBtn->hide();
			connect(hoverBtn, &QToolButton::clicked, this, &DefaultPlatformsAddList::runBtnCMD, Qt::QueuedConnection);
		}

		ui->DefaultPlatformsLayout->addWidget(btn);
	}
}
