//
//  PLSLoadingButton.cpp
//  prism-live-studio
//
//  Created by Sam Zhang on 2025/10/31.
//

#include "PLSLoadingButton.h"
#include <QHBoxLayout>
#include <QLabel>
#include "PLSLoadingView.h"

PLSLoadingButton::PLSLoadingButton(const QString &text, QWidget *parent) : QPushButton(parent), m_loading(false), m_normalText(text), m_loadingText(text)
{
	m_container = new QWidget(this);
	m_container->setAttribute(Qt::WA_TransparentForMouseEvents);
	m_container->setStyleSheet("background: transparent;");

	QHBoxLayout *layout = new QHBoxLayout(m_container);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(10);
	layout->setAlignment(Qt::AlignCenter);

	m_textLabel = new QLabel(text, m_container);
	m_textLabel->setStyleSheet("font-weight: normal;");
	layout->addWidget(m_textLabel);

	m_loadingView = new PLSLoadingView();
	m_loadingView->setFixedSize(24, 24);
	m_loadingView->hide();
	layout->addWidget(m_loadingView);

	setText("");
}

PLSLoadingButton::~PLSLoadingButton() {}

void PLSLoadingButton::setLensSelected(bool selected)
{
	m_lensSelected = selected;
	updateText();
}

void PLSLoadingButton::setLoading(bool loading)
{
	m_loading = loading;
	updateText();
}

bool PLSLoadingButton::isLensSelected() const
{
	return m_lensSelected;
}

bool PLSLoadingButton::isLoading() const
{
	return m_loading;
}

void PLSLoadingButton::setNormalText(const QString &text)
{
	m_normalText = text;
	updateText();
}

void PLSLoadingButton::setLoadingText(const QString &text)
{
	m_loadingText = text;
	updateText();
}

void PLSLoadingButton::setSelectText(const QString &text)
{
	m_selectLensText = text;
	updateText();
}

void PLSLoadingButton::resizeEvent(QResizeEvent *event)
{
	QPushButton::resizeEvent(event);
	m_container->setGeometry(rect());
}

void PLSLoadingButton::updateText()
{
	if (!m_lensSelected) {
		m_loadingView->hide();
		m_textLabel->setText(m_selectLensText);
		setEnabled(true);
	} else {
		if (m_loading) {
			m_loadingView->show();
			m_textLabel->setText(m_loadingText);
			setEnabled(false);
		} else {
			m_loadingView->hide();
			m_textLabel->setText(m_normalText);
			setEnabled(true);
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------
PLSLoadingVbButton::PLSLoadingVbButton(const QString &text, QWidget *parent) : PLSLoadingButton(text, parent) {}

PLSLoadingVbButton::~PLSLoadingVbButton() {}

void PLSLoadingVbButton::setButtonText(const QString &text)
{
	if (m_textLabel->text() != text) {
		m_textLabel->setText(text);
	}
}

void PLSLoadingVbButton::updateText()
{
	if (m_loading) {
		m_loadingView->show();
		setEnabled(false);
	} else {
		m_loadingView->hide();
		setEnabled(true);
	}
}