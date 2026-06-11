#ifndef PLSNAVERSHOPPINGNOTICE_H
#define PLSNAVERSHOPPINGNOTICE_H

#include "PLSDialogView.h"
#include "libbrowser.h"

namespace Ui {
class PLSNaverShoppingNotice;
}

class PLSNaverShoppingNotice : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSNaverShoppingNotice(QWidget *parent = nullptr);
	~PLSNaverShoppingNotice() override;
	void setURL(const QString &url);

private slots:
	void on_closeButton_clicked();

private:
	Ui::PLSNaverShoppingNotice *ui;
	pls::browser::BrowserWidget *m_browserWidget;
};

#endif // PLSNAVERSHOPPINGNOTICE_H
