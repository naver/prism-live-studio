/*
 * @fine      PrismLiveStudio
 * @brief     login with email view; email login request and response handler
 * @date      2019-09-27
 * @author    Bing Cheng
 * @attention

 * @version   v1.0
 * @modify
 */

#ifndef LOGIN_LOGINWITHEMAILVIEW_H
#define LOGIN_LOGINWITHEMAILVIEW_H

#include <QStackedWidget>
#include <QWidget>
#include "PLSErrorHandler.h"

namespace Ui {
class LoginWithEmailView;
}

class LoginWithEmailView : public QFrame {
	Q_OBJECT

public:
	explicit LoginWithEmailView(QStackedWidget *stackWidget, QWidget *parent = nullptr);
	~LoginWithEmailView() override;
	/**
     * @brief initialize init login button status
     */
	void initUi();
	void setEmailStr(const QString &emailStr);
	void translateLanguage();
	void changeToNCB2BUi();
	void setLoginButtonStatus(bool isOk);

protected:
	bool eventFilter(QObject *watch, QEvent *e) override;

private:
	void initBackButton();
	/**
     * @brief loginRequest email request init and handler
     */
	void loginRequest();

	void clickEmailLogin();
	void clickNCB2BLogin();

	void emailLoginBtnCheck();
	void ncB2BLoginBtnCheck();

private slots:

	void responseErrorHandler(const PLSErrorHandler::NetworkData &netData, const PLSErrorHandler::ExtraData &extraData);
	/**
     * @brief on_loginListBtn_clicked :go back login background view when language is other
     */
	void on_loginListBtn_clicked();
	/**
     * @brief on_loginListBtn_2_clicked :go back login background view when language is ko-KR
     */
	void on_loginListBtn_2_clicked();
	/**
     * @brief on_loginForgotPasswordBtn_clicked: show forgotPassword view
     */
	void on_loginForgotPasswordBtn_clicked() const;
	/**
     * @brief on_loginBtn_clicked :trigger email login request
     */
	void on_loginBtn_clicked();

	void updateLoginBtnAvailable(const QString &);

	void on_tipIconBtn_clicked();

signals:
	/**
     * @brief emailLoginSuccess notify email login success
     */
	void emailLoginSuccess();
	void getNCB2BAuthUrl(const QString &authUrl);

private:
	Ui::LoginWithEmailView *ui = nullptr;
	QStackedWidget *m_parentStackWidget = nullptr;
	bool m_isNCB2BLogin = false;
};

#endif // LOGINWITHEMAILVIEW_H
