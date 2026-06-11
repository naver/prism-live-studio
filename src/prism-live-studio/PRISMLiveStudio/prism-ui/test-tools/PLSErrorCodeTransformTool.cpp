#include "PLSErrorCodeTransformTool.h"
#include "ui_PLSErrorCodeTransformTool.h"
#include <QFileDialog>
#include <QProcess>
#include <libresource.h>
#include "obs-app.hpp"

PLSErrorCodeTransformTool::PLSErrorCodeTransformTool(QWidget *parent) : PLSToolView<PLSErrorCodeTransformTool>(parent), ui(new Ui::PLSErrorCodeTransformTool)
{
	setStyleSheet("QPlainTextEdit { min-height:  60px; max-height: 60px;}; QTextEdit { background-color: #222;};");
	setupUi(ui);
	initSize(1200, 680);
	setWindowTitle("Error Code Transform Tool");

	m_defaultOutputPath = PLS_RSM_getLibraryPolicyPC_Path(QStringLiteral("Library_Policy_PC/errorCode.json"));
	ui->plainTextEdit_2->setPlaceholderText(QString("eg: %1").arg(m_defaultOutputPath));

	QString placeholderText =
		QString("This tool can convert the Error Code Excel file downloaded from NDriver into a JSON file that can be used by Prism.\nIf the input file path is: \n%1, \nPRISM will re-load the new JSON file immediately after the conversion is successful.")
			.arg(m_defaultOutputPath);
	ui->textEdit->setPlaceholderText(placeholderText);

	ui->plainTextEdit->setPlainText(config_get_string(App()->GetUserConfig(), "ErrorCodeTransform", "InputFile"));
	ui->plainTextEdit_2->setPlainText(config_get_string(App()->GetUserConfig(), "ErrorCodeTransform", "OutputFile"));
	if (ui->plainTextEdit_2->toPlainText().trimmed().isEmpty()) {
		ui->plainTextEdit_2->setPlainText(m_defaultOutputPath);
	}

	connect(ui->pushButton, &QPushButton::clicked, this, [this]() {
		auto tmp = ui->plainTextEdit->toPlainText().trimmed();
		if (tmp.isEmpty()) {
			tmp = QString(config_get_string(App()->GetUserConfig(), "ErrorCodeTransform", "InputFile")).trimmed();
		}

		QString imageFilePath = QFileDialog::getOpenFileName(this, tr("Browse"), QFileInfo(tmp).absolutePath(), "Excel Files (*.xlsx)");
		if (!imageFilePath.isEmpty()) {
			ui->plainTextEdit->setPlainText(imageFilePath);
		}
#ifdef Q_OS_MACOS
		pls_bring_mac_window_to_front(winId());
#endif
	});

	connect(ui->pushButton_file, &QPushButton::clicked, this, [this]() {
		auto tmp = ui->plainTextEdit_2->toPlainText().trimmed();
		if (tmp.isEmpty()) {
			tmp = QString(config_get_string(App()->GetUserConfig(), "ErrorCodeTransform", "OutputFile")).trimmed();
		}
		QString imageFilePath = QFileDialog::getOpenFileName(this, tr("Browse"), QFileInfo(tmp).absolutePath(), "Excel Files (*.json)");
		if (!imageFilePath.isEmpty()) {
			ui->plainTextEdit_2->setPlainText(imageFilePath);
		}
#ifdef Q_OS_MACOS
		pls_bring_mac_window_to_front(winId());
#endif
	});

	connect(ui->pushButton_default, &QPushButton::clicked, this, [this]() { ui->plainTextEdit_2->setPlainText(m_defaultOutputPath); });

	connect(ui->pushButton_start, &QPushButton::clicked, this, &PLSErrorCodeTransformTool::startTransform);
	connect(ui->pushButton_clear, &QPushButton::clicked, this, [this]() { ui->textEdit->clear(); });
}

void PLSErrorCodeTransformTool::startTransform()
{

	auto inputFile = ui->plainTextEdit->toPlainText().trimmed();
	auto outputFile = ui->plainTextEdit_2->toPlainText().trimmed();
	if (!QFile::exists(inputFile) || !inputFile.endsWith(".xlsx")) {
		ui->textEdit->insertPlainText("\ninput file must exists. and end with .xlsx");
		return;
	}
	if (outputFile.isEmpty()) {
		outputFile = m_defaultOutputPath;
	}

	QFileInfo fileinfo(outputFile);
	if (fileinfo.isDir()) {
		outputFile.append("/errorCode.json");
	}

	QProcess process;
	auto path = QFileInfo(pls::rsm::getDataPath()).absoluteFilePath();
	process.setWorkingDirectory(path);
	auto args = QStringList() << "errorExcel2Json.py"
				  << "-o" << outputFile << "-i" << inputFile << "--fromui";
	QString exe = pls_is_os_sys_macos() ? "python3" : "python";
	process.start(exe, args);
	qDebug() << "cmd: " << QString("cd %1 && %2 errorExcel2Json.py -o \"%3\" -i \"%4\" --fromui").arg(path).arg(exe).arg(outputFile).arg(inputFile);

	while (process.waitForReadyRead()) {
		while (process.canReadLine()) {
			ui->textEdit->insertPlainText("\n" + process.readLine().trimmed());
		}
	}
	ui->textEdit->insertPlainText(QString("\nexitCode: %1").arg(process.exitCode()));
	ui->textEdit->insertPlainText(QString("\ndone: %1").arg(process.exitCode() == 0 ? "succeed" : "failed"));

	if (process.exitCode() != 0) {
		return;
	}
	config_set_string(App()->GetUserConfig(), "ErrorCodeTransform", "InputFile", inputFile.toUtf8().constData());
	config_set_string(App()->GetUserConfig(), "ErrorCodeTransform", "OutputFile", outputFile.toUtf8().constData());

	if (outputFile == m_defaultOutputPath) {
		PLSErrorHandler::instance()->loadJson();
		ui->textEdit->insertPlainText("\nError Code Json Re-Load succeed!");
	}
	ui->textEdit->insertPlainText("\n---------\n");
}

PLSErrorCodeTransformTool::~PLSErrorCodeTransformTool()
{
	delete ui;
}
