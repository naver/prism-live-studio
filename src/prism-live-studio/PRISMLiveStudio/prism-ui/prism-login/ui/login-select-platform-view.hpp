/*
 * @fine      PrismLiveStudio
 * @brief     third party select login platform view
 * @date      2019-09-04
 * @author    Bing Cheng
 * @attention

 * @version   v1.0
 * @modify
 */
#ifndef LOGIN_SELECTLOGINPLATFORMVIEW_H
#define LOGIN_SELECTLOGINPLATFORMVIEW_H

#include "ui_PLSSelectLoginPlatformView.h"

#include <QWidget>
#include <QMetaEnum>
#include <QUrl>
#include <QUrlQuery>
#include <qjsonobject.h>
#include "login-user-info.hpp"

class QEvent;
class PLSRecentLoginBadge;
class QResizeEvent;
class QShowEvent;
class QTcpSocket;
class QTcpServer;
class QStackedWidget;
using callback = const std::function<void(const QString &, const QJsonObject &)>;

namespace Ui {
class PLSSelectLoginPlatformView;
}
class PLSSelectLoginPlatformView : public QWidget {
	Q_OBJECT
public:
	enum class PRISMLOGINTYPE { Facebook = 1, Google, Twitch, NAVER, Apple, LINE, NAVER_Cloud_B2B };
	Q_ENUM(PRISMLOGINTYPE)
	explicit PLSSelectLoginPlatformView(QWidget *parent = nullptr);
	~PLSSelectLoginPlatformView() override;
	/**
     * @brief setStackWidget get stackwidget object, is to back other login view
     * @param stackWidget
     */
	void setLoginStackWidget(QStackedWidget *stackWidget);
	static QPushButton *creatSnsLoginBtn(const QString &prismLoginName, const QJsonObject &loginAttri, bool isSignUp);

	void snsLoginHandler(int loginType, QPushButton *button);

	bool loginWithAccount(int loginType, const QString &loginName, qint32 recentKind, const QWidget *parent = nullptr);
	void loginWithAccountAsync(const std::function<void(bool ok, const QJsonObject &)> &callback, int loginType, const QString &loginName, qint32 recentKind,
				   QPushButton *button, const QWidget *parent = nullptr);
	static void loginResultHandler(const QStackedWidget *stackWidget, bool isSuccess);
	static QString getLoginIcon(const QString &logingName, const QString &iconPath);
	static QList<QPair<QString, QPair<QJsonObject, int>>> getLoginParamInfo();
	QPair<QJsonObject, int> getLoginParam(const QString &loginName);

private:
	void initUi();
	QString getmoduleName(int index) const;
	template<class Callback> bool pls_run_http_server(const char *path, QString &addr, Callback callback);
	template<class Callback> void readDataFromRemote(QTcpSocket *tcpClient, const QString &path, Callback callback, const QString &addr, QTcpServer *tcpServer) const;
	template<class Callback> void onWriteClient(const QString &addr, Callback &&callback, QTcpSocket *tcpClient, const QUrl &url, const QString &path) const;
	void selectB2BLogin(int type, const QString &loginName, qint32 recentKind);
	void getAuthUrlCallback(const QString &authUrl, const QString &loginName, int type, qint32 recentKind);
	bool isB2BAccessDenied(const QString &urlStr) const;
	bool runLocalServer(const QString &callbackUrl, const std::function<void(const QString &code)> &callback);
	bool isSpecialLogin(PRISMLOGINTYPE loginType);
	QWidget *anchorWidgetForRecentKind(qint32 kind) const;
	static QUrl buildFacebookLoginAuthUrl();
	void updateRecentBadge();
	void ensureRecentBadge();
protected:
	void showEvent(QShowEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private slots:
	/**
     * @brief on_defaultNaverLoginBtn_clicked :go to email login view
     */
	void on_defaultLoginWithEmailBtn_clicked();
	/**
     * @brief on_defaultNaverLoginBtn_clicked :go to email sign up view
     */
	void on_defaultSignupBtn_clicked();
	void on_loginContactUsBtn_clicked();
signals:
	void sendAuthCode(const QString code);

private:
	Ui::SelectLoginPlatformView *ui;
	PLSRecentLoginBadge *m_recentBadge = nullptr;
	QStackedWidget *m_loginStackFrame = nullptr;
	QString m_requesetUrl;
	QString m_httpServerAddress;
	QList<QPair<QString, QPair<QJsonObject, int>>> m_snsLoginInfo;
	QString m_ncpAuthUrl;
	QList<PRISMLOGINTYPE> m_specialLoginList = {PRISMLOGINTYPE::Apple, PRISMLOGINTYPE::LINE};
};

#endif // SELECTLOGINPLATFORMVIEW_H
