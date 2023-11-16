#include "PLSNaverShoppingUseTerm.h"
#include "ui_PLSNaverShoppingUseTerm.h"
#include "window-basic-main.hpp"
#include <QDir>
#include "libui.h"
#include "liblog.h"
#include "pls-common-define.hpp"

constexpr auto logMoudule = "PLSNaverShoppingUseTerm";

PLSNaverShoppingUseTerm::PLSNaverShoppingUseTerm(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::PLSNaverShoppingUseTerm>();
	pls_add_css(this, {"PLSNaverShoppingUseTerm"});
	setupUi(ui);
	initSize(720, 598);
	setResizeEnabled(false);

#ifdef Q_OS_MACOS
	setHasCaption(true);
	setMoveInContent(false);
	setHasHLine(false);
	setWindowTitle("");
#else
	setHasCaption(false);
	setMoveInContent(true);
	setHasHLine(true);
#endif // Q_OS_MACOS

	connect(ui->allSelectedCheckBox, &PLSCheckBox::stateChanged, this, &PLSNaverShoppingUseTerm::allAgreeButtonStateChanged);
	connect(ui->termOfUserCheckBox, &PLSCheckBox::stateChanged, this, &PLSNaverShoppingUseTerm::checkBoxButtonStateChanged);
	connect(ui->operationPolicyCheckBox, &PLSCheckBox::stateChanged, this, &PLSNaverShoppingUseTerm::checkBoxButtonStateChanged);
	doUpdateOkButtonState();

	auto closeEvent = [this](const QCloseEvent *) {
		hide();
		m_useTermCefWidget->closeBrowser();
		m_policyCefWidget->closeBrowser();
		return true;
	};
	setCloseEventCallback(closeEvent);
}

PLSNaverShoppingUseTerm::~PLSNaverShoppingUseTerm()
{
	pls_delete(ui);
}

void PLSNaverShoppingUseTerm::setLoadingURL(const QString &useTermUrl, const QString &policyUrl)
{
	m_useTermCefWidget = pls::browser::newBrowserWidget(pls::browser::Params().url(useTermUrl).initBkgColor(QColor(17, 17, 17)).showAtLoadEnded(true).css(common::TERM_WEBVIEW_CSS));
	ui->verticalLayout_2->addWidget(m_useTermCefWidget);

	m_policyCefWidget = pls::browser::newBrowserWidget(pls::browser::Params().url(policyUrl).initBkgColor(QColor(17, 17, 17)).showAtLoadEnded(true).css(common::TERM_WEBVIEW_CSS));
	ui->verticalLayout_3->addWidget(m_policyCefWidget);
}

void PLSNaverShoppingUseTerm::on_closeButton_clicked()
{
	PLS_UI_STEP(logMoudule, "Live Term Status: PLSNaverShoppingUseTerm close button", ACTION_CLICK);
	this->reject();
}

void PLSNaverShoppingUseTerm::on_confirmButton_clicked()
{
	PLS_UI_STEP(logMoudule, "Live Term Status: PLSNaverShoppingUseTerm confirm button", ACTION_CLICK);
	this->accept();
}

void PLSNaverShoppingUseTerm::allAgreeButtonStateChanged(int)
{
	QSignalBlocker signalBlocker1(ui->termOfUserCheckBox);
	QSignalBlocker signalBlocker2(ui->operationPolicyCheckBox);
	bool checked = Qt::Checked == ui->allSelectedCheckBox->checkState();
	ui->termOfUserCheckBox->setChecked(checked);
	ui->operationPolicyCheckBox->setChecked(checked);
	doUpdateOkButtonState();
}

void PLSNaverShoppingUseTerm::checkBoxButtonStateChanged(int)
{
	QSignalBlocker signalBlocker(ui->allSelectedCheckBox);
	bool checked = false;
	if ((Qt::Checked == ui->termOfUserCheckBox->checkState()) && (Qt::Checked == ui->operationPolicyCheckBox->checkState())) {
		checked = true;
	}
	ui->allSelectedCheckBox->setChecked(checked);
	doUpdateOkButtonState();
}

void PLSNaverShoppingUseTerm::doUpdateOkButtonState()
{
	ui->confirmButton->setEnabled(Qt::Checked == ui->allSelectedCheckBox->checkState());
}

QString PLSNaverShoppingUseTerm::getPolicyJavaScript() const
{
	QString filePath(":/Configs/resource/DefaultResources/pls_navershopping_policy.js");
	QFile file(filePath);
	file.open(QIODevice::ReadOnly | QIODevice::Text);
	QByteArray byteArray = file.readAll();
	file.close();
	QString str = byteArray;
	return str;
}
