/*
 * @fine      PrismLiveStudio
 * @brief     login view include TermsOfUse and PrivacyPolicy module
 *            stackWidget include selectLoginPlatformView and loadingview
 * @date      2019-09-26
 * @author    Bing Cheng
 * @attention

 * @version   v1.0
 * @modify
 */

#ifndef LOGIN_LOGINBACKGROUNDVIEW_H
#define LOGIN_LOGINBACKGROUNDVIEW_H

#include <QFrame>
#include <QStackedWidget>
#include <QTimer>
#include <qwidget.h>
#include <qmovie.h>

class LoginLoadingView;
class PLSSelectLoginPlatformView;

namespace Ui {
class LoginBackgroundView;
}
class LoginBackgroundView : public QFrame {
	Q_OBJECT

public:
	explicit LoginBackgroundView(QStackedWidget *stackWidget, const QString &downloadFileUrl = QString(), QWidget *parent = nullptr);
	~LoginBackgroundView() override;
	/**
     * @brief initialize creat loading view and creat selectPlatform view
     *          and push back stackWidget
     */
	void initUi(const QString &downloadFileUrl = QString());

	void translateLanguage();

protected:
	void showEvent(QShowEvent *showEvent) override;
	void hideEvent(QHideEvent *hideEvent) override;

private:
	void setConnect() const;
	QString getLanguageType() const;

private slots:
	/**
     * @brief onLoginPrivacyPolicyBtn open url show privacyPolicy web page
     */
	void onLoginPrivacyPolicyBtn() const;
	/**
     * @brief onLoginTermsOfUseBtn open url show TermsOfUse web page
     */
	void onLoginTermsOfUseBtn() const;

	void startBeginLogo();
signals:
	void startCheckUpdate(const QString &localFilePath);

private:
	Ui::LoginBackgroundView *ui;
	LoginLoadingView *m_loginLodaingView = nullptr;                  // login attach view
	PLSSelectLoginPlatformView *m_selectLoginPlatformView = nullptr; // select platform login view
	QStackedWidget *m_loginStackWidget = nullptr;
	QTimer m_startTimer;
	QMovie m_logoMovie;
	bool m_isFirstShow = true;
};

#endif // LOGINBACKGROUNDVIEW_H
