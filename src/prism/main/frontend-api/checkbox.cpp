#include "checkbox.hpp"

#include "frontend-api.h"
#include "PLSDpiHelper.h"

PLSElideCheckBox::PLSElideCheckBox(QWidget *parent) : QCheckBox(parent) {}

PLSElideCheckBox::PLSElideCheckBox(const QString &text, QWidget *parent) : QCheckBox(text, parent), originText(text) {}

PLSElideCheckBox::~PLSElideCheckBox() {}

void PLSElideCheckBox::setText(const QString &text)
{
	originText = text;
	updateText();
}

QString PLSElideCheckBox::text() const
{
	return originText;
}

QSize PLSElideCheckBox::sizeHint() const
{
	return QSize(0, QCheckBox::sizeHint().height());
}

QSize PLSElideCheckBox::minimumSizeHint() const
{
	return QSize(0, QCheckBox::minimumSizeHint().height());
}

void PLSElideCheckBox::updateText()
{
	if (originText.isEmpty()) {
		QCheckBox::setText(originText);
		return;
	}

	double spacing = PLSDpiHelper::calculate(this, 10);
	QString text = this->fontMetrics().elidedText(originText, Qt::ElideRight, width() - iconSize().width() - spacing);
	QCheckBox::setText(text);
}

void PLSElideCheckBox::resizeEvent(QResizeEvent *event)
{
	QCheckBox::resizeEvent(event);
	updateText();
}
