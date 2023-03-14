/******************************************************************************
    Copyright (C) 2015 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <obs.hpp>
#include <util/util.hpp>
#include <QVariant>
#include <QFileDialog>
#include <QStandardPaths>
#include "item-widget-helpers.hpp"
#include "window-basic-main.hpp"
#include "window-namedialog.hpp"
#include "qt-wrappers.hpp"
#include "log.h"
#include "action.h"
#include "log/module_names.h"
#include "PLSSceneDataMgr.h"
#include "PLSVirtualBackgroundDialog.h"
#include "json11.hpp"
#include "platform.hpp"

using namespace std;
using namespace json11;

void EnumSceneCollections(std::function<bool(const char *, const char *)> &&cb)
{
	char path[512];
	int ret = GetConfigPath(path, sizeof(path), "PRISMLiveStudio/basic/scenes/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get config path for scene "
					  "collections");
		return;
	}

	// order by created time
	auto orderByCreateTime = [](const QFileInfo &left, const QFileInfo &right) {
		if (left.created().toMSecsSinceEpoch() != right.created().toMSecsSinceEpoch()) {
			return left.created().toMSecsSinceEpoch() < right.created().toMSecsSinceEpoch();
		} else {
			return left.lastModified().toMSecsSinceEpoch() < right.lastModified().toMSecsSinceEpoch();
		}
	};

	QDir sourceDir(path);
	QFileInfoList fileInfoList = sourceDir.entryInfoList(QStringList() << "*.json", QDir::Files, QDir::Time);
	std::sort(fileInfoList.begin(), fileInfoList.end(), orderByCreateTime);
	for (auto iter = --fileInfoList.end(); iter != --fileInfoList.begin(); iter--) {
		auto filePath = path + (*iter).fileName();

		obs_data_t *data = obs_data_create_from_json_file_safe(filePath.toStdString().c_str(), "bak");
		std::string name = obs_data_get_string(data, "name");

		/* if no name found, use the file name as the name
		 * (this only happens when switching to the new version) */
		if (name.empty()) {
			name = strrchr(filePath.toStdString().c_str(), '/') + 1;
			name.resize(name.size() - 5);
		}

		obs_data_release(data);

		if (!cb(name.c_str(), filePath.toStdString().c_str()))
			break;
	}
}

bool PLSBasic::SceneCollectionExists(const char *findName)
{
	bool found = false;
	QList<QAction *> menuActions = ui->sceneCollectionMenu->actions();
	for (int i = 0; i < menuActions.count(); i++) {
		QMenu *menu = menuActions[i]->menu();
		if (!menu)
			continue;

		QVariant v = menu->property("name");
		if (v.typeName() == nullptr)
			continue;

		if (0 == strcmp(findName, v.toString().toStdString().c_str())) {
			found = true;
			return true;
		}
	}

	return found;
}

static bool GetUnusedSceneCollectionFile(std::string &name, std::string &file)
{
	char path[512];
	int ret;

	if (!GetFileSafeName(name.c_str(), file)) {
		PLS_WARN(MAINMENU_MODULE, "Failed to create safe file name for '%s'", name.c_str());
		return false;
	}

	ret = GetConfigPath(path, sizeof(path), "PRISMLiveStudio/basic/scenes/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get scene collection config path");
		return false;
	}

	file.insert(0, path);

	if (!GetClosestUnusedFileName(file, "json")) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get closest file name for %s", file.c_str());
		return false;
	}

	file.erase(file.size() - 5, 5);
	file.erase(0, strlen(path));
	return true;
}

bool PLSBasic::GetSceneCollectionName(QWidget *parent, std::string &name, std::string &file, const char *oldName)
{
	bool rename = oldName != nullptr;
	const char *title;
	const char *text;

	if (rename) {
		title = Str("Basic.Main.RenameSceneCollection.Title");
		text = Str("Basic.Main.AddSceneCollection.Text");
	} else {
		title = Str("Basic.Main.AddSceneCollection.Title");
		text = Str("Basic.Main.AddSceneCollection.Text");
	}

	for (;;) {
		bool success = NameDialog::AskForName(parent, title, text, name, QT_UTF8(oldName));
		if (!success) {
			return false;
		}

		name = QString(name.c_str()).simplified().toStdString();
		if (name.empty()) {
			PLSMessageBox::warning(parent, QTStr("NoNameEntered.Title"), QTStr("NoNameEntered.Text"));
			continue;
		}
		if (SceneCollectionExists(name.c_str())) {
			PLSMessageBox::warning(parent, QTStr("NameExists.Title"), QTStr("NameExists.Text"));
			continue;
		}
		break;
	}

	if (!GetUnusedSceneCollectionFile(name, file)) {
		return false;
	}

	return true;
}

