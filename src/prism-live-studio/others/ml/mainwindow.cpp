#include <windows.h>
#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <algorithm>
#include <xlsxdocument.h>

#include <QDebug>
#include <QFileDialog>
#include <QSettings>
#include <QTextCodec>
#include <QMessageBox>
#include <QThreadPool>
#include <QtConcurrent>

static QString g_appDir = "C:/code/prism-live-studio/bin/";
static QHash<int, QXlsx::Format> m_formats;
static QStringList m_oldKeys;
static bool g_isIni = true;
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	QXlsx::Format hdrFormat, newFormat, emptyFormat, defaultFormat;
	hdrFormat.setPatternBackgroundColor(Qt::gray);
	hdrFormat.setFontColor(Qt::white);
	newFormat.setPatternBackgroundColor(Qt::blue);
	newFormat.setFontColor(Qt::white);
	emptyFormat.setPatternBackgroundColor(Qt::yellow);
	emptyFormat.setFontColor(Qt::white);
	defaultFormat.setPatternBackgroundColor(Qt::white);
	defaultFormat.setFontColor(Qt::black);
	defaultFormat.setBorderColor(Qt::gray);
	defaultFormat.setBorderStyle(QXlsx::Format::BorderThick);
	m_formats.insert(Qt::gray, hdrFormat);
	m_formats.insert(Qt::blue, newFormat);
	m_formats.insert(Qt::yellow, emptyFormat);
	m_formats.insert(Qt::white, defaultFormat);

	g_isIni = ui->iniRadioBtn->isChecked();
	m_fileFlagGroup.addButton(ui->iniRadioBtn, 0);
	m_fileFlagGroup.addButton(ui->jsonRadioBtn, 1);
	connect(&m_fileFlagGroup, &QButtonGroup::idClicked, [this](int id) { g_isIni = (id == 0); });
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::on_localeDirBrowse_clicked()
{
	QString dir = QFileDialog::getExistingDirectory(this, "select", "C:/");
	ui->appDir->setText(dir);
}

void MainWindow::on_outFileBrowse_clicked()
{
	QString file = QFileDialog::getSaveFileName(this, "save as", "C:/", "*.xlsx");
	ui->outFile->setText(file);
}

struct Text {
	QString key;
	QString value;
	QStringList files;
	QString path;
};

static bool isEqual(unsigned int a, unsigned int b)
{
	return a == b;
}

