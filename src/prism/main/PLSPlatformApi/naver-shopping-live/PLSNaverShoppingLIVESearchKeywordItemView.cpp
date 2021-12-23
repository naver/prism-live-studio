#include "PLSNaverShoppingLIVESearchKeywordItemView.h"
#include "ui_PLSNaverShoppingLIVESearchKeywordItemView.h"
#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSNaverShoppingLIVEItemViewCache.h"
#include "frontend-api.h"

#include <QMouseEvent>
#include <QDesktopServices>

static PLSNaverShoppingLIVEItemViewCache<PLSNaverShoppingLIVESearchKeywordItemView> itemViewCache;

PLSNaverShoppingLIVESearchKeywordItemView::PLSNaverShoppingLIVESearchKeywordItemView(QWidget *parent) : QPushButton(parent), ui(new Ui::PLSNaverShoppingLIVESearchKeywordItemView)
{
	setAttribute(Qt::WA_Hover);
	ui->setupUi(this);

	setDefault(false);
	setAutoDefault(false);
	setFocusPolicy(Qt::NoFocus);

	ui->textLabel->installEventFilter(this);
	connect(ui->removeButton, &QPushButton::clicked, this, [this]() { removeButtonClicked(this); });
	connect(this, &QPushButton::clicked, this, [this]() { searchButtonClicked(this); });
}

PLSNaverShoppingLIVESearchKeywordItemView::~PLSNaverShoppingLIVESearchKeywordItemView()
{
	delete ui;
}

QString PLSNaverShoppingLIVESearchKeywordItemView::getSearchKeyword() const
{
	return searchKeyword;
}

void PLSNaverShoppingLIVESearchKeywordItemView::setInfo(const QString &searchKeyword)
{
	this->searchKeyword = searchKeyword;
	ui->textLabel->setText(searchKeyword);
}

void PLSNaverShoppingLIVESearchKeywordItemView::addBatchCache(int batchCount, bool check)
{
	itemViewCache.addBatchCache(batchCount, check);
}

void PLSNaverShoppingLIVESearchKeywordItemView::cleaupCache()
{
	itemViewCache.cleaupCache();
}

PLSNaverShoppingLIVESearchKeywordItemView *PLSNaverShoppingLIVESearchKeywordItemView::alloc(QWidget *parent, const QString &searchKeyword)
{
	return itemViewCache.alloc(parent, searchKeyword);
}

void PLSNaverShoppingLIVESearchKeywordItemView::dealloc(PLSNaverShoppingLIVESearchKeywordItemView *view)
{
	QObject::disconnect(view->removeButtonClickedConnection);
	QObject::disconnect(view->searchButtonClickedConnection);
	itemViewCache.dealloc(view);
}

void PLSNaverShoppingLIVESearchKeywordItemView::dealloc(QList<PLSNaverShoppingLIVESearchKeywordItemView *> &views, QVBoxLayout *layout)
{
	itemViewCache.dealloc(views, layout, [](PLSNaverShoppingLIVESearchKeywordItemView *view) {
		QObject::disconnect(view->removeButtonClickedConnection);
		QObject::disconnect(view->searchButtonClickedConnection);
	});
}

void PLSNaverShoppingLIVESearchKeywordItemView::updateSearchKeyword(QList<PLSNaverShoppingLIVESearchKeywordItemView *> &views)
{
	for (auto view : views) {
		QString text = view->ui->textLabel->fontMetrics().elidedText(view->searchKeyword, Qt::ElideRight, view->ui->textLabel->width());
		view->ui->textLabel->setText(text);
	}
}

bool PLSNaverShoppingLIVESearchKeywordItemView::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::HoverEnter: {
		pls_flush_style_recursive(this, "hovered", true);
		break;
	}
	case QEvent::HoverLeave: {
		pls_flush_style_recursive(this, "hovered", false);
		break;
	}
	}
	return QPushButton::event(event);
}

bool PLSNaverShoppingLIVESearchKeywordItemView::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->textLabel) {
		switch (event->type()) {
		case QEvent::Resize: {
			QString text = ui->textLabel->fontMetrics().elidedText(searchKeyword, Qt::ElideRight, ui->textLabel->width());
			ui->textLabel->setText(text);
			break;
		}
		}
	}

	return QPushButton::eventFilter(watched, event);
}
