/*
* @file		PLSLiveInfoBase.h
* @brief	A base class to support moving the window in any client area
* @author	wu.longyue@navercorp.com
* @date		2020-01-06
*/

#pragma once

#include "PLSPlatformBase.hpp"
#include "PLSDialogView.h"
#include "loading-event.hpp"

class PLSPlatformBase;

class PLSLiveInfoBase : public PLSDialogView {
	Q_OBJECT
public:
	PLSLiveInfoBase(PLSPlatformBase *pPlatformBase, QWidget *parent = nullptr);
	~PLSLiveInfoBase() override = default;

	void setPlatformBase(PLSPlatformBase *pPlatformBase) { m_pPlatformBase = pPlatformBase; }
	virtual void showResolutionGuide();
	bool getIsRunLoading() const { return m_isRunLoading; }

signals:
	void loadingFinished();

protected:
	void updateStepTitle(QPushButton *button);
	void showLoading(QWidget *parent);
	void hideLoading();
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *watcher, QEvent *event) override;

	void closeEvent(QCloseEvent *event) override;
	QWidget *createResolutionButtonsFrame();

private:
	PLSPlatformBase *m_pPlatformBase;
	PLSLoadingEvent m_loadingEvent;
	QPointer<QObject> m_pWidgetLoadingBGParent = nullptr;
	QPointer<QWidget> m_pWidgetLoadingBG = nullptr;

	bool m_isRunLoading{false};
};
