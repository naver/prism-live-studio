#ifndef PLSLOGINDLG_H
#define PLSLOGINDLG_H

#include "PLSDialogView.h"
#include "ui_plslogindlg.h"

namespace Ui {
class PLSLoginDlg;
}

class PLSLoginDlg : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSLoginDlg(QWidget *parent = nullptr);
	~PLSLoginDlg() override;

	QString loginUrl() const;

private slots:
	void on_naverLoginButton_clicked();
	void on_storeLoginButton_clicked();

private:
	Ui::PLSLoginDlg *ui = nullptr;
	QString m_loginUrl;
};

#endif // PLSLOGINDLG_H
