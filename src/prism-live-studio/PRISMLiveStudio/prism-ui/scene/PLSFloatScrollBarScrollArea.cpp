#include "PLSFloatScrollBarScrollArea.h"
//#include "PLSDpiHelper.h"
#include "libutils-api.h"
#include <QTimer>

PLSFloatScrollBarScrollArea::PLSFloatScrollBarScrollArea(QWidget *parent) : QScrollArea(parent)
{
	this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	delegateScrollBar = pls_new<QScrollBar>(Qt::Vertical, this);
	delegateScrollBar->setObjectName("flatScrollBar");
	delegateScrollBar->installEventFilter(this);

	connect(this->verticalScrollBar(), &QScrollBar::valueChanged, this, &PLSFloatScrollBarScrollArea::OnVerticalScrollBarValueChanged, Qt::QueuedConnection);

	connect(this->verticalScrollBar(), &QScrollBar::rangeChanged, this, &PLSFloatScrollBarScrollArea::OnRangeChanged, Qt::QueuedConnection);

	connect(delegateScrollBar, &QScrollBar::valueChanged, [this](int value) { this->verticalScrollBar()->setValue(value); });

	connect(delegateScrollBar, &QScrollBar::rangeChanged, [this](int min, int max) {
		this->verticalScrollBar()->blockSignals(true);
		this->verticalScrollBar()->setRange(min, max);
		this->verticalScrollBar()->blockSignals(false);
	});
}

void PLSFloatScrollBarScrollArea::SetScrollBarRightMargin(int rightMargin_)
{
	rightMargin = rightMargin_;
}

void PLSFloatScrollBarScrollArea::setTopMargin(int value)
{
	topMarginValue = value;
	UpdateSlider();
}

int PLSFloatScrollBarScrollArea::topMargin() const
{
	return topMarginValue;
}

void PLSFloatScrollBarScrollArea::setBottomMargin(int value)
{
	bottomMarginValue = value;
	UpdateSlider();
}

int PLSFloatScrollBarScrollArea::bottomMargin() const
{
	return bottomMarginValue;
}

void PLSFloatScrollBarScrollArea::resizeEvent(QResizeEvent *event)
{
	UpdateSlider();
	QScrollArea::resizeEvent(event);
}

bool PLSFloatScrollBarScrollArea::eventFilter(QObject *watcher, QEvent *e)
{
	if (watcher == delegateScrollBar) {
		if (e->type() == QEvent::Show) {
			QTimer::singleShot(0, this, &PLSFloatScrollBarScrollArea::UpdateSliderGeometry);
		}
	}
	return QScrollArea::eventFilter(watcher, e);
}

void PLSFloatScrollBarScrollArea::OnVerticalScrollBarValueChanged(int value)
{
	int pageStep = this->verticalScrollBar()->pageStep();
	int singleStep = this->verticalScrollBar()->singleStep();
	int maxValue = this->verticalScrollBar()->maximum();
	int minValue = this->verticalScrollBar()->minimum();
	delegateScrollBar->setPageStep(pageStep);
	delegateScrollBar->setSingleStep(singleStep);
	delegateScrollBar->blockSignals(true);
	delegateScrollBar->setValue(value);
	delegateScrollBar->blockSignals(false);

	if (value == maxValue)
		emit ScrolledToEnd();
	else if (value == minValue)
		emit ScrolledToTop();
}

void PLSFloatScrollBarScrollArea::OnRangeChanged(int, int)
{
	delegateScrollBar->blockSignals(true);
	UpdateSlider();
	emit ScrollBarRangChanged();
	delegateScrollBar->blockSignals(false);
}

void PLSFloatScrollBarScrollArea::UpdateSlider()
{
	UpdateSliderGeometry();
	int min = this->verticalScrollBar()->minimum();
	int max = this->verticalScrollBar()->maximum();
	bool needShow = max > 0;
	int pageStep = this->verticalScrollBar()->pageStep();
	int singleStep = this->verticalScrollBar()->singleStep();
	if (delegateScrollBar->isVisible() != needShow) {
		emit ScrollBarVisibleChanged(needShow);
	}
	delegateScrollBar->setVisible(needShow);
	delegateScrollBar->setPageStep(pageStep);
	delegateScrollBar->setSingleStep(singleStep);
	delegateScrollBar->setMinimum(min);
	delegateScrollBar->setMaximum(max);
}

void PLSFloatScrollBarScrollArea::UpdateSliderGeometry()
{
	delegateScrollBar->move(width() - delegateScrollBar->width() - rightMargin, topMarginValue);
	delegateScrollBar->resize(delegateScrollBar->width(), height() - topMarginValue + bottomMarginValue);
}
