#include "label.hpp"

#include "frontend-api.h"

PLSElideLabel::PLSElideLabel(QWidget *parent) : QLabel(parent) {}

PLSElideLabel::PLSElideLabel(const QString &text, QWidget *parent) : QLabel(text, parent) {}

PLSElideLabel::~PLSElideLabel() {}

void PLSElideLabel::setText(const QString &text)
{
	originText = text;
	updateText();
}

QString PLSElideLabel::text() const
{
	return originText;
}

QSize PLSElideLabel::sizeHint() const
{
	return QSize(0, QLabel::sizeHint().height());
}

QSize PLSElideLabel::minimumSizeHint() const
{
	return QSize(0, QLabel::minimumSizeHint().height());
}

void PLSElideLabel::updateText()
{
	if (originText.isEmpty()) {
		QLabel::setText(originText);
		return;
	}

	QString text = this->fontMetrics().elidedText(originText, Qt::ElideRight, width());
	QLabel::setText(text);
}

void PLSElideLabel::resizeEvent(QResizeEvent *event)
{
	QLabel::resizeEvent(event);
	updateText();
}
