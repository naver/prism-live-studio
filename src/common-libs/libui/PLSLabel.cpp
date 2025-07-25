#include "PLSLabel.h"
#include <libutils-api.h>

#include <qtimer.h>
#include <qpainter.h>
#include <qstyle.h>
#include <qmovie.h>
#include <qstyleoption.h>
#include <QToolTip>
#include "libui.h"
#include "libutils-api.h"
#include <QResizeEvent>

PLSElideLabel::PLSElideLabel(QWidget *parent) : QLabel(parent) {}

PLSElideLabel::PLSElideLabel(const QString &text, QWidget *parent) : QLabel(text, parent) {}

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

PLSLabel::PLSLabel(QWidget *parent, bool cutText) : QLabel(parent), cutTextIfNeed(cutText)
{
	if (!cutTextIfNeed)
		setWordWrap(true);
}

PLSLabel::PLSLabel(const QString &text, QWidget *parent, bool cutText) : QLabel(parent), cutTextIfNeed(cutText)
{
	if (!cutTextIfNeed)
		setWordWrap(true);

	SetText(text);
}

void PLSLabel::SetText(const QString &text)
{
	this->realText = text;
	this->setText(GetNameElideString());

	if (realText != this->text()) {
		setToolTip(realText);
	} else {
		setToolTip("");
	}
}

QString PLSLabel::Text() const
{
	return realText;
}

void PLSLabel::resizeEvent(QResizeEvent *event)
{
	QLabel::resizeEvent(event);
	QTimer::singleShot(0, this, [this]() { this->setText(GetNameElideString()); });
}

QString PLSLabel::GetNameElideString() const
{
	if (!cutTextIfNeed)
		return realText;

	QFontMetrics fontWidth(this->font());
	if (fontWidth.horizontalAdvance(realText) > this->width()) {
		return fontWidth.elidedText(realText, Qt::ElideRight, this->width());
	}

	return realText;
}

PLSCombinedLabel::PLSCombinedLabel(QWidget *parent) : QLabel(parent)
{
	additionalLabel = pls_new<QLabel>(this);
	additionalLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	additionalLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	additionalLabel->setObjectName("additionalLabel");
	additionalLabel->hide();

	middleLabel = pls_new<QLabel>(this);
	middleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	middleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	middleLabel->setObjectName("middleLabel");
	middleLabel->hide();
}

void PLSCombinedLabel::SetShowText(const QString &strText_)
{
	strText = strText_;
	UpdataUi();
}

void PLSCombinedLabel::SetAdditionalText(const QString &strText_)
{
	strAdditional = strText_;
	additionalLabel->setText(strText_);
	pls_async_call(this, [this, strText_]() {
		int width = additionalLabel->fontMetrics().horizontalAdvance(strText_);
		additionalLabel->setFixedSize(width, this->height());
		UpdataUi();
	});
}

void PLSCombinedLabel::UpdataUi()
{
	int textWidth = GetTextSpacing();
	QString displayText = strText;
	if (this->fontMetrics().horizontalAdvance(strText) > textWidth) {
		displayText = fontMetrics().elidedText(strText, Qt::ElideRight, textWidth);
	}
	setText(displayText);

	if (middleLabel->isVisible()) {
		middleLabel->move(fontMetrics().horizontalAdvance(displayText) + spacing, (height() - middleLabel->height()) / 2);
		if (additionalLabel->isVisible()) {
			additionalLabel->move(middleLabel->width() + fontMetrics().horizontalAdvance(displayText) + spacing, (height() - additionalLabel->height()) / 2);
		}
	} else {
		if (additionalLabel->isVisible()) {
			additionalLabel->move(fontMetrics().horizontalAdvance(displayText) + spacing, (height() - additionalLabel->height()) / 2);
		}
	}
}

void PLSCombinedLabel::SetSpacing(int spacing_)
{
	spacing = spacing_;
	UpdataUi();
}

void PLSCombinedLabel::SetAdditionalVisible(bool visible)
{
	additionalLabel->setVisible(visible);
	UpdataUi();
}

void PLSCombinedLabel::SetMiddleVisible(bool visible)
{
	middleLabel->setVisible(visible);
	UpdataUi();
}

void PLSCombinedLabel::resizeEvent(QResizeEvent *event)
{
	UpdataUi();
	QLabel::resizeEvent(event);
	pls_async_call(this, [this]() {
		if (!strAdditional.isEmpty()) {
			int width = additionalLabel->fontMetrics().horizontalAdvance(strAdditional);
			additionalLabel->setFixedSize(width, this->height());
		}

		UpdataUi();
	});
}

int PLSCombinedLabel::GetTextSpacing()
{
	int width = 0;
	if (additionalLabel->isVisible()) {
		if (middleLabel && middleLabel->isVisible()) {
			width = middleLabel->width() + additionalLabel->width() + spacing;
		} else {
			width = additionalLabel->width() + spacing;
		}
	}
	return this->width() - width;
}

void PLSApngLabel::paintEvent(QPaintEvent *)
{
	QStyle *style = QWidget::style();
	QPainter painter(this);
	drawFrame(&painter);
	QRect cr = contentsRect();
	auto margin = QLabel::margin();
	cr.adjust(margin, margin, -margin, -margin);
	int align = QStyle::visualAlignment(layoutDirection(), alignment());

	auto movie = QLabel::movie();
	if (!movie)
		return;

	QImage image = movie->currentImage();
	if (image.isNull())
		return;

	QPixmap pix;
	if (hasScaledContents()) {
		QSize scaledSize = cr.size() * devicePixelRatio();
		QImage scaledImage = image.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		pix = QPixmap::fromImage(std::move(scaledImage));
		pix.setDevicePixelRatio(devicePixelRatio());
	} else {
		pix = QPixmap::fromImage(std::move(image));
		pix.setDevicePixelRatio(devicePixelRatio());
	}

	QStyleOption opt;
	opt.initFrom(this);
	if (!isEnabled())
		pix = style->generatedIconPixmap(QIcon::Disabled, pix, &opt);
	style->drawItemPixmap(&painter, cr, align, pix);
}

PLSHelpIcon::PLSHelpIcon(QWidget *parent, bool handleTooltip_) : QLabel(parent), handleTooltip(handleTooltip_)
{
	setFrameShape(QFrame::NoFrame);
	setProperty("showHandCursor", QVariant(true));
#if defined(Q_OS_MACOS)
	installEventFilter(this);
#endif
}

void PLSHelpIcon::setHandleTooltip(bool handleTooltip)
{
	this->handleTooltip = handleTooltip;
}

bool PLSHelpIcon::eventFilter(QObject *watched, QEvent *event)
{
#if defined(Q_OS_MACOS)
	if (!handleTooltip) {
		return QLabel::eventFilter(watched, event);
	}
	if (watched == this && event->type() == QEvent::ToolTip) {
		QPoint pos = this->rect().center();
		QPoint global = this->mapToGlobal(pos);
		QToolTip::showText(QPoint(global.rx(), global.ry() - 20), this->toolTip(), this);
		return true;
	}
#endif
	return QLabel::eventFilter(watched, event);
}

PLSFormLabel::PLSFormLabel(QWidget *parent) : QLabel(parent) {}

void PLSFormLabel::setText(const QString &value)
{
	QLabel::setText(value);
	setToolTip(text());
}

void PLSFormLabel::resizeEvent(QResizeEvent *event)
{
	QLabel::resizeEvent(event);

	auto actualSize = event->size();
	if (heightForWidth(actualSize.width()) > actualSize.height()) {
		setAlignment(Qt::AlignLeft | Qt::AlignTop);
	} else {
		setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	}
}
