#include "ChannelsAddWin.h"
#include <QListWidget>
#include <QListWidgetItem>
#include <QToolButton>
#include "ChannelCommonFunctions.h"
#include "ChannelConst.h"
#include "ComplexButton.h"
#include "LogPredefine.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelsVirualAPI.h"
#include "PLSDpiHelper.h"
#include "ui_ChannelsAddWin.h"

using namespace ChannelData;

ChannelsAddWin::ChannelsAddWin(QWidget *parent) : WidgetDpiAdapter(parent), ui(new Ui::ChannelsAddWin)
{
	ui->setupUi(this);
	PLSDpiHelper dpiHelper;
	dpiHelper.setCss(this, {PLSCssIndex::ChannelsAddWin});
	initDefault();
	updateUi();
}

ChannelsAddWin::~ChannelsAddWin()
{
	delete ui;
}

void ChannelsAddWin::updateUi()
{
	for (int i = 0; i < ui->ItemGridLayout->count(); ++i) {
		updateItem(i);
	}
}

void ChannelsAddWin::updateItem(int index)
{
	auto item = ui->ItemGridLayout->itemAt(index);
	auto widget = dynamic_cast<QToolButton *>(item->widget());
	if (widget) {
		QString platForm = getInfoOfObject(widget, g_platformName.toStdString().c_str(), QString());
		auto &info = PLSCHANNELS_API->getChanelInfoRefByPlatformName(platForm, ChannelType);

		if (info.isEmpty()) {
			widget->setEnabled(true);
			QString txt = getInfoOfObject(widget, g_nickName.toStdString().c_str(), QString());
			widget->setText(txt);
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
	auto srcBtn = dynamic_cast<QAbstractButton *>(watched);
	if (srcBtn == nullptr) {
		return false;
	}
	auto hoverBtn = srcBtn->findChild<QAbstractButton *>();
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

void ChannelsAddWin::appendItem(const QString &platformName)
{
	QAbstractButton *btn = nullptr;
	bool isCustomRTMP = platformName.contains(CUSTOM_RTMP);
	if (isCustomRTMP) {
		auto btn = ui->AddRTMPBtn;
		ui->AddRTMPBtn->setAliginment(Qt::AlignCenter);
		btn->setText(CHANNELS_TR(AddRTMP));
		pls_flush_style(ui->AddRTMPBtn);
		btn->setProperty(g_platformName.toStdString().c_str(), platformName);
		connect(btn, &ComplexButton::clicked, this, &ChannelsAddWin::runBtnCMD, Qt::QueuedConnection);
		return;
	}
	QString txt;
	auto tBtn = new QToolButton(ui->scrollAreaWidgetContents);
	tBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	txt = translatePlatformName(platformName);

	tBtn->setObjectName(platformName);
	btn = tBtn;

	btn->installEventFilter(this);
	tBtn->setText(txt);
	btn->setProperty(g_nickName.toStdString().c_str(), txt);
	btn->setProperty(g_platformName.toStdString().c_str(), platformName);
	QSize iconSize(0, 0);
	QString iconPath, disablePath;

	iconPath = getPlatformImageFromName(platformName, "btn.+", "\\.svg");
	disablePath = getPlatformImageFromName(platformName, "btn.+", "\\-on.svg");
	iconSize = QSize(115, 40);
	QIcon Icon;
	Icon.addFile(iconPath, iconSize, QIcon::Normal);
	Icon.addFile(disablePath, iconSize, QIcon::Disabled);
	btn->setIcon(Icon);
	PLSDpiHelper dpiHelper;
	dpiHelper.notifyDpiChanged(btn, [=](double dpi) { btn->setIconSize(PLSDpiHelper::calculate(dpi, iconSize)); });

	QAbstractButton *hoverBtn = nullptr;

	auto thoverBtn = new QToolButton(btn);
	thoverBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
	hoverBtn = thoverBtn;
	hoverBtn->setObjectName("hoverBtn");

	hoverBtn->setProperty(g_platformName.toStdString().c_str(), platformName);
	hoverBtn->hide();
	connect(hoverBtn, &QAbstractButton::clicked, this, &ChannelsAddWin::runBtnCMD, Qt::QueuedConnection);

	static int countOfRow = 4;
	auto layout = ui->ItemGridLayout;
	int row = layout->count() / countOfRow;
	int column = layout->count() % countOfRow;
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
	ui->ChannelsListWid->adjustSize();
}

void ChannelsAddWin::runBtnCMD()
{
	auto btn = sender();
	auto cmdStr = getInfoOfObject(btn, g_platformName.toStdString().c_str(), QString("add"));
	PRE_LOG_UI_MSG_STRING("ADD " + cmdStr, "Clicked");
	runCMD(cmdStr);
}
