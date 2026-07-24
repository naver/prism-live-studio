
#ifndef LOGINPASSWORDLINEEDIT_H
#define LOGINPASSWORDLINEEDIT_H

#include <QLineEdit>
#include "PLSEdit.h"

#include "ui_PLSLoginPasswordLineEdit.h"

namespace Ui {
class PLSLoginPasswordLineEdit;
}
class LoginPasswordLineEdit : public PLSLineEdit {
	Q_OBJECT

public:
	explicit LoginPasswordLineEdit(QWidget *parent = nullptr);
	~LoginPasswordLineEdit() override;

private slots:
	/**
     * @brief on_stateButton_clicked show or hide password
     */
	void on_stateButton_clicked();

private:
	Ui::PLSLoginPasswordLineEdit *ui;
	bool m_isVisablePwd = true;
};

#endif // LOGINPASSWORDLINEEDIT_H
