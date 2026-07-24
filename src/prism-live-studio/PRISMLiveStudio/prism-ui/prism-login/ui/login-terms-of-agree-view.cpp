#include "login-terms-of-agree-view.hpp"
#include <QTextBlock>
#include <QTextEdit>
#include "login-common-helper.hpp"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSCommonConst.h"
#include "PLSCommonFunc.h"
#include <qdir.h>
#include "libutils-api.h"
#include <qscrollbar.h>
#include <QDesktopServices>
#include "PLSBasic.h"
#include "PLSErrorHandler.h"
#include "PLSLoginDataHandler.h"
#include "PLSTextLoadingView.h"

using namespace common;

PLSTermsOfAgreeView::PLSTermsOfAgreeView(QWidget *parent) : PLSDialogView(parent)
{
	PLS_DISABLE_UISTEP_V2(this);
	pls_uistep_v2_set_title(this, pls_uistep_v2_get_english("login.agreement.title"));
	ui = pls_new<Ui::TermsOfAgreeView>();
	pls_set_css(this, {"prismTermOfAgreeView"});
	setFixedSize(730, 544);
	initSize(730, 544);
	activateWindow();
	addMacTopMargin();
}

bool PLSTermsOfAgreeView::showTermsDialog(QWidget *parent, const QString &prismLoginName)
{
	PLSTermsOfAgreeView view(parent);
	view.initUi(prismLoginName, PLSLoginDataHandler::instance()->getServiceTermsHtml());
	connect(&view, &PLSTermsOfAgreeView::termsToRetry, &view, [&view]() {
		view.showTermsLoading();
		PLSLoginDataHandler::instance()->getServiceTermsHTML(true);
		const QString html = PLSLoginDataHandler::instance()->getServiceTermsHtml();
		if (html.isEmpty()) {
			view.showRetryState();
			return;
		}
		view.setTermsContent(html);
	});
	return view.exec() == QDialog::Accepted;
}

void PLSTermsOfAgreeView::showEvent(QShowEvent *event)
{
	PLSDialogView::showEvent(event);

	ui->textEdit_agree_en->verticalScrollBar()->setSliderPosition(0);
	ui->textEdit_privacy_en->verticalScrollBar()->setSliderPosition(0);
#if defined(Q_OS_MACOS)
	setWindowTitle(tr("login.agreement.title"));
#endif
}

void PLSTermsOfAgreeView::setTextEditDisplayProperty(QTextEdit *textEdit, const QString &text) const
{
	QTextCursor textStyleCursor = textEdit->textCursor();
	textStyleCursor.beginEditBlock();
	QTextBlockFormat textStyleFormat = textStyleCursor.blockFormat();
	textStyleFormat.setRightMargin(RIGHT_MARGIN);
	textStyleFormat.setLeftMargin(LEFT_MARGIN);
	textStyleCursor.setBlockFormat(textStyleFormat);
	textStyleCursor.insertHtml(text);
	textStyleCursor.movePosition(QTextCursor::Start);
	textStyleCursor.endEditBlock();
	textEdit->setTextCursor(textStyleCursor);
}

void PLSTermsOfAgreeView::initUi(const QString &prismLoginName, const QString &payTermsHtml)
{
	m_prismLoginName = prismLoginName;

	setupUi(ui);
	setWindowTitle(QString());
	setResizeEnabled(false);

	ui->agreeButton->setProperty(STATUS, STATUS_DISABLE);
	ui->agreeButton->setEnabled(false);
	ui->verticalLayout_3->setAlignment(ui->retryButton, Qt::AlignHCenter);
	setConnect();
	QString lang = pls_get_locale();
	if (lang.compare("ko-KR", Qt::CaseInsensitive) != 0) {
		lang = "en-US";
	}
	m_otherLinkUrl = QString("https://prismlive.com/%1/policy/privacy_content.html?app=pcapp").arg(PLSBasic::instance()->getSupportLanguage());
	m_ruleLang = lang;
	ui->termOfUserCheckBox->setEnabled(false);
	initContent(lang, payTermsHtml);
#if defined(Q_OS_WIN)
	auto margins = ui->verticalLayout_2->contentsMargins();
	margins.setTop(40);
	ui->verticalLayout_2->setContentsMargins(margins);
	ui->verticalLayout->removeWidget(ui->topWidget);
	setTitleWidget(ui->topWidget);
	adjustSize();
#endif
}

