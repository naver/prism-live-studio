#include "PLSNewIconActionWidget.hpp"
#include "ui_PLSNewIconActionWidget.h"
#include "utils-api.h"
#include "libui.h"
#include <QFontMetrics>
#include <QIcon>
#include <QLayoutItem>
#include <QMargins>
#include <QTimer>
#include <QStyleOption>
#include <QPainter>

constexpr auto TITLE_DISABLE_COLOR = "color:#666666";
constexpr auto TITLE_NORMAL_COLOR = "color:#ffffff";

PLSNewIconActionWidget::PLSNewIconActionWidget(const QString &title, QWidget *parent, const QString &itemIconQrc) : QWidget(parent)
{
	ui = pls_new<Ui::PLSNewIconActionWidget>();
	ui->setupUi(this);
	if (!itemIconQrc.isEmpty()) {
		setupHelpMenuIconRow(itemIconQrc);
	}
	setText(title);
	setBadgeVisible();
	pls_set_css(this, {"PLSNewIconActionWidget"});
	ui->titleLabel->setStyleSheet(TITLE_NORMAL_COLOR);
}

void PLSNewIconActionWidget::setupHelpMenuIconRow(const QString &itemIconQrc)
{
	m_itemIcon = new QLabel(this);
	m_itemIcon->setObjectName(QStringLiteral("itemIcon"));
	m_itemIcon->setFixedSize(22, 22);
	m_itemIcon->setScaledContents(true);
	m_itemIcon->setAlignment(Qt::AlignCenter);
	m_itemIcon->setPixmap(QIcon(itemIconQrc).pixmap(22, 22));
	ui->horizontalLayout->insertWidget(0, m_itemIcon);
	ui->horizontalLayout->insertSpacing(1, 10);
	QMargins mg = ui->horizontalLayout->contentsMargins();
	mg.setLeft(20);
	ui->horizontalLayout->setContentsMargins(mg);
}

PLSNewIconActionWidget::~PLSNewIconActionWidget()
{
	pls_delete(ui);
}

void PLSNewIconActionWidget::setText(const QString &text)
{
	ui->titleLabel->setText(text);
}

int PLSNewIconActionWidget::helpMenuRowMinimumWidth() const
{
	const QMargins mg = ui->horizontalLayout->contentsMargins();
	int width = mg.left() + mg.right();
	const int count = ui->horizontalLayout->count();
	const int spacing = ui->horizontalLayout->spacing();
	for (int i = 0; i < count; ++i) {
		if (i > 0) {
			width += spacing;
		}
		QLayoutItem *li = ui->horizontalLayout->itemAt(i);
		if (!li) {
			continue;
		}
		if (QWidget *w = li->widget()) {
			if (w == ui->titleLabel) {
				const QFontMetrics fm(ui->titleLabel->font());
				width += fm.horizontalAdvance(ui->titleLabel->text());
				width += 6; // PLSNewIconActionWidget.css #titleLabel padding-right
			} else {
				width += w->minimumSizeHint().width();
			}
		} else if (QSpacerItem *si = li->spacerItem()) {
			if (si->expandingDirections() & Qt::Horizontal) {
				continue;
			}
			width += si->sizeHint().width();
		} else if (QLayout *sub = li->layout()) {
			width += sub->minimumSize().width();
		}
	}
	return width;
}

void PLSNewIconActionWidget::setBadgeVisible(bool visible)
{
	ui->badgeNew->setVisible(visible);
	if (auto *layout = ui->horizontalLayout) {
		layout->activate();
	}
	updateGeometry();
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

void PLSNewIconActionWidget::setNoticeTipsVisible(bool visible)
{
	if (!m_noticeTipsIcon) {
		m_noticeTipsIcon = new QLabel(ui->titleLabel);
		m_noticeTipsIcon->setScaledContents(true);
		m_noticeTipsIcon->setObjectName("noticeTipsIcon");
	}
	m_noticeTipsIcon->setVisible(visible);
	if (visible) {
		m_noticeTipsIcon->move(ui->titleLabel->width() - 5, 10);
	}
}

void PLSNewIconActionWidget::enterEvent(QEnterEvent *event)
{
	if (m_disabled) {
		return;
	}

	QWidget::enterEvent(event);
}

void PLSNewIconActionWidget::leaveEvent(QEvent *event)
{
	if (m_disabled) {
		return;
	}

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
