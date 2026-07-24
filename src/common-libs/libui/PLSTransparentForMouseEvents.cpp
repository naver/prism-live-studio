#include "PLSTransparentForMouseEvents.h"

#include <qcoreevent.h>
#include <qapplication.h>
#include <qabstractbutton.h>
#include <qlineedit.h>
#include <qcombobox.h>
#include <qabstractspinbox.h>
#include <qabstractslider.h>
#include <qframe.h>
#include <qtextedit.h>
#include <qlistview.h>
#include <qstackedwidget.h>
#include <qscrollarea.h>

#include <libutils-api.h>

#include "PLSCheckBox.h"
#include "PLSRadioButton.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace pls {
namespace ui {

static bool isExclude(const QObjectList &children, const MoveExcludeChecker &excludeChecker, const QPoint &pos)
{
	for (auto i = children.rbegin(), e = children.rend(); i != e; ++i) {
		if (auto object = *i; !object->isWidgetType())
			continue;
		else if (auto widget = static_cast<QWidget *>(object); !widget->isVisible())
			continue;
		else if (!widget->rect().contains(widget->mapFromGlobal(pos)))
			continue;
		else if (!excludeChecker(widget))
			return isExclude(widget->children(), excludeChecker, pos);
		else
			return true;
	}
	return false;
}

LIBUI_API bool transparentForMouseEvents_nativeEvent(QWidget *widget, const QByteArray &eventType, void *message, qintptr *result, const MoveExcludeChecker &excludeChecker)
{
#ifdef Q_OS_WIN
	PMSG msg = (PMSG)message;
	switch (msg->message) {
	case WM_NCHITTEST:
		if (!excludeChecker || !isExclude(widget->children(), excludeChecker, QCursor::pos())) {
			*result = HTTRANSPARENT;
			return true;
		}
		break;
	default:
		break;
	}
#endif
	return false;
}

LIBUI_API bool transparentForMouseEvents_moveExcludeChild(QWidget *child)
{
	if (auto mo = child->metaObject(); mo->inherits(&QAbstractButton::staticMetaObject)     //
					   || mo->inherits(&QLineEdit::staticMetaObject)        //
					   || mo->inherits(&QComboBox::staticMetaObject)        //
					   || mo->inherits(&QAbstractSpinBox::staticMetaObject) //
					   || mo->inherits(&QAbstractSlider::staticMetaObject)  //
					   || mo->inherits(&QTextEdit::staticMetaObject)        //
					   || mo->inherits(&PLSCheckBox::staticMetaObject)      //
					   || mo->inherits(&PLSRadioButton::staticMetaObject)   //
					   || mo->inherits(&QListView::staticMetaObject)        //
					   || mo->inherits(&QScrollArea::staticMetaObject)      //
					   || mo->inherits(&QStackedWidget::staticMetaObject)) {
		return true;
	}
	return false;
}

}
}
