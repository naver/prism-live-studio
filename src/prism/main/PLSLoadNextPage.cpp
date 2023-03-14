#include "PLSLoadNextPage.h"

#include "PLSDpiHelper.h"

#include <QScrollArea>
#include <QScrollBar>

#define SCROLL_END_RANGE 15

PLSLoadNextPage::PLSLoadNextPage(QScrollArea *scrollArea)
	: QObject(scrollArea), scrollBarValueChangedConnection(), scrollBar(scrollArea->verticalScrollBar()), scrollEndRange(PLSDpiHelper::calculate(scrollArea, SCROLL_END_RANGE)), signalEmited(false)
{
	scrollBarValueChangedConnection = connect(scrollBar, &QScrollBar::valueChanged, this, &PLSLoadNextPage::onScrollBarValueChanged);
}

PLSLoadNextPage::~PLSLoadNextPage()
{
	QObject::disconnect(scrollBarValueChangedConnection);
}

PLSLoadNextPage *PLSLoadNextPage::newLoadNextPage(PLSLoadNextPage *&loadNextPage, QScrollArea *scrollArea)
{
	loadNextPage = new PLSLoadNextPage(scrollArea);
	return loadNextPage;
}

void PLSLoadNextPage::deleteLoadNextPage(PLSLoadNextPage *&loadNextPage)
{
	if (loadNextPage) {
		delete loadNextPage;
		loadNextPage = nullptr;
	}
}

void PLSLoadNextPage::onScrollBarValueChanged(int value)
{
	if (!signalEmited && (value >= (scrollBar->maximum() - scrollEndRange))) {
		signalEmited = true;
		emit loadNextPage();
	}
}
