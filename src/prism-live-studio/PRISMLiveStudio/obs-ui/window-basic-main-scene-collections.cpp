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
#include <QMessageBox>
#include <QVariant>
#include <QFileDialog>
#include <QStandardPaths>
#include "item-widget-helpers.hpp"
#include "window-basic-main.hpp"
#include "window-importer.hpp"
#include "PLSNameDialog.hpp"
#include "qt-wrappers.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "libutils-api.h"
#include "PLSSceneDataMgr.h"

using namespace std;

void EnumSceneCollections(std::function<bool(const char *, const char *)> &&cb)
{
	char path[512];
	os_glob_t *glob;

	int ret = GetConfigPath(path, sizeof(path),
				"PRISMLiveStudio/basic/scenes/*.json");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get config path for scene "
				  "collections");
		return;
	}

	if (os_glob(path, 0, &glob) != 0) {
		blog(LOG_WARNING, "Failed to glob scene collections");
		return;
	}

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		const char *filePath = glob->gl_pathv[i].path;

		if (glob->gl_pathv[i].directory)
			continue;

		OBSDataAutoRelease data =
			obs_data_create_from_json_file_safe(filePath, "bak");
		std::string name = obs_data_get_string(data, "name");

		/* if no name found, use the file name as the name
		 * (this only happens when switching to the new version) */
		if (name.empty()) {
			name = strrchr(filePath, '/') + 1;
			name.resize(name.size() - 5);
		}

		if (!cb(name.c_str(), filePath))
			break;
	}

	os_globfree(glob);
}

std::string OBSBasic::ExtractFileName(const std::string &filePath) const
{
	if (filePath.empty()) {
		return string();
	}
	std::string file_base = filePath;
	file_base = file_base.substr(file_base.find_last_of('/') + 1);
	std::string suffix = file_base.substr(file_base.find_last_of('.'));
	file_base.erase(file_base.size() - strlen(suffix.c_str()),
			strlen(suffix.c_str()));
	return file_base;
}

#if 0
bool SceneCollectionExists(const char *findName)
{
	bool found = false;
	auto func = [&](const char *name, const char *) {
		if (strcmp(name, findName) == 0) {
			found = true;
			return false;
		}

		return true;
	};

	EnumSceneCollections(func);
	return found;
}
#endif

void duplicateScene(bool &duplicateCurrentFlag, const std::string &name,
		    const std::string &file, const QString &dupName,
		    const QString &dupFile)
{
	PLSSceneDataMgr::Instance()->MoveSrcToDest(
		dupFile, QString::fromStdString(file));
	const char *curName = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollection");
	const char *curFile = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollectionFile");
	if (0 == dupFile.compare(curFile) && 0 == dupName.compare(curName)) {
		config_set_string(App()->GlobalConfig(), "Basic",
				  "SceneCollection", name.c_str());
		config_set_string(App()->GlobalConfig(), "Basic",
				  "SceneCollectionFile", file.c_str());
		duplicateCurrentFlag = true;
	} else {
		QString dupPath =
			pls_get_user_path("PRISMLiveStudio/basic/scenes/")
				.append(dupFile.toStdString().c_str())
				.append(".json");
		QString newPath =
			pls_get_user_path("PRISMLiveStudio/basic/scenes/")
				.append(file.c_str())
				.append(".json");
		int res = os_copyfile(dupPath.toStdString().c_str(),
				      newPath.toStdString().c_str());
		if (res == 0) {
			OBSData scenedata = obs_data_create_from_json_file(
				newPath.toStdString().c_str());
			obs_data_release(scenedata);
			obs_data_set_string(scenedata, "name", name.c_str());
			obs_data_save_json_safe(scenedata,
						newPath.toStdString().c_str(),
						"tmp", "bak");
		} else {
			PLS_WARN(MAIN_SCENE_COLLECTION,
				 "CopyCollection: Failed to copy file %s to %s",
				 pls_get_path_file_name(
					 dupPath.toStdString().c_str()),
				 pls_get_path_file_name(
					 newPath.toStdString().c_str()));
		}
	}
}

