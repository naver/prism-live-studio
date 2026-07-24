/*
 * @fine      PrismLiveStudio
 * @brief     TermOfAgreeView
 * @date      2019-09-27
 * @author    Bing Cheng
 * @attention

 * @version   v1.0
 * @modify
 */

#ifndef LOGIN_TERMSOFAGREEVIEW_H
#define LOGIN_TERMSOFAGREEVIEW_H

#include <PLSDialogView.h>
#include "ui_PLSTermsOfAgreeView.h"
#include <QFrame>
#include <QDialog>
#include <QPointer>

class QTextEdit;
class PLSTextLoadingView;
namespace Ui {
class TermsOfAgreeView;
}
class PLSTermsOfAgreeView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSTermsOfAgreeView(QWidget *parent = nullptr);
	~PLSTermsOfAgreeView() override = default;

	static bool showTermsDialog(QWidget *parent = nullptr, const QString &prismLoginName = QString());
	void initUi(const QString &prismLoginName = QString(), const QString &payTermsHtml = QString());
	void setTermsContent(const QString &htmlContent);

signals:
	void termsToRetry();

protected:
	void showEvent(QShowEvent *event) override;

private:
	/**
     * @brief setTextEditDisplayProperty set display text format
     * @param textEdit
     * @param text display content
     */
	void setTextEditDisplayProperty(QTextEdit *textEdit, const QString &text) const;
	void setConnect() const;
	void initContent(const QString &lang, const QString &payTermsHtml, double dpi = 1.0);
	QByteArray getFileContent(const QString &lang) const;
	QString getInfoByDpiChange(const QString &srcStr, double dp = 1.0) const;
	void retryGetHtml();
	void showTermsLoading();
	void hideTermsLoading();
	void showRetryState();

private slots:
	/**
     * @brief agreeButtonStateChanged change button state
     * @param state
     */
	void agreeButtonStateChanged(bool isChecked);
	/**
     * @brief onAgreeButtonClicked  accpet agree
     */
	void onAgreeButtonClicked();

	/**
     * @brief onBackButtonClicked reject agree
     */
	void onBackButtonClicked();
	void openOtherLink() const;

private:
	Ui::TermsOfAgreeView *ui;
	QString termOfUseInfo;
	QString usePersonInfo;

	QString m_agreeInfo;
	QString m_privacyInfo;
	QString m_prismLoginName;
	bool isInitContentAgain = false;
	QString m_ruleLang;
	QString m_otherLinkUrl;
	QPointer<PLSTextLoadingView> m_termsLoading;
};

#endif // TERMSOFAGREEVIEW_H