static QString removeBom(const QString &key)
{
	if (key.length() >= 3 && (key[0] == 0xEF) && (key[1] == 0xBB) && (key[2] == 0xBF)) {
		return key.mid(3);
	}
	return key;
}
static QString pls_remove_quotes(const QString &str)
{
	if (!str.startsWith('"') || !str.endsWith('"')) {
		return str;
	}

	return str.mid(1, str.length() - 2);
}
static void insert_text(QTextEdit *log, QStringList &keys, const QString &path, QHash<QString, QHash<QString, Text>> &texts, const QString &language, const QString &key, const QString &value,
			const QString &relativeFileName)
{
	//log->append(QString("%1=%2").arg(key, value));

	if (!keys.contains(key, Qt::CaseInsensitive)) {
		keys.append(key);
	}

	auto lkey = key.toLower();
	auto &vals = texts[language];
	auto iter = vals.find(lkey);
	if (iter == vals.end()) {
		vals[lkey] = Text{key, value, QStringList{relativeFileName}, path};
	} else {
		iter.value().files.append(relativeFileName);
		log->append(QString("%1 duplicated, files: %2").arg(key, iter.value().files.join(',')));
		QApplication::processEvents();
	}
}
static void add_text(QTextEdit *log, QStringList &keys, QHash<QString, QHash<QString, Text>> &texts, const QString &languageFile, const QString &relativeFileName, const QString &language)
{
	log->append(QString("process %1").arg(relativeFileName));
	QFileInfo fileInfo(languageFile);
	QDir dir(fileInfo.absoluteDir());
	if (g_isIni) {
		QSettings settings(languageFile, QSettings::IniFormat);
		settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
		for (const QString &key : settings.allKeys()) {
			QApplication::processEvents();
			QString value = pls_remove_quotes(settings.value(key).toString());
			insert_text(log, keys, fileInfo.absolutePath().remove(g_appDir), texts, language, removeBom(key), value, relativeFileName);
		}
	} else {
		QFile file(languageFile);
		if (file.open(QFile::ReadOnly)) {
			auto data = file.readAll();
			auto jsonObj = QJsonDocument::fromJson(data).object();
			for (const QString &key : jsonObj.keys()) {
				QApplication::processEvents();
				QString value = pls_remove_quotes(jsonObj.value(key).toString());
				insert_text(log, keys, fileInfo.absolutePath().remove(g_appDir), texts, language, removeBom(key), value, relativeFileName);
			}
		}
	}
}
static void load_language(QTextEdit *log, QStringList &keys, QHash<QString, QHash<QString, Text>> &texts, const QString &languageDir, const QString &language, bool ignoreRootFile,
			  const QString &relativeFileName = QString())
{
	QString ignoreFileName = QStringLiteral("data/prism-studio/locale/") + language + QStringLiteral(".ini");

	QDir dir(languageDir);
	auto fis = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsLast | QDir::Name);
	for (const auto &fi : fis) {
		QString relFileName;
		if (!relativeFileName.isEmpty()) {
			relFileName = relativeFileName + '/' + fi.fileName();
		} else {
			relFileName = fi.fileName();
		}

		if (fi.isDir()) {
			load_language(log, keys, texts, fi.absoluteFilePath(), language, ignoreRootFile, relFileName);
		} else if (fi.completeBaseName() == language && (!ignoreRootFile || relFileName != ignoreFileName)) {
			add_text(log, keys, texts, fi.absoluteFilePath(), relFileName, language);
		}
	}
}
static int findRow(QXlsx::Document &doc, const QString &key)
{
	auto dim = doc.dimension();
	for (int row = 2, rowCount = dim.rowCount(); row <= rowCount; ++row) {
		if (doc.read(row, 2).toString().trimmed() == key) {
			return row;
		}
	}
	return -1;
}
static void genExcelFromJson(QTextEdit *log, QXlsx::Document &doc, const QStringList &langs, int langCount, const QString &localeDir)
{
	QStringList keys;
	QHash<QString, QHash<QString, Text>> texts;
}
static void gen(QTextEdit *log, QXlsx::Document &doc, const QString &feature, const QStringList &langs, int langCount, const QString &appDir, const QString &languageDir, bool ignoreRootFile,
		const QXlsx::Format &hdrFormat, const QXlsx::Format &newFormat)
{
	QStringList keys;
	QHash<QString, QHash<QString, Text>> texts;
	for (const auto &lang : langs) {
		load_language(log, keys, texts, appDir + languageDir, lang, ignoreRootFile, languageDir);
	}
	QStringList vikeys;
	QHash<QString, QHash<QString, Text>> viTexts;
	if (feature == "prism") {

		add_text(log, vikeys, viTexts, appDir + languageDir + "/vi-VN.ini", languageDir + "/vi-VN.ini", "vi-VN");
	}
	if (keys.isEmpty()) {
		return;
	}
	m_oldKeys = keys;

	doc.addSheet(feature);
	doc.selectSheet(feature);

	if (doc.dimension().rowCount() <= 0) {
		doc.write(1, 1, "path", hdrFormat);
		doc.write(1, 2, "Key", hdrFormat);
		for (int i = 0; i < langCount; ++i) {
			auto text = langs.at(i);
			doc.write(1, 3 + i, text, text.isEmpty() ? m_formats.value(Qt::yellow) : hdrFormat);
		}

		for (int i = 0, count = keys.count(), row = 2; i < count; ++i, ++row) {
			QApplication::processEvents();

			const auto &key = keys[i];
			doc.write(row, 2, key);

			for (int j = 0; j < langCount; ++j) {
				const auto &text = texts[langs.at(j)][key.toLower()].value;
				const auto &pathStr = texts[langs.at(j)][key.toLower()].path;
				if (!pathStr.isEmpty()) {
					doc.write(row, 1, pathStr, text.isEmpty() ? m_formats.value(Qt::yellow) : QXlsx::Format());
				}
				if (j == langCount - 1 && feature == "prism") {
					const auto &viText = viTexts[langs.at(j)][key.toLower()].value;

					doc.write(row, 3 + j, viText, viText.isEmpty() ? m_formats.value(Qt::yellow) : QXlsx::Format());
				} else {
					doc.write(row, 3 + j, text, text.isEmpty() ? m_formats.value(Qt::yellow) : QXlsx::Format());
				}
			}
		}
	} else {
		for (const auto &key : keys) {
			QApplication::processEvents();

			int row = findRow(doc, key);
			if (row < 0) {
				row = doc.dimension().rowCount() + 1;
				doc.write(row, 2, key, newFormat);
			} else {
				doc.write(row, 2, key);
			}
			doc.write(row, 1, texts[langs.at(0)][key.toLower()].path);
			for (int j = 0; j < langCount; ++j) {
				const auto &text = texts[langs.at(j)][key.toLower()].value;
				doc.write(row, 3 + j, text, text.isEmpty() ? m_formats.value(Qt::yellow) : QXlsx::Format());
			}
		}
	}
}