bool OBSBasic::AddSceneCollection(bool create_new, QWidget *parent,
				  const QString &qname, const QString &dupName,
				  const QString &dupFile)
{
	std::string name;
	std::string file;

	if (qname.isEmpty() || !create_new) {
		auto type = qname.isEmpty()
				    ? SceneSetOperatorType::AddNewSceneSet
				    : SceneSetOperatorType::CopySceneSet;
		if (!GetSceneCollectionName(parent, name, file, type,
					    qname.toUtf8().constData()))
			return false;
	} else {
		name = QT_TO_UTF8(qname);
		if (SceneCollectionExists(name.c_str()))
			return false;

		if (!GetUnusedSceneCollectionFile(name, file)) {
			return false;
		}
	}

	SaveProjectNow();

	auto new_collection = [this, create_new, dupName,
			       dupFile](const std::string &file,
					const std::string &name) {
		bool duplicateCurrent = false;
		if (!create_new) {
			duplicateScene(duplicateCurrent, name, file, dupName,
				       dupFile);
		} else {
			config_set_string(App()->GlobalConfig(), "Basic",
					  "SceneCollection", name.c_str());
			config_set_string(App()->GlobalConfig(), "Basic",
					  "SceneCollectionFile", file.c_str());
			CreateDefaultScene(true);
			ui->scenesFrame->StartRefreshThumbnailTimer();
			obs_display_add_draw_callback(ui->preview->GetDisplay(),
						      OBSBasic::RenderMain,
						      this);
		}

		SaveProjectNow();
		QString path =
			pls_get_user_path("PRISMLiveStudio/basic/scenes/")
				.append(file.c_str())
				.append(".json");
		sceneCollectionView->AddSceneCollectionItem(name.c_str(), path);
		sceneCollectionManageView->AddSceneCollection(name.c_str(),
							      path);
		if (create_new || duplicateCurrent) {
			sceneCollectionManageView->SetCurrentText(name.c_str(),
								  path);
			sceneCollectionView->SetCurrentText(name.c_str(), path);
			sceneCollectionManageTitle->SetText(name.c_str());
		}

		//RefreshSceneCollections();
	};

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING);

	new_collection(file, name);

	blog(LOG_INFO, "Added scene collection '%s' (%s, %s.json)",
	     name.c_str(), create_new ? "clean" : "duplicate", file.c_str());
	blog(LOG_INFO, "------------------------------------------------");

	UpdateTitleBar();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
	}

	return true;
}

void OBSBasic::RefreshSceneCollections()
{
	QList<QAction *> menuActions = ui->sceneCollectionMenu->actions();
	int count = 0;

	for (int i = 0; i < menuActions.count(); i++) {
		QVariant v = menuActions[i]->property("file_name");
		if (v.typeName() != nullptr)
			delete menuActions[i];
	}

	const char *cur_name = config_get_string(App()->GlobalConfig(), "Basic",
						 "SceneCollection");

	auto addCollection = [&](const char *name, const char *path) {
		std::string file = strrchr(path, '/') + 1;
		file.erase(file.size() - 5, 5);

		QAction *action = new QAction(QT_UTF8(name), this);
		action->setProperty("file_name", QT_UTF8(path));
		connect(action, &QAction::triggered, this,
			&OBSBasic::ChangeSceneCollection);
		action->setCheckable(true);

		action->setChecked(strcmp(name, cur_name) == 0);

		ui->sceneCollectionMenu->addAction(action);
		count++;
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

	ui->actionRemoveSceneCollection->setEnabled(count > 1);

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	main->ui->actionPasteFilters->setEnabled(false);
	main->ui->actionPasteRef->setEnabled(false);
	main->ui->actionPasteDup->setEnabled(false);
}

void OBSBasic::on_actionNewSceneCollection_triggered()
{
	on_actionNewSceneCollection_triggered_with_parent(this);
}

void OBSBasic::on_actionNewSceneCollection_triggered_with_parent(QWidget *parent)
{
	AddSceneCollection(true, parent);
}

void OBSBasic::on_actionDupSceneCollection_triggered(const QString &name,
						     const QString &path)
{
	QString oldFile = path;
	if (oldFile.isEmpty())
		return;
	QString fileName = ExtractFileName(oldFile.toStdString()).c_str();

	QString oldName = name;
	if (oldName.isEmpty())
		return;
	AddSceneCollection(false, sceneCollectionView, name, oldName, fileName);
}

void OBSBasic::on_actionRenameSceneCollection_triggered()
{
#if 0
	std::string name;
	std::string file;
	std::string oname;

	std::string oldFile = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollectionFile");
	const char *oldName = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollection");
	oname = std::string(oldName);

	bool success = GetSceneCollectionName(this, name, file, oldName);
	if (!success)
		return;

	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollection",
			  name.c_str());
	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile",
			  file.c_str());
	SaveProjectNow();

	char path[512];
	int ret = GetConfigPath(path, 512, "PRISMLiveStudio/basic/scenes/");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get scene collection config path");
		return;
	}

	oldFile.insert(0, path);
	oldFile += ".json";
	os_unlink(oldFile.c_str());
	oldFile += ".bak";
	os_unlink(oldFile.c_str());

	blog(LOG_INFO, "------------------------------------------------");
	blog(LOG_INFO, "Renamed scene collection to '%s' (%s.json)",
	     name.c_str(), file.c_str());
	blog(LOG_INFO, "------------------------------------------------");

	UpdateTitleBar();
	RefreshSceneCollections();

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_RENAMED);
#endif
}

