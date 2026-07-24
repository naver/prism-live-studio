#include "qt-display-failed-view.hpp"

#include <QLabel>
#include <QStackedLayout>
#include <QVBoxLayout>

#include "PLSLoadingView.h"
#include "libui.h"

namespace {
constexpr int kLoadingSize = 24;
} // namespace

OBSQTDisplayFailedView::OBSQTDisplayFailedView(QWidget *parent) : QWidget(parent)
{
	setObjectName("failedTextView");
	setAttribute(Qt::WA_StyledBackground, true);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	pls_add_css(this, {"PLSQTDisplay"});

	m_loadingPage = pls_new<QWidget>(this);
	m_loadingPage->setObjectName("sourceLoadingPage");

	auto loadingLayout = pls_new<QVBoxLayout>(m_loadingPage);
	loadingLayout->setContentsMargins(0, 0, 0, 0);
	loadingLayout->addStretch();

	m_loadingView = pls_new<PLSLoadingView>(this);
	m_loadingView->setFixedSize(kLoadingSize, kLoadingSize);
	m_loadingView->setAttribute(Qt::WA_TransparentForMouseEvents);
	m_loadingView->hide();
	loadingLayout->addWidget(m_loadingView, 0, Qt::AlignCenter);
	loadingLayout->addStretch();

	m_textLabel = pls_new<QLabel>(this);
	m_textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_textLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	m_textLabel->setObjectName("failedText");
	m_textLabel->setWordWrap(true);
	m_textLabel->setIndent(-1);
	m_textLabel->hide();

	auto layout = pls_new<QStackedLayout>(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setStackingMode(QStackedLayout::StackOne);
	layout->addWidget(m_loadingPage);
	layout->addWidget(m_textLabel);

	m_contentLayout = layout;

	applyState();
}

void OBSQTDisplayFailedView::setContent(const QString &text, bool needLoading)
{
	m_text = text;
	m_state = needLoading ? ContentState::Loading : (text.isEmpty() ? ContentState::Hidden : ContentState::Text);
	applyState();
}

void OBSQTDisplayFailedView::refreshContentLayout()
{
	applyState();
}

void OBSQTDisplayFailedView::hideContent()
{
	m_text.clear();
	m_state = ContentState::Hidden;
	applyState();
}

void OBSQTDisplayFailedView::applyState()
{
	m_textLabel->setContentsMargins(0, 0, 0, 0);
	m_textLabel->setText(m_text);
	m_textLabel->updateGeometry();

	switch (m_state) {
	case ContentState::Hidden:
		m_loadingView->hide();
		m_textLabel->clear();
		m_textLabel->hide();
		hide();
		return;

	case ContentState::Loading:
		m_contentLayout->setCurrentWidget(m_loadingPage);
		m_textLabel->hide();
		m_loadingView->show();
		show();
		return;

	case ContentState::Text:
		m_contentLayout->setCurrentWidget(m_textLabel);
		m_loadingView->hide();
		m_textLabel->show();
		show();
		return;
	}
}
