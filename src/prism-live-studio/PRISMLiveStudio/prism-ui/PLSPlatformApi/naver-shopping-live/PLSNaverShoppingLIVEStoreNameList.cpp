#include "PLSNaverShoppingLIVEStoreNameList.h"
#include "PLSNaverShoppingLIVEProductDialogView.h"
#include "frontend-api.h"
#include "vertical-scroll-area.hpp"
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QScrollArea>

bool PLSNaverShoppingLIVEStoreNameList::storesNameListVisible = false;

PLSNaverShoppingLIVESmartStoreLabel::PLSNaverShoppingLIVESmartStoreLabel(double dpi_, const QString &storeId_, const QString &storeName_, bool selected, QWidget *parent)
	: QLabel(storeName_, parent), dpi(dpi_), storeId(storeId_), storeName(storeName_)
{
	setAttribute(Qt::WA_Hover);
	setMouseTracking(true);
	setObjectName("smartStoreLabel");
	setProperty("selected", selected);
}

bool PLSNaverShoppingLIVESmartStoreLabel::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::MouseButtonRelease:
		storeSelected(storeId, storeName);
		break;
	case QEvent::HoverEnter:
		pls_flush_style(this, "hovered", true);
		break;
	case QEvent::HoverLeave:
		pls_flush_style(this, "hovered", false);
		break;
	case QEvent::Resize:
		setText(fontMetrics().elidedText(storeName, Qt::ElideRight, static_cast<QResizeEvent *>(event)->size().width() - 16 * 2));
		break;
	default:
		break;
	}
	return QLabel::event(event);
}

PLSNaverShoppingLIVEStoreNameList::PLSNaverShoppingLIVEStoreNameList(double dpi, QWidget *droplistButton_, PLSNaverShoppingLIVEProductDialogView *productDialogView)
	: QFrame(productDialogView, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint), droplistButton(droplistButton_)
{
	setAttribute(Qt::WA_DeleteOnClose, true);

	int _8px = 8;

	QVBoxLayout *thisLayout = pls_new<QVBoxLayout>(this);
	thisLayout->setContentsMargins(0, _8px, 0, _8px);
	thisLayout->setSpacing(0);

	scrollArea = pls_new<VScrollArea>(this);
	scrollArea->setObjectName("scrollArea");
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setWidgetResizable(true);
	thisLayout->addWidget(scrollArea);

	QWidget *scrollAreaContent = pls_new<QWidget>();
	scrollAreaContent->setObjectName("scrollAreaContent");

	QVBoxLayout *scrollAreaContentLayout = pls_new<QVBoxLayout>(scrollAreaContent);
	scrollAreaContentLayout->setContentsMargins(0, 0, 0, 0);
	scrollAreaContentLayout->setSpacing(0);

	for (const auto &smartStore : productDialogView->smartStores) {
		bool selected = smartStore.storeId == productDialogView->currentSmartStore.storeId;
		smartStoreLabel = pls_new<PLSNaverShoppingLIVESmartStoreLabel>(dpi, smartStore.storeId, smartStore.storeName, selected, scrollAreaContent);
		scrollAreaContentLayout->addWidget(smartStoreLabel);
		connect(smartStoreLabel, &PLSNaverShoppingLIVESmartStoreLabel::storeSelected, this, [this](const QString &storeId, const QString &storeName) {
			emit storeSelected(storeId, storeName);
			close();
		});
		if (selected) {
			currentSmartStoreLabel = smartStoreLabel;
		}
	}

	scrollArea->setWidget(scrollAreaContent);

	verticalScrollBar = scrollArea->verticalScrollBar();
	if (verticalScrollBar) {
		verticalScrollBar->installEventFilter(this);
	}
}

PLSNaverShoppingLIVEStoreNameList ::~PLSNaverShoppingLIVEStoreNameList()
{
	if (!QRect(droplistButton->mapToGlobal(QPoint(0, 0)), droplistButton->size()).contains(QCursor::pos())) {
		storesNameListVisible = false;
	}
}

void PLSNaverShoppingLIVEStoreNameList::popup(QWidget *droplistButton, PLSNaverShoppingLIVEProductDialogView *productDialogView,
					      const std::function<void(const QString &storeId, const QString &storeName)> &callback)
{
	if (storesNameListVisible) {
		storesNameListVisible = false;
		return;
	}

	storesNameListVisible = true;

	double dpi = 1;

	auto storeNameList = pls_new<PLSNaverShoppingLIVEStoreNameList>(dpi, droplistButton, productDialogView);
	connect(storeNameList, &PLSNaverShoppingLIVEStoreNameList::storeSelected, productDialogView, callback);
	pls_flush_style_recursive(storeNameList);

	QPoint pos = droplistButton->mapToGlobal(QPoint(0, droplistButton->height() + 10));
	int _8px = 8;
	int _20px = 20;
	int _40px = 40;
	int _180px = 180;
	int _380px = 380;

	QSize size(1, 1);

	for (const auto &smartStore : productDialogView->smartStores) {
		size.setWidth(qMax(size.width(), _20px + storeNameList->smartStoreLabel->fontMetrics().horizontalAdvance(smartStore.storeName) + _20px));
	}
	size.setWidth(qMin(qMax(_180px, size.width()), _380px));

	int smartStoreCount = productDialogView->smartStores.count();
	int visibleItems = (smartStoreCount <= MaxVisibleItems) ? smartStoreCount : MaxVisibleItems;
	size.setHeight(_8px + visibleItems * _40px + _8px);

	storeNameList->setFixedSize(size);
	storeNameList->move(pos.x(), pos.y());
	storeNameList->show();
}

bool PLSNaverShoppingLIVEStoreNameList::eventFilter(QObject *watched, QEvent *event)
{
	if (verticalScrollBar && (verticalScrollBar == watched)) {
		switch (event->type()) {
		case QEvent::Show:
			setFixedWidth(width() + verticalScrollBar->width());
			break;
		default:
			break;
		}
	}

	return QFrame::eventFilter(watched, event);
}

void PLSNaverShoppingLIVEStoreNameList::resizeEvent(QResizeEvent *event)
{
	QFrame::resizeEvent(event);

	if (currentSmartStoreLabel) {
		scrollArea->ensureWidgetVisible(currentSmartStoreLabel);
	}
}
