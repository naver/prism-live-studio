#include "ChannelsAddWin.h"
#include "ui_ChannelsAddWin.h"
#include <QToolButton>
#include <QListWidget>
#include <QListWidgetItem>
#include "ChannelConst.h"
#include "ChannelCommonFunctions.h"
#include "PLSChannelsVirualAPI.h"
#include "LogPredefine.h"

using namespace ChannelData;

ChannelsAddWin::ChannelsAddWin(QWidget *parent) : QFrame(parent), ui(new Ui::ChannelsAddWin)
{
	ui->setupUi(this);
	initDefault();
	updateUi();
}

ChannelsAddWin::~ChannelsAddWin()
{
	delete ui;
}

void ChannelsAddWin::updateUi()
{
	for (int i = 0; i < ui->scrollAreaWidgetContents->layout()->count(); ++i) {
		updateItem(i);
	}
}

void ChannelsAddWin::updateItem(int index)
{
	auto item = ui->scrollAreaWidgetContents->layout()->itemAt(index);
	auto widget = dynamic_cast<QToolButton *>(item->widget());
	if (widget) {
		QString platForm = getInfoOfObject(widget, g_channelName.toStdString().c_str(), QString());
		auto &info = PLSCHANNELS_API->getChanelInfoRefByPlatformName(platForm, ChannelType);

		if (info.isEmpty()) {
			widget->setEnabled(true);
			widget->setText(platForm);
			return;
		}
		widget->setEnabled(false);
		widget->setText(CHANNELS_TR(Linked));
	}
}

void ChannelsAddWin::changeEvent(QEvent *e)
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

bool ChannelsAddWin::eventFilter(QObject *watched, QEvent *event)
{
	auto srcBtn = dynamic_cast<QToolButton *>(watched);
	if (srcBtn == nullptr) {
		return false;
	}
	auto hoverBtn = srcBtn->findChild<QToolButton *>();
	if (hoverBtn == nullptr) {
		return false;
	}
	if (event->type() == QEvent::HoverEnter && srcBtn->isEnabled()) {
		hoverBtn->show();
		return true;
	}

	if (event->type() == QEvent::HoverLeave) {
		hoverBtn->hide();
		return true;
	}
	return false;
}

void ChannelsAddWin::appendItem(const QString &text)
{
	QToolButton *btn = new QToolButton(ui->scrollAreaWidgetContents);
	btn->setProperty(g_channelName.toStdString().c_str(), text);
	btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	btn->setObjectName(text);
	btn->setText(text);

	QString iconPath, disablePath;
	if (text.contains(CUSTOM_RTMP)) {
		iconPath = ":/Images/skin/btn-custom-rtmp-normal.png";
		disablePath = iconPath;
		btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
	} else {
		iconPath = getPlatformImageFromName(text, "btn.+-", "\\.png");
		disablePath = getPlatformImageFromName(text, "btn.+-", "-on");
	}
	QIcon Icon(iconPath);
	Icon.addPixmap(QPixmap(disablePath), QIcon::Disabled);

	btn->setIcon(Icon);
	btn->installEventFilter(this);

	auto hoverBtn = new QToolButton(btn);
	hoverBtn->setObjectName("hoverBtn");
	hoverBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
	hoverBtn->setProperty(g_channelName.toStdString().c_str(), text);
	hoverBtn->hide();
	connect(hoverBtn, &QToolButton::clicked, this, &ChannelsAddWin::runBtnCMD, Qt::QueuedConnection);

	auto layout = dynamic_cast<QGridLayout *>(ui->scrollAreaWidgetContents->layout());
	int row = layout->count() / 4;
	int column = layout->count() % 5;
	layout->addWidget(btn, row, column);
}

void ChannelsAddWin::on_ClosePtn_clicked()
{
	this->close();
}

void ChannelsAddWin::initDefault()
{
	for (const QString &platform : getDefaultPlatforms()) {
		appendItem(platform);
	}
}

void ChannelsAddWin::runBtnCMD()
{
	PRE_LOG_UI(try add channel, ChannelsAddWin);
	auto btn = dynamic_cast<QToolButton *>(sender());
	auto cmdStr = getInfoOfObject(btn, g_channelName.toStdString().c_str(), QString("add"));
	runCMD(cmdStr);
}
