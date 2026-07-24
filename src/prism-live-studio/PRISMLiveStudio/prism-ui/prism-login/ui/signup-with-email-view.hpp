/*
 * @fine      PrismLiveStudio
 * @brief     Email registration platform view; sign up request and response handler
 * @date      2019-09-27
 * @author    Bing Cheng
 * @attention

 * @version   v1.0
 * @modify
 */

#ifndef LOGIN_SIGNUPWITHEMAILVIEW_H
#define LOGIN_SIGNUPWITHEMAILVIEW_H

#include "ui_PLSSignupWithEmailView.h"

#include <QStackedWidget>
#include <QWidget>
#include "PLSErrorHandler.h"

namespace Ui {
class SignupWithEmailView;
}
class SignupWithEmailView : public QFrame {
	Q_OBJECT

public:
	explicit SignupWithEmailView(QStackedWidget *stackWidget, QWidget *parent = nullptr);
	~SignupWithEmailView() override;

	void translateLanguage();
	void initUi();
	void clearView();

private:
	void initBackButton();
	void setConnect() const;
	/**
     * @brief signupRequest email sign up  request init and handler
     * @param isAgree
     */
	void signupRequest(bool isAgree);
	void showTermOfAgreeView();
	/**
     * @brief clearInputInfo:clear input info
     */
	void clearInputInfo();

	QWidget *findSnsLoginView(const QStackedWidget *stackWidget);

private slots:

	/**
     * @brief on_loginBtn_clicked :back select other login view
     */
	void on_signLoginBtn_clicked();
	void on_signLoginBtn_2_clicked();

	void updateCreateNewAccountBtnAvailable(const QString &);
	void on_createNewAccountBtn_clicked();

signals:
	/**
     * @brief emailSignUpSuccess notify email sign up success
     */
	void emailSignUpSuccess();

private:
	Ui::SignupWithEmailView *ui;
	QStackedWidget *m_loginStackFrame;
};

#endif // SIGNUPWITHEMAILVIEW_H
