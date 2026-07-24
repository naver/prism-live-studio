#include "PLSErrorCodeTransformTool.h"
#include "ui_PLSErrorCodeTransformTool.h"
#include <QFileDialog>
#include <QProcess>
#include <QStandardPaths>
#include <libresource.h>
#include "frontend-api.h"
#include "obs-app.hpp"

namespace {

QString defaultErrorCodeTransformPythonExe()
{
	return pls_is_os_sys_macos() ? QStringLiteral("python3") : QStringLiteral("python");
}

} // namespace

PLSErrorCodeTransformTool::PLSErrorCodeTransformTool(QWidget *parent) : PLSToolView<PLSErrorCodeTransformTool>(parent), ui(new Ui::PLSErrorCodeTransformTool)
{
	setStyleSheet("QPlainTextEdit { min-height:  60px; max-height: 60px;}; QTextEdit { background-color: #222;};");
	setupUi(ui);
	initSize(1200, 680);
	setWindowTitle("Error Code Transform Tool");

	const QString appDirPath = pls_get_app_data_path_pn(QStringLiteral("resources/errorCode"));
	m_defaultOutputPath = pls_get_app_user_data_dir_path_pn(QStringLiteral("resources/library/Library_Policy_PC/errorCode.json"), false);
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

	const QString defaultPythonExe = defaultErrorCodeTransformPythonExe();
	QString savedPython = QString(config_get_string(App()->GetUserConfig(), "ErrorCodeTransform", "PythonInterpreter")).trimmed();
	ui->lineEdit_python->setText(savedPython.isEmpty() ? defaultPythonExe : savedPython);

	connect(ui->pushButton, &QPushButton::clicked, this, [this]() {
		auto tmp = ui->plainTextEdit->toPlainText().trimmed();
		if (tmp.isEmpty()) {
			tmp = QString(config_get_string(App()->GetUserConfig(), "ErrorCodeTransform", "InputFile")).trimmed();
		}
		if (tmp.isEmpty()) {
			tmp = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
		}

		QString startDir;
		if (!tmp.isEmpty()) {
			const QFileInfo fi(tmp);
			startDir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
		}
		QString imageFilePath = QFileDialog::getOpenFileName(this, tr("Browse"), startDir, "Excel Files (*.xlsx)");
		if (!imageFilePath.isEmpty()) {
			ui->plainTextEdit->setPlainText(imageFilePath);
			config_set_string(App()->GetUserConfig(), "ErrorCodeTransform", "InputFile", imageFilePath.toUtf8().constData());
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
			config_set_string(App()->GetUserConfig(), "ErrorCodeTransform", "OutputFile", imageFilePath.toUtf8().constData());
		}
#ifdef Q_OS_MACOS
		pls_bring_mac_window_to_front(winId());
#endif
	});

	connect(ui->pushButton_default, &QPushButton::clicked, this, [this]() {
		if (m_defaultOutputPath.isEmpty()) {
			m_defaultOutputPath = PLS_RSM_getLibraryPolicy_Path(QStringLiteral("Library_Policy_PC/errorCode.json"));
		}
		ui->plainTextEdit_2->setPlainText(m_defaultOutputPath);
		config_set_string(App()->GetUserConfig(), "ErrorCodeTransform", "OutputFile", m_defaultOutputPath.toUtf8().constData());
	});

	connect(ui->pushButton_start, &QPushButton::clicked, this, &PLSErrorCodeTransformTool::startTransform);
	connect(ui->pushButton_clear, &QPushButton::clicked, this, [this]() { ui->textEdit->clear(); });

	connect(ui->pushButton_test, &QPushButton::clicked, this, &PLSErrorCodeTransformTool::testPrismCodeAlert);
	connect(ui->lineEdit_prismCode, &QLineEdit::returnPressed, this, &PLSErrorCodeTransformTool::testPrismCodeAlert);
}

void PLSErrorCodeTransformTool::testPrismCodeAlert()
{
	bool ok = false;
	const int code = ui->lineEdit_prismCode->text().trimmed().toInt(&ok);
	if (!ok) {
		ui->textEdit->append(tr("[test] Invalid prism code: enter a decimal integer."));
		return;
	}
	PLSErrorHandler::showAlertByPrismCode(static_cast<PLSErrorHandler::ErrCode>(code), PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData("ErrorCodeTransformTool test alert"), this);
	ui->textEdit->append(tr("[test] Prism code %1: alert dialog shown.").arg(code));
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
	QString exe = ui->lineEdit_python->text().trimmed();
	if (exe.isEmpty()) {
		exe = defaultErrorCodeTransformPythonExe();
	}
	process.setProcessChannelMode(QProcess::MergedChannels);
	process.start(exe, args);
	qDebug() << "cmd: " << QString("%1 %2/errorExcel2Json.py -o \"%3\" -i \"%4\" --fromui").arg(exe, path, outputFile, inputFile);

	constexpr int kStartTimeoutMs = 30000;
	if (!process.waitForStarted(kStartTimeoutMs)) {
		ui->textEdit->insertPlainText(QString("\nprocess failed to start: %1").arg(process.errorString()));
		ui->textEdit->insertPlainText(QString("\nQProcess::ProcessError: %1").arg(static_cast<int>(process.error())));
		ui->textEdit->insertPlainText("\nexitCode: N/A");
		ui->textEdit->insertPlainText("\ndone: failed");
		ui->textEdit->insertPlainText("\n---------\n");
		return;
	}

	process.waitForFinished(-1);
	const QString mergedOutput = QString::fromUtf8(process.readAllStandardOutput());
	for (const QString &line : mergedOutput.split('\n')) {
		const QString t = line.trimmed();
		if (!t.isEmpty()) {
			ui->textEdit->insertPlainText("\n" + t);
		}
	}
	ui->textEdit->insertPlainText(QString("\nexitCode: %1").arg(process.exitCode()));
	ui->textEdit->insertPlainText(QString("\ndone: %1").arg(process.exitCode() == 0 ? "succeed" : "failed"));

	if (process.exitCode() != 0) {
		return;
	}
	config_set_string(App()->GetUserConfig(), "ErrorCodeTransform", "InputFile", inputFile.toUtf8().constData());
	config_set_string(App()->GetUserConfig(), "ErrorCodeTransform", "OutputFile", outputFile.toUtf8().constData());
	config_set_string(App()->GetUserConfig(), "ErrorCodeTransform", "PythonInterpreter", exe.toUtf8().constData());

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