bool PLSBasic::AddSceneCollection(bool create_new, const QString &qname, const QString &dupName, const QString &dupFile)
{
	std::string name;
	std::string file;

	if (qname.isEmpty()) {
		if (!GetSceneCollectionName(this, name, file))
			return false;
	} else {
		if (SceneCollectionExists(qname.toStdString().c_str()))
			return false;
		if (!GetUnusedSceneCollectionFile(name, file)) {
			return false;
		}
	}

	SaveProjectNow();

	if (!create_new) {
		PLSSceneDataMgr::Instance()->MoveSrcToDest(dupFile, QString::fromStdString(file));
		const char *curName = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
		const char *curFile = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");
		if (0 == dupFile.compare(curFile) && 0 == dupName.compare(curName)) {
			config_set_string(App()->GlobalConfig(), "Basic", "SceneCollection", name.c_str());
			config_set_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile", file.c_str());
		} else {
			QString dupPath = pls_get_user_path("PRISMLiveStudio/basic/scenes/").append(dupFile.toStdString().c_str()).append(".json");
			QString newPath = pls_get_user_path("PRISMLiveStudio/basic/scenes/").append(file.c_str()).append(".json");
			int res = os_copyfile(dupPath.toStdString().c_str(), newPath.toStdString().c_str());
			if (res == 0) {
				OBSData scenedata = obs_data_create_from_json_file(newPath.toStdString().c_str());
				obs_data_release(scenedata);
				obs_data_set_string(scenedata, "name", name.c_str());
				obs_data_save_json_safe(scenedata, newPath.toStdString().c_str(), "tmp", "bak");
			} else {
				PLS_WARN(MAIN_SCENE_COLLECTION, "CopyCollection: Failed to copy file %s to %s", GetFileName(dupPath.toStdString().c_str()).c_str(),
					 GetFileName(newPath.toStdString().c_str()).c_str());
			}
		}
		goto end;
	}

	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollection", name.c_str());
	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile", file.c_str());
	CreateDefaultScene(true);

	PLSVirtualBackgroundDialog *vb = PLSBasic::Get()->getVirtualBgDialog();
	if (vb) {
		vb->setPreviewCallback(true);
	}
	ui->scenesFrame->StartRefreshThumbnailTimer();
	obs_display_add_draw_callback(ui->preview->GetDisplay(), PLSBasic::RenderMain, this);

end:
	SaveProjectNow();
	RefreshSceneCollections();
	PLS_INFO(MAINMENU_MODULE, "Added scene collection '%s' (%s, %s.json)", name.c_str(), create_new ? "clean" : "duplicate", file.c_str());
	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");

	UpdateTitleBar();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
	}

	return true;
}

