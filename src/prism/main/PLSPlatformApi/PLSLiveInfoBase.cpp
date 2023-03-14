#include <Windows.h>

#include "PLSLiveInfoBase.h"

#include <QPushButton>
#include <QHBoxLayout>

#include "pls-app.hpp"
#include "PLSPlatformApi.h"
#include "PLSLiveInfoDialogs.h"
#include "main-view.hpp"
#include "ResolutionGuidePage.h"

PLSLiveInfoBase::PLSLiveInfoBase(PLSPlatformBase *pPlatformBase, QWidget *parent, PLSDpiHelper dpiHelper)
	: PLSDialogView(parent, dpiHelper), m_pPlatformBase(pPlatformBase), m_pWidgetLoadingBG(nullptr)
{

	dpiHelper.setCss(this, {PLSCssIndex::PLSLoadingBtn, PLSCssIndex::PLSLiveInfoBase});
	dpiHelper.setFixedSize(this, {720, 710});
	/*dpiHelper.notifyDpiChanged(this, [=]() {
		if (m_pWidgetLoadingBG != nullptr && m_pWidgetLoadingBG->isVisible()) {
			m_pWidgetLoadingBG->setGeometry(m_pWidgetLoadingBG->parentWidget()->geometry());
		}
	});*/
	setHasCloseButton(false);
	setResizeEnabled(false);
}

PLSLiveInfoBase::~PLSLiveInfoBase() {}

void PLSLiveInfoBase::updateStepTitle(QPushButton *button)
{
	button->setProperty("prepareLive", PLS_PLATFORM_API->isPrepareLive());
	button->style()->unpolish(button);
	button->style()->polish(button);

	if (PLS_PLATFORM_API->isPrepareLive()) {
		int iTotalSteps = PLS_PLATFORM_API->getTotalSteps();
		int iCurrStep = m_pPlatformBase->getCurrentStep();

		button->setText(iCurrStep == iTotalSteps ? QTStr("Live.Check.GoLive") : QTStr("Next"));

		if (iTotalSteps > 1) {
			auto title = windowTitle();
			title.append(QString("(%1/%2)").arg(iCurrStep).arg(iTotalSteps));
			setWindowTitle(title);
		}
	}
}

void PLSLiveInfoBase::showLoading(QWidget *parent)
{
	hideLoading();

	m_pWidgetLoadingBGParent = parent;

	m_pWidgetLoadingBG = new QWidget(parent);
	m_pWidgetLoadingBG->setObjectName("loadingBG");
	m_pWidgetLoadingBG->setGeometry(parent->geometry());
	m_pWidgetLoadingBG->show();

	auto layout = new QHBoxLayout(m_pWidgetLoadingBG);
	auto loadingBtn = new QPushButton(m_pWidgetLoadingBG);
	layout->addWidget(loadingBtn);
	loadingBtn->setObjectName("loadingBtn");
	loadingBtn->show();

	m_loadingEvent.startLoadingTimer(loadingBtn);

	if (m_pWidgetLoadingBGParent) {
		m_pWidgetLoadingBGParent->installEventFilter(this);
	}
}

void PLSLiveInfoBase::hideLoading()
{
	if (m_pWidgetLoadingBGParent) {
		m_pWidgetLoadingBGParent->removeEventFilter(this);
		m_pWidgetLoadingBGParent = nullptr;
	}

	if (nullptr != m_pWidgetLoadingBG) {
		m_loadingEvent.stopLoadingTimer();

		delete m_pWidgetLoadingBG;
		m_pWidgetLoadingBG = nullptr;
	}
}

void PLSLiveInfoBase::showEvent(QShowEvent *event)
{
	PLSDialogView::showEvent(event);
	ResolutionGuidePage::checkResolution(this, m_pPlatformBase->getChannelUUID());
}

bool PLSLiveInfoBase::eventFilter(QObject *watcher, QEvent *event)
{
	if (m_pWidgetLoadingBG && (watcher == m_pWidgetLoadingBGParent) && (event->type() == QEvent::Resize)) {
		QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
		m_pWidgetLoadingBG->setGeometry(0, 0, resizeEvent->size().width(), resizeEvent->size().height());
	}

	return PLSDialogView::eventFilter(watcher, event);
}

void PLSLiveInfoBase::showResolutionGuide()
{
	ResolutionGuidePage::showResolutionGuideCloseAfterChange(this);
}

void PLSLiveInfoBase::closeEvent(QCloseEvent *event)
{
	if ((GetAsyncKeyState(VK_MENU) < 0) && (GetAsyncKeyState(VK_F4) < 0)) { // ALT+F4
		event->ignore();
	} else {
		PLSDialogView::closeEvent(event);
	}
}

QWidget *PLSLiveInfoBase::createResolutionButtonsFrame()
{
	return ResolutionGuidePage::createResolutionButtonsFrame(this);
}