static void gen(QTextEdit *log, QXlsx::Document &doc, const QStringList &langs, int langCount, const QString &appDir, const QString &pluginsDir, bool ignoreRootFile, const QXlsx::Format &hdrFormat,
		const QXlsx::Format &newFormat)
{
	QDir dir(appDir + pluginsDir);
	if (!dir.exists()) {
		return;
	}

	for (const auto &fi : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		gen(log, doc, fi.fileName(), langs, langCount, appDir, pluginsDir + '/' + fi.fileName() + "/locale", ignoreRootFile, hdrFormat, newFormat);
	}
}

void MainWindow::on_gen_clicked()
{
	auto appDir = ui->appDir->text() + '/';
	g_appDir = appDir;
	if (appDir.isEmpty()) {
		QMessageBox::critical(this, "Error", "Please select PRISM application directory");
		return;
	}

	QDir dir(appDir);
	if (!dir.exists()) {
		QMessageBox::critical(this, "Error", "PRISM application directory not found");
		return;
	} else if (!dir.exists("data") && g_isIni) {
		QMessageBox::critical(this, "Error", "data directory not found");
		return;
	}

	auto langs = ui->langs->text().split(';');
	int langCount = langs.count();

	QXlsx::Document doc(ui->outFile->text());
	doc.load();
	if (g_isIni) {
		//prism live stuido ini ->excel
		gen(ui->log, doc, "prism", langs, langCount, appDir, "data/prism-studio/locale", true, m_formats.value(Qt::gray), m_formats.value(Qt::blue));
		gen(ui->log, doc, "setup", langs, langCount, appDir, "data/prism-setup/locale", true, m_formats.value(Qt::gray), m_formats.value(Qt::blue));
		gen(ui->log, doc, "logger", langs, langCount, appDir, "data/prism-logger/locale", true, m_formats.value(Qt::gray), m_formats.value(Qt::blue));
		gen(ui->log, doc, langs, langCount, appDir, "data/obs-plugins", false, m_formats.value(Qt::gray), m_formats.value(Qt::blue));
		gen(ui->log, doc, langs, langCount, appDir, "data/prism-plugins", false, m_formats.value(Qt::gray), m_formats.value(Qt::blue));
		gen(ui->log, doc, langs, langCount, appDir, "data/lab-plugins", false, m_formats.value(Qt::gray), m_formats.value(Qt::blue));
	} else {
		//cam studio json->excel
        gen(ui->log, doc, "cam-studio", langs, langCount, appDir, "data/cam-studio/locale", false, m_formats.value(Qt::gray), m_formats.value(Qt::blue));
	}

	doc.save();
}

void MainWindow::creatSettings(const QString &path)
{
	auto langList = ui->langs->text().split(';');
	for (const auto &langName : langList) {
		QHash<QString, QMap<QString, QString>> contents;
		contents.insert(langName, {});
		m_contents.insert(path, contents);
	}
}

