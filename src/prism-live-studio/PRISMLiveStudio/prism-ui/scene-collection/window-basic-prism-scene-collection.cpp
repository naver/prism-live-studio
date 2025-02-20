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
#include "PLSNodeManager.h"
#include "ResolutionGuidePage.h"
#include "PLSChannelDataAPI.h"
#include "PLSPlatformApi.h"

#include <QCryptographicHash>

using namespace std;

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
		obs_data_t *data = nullptr;
		try {
			data = obs_data_create_from_json_file_safe(filePath.toStdString().c_str(), "bak");
		} catch (...) {
			PLS_WARN(MAINMENU_MODULE, "get scene collection json exception : %s", filePath.toStdString().c_str());
			assert(false);
		}

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
	if (api)
		api->on_event(obs_frontend_event::OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING);

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
	if (m_startChangeSceneCollection) {
		return;
	}
	m_startChangeSceneCollection = true;
	PLS_INFO(MAIN_SCENE_COLLECTION, "Start to switch scene collection : %s", name.toStdString().c_str());
	sceneCollectionManageTitle->SetText(name);
	sceneCollectionManageView->SetCurrentText(name, path);
	sceneCollectionView->SetCurrentText(name, path);

	LoadSceneCollection(name, path);

	if (textMode) {
		sceneCollectionManageTitle->GetPopupMenu()->setVisible(false);
	}

	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollection", name.toUtf8().constData());
	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile", ExtractFileName(path.toStdString()).c_str());

	if (ui && ui->scenesFrame) {
		EnableTransitionWidgets(true);
	}
	m_startChangeSceneCollection = false;
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

	if (api) {
		api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_SCENE_COLLECTION_ABOUT_TO_CHANGED);
		api->on_event(obs_frontend_event::OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING);
	}

	const char *oldName = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
	const char *oldFile = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");

	if (QString file = ExtractFileName(filePath.toStdString()).c_str(); name.compare(QT_UTF8(oldName)) == 0 && file.compare(QT_UTF8(oldFile)) == 0 && GetCurrentScene()) {
		PLS_INFO(MAIN_SCENE_COLLECTION, "The scene set to be switched is the same and does not need to be switched.");
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
	bool supportExportTemplates = pls_prism_get_qsetting_value("SupportExportTemplates").toBool();
	if (supportExportTemplates) {
		fileFilter += ";; Overlay Files (";
		fileFilter += "*";
		fileFilter += OVERLAY_FILE;
		fileFilter += ")";
	}
	pls::HotKeyLocker locker;
	QString qfilePath = QFileDialog::getOpenFileName(parent, QTStr("Basic.MainMenu.SceneCollection.Import"), lastCollectionPath, fileFilter);
	if (!qfilePath.isEmpty() && !qfilePath.isNull()) {
		lastCollectionPath = qfilePath.mid(0, qfilePath.lastIndexOf("/"));
		if (qfilePath.endsWith(OVERLAY_FILE)) {
			importLocalSceneTemplate(qfilePath);
		} else {
			ImportSceneCollection(parent, qfilePath, LoadSceneCollectionWay::ImportSceneCollection);
		}
	}
}

