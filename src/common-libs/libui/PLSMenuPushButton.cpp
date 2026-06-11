#include "PLSMenuPushButton.h"
#include "libutils-api.h"
#include "pls-common-define.hpp"
using namespace common;

#include <QStyleOption>
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>

PLSMenuPushButton::PLSMenuPushButton(QWidget *parent, bool needTipsDelegate) : QPushButton(parent)
{
	pls_add_css(this, {"PLSMenuPushButton"});

	setMouseTracking(true);

	popupBtn = pls_new<QPushButton>(this);
	popupBtn->setObjectName("menuPushButtonPopupBtn");
	popupBtn->setAttribute(Qt::WA_TransparentForMouseEvents);

	popupMenu = pls_new<QMenu>(this);
	popupMenu->setWindowFlag(Qt::NoDropShadowWindowHint);
	popupMenu->setObjectName("menuPushButtonPopupMenu");

	this->setMenu(popupMenu);
	popupMenu->installEventFilter(this);

	if (needTipsDelegate) {
		tipsDelegate = pls_new<QWidget>(this);
		tipsDelegate->setObjectName("dockTipsDelegate");
		tipsDelegate->setAttribute(Qt::WA_TransparentForMouseEvents);
	}
}

void PLSMenuPushButton::SetText(const QString &text)
{
	name = text;
	needUpdate = true;
	update();
}

QString PLSMenuPushButton::GetText() const
{
	return name;
}

QMenu *PLSMenuPushButton::GetPopupMenu()
{
	return popupMenu;
}

int PLSMenuPushButton::LeftSpacing() const
{
	return leftSpacing;
}

void PLSMenuPushButton::SetLeftSpacing(int v)
{
	leftSpacing = v;
	needUpdate = true;
	update();
}

int PLSMenuPushButton::RightSpacing() const
{
	return rightSpacing;
}

void PLSMenuPushButton::SetRightSpacing(int v)
{
	rightSpacing = v;
	needUpdate = true;
	update();
}

int PLSMenuPushButton::Spacing() const
{
	return spacing;
}

void PLSMenuPushButton::SetSpacing(int v)
{
	spacing = v;
	needUpdate = true;
	update();
}

int PLSMenuPushButton::ContentWidth() const
{
	return contentRect.width();
}

bool PLSMenuPushButton::eventFilter(QObject *obj, QEvent *event)
{
	if (event && obj == popupMenu) {
		if (event->type() == QEvent::Show) {
			emit ResizeMenu();
			emit ShowMenu();
			pls_flush_style(popupBtn, "menuStatus", "on");
		} else if (event->type() == QEvent::Hide) {
			emit HideMenu();
			pls_flush_style(popupBtn, "menuStatus", "off");
		}
	}

	return QPushButton::eventFilter(obj, event);
}

void PLSMenuPushButton::paintEvent(QPaintEvent *event)
{
	QPushButton::paintEvent(event);
	auto font = this->font();
	QFontMetrics fontMeterics(font);
	int nameWidth = fontMeterics.horizontalAdvance(name);
	auto iconSize = QSize(12, 12);
	auto marginLeft = leftSpacing;
	auto marginRight = rightSpacing;
	auto sp = spacing;

	QPainter painter(this);
	painter.setFont(font);

	QStyleOption opt;
	opt.initFrom(this);
	auto textColor = opt.palette.color(QPalette::Text);
	painter.setPen(textColor);
	auto minWidth = (marginLeft + marginRight + iconSize.width() + fontMeterics.horizontalAdvance("..."));
	if (width() <= minWidth) {
		menuBtnX = std::max(6, (width() - iconSize.width()) / 2);
		contentRect = QRect(0, (height() - 22) / 2, width(), 22);
		checkNeedUpdate();
		return;
	}

	auto textOpt = QTextOption(Qt::AlignLeft | Qt::AlignVCenter);
	textOpt.setWrapMode(QTextOption::NoWrap);
	auto offset = width() - (nameWidth + iconSize.width() + marginLeft + marginRight + spacing);
	if (offset < 0) {
		auto textWidth = nameWidth + offset;
		QRect rectText = QRect(marginLeft, 0, textWidth, height());
		painter.drawText(rectText, fontMeterics.elidedText(name, Qt::ElideRight, textWidth), textOpt);
		menuBtnX = rectText.right();
	} else {
		QRect rectText = QRect(marginLeft, 0, nameWidth, height());
		painter.drawText(rectText, name, textOpt);
		menuBtnX = rectText.right() + sp;
	}
	contentRect = QRect(0, (height() - 22) / 2, menuBtnX + popupBtn->width() + marginRight, 22);
	checkNeedUpdate();
}

void PLSMenuPushButton::resizeEvent(QResizeEvent *event)
{
	needUpdate = true;
	QPushButton::resizeEvent(event);
}

void PLSMenuPushButton::mousePressEvent(QMouseEvent *event)
{
	if (event->pos().x() > popupBtn->geometry().right() + rightSpacing) {
		event->ignore();
		return;
	}

	setMouseStatus(STATUS_PRESSED);
	emit PopupClicked();

	QPushButton::mousePressEvent(event);
}

void PLSMenuPushButton::mouseMoveEvent(QMouseEvent *event)
{
	if (event->pos().x() > popupBtn->geometry().right() + rightSpacing) {
		setCursor(Qt::ArrowCursor);
		if (lastStatus != QEvent::Leave) {
			setMouseStatus(STATUS_NORMAL);
			lastStatus = QEvent::Leave;
		}
		event->ignore();
		return;
	} else {
		setCursor(Qt::PointingHandCursor);
		if (lastStatus != QEvent::Enter) {
			setMouseStatus(STATUS_HOVER);
			lastStatus = QEvent::Enter;
		}
	}

	QPushButton::mouseMoveEvent(event);
}

void PLSMenuPushButton::leaveEvent(QEvent *event)
{
	setMouseStatus(STATUS_NORMAL);
	lastStatus = QEvent::Leave;
	QPushButton::leaveEvent(event);
}

void PLSMenuPushButton::setMouseStatus(const char *status)
{
	pls_flush_style(this, STATUS, status);
	pls_flush_style(popupBtn, STATUS, status);
}

void PLSMenuPushButton::asyncUpdate()
{
	QTimer::singleShot(0, this, [this]() {
		popupBtn->move(menuBtnX, (height() - popupBtn->height()) / 2 - 1);
		if (tipsDelegate) {
			tipsDelegate->setGeometry(contentRect);
		}
	});
}

void PLSMenuPushButton::checkNeedUpdate()
{
	if (needUpdate) {
		asyncUpdate();
		needUpdate = false;
		update();
	}
}