void PLSTermsOfAgreeView::setConnect() const
{
	connect(ui->termOfUserCheckBox, &PLSCheckBox::clicked, this, &PLSTermsOfAgreeView::agreeButtonStateChanged);
	connect(ui->checkBox_14, &PLSCheckBox::clicked, this, &PLSTermsOfAgreeView::agreeButtonStateChanged);
	connect(ui->agreeButton, &QPushButton::clicked, this, &PLSTermsOfAgreeView::onAgreeButtonClicked);
	connect(ui->backButton, &QPushButton::clicked, this, &PLSTermsOfAgreeView::onBackButtonClicked);
	connect(ui->toolButton, &QPushButton::clicked, this, &PLSTermsOfAgreeView::openOtherLink);
	connect(ui->retryButton, &QPushButton::clicked, this, &PLSTermsOfAgreeView::retryGetHtml);
}
void PLSTermsOfAgreeView::initContent(const QString &_lang, const QString &payTermsHtml, double dpi)
{
	pls_unused(dpi);
	QString lang(_lang.split('-')[0]);
#if defined(Q_OS_WIN)
	QString privacyPath = QApplication::applicationDirPath() + QString("/../../data/prism-studio/webpage/%1_%2.html").arg("privacy").arg(lang);
#elif defined(Q_OS_MACOS)
	QString privacyPath = pls_get_app_resource_dir() + QString("/data/prism-studio/webpage/%1_%2.html").arg("privacy").arg(lang);
#endif
	if (!QFileInfo(privacyPath).exists()) {
		PLS_INFO(LAUNCHER_LOGIN, "privacyPath html path find error");
	}

	m_privacyInfo = getFileContent(privacyPath);
	ui->textEdit_privacy_en->setHtml(getInfoByDpiChange(m_privacyInfo));
	setTermsContent(payTermsHtml);
}
QByteArray PLSTermsOfAgreeView::getFileContent(const QString &filePath) const
{
	QByteArray fileContent;
	QFile file(filePath);
	if (file.open(QIODevice::ReadOnly | QFile::Text)) {
		fileContent = file.readAll();
	} else {
		fileContent = file.errorString().toUtf8();
	}
	file.close();
	return fileContent;
}
QString PLSTermsOfAgreeView::getInfoByDpiChange(const QString &srcStr, double dpi) const
{
	pls_unused(dpi);
	QString dstStr = srcStr;
	dstStr.replace("1pt", QString("%1px").arg(15));
	return dstStr;
}
void PLSTermsOfAgreeView::agreeButtonStateChanged(bool icChecked)
{
	pls_unused(icChecked);
	if ((ui->termOfUserCheckBox->isChecked()) && (ui->checkBox_14->isChecked())) {
		ui->agreeButton->setProperty(STATUS, STATUS_ENABLE);
		ui->agreeButton->setEnabled(true);
	} else {
		ui->agreeButton->setProperty(STATUS, STATUS_DISABLE);
		ui->agreeButton->setEnabled(false);
	}
	pls_flush_style(ui->agreeButton);
}

void PLSTermsOfAgreeView::onAgreeButtonClicked()
{
	if (!pls_get_network_state()) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LOGIN_CHECK_NOTE_NETWORK, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData("PLSTermsOfAgreeView::onAgreeButtonClicked"));
		return;
	}
	PLS_UI_STEP(LAUNCHER_LOGIN, " argree terms Button", ACTION_CLICK);
	if (pls_launcher_const::PLS_WHALESPACE_NAME == m_prismLoginName && !isInitContentAgain) {
		isInitContentAgain = true;
		ui->termOfUserCheckBox->setText(tr("login.agree.user.terms.whalespace"));
		ui->toolButton->setVisible(false);
		ui->termOfUserCheckBox->setChecked(false);
		ui->textEdit_privacy_en->setVisible(false);
		ui->verticalLayout->removeItem(ui->verticalSpacer_4);
		ui->verticalLayout->removeItem(ui->verticalSpacer_5);
		ui->checkBox_14->setChecked(true);
		ui->checkBox_14->setVisible(false);

		ui->verticalLayout->removeItem(ui->verticalSpacer_7);

		QDir appDir(QApplication::applicationDirPath());
#if defined(Q_OS_WIN)
		QString agreePath = QApplication::applicationDirPath() + QString("/../../data/prism-studio/webpage/whalespace.html");
#elif defined(Q_OS_MACOS)
		QString agreePath = pls_get_app_resource_dir() + QString("/data/prism-studio/webpage/whalespace.html");

#endif
		m_agreeInfo = getFileContent(agreePath);
		ui->textEdit_agree_en->setHtml(getInfoByDpiChange(m_agreeInfo));
		agreeButtonStateChanged(ui->termOfUserCheckBox->isChecked());
	}
	if (ui->termOfUserCheckBox->isChecked() && ui->checkBox_14->isChecked()) {
		accept();
	}
}

