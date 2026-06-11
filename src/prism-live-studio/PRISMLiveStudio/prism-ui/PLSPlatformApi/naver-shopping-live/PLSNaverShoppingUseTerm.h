#ifndef PLSNAVERSHOPPINGUSETERM_H
#define PLSNAVERSHOPPINGUSETERM_H

#include "PLSDialogView.h"
#include "libbrowser.h"

namespace Ui {
class PLSNaverShoppingUseTerm;
}

class PLSNaverShoppingUseTerm : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSNaverShoppingUseTerm(QWidget *parent = nullptr);
	~PLSNaverShoppingUseTerm() override;
	void setLoadingURL(const QString &useTermUrl, const QString &policyUrl);

private slots:
	void on_closeButton_clicked();
	void on_confirmButton_clicked();
	void allAgreeButtonStateChanged(int state);
	void checkBoxButtonStateChanged(int state);

private:
	void doUpdateOkButtonState();
	QString getPolicyJavaScript() const;
	Ui::PLSNaverShoppingUseTerm *ui;
	pls::browser::BrowserWidget *m_useTermCefWidget;
	pls::browser::BrowserWidget *m_policyCefWidget;
};

#endif // PLSNAVERSHOPPINGUSETERM_H
