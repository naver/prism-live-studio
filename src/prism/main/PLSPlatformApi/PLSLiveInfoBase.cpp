#include "PLSLiveInfoBase.h"

#include <QPushButton>
#include <QHBoxLayout>

#include "pls-app.hpp"
#include "PLSPlatformApi.h"
#include "PLSLiveInfoDialogs.h"

PLSLiveInfoBase::PLSLiveInfoBase(PLSPlatformBase *pPlatformBase, QWidget *parent) : PLSDialogView(parent), m_pPlatformBase(pPlatformBase), m_pWidgetLoadingBG(nullptr)
{
	setHasCloseButton(false);
	setResizeEnabled(false);
}

void PLSLiveInfoBase::updateStepTitle(QPushButton *button)
{
	button->setProperty("prepareLive", PLS_PLATFORM_API->isPrepareLive());
	button->style()->unpolish(button);
	button->style()->polish(button);

	if (PLS_PLATFORM_API->isPrepareLive()) {
		int iTotalSteps = PLS_PLATFORM_API->getTotalSteps();
		int iCurrStep = m_pPlatformBase->getCurrentStep();

		button->setText(iCurrStep == iTotalSteps ? QTStr("Live.Check.GoLive") : QTStr("Live.Check.Next"));

		if (iTotalSteps > 1) {
			auto title = windowTitle();
			title.append(QString("(%1/%2)").arg(iCurrStep).arg(iTotalSteps));
			setWindowTitle(title);
		}
	}
}

void PLSLiveInfoBase::showLoading(QWidget *parent)
{
	m_pWidgetLoadingBG = new QWidget(parent->parentWidget());
	m_pWidgetLoadingBG->setObjectName("loadingBG");
	m_pWidgetLoadingBG->setGeometry(parent->geometry());
	m_pWidgetLoadingBG->show();

	auto layout = new QHBoxLayout(m_pWidgetLoadingBG);
	auto loadingBtn = new QPushButton(m_pWidgetLoadingBG);
	layout->addWidget(loadingBtn);
	loadingBtn->setObjectName("loadingBtn");
	loadingBtn->show();

	m_loadingEvent.startLoadingTimer(loadingBtn);
}

void PLSLiveInfoBase::hideLoading()
{
	if (nullptr != m_pWidgetLoadingBG) {
		m_loadingEvent.stopLoadingTimer();

		delete m_pWidgetLoadingBG;
		m_pWidgetLoadingBG = nullptr;
	}
}
