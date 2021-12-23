#include "PLSLabel.hpp"
#include "PLSDpiHelper.h"
#include <QTimer>

PLSLabel::PLSLabel(QWidget *parent, bool showTooltip) : QLabel(parent)
{
	this->showTooltip = showTooltip;
}

PLSLabel::PLSLabel(const QString &text, bool showTooltip, QWidget *parent) : QLabel(parent)
{
	this->showTooltip = showTooltip;
	SetText(text);
}

PLSLabel::~PLSLabel() {}

void PLSLabel::SetText(const QString &text)
{
	this->realText = text;
	this->setText(GetNameElideString());

	if (showTooltip)
		setToolTip(text);
}

QString PLSLabel::Text()
{
	return realText;
}

void PLSLabel::resizeEvent(QResizeEvent *event)
{
	QLabel::resizeEvent(event);
	QTimer::singleShot(0, this, [=]() { this->setText(GetNameElideString()); });
}

QString PLSLabel::GetNameElideString()
{
	QFontMetrics fontWidth(this->font());
	if (fontWidth.width(realText) > this->width()) {
		return fontWidth.elidedText(realText, Qt::ElideRight, this->width());
	}

	return realText;
}

CombinedLabel::CombinedLabel(QWidget *parent) : QLabel(parent)
{
	additionalLabel = new QLabel(this);
	additionalLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	additionalLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	additionalLabel->setObjectName("additionalLabel");
	additionalLabel->hide();

	middleLabel = new QLabel(this);
	middleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	middleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	middleLabel->setObjectName("middleLabel");
	middleLabel->hide();
}

CombinedLabel::~CombinedLabel() {}

void CombinedLabel::SetShowText(const QString &strText_)
{
	strText = strText_;
	UpdataUi();
}

void CombinedLabel::SetAdditionalText(const QString &strText_)
{
	strAdditional = strText_;
	additionalLabel->setText(strText_);
	QTimer::singleShot(0, this, [=]() {
		int width = additionalLabel->fontMetrics().width(strText_);
		additionalLabel->setFixedSize(width, this->height());
		UpdataUi();
	});
}

void CombinedLabel::UpdataUi()
{
	int textWidth = GetTextSpacing();
	QString displayText = strText;
	if (this->fontMetrics().width(strText) > textWidth) {
		displayText = fontMetrics().elidedText(strText, Qt::ElideRight, textWidth);
	}
	setText(displayText);

	if (middleLabel->isVisible()) {
		middleLabel->move(fontMetrics().width(displayText) + PLSDpiHelper::calculate(this, spacing), (height() - middleLabel->height()) / 2);
		if (additionalLabel->isVisible()) {
			additionalLabel->move(middleLabel->width() + fontMetrics().width(displayText) + PLSDpiHelper::calculate(this, spacing), (height() - additionalLabel->height()) / 2);
		}
	} else {
		if (additionalLabel->isVisible()) {
			additionalLabel->move(fontMetrics().width(displayText) + PLSDpiHelper::calculate(this, spacing), (height() - additionalLabel->height()) / 2);
		}
	}
}

void CombinedLabel::SetSpacing(int spacing_)
{
	spacing = spacing_;
	UpdataUi();
}

void CombinedLabel::SetAdditionalVisible(bool visible)
{
	additionalLabel->setVisible(visible);
	UpdataUi();
}

void CombinedLabel::SetMiddleVisible(bool visible)
{
	middleLabel->setVisible(visible);
	UpdataUi();
}

void CombinedLabel::resizeEvent(QResizeEvent *event)
{
	UpdataUi();
	QLabel::resizeEvent(event);
	QTimer::singleShot(0, this, [=]() {
		if (!strAdditional.isEmpty()) {
			int width = additionalLabel->fontMetrics().width(strAdditional);
			additionalLabel->setFixedSize(width, this->height());
		}

		UpdataUi();
	});
}

int CombinedLabel::GetTextSpacing()
{
	int width = 0;
	if (additionalLabel->isVisible()) {
		if (middleLabel && middleLabel->isVisible()) {
			width = middleLabel->width() + additionalLabel->width() + PLSDpiHelper::calculate(this, spacing);
		} else {
			width = additionalLabel->width() + PLSDpiHelper::calculate(this, spacing);
		}
	}
	return this->width() - width;
}