QString OBSBasic::ImportSceneCollection(QWidget *parent, const QString &importFile, LoadSceneCollectionWay way, bool fromExport)
{
	pls::chars<512> path;
	if (int ret = GetConfigPath(path, 512, "PRISMLiveStudio/basic/scenes/"); ret <= 0) {
		PLS_WARN(MAINFRAME_MODULE, "Failed to get scene collection config path");
		return QString();
	}

	if (importFile.isEmpty() || importFile.isNull()) {
		PLS_WARN(MAINFRAME_MODULE, "Failed to get scene collection import file");
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

	if (way == LoadSceneCollectionWay::ImportSceneTemplates) {
		sceneCollectionView->close();
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

bool OBSBasic::importLocalSceneTemplate(const QString &overlayFile)
{
	QFileInfo info(overlayFile);
	QString templateName = info.baseName();

	QString dataPath = PLSNodeManagerPtr->getConfigDataPath();
	dataPath.append(templateName).append(QDir::separator());

	QString error;
	if (!pls::rsm::unzip(overlayFile, dataPath, false, &error)) {
		pls_alert_warning("importSceneTemplate", QString("unzip %1.overlay file error : %2").arg(templateName).arg(error).toStdString().c_str());
		return false;
	}

	QString configPath;
	auto path = pls_find_subdir_contains_spec_file(dataPath, QStringLiteral("config.json"));
	if (path) {
		configPath = path.value();
	}

	return importSceneTemplate(std::make_tuple(QString(), templateName, configPath, QString(), 0, 0), false);
}

bool OBSBasic::importSceneTemplate(const std::tuple<QString, QString, QString, QString, int, int> &model, bool checkResolution)
{
	if (get<1>(model).isEmpty() || get<2>(model).isEmpty()) {
		PLSAlertView::information(this, QTStr("Alert.Title"), QTStr("SceneTemplate.Install.Common.Error"));
		return false;
	}

	QString versionLimit = get<3>(model);
	auto trimmedStr = [&versionLimit](const char *str) {
		auto index = versionLimit.indexOf(str);
		if (index >= 0) {
			versionLimit = versionLimit.mid(index + strlen(str)).trimmed();
		}
	};

	auto checkTrimmedStr = [trimmedStr, &versionLimit]() {
		if (versionLimit.startsWith(">=")) {
			trimmedStr(">=");
		} else if (versionLimit.startsWith(">")) {
			trimmedStr(">");
		}
	};

	if (!checkSceneTemplateVersion(versionLimit)) {
		checkTrimmedStr();
		showSceneTemplateVersionLowerAlert(versionLimit);
		return false;
	}

	// check resolution when import scene template
	if (checkResolution && !checkSceneTemplateResolution(model)) {
		return false;
	}

	QString collectionPath;
	auto res = PLSNodeManagerPtr->loadConfig(get<1>(model), get<2>(model), collectionPath);
	if (res == NodeErrorType::Ok) {
		collectionPath = ImportSceneCollection(this, collectionPath, LoadSceneCollectionWay::ImportSceneTemplates);
		if (collectionPath.isEmpty()) {
			PLSAlertView::information(this, QTStr("Alert.Title"), QTStr("SceneTemplate.Install.Common.Error"));
			return false;
		}
		if (checkResolution) {
			PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"sceneTemplateId", get<0>(model).toStdString().c_str()}}, "install %s scene template success.",
				  get<1>(model).toStdString().c_str());
			PLS_PLATFORM_API->sendSceneTemplateAnalog({{"templateId", get<0>(model)}});
		}

		// create audio mixer when import scene templates
		CreateFirstRunSources();
		return true;
	}

	if (res == NodeErrorType::UnregisterNode || res == NodeErrorType::SourceNotRegistered) {
		checkTrimmedStr();
		showSceneTemplateVersionLowerAlert(versionLimit);
	} else {
		PLSAlertView::information(this, QTStr("Alert.Title"), QTStr("SceneTemplate.Install.Common.Error"));
	}
	return false;
}

bool OBSBasic::checkSceneTemplateResolution(const std::tuple<QString, QString, QString, QString, int, int> &model)
{
	uint64_t cx = config_get_uint(Config(), "Video", "BaseCX");
	uint64_t cy = config_get_uint(Config(), "Video", "BaseCY");
	uint64_t fps = config_get_uint(Config(), "Video", "FPSInt");

	float baseScale = (float)((float)cx / (float)cy);
	float TemplateScale = (float)((float)get<4>(model) / (float)get<5>(model));
	bool isEqual = qAbs(baseScale - TemplateScale) < EPSILON;
	if (isEqual) {
		return true;
	}
	QString text;
	bool isHorizontal = get<4>(model) > get<5>(model);
	if (isHorizontal) {
		text = QTStr("SceneTemplate.Horizontal.Template");
	} else {
		text = QTStr("SceneTemplate.Vertical.Template");
	}

	PLSAlertView::Button button = PLSAlertView::information(
		this, QTStr("Alert.title"), text, {{PLSAlertView::Button::Yes, QTStr("SceneTemplate.Change.resolution")}, {PLSAlertView::Button::Cancel, QTStr("SceneTemplate.Keep.resolution")}},
		PLSAlertView::Button::Yes);
	if (button == PLSAlertView::Button::Yes) {
		QString text;
		if (obs_video_active()) {
			if (PLSCHANNELS_API->isLivingOrRecording() && VirtualCamActive()) {
				text = QTStr("Resolution.InputIsActived");
			} else if (VirtualCamActive()) {
				text = QTStr("Resolution.VirtualCamIsActived");
			} else {
				text = QTStr("Resolution.InputIsActived");
			}
			OBSMessageBox::information(this, QTStr("Alert.Title"), text);
			return false;
		}

		ResolutionGuidePage::setResolution(get<4>(model), get<5>(model), fps);
	}
	return true;
}

