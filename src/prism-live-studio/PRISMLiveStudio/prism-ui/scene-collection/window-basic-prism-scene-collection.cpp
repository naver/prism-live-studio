#include "window-basic-main.hpp"
#include "PLSBasic.h"
#include "importers/importers.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "frontend-api.h"
#include "qt-wrappers.hpp"
#include "PLSNameDialog.hpp"
#include "PLSSceneDataMgr.h"
#include "PLSMessageBox.h"
#include "PLSImporter.h"
#include "PLSPrismShareMemory.h"
#include "pls/pls-obs-api.h"

#include <QCryptographicHash>

static const std::vector<QString> appSupportSuffix = {".json", ".psc", ".bpres", ".xml", ".xconfig"};
static constexpr const char *PRISM_APP_DEFAULT_SUFFIX = ".psc";
static constexpr const char *SCENE_COLLECTION_COMBOBOX_MANAGEMENT = "Scene Collection Management";

void EnumSceneCollections(const std::function<bool(const char *, const char *)> &cb)
{
	pls::chars<512> path;
	if (int ret = GetConfigPath(path, sizeof(path), "PRISMLiveStudio/basic/scenes/"); ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get config path for scene "
					  "collections");
		return;
	}

	// order by created time
	auto orderByCreateTime = [](const QFileInfo &left, const QFileInfo &right) {
		if (left.birthTime().toMSecsSinceEpoch() != right.birthTime().toMSecsSinceEpoch()) {
			return left.birthTime().toMSecsSinceEpoch() > right.birthTime().toMSecsSinceEpoch();
		} else {
			return left.lastModified().toMSecsSinceEpoch() > right.lastModified().toMSecsSinceEpoch();
		}
	};

	QDir sourceDir(path.toString());
	QStringList supportList;
	auto supportFunc = [&supportList](const QString &supportSuffix) { supportList << "*" + supportSuffix; };
	std::for_each(appSupportSuffix.begin(), appSupportSuffix.end(), supportFunc);

	QFileInfoList fileInfoList = sourceDir.entryInfoList(supportList, QDir::Files, QDir::Time);
	std::sort(fileInfoList.begin(), fileInfoList.end(), orderByCreateTime);

	for (const auto &fileInfo : fileInfoList) {
		auto filePath = path + fileInfo.fileName();
		obs_data_t *data = obs_data_create_from_json_file_safe(filePath.toStdString().c_str(), "bak");
		std::string name = obs_data_get_string(data, "name");
		/* if no name found, use the file name as the name
		 * (this only happens when switching to the new version) */
		if (name.empty()) {
			name = filePath.toStdString();
			name = name.substr(name.find_last_of('/') + 1);
			std::string suffix = name.substr(name.find_last_of('.'));
			name.erase(name.size() - strlen(suffix.c_str()), strlen(suffix.c_str()));
		}
		obs_data_release(data);
		if (!cb(name.c_str(), filePath.toStdString().c_str()))
			break;
	}
}

void OBSBasic::on_actionSceneCollectionManagement_triggered()
{
	ShowSceneCollectionView();
}