void PLSTermsOfAgreeView::onBackButtonClicked()
{
	PLS_UI_STEP(LAUNCHER_LOGIN, " disagree terms button", ACTION_CLICK);
	if (isInitContentAgain && pls_launcher_const::PLS_WHALESPACE_NAME == m_prismLoginName) {
		PLS_INFO(LAUNCHER_LOGIN, "back to prism rules");
		initContent(m_ruleLang, m_agreeInfo);
		ui->termOfUserCheckBox->setText(tr("login.agree.user.terms"));
		isInitContentAgain = false;
		ui->termOfUserCheckBox->setChecked(true);
		ui->checkBox_14->setChecked(true);
		ui->toolButton->setVisible(true);
		ui->textEdit_privacy_en->setVisible(true);
		ui->checkBox_14->setVisible(true);
		ui->verticalLayout->insertItem(7, ui->verticalSpacer_4);
		ui->verticalLayout->insertItem(9, ui->verticalSpacer_5);
		ui->verticalLayout->insertItem(11, ui->verticalSpacer_7);
		agreeButtonStateChanged(ui->termOfUserCheckBox->isChecked() && ui->checkBox_14->isChecked());
	} else {
		reject();
	}
}

void PLSTermsOfAgreeView::openOtherLink() const
{
	pls_async_invoke([this]() { QDesktopServices::openUrl(QUrl(m_otherLinkUrl, QUrl::TolerantMode)); });
	PLS_UI_ACTION("PLSTermsOfAgreeView see all view");
}

void PLSTermsOfAgreeView::retryGetHtml()
{
	ui->termOfUserCheckBox->setEnabled(false);
	ui->agreeButton->setEnabled(false);
	ui->retryButton->setVisible(false);
	ui->retryLabel->setVisible(false);
	emit termsToRetry();
}

void PLSTermsOfAgreeView::showTermsLoading()
{
	hideTermsLoading();
	m_termsLoading = pls_new<PLSTextLoadingView>(tr("ResolutionGuide.LoadingMessage"), ui->textEdit_agree_en);
	m_termsLoading->setGeometry(ui->textEdit_agree_en->rect());
	m_termsLoading->show();
	m_termsLoading->raise();
}

void PLSTermsOfAgreeView::hideTermsLoading()
{
	if (m_termsLoading) {
		pls_delete(m_termsLoading);
		m_termsLoading = nullptr;
	}
}

void PLSTermsOfAgreeView::showRetryState()
{
	hideTermsLoading();
	ui->termOfUserCheckBox->setEnabled(false);
	ui->agreeButton->setEnabled(false);
	ui->retryButton->setVisible(true);
	ui->retryLabel->setVisible(true);
}

void PLSTermsOfAgreeView::setTermsContent(const QString &htmlContent)
{
	hideTermsLoading();
	if (htmlContent.isEmpty()) {
		return;
	}
	m_agreeInfo = htmlContent;
	ui->textEdit_agree_en->setHtml(getInfoByDpiChange(m_agreeInfo));
	ui->termOfUserCheckBox->setEnabled(true);
	ui->retryButton->setVisible(false);
	ui->retryLabel->setVisible(false);
	ui->textEdit_agree_en->document()->setDefaultStyleSheet("a { outline: none}");
	QTextCursor cursor = ui->textEdit_agree_en->textCursor();
	cursor.clearSelection();
	cursor.movePosition(QTextCursor::Start);
	ui->textEdit_agree_en->setTextCursor(cursor);
}