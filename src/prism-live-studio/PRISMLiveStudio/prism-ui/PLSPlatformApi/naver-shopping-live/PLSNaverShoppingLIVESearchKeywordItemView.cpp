#include "PLSNaverShoppingLIVESearchKeywordItemView.h"
#include "ui_PLSNaverShoppingLIVESearchKeywordItemView.h"
#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSNaverShoppingLIVEItemViewCache.h"
#include "frontend-api.h"
#include "libui.h"
#include <QMouseEvent>
#include <QDesktopServices>

using ItemViewCache = PLSNaverShoppingLIVEItemViewCache<PLSNaverShoppingLIVESearchKeywordItemView>;

PLSNaverShoppingLIVESearchKeywordItemView::PLSNaverShoppingLIVESearchKeywordItemView(QWidget *parent) : QPushButton(parent)
{
	ui = pls_new<Ui::PLSNaverShoppingLIVESearchKeywordItemView>();
	setAttribute(Qt::WA_Hover);
	ui->setupUi(this);

	setDefault(false);
	setAutoDefault(false);
	setFocusPolicy(Qt::NoFocus);

	ui->textLabel->installEventFilter(this);
	ui->removeButton->setAutoDefault(false);
	ui->removeButton->setDefault(false);
	connect(ui->removeButton, &QPushButton::clicked, this, [this]() { removeButtonClicked(this); });
	connect(this, &QPushButton::clicked, this, [this]() { searchButtonClicked(this); });
}

PLSNaverShoppingLIVESearchKeywordItemView::~PLSNaverShoppingLIVESearchKeywordItemView()
{
	pls_delete(ui, nullptr);
}

QString PLSNaverShoppingLIVESearchKeywordItemView::getSearchKeyword() const
{
	return searchKeyword;
}

void PLSNaverShoppingLIVESearchKeywordItemView::setInfo(const QString &searchKeyword_)
{
	this->searchKeyword = searchKeyword_;
	ui->textLabel->setText(searchKeyword_);
}

void PLSNaverShoppingLIVESearchKeywordItemView::addBatchCache(int batchCount, bool check)
{
	ItemViewCache::instance()->addBatchCache(batchCount, check);
}

void PLSNaverShoppingLIVESearchKeywordItemView::cleaupCache()
{
	ItemViewCache::instance()->cleaupCache();
}

PLSNaverShoppingLIVESearchKeywordItemView *PLSNaverShoppingLIVESearchKeywordItemView::alloc(QWidget *parent, const QString &searchKeyword)
{
	return ItemViewCache::instance()->alloc(parent, searchKeyword);
}

void PLSNaverShoppingLIVESearchKeywordItemView::dealloc(PLSNaverShoppingLIVESearchKeywordItemView *view)
{
	QObject::disconnect(view->removeButtonClickedConnection);
	QObject::disconnect(view->searchButtonClickedConnection);
	ItemViewCache::instance()->dealloc(view);
}

void PLSNaverShoppingLIVESearchKeywordItemView::dealloc(QList<PLSNaverShoppingLIVESearchKeywordItemView *> &views, QVBoxLayout *layout)
{
	ItemViewCache::instance()->dealloc(views, layout, [](const PLSNaverShoppingLIVESearchKeywordItemView *view) {
		QObject::disconnect(view->removeButtonClickedConnection);
		QObject::disconnect(view->searchButtonClickedConnection);
	});
}

void PLSNaverShoppingLIVESearchKeywordItemView::updateSearchKeyword(const QList<PLSNaverShoppingLIVESearchKeywordItemView *> &views)
{
	for (auto view : views) {
		QString text = view->ui->textLabel->fontMetrics().elidedText(view->searchKeyword, Qt::ElideRight, view->ui->textLabel->width());
		view->ui->textLabel->setText(text);
	}
}

bool PLSNaverShoppingLIVESearchKeywordItemView::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::HoverEnter:
		pls_flush_style_recursive(this, "hovered", true);
		break;
	case QEvent::HoverLeave:
		pls_flush_style_recursive(this, "hovered", false);
		break;
	default:
		break;
	}
	return QPushButton::event(event);
}

bool PLSNaverShoppingLIVESearchKeywordItemView::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->textLabel) {
		switch (event->type()) {
		case QEvent::Resize:
			ui->textLabel->setText(ui->textLabel->fontMetrics().elidedText(searchKeyword, Qt::ElideRight, ui->textLabel->width()));
			break;
		default:
			break;
		}
	}

	return QPushButton::eventFilter(watched, event);
}
