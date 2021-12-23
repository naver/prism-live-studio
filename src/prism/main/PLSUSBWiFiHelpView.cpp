#include "PLSUSBWiFiHelpView.h"
#include "ui_PLSUSBWiFiHelpView.h"
#include <QStyle>
#include <QButtonGroup>
#include <qdesktopservices.h>
#include "pls-app.hpp"
#include "main-view.hpp"

PLSUSBWiFiHelpView::PLSUSBWiFiHelpView(DialogInfo info, QWidget *parent, PLSDpiHelper dpiHelper)
	: PLSDialogView(info, parent, dpiHelper), ui(new Ui::PLSUSBWiFiHelpView), m_buttonGroup(new QButtonGroup(this))
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSUSBWiFiHelpView});
	ui->setupUi(this->content());
	notifyFirstShow([=]() { this->InitGeometry(true); });
	setHasMaxResButton(true);
	setWindowTitle(tr("wifihelper.title"));
	ui->topLeftBtn->setText(tr("wifihelper.top.left.text"));
	ui->topRightBtn->setText(tr("wifihelper.top.right.text"));
	m_buttonGroup->addButton(ui->topLeftBtn, 0);
	m_buttonGroup->addButton(ui->topRightBtn, 1);
	connect(m_buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &PLSUSBWiFiHelpView::buttonGroupSlot);
	m_buttonId = 0;
	updateShowPage();

	connect(ui->googleBtn, &QPushButton::clicked, [] { QDesktopServices::openUrl(QString("https://play.google.com/store/apps/details?id=com.prism.live")); });
	connect(ui->appleBtn, &QPushButton::clicked, [] { QDesktopServices::openUrl(QString("https://apps.apple.com/app/id1319056339")); });
}

PLSUSBWiFiHelpView::~PLSUSBWiFiHelpView()
{
	delete ui;
}

void PLSUSBWiFiHelpView::buttonGroupSlot(int buttonId)
{
	m_buttonId = buttonId;
	updateShowPage();
}

void PLSUSBWiFiHelpView::updateShowPage()
{
	ui->stackedWidget->setCurrentIndex(m_buttonId);
}
void PLSUSBWiFiHelpView::showEvent(QShowEvent *event)
{
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::WiFiConfig, true);
	pls_window_right_margin_fit(this);
	PLSDialogView::showEvent(event);
}

void PLSUSBWiFiHelpView::hideEvent(QHideEvent *event)
{
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::WiFiConfig, false);
	PLSDialogView::hideEvent(event);
}

void PLSUSBWiFiHelpView::closeEvent(QCloseEvent *event)
{
	hide();
	event->ignore();
}
