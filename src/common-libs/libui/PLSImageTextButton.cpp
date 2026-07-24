#include "PLSImageTextButton.h"
#include "pls-common-define.hpp"
#include <QHBoxLayout>
#include "libui.h"
#include "libutils-api.h"
using namespace common;

PLSImageTextButton::PLSImageTextButton(QWidget *parent) : QPushButton(parent)
{
	auto horizontalLayout = pls_new<QHBoxLayout>(parent);
	horizontalLayout->setContentsMargins(1, 0, 1, 0);
	horizontalLayout->setSpacing(4);

	m_leftSpacer = pls_new<QSpacerItem>(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
	horizontalLayout->addItem(m_leftSpacer);

	m_labelLeft = pls_new<QLabel>();
	m_labelLeft->setObjectName(QString::fromUtf8("labelLeft"));
	m_imgLabel = m_labelLeft;

	horizontalLayout->addWidget(m_labelLeft);

	m_labelRight = pls_new<QLabel>();
	m_labelRight->setObjectName(QString::fromUtf8("labelRight"));
	m_textLabel = m_labelRight;
	horizontalLayout->addWidget(m_labelRight, 1, Qt::AlignLeft | Qt::AlignVCenter);

	this->setLayout(horizontalLayout);
	pls_flush_style_recursive(this, STATUS, STATUS_NORMAL);
}

void PLSImageTextButton::setLabelText(const QString &str, bool isElidedText, bool isIconLeft)
{
	m_oriText = str;
	if (isIconLeft) {
		m_imgLabel = m_labelLeft;
		m_textLabel = m_labelRight;
	} else {
		m_imgLabel = m_labelRight;
		m_textLabel = m_labelLeft;
	}
	m_textLabel->setText(str);
	m_isElidedText = isElidedText;
	elidedLabelText();
}
void PLSImageTextButton::modifyContentCenter()
{
	auto l = dynamic_cast<QHBoxLayout *>(layout());
	m_leftSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
	l->addItem(new QSpacerItem(0, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum));
	l->addWidget(m_textLabel, 0, Qt::AlignHCenter | Qt::AlignVCenter);
	l->addWidget(m_imgLabel, 0, Qt::AlignHCenter | Qt::AlignVCenter);
	l->addItem(new QSpacerItem(0, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum));
}
void PLSImageTextButton::setFileButtonEnabled(bool enabled)
{
	this->setEnabled(enabled);
	pls_flush_style_recursive(this, STATUS, enabled ? STATUS_NORMAL : STATUS_DISABLE);
}

void PLSImageTextButton::setWordWrap(bool wordWrap)
{
	m_textLabel->setWordWrap(wordWrap);
}

void PLSImageTextButton::seIsLeftAlign(bool isLeft)
{
	if (isLeft) {
		m_leftSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
	}
}
void PLSImageTextButton::onlyHideContent(bool hide)
{
	m_imgLabel->setHidden(hide);
	m_textLabel->setHidden(hide);

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
	QFontMetrics titleFont(m_textLabel->font());
	QString elidedText = titleFont.elidedText(m_oriText, Qt::ElideRight, this->geometry().width() - 22 /*left width*/);
	m_textLabel->setText(elidedText);
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
