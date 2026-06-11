
#include "MutiLanguageTestView.h"
#include "ui_MutiLanguageTestView.h"
#include <QMessageBox>
#include "PLSAlertView.h"
#include "frontend-api.h"

MutiLanguageTestView::MutiLanguageTestView(QWidget *parent) : PLSDialogView(parent)
{
	pls_add_css(this, {"PLSLoginMainView", "PrismLoginView", "PLSEidt"});
	ui = pls_new<Ui::MutiLanguageTestView>();
	setupUi(ui);
	setWindowTitle(QString("MutiLanguage Tool"));
	setResizeEnabled(false);
	setHasMaxResButton(false);
	setHasMinButton(false);
	setMoveInContent(true);
	setWindowIcon(QIcon(":/resource/images/logo/PRISMLiveStudio.ico"));
	addMacTopMargin();
	initSize(600, 450);
	initUi();
}

MutiLanguageTestView::~MutiLanguageTestView()
{
	delete ui;
}

void MutiLanguageTestView::initUi()
{
	const auto metaEnum = QMetaEnum::fromType<ButtonRole>();
	for (int i = 0; i < metaEnum.keyCount(); i++) {
		auto enumValue = metaEnum.value(i);
		ui->comboBox_button_1->addItem(metaEnum.valueToKey(enumValue), enumValue);
		ui->comboBox_button_2->addItem(metaEnum.valueToKey(enumValue), enumValue);
		ui->comboBox_button_3->addItem(metaEnum.valueToKey(enumValue), enumValue);
	}
	ui->comboBox_button_1->setCurrentIndex(1);
}

void MutiLanguageTestView::on_button_show_message_clicked()
{
	auto title = ui->comboBox_title_key->currentText().trimmed();
	auto message = ui->lineEdit_message_key->text().trimmed();
	auto button1Text = ui->lineEdit_button_1_key->text().trimmed();
	auto button2Text = ui->lineEdit_button_2_key->text().trimmed();
	auto button3Text = ui->lineEdit_button_3_key->text().trimmed();

	QDialogButtonBox::StandardButtons buttons = ui->comboBox_button_1->currentData().value<QDialogButtonBox::StandardButton>() |
						    ui->comboBox_button_2->currentData().value<QDialogButtonBox::StandardButton>() |
						    ui->comboBox_button_3->currentData().value<QDialogButtonBox::StandardButton>();
	if (buttons == 0) {
		return;
	}
	QMap<PLSAlertView::Button, QString> buttonsEnums{{ui->comboBox_button_1->currentData().value<QDialogButtonBox::StandardButton>(),
							  button1Text.isEmpty() ? tr(ui->comboBox_button_1->currentText().toUtf8().constData()) : tr(button1Text.toUtf8().constData())},
							 {ui->comboBox_button_2->currentData().value<QDialogButtonBox::StandardButton>(),
							  button2Text.isEmpty() ? tr(ui->comboBox_button_2->currentText().toUtf8().constData()) : tr(button2Text.toUtf8().constData())},
							 {ui->comboBox_button_3->currentData().value<QDialogButtonBox::StandardButton>(),
							  button3Text.isEmpty() ? tr(ui->comboBox_button_3->currentText().toUtf8().constData()) : tr(button3Text.toUtf8().constData())}};

	if (ui->comboBox_message_type->currentText() == "Normal") {
		PLSAlertView::information(nullptr, tr(title.toUtf8().constData()), tr(message.toUtf8().constData()), buttonsEnums);
	} else {
		pls_alert_error_message(nullptr, tr(title.toUtf8().constData()), tr(message.toUtf8().constData()), buttonsEnums);
	}
}
