#include "PLSTextLoadingView.h"

#include "PLSLoadingView.h"

#include <QLabel>
#include <QShowEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

PLSTextLoadingView::PLSTextLoadingView(const QString &text, QWidget *parent, const QString &pathImage) : QWidget(parent)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	m_panel = new QWidget(this);
	m_panel->setStyleSheet(QStringLiteral("background-color: #272727; border: none;"));

	m_loadingView = new PLSLoadingView(m_panel, pathImage);
	m_loadingView->setFixedSize(SPINNER_SIZE, SPINNER_SIZE);

	m_textLabel = new QLabel(text, m_panel);
	m_textLabel->setAlignment(Qt::AlignHCenter);
	m_textLabel->setWordWrap(true);
	m_textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_textLabel->setStyleSheet(QStringLiteral("color: #ffffff; background: transparent;"));
	m_textLabel->setAttribute(Qt::WA_TranslucentBackground);

	auto *inner = new QVBoxLayout(m_panel);
	inner->setContentsMargins(0, 0, 0, 0);
	inner->setSpacing(SPACING);
	inner->addStretch();
	inner->addWidget(m_loadingView, 0, Qt::AlignHCenter);
	inner->addWidget(m_textLabel, 0);
	inner->addStretch();

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);
	root->addWidget(m_panel);
}

void PLSTextLoadingView::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	raise();
}

void PLSTextLoadingView::setLoadingText(const QString &text)
{
	if (m_textLabel) {
		m_textLabel->setText(text);
	}
}

QString PLSTextLoadingView::loadingText() const
{
	return m_textLabel ? m_textLabel->text() : QString();
}