void PLSBasic::RefreshSceneCollections()
{
	QList<QAction *> menuActions = ui->sceneCollectionMenu->actions();
	int count = 0;

	for (int i = 0; i < menuActions.count(); i++) {
		QMenu *menu = menuActions[i]->menu();
		if (!menu)
			continue;

		QVariant v = menu->property("name");
		if (v.typeName() == nullptr)
			continue;

		delete menuActions[i];
	}

	const char *cur_name = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
	const char *cur_file = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");

	QDir sourceDir(pls_get_user_path("PRISMLiveStudio/basic/scenes/"));
	QFileInfoList fileInfoList = sourceDir.entryInfoList(QStringList() << "*.json", QDir::Files, QDir::Time);

	auto addCollection = [&](const char *name, const char *path) {
		std::string file = strrchr(path, '/') + 1;
		file.erase(file.size() - 5, 5);
		PLSMenu *menu = new PLSMenu(QT_UTF8(name), this);
		menu->setProperty("name", QT_UTF8(name));
		menu->menuAction()->setProperty("file_name", QT_UTF8(path));
		menu->menuAction()->setCheckable(strcmp(name, cur_name) == 0 && strcmp(file.c_str(), cur_file) == 0);
		menu->menuAction()->setChecked(strcmp(name, cur_name) == 0 && strcmp(file.c_str(), cur_file) == 0);
		connect(menu->menuAction(), &QAction::triggered, this, [=](bool checked) {
			if (!checked)
				menu->menuAction()->setChecked(menu->menuAction()->isCheckable());
		});

		// apply
		QAction *applyAction = new QAction(QTStr("Apply"));
		applyAction->setProperty("file_name", QT_UTF8(path));
		applyAction->setProperty("name", QT_UTF8(name));
		menu->addAction(applyAction);
		connect(applyAction, &QAction::triggered, this, &PLSBasic::ChangeSceneCollection);

		// rename
		QAction *renameAction = new QAction(QTStr("Rename"));
		menu->addAction(renameAction);
		renameAction->setProperty("file_name", QT_UTF8(path));
		renameAction->setProperty("name", QT_UTF8(name));
		connect(renameAction, &QAction::triggered, this, &PLSBasic::on_actionRenameSceneCollection_triggered);

		// delete
		QAction *deleteAction = new QAction(QTStr("Delete"));
		menu->addAction(deleteAction);
		deleteAction->setProperty("file_name", QT_UTF8(path));
		deleteAction->setProperty("name", QT_UTF8(name));
		deleteAction->setEnabled(fileInfoList.count() > 1);

		connect(deleteAction, &QAction::triggered, this, &PLSBasic::on_actionRemoveSceneCollection_triggered);

		// duplicate
		QAction *dupAction = new QAction(QTStr("Duplicate"));
		menu->addAction(dupAction);
		dupAction->setProperty("file_name", QT_UTF8(path));
		dupAction->setProperty("name", QT_UTF8(name));
		connect(dupAction, &QAction::triggered, this, &PLSBasic::on_actionDupSceneCollection_triggered);
		count++;
		ui->sceneCollectionMenu->addMenu(menu);
		return true;
	};

	EnumSceneCollections(addCollection);

	/* force saving of first scene collection on first run, otherwise
	 * no scene collections will show up */
	if (!count) {
		long prevDisableVal = disableSaving;

		disableSaving = 0;
		SaveProjectNow();
		disableSaving = prevDisableVal;

		EnumSceneCollections(addCollection);
	}

	//ui->actionRemoveSceneCollection->setEnabled(count > 1);

	PLSBasic *main = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());

	main->ui->actionPasteFilters->setEnabled(false);
	main->ui->actionPasteRef->setEnabled(false);
	main->ui->actionPasteDup->setEnabled(false);
}

void PLSBasic::on_actionNewSceneCollection_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Scene Collection New", ACTION_CLICK);
	AddSceneCollection(true);
}

void PLSBasic::on_actionDupSceneCollection_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Scene Collection Duplicate", ACTION_CLICK);

	QAction *action = reinterpret_cast<QAction *>(sender());
	if (!action) {
		return;
	}

	QString oldFile;
	oldFile = QT_TO_UTF8(action->property("file_name").value<QString>());
	if (oldFile.isEmpty())
		return;
	QString fileName = strrchr(oldFile.toStdString().c_str(), '/') + 1;
	fileName.resize(fileName.size() - 5);

	QString oldName;
	oldName = QT_TO_UTF8(action->property("name").value<QString>());
	if (oldName.isEmpty())
		return;
	AddSceneCollection(false, "", oldName, fileName);
}

void PLSBasic::on_actionRenameSceneCollection_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Scene Collection Rename", ACTION_CLICK);

	std::string name;
	std::string file;

	QAction *action = reinterpret_cast<QAction *>(sender());
	if (!action) {
		return;
	}

	QString oldFile;
	oldFile = QT_TO_UTF8(action->property("file_name").value<QString>());
	if (oldFile.isEmpty())
		return;
	QString fileName = strrchr(oldFile.toStdString().c_str(), '/') + 1;
	fileName.resize(fileName.size() - 5);

	QString oldName;
	oldName = QT_TO_UTF8(action->property("name").value<QString>());
	if (oldName.isEmpty())
		return;

	bool success = GetSceneCollectionName(this, name, file, oldName.toStdString().c_str());
	if (!success)
		return;

	const char *curName = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
	const char *curFile = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");
	if (0 == fileName.compare(curFile) && 0 == oldName.compare(curName)) {
		config_set_string(App()->GlobalConfig(), "Basic", "SceneCollection", name.c_str());
		config_set_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile", file.c_str());
	}
	PLSSceneDataMgr::Instance()->MoveSrcToDest(QString::fromStdString(fileName.toStdString().c_str()), QString::fromStdString(file));

	QString newPath = (file.insert(0, pls_get_user_path("PRISMLiveStudio/basic/scenes/").toStdString()) + ".json").c_str();
	int res = os_rename(oldFile.toStdString().c_str(), newPath.toStdString().c_str());
	if (0 == res) {
		OBSData scenedata = obs_data_create_from_json_file(newPath.toStdString().c_str());
		obs_data_release(scenedata);
		obs_data_set_string(scenedata, "name", name.c_str());
		obs_data_save_json_safe(scenedata, newPath.toStdString().c_str(), "tmp", "bak");
	} else {
		PLS_WARN(MAIN_SCENE_COLLECTION, "RenameCollection: Failed to rename file %s to %s", GetFileName(oldFile.toStdString().c_str()).c_str(),
			 GetFileName(newPath.toStdString().c_str()).c_str());
	}
	os_unlink(oldFile.toStdString().c_str());

	oldFile += ".bak";
	os_unlink(oldFile.toStdString().c_str());

	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");
	PLS_INFO(MAINMENU_MODULE, "Renamed scene collection to '%s' (%s.json)", name.c_str(), GetFileName(file).c_str());
	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");

	UpdateTitleBar();
	RefreshSceneCollections();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
	}
}