void MainWindow::genIni()
{
	qDebug() << "start gen ini";
	auto contents = m_contents;
	for (auto fileInter = contents.constBegin(); fileInter != contents.constEnd(); ++fileInter) {
		auto path = g_appDir + fileInter.key();
		QDir dir(path);
		if (!dir.exists()) {
			dir.mkpath(path);
		}
		for (auto langInter = fileInter.value().constBegin(); langInter != fileInter.value().constEnd(); ++langInter) {
			auto iniFilePath = dir.absolutePath() + QString("/%1.ini").arg(langInter.key());
			if (!g_isIni) {
				iniFilePath = dir.absolutePath() + QString("/%1.json").arg(langInter.key());
			}
			QFile file(iniFilePath);
			file.open(QIODevice::WriteOnly);
			auto values = langInter.value();
			QJsonObject obj;
			for (auto contentInter = values.constBegin(); contentInter != values.constEnd(); ++contentInter) {
				if (g_isIni) {
					auto value = contentInter.value();
					if (value.isEmpty()) {
						qDebug() << "----------isEmpyt"
							 << "key = " << contentInter.key();
						continue;
					}
					auto str = contentInter.key() + '=' + '"' + value.replace('\r', "\\r").replace('\n', "\\n").replace('\t', "\\t").replace('"', "\\\"") + '"' + '\n';
					file.write(str.toUtf8());
				} else {
					auto value = contentInter.value();
					if (value.isEmpty()) {
						qDebug() << "----------isEmpyt"
							 << "key = " << contentInter.key();
						continue;
					}
					obj.insert(contentInter.key(), value.replace('\r', "\\r").replace('\n', "\\n").replace('\t', "\\t").replace('"', "\\\""));
				}
			}
			if (!g_isIni) {
				QJsonDocument doc;
				doc.setObject(obj);
				file.write(doc.toJson());
			}

			qDebug() << iniFilePath + "save ok!";
			file.close();
		}
	}
}
void MainWindow::on_fillColorFileBrowse_clicked()
{
	const auto &filePath = ui->outFile->text();
	if (filePath.isEmpty()) {
		QString file = QFileDialog::getOpenFileName(this, "open as", "C:/", "*.xlsx");
		ui->fillColorFile->setText(file);
	} else {
		ui->fillColorFile->setText(filePath);
	}
}

void MainWindow::on_chooseColor_clicked()
{
	loadOldIni();
}

void MainWindow::on_fillColor_clicked() {}

void MainWindow::on_import_2_clicked()
{
	QFileInfo info(ui->fillColorFile->text());
	if (!info.isFile()) {
		QMessageBox::critical(this, "Error", "please a excel file!");
		return;
	}

	QXlsx::Document doc(ui->fillColorFile->text());
	doc.load();

	const auto &sheetNames = doc.sheetNames();
	for (const auto &sheetName : sheetNames) {
		doc.selectSheet(sheetName);
		const auto &rowCount = doc.dimension().rowCount();
		const auto &colCount = doc.dimension().columnCount();
		if (rowCount <= 1) {
			continue;
		}

		for (auto rowIndex = 2; rowIndex <= rowCount; ++rowIndex) {
			const auto &path = doc.read(rowIndex, 1).value<QString>();
			const auto &key = doc.read(rowIndex, 2).value<QString>();
			if (m_contents.find(path) == m_contents.end()) {
				creatSettings(path);
			}
			for (auto colIndex = 3; colIndex <= colCount; ++colIndex) {
				auto lang = doc.read(1, colIndex).value<QString>();
				if(lang.isEmpty()){
					continue;
				}
				auto value = doc.read(rowIndex, colIndex).value<QString>();
				m_contents[path][lang].insert(key, value);
			}
		}

		QtConcurrent::run(this, &MainWindow::genIni);
	}
}

void MainWindow::loadOldIni()
{
	QSettings info("C:/code/PRISMLiveStudio/src/prism/main/data/locale/en-US.ini", QSettings::IniFormat);
	m_keys = info.allKeys();

	for (const auto &oldKey : m_oldKeys) {
		if (!m_keys.contains(oldKey)) {
			qDebug() << oldKey << '\n';
		}
	}
}

void MainWindow::on_clearLog_clicked()
{
	ui->log->clear();
}

void MainWindow::on_cmpSrcDirBrowse_clicked()
{
	QString dir = QFileDialog::getExistingDirectory(this, "select", "C:/");
	ui->cmpSrcDir->setText(dir);
}

void MainWindow::on_cmpDstDirBrowse_clicked()
{
	QString dir = QFileDialog::getExistingDirectory(this, "select", "C:/");
	ui->cmpDstDir->setText(dir);
}

