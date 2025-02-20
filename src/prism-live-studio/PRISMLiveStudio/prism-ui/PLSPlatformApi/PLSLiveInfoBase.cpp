#include <qglobal.h>
#if defined(Q_OS_WIN)
#include <Windows.h>
#endif
#include "PLSLiveInfoBase.h"

#include <QPushButton>
#include <QHBoxLayout>

//#include "pls-app.hpp"
#include "PLSPlatformApi.h"
#include "PLSLiveInfoDialogs.h"
//#include "main-view.hpp"
#include "ResolutionGuidePage.h"
#include "libutils-api.h"
#include "PLSChannelDataAPI.h"
#include "ChannelCommonFunctions.h"
#include "pls/pls-dual-output.h"

PLSLiveInfoBase::PLSLiveInfoBase(PLSPlatformBase *pPlatformBase, QWidget *parent) : PLSDialogView(parent), m_pPlatformBase(pPlatformBase), m_pWidgetLoadingBG(nullptr)
{
	pls_add_css(this, {"PLSLoadingBtn", "PLSLiveInfoBase"});
	setHasCloseButton(false);
	setResizeEnabled(false);

	channelUUid = m_pPlatformBase->getChannelUUID();

	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::channelRemoved, this,
		[this](const QString &uuid) {
			if (uuid == channelUUid) {
				pls_notify_close_modal_views_with_parent(this);
				close();
			}
		},
		Qt::QueuedConnection);

#if defined(Q_OS_MACOS)
	setFixedSize({720, 670});
#else
	setFixedSize({720, 710});
#endif
}

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

	m_isRunLoading = true;
	m_pWidgetLoadingBGParent = parent;

	m_pWidgetLoadingBG = new QWidget(parent);
	m_pWidgetLoadingBG->setObjectName("loadingBG");
#if defined(Q_OS_MACOS)
	m_pWidgetLoadingBG->setAttribute(Qt::WA_DontCreateNativeAncestors);
	m_pWidgetLoadingBG->setAttribute(Qt::WA_NativeWindow);
#endif

	m_pWidgetLoadingBG->setGeometry(0, 0, parent->geometry().size().width(), parent->geometry().size().height());
	m_pWidgetLoadingBG->show();

	auto layout = pls_new<QHBoxLayout>(m_pWidgetLoadingBG);
	auto loadingBtn = pls_new<QPushButton>(m_pWidgetLoadingBG);
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
	m_isRunLoading = false;
	if (m_pWidgetLoadingBGParent && pls_object_is_valid(m_pWidgetLoadingBGParent)) {
		m_pWidgetLoadingBGParent->removeEventFilter(this);
		m_pWidgetLoadingBGParent = nullptr;
	}

	if (m_pWidgetLoadingBG && pls_object_is_valid(m_pWidgetLoadingBG)) {
		m_loadingEvent.stopLoadingTimer();

		pls_delete(m_pWidgetLoadingBG);
		m_pWidgetLoadingBG = nullptr;
	}
}

void PLSLiveInfoBase::showEvent(QShowEvent *event)
{
	PLSDialogView::showEvent(event);
	if (m_pPlatformBase) {
		ResolutionGuidePage::checkResolution(this, m_pPlatformBase->getChannelUUID());
	}
}

bool PLSLiveInfoBase::eventFilter(QObject *watcher, QEvent *event)
{
	if (m_pWidgetLoadingBG && (watcher == m_pWidgetLoadingBGParent) && (event->type() == QEvent::Resize)) {
		const QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
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
#if defined(Q_OS_WIN)
	if ((GetAsyncKeyState(VK_MENU) < 0) && (GetAsyncKeyState(VK_F4) < 0)) { // ALT+F4
		event->ignore();
		return;
	}
#endif
	PLSDialogView::closeEvent(event);
}

QWidget *PLSLiveInfoBase::createResolutionButtonsFrame(bool bNcp)
{
	return ResolutionGuidePage::createResolutionButtonsFrame(this, bNcp);
}

PLSLiveInfoDualWidget::PLSLiveInfoDualWidget(QWidget *parent) : QWidget(parent)
{
	QHBoxLayout *l = new QHBoxLayout(this);
	l->setAlignment(Qt::AlignLeft);
	l->setContentsMargins({});
	l->setSpacing(5);

	m_imgLabel = new QLabel(this);
	m_imgLabel->setObjectName("dualImageLabel");
	l->addWidget(m_imgLabel);

	m_titleLabel = new QLabel(this);
	m_titleLabel->setObjectName("dualTitleLabel");
	l->addWidget(m_titleLabel);

	connect(PLSChannelDataAPI::getInstance(), &PLSChannelDataAPI::sigSetChannelDualOutput, this, &PLSLiveInfoDualWidget::onPlatformDualChanged, Qt::QueuedConnection);
}
PLSLiveInfoDualWidget *PLSLiveInfoDualWidget::setText(const QString &text)
{
	m_titleLabel->setText(text);
	return this;
}

void PLSLiveInfoDualWidget::onPlatformDualChanged(const QString &uuid, ChannelData::ChannelDualOutput outputType)
{
	pls_check_app_exiting();
	if (uuid == m_channelUuid) {
		setUIIconStyle(outputType);
	}
}

PLSLiveInfoDualWidget *PLSLiveInfoDualWidget::setUUID(const QString &channelUuid)
{
	m_channelUuid = channelUuid;
	auto outputType = PLSCHANNELS_API->getValueOfChannel(channelUuid, ChannelData::g_channelDualOutput, channel_data::NoSet);
	return setUIIconStyle(outputType);
}

PLSLiveInfoDualWidget *PLSLiveInfoDualWidget::setUIIconStyle(ChannelData::ChannelDualOutput outputType)
{
	if (!pls_is_dual_output_on()) {
		outputType = channel_data::NoSet;
	}
	switch (outputType) {
	case channel_data::NoSet:
		pls_flush_style(this, "dualModel", "no");
		m_imgLabel->setVisible(false);
		break;
	case channel_data::HorizontalOutput:
		pls_flush_style(this, "dualModel", "h");
		m_imgLabel->setVisible(true);
		break;
	case channel_data::VerticalOutput:
		pls_flush_style(this, "dualModel", "v");
		m_imgLabel->setVisible(true);
		break;
	default:
		m_imgLabel->setVisible(false);
		break;
	}
	return this;
}
