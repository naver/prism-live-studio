#include "PLSNewIconActionWidget.hpp"
#include "ui_PLSNewIconActionWidget.h"
#include "utils-api.h"
#include "libui.h"
#include <QTimer>
#include <QStyleOption>
#include <QPainter>

constexpr auto TITLE_DISABLE_COLOR = "color:#666666";
constexpr auto TITLE_NORMAL_COLOR = "color:#ffffff";

PLSNewIconActionWidget::PLSNewIconActionWidget(const QString &title, QWidget *parent) : QWidget(parent)
{
	ui = pls_new<Ui::PLSNewIconActionWidget>();
	ui->setupUi(this);
	setText(title);
	setBadgeVisible();
	pls_set_css(this, {"PLSNewIconActionWidget"});
	ui->titleLabel->setStyleSheet(TITLE_NORMAL_COLOR);
}

PLSNewIconActionWidget::~PLSNewIconActionWidget()
{
	pls_delete(ui);
}

void PLSNewIconActionWidget::setText(const QString &text)
{
	ui->titleLabel->setText(text);
}

void PLSNewIconActionWidget::setBadgeVisible(bool visible)
{
	ui->badgeNew->setVisible(visible);
}

void PLSNewIconActionWidget::setItemDisabled(bool disabled)
{
	m_disabled = disabled;
	if (m_disabled) {
		ui->titleLabel->setStyleSheet(TITLE_DISABLE_COLOR);
	} else {
		ui->titleLabel->setStyleSheet(TITLE_NORMAL_COLOR);
	}
}

int PLSNewIconActionWidget::getTextMarginLeft() const
{
	return textMarginLeft;
}

void PLSNewIconActionWidget::setTextMarginLeft(int textMarginLeft_)
{
	this->textMarginLeft = textMarginLeft_;
	QMargins margins = ui->horizontalLayout->contentsMargins();
	margins.setLeft(textMarginLeft);
	ui->horizontalLayout->setContentsMargins(margins);
}

void PLSNewIconActionWidget::enterEvent(QEnterEvent *event)
{
	if (m_disabled) {
		return;
	}
	ui->titleLabel->setStyleSheet(TITLE_NORMAL_COLOR);
	QWidget::enterEvent(event);
}

void PLSNewIconActionWidget::leaveEvent(QEvent *event)
{
	if (m_disabled) {
		return;
	}
	ui->titleLabel->setStyleSheet(TITLE_NORMAL_COLOR);
	QWidget::leaveEvent(event);
}

void PLSNewIconActionWidget::mousePressEvent(QMouseEvent *event)
{
	if (m_disabled) {
		return;
	}
	QWidget::mousePressEvent(event);
}

void PLSNewIconActionWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if (m_disabled) {
		return;
	}
	QWidget::mouseReleaseEvent(event);
}

void PLSNewIconActionWidget::paintEvent(QPaintEvent *event)
{
	QStyleOption opt;
	opt.initFrom(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
	QWidget::paintEvent(event);
}
