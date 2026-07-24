#include "PLSSpinBox.h"

#include <QWheelEvent>
#include <QLineEdit>

PLSSpinBox::PLSSpinBox(QWidget *parent) : QSpinBox(parent)
{
#if defined(PLS_UI_ACTION_STATS)
	connect(this, &PLSSpinBox::valueChanged, this, [this](int value) {
		auto title = pls_uistep_v2_get_title(this).toUtf8();
		auto name = pls_uistep_v2_get_name(this, PLS_UI_STEPS_V2_SIGNAL_VALUECHANGED).toUtf8();
		PLS_UI_ACTION("In %s, SpinBox: %s, Value Changed: %d.", title.constData(), name.constData(), value);
	});
#endif
	// setButtonSymbols(QAbstractSpinBox::ButtonSymbols::NoButtons)
}

void PLSSpinBox::makeTextVCenter() const
{
	QLineEdit *edit = this->findChild<QLineEdit *>();
	if (edit) {
		edit->setContentsMargins({0, 0, 0, 1});
	}
}

void PLSSpinBox::stepBy(int steps)
{
#if defined(PLS_UI_ACTION_STATS)
	auto title = pls_uistep_v2_get_title(this).toUtf8();
	auto name = pls_uistep_v2_get_name(this, PLS_UI_STEPS_V2_SIGNAL_VALUECHANGED).toUtf8();
	PLS_UI_ACTION((steps > 0) ? "In %s, Click SpinBox: %s Step Up Button" : "In %s, Click SpinBox: %s Step Down Button", title.constData(), name.constData());
#endif

	QSpinBox::stepBy(steps);
}

void PLSSpinBox::wheelEvent(QWheelEvent *event)
{
	event->ignore();
}

PLSDoubleSpinBox::PLSDoubleSpinBox(QWidget *parent) : QDoubleSpinBox(parent)
{
#if defined(PLS_UI_ACTION_STATS)
	connect(this, &PLSDoubleSpinBox::valueChanged, this, [this](double value) {
		auto title = pls_uistep_v2_get_title(this).toUtf8();
		auto name = pls_uistep_v2_get_name(this, PLS_UI_STEPS_V2_SIGNAL_VALUECHANGED).toUtf8();
		PLS_UI_ACTION("In %s, DoubleSpinBox: %s, Value Changed: %lf.", title.constData(), name.constData(), value);
	});
#endif
	// setButtonSymbols(QAbstractSpinBox::ButtonSymbols::NoButtons)
}

void PLSDoubleSpinBox::stepBy(int steps)
{
#if defined(PLS_UI_ACTION_STATS)
	auto title = pls_uistep_v2_get_title(this).toUtf8();
	auto name = pls_uistep_v2_get_name(this, PLS_UI_STEPS_V2_SIGNAL_VALUECHANGED).toUtf8();
	PLS_UI_ACTION((steps > 0) ? "In %s, Click DoubleSpinBox: %s Step Up Button" : "In %s, Click DoubleSpinBox: %s Step Down Button", title.constData(), name.constData());
#endif

	QDoubleSpinBox::stepBy(steps);
}

void PLSDoubleSpinBox::wheelEvent(QWheelEvent *event)
{
	event->ignore();
}

QValidator::State PLSSpinBox::validate(QString &input, int &pos) const
{
	const int minVal = minimum();
	const int maxVal = maximum();
	const bool zeroInRange = (minVal <= 0 && maxVal >= 0);
	const QString pfx = prefix();
	const QString sfx = suffix();

	// Strip prefix before numeric validation
	QString numStr = input;
	if (!pfx.isEmpty()) {
		if (numStr.startsWith(pfx))
			numStr.remove(0, pfx.size());
		else
			return QValidator::Invalid;
	}

	// Strip suffix (full or partial) and track whether it is complete
	bool suffixComplete = sfx.isEmpty();
	if (!sfx.isEmpty()) {
		if (numStr.endsWith(sfx)) {
			numStr.chop(sfx.size());
			suffixComplete = true;
		} else {
			// Check for a partial suffix at the end of the string
			for (int i = qMin(sfx.size(), numStr.size()); i >= 1; --i) {
				if (sfx.startsWith(numStr.right(i))) {
					numStr.chop(i);
					break;
				}
			}
			// suffixComplete remains false — at most Intermediate
		}
	}

	// Existing numeric validation applied to numStr
	if (numStr.isEmpty() || (minVal < 0 && numStr == "-"))
		return QValidator::Intermediate;

	if (numStr.size() > 1 && numStr[0] == QLatin1Char('0')) {
		return QValidator::Invalid;
	}

	if (!zeroInRange && numStr == QLatin1String("0")) {
		return QValidator::Invalid;
	}

	if (numStr.size() >= 2 && numStr[0] == QLatin1Char('-') && numStr[1] == QLatin1Char('0')) {
		return QValidator::Invalid;
	}

	bool ok = false;
	const int v = numStr.toInt(&ok);
	if (!ok)
		return QValidator::Invalid;

	if (v >= minVal && v <= maxVal)
		return suffixComplete ? QValidator::Acceptable : QValidator::Intermediate;

	if (v < minVal) {
		if (minVal >= 0 && v >= 0) {
			return QValidator::Intermediate;
		}
		return QValidator::Invalid;
	}

	if (v > maxVal) {
		if (maxVal < 0) {
			return QValidator::Intermediate;
		}
		return QValidator::Invalid;
	}

	return QValidator::Acceptable;
}