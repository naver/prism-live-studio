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


#include <QStackedWidget>
#include <QWidget>

class PLSNetworkAccessManager;
namespace Ui {
class SignupWithEmailView;
}
class SignupWithEmailView : public QFrame {
	Q_OBJECT

public:
	explicit SignupWithEmailView(QStackedWidget *stackWidget, QWidget *parent = nullptr);
	~SignupWithEmailView();

	void translateLanguage();
	void initUi();
	void clearView();

private:
	void initBackButton();
	void setConnect();
	/**
     * @brief signupRequest email sign up  request init and handler
     * @param isAgree
     */
	void signupRequest(bool isAgree);
	void responseErrorHandler(const QByteArray &array);
	void showTermOfAgreeView();
	/**
     * @brief showExitIdMessage:show message When the response message is that the Id already exists
     */
	void exitEmailHandler();
	/**
     * @brief clearInputInfo:clear input info
     */
	void clearInputInfo();

	QWidget *findSnsLoginView(QStackedWidget *stackWidget);

private slots:
	/**
     * @brief on_loginBtn_clicked :back select other login view
     */
	void on_signLoginBtn_clicked();
	void on_signLoginBtn_2_clicked();
	/**
     * @brief updateCreateNewAccountBtnAvailable: control login button is or not available;
     *        when Email, nickname and password are entered,the button is available ,otherwise not;
     */
	void updateCreateNewAccountBtnAvailable(const QString &);
	/**
     * @brief on_createNewAccountBtn_clicked trigger email sign up request
     */
	void on_createNewAccountBtn_clicked();
	/**
     * @brief replyResultDataHandler response handler
     * @param statusCode response status code
     * @param url is requeset url is to match with who send http request
     * @param array response data include code and body
     */
	void replyResultDataHandler(int statusCode, const QString &url, const QByteArray array);
	/**
     * @brief replyErrorHandler response error info
     * @param url is requeset url is to match with who send http request
     * @param errorStr response error info
     */
	void replyErrorHandler(int statusCode, const QString &url, const QString &body, const QString &errorInfo);

signals:
	/**
     * @brief emailSignUpSuccess notify email sign up success
     */
	void emailSignUpSuccess();

private:
	Ui::SignupWithEmailView *ui;
	QStackedWidget *m_loginStackFrame;
	PLSNetworkAccessManager *m_networkAccessManager;
};

#endif // SIGNUPWITHEMAILVIEW_H