void PLSBasic::on_actionRemoveSceneCollection_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Scene Collection Delete", ACTION_CLICK);

	QAction *action = reinterpret_cast<QAction *>(sender());
	if (!action) {
		return;
	}

	QString oldFile;
	oldFile = QT_TO_UTF8(action->property("file_name").value<QString>());
	if (oldFile.isEmpty())
		return;
	QString fileName = strrchr(oldFile.toStdString().c_str(), '/') + 1;
	fileName.resize(fileName.size() - 5);

	QString oldName;
	oldName = QT_TO_UTF8(action->property("name").value<QString>());
	if (oldName.isEmpty())
		return;

	std::string newName;
	std::string newPath;
	auto cb = [&](const char *name, const char *filePath) {
		if (strcmp(oldName.toStdString().c_str(), name) != 0) {
			newName = name;
			newPath = filePath;
			return false;
		}

		return true;
	};

	QString text = QTStr("ConfirmRemove.Text.title");
	PLSAlertView::Button button = PLSAlertView::Button::NoButton;
	if (0 == strcmp(App()->GetLocale(), "ko-KR")) {
		button = PLSMessageBox::question(this, QTStr("ConfirmRemove.Title"), oldName, text, PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel);
	} else {
		button = PLSMessageBox::question(this, QTStr("ConfirmRemove.Title"), text, oldName, PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel);
	}

	if (PLSAlertView::Button::Ok != button) {
		return;
	}

	os_unlink(oldFile.toStdString().c_str());
	os_unlink((oldFile + ".bak").toStdString().c_str());

	const char *curName = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
	const char *curFile = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");
	if (0 != fileName.compare(curFile) || 0 != oldName.compare(curName)) {
		goto end;
	}

	EnumSceneCollections(cb);

	/* this should never be true due to menu item being grayed out */
	if (newPath.empty())
		return;

	Load(newPath.c_str());

	PLSVirtualBackgroundDialog *vb = PLSBasic::Get()->getVirtualBgDialog();
	if (vb) {
		vb->setPreviewCallback(true);
	}
	ui->scenesFrame->StartRefreshThumbnailTimer();
	obs_display_add_draw_callback(ui->preview->GetDisplay(), PLSBasic::RenderMain, this);

end:
	RefreshSceneCollections();

	const char *newFile = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");
	newName = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");

	PLS_INFO(MAINMENU_MODULE,
		 "Removed scene collection '%s' (%s.json), "
		 "switched to '%s' (%s.json)",
		 oldName.toStdString().c_str(), GetFileName(oldFile.toStdString().c_str()).c_str(), newName.c_str(), newFile);
	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");

	UpdateTitleBar();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
	}
}

void PLSBasic::ImportSceneCollection(const QString &importFile)
{
	char path[512];
	int ret = GetConfigPath(path, 512, "PRISMLiveStudio/basic/scenes/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get scene collection config path");
		return;
	}

	QFileInfo finfo(importFile);
	QString qfilename = finfo.fileName();
	QString qpath = QT_UTF8(path);
	QFileInfo destinfo(QT_UTF8(path) + qfilename);

	if (!importFile.isEmpty() && !importFile.isNull()) {
		if (!CheckSceneCollection(finfo.absoluteFilePath())) {
			PLS_INFO(MAIN_SCENE_COLLECTION, "the file[%s] imported was not scene collection file", finfo.fileName().toStdString().c_str());
			return;
		}
		string absPath = QT_TO_UTF8(finfo.absoluteFilePath());

		OBSData scenedata = obs_data_create_from_json_file(absPath.c_str());
		obs_data_release(scenedata);

		QString name = qfilename.mid(0, qfilename.lastIndexOf('.'));
		QString suffix = qfilename.mid(qfilename.lastIndexOf('.'));

		QString origName = name;
		string file;
		int inc = 1;

		while (SceneCollectionExists(name.toStdString().c_str())) {
			name = origName + " (" + QString::number(++inc) + ")";
		}

		if (!GetFileSafeName(name.toStdString().c_str(), file)) {
			PLS_WARN(MAINMENU_MODULE,
				 "Failed to create "
				 "safe file name for '%s'",
				 GetFileName(name.toStdString().c_str()).c_str());
			return;
		}

		string filePath = std::string(path) + name.toStdString();

		if (!GetClosestUnusedFileName(filePath, "json")) {
			PLS_WARN(MAINMENU_MODULE,
				 "Failed to get "
				 "closest file name for %s",
				 GetFileName(file.c_str()).c_str());
			return;
		}

		QFileInfo finfo(filePath.c_str());
		QString newName = finfo.fileName();
		name = newName.mid(0, newName.lastIndexOf('.'));
		obs_data_set_string(scenedata, "name", name.toStdString().c_str());

		obs_data_save_json_safe(scenedata, filePath.c_str(), "tmp", "bak");
		RefreshSceneCollections();

		// add scene collection list changed event
		if (api) {
			api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED);
		}
	}
}

