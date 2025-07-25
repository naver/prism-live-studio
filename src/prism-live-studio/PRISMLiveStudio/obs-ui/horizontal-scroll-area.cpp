#include <QResizeEvent>
#include "moc_horizontal-scroll-area.cpp"

void HScrollArea::resizeEvent(QResizeEvent *event)
{
#if 0 
	if (!!widget())
		widget()->setMaximumHeight(event->size().height());
#endif

	QScrollArea::resizeEvent(event);
}
