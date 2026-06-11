#include "ChannelsAddWin.h"
#include <QListWidget>
#include <QListWidgetItem>
#include <QToolButton>
#include "ChannelCommonFunctions.h"
#include "ComplexButton.h"
#include "LogPredefine.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelsVirualAPI.h"
#include "libui.h"
#include "pls-channel-const.h"
#include "ui_ChannelsAddWin.h"

using namespace ChannelData;

ChannelsAddWin::ChannelsAddWin(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::ChannelsAddWin>();
	setupUi(ui);
	setFixedSize(626, 401);
#if defined(Q_OS_MACOS)
	this->setHasCloseButton(true);
	this->setHasMinButton(false);
	this->setHasMaxResButton(false);
	this->setWindowTitle(tr("Channels.addwin.Channels"));
	ui->ClosePtn->hide();
#else
	this->setHasCaption(false);
#endif
	this->setHasHLine(false);
	pls_add_css(this, {"ChannelsAddWin"});
	initDefault();
	updateUi();
}

ChannelsAddWin::~ChannelsAddWin()
{
	pls_delete(ui, nullptr);
}

void ChannelsAddWin::updateUi() const
{
	for (int i = 0; i < ui->ItemGridLayout->count(); ++i) {
		updateItem(i);
	}
}

void ChannelsAddWin::updateItem(int index) const
{
	auto item = ui->ItemGridLayout->itemAt(index);
	auto widget = dynamic_cast<QToolButton *>(item->widget());
	if (widget) {
		QString platForm = getInfoOfObject(widget, g_channelName.toStdString().c_str(), QString());

		if (const auto &info = PLSCHANNELS_API->getChanelInfoRefByPlatformName(platForm, ChannelType); info.isEmpty()) {
			widget->setEnabled(true);
			QString txt = getInfoOfObject(widget, g_nickName.toStdString().c_str(), QString());
			widget->setText(txt);
			return;
		}
		widget->setEnabled(false);
		widget->setText(CHANNELS_TR(Linked));
	}
}

bool ChannelsAddWin::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == this && event->type() == QEvent::WindowDeactivate) {
		this->setHidden(true);
		this->deleteLater();
		return true;
	}
	auto srcBtn = dynamic_cast<QAbstractButton *>(watched);
	if (srcBtn == nullptr) {
		return false;
	}
	auto hoverBtn = srcBtn->findChild<QAbstractButton *>();
	if (hoverBtn == nullptr) {
		return false;
	}
	if (event->type() == QEvent::Enter && srcBtn->isEnabled()) {
		hoverBtn->show();
		return true;
	}

	if (event->type() == QEvent::Leave) {
		hoverBtn->hide();
		return true;
	}
	return PLSDialogView::eventFilter(watched, event);
}

void ChannelsAddWin::appendItem(const QString &platformName)
{
	QAbstractButton *btn = nullptr;

	if (platformName.contains(CUSTOM_RTMP)) {
		appendRTMPItem(platformName);
		return;
	}
	QString txt;
	auto tBtn = new QToolButton(ui->scrollAreaWidgetContents);
	tBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	txt = translatePlatformName(platformName);

	tBtn->setObjectName(platformName);
	btn = tBtn;

	btn->installEventFilter(this);
	this->installEventFilter(this);
	tBtn->setText(txt);
	btn->setProperty(g_nickName.toStdString().c_str(), txt);
	btn->setProperty(g_channelName.toStdString().c_str(), platformName);
	QSize iconSize(0, 0);
	QString iconPath;
	QString disablePath;

	iconPath = getPlatformImageFromName(platformName, ImageType::addChannelButtonIcon, "btn.+", "\\.svg");
	disablePath = getPlatformImageFromName(platformName, ImageType::addChannelButtonConnectedIcon, "btn.+", "\\-on.svg");
	iconSize = QSize(115, 40);
	QIcon Icon;
	Icon.addFile(iconPath, iconSize, QIcon::Normal);
	Icon.addFile(disablePath, iconSize, QIcon::Disabled);
	btn->setIcon(Icon);

	auto notifyDpiChanged = [btn, iconSize]() { btn->setIconSize(iconSize); };
	notifyDpiChanged();
	QAbstractButton *hoverBtn = nullptr;

	auto thoverBtn = new QToolButton(btn);
	thoverBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
	hoverBtn = thoverBtn;
	hoverBtn->setObjectName("hoverBtn");

	hoverBtn->setProperty(g_channelName.toStdString().c_str(), platformName);
	hoverBtn->hide();
	connect(hoverBtn, &QAbstractButton::clicked, this, &ChannelsAddWin::runBtnCMD, Qt::QueuedConnection);

	static int countOfRow = 4;
	auto layout = ui->ItemGridLayout;
	int row = layout->count() / countOfRow;
	int column = layout->count() % countOfRow;
	layout->addWidget(btn, row, column);
}

void ChannelsAddWin::appendRTMPItem(const QString &platformName)
{
	auto btnTmp = ui->AddRTMPBtn;
	ui->AddRTMPBtn->setAliginment(Qt::AlignCenter);
	btnTmp->setText(CHANNELS_TR(AddRTMP));
	pls_flush_style(ui->AddRTMPBtn);
	btnTmp->setProperty(g_channelName.toStdString().c_str(), platformName);
	btnTmp->setProperty("showHandCursor", true);
	connect(btnTmp, &ComplexButton::clicked, this, &ChannelsAddWin::runBtnCMD, Qt::QueuedConnection);
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
	auto cmdStr = getInfoOfObject(btn, g_channelName.toStdString().c_str(), QString("add"));
	PRE_LOG_UI_MSG_STRING("ADD " + cmdStr, "Clicked")
	this->close();
	runCMD(cmdStr);
}
