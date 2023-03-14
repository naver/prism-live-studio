#ifndef PLSNAVERSHOPPINGTERM_H
#define PLSNAVERSHOPPINGTERM_H

#include "dialog-view.hpp"
#include "PLSDpiHelper.h"
#include <browser-panel.hpp>

namespace Ui {
class PLSNaverShoppingTerm;
}

class PLSNaverShoppingTerm : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSNaverShoppingTerm(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSNaverShoppingTerm();
	void setURL(const QString &url);
	void setMoreUrl(const QString &url);
	void setMoreLabelTitle(const QString &title);
	void setMoreButtonHidden(bool hidden);
	void setCancelButtonHidden(bool hidden);
	void setOKButtonTitle(const QString &title);

private slots:
	void on_moreButton_clicked();
	void on_closeButton_clicked();
	void on_confirmButton_clicked();
	void onTimeOut();

private:
	QTimer *timer;
	Ui::PLSNaverShoppingTerm *ui;
	QCefWidget *m_cefWidget;
	QString m_updateInfoUrl;
	QString m_viewAllUrl;
};

#endif // PLSNAVERSHOPPINGTERM_H
