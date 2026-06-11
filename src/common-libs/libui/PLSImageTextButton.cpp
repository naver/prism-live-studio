#include "PLSImageTextButton.h"
#include "pls-common-define.hpp"
#include <QHBoxLayout>
#include "libui.h"
#include "libutils-api.h"
using namespace common;

PLSImageTextButton::PLSImageTextButton(QWidget *parent) : QPushButton(parent)
{
	auto horizontalLayout = pls_new<QHBoxLayout>(parent);
	horizontalLayout->setContentsMargins(0, 0, 0, 0);
	horizontalLayout->setSpacing(4);

	m_leftSpacer = pls_new<QSpacerItem>(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
	horizontalLayout->addItem(m_leftSpacer);

	m_labelLeft = pls_new<QLabel>();
	m_labelLeft->setObjectName(QString::fromUtf8("labelLeft"));

	horizontalLayout->addWidget(m_labelLeft);

	m_labelRight = pls_new<QLabel>();
	m_labelRight->setObjectName(QString::fromUtf8("labelRight"));
	horizontalLayout->addWidget(m_labelRight, 1, Qt::AlignLeft | Qt::AlignVCenter);

	this->setLayout(horizontalLayout);
	pls_flush_style_recursive(this, STATUS, STATUS_NORMAL);
}

void PLSImageTextButton::setLabelText(const QString &str, bool isElidedText)
{
	m_oriRightText = str;
	m_labelRight->setText(str);
	m_isElidedText = isElidedText;

	elidedLabelText();
}

void PLSImageTextButton::setFileButtonEnabled(bool enabled)
{
	this->setEnabled(enabled);
	pls_flush_style_recursive(this, STATUS, enabled ? STATUS_NORMAL : STATUS_DISABLE);
}

const QFont &PLSImageTextButton::getRightFont() const
{
	if (m_labelRight) {
		return m_labelRight->font();
	}
	return this->font();
}

void PLSImageTextButton::setWordWrap(bool wordWrap)
{
	if (m_labelRight) {
		m_labelRight->setWordWrap(wordWrap);
	}
}

void PLSImageTextButton::seIsLeftAlign(bool isLeft)
{
	if (isLeft) {
		m_leftSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
	}
}
void PLSImageTextButton::onlyHideContent(bool hide)
{
	m_labelLeft->setHidden(hide);
	m_labelRight->setHidden(hide);

	setDisabled(hide);
}
void PLSImageTextButton::enterEvent(QEnterEvent *event)
{
	if (!this->isEnabled()) {
		return;
	}
	pls_flush_style_recursive(this, STATUS, STATUS_HOVER);
	QPushButton::enterEvent(event);
}

void PLSImageTextButton::leaveEvent(QEvent *event)
{
	if (!this->isEnabled()) {
		return;
	}
	pls_flush_style_recursive(this, STATUS, STATUS_NORMAL);
	QPushButton::leaveEvent(event);
}

void PLSImageTextButton::mousePressEvent(QMouseEvent *event)
{
	if (!this->isEnabled()) {
		return;
	}
	pls_flush_style_recursive(this, STATUS, STATUS_CLICKED);
	QPushButton::mousePressEvent(event);
}

void PLSImageTextButton::mouseReleaseEvent(QMouseEvent *event)
{
	if (!this->isEnabled()) {
		return;
	}
	pls_flush_style_recursive(this, STATUS, STATUS_NORMAL);
	QPushButton::mouseReleaseEvent(event);
}
void PLSImageTextButton::resizeEvent(QResizeEvent *event)
{
	QPushButton::resizeEvent(event);
	elidedLabelText();
}
void PLSImageTextButton::elidedLabelText()
{
	if (!m_isElidedText) {
		return;
	}
	QFontMetrics titleFont(m_labelRight->font());
	QString elidedText = titleFont.elidedText(m_oriRightText, Qt::ElideRight, this->geometry().width() - 20 /*left width*/);
	m_labelRight->setText(elidedText);
}

PLSBorderButton::PLSBorderButton(QWidget *parent) : QPushButton(parent)
{
	auto horizontalLayout = pls_new<QHBoxLayout>(parent);
	horizontalLayout->setContentsMargins(0, 0, 0, 0);
	horizontalLayout->setSpacing(0);
	m_boderLabel = pls_new<QLabel>();
	m_boderLabel->setObjectName(QString::fromUtf8("boderLabel"));
	horizontalLayout->addWidget(m_boderLabel);
	this->setLayout(horizontalLayout);
}
