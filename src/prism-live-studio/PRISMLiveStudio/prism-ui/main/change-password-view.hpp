/*
 * @file      PLSChangePasswordView.h
 * @brief     The View of change password.
 * @date      2019/11/03
 * @author    liuying
 * @attention null
 * @version   2.0.1
 * @modify    liuying create 2019/11/03
 */
#ifndef LOGIN_PRISM_LOGINCHANGEPASSWORD_H
#define LOGIN_PRISM_LOGINCHANGEPASSWORD_H

#include "ui_PLSChangePasswordView.h"
#include "PLSDialogView.h"

namespace Ui {
class PLSChangePasswordView;
}
class PLSChangePasswordView : public PLSDialogView {

	Q_OBJECT
public:
	explicit PLSChangePasswordView(QWidget *parent = nullptr);

	~PLSChangePasswordView() override = default;

	/**
     * @brief translateLanguage
     */
	void translateLanguage();

	/**
     * @brief set signals and slot function
     */
	void setConnect() const;

public slots:
	void onOkButtonClicked();
	void onCancelButtonClicked();
	void editTextChange(const QString &text);
	void on_forgotPasswordBtn_clicked();
	/**
     * @brief replyErrorHandler response error info
     * @param url is requeset url is to match with who send http request
     * @param errorStr response error info
     */
	void replyErrorHandler(const QJsonObject &obj, const int statusCode);

private:
	Ui::PLSChangePasswordView *ui;
};

#endif // LOGIN_PRISM_LOGINCHANGEPASSWORD_H