void OBSBasic::on_actionRenameSceneCollection_triggered(const QString &name, const QString &path)
{
	std::string newName;
	std::string file;

	QString oldFile = path;
	if (oldFile.isEmpty())
		return;
	QString fileName = ExtractFileName(oldFile.toStdString()).c_str();

	QString oldName = name;
	if (oldName.isEmpty())
		return;

	if (bool success = GetSceneCollectionName(sceneCollectionView, newName, file, SceneSetOperatorType::RenameSceneSet, oldName.toStdString().c_str()); !success)
		return;

	const char *curName = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
	const char *curFile = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");
	pls_used(curFile);
	if (0 == fileName.compare(curFile) && 0 == oldName.compare(curName)) {
		config_set_string(App()->GlobalConfig(), "Basic", "SceneCollection", newName.c_str());
		config_set_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile", file.c_str());
	}
	PLSSceneDataMgr::Instance()->MoveSrcToDest(QString::fromStdString(fileName.toStdString().c_str()), QString::fromStdString(file));

	QString newPath = (file.insert(0, pls_get_user_path("PRISMLiveStudio/basic/scenes/").toStdString()) + ".json").c_str();
	if (int res = os_rename(oldFile.toStdString().c_str(), newPath.toStdString().c_str()); 0 == res) {
		OBSData scenedata = obs_data_create_from_json_file(newPath.toStdString().c_str());
		obs_data_release(scenedata);
		obs_data_set_string(scenedata, "name", newName.c_str());
		obs_data_save_json_safe(scenedata, newPath.toStdString().c_str(), "tmp", "bak");
	} else {
		PLS_WARN(MAIN_SCENE_COLLECTION, "RenameCollection: Failed to rename file %s to %s", pls_get_path_file_name(oldFile.toStdString().c_str()),
			 pls_get_path_file_name(newPath.toStdString().c_str()));
	}
	os_unlink(oldFile.toStdString().c_str());

	oldFile += ".bak";
	os_unlink(oldFile.toStdString().c_str());

	sceneCollectionManageView->RenameSceneCollection(name, path, newName.c_str(), newPath);
	sceneCollectionView->RenameCollectionItem(name, path, newName.c_str(), newPath);
	sceneCollectionManageTitle->SetText(config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection"));

	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");
	PLS_INFO(MAINMENU_MODULE, "Renamed scene collection to '%s' (%s.json)", newName.c_str(), pls_get_path_file_name(file.c_str()));
	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");

	UpdateTitleBar();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
	}
}

void OBSBasic::on_actionRemoveSceneCollection_triggered(const QString &name, const QString &path)
{
	QString oldFile = path;
	if (oldFile.isEmpty())
		return;
	QString fileName = ExtractFileName(oldFile.toStdString()).c_str();

	QString oldName = name;
	if (oldName.isEmpty())
		return;

	QString text = QTStr("ConfirmRemove.Text.title");
	PLSAlertView::Button button = PLSAlertView::Button::NoButton;
	QString showName = QString("'").append(oldName).append("'");
	if (0 == strcmp(App()->GetLocale(), "ko-KR")) {
		button = PLSMessageBox::question(sceneCollectionView, QTStr("Confirm"), showName, text, PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel);
	} else {
		button = PLSMessageBox::question(sceneCollectionView, QTStr("Confirm"), text, showName, PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel);
	}

	if (PLSAlertView::Button::Ok != button) {
		return;
	}

	sceneCollectionView->RemoveCollectionItem(name, path);
	sceneCollectionManageView->RemoveSceneCollection(name, path);

	os_unlink(oldFile.toStdString().c_str());
	os_unlink((oldFile + ".bak").toStdString().c_str());

	const char *curName = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
	const char *curFile = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");
	std::string newName;
	std::string newPath;
	if (0 == fileName.compare(curFile) || 0 == oldName.compare(curName)) {
		pls_used(curName);
		pls_used(curFile);
		QVector<PLSSceneCollectionData> datas = sceneCollectionView->GetDatas();
		pls_used(datas);

		if (!datas.isEmpty()) {
			newName = datas[0].fileName.toStdString();
			newPath = datas[0].filePath.toStdString();
		}

		/* this should never be true due to menu item being grayed out */
		if (newPath.empty())
			return;

		Load(newPath.c_str());

		ui->scenesFrame->StartRefreshThumbnailTimer();
		obs_display_add_draw_callback(ui->preview->GetDisplay(), PLSBasic::RenderMain, this);
	}

	const char *newFile = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");
	newName = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");

	sceneCollectionManageTitle->SetText(newName.c_str());
	sceneCollectionManageView->SetCurrentText(newName.c_str(), pls_get_user_path("PRISMLiveStudio/basic/scenes/").append(newFile).append(".json"));
	sceneCollectionView->SetCurrentText(newName.c_str(), pls_get_user_path("PRISMLiveStudio/basic/scenes/").append(newFile).append(".json"));

	PLS_INFO(MAINMENU_MODULE,
		 "Removed scene collection '%s' (%s.json), "
		 "switched to '%s' (%s.json)",
		 oldName.toStdString().c_str(), pls_get_path_file_name(oldFile.toStdString().c_str()), newName.c_str(), newFile);
	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");

	UpdateTitleBar();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
	}
}

void OBSBasic::ReorderSceneCollectionManageView() const
{
	QVector<PLSSceneCollectionData> datas = sceneCollectionView->GetDatas();
	sceneCollectionManageView->InitDefaultCollectionText(datas);
}

