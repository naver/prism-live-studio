#include "PLSSearchLineEdit.h"
#include "libutils-api.h"
#include "libui.h"

#include <QHBoxLayout>
#include <QToolButton>
#include <QKeyEvent>
#include <QPushButton>

PLSSearchLineEdit::PLSSearchLineEdit(QWidget *parent) : QLineEdit(parent)
{
	pls_add_css(this, {"PLSSearchLineEdit"});

	connect(this, &QLineEdit::textChanged, this, [this]() {
		PLS_UI_ACTION("In search line edit, the line edit text changed.");
		updatePlaceHolderColor();
		if (property("usedFor").toString() == QStringLiteral("addsource")) {
			SetDeleteBtnVisible(!text().isEmpty());
		}
	});

	searchLayout = pls_new<QHBoxLayout>(this);
	searchLayout->setContentsMargins(0, 8, 9, 10);
	searchLayout->setSpacing(0);

	deleteBtn = pls_new<QPushButton>(this);
	pls_uistep_v2_set_value(deleteBtn, QStringLiteral("Delete"));
	deleteBtn->setObjectName("deleteBtn");
	deleteBtn->setVisible(false);
	deleteBtn->setAutoDefault(false);
	deleteBtn->setDefault(false);
	deleteBtn->setFocusPolicy(Qt::NoFocus);
	connect(deleteBtn, &QPushButton::clicked, this, [this]() {
		clear();
		deleteBtn->setVisible(false);
		PLS_UI_ACTION("In search line edit, the detele button has been closed.");
	});

	toolBtnSearch = pls_new<QToolButton>(this);
	pls_uistep_v2_set_value(toolBtnSearch, QStringLiteral("Search"));
	connect(toolBtnSearch, &QToolButton::clicked, this, [this]() { emit SearchIconClicked(text()); });
	toolBtnSearch->setObjectName("toolBtnSearch");
	toolBtnSearch->setProperty("searchOn", false);
	pls_flush_style(toolBtnSearch);

	updateLayout();

	updatePlaceHolderColor();
}

void PLSSearchLineEdit::SetDeleteBtnVisible(bool visible)
{
	deleteBtn->setVisible(visible);
}

void PLSSearchLineEdit::setSearchButtonPosition(SearchButtonPosition position)
{
	if (m_searchButtonPosition == position) {
		return;
	}
	m_searchButtonPosition = position;
	updateLayout();
}

void PLSSearchLineEdit::updateLayout()
{
	QLayoutItem *item;
	while ((item = searchLayout->takeAt(0)) != nullptr) {
		if (item->spacerItem()) {
			delete item;
		} else if (item->widget() == nullptr) {
			delete item;
		} else {
			searchLayout->removeWidget(item->widget());
			delete item;
		}
	}

	int leftMargin = property("leftMargin").toInt();
	if (leftMargin > 0) {
		searchLayout->setContentsMargins(leftMargin, 8, 9, 10);
	} else {
		searchLayout->setContentsMargins(0, 8, 9, 10);
	}

	if (m_searchButtonPosition == SearchButtonLeft) {
		searchLayout->addWidget(toolBtnSearch);
		searchLayout->addSpacing(7);
		searchLayout->addStretch();
		searchLayout->addWidget(deleteBtn);
		setProperty("searchButtonPosition", "left");
	} else {
		searchLayout->addStretch();
		searchLayout->addWidget(deleteBtn);
		searchLayout->addSpacing(7);
		searchLayout->addWidget(toolBtnSearch);
		setProperty("searchButtonPosition", "right");
	}
	pls_flush_style(this);
}

void PLSSearchLineEdit::updatePlaceHolderColor()
{
	setProperty("showPlaceholder", text().isEmpty());
	pls_flush_style(this);
}

void PLSSearchLineEdit::focusInEvent(QFocusEvent *e)
{
	toolBtnSearch->setProperty("searchOn", true);
	pls_flush_style(toolBtnSearch);
	QLineEdit::focusInEvent(e);
}

void PLSSearchLineEdit::focusOutEvent(QFocusEvent *e)
{
	emit SearchMenuRequested(false);
	toolBtnSearch->setProperty("searchOn", false);
	pls_flush_style(toolBtnSearch);
	QLineEdit::focusOutEvent(e);
}

void PLSSearchLineEdit::keyReleaseEvent(QKeyEvent *event)
{
	if (event->type() == QEvent::KeyRelease) {
		switch (event->key()) {
		case Qt::Key_Enter:
		case Qt::Key_Return:
			emit SearchTrigger(this->text());
			break;
		default:
			break;
		}
	}
	QLineEdit::keyReleaseEvent(event);
}

void PLSSearchLineEdit::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit SearchMenuRequested(true);
	}
	QLineEdit::mousePressEvent(event);
}
