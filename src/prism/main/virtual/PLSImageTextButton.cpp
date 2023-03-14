#include "PLSImageTextButton.h"
#include "pls-common-define.hpp"
#include <QHBoxLayout>
#include "frontend-api.h"

PLSImageTextButton::PLSImageTextButton(QWidget *parent) : QPushButton(parent)
{
	QHBoxLayout *horizontalLayout = new QHBoxLayout(parent);
	horizontalLayout->setContentsMargins(0, 0, 0, 0);
	horizontalLayout->setSpacing(4);

	QSpacerItem *verticalSpacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
	horizontalLayout->addItem(verticalSpacer);

	QLabel *labelLeft = new QLabel();
	labelLeft->setObjectName(QString::fromUtf8("labelLeft"));

	horizontalLayout->addWidget(labelLeft);

	m_labelRight = new QLabel();
	m_labelRight->setObjectName(QString::fromUtf8("labelRight"));
	horizontalLayout->addWidget(m_labelRight, 0, Qt::AlignLeft | Qt::AlignVCenter);

	this->setLayout(horizontalLayout);
	pls_flush_style_recursive(this, STATUS, STATUS_NORMAL);
}

void PLSImageTextButton::setLabelText(const QString &str)
{
	m_labelRight->setText(str);
}

void PLSImageTextButton::setFileButtonEnabled(bool enabled)
{
	this->setEnabled(enabled);
	pls_flush_style_recursive(this, STATUS, enabled ? STATUS_NORMAL : STATUS_DISABLE);
}

void PLSImageTextButton::enterEvent(QEvent *event)
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

PLSBorderButton::PLSBorderButton(QWidget *parent) : QPushButton(parent)
{
	QHBoxLayout *horizontalLayout = new QHBoxLayout(parent);
	horizontalLayout->setContentsMargins(0, 0, 0, 0);
	horizontalLayout->setSpacing(0);
	m_boderLabel = new QLabel();
	m_boderLabel->setObjectName(QString::fromUtf8("boderLabel"));
	horizontalLayout->addWidget(m_boderLabel);
	this->setLayout(horizontalLayout);
}
