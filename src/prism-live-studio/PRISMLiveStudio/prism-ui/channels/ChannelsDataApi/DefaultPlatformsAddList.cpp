#include "DefaultPlatformsAddList.h"
#include <QLabel>
#include <QToolButton>
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelsVirualAPI.h"
#include "PLSLoginDataHandler.h"
#include "PLSSyncServerManager.hpp"
#include "libui.h"
#include "pls-channel-const.h"
#include "pls-shared-functions.h"
#include "ui_DefaultPlatformsAddList.h"

using namespace ChannelData;

DefaultPlatformsAddList::DefaultPlatformsAddList(QWidget *parent) : QFrame(parent), ui(new Ui::DefaultPlatformsAddList)
{
	ui->setupUi(this);
	initUi();
	pls_add_css(this, {"DefaultPlatformsAddList"});
}

DefaultPlatformsAddList::~DefaultPlatformsAddList()
{
	delete ui;
}

void DefaultPlatformsAddList::runBtnCMD() const
{
	auto btn = dynamic_cast<QToolButton *>(sender());
	auto cmdStr = getInfoOfObject(btn, g_channelName.toStdString().c_str(), QString("add"));
	PRE_LOG_UI_MSG_STRING(("Default Platform" + cmdStr), "clicked")
	btn->hide();
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

	if (!hoverBtn->isEnabled()) {
		return false;
	}

	if (event->type() == QEvent::Enter) {
		hoverBtn->show();
		return true;
	}

	if (event->type() == QEvent::Leave) {
		hoverBtn->hide();
		return true;
	}
	return false;
}

void DefaultPlatformsAddList::initUi()
{
	auto platforms = getDefaultPlatforms();
	auto size = platforms.size();
	for (int i = 0; i < size; ++i) {
		const QString &platformName = platforms[i];
		auto fixPlatformName = channleNameConvertFixPlatformName(platformName);
		auto btn = new QToolButton;

		btn->setObjectName(platformName);
		btn->setToolButtonStyle(Qt::ToolButtonIconOnly);

		auto updateIcon = [platformName, btn, fixPlatformName]() {
			pls_check_app_exiting();
			QString iconPath;
			QSize iconSize(90, 33);
			if (platformName.contains(CUSTOM_RTMP)) {
				iconPath = g_defaultRTMPAddButtonIcon;
			} else {
				iconPath = getPlatformImageFromName(platformName, ImageType::dashboardButtonIcon, "btn-mych.+", "\\.svg");
				if (fixPlatformName.contains(NCB2B)) {
					iconSize = QSize(95, 33);
					btn->setObjectName("NCB2B");
				}
			}
			QIcon icon;
			QFileInfo info(iconPath);
			if (0 == info.suffix().compare("svg", Qt::CaseInsensitive)) {
				icon.addPixmap(pls_shared_paint_svg(iconPath, iconSize * 4), QIcon::Normal);
			} else {
				icon.addFile(iconPath);
			}
			btn->setIcon(icon);
		};
		updateIcon();
		if (fixPlatformName == NCB2B) {
			connect(PLSLOGINDATAHANDLER, &PLSLoginDataHandler::updateNCB2BIcon, updateIcon);
		} else if (fixPlatformName == CUSTOM_RTMP) {
			connect(PLS_SYNC_SERVER_MANAGE, &PLSSyncServerManager::libraryNeedUpdate, updateIcon);
		}

		btn->installEventFilter(this);

		auto hoverBtn = new QToolButton(btn);
		hoverBtn->setObjectName("hoverBtn");

		hoverBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
		hoverBtn->setProperty(g_channelName.toStdString().c_str(), platformName);
		if (fixPlatformName.contains(NCB2B)) {
			hoverBtn->setProperty(g_fixPlatformName.toStdString().c_str(), "NCB2B");
		}
		hoverBtn->hide();
		connect(hoverBtn, &QToolButton::clicked, this, &DefaultPlatformsAddList::runBtnCMD, Qt::QueuedConnection);

		ui->DefaultPlatformsLayout->addWidget(btn);
		if (i == size - 1) {
			auto spaceLabel = new QLabel(this);
			spaceLabel->setObjectName("spaceLabel");
			ui->DefaultPlatformsLayout->addWidget(spaceLabel);
		}
	}
}