bool OBSBasic::checkSceneTemplateVersion(QString versionLimit)
{
	if (versionLimit.isEmpty()) {
		return true;
	}
	auto plsVersion = QVersionNumber(pls_get_prism_version_major(), pls_get_prism_version_minor(), pls_get_prism_version_patch());
	auto bMatch = pls_check_version(versionLimit.toUtf8(), plsVersion);
	if (!bMatch.has_value()) {
		PLS_WARN(SCENE_TEMPLATE, "scene template item version: %s is a wrong format", qUtf8Printable(versionLimit));
		return false;
	} else {
		return bMatch.value();
	}
}

void OBSBasic::showSceneTemplateVersionLowerAlert(QString versionLimit)
{
	PLSAlertView::Button button = PLSAlertView::question(this, QTStr("Alert.Title"), QTStr("SceneTemplate.Install.Source.Not.Found.Error").arg(versionLimit),
							     {{PLSAlertView::Button::Ok, QObject::tr("Update.Bottom.Force.Button.Text")}, {PLSAlertView::Button::Cancel, QObject::tr("Cancel")}},
							     PLSAlertView::Button::Cancel);
	if (PLSAlertView::Button::Ok == button) {
		// app need update
		PLSBasic::instance()->startDownloading();
	}
}

static bool enumSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_id(source);
	PLSNodeManager::SceneItemUuidMap &uuidMap = *static_cast<PLSNodeManager::SceneItemUuidMap *>(param);

	QString uuid = pls_gen_uuid();
	uuidMap.insert((int64_t)item, uuid);

	if (obs_sceneitem_is_group(item)) {
		obs_sceneitem_group_enum_items(item, enumSceneItem, &uuidMap);
	}

	return true;
}

struct SourceUuidMap {
	PLSNodeManager::SceneItemUuidMap sceneItemUuidMap;
	PLSNodeManager::SceneUuidMap sceneUuidMap;
};

bool enum_all_scenes_callback(void *param, obs_source_t *src)
{
	SourceUuidMap &uuidMap = *static_cast<SourceUuidMap *>(param);
	obs_scene_t *scene = obs_scene_from_source(src);
	if (scene) {
		obs_source_t *source = obs_scene_get_source(scene);
		const char *name = obs_source_get_name(source);

		QString uuid = pls_gen_uuid();
		uuidMap.sceneUuidMap.insert(name, uuid);
		obs_scene_enum_items(scene, enumSceneItem, &uuidMap.sceneItemUuidMap);
	}
	return true;
}

bool OBSBasic::exportSceneTemplate(const QString &overlayFile)
{
	connect(
		PLSNodeManagerPtr, &PLSNodeManager::zipFinished, this,
		[this](bool result) { PLSAlertView::information(this, QTStr("Alert.Title"), QString("Export scene template %1").arg(result ? "success" : "failed")); }, Qt::SingleShotConnection);

	SourceUuidMap suuidMap;
	obs_enum_scenes(enum_all_scenes_callback, &suuidMap);

	PLSNodeManagerPtr->setSceneItemUuidInfo(suuidMap.sceneItemUuidMap);
	PLSNodeManagerPtr->setSceneUuidInfo(suuidMap.sceneUuidMap);

	QString templateName = overlayFile.mid(overlayFile.lastIndexOf("/") + 1);
	templateName = templateName.mid(0, templateName.lastIndexOf("."));

	QString templatePath = overlayFile.mid(0, overlayFile.lastIndexOf("/") + 1);

	OBSDataAutoRelease settings = obs_data_create();
	OBSDataAutoRelease priSettings = obs_data_create();
	QJsonObject outputObject;
	PLSNodeManagerPtr->setExportDir(templatePath);
	PLSNodeManagerPtr->setExportName(templateName);
	PLSNodeManagerPtr->exportLoadInfo(settings, priSettings, SNodeType::RootNode, outputObject);
	if (PLSNodeManagerPtr->doCopyFinished()) {
		PLSNodeManagerPtr->doZip(templateName);
	}
	return true;
}

