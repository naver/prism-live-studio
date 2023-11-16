#include "PLSLoadNextPage.h"
#include <QScrollArea>
#include <QScrollBar>

const int SCROLL_END_RANGE = 15;

PLSLoadNextPage::PLSLoadNextPage(QScrollArea *scrollArea)
	: QObject(scrollArea),
	  scrollBar(scrollArea->verticalScrollBar()),
	  scrollBarValueChangedConnection(connect(scrollBar, &QScrollBar::valueChanged, this, &PLSLoadNextPage::onScrollBarValueChanged)),
	  scrollEndRange(SCROLL_END_RANGE)
{
}

PLSLoadNextPage *PLSLoadNextPage::newLoadNextPage(PLSLoadNextPage *&loadNextPage, QScrollArea *scrollArea)
{
	loadNextPage = pls_new<PLSLoadNextPage>(scrollArea);
	return loadNextPage;
}

void PLSLoadNextPage::deleteLoadNextPage(PLSLoadNextPage *&loadNextPage)
{
	pls_delete(loadNextPage, nullptr);
}

void PLSLoadNextPage::onScrollBarValueChanged(int value)
{
	if (!signalEmited && (value >= (scrollBar->maximum() - scrollEndRange))) {
		signalEmited = true;
		emit loadNextPage();
	}
}