void OBSBasic::on_actionImportFromOtherSceneCollection_triggered()
{
	auto imp = pls_new<PLSImporter>(sceneCollectionView);
	if (imp->exec() != PLSDialogView::Accepted) {
		return;
	}
	QVector<PLSSceneCollectionData> importFiles = imp->GetImportFiles();
	if (importFiles.isEmpty()) {
		return;
	}
	for (const auto &importerFile : importFiles) {
		sceneCollectionView->AddSceneCollectionItem(importerFile.fileName, importerFile.filePath);
		sceneCollectionManageView->AddSceneCollection(importerFile.fileName, importerFile.filePath);
	}
	imp->ClearImportFiles();
}

void OBSBasic::on_actionChangeSceneCollection_triggered(const QString &name, const QString &path, bool textMode)
{
	sceneCollectionManageTitle->SetText(name);
	sceneCollectionManageView->SetCurrentText(name, path);
	sceneCollectionView->SetCurrentText(name, path);

	LoadSceneCollection(name, path);

	if (textMode) {
		sceneCollectionManageTitle->GetPopupMenu()->setVisible(false);
	}

	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollection", name.toUtf8().constData());
	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile", ExtractFileName(path.toStdString()).c_str());
}

QVector<QString> OBSBasic::GetSceneCollections() const
{
	QVector<QString> collections;
	if (!sceneCollectionView) {
		return collections;
	}
	QVector<PLSSceneCollectionData> datas = sceneCollectionView->GetDatas();
	for (const auto &data : datas) {
		collections.push_back(data.fileName);
	}
	return collections;
}

void OBSBasic::InitSceneCollections()
{
	const char *cur_name = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
	const char *cur_file = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");

	QDir sourceDir(pls_get_user_path("PRISMLiveStudio/basic/scenes/"));
	QStringList supportList;
	auto supportFunc = [&supportList](const QString &supportSuffix) { supportList << "*" + supportSuffix; };
	std::for_each(appSupportSuffix.begin(), appSupportSuffix.end(), supportFunc);
	QFileInfoList fileInfoList = sourceDir.entryInfoList(supportList, QDir::Files, QDir::Time);

	if (!sceneCollectionView) {
		CreateSceneCollectionView();
	}

	if (!sceneCollectionManageTitle) {
		auto function = [this]() {
			QPoint point = ui->scenesDock->mapToGlobal(ui->scenesDock->rect().topLeft());
			auto count = qMin((int)GetSceneCollections().count() + 1, 5);
			sceneCollectionManageView->Resize(count);
			sceneCollectionManageTitle->GetPopupMenu()->setFixedSize(200, count * 40 + 3);
			sceneCollectionManageTitle->GetPopupMenu()->move(point.x() + 20, point.y() + 45);
		};
		sceneCollectionManageTitle = pls_new<PLSMenuPushButton>(ui->scenesDock, true);
		sceneCollectionManageTitle->setObjectName("sceneCollectionManageTitle");
		connect(sceneCollectionManageTitle, &PLSMenuPushButton::ResizeMenu, this, function);
		connect(sceneCollectionManageTitle, &PLSMenuPushButton::PopupClicked, this, function, Qt::QueuedConnection);
	}

	if (!sceneCollectionManageView) {
		auto menu = sceneCollectionManageTitle->GetPopupMenu();
		auto actionWidget = pls_new<QWidgetAction>(menu);
		sceneCollectionManageView = pls_new<PLSSceneCollectionManagement>(menu);
		actionWidget->setDefaultWidget(sceneCollectionManageView);
		menu->addAction(actionWidget);

		connect(sceneCollectionManageView, &PLSSceneCollectionManagement::ShowSceneCollectionView, this, [this]() {
			sceneCollectionManageTitle->GetPopupMenu()->setVisible(false);
			ShowSceneCollectionView();
		});
	}

	ImportersInit();

	QVector<PLSSceneCollectionData> datas;
	EnumSceneCollections([this, &datas, cur_name, cur_file](const char *name, const char *path) { return addCollection(datas, name, path, cur_name, cur_file); });
	auto count = datas.count();

	sceneCollectionView->InitDefaultCollectionItem(datas);
	sceneCollectionManageView->InitDefaultCollectionText(datas);
	sceneCollectionManageTitle->SetText(cur_name);

	/* force saving of first scene collection on first run, otherwise
	 * no scene collections will show up */
	if (!count) {
		long prevDisableVal = disableSaving;

		disableSaving = 0;
		SaveProjectNow();
		disableSaving = prevDisableVal;

		EnumSceneCollections([this, &datas, cur_name, cur_file](const char *name, const char *path) { return addCollection(datas, name, path, cur_name, cur_file); });
	}

#if 0
	ui->actionRemoveSceneCollection->setEnabled(count > 1);
#endif

	auto main = dynamic_cast<PLSBasic *>(App()->GetMainWindow());

	main->ui->actionPasteFilters->setEnabled(false);
	main->ui->actionPasteRef->setEnabled(false);
	main->ui->actionPasteDup->setEnabled(false);
}