static bool findUpdateSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto source = obs_sceneitem_get_source(item);
	if (!source) {
		return true;
	}
	bool &dst = *reinterpret_cast<bool *>(param);
	if (PLSNodeManagerPtr->checkSourceHasUpgrade(obs_source_get_id(source))) {
		dst = true;
		return false;
	}

	if (obs_sceneitem_is_group(item)) {
		obs_sceneitem_group_enum_items(item, findUpdateSceneItem, param);
	}

	return true;
};

static bool findUpdatedSource()
{
	bool sourceHasUpdated = false;

	auto cb = [](void *param, obs_source_t *src) {
		obs_scene_t *sceneSource = obs_scene_from_source(src);
		if (sceneSource) {
			obs_scene_enum_items(sceneSource, findUpdateSceneItem, param);
		}
		return true;
	};
	obs_enum_scenes(cb, &sourceHasUpdated);
	return sourceHasUpdated;
}

void OBSBasic::checkSceneTemplateSourceUpdate(obs_data_t *data)
{
	if (!data) {
		return;
	}
	if (!obs_data_has_user_value(data, FROM_SCENE_TEMPLATE)) {
		fromSceneTemplate = true;
	} else {
		fromSceneTemplate = obs_data_get_bool(data, FROM_SCENE_TEMPLATE);
	}
	if (fromSceneTemplate && findUpdatedSource()) {
		PLSBasic::instance()->setAlertParentWithBanner(
			[this](QWidget *parent) { pls_async_call(this, [parent]() { PLSAlertView::warning(parent, tr("Alert.Title"), QTStr("SceneTemplate.Install.Source.Has.Update.Tip")); }); });
	}
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

	bool supportExportTemplates = pls_prism_get_qsetting_value("SupportExportTemplates").toBool();
	if (supportExportTemplates) {
		fileFilter += ";;";
		fileFilter += QString("Overlay Files (*%1)").arg(OVERLAY_FILE);
	}

	pls::HotKeyLocker locker;
	QString exportFile = QFileDialog::getSaveFileName(parent, QTStr("Scene.Collection.Export"), lastCollectionPath + "/" + fileName, fileFilter);
	if (!exportFile.isEmpty() && !exportFile.isNull()) {
		if (exportFile.endsWith(OVERLAY_FILE)) {
			exportSceneTemplate(exportFile);
		} else {
			OnSelectExportFile(exportFile, QT_UTF8(scenePath.data()), fileName, callback, import_scene);
		}
	}
}

void OBSBasic::OnSelectExportFile(const QString &exportFile, const QString &path, const QString &currentFile, ExprotCallback callback, bool import_scene)
{
	pls_check_app_exiting();

	std::string file = QT_TO_UTF8(exportFile);

	if (!exportFile.isEmpty() && !exportFile.isNull()) {
		QString inputFile = path + currentFile + ".json";

		OBSDataAutoRelease collection = obs_data_create_from_json_file(QT_TO_UTF8(inputFile));

		OBSDataArrayAutoRelease sources = obs_data_get_array(collection, "sources");
		if (!sources) {
			blog(LOG_WARNING, "No sources in exported scene collection");
			return;
		}
		obs_data_erase(collection, "sources");

		// We're just using std::sort on a vector to make life easier.
		std::vector<OBSData> sourceItems;
		obs_data_array_enum(
			sources,
			[](obs_data_t *data, void *pVec) -> void {
				auto &sourceItems = *static_cast<std::vector<OBSData> *>(pVec);
				sourceItems.push_back(data);
			},
			&sourceItems);

		std::sort(sourceItems.begin(), sourceItems.end(), [](const OBSData &a, const OBSData &b) { return astrcmpi(obs_data_get_string(a, "name"), obs_data_get_string(b, "name")) < 0; });

		OBSDataArrayAutoRelease newSources = obs_data_array_create();
		for (auto &item : sourceItems)
			obs_data_array_push_back(newSources, item);

		obs_data_set_array(collection, "sources", newSources);
		obs_data_save_json_pretty_safe(collection, QT_TO_UTF8(exportFile), "tmp", "bak");
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
