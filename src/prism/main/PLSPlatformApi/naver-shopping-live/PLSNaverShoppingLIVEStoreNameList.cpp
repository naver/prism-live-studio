#include "PLSNaverShoppingLIVEStoreNameList.h"
#include "PLSNaverShoppingLIVEProductDialogView.h"

#include "PLSDpiHelper.h"
#include "frontend-api.h"
#include "vertical-scroll-area.hpp"

#include <QVBoxLayout>
#include <QScrollBar>
#include <QScrollArea>

static bool storesNameListVisible = false;

PLSNaverShoppingLIVESmartStoreLabel::PLSNaverShoppingLIVESmartStoreLabel(double dpi_, const QString &storeId_, const QString &storeName_, bool selected, QWidget *parent)
	: QLabel(storeName_, parent), dpi(dpi_), storeId(storeId_), storeName(storeName_)
{
	setAttribute(Qt::WA_Hover);
	setMouseTracking(true);
	setObjectName("smartStoreLabel");
	setProperty("selected", selected);
}

PLSNaverShoppingLIVESmartStoreLabel::~PLSNaverShoppingLIVESmartStoreLabel() {}

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
	case QEvent::Resize: {
		setText(fontMetrics().elidedText(storeName, Qt::ElideRight, static_cast<QResizeEvent *>(event)->size().width() - PLSDpiHelper::calculate(dpi, 16) * 2));
		break;
	}
	}
	return QLabel::event(event);
}

PLSNaverShoppingLIVEStoreNameList::PLSNaverShoppingLIVEStoreNameList(double dpi, QWidget *droplistButton_, PLSNaverShoppingLIVEProductDialogView *productDialogView)
	: QFrame(productDialogView, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint), droplistButton(droplistButton_)
{
	setAttribute(Qt::WA_DeleteOnClose, true);

	int _8px = PLSDpiHelper::calculate(dpi, 8);

	QVBoxLayout *thisLayout = new QVBoxLayout(this);
	thisLayout->setContentsMargins(0, _8px, 0, _8px);
	thisLayout->setSpacing(0);

	scrollArea = new VScrollArea(this);
	scrollArea->setObjectName("scrollArea");
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setWidgetResizable(true);
	thisLayout->addWidget(scrollArea);

	QWidget *scrollAreaContent = new QWidget();
	scrollAreaContent->setObjectName("scrollAreaContent");

	QVBoxLayout *scrollAreaContentLayout = new QVBoxLayout(scrollAreaContent);
	scrollAreaContentLayout->setContentsMargins(0, 0, 0, 0);
	scrollAreaContentLayout->setSpacing(0);

	for (const auto &smartStore : productDialogView->smartStores) {
		bool selected = smartStore.storeId == productDialogView->currentSmartStore.storeId;
		smartStoreLabel = new PLSNaverShoppingLIVESmartStoreLabel(dpi, smartStore.storeId, smartStore.storeName, selected, scrollAreaContent);
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
					      std::function<void(const QString &storeId, const QString &storeName)> callback)
{
	if (storesNameListVisible) {
		storesNameListVisible = false;
		return;
	}

	storesNameListVisible = true;

	double dpi = PLSDpiHelper::getDpi(productDialogView);

	auto storeNameList = new PLSNaverShoppingLIVEStoreNameList(dpi, droplistButton, productDialogView);
	connect(storeNameList, &PLSNaverShoppingLIVEStoreNameList::storeSelected, productDialogView, callback);
	pls_flush_style_recursive(storeNameList);

	QPoint pos = droplistButton->mapToGlobal(QPoint(0, droplistButton->height() + PLSDpiHelper::calculate(dpi, 10)));
	int _8px = PLSDpiHelper::calculate(dpi, 8);
	int _20px = PLSDpiHelper::calculate(dpi, 20);
	int _40px = PLSDpiHelper::calculate(dpi, 40);
	int _180px = PLSDpiHelper::calculate(dpi, 180);
	int _380px = PLSDpiHelper::calculate(dpi, 380);

	QSize size(1, 1);

	for (const auto &smartStore : productDialogView->smartStores) {
		size.setWidth(qMax(size.width(), _20px + storeNameList->smartStoreLabel->fontMetrics().width(smartStore.storeName) + _20px));
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
		case QEvent::Show: {
			setFixedWidth(width() + verticalScrollBar->width());
			break;
		}
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