bool PLSBasic::CheckSceneCollection(const QString &importFile)
{
	BPtr<char> file_data = os_quick_read_utf8_file(importFile.toUtf8().constData());
	string err;
	Json collection = Json::parse(file_data, err);

	if (err != "")
		return false;

	if (collection.is_null())
		return false;

	if (collection["sources"].is_null())
		return false;

	if (collection["name"].is_null())
		return false;

	if (collection["current_scene"].is_null())
		return false;

	return true;
}

void PLSBasic::on_actionImportSceneCollection_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Scene Collection Import", ACTION_CLICK);
	if (lastCollectionPath.isEmpty()) {
		lastCollectionPath = QDir::homePath();
	}
	QString qfilePath = QFileDialog::getOpenFileName(this, QTStr("Basic.MainMenu.SceneCollection.Import"), lastCollectionPath, "JSON Files (*.json)");
	if (!qfilePath.isEmpty()) {
		lastCollectionPath = qfilePath.mid(0, qfilePath.lastIndexOf("/"));
	}

	ImportSceneCollection(qfilePath);
}

QString PLSBasic::ExportSceneCollection()
{
	SaveProjectNow();

	char path[512];
	QString currentFile = QT_UTF8(config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile"));

	int ret = GetConfigPath(path, 512, "PRISMLiveStudio/basic/scenes/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get scene collection config path");
		return QString();
	}

	if (lastCollectionPath.isEmpty()) {
		lastCollectionPath = QDir::homePath();
	}
	QString exportFile = QFileDialog::getSaveFileName(this, QTStr("Basic.MainMenu.SceneCollection.Export"), lastCollectionPath + "/" + currentFile, "JSON Files (*.json)");

	string file = QT_TO_UTF8(exportFile);

	if (!exportFile.isEmpty() && !exportFile.isNull()) {
		lastCollectionPath = exportFile.mid(0, exportFile.lastIndexOf("/"));
		if (QFile::exists(exportFile))
			QFile::remove(exportFile);

		QFile::copy(path + currentFile + ".json", exportFile);
	}
	return exportFile;
}

void PLSBasic::on_actionExportSceneCollection_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Scene Collection Export", ACTION_CLICK);

	QString exportFile = ExportSceneCollection();
	if (exportFile.isEmpty()) {
		return;
	}

	ImportSceneCollection(exportFile);
}

void PLSBasic::UpdateSceneCollection(QAction *action)
{
	if (!action)
		return;
	LoadSceneCollection(action);
}

void PLSBasic::LoadSceneCollection(QAction *action)
{
	if (!action)
		return;

	std::string fileName;
	fileName = QT_TO_UTF8(action->property("file_name").value<QString>());
	if (fileName.empty())
		return;

	QString name;
	name = QT_TO_UTF8(action->property("name").value<QString>());
	if (name.isEmpty())
		return;

	if (api)
		api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_SCENE_COLLECTION_ABOUT_TO_CHANGED);

	const char *oldName = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
	const char *oldFile = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");

	QString file = strrchr(fileName.c_str(), '/') + 1;
	file.remove(file.size() - 5, 5);

	if (name.compare(QT_UTF8(oldName)) == 0 && file.compare(QT_UTF8(oldFile)) == 0) {
		action->setChecked(true);
		return;
	}

	SaveProjectNow();
	Load(fileName.c_str());
	RefreshSceneCollections();

	PLSVirtualBackgroundDialog *vb = PLSBasic::Get()->getVirtualBgDialog();
	if (vb) {
		vb->setPreviewCallback(true);
	}
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

void PLSBasic::ChangeSceneCollection()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	std::string fileName;

	if (!action)
		return;

	LoadSceneCollection(action);
}
