#ifndef PLSNCB2BBROWSERDOCKCONTENT_H
#define PLSNCB2BBROWSERDOCKCONTENT_H

#include <QWidget>
#include <QPushButton>
#include <QButtonGroup>
#include "libbrowser.h"
#include "PLSNCB2bBroSettingsItem.h"
#include "PLSErrorHandler.h"

namespace Ui {
class PLSNCB2bBrowserDockContent;
}

class PLSPushButton;
class PLSNCB2bBrowserDockContent : public QWidget {
	Q_OBJECT

public:
	explicit PLSNCB2bBrowserDockContent(QWidget *parent = nullptr);
	~PLSNCB2bBrowserDockContent() override;
	void refreshUI(PLSErrorHandler::ErrCode errCode);
	void refreshScrollArea();
	void closeBrowser();
	void setCheckedTitle(const QString &selectedTitle);
	QString getCheckedTitle();

private:
	void updateUI(const PLSNCB2bBrowserSettingData &data, int index);
	void check();
	int getFirstVisibleButton();
	void createCategoryButton(const PLSNCB2bBrowserSettingData &data, int index);
	void removeExtraButtons(int keepCount, int totalCount);
	bool isValidButtonIndex(int index) const;
	void switchToNoSourcePage();
	double getHorScrollBarValue();

private slots:
	void onBtnGroupClicked(int index);
	void onStackedWidgetCurrentChanged(int index);
	void onPreButtonClicked();
	void onNextButtonClicked();
	void checkPreNextButtonNeedShow();

private:
	Ui::PLSNCB2bBrowserDockContent *ui;
	pls::browser::BrowserWidget *cefWidget{nullptr};
	QButtonGroup *btnGroup{nullptr};
	QString m_checkedTitle;
	PLSErrorHandler::ErrCode m_errorCode = PLSErrorHandler::SUCCESS;
};

#endif // PLSNCB2BBROWSERDOCKCONTENT_H