void OBSBasic::on_actionRemoveSceneCollection_triggered()
{
#if 0
	std::string newName;
	std::string newPath;

	std::string oldFile = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollectionFile");
	std::string oldName = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollection");

	auto cb = [&](const char *name, const char *filePath) {
		if (strcmp(oldName.c_str(), name) != 0) {
			newName = name;
			newPath = filePath;
			return false;
		}

		return true;
	};

	EnumSceneCollections(cb);

	/* this should never be true due to menu item being grayed out */
	if (newPath.empty())
		return;

	QString text =
		QTStr("ConfirmRemove.Text").arg(QT_UTF8(oldName.c_str()));

	QMessageBox::StandardButton button = OBSMessageBox::question(
		this, QTStr("ConfirmRemove.Title"), text);
	if (button == QMessageBox::No)
		return;

	char path[512];
	int ret = GetConfigPath(path, 512, "PRISMLiveStudio/basic/scenes/");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get scene collection config path");
		return;
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING);

	oldFile.insert(0, path);
	oldFile += ".json";

	os_unlink(oldFile.c_str());
	oldFile += ".bak";
	os_unlink(oldFile.c_str());

	Load(newPath.c_str());
	RefreshSceneCollections();

	const char *newFile = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollectionFile");

	blog(LOG_INFO,
	     "Removed scene collection '%s' (%s.json), "
	     "switched to '%s' (%s.json)",
	     oldName.c_str(), oldFile.c_str(), newName.c_str(), newFile);
	blog(LOG_INFO, "------------------------------------------------");

	UpdateTitleBar();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
	}
#endif
}

void OBSBasic::on_actionImportSceneCollection_triggered()
{
	on_actionImportSceneCollection_triggered_with_parent(this);
}

void OBSBasic::on_actionExportSceneCollection_triggered()
{
	const char *name = config_get_string(App()->GlobalConfig(), "Basic",
					     "SceneCollection");
	const char *file = config_get_string(App()->GlobalConfig(), "Basic",
					     "SceneCollectionFile");

	on_actionExportSceneCollection_triggered_with_path(name, file, this);
}

void OBSBasic::SetCurrentSceneCollection(const QString &name)
{
	QVector<PLSSceneCollectionData> datas = sceneCollectionView->GetDatas();
	for (int i = 0; i < datas.count(); i++) {
		PLSSceneCollectionData data = datas[i];
		if (0 == data.fileName.compare(name)) {
			on_actionChangeSceneCollection_triggered(
				name, data.filePath, false);
			break;
		}
	}
}

void OBSBasic::on_actionExportSceneCollection_triggered_with_path(
	const QString &name, const QString &fileName, QWidget *parent)
{
	ExportSceneCollection(name, fileName, parent, true);
}

void OBSBasic::ChangeSceneCollection()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	std::string fileName;

	if (!action)
		return;

	fileName = QT_TO_UTF8(action->property("file_name").value<QString>());
	if (fileName.empty())
		return;

	const char *oldName = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollection");

	if (action->text().compare(QT_UTF8(oldName)) == 0) {
		action->setChecked(true);
		return;
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING);

	SaveProjectNow();

	Load(fileName.c_str());
	RefreshSceneCollections();

	const char *newName = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollection");
	const char *newFile = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollectionFile");

	blog(LOG_INFO, "Switched to scene collection '%s' (%s.json)", newName,
	     newFile);
	blog(LOG_INFO, "------------------------------------------------");

	UpdateTitleBar();

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
}
