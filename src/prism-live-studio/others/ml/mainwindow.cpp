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

static QString getAppSourceRootDir(const QString&dirPath)
{
	auto exportIniPath = dirPath;
	QDir dir(QDir::toNativeSeparators(exportIniPath));
	if(!dir.exists())
	{
		dir.setPath(g_appDir);
	}
	bool isSuccess = dir.absolutePath().contains("/src");
	QString appRootPath= dir.absolutePath() ;
	if(isSuccess){
		auto index = appRootPath.indexOf("/src");
		appRootPath = appRootPath.left(index) +"/src/prism-live-studio/";
	}else{
		appRootPath = appRootPath+"/src/prism-live-studio/";
	}
	return appRootPath;
}

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
	connect(&m_timer, &QTimer::timeout,this,[this](){if(m_genCount ==0){
		ui->log->append("start importIni to App\n");
		startImportIniToApp();
		m_timer.stop();
		m_genCount = -1;
		QMessageBox::information(this, "Notice", "The latest ini file has been copied to the app directory");

}});
	m_timer.setInterval(1000);
	g_appDir = QCoreApplication::applicationDirPath()+"/";
	ui->lineEdit_3->setText(g_appDir);
	ui->export_dir_edit->setText(getAppSourceRootDir(g_appDir));
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
	qDebug()<<"current thread"<<QThread::currentThreadId();
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
			QMetaObject::invokeMethod(this,[this,iniFilePath](){
				ui->log->append(iniFilePath + "save ok!");
			});
			file.close();
		}
	}
	--m_genCount;
}

static void findLocaleDirPath(const QString&targetName, const QString &modulePath, QString& targetAbsolutePath)
{
	if(targetName.isEmpty() || modulePath.isEmpty())
	{
		return;
	}
	QDir dir(modulePath);
	QStringList subdirectories = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
	for (auto subdirName : subdirectories) {
		QDir subdir(dir.filePath(subdirName));
		if (subdir.dirName() == targetName) {
			targetAbsolutePath =  subdir.absolutePath();
		}
		findLocaleDirPath(targetName, subdir.absolutePath(),targetAbsolutePath);
	}
}

static bool copyDirectory(const QString &srcPath, const QString &dstPath, bool coverFileIfExist)
{
	QDir srcDir(srcPath);
	QDir dstDir(dstPath);
	if (!dstDir.exists()) {
		if (!dstDir.mkpath(dstDir.absolutePath()))
			return false;
	}

	QFileInfoList fileInfoList = srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
	for (QFileInfo fileInfo : fileInfoList) {
		if (fileInfo.isDir()) {
			if (!copyDirectory(fileInfo.filePath(), dstDir.filePath(fileInfo.fileName()), coverFileIfExist)) {
				return false;
			}
			continue;
		}
		if (dstDir.exists(fileInfo.fileName())) {
			if (!coverFileIfExist) {
				continue;
			}
			dstDir.remove(fileInfo.fileName());
		}
		if (!QFile::copy(fileInfo.filePath(), dstDir.filePath(fileInfo.fileName()))) {
			return false;
		}
	}
	return true;
}

static void importPrismMainIni(const QString&srcDirPath, const QString&dstDirPath){
	copyDirectory(srcDirPath+"/locale", dstDirPath+"/data/locale",true);

}

static void importPrismPluginsIni(const QString&srcDirPath, const QString&dstDirPath){

	QDir srcDir(srcDirPath);
	auto subDirs = srcDir.entryList(QDir::Dirs|QDir::NoDotAndDotDot);
	for(auto subdirName : subDirs)
	{
		QDir subdir(srcDir.filePath(subdirName));
		QString targetDirPath;
		findLocaleDirPath(subdir.dirName(), dstDirPath,targetDirPath);
		copyDirectory(subdir.absoluteFilePath("locale"),targetDirPath+"/data/locale",true);
	}
}
static void importPrismSetupIni(const QString&srcDirPath, const QString&dstDirPath){
        copyDirectory(srcDirPath+"/locale", dstDirPath+"/data/locale",true);
}

