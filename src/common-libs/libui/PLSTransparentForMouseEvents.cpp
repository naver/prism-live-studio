#include "PLSTransparentForMouseEvents.h"

#include <qcoreevent.h>
#include <qapplication.h>
#include <qabstractbutton.h>
#include <qlineedit.h>
#include <qcombobox.h>
#include <qabstractspinbox.h>
#include <qabstractslider.h>
#include <qframe.h>
#include <QTextEdit>

#include <libutils-api.h>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace pls {
namespace ui {

static CheckResult checkChild(QWidget *widget, const CustomChecker &customChecker)
{
	QWidget *childWidget = QApplication::widgetAt(QCursor::pos());
	if (!childWidget || widget == childWidget) {
		return CheckResult::Include;
	} else if (auto result = pls_invoke_safe(CheckResult::Unknown, customChecker, widget, childWidget); result != CheckResult::Unknown) {
		return result;
	}

	if (QString excludeChildren = widget->property("excludeChildren").toString(); excludeChildren == QStringLiteral("All")) {
		return CheckResult::Exclude;
	} else if (QStringList excludeSpecChildren = widget->property("excludeSpecChildren").toStringList(); excludeSpecChildren.contains(childWidget->objectName())) {
		return CheckResult::Exclude;
	}
	return CheckResult::Include;
}

LIBUI_API bool transparentForMouseEvents_nativeEvent(QWidget *widget, const QByteArray &eventType, void *message, qintptr *result, const CustomChecker &customChecker)
{
#ifdef Q_OS_WIN
	PMSG msg = (PMSG)message;
	switch (msg->message) {
	case WM_NCHITTEST:
		if (checkChild(widget, customChecker) == CheckResult::Include) {
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

LIBUI_API bool transparentForMouseEvents_moveInContentIncludeChild(QWidget *parentWidget, QWidget *childWidget)
{
	auto mo = childWidget->metaObject();
	if ((mo == &QLabel::staticMetaObject)    //
	    || (mo == &QFrame::staticMetaObject) //
	    || (mo == &QWidget::staticMetaObject)) {
		for (QWidget *widget = childWidget->parentWidget(); widget && widget != parentWidget; widget = widget->parentWidget()) {
			if (transparentForMouseEvents_moveInContentExcludeChild(parentWidget, widget)) {
				return false;
			}
		}
		return true;
	}
	return false;
}
LIBUI_API bool transparentForMouseEvents_moveInContentExcludeChild(QWidget *parentWidget, QWidget *childWidget)
{
	if (parentWidget == childWidget) {
		return false;
	}

	auto mo = childWidget->metaObject();
	if (mo->inherits(&QAbstractButton::staticMetaObject)     //
	    || mo->inherits(&QLineEdit::staticMetaObject)        //
	    || mo->inherits(&QComboBox::staticMetaObject)        //
	    || mo->inherits(&QAbstractSpinBox::staticMetaObject) //
	    || mo->inherits(&QAbstractSlider::staticMetaObject) 
		|| mo->inherits(&QTextEdit::staticMetaObject)) {
		return true;
	}
	return false;
}

}
}
