/*
 * @fine      PrismLiveStudio
 * @brief     Forgot your email login password and apply for a forgotten password from the server
 * @date      2019-09-27
 * @author    Bing Cheng
 * @attention

 * @version   v1.0
 * @modify
 */

#ifndef LOGIN_RESETPASSWORDEMAILVIEW_H
#define LOGIN_RESETPASSWORDEMAILVIEW_H

#include <QWidget>
#include <dialog-view.hpp>
#include "PLSDpiHelper.h"

class PLSNetworkAccessManager;
namespace Ui {
class PLSResetPasswordEmailView;
}
class PLSResetPasswordEmailView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSResetPasswordEmailView(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSResetPasswordEmailView();

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

private:
	Ui::PLSResetPasswordEmailView *ui;
	QPoint m_lastPositon;
	PLSNetworkAccessManager *m_networkAccessManager;
	bool m_isMove;
};

#endif // RESETPASSWORDEMAILVIEW_H