bool OBSBasic::addCollection(QVector<PLSSceneCollectionData> &datas, const char *name, const char *path, const char *cur_name, const char *cur_file)
{
	PLSSceneCollectionData data;
	data.fileName = name;
	data.filePath = path;

	std::string file_base = path;
	file_base = file_base.substr(file_base.find_last_of('/') + 1);
	std::string suffix = file_base.substr(file_base.find_last_of('.'));
	std::string new_file = path;
	if (0 != suffix.compare(".json")) {
		// rename to json file
		QString destName;
		QString destPath;
		if (!RenamePscToJsonFile(path, destName, destPath)) {
			return false;
		}

		data.fileName = destName;
		data.filePath = destPath;
	}
	if (0 == strcmp(name, cur_name) && 0 == strcmp(ExtractFileName(path).c_str(), cur_file)) {
		data.current = true;
	}

	datas.push_back(data);
	return true;
}

bool OBSBasic::RenamePscToJsonFile(const char *path, QString &destName, QString &destPath)
{
	// rename to json file
	std::string file_path = path;
	file_path = file_path.substr(0, file_path.find_last_of('.'));
	if (!GetClosestUnusedFileName(file_path, "json")) {
		PLS_WARN(MAIN_SCENE_COLLECTION,
			 "Failed to get "
			 "closest file name for %s",
			 pls_get_path_file_name(file_path.c_str()));
		return false;
	}

	if (!QFile::rename(path, file_path.c_str())) {
		PLS_WARN(MAIN_SCENE_COLLECTION, "Failed to rename psc file[%s] to json file.", pls_get_path_file_name(file_path.c_str()));
		return false;
	}

	QFileInfo fileInfo(file_path.c_str());
	QString newName = fileInfo.fileName();
	newName = newName.mid(0, newName.lastIndexOf('.'));

	destName = newName;
	destPath = file_path.c_str();

	OBSData scenedata = obs_data_create_from_json_file(file_path.c_str());
	obs_data_release(scenedata);

	obs_data_set_string(scenedata, "name", newName.toUtf8().constData());
	obs_data_save_json_safe(scenedata, file_path.c_str(), "tmp", "bak");
	return true;
}

bool OBSBasic::CheckPscFileInPrismUserPath(QString &pscPath)
{
	QString configPath = pls_get_user_path("PRISMLiveStudio/basic/scenes/");
	configPath.replace("\\", "/");
	if (pscPath.contains(configPath)) {
		QString destName;
		QString destPath;
		if (RenamePscToJsonFile(pscPath.toUtf8().constData(), destName, destPath)) {
			pscPath.swap(destPath);
		}
		return true;
	}
	return false;
}

void OBSBasic::LoadSceneCollection(QString name, QString filePath)
{
	if (filePath.isEmpty() || name.isEmpty())
		return;

	if (api)
		api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_SCENE_COLLECTION_ABOUT_TO_CHANGED);

	const char *oldName = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
	const char *oldFile = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");

	if (QString file = ExtractFileName(filePath.toStdString()).c_str(); name.compare(QT_UTF8(oldName)) == 0 && file.compare(QT_UTF8(oldFile)) == 0 && GetCurrentScene()) {
		return;
	}

	SaveProjectNow();
	Load(filePath.toUtf8().constData());

	ui->scenesFrame->StartRefreshThumbnailTimer();
	obs_display_add_draw_callback(ui->preview->GetDisplay(), PLSBasic::RenderMain, this);

	const char *newName = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
	const char *newFile = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");

	PLS_INFO(MAINMENU_MODULE, "Switched to scene collection '%s' (%s.json)", newName, newFile);
	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");

	UpdateTitleBar();

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
}

