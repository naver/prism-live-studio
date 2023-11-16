#include <QResizeEvent>
#include "horizontal-scroll-area.hpp"

void HScrollArea::resizeEvent(QResizeEvent *event)
{
#if 0 
	if (!!widget())
		widget()->setMaximumHeight(event->size().height());
#endif

	QScrollArea::resizeEvent(event);
}
