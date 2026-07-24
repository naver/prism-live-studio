
#ifndef LOGIN_RESETPASSWORDEMAILVIEW_H
#define LOGIN_RESETPASSWORDEMAILVIEW_H

#include <QWidget>
#include "PLSDialogView.h"
#include "PLSErrorHandler.h"

namespace Ui {
class PLSResetPasswordEmailView;
}
class PLSResetPasswordEmailView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSResetPasswordEmailView(QWidget *parent = nullptr);
	~PLSResetPasswordEmailView() override = default;

private:
	/**
     * @brief loginRequest http queset init and handler
     */
	void loginRequest();

private slots:
	/**
     * @brief emailEditTextChange control ok button status
     * @param text
     */
	void emailEditTextChange(const QString &text);
	/**
     * @brief on_okBtn_clicked trigger forgot password request
     */
	void on_okBtn_clicked();
	/**
     * @brief on_cancelBtn_clicked cancel all handler and close the view
     */
	void on_cancelBtn_clicked();

private:
	Ui::PLSResetPasswordEmailView *ui;
	QPoint m_lastPositon;
	bool m_isMove = false;
};

#endif // RESETPASSWORDEMAILVIEW_H