bool OBSBasic::CheckSceneCollectionNameAndPath(QString path, std::string &destName, std::string &destPath) const
{
	QString origName = destName.c_str();
	std::string file;
	int inc = 1;

	while (SceneCollectionExists(destName.c_str())) {
		inc = inc + 1;
		destName = (origName + QString(" (").append(QString::number(inc)).append(")")).toStdString();
	}

	if (!GetFileSafeName(destName.c_str(), file)) {
		PLS_WARN(MAINMENU_MODULE,
			 "Failed to create "
			 "safe file name for '%s'",
			 pls_get_path_file_name(destName.c_str()));
		return false;
	}

	destPath = path.append(destName.c_str()).toStdString();
	if (!GetClosestUnusedFileName(destPath, "json")) {
		PLS_WARN(MAINMENU_MODULE,
			 "Failed to get "
			 "closest file name for %s",
			 pls_get_path_file_name(file.c_str()));
		return false;
	}

	QFileInfo fileInfo(destPath.c_str());
	QString name = fileInfo.fileName();
	destName = name.mid(0, name.lastIndexOf('.')).toStdString();
	return true;
}

PLSSceneCollectionData OBSBasic::GetSceneCollectionDataWithUserLocalPath(QString userLocalPath) const
{
	if (!sceneCollectionView || userLocalPath.isEmpty()) {
		return PLSSceneCollectionData();
	}
	QVector<PLSSceneCollectionData> datas = sceneCollectionView->GetDatas();
	for (const auto &data : datas) {
		if (0 == data.userLocalPath.compare(userLocalPath)) {
			return data;
		}
	}
	return PLSSceneCollectionData();
}

static QByteArray GetHashText(QString path)
{
	QFile file(path);
	if (!file.open(QFile::ReadOnly)) {
		return QByteArray();
	}

	QCryptographicHash hashTest(QCryptographicHash::Md5);
	hashTest.reset();
	while (!file.atEnd()) {
		hashTest.addData(file.readLine());
	}
	file.close();
	return hashTest.result().toHex();
}

bool OBSBasic::CheckSameSceneCollection(QString name, QString userLocalPath) const
{
	if (!sceneCollectionView || userLocalPath.isEmpty()) {
		return false;
	}
	QVector<PLSSceneCollectionData> datas = sceneCollectionView->GetDatas();
	for (const auto &data : datas) {
		if (0 != data.fileName.compare(name)) {
			continue;
		}

		return GetHashText(data.filePath) == GetHashText(userLocalPath);
	}
	return false;
}

void OBSBasic::AddCollectionUserLocalPath(QString name, QString userLocalPath) const
{
	if (!sceneCollectionView || name.isEmpty() || userLocalPath.isEmpty()) {
		return;
	}

	sceneCollectionView->AddCollectionUserLocalPath(name, userLocalPath);
}

QString OBSBasic::GetSceneCollectionPathByName(QString name) const
{
	if (!sceneCollectionView || name.isEmpty()) {
		return QString();
	}
	QVector<PLSSceneCollectionData> datas = sceneCollectionView->GetDatas();
	for (const auto &data : datas) {
		if (0 == data.fileName.compare(name)) {
			return data.filePath;
		}
	}
	return QString();
}

bool OBSBasic::SceneCollectionExists(const char *findName) const
{
	QVector<PLSSceneCollectionData> datas = sceneCollectionView->GetDatas();
	return std::any_of(datas.begin(), datas.end(), [findName](const PLSSceneCollectionData &data) { return 0 == data.fileName.compare(findName); });
}