static void importIniToBin(const QString&dstDirPath)
{
	copyDirectory(g_appDir +"data/prism-studio",dstDirPath +"prism-studio",true);
	copyDirectory(g_appDir +"data/prism-setup",dstDirPath +"prism-setup",true);
	copyDirectory(g_appDir +"data/prism-plugins",dstDirPath +"prism-plugins",true);

}

 QString MainWindow::absoluteDirPath(const QString&moduleName){

        return getAppSourceRootDir(ui->export_dir_edit->text()) + moduleName;
}
void MainWindow::startImportIniToApp()
{
	auto iniSrcDirPath = g_appDir +"data/";
	if(ui->prism_studio->isChecked())
		importPrismMainIni(iniSrcDirPath + ui->prism_studio->text(),absoluteDirPath("PRISMLiveStudio"));
	if(ui->prism_setup->isChecked())
		importPrismSetupIni(iniSrcDirPath + ui->prism_setup->text(),absoluteDirPath("PRISMSetup"));
	if(ui->prism_plugins->isChecked())
		importPrismPluginsIni(iniSrcDirPath + ui->prism_plugins->text(),absoluteDirPath("PRISMLiveStudio/plugins"));
	if(ui->checkBox->isChecked())
	{
		auto dstDirPath = absoluteDirPath(QString("../../bin/prism/windows/%1/data/").arg(ui->debug_radio->isChecked()?"Debug":"Release"));
		importIniToBin(dstDirPath);
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
	m_genCount = -1;
	m_timer.start();

	QFileInfo info(ui->fillColorFile->text());
	if (!info.isFile()) {
		QMessageBox::critical(this, "Error", "please a excel file!");
		return;
	}
	if(ui->export_dir_edit->text().isEmpty()){
		QMessageBox::critical(this, "Error", "please input app project path!");
		return;
	}

	QXlsx::Document doc(ui->fillColorFile->text());
	doc.load();
	ui->log->append("start READ excels");
	QApplication::processEvents();

	const auto &sheetNames = doc.sheetNames();
	m_genCount = sheetNames.size();
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
		ui->log->append("start gen ini files");
		QApplication::processEvents();
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


void MainWindow::on_pushButton_6_clicked()
{
    auto path = QFileDialog::getOpenFileName(this, "From", QString(), "Excel (*.xlsx)");
    if (!path.isEmpty()) {
        ui->lineEdit_FromFile->setText(path);
    }
}

void MainWindow::on_pushButton_7_clicked()
{
    auto path = QFileDialog::getOpenFileName(this, "To", QString(), "Excel (*.xlsx)");
    if (!path.isEmpty()) {
        ui->lineEdit_ToFile->setText(path);
    }
}

void MainWindow::on_pushButton_5_clicked()
{
    static bool bDoing = false;
    if (bDoing)
    {
        return;
    }
    bDoing = true;
    auto _cleanup = qScopeGuard([]{ bDoing = false; });

    if (ui->lineEdit_FromFile->text().isEmpty() || ui->lineEdit_ToFile->text().isEmpty() || ui->lineEdit_FromFile->text() == ui->lineEdit_ToFile->text())
    {
        ui->log->append("Please input from and to files");
        return;
    }

    if (!QFileInfo(ui->lineEdit_FromFile->text()).exists())
    {
        ui->log->append("The from excel file is not exists");
        return;
    }

    if (!QFileInfo(ui->lineEdit_ToFile->text()).exists())
    {
        ui->log->append("The to excel file is not exists");
        return;
    }

    QXlsx::Document docFrom(ui->lineEdit_FromFile->text());
    QXlsx::Document docTo(ui->lineEdit_ToFile->text());
    QXlsx::Document docTmp;

    if (!docFrom.load() || !docTo.load())
    {
        ui->log->append("Can't load excel files");
        return;
    }

    auto colIndex = ui->spinBox_FromCol->value();
    QMap<QString, QMap<QString, QMap<QString, QVariant>>> mapData;

    for (auto &sheetName : docFrom.sheetNames())
    {
        docFrom.selectSheet(sheetName);
        ui->log->append(QString("Process sheet: %1 ...").arg(sheetName));
        QApplication::processEvents();

        auto rowIndex = 1;
        while (true)
        {
            auto valuePath = docFrom.read(rowIndex, 1).toString();
            auto valueKey = docFrom.read(rowIndex, 2).toString();
            auto value = docFrom.read(rowIndex, colIndex);

            if (valuePath.isEmpty() || valueKey.isEmpty())
            {
                break;
            }

            if (mapData[sheetName][valuePath].contains(valueKey))
            {
                ui->log->append(QString("Sheet:%1 path:%2 key:%3 is duplicated").arg(sheetName, valuePath, valueKey));
                QApplication::processEvents();
            }
            mapData[sheetName][valuePath][valueKey] = value;

            ++rowIndex;
        }
    }

    for (auto &sheetName : docTo.sheetNames())
    {
        docTo.selectSheet(sheetName);
        docTmp.addSheet(sheetName);

        auto rowIndex = 1;
        while (true)
        {
            auto valuePath = docTo.read(rowIndex, 1).toString();
            auto valueKey = docTo.read(rowIndex, 2).toString();

            if (valuePath.isEmpty() || valueKey.isEmpty())
            {
                break;
            }

            if (!mapData[sheetName][valuePath].contains(valueKey))
            {
                ui->log->append(QString("Sheet:%1 path:%2 key:%3 is not exists").arg(sheetName, valuePath, valueKey));
                QApplication::processEvents();
            }

            docTmp.write(rowIndex, 1, valuePath);
            docTmp.write(rowIndex, 2, valueKey);
            docTmp.write(rowIndex, 3, mapData[sheetName][valuePath][valueKey]);

            ++rowIndex;
        }
    }
    docTmp.saveAs("tmp.xlsx");

    ui->log->append("Merge done");
}