void MainWindow::on_cmp_clicked()
{
	m_contents.clear();
	auto otherPath = "data/prism-studio/locale/other";
	//gen old
	QStringList oldkeys;
	QHash<QString, QHash<QString, Text>> oldTexts;
	auto langs = ui->cmpLangs->text().split(';');
	int langCount = langs.count();
	for (const auto &lang : langs) {
		load_language(ui->log, oldkeys, oldTexts, ui->cmpSrcDir->text(), lang, true, "");
		QHash<QString, QMap<QString, QString>> contents;
		contents.insert(lang, {});
		m_contents.insert(otherPath, contents);
	}
	//gen new
	QStringList newkeys;
	QHash<QString, QHash<QString, Text>> newTexts;
	auto newLangDir = "data/prism-studio/locale";

	for (const auto &lang : langs) {
		load_language(ui->log, newkeys, newTexts, ui->cmpDstDir->text() + '/' + newLangDir, lang, true, newLangDir);
	}

	for (const auto &lang : langs) {
		ui->log->append("-----------------------------------------  " + lang + "  ---------------------------------------");

		const auto &oldTextkeys = oldTexts.value(lang).keys();

		const auto &newTextKeys = newTexts.value(lang).keys();
		for (const auto &oldKey : oldTextkeys) {
			if (lang == "id-ID") {
				qDebug() << "old-keys=" << oldKey << "\n";
			}
			bool isExist = false;
			for (const auto &newKey : newTextKeys) {
				if (0 == newKey.compare(oldKey, Qt::CaseInsensitive) || oldKey.startsWith("textmotion")) {
					isExist = true;
					break;
				}
			}
			if (!isExist) {
				auto textInfo = oldTexts.value(lang).value(oldKey);
				ui->log->append(QString("lang: %1, oldKey = %2, oldValue =%3. Can not find in new ini file").arg(lang).arg(textInfo.key).arg(textInfo.value));
				m_contents[otherPath][lang][textInfo.key] = textInfo.value;
			}
		}
	}
	genIni();
}

void MainWindow::on_toolButton_triggered(QAction *arg1)
{
	QString dir = QFileDialog::getExistingDirectory(this, "select", "C:/");
	ui->lineEdit->setText(dir);
	m_repalaceSrcDir = ui->lineEdit->text();
}

void MainWindow::on_toolButton_2_triggered(QAction *arg1)
{
	QString dir = QFileDialog::getExistingDirectory(this, "select", "C:/");
	ui->lineEdit_2->setText(dir);
	m_replaceDstDir = ui->lineEdit_2->text();
}

bool copyDirectoryFiles(const QString &fromDir, const QString &toDir, bool coverFileIfExist)
{
	QDir sourceDir(fromDir);
	QDir targetDir(toDir);
	if (!targetDir.exists()) {
		if (!targetDir.mkdir(targetDir.absolutePath()))
			return false;
	}

	QFileInfoList fileInfoList = sourceDir.entryInfoList();
	foreach(QFileInfo fileInfo, fileInfoList)
	{
		if (fileInfo.fileName() == "." || fileInfo.fileName() == "..")
			continue;

		if (fileInfo.isDir()) {
			if (!copyDirectoryFiles(fileInfo.filePath(), targetDir.filePath(fileInfo.fileName()), coverFileIfExist))
				return false;
		} else {
			if (coverFileIfExist && targetDir.exists(fileInfo.fileName())) {
				targetDir.remove(fileInfo.fileName());
			}

			if (!QFile::copy(fileInfo.filePath(), targetDir.filePath(fileInfo.fileName()))) {
				return false;
			}
		}
	}
	return true;
}

void MainWindow::on_pushButton_clicked()
{
	m_repalaceSrcDir = ui->lineEdit->text();
	m_replaceDstDir = ui->lineEdit_2->text();

	QDir dstSubDirs(m_replaceDstDir);
	QDir srcSubDirs(m_repalaceSrcDir);
	auto srcList = srcSubDirs.entryList(QDir::Dirs);
	auto dstList = dstSubDirs.entryList(QDir::Dirs);
	srcList.removeOne(".");
	srcList.removeOne("..");
	dstList.removeOne(".");
	dstList.removeOne("..");
	for (auto dirName : dstList) {
		auto dstDir = m_replaceDstDir + "/" + dirName + "/data/locale";
		if (srcList.contains(dirName, Qt::CaseInsensitive)) {
			auto srcDir = m_repalaceSrcDir + "/" + dirName + "/locale";
			auto ret = copyDirectoryFiles(srcDir.toStdString().c_str(), dstDir.toStdString().c_str(), true);
		}
	}
}


void MainWindow::on_pushButton_4_clicked()
{
	QString dir = QFileDialog::getExistingDirectory(this, "select", "C:/");
	ui->lineEdit_3->setText(dir);
	g_appDir = dir+"/";
}