bool OBSBasic::GetSceneCollectionName(QWidget *parent, std::string &name, std::string &file, SceneSetOperatorType type, const char *oldName)
{
	bool rename = oldName != nullptr;
	const char *title;
	const char *text;

	switch (type) {
	case SceneSetOperatorType::AddNewSceneSet:
		title = Str("Basic.Main.AddSceneCollection.Title");
		text = Str("Basic.Main.AddSceneCollection.Text");
		break;
	case SceneSetOperatorType::CopySceneSet:
		title = Str("Basic.Main.CopySceneCollection.Title");
		text = Str("Basic.Main.AddSceneCollection.Text");
		break;
	case SceneSetOperatorType::RenameSceneSet:
		title = Str("Basic.Main.RenameSceneCollection.Title");
		text = Str("Basic.Main.AddSceneCollection.Text");
		break;
	default:
		assert(false);
		title = Str("Basic.Main.AddSceneCollection.Title");
		text = Str("Basic.Main.AddSceneCollection.Text");
		break;
	}

	for (;;) {
		bool success = PLSNameDialog::AskForName(parent, title, text, name, QT_UTF8(oldName));
		if (!success) {
			return false;
		}

		name = QString(name.c_str()).simplified().toStdString();
		if (name.empty()) {
			OBSMessageBox::warning(parent, QTStr("Alert.Title"), QTStr("NoNameEntered.Text"));
			continue;
		}
		if (SceneCollectionExists(name.c_str())) {
			OBSMessageBox::warning(parent, QTStr("Alert.Title"), QTStr("NameExists.Text"));
			continue;
		}
		break;
	}

	if (!GetUnusedSceneCollectionFile(name, file)) {
		return false;
	}

	return true;
}

void OBSBasic::on_actionImportSceneCollection_triggered_with_parent(QWidget *parent)
{
	if (lastCollectionPath.isEmpty()) {
		lastCollectionPath = QDir::homePath();
	}

	QString suffix;
	auto func = [&suffix](const QString &supportSuffix) {
		suffix += "*";
		suffix += supportSuffix;
		suffix += " ";
	};
	std::for_each(appSupportSuffix.begin(), appSupportSuffix.end(), func);

	QString fileFilter = "JSON Files (";
	fileFilter += suffix;
	fileFilter += ")";

	QString qfilePath = QFileDialog::getOpenFileName(parent, QTStr("Basic.MainMenu.SceneCollection.Import"), lastCollectionPath, fileFilter);
	if (!qfilePath.isEmpty() && !qfilePath.isNull()) {
		lastCollectionPath = qfilePath.mid(0, qfilePath.lastIndexOf("/"));
		ImportSceneCollection(parent, qfilePath, LoadSceneCollectionWay::ImportSceneCollection);
	}
}

QString OBSBasic::ImportSceneCollection(QWidget *parent, const QString &importFile, LoadSceneCollectionWay way, bool fromExport)
{
	pls::chars<512> path;
	if (int ret = GetConfigPath(path, 512, "PRISMLiveStudio/basic/scenes/"); ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get scene collection config path");
		return QString();
	}

	if (importFile.isEmpty() || importFile.isNull()) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get scene collection import file");
		return QString();
	}

	QFileInfo finfo(importFile);
	QString qfilename = finfo.fileName();
	QString qpath = QT_UTF8(path);
	QFileInfo destinfo(QT_UTF8(path) + qfilename);
	std::string absPath = QT_TO_UTF8(finfo.absoluteFilePath());

	std::string importedName;
	std::string destPath;

	// import other suffix scene collection file
	std::string programName = DetectProgram(absPath);
	importedName = GetSCName(absPath, programName).c_str();
	json11::Json res;
	int errorCode = ImportSC(absPath, importedName, res);
	QString errorString = QTStr("Scene.Collection.Import.Error");
	if (errorCode == IMPORTER_FILE_NOT_FOUND) {
		errorString = QTStr("Scene.Collection.Import.NotFound");
	}

	if (res == json11::Json()) {
		if (LoadSceneCollectionWay::RunPscWhenPrismExisted == way) {
			PLSAlertView::warning(parent, QTStr("Alert.title"), errorString);
		} else if (LoadSceneCollectionWay::RunPscWhenNoPrism == way) {
			showLoadSceneCollectionError = true;
			showLoadSceneCollectionErrorStr = errorString;
		} else if (LoadSceneCollectionWay::ImportSceneCollection == way) {
			PLSAlertView::warning(parent, QTStr("Alert.title"), errorString);
		}
		return QString();
	}
	json11::Json::object out = res.object_items();
	std::string name = res["name"].string_value();
	std::string out_str = json11::Json(out).dump();

	importedName = name;
	if (!CheckSceneCollectionNameAndPath(path.data(), importedName, destPath)) {
		return QString();
	}
	json11::Json::object newOut = out;
	newOut["name"] = importedName;
	out = newOut;

	out_str = json11::Json(newOut).dump();
	bool success = os_quick_write_utf8_file(destPath.c_str(), out_str.c_str(), out_str.size(), false);
	if (!success) {
		return QString();
	}

	// run psc from user local path
	if (way == LoadSceneCollectionWay::RunPscWhenNoPrism || way == LoadSceneCollectionWay::RunPscWhenPrismExisted) {
		sceneCollectionView->AddSceneCollectionItem(importedName.c_str(), destPath.c_str(), importFile);
	} else {
		sceneCollectionView->AddSceneCollectionItem(importedName.c_str(), destPath.c_str());
	}
	sceneCollectionManageView->AddSceneCollection(importedName.c_str(), destPath.c_str());

	if (!fromExport)
		on_actionChangeSceneCollection_triggered(importedName.c_str(), destPath.c_str(), false);

	// add scene collection list changed event
	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED);
	}

	return destPath.c_str();
}

