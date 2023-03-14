#include "PLSWiFiUSBTipView.h"
#include "ui_PLSWiFiUSBTipView.h"
#include "PLSDpiHelper.h"
#include <QRegularExpression>

PLSWiFiUSBTipView::PLSWiFiUSBTipView(QWidget *parent) : QFrame(parent), ui(new Ui::PLSWiFiUSBTipView)
{
	ui->setupUi(this);
}

PLSWiFiUSBTipView::~PLSWiFiUSBTipView()
{
	delete ui;
}

void PLSWiFiUSBTipView::updateContent()
{
	auto fontMetric = ui->contentLabel->fontMetrics();
	auto txt = ui->contentLabel->text();
	txt.remove(QRegularExpression("<[^<]+>"));
	auto rec = fontMetric.boundingRect(ui->contentLabel->rect(), Qt::AlignBottom | Qt::AlignLeft | Qt::TextWordWrap, txt);
	int height = PLSDpiHelper::calculate(this, 75 + 15 + 30) + rec.height();
	this->resize(this->width(), height);
}

void PLSWiFiUSBTipView::on_closeBtn_clicked()
{
	setVisible(false);
	emit closed();
}
