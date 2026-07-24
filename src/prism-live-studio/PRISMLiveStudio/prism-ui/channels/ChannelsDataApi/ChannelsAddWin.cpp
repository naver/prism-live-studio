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
#include "libutils-api.h"
#include "pls-performance.h"
#include "ui_ChannelsAddWin.h"
using namespace ChannelData;

ChannelsAddWin::ChannelsAddWin(QWidget *parent) : PLSDialogView(parent, {}, CreateWinId::Create)
{
	PLS_PERFORMANCE_FUNCTION();
	PLS_DISABLE_UISTEP_V2(this);
	pls_add_css(this, {"ChannelsAddWin"});
	ui = pls_new<Ui::ChannelsAddWin>();
	PLS_PERFORMANCE_START(setupUi);
	setupUi(ui);
	PLS_PERFORMANCE_END(setupUi);
	PLS_PERFORMANCE_START(setWindow);
	setFixedSize(626, 401);
	setResizeEnabled(false);
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
	PLS_PERFORMANCE_END(setWindow);
	initDefault();
	updateUi();
	pls_uistep_v2_set_value(ui->ClosePtn, QStringLiteral("*"), QStringLiteral("Close"));
	pls_uistep_v2_set_title(this, QStringLiteral("Add Channels View"));
}

ChannelsAddWin::~ChannelsAddWin()
{
	pls_delete(ui, nullptr);
}

void ChannelsAddWin::updateUi() const
{
	PLS_PERFORMANCE_FUNCTION();
	for (int i = 0; i < ui->ItemGridLayout->count(); ++i) {
		updateItem(i);
	}
}

void ChannelsAddWin::updateItem(int index) const
{
	PLS_PERFORMANCE_FUNCTION();
	auto item = ui->ItemGridLayout->itemAt(index);
	auto widget = dynamic_cast<QToolButton *>(item->widget());
	if (widget) {
		QString platForm = getInfoOfObject(widget, g_channelName.toUtf8().constData(), QString());

		if (const auto &info = PLSCHANNELS_API->getChanelInfoRefByPlatformName(platForm, ChannelType); info.isEmpty()) {
			widget->setEnabled(true);
			QString txt = getInfoOfObject(widget, g_nickName.toUtf8().constData(), QString());
			widget->setText(txt);
			return;
		}
		widget->setEnabled(false);
		widget->setText(CHANNELS_TR(Linked));
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
	PLS_PERFORMANCE_FUNCTION();
	if (platformName.contains(CUSTOM_RTMP)) {
		appendRTMPItem(platformName);
		return;
	}
	QString txt = translatePlatformName(platformName);
	auto tBtn = new QToolButton(ui->scrollAreaWidgetContents);
	tBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	tBtn->setObjectName(platformName);
	tBtn->installEventFilter(this);
	tBtn->setText(txt);
	tBtn->setProperty(g_nickName.toUtf8().constData(), txt);
	tBtn->setProperty(g_channelName.toUtf8().constData(), platformName);
	pls_uistep_v2_set_custom_enter_leave_name(tBtn, (platformName + " Add Button").toUtf8().constData());
	QString iconPath = getPlatformImageFromName(platformName, ImageType::addChannelButtonIcon, "btn.+", "\\.svg");
	QString disablePath = getPlatformImageFromName(platformName, ImageType::addChannelButtonConnectedIcon, "btn.+", "\\-on.svg");
	QSize iconSize(115, 40);
	QIcon icon;
	icon.addFile(iconPath, iconSize, QIcon::Normal);
	icon.addFile(disablePath, iconSize, QIcon::Disabled);
	tBtn->setIcon(icon);
	tBtn->setIconSize(iconSize);

	auto hoverBtn = new QToolButton(tBtn);
	hoverBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
	hoverBtn->setObjectName("hoverBtn");
	hoverBtn->setProperty(g_channelName.toUtf8().constData(), platformName);
	hoverBtn->hide();
	connect(hoverBtn, &QAbstractButton::clicked, this, &ChannelsAddWin::runBtnCMD, Qt::QueuedConnection);
	pls_uistep_v2_set_value(hoverBtn, QStringLiteral("*"), platformName);
	pls_uistep_v2_set_custom_show_hide_name(hoverBtn, (platformName + " Hover Button").toUtf8().constData());
	static constexpr int countOfRow = 4;
	int count = ui->ItemGridLayout->count();
	ui->ItemGridLayout->addWidget(tBtn, count / countOfRow, count % countOfRow);
}

void ChannelsAddWin::appendRTMPItem(const QString &platformName)
{
	auto btnTmp = ui->AddRTMPBtn;
	ui->AddRTMPBtn->setAliginment(Qt::AlignCenter);
	btnTmp->setText(CHANNELS_TR(AddRTMP));
	pls_flush_style(ui->AddRTMPBtn);
	btnTmp->setProperty(g_channelName.toUtf8().constData(), platformName);
	btnTmp->setProperty("showHandCursor", true);
	connect(btnTmp, &ComplexButton::clicked, this, &ChannelsAddWin::runBtnCMD, Qt::QueuedConnection);
}

void ChannelsAddWin::on_ClosePtn_clicked()
{
	this->close();
}

void ChannelsAddWin::initDefault()
{
	PLS_PERFORMANCE_FUNCTION();
	ui->scrollAreaWidgetContents->setUpdatesEnabled(false);
	for (const QString &platform : getDefaultPlatforms()) {
		appendItem(platform);
	}
	ui->scrollAreaWidgetContents->setUpdatesEnabled(true);
	ui->ChannelsListWid->adjustSize();
}

void ChannelsAddWin::runBtnCMD()
{
	auto btn = sender();
	auto cmdStr = getInfoOfObject(btn, g_channelName.toUtf8().constData(), QString("add"));
	// Close this dialog first; defer runCMD so RTMP/guide are not nested under this exec (guide layout).
	pls_async_call_mt([cmdStr]() { runCMD(cmdStr); });
	accept();
}