void OBSBasic::ExportSceneCollection(const QString &name, const QString &fileName, QWidget *parent, bool import_scene, ExprotCallback callback)
{
	Q_UNUSED(name)
	SaveProjectNow();

	pls::chars<512> scenePath;
	if (int ret = GetConfigPath(scenePath.data(), scenePath.size(), "PRISMLiveStudio/basic/scenes/"); ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get scene collection config path");
		return;
	}

	if (lastCollectionPath.isEmpty()) {
		lastCollectionPath = QDir::homePath();
	}

	QString fileFilter = QString("PRISM Scene Collection (*%1)").arg(".psc");
	fileFilter += ";;";
	fileFilter += QString("JSON Files (*%1)").arg(".json");

	QString exportFile = QFileDialog::getSaveFileName(parent, QTStr("Basic.MainMenu.SceneCollection.Export"), lastCollectionPath + "/" + fileName, fileFilter);
	if (!exportFile.isEmpty() && !exportFile.isNull()) {
		OnSelectExportFile(exportFile, QT_UTF8(scenePath.data()), fileName, callback, import_scene);
	}
}

void OBSBasic::OnSelectExportFile(const QString &exportFile, const QString &path, const QString &currentFile, ExprotCallback callback, bool import_scene)
{
	pls_check_app_exiting();

	std::string file = QT_TO_UTF8(exportFile);

	if (!exportFile.isEmpty() && !exportFile.isNull()) {
		lastCollectionPath = exportFile.mid(0, exportFile.lastIndexOf("/"));
		if (QFile::exists(exportFile))
			QFile::remove(exportFile);

		QFile::copy(path + currentFile + ".json", exportFile);
	}
	if (import_scene && !exportFile.isEmpty())
		ImportSceneCollection(this, exportFile, LoadSceneCollectionWay::ImportSceneCollectionFromExport, true);

	if (callback) {
		callback();
	}
}

void OBSBasic::RunPrismByPscPath()
{
	if (pls_has_modal_view()) {
		PLS_INFO(MAINFRAME_MODULE, "There was a modal view in main window");
		return;
	}

	QString pscStr;
#ifdef Q_OS_WIN
	pscStr = PLSPrismShareMemory::instance()->tryGetMemory();
#else
	pscStr = App()->getAppRunningPath();
#endif // Q_OS_WIN
	if (pscStr.isEmpty()) {
		PLS_INFO(MAINFRAME_MODULE, "PRISM wakeup with none psc path");
		return;
	}
	PLS_INFO(MAINFRAME_MODULE, "PRISM wakeup with psc name:%s", pls_get_path_file_name(pscStr.toUtf8().constData()));

	pscStr.replace("\\", "/");
	App()->setAppRunningPath(pscStr);
	const char *cur_name = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
	const char *cur_file = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");
	loadProfile(pls_get_user_path("PRISMLiveStudio/basic/scenes/").append(cur_file).toUtf8().constData(), cur_file, LoadSceneCollectionWay::RunPscWhenPrismExisted);

	if (bool fromUserPath = CheckPscFileInPrismUserPath(pscStr); fromUserPath) {
		QString path = pls_get_user_path("PRISMLiveStudio/basic/scenes/").append(cur_file).append(".json");
		sceneCollectionView->AddSceneCollectionItem(cur_name, path);
		sceneCollectionManageView->AddSceneCollection(cur_name, path);
		sceneCollectionView->SetCurrentText(cur_name, path);
	}

	// add scene collection list changed event
	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED);
	}
}
