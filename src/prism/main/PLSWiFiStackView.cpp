#include "PLSWiFiStackView.h"
#include "ui_PLSWiFiStackView.h"
#include <QStyle>
#include <QButtonGroup>
#include "PLSDpiHelper.h"
#include <QDebug>
static const int PLSIndicatorMaxIndex = 3;

PLSWiFiStackView::PLSWiFiStackView(QWidget *parent) : QFrame(parent), ui(new Ui::PLSWiFiStackView), m_buttonGroup(new QButtonGroup(this))
{
	ui->setupUi(this);
	m_buttonGroup->addButton(ui->indicatorHome, 0);
	m_buttonGroup->addButton(ui->indicatorOne, 1);
	m_buttonGroup->addButton(ui->indicatorTwo, 2);
	m_buttonGroup->addButton(ui->indicatorThree, 3);
	connect(m_buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &PLSWiFiStackView::buttonGroupSlot);
	m_id = 0;
	m_tipView = new PLSWiFiUSBTipView(this);
	//ui->verticalLayout_4->insertWidget(1, m_tipView);
	m_tipView->setVisible(false);
	connect(m_tipView, &PLSWiFiUSBTipView::closed, [this]() { ui->tipsBtn->show(); });

	updateArrowButtonState();
	connect(this, &PLSWiFiStackView::sigUpdateTipView, this, &PLSWiFiStackView::updateTipView, Qt::QueuedConnection);
	this->installEventFilter(this);

	auto dpiHelper = PLSDpiHelper();
	dpiHelper.notifyDpiChanged(ui->tipsBtn, [this](double dpi) { ui->tipsBtn->setIconSize(PLSDpiHelper::calculate(dpi, QSize(11, 6))); });
}

PLSWiFiStackView::~PLSWiFiStackView()
{
	delete ui;
}

bool PLSWiFiStackView::eventFilter(QObject *obj, QEvent *ev)
{
	if (m_tipView->isVisible() && (ev->type() == QEvent::Resize || ev->type() == QEvent::Show)) {
		emit sigUpdateTipView();
	}
	return QFrame::eventFilter(obj, ev);
}

void PLSWiFiStackView::updateTipView()
{
	int width = ui->tipsBtn->width();
	m_tipView->resize(width, PLSDpiHelper::calculate(this, 150));
	m_tipView->updateContent();
	auto pos = ui->tipsBtn->pos() + QPoint(0, ui->tipsBtn->height() - m_tipView->height());
	m_tipView->move(pos);
}

void PLSWiFiStackView::buttonGroupSlot(int buttonId)
{
	m_id = buttonId;
	updateArrowButtonState();
}

void PLSWiFiStackView::on_leftPageBtn_clicked()
{
	--m_id;
	m_buttonGroup->button(m_id)->setChecked(true);
	updateArrowButtonState();
}

void PLSWiFiStackView::on_rightPageBtn_clicked()
{
	++m_id;
	m_buttonGroup->button(m_id)->setChecked(true);
	updateArrowButtonState();
}

void PLSWiFiStackView::updateArrowButtonState()
{
	bool leftEnabled = m_id == 0 ? false : true;
	bool rightEnabled = m_id == PLSIndicatorMaxIndex ? false : true;
	ui->leftPageBtn->setEnabled(leftEnabled);
	ui->rightPageBtn->setEnabled(rightEnabled);
	ui->pageLabel->setProperty("wifi_Image", m_id);
	ui->pageLabel->style()->unpolish(ui->pageLabel);
	ui->pageLabel->style()->polish(ui->pageLabel);
	ui->pageLabel->update();
	QString tipTxt = QString("<p style=\"line-height:22px;\">%1</p>").arg(getTextForTag(m_id).replace("\n", "<br>"));
	ui->tipLabel->setText(tipTxt);
}

QString PLSWiFiStackView::getTextForTag(int tag)
{
	if (tag == 0) {
		return tr("wifihelper.wifi.home.tip");
	} else if (tag == 1) {
		return tr("wifihelper.wifi.first.tip");
	} else if (tag == 2) {
		return tr("wifihelper.wifi.second.tip");
	} else if (tag == 3) {
		return tr("wifihelper.wifi.third.tip");
	}
	return "";
}

void PLSWiFiStackView::on_tipsBtn_clicked()
{
	m_tipView->setVisible(true);
	updateTipView();
}
