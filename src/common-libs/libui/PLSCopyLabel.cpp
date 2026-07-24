#include "PLSCopyLabel.h"
#include <QClipboard>
#include <QApplication>
#include "libui.h"

PLSCopyLabel::PLSCopyLabel(QWidget *parent) : QFrame(parent)
{
	setupUI();	
}

PLSCopyLabel::PLSCopyLabel(const QString &text, QWidget *parent) : QFrame(parent), m_text(text)
{
	setupUI();
}

void PLSCopyLabel::setText(const QString &text)
{
	m_text = text;
	m_copyButton->setDisabled(m_text.isEmpty());
	m_label->setText(text);
}

QString PLSCopyLabel::getText() const
{
	return m_text;
}

void PLSCopyLabel::setSpacing(int spacing)
{
	m_spacing = spacing;
	m_layout->setSpacing(m_spacing);
}

int PLSCopyLabel::getSpacing() const
{
	return m_spacing;
}

void PLSCopyLabel::setToolTip(const QString &tip)
{
	m_copyButton->setToolTip(tip);
}

void PLSCopyLabel::setupUI()
{
	pls_add_css(this, {"PLSCopyLabel"});

	m_layout = new QHBoxLayout(this);
	m_layout->setContentsMargins(0, 0, 0, 0);
	m_layout->setSpacing(m_spacing);

	m_label = new QLabel(this);
	m_label->setObjectName("textLabel");
	m_label->setText(m_text);
	m_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	m_copyButton = new QPushButton(this);
	m_copyButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_copyButton->setText("");
	m_copyButton->setToolTip(QObject::tr("Copy.UserId.ToolTip"));
	m_copyButton->setObjectName("copyButton");
	pls_uistep_v2_set_custom_enter_leave_name(m_copyButton, "Copy Button");
	connect(m_copyButton, &QPushButton::clicked, this, &PLSCopyLabel::onCopyButtonClicked);

	m_copyButton->setDisabled(m_text.isEmpty());
	pls_uistep_v2_set_custom_enter_leave_name(m_copyButton, "PLSCopyLabel");
	m_layout->addWidget(m_label);
	m_layout->addWidget(m_copyButton);
	setLayout(m_layout);
}

void PLSCopyLabel::onCopyButtonClicked()
{
	if (!m_text.isEmpty()) {
		QApplication::clipboard()->setText(m_text);
	}
	PLS_UI_ACTION("PLSCopyLabel:onCopyButtonClicked");
}