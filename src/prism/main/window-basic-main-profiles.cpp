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
#include <util/platform.h>
#include <util/util.hpp>
#include <QVariant>
#include <QFileDialog>
#include <QDir>
#include "window-basic-main.hpp"
#include "window-namedialog.hpp"
#include "qt-wrappers.hpp"

extern void DestroyPanelCookieManager();
extern void CheckExistingCookieId();
extern void DeleteCookies();

static int GetProfilesCount()
{
	int profileCount = 0;
	QDir sourceDir(pls_get_user_path("PRISMLiveStudio/basic/profiles/"));
	QFileInfoList fileInfoList = sourceDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
	for (auto &fileInfo : fileInfoList) {
		QString fileName = fileInfo.absolutePath() + "/" + fileInfo.fileName() + "/basic.ini";
		ConfigFile config;
		int ret = config.Open(fileName.toStdString().c_str(), CONFIG_OPEN_EXISTING);
		if (ret != CONFIG_SUCCESS)
			continue;
		profileCount++;
	}
	return profileCount;
}

void EnumProfiles(std::function<bool(const char *, const char *)> &&cb)
{
	char path[512];
	int ret = GetConfigPath(path, sizeof(path), "PRISMLiveStudio/basic/profiles/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get profiles config path");
		return;
	}

	const char *curDir = config_get_string(App()->GlobalConfig(), "Basic", "Profile");

	// order by created time
	QMap<int, QString> filePathMap;
	QDir sourceDir(path);
	bool curExisted = false;
	QFileInfoList fileInfoList = sourceDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
	for (auto &fileInfo : fileInfoList) {
		if (0 == strcmp(fileInfo.fileName().toUtf8().constData(), curDir)) {
			curExisted = true;
		}
		filePathMap.insert(fileInfo.created().toMSecsSinceEpoch(), path + fileInfo.fileName());
	}

	for (auto iter = --filePathMap.end(); iter != --filePathMap.begin(); iter--) {
		auto filePath = iter.value();
		QString fileName = filePath + "/basic.ini";
		if (!QFile::exists(fileName)) {
			QDir dir(filePath);
			dir.removeRecursively();
			continue;
		}

		ConfigFile config;
		int ret = config.Open(fileName.toStdString().c_str(), CONFIG_OPEN_EXISTING);
		if (ret != CONFIG_SUCCESS)
			continue;

		const char *name = config_get_string(config, "General", "Name");
		if (!name)
			name = strrchr(filePath.toStdString().c_str(), '/') + 1;

		if (!curExisted) {
			config_set_string(App()->GlobalConfig(), "Basic", "Profile", name);
			config_set_string(App()->GlobalConfig(), "Basic", "ProfileDir", strrchr(filePath.toUtf8().constData(), '/') + 1);
			curExisted = true;
		}

		if (!cb(name, filePath.toStdString().c_str()))
			break;
	}
}

static bool ProfileExists(const char *findName)
{
	bool found = false;
	auto func = [&](const char *name, const char *) {
		if (strcmp(name, findName) == 0) {
			found = true;
			return false;
		}
		return true;
	};

	EnumProfiles(func);
	return found;
}

static bool GetProfileName(QWidget *parent, std::string &name, std::string &file, const char *title, const char *text, const char *oldName = nullptr)
{
	char path[512];
	int ret;

	for (;;) {
		bool success = NameDialog::AskForName(parent, title, text, name, QT_UTF8(oldName));
		if (!success) {
			return false;
		}
		if (name.empty()) {
			PLSMessageBox::warning(parent, QTStr("NoNameEntered.Title"), QTStr("NoNameEntered.Text"));
			continue;
		}
		if (ProfileExists(name.c_str())) {
			PLSMessageBox::warning(parent, QTStr("NameExists.Title"), QTStr("NameExists.Text"));
			continue;
		}
		break;
	}

	if (!GetFileSafeName(name.c_str(), file)) {
		PLS_WARN(MAINMENU_MODULE, "Failed to create safe profile file.");
		return false;
	}

	ret = GetConfigPath(path, sizeof(path), "PRISMLiveStudio/basic/profiles/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get profiles config path");
		return false;
	}

	file.insert(0, path);

	if (!GetClosestUnusedFileName(file, nullptr)) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get closest profile file.");
		return false;
	}

	file.erase(0, ret);
	return true;
}

static bool CopyProfile(const char *fromPartial, const char *to)
{
	os_glob_t *glob;
	char path[514];
	char dir[512];
	int ret;

	ret = GetConfigPath(dir, sizeof(dir), "PRISMLiveStudio/basic/profiles/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get profiles config path");
		return false;
	}

	snprintf(path, sizeof(path), "%s%s/*", dir, fromPartial);

	if (os_glob(path, 0, &glob) != 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to glob profile.");
		return false;
	}

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		const char *filePath = glob->gl_pathv[i].path;
		if (glob->gl_pathv[i].directory)
			continue;

		ret = snprintf(path, sizeof(path), "%s/%s", to, strrchr(filePath, '/') + 1);
		if (ret > 0) {
			if (os_copyfile(filePath, path) != 0) {
				PLS_WARN(MAINMENU_MODULE, "CopyProfile: Failed.");
			}
		}
	}

	os_globfree(glob);

	return true;
}

bool PLSBasic::AddProfile(bool create_new, const char *title, const char *text, const char *init_text, bool rename, bool addDefault)
{
	std::string newName = Str("Untitled");
	std::string newDir = Str("Untitled");

	if (!addDefault && !GetProfileName(this, newName, newDir, title, text, nullptr))
		return false;

	return AddProfileImpl(create_new, newName.c_str(), newDir.c_str(), init_text, rename, addDefault);
}

bool PLSBasic::AddProfileImpl(bool create_new, const char *name, const char *dir, const char *init_text, bool /*rename*/, bool /*addDefault*/)
{
	ConfigFile config;

	std::string curDir = config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");
	if (init_text) {
		curDir = init_text;
	}

	char baseDir[512];
	int ret = GetConfigPath(baseDir, sizeof(baseDir), "PRISMLiveStudio/basic/profiles/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get profiles config path");
		return false;
	}

	std::string newPath = baseDir;
	newPath += dir;

	QDir direc;
	if (!direc.mkpath(newPath.c_str())) {
		PLS_WARN(MAINMENU_MODULE, "Failed to create profile directory.");
		return false;
	}

	if (!create_new)
		CopyProfile(curDir.c_str(), newPath.c_str());

	newPath += "/basic.ini";

	if (config.Open(newPath.c_str(), CONFIG_OPEN_ALWAYS) != 0) {
		PLS_ERROR(MAINMENU_MODULE, "Failed to open new config file.");
		return false;
	}

	const char *profileDir = config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");
	if (0 == strcmp(curDir.c_str(), profileDir)) {
		config_set_string(App()->GlobalConfig(), "Basic", "Profile", name);
		config_set_string(App()->GlobalConfig(), "Basic", "ProfileDir", dir);
	}

	Auth::Save();
	if (create_new) {
		auth.reset();
		DestroyPanelCookieManager();
	}

	config_set_string(config, "General", "Name", name);
	basicConfig.SaveSafe("tmp");
	config.SaveSafe("tmp");
	config.Swap(basicConfig);
	InitBasicConfigDefaults();
	InitBasicConfigDefaults2();
	RefreshProfiles();

	if (create_new)
		ResetProfileData();

	PLS_INFO(MAINMENU_MODULE, "Created profile.");
	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");

	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
	UpdateTitleBar();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
	}
	return true;
}

void PLSBasic::DeleteProfile(const char *profileName, const char *profileDir)
{
	char profilePath[512];
	char basePath[512];

	int ret = GetConfigPath(basePath, 512, "PRISMLiveStudio/basic/profiles");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get profiles config path");
		return;
	}

	ret = snprintf(profilePath, 512, "%s/%s/*", basePath, profileDir);
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get path for profile dir.");
		return;
	}

	os_glob_t *glob;
	if (os_glob(profilePath, 0, &glob) != 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to glob profile dir.");
		return;
	}

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		const char *filePath = glob->gl_pathv[i].path;

		if (glob->gl_pathv[i].directory)
			continue;

		os_unlink(filePath);
	}

	os_globfree(glob);

	ret = snprintf(profilePath, 512, "%s/%s", basePath, profileDir);
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get path for profile dir.");
		return;
	}

	os_rmdir(profilePath);

	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");
	PLS_INFO(MAINMENU_MODULE, "Removed profile.");
	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");
}

bool PLSBasic::RenameProfile(const char *title, const char *text, const char *old_name, const char *old_dir)
{
	std::string newName = Str("Untitled");
	std::string newDir = Str("Untitled");
	std::string newPath;
	ConfigFile config;

	if (!GetProfileName(this, newName, newDir, title, text, old_name))
		return false;

	char baseDir[512];
	int ret = GetConfigPath(baseDir, sizeof(baseDir), "PRISMLiveStudio/basic/profiles/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get profiles config path");
		return false;
	}

	newPath = baseDir;
	newPath += newDir;
	std::string curDir = std::string(baseDir) + old_dir;
	QFile::rename(curDir.c_str(), newPath.c_str());

	newPath += "/basic.ini";

	if (config.Open(newPath.c_str(), CONFIG_OPEN_ALWAYS) != 0) {
		PLS_ERROR(MAINMENU_MODULE, "Failed to open new config file.");
		return false;
	}

	Auth::Save();

	config_set_string(config, "General", "Name", newName.c_str());
	config.SaveSafe("tmp");

	const char *profile = config_get_string(App()->GlobalConfig(), "Basic", "Profile");
	const char *profileDir = config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");

	if (0 == strcmp(old_dir, profileDir) && 0 == strcmp(old_name, profile)) {
		config_set_string(App()->GlobalConfig(), "Basic", "Profile", newName.c_str());
		config_set_string(App()->GlobalConfig(), "Basic", "ProfileDir", newDir.c_str());
		config.Swap(basicConfig);
	}

	InitBasicConfigDefaults();
	InitBasicConfigDefaults2();
	RefreshProfiles();

	PLS_INFO(MAINMENU_MODULE, "Rename profile.");
	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");

	UpdateTitleBar();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
	}
	return true;
}

void PLSBasic::RefreshProfiles()
{
	QList<QAction *> menuActions = ui->profileMenu->actions();
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

	QDir sourceDir(pls_get_user_path("PRISMLiveStudio/basic/profiles/"));
	auto addProfile = [&](const char *name, const char *path) {
		const char *curName = config_get_string(App()->GlobalConfig(), "Basic", "Profile");

		std::string file = strrchr(path, '/') + 1;
		PLSMenu *menu = new PLSMenu(QT_UTF8(name), this);
		menu->setProperty("name", QT_UTF8(name));
		menu->setProperty("file_name", QT_UTF8(path));
		menu->menuAction()->setProperty("file_name", QT_UTF8(path));
		menu->menuAction()->setCheckable(strcmp(name, curName) == 0);
		menu->menuAction()->setChecked(strcmp(name, curName) == 0);
		connect(menu->menuAction(), &QAction::triggered, this, [=](bool checked) {
			if (!checked)
				menu->menuAction()->setChecked(menu->menuAction()->isCheckable());
		});
		// apply
		QAction *applyAction = new QAction(QTStr("Apply"));
		applyAction->setProperty("file_name", QT_UTF8(path));
		applyAction->setProperty("name", QT_UTF8(name));
		menu->addAction(applyAction);
		connect(applyAction, &QAction::triggered, this, &PLSBasic::ChangeProfile);

		// open profile folder
		QAction *actionShowProfileFolder = new QAction(QTStr("Basic.MainMenu.Profile.ShowProfileFolder"));
		actionShowProfileFolder->setProperty("file_name", QT_UTF8(path));
		actionShowProfileFolder->setProperty("name", QT_UTF8(name));
		menu->addAction(actionShowProfileFolder);
		connect(actionShowProfileFolder, &QAction::triggered, this, &PLSBasic::on_actionShowProfileFolder_triggered);

		// rename
		QAction *renameAction = new QAction(QTStr("Rename"));
		menu->addAction(renameAction);
		renameAction->setProperty("file_name", QT_UTF8(path));
		renameAction->setProperty("name", QT_UTF8(name));
		connect(renameAction, &QAction::triggered, this, &PLSBasic::on_actionRenameProfile_triggered);

		// delete
		QAction *deleteAction = new QAction(QTStr("Delete"));
		menu->addAction(deleteAction);
		deleteAction->setProperty("file_name", QT_UTF8(path));
		deleteAction->setProperty("name", QT_UTF8(name));
		deleteAction->setEnabled(GetProfilesCount() > 1);

		connect(deleteAction, &QAction::triggered, this, &PLSBasic::on_actionRemoveProfile_triggered);

		// duplicate
		QAction *dupAction = new QAction(QTStr("Duplicate"));
		menu->addAction(dupAction);
		dupAction->setProperty("file_name", QT_UTF8(path));
		dupAction->setProperty("name", QT_UTF8(name));
		connect(dupAction, &QAction::triggered, this, &PLSBasic::on_actionDupProfile_triggered);
		count++;
		ui->profileMenu->addMenu(menu);
		return true;
	};

	EnumProfiles(addProfile);

	//ui->actionRemoveProfile->setEnabled(count > 1);
}

void PLSBasic::ResetProfileData()
{
	ResetVideo();
	service = nullptr;
	InitService();
	ResetOutputs();
	ClearHotkeys();
	CreateHotkeys();

	/* load audio monitoring */
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	const char *device_name = config_get_string(basicConfig, "Audio", "MonitoringDeviceName");
	const char *device_id = config_get_string(basicConfig, "Audio", "MonitoringDeviceId");

	obs_set_audio_monitoring_device(device_name, device_id);

	PLS_INFO(MAINMENU_MODULE, "Audio monitoring device:\n\tname: %s\n\tid: %s", device_name, device_id);
#endif
}

void PLSBasic::on_actionNewProfile_triggered()
{
	AddProfile(true, Str("AddProfile.Title"), Str("AddProfile.Text"));
}

void PLSBasic::on_actionDupProfile_triggered()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	if (!action)
		return;

	std::string path = QT_TO_UTF8(action->property("file_name").value<QString>());
	if (path.empty())
		return;
	std::string curDir = strrchr(path.c_str(), '/') + 1;

	AddProfile(false, Str("AddProfile.Title"), Str("AddProfile.Text"), curDir.c_str());
}

void PLSBasic::on_actionRenameProfile_triggered()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	if (!action)
		return;

	std::string path = QT_TO_UTF8(action->property("file_name").value<QString>());
	if (path.empty())
		return;
	std::string curDir = strrchr(path.c_str(), '/') + 1;

	std::string curName = QT_TO_UTF8(action->property("name").value<QString>());
	if (curName.empty())
		return;

	/* Duplicate and delete in case there are any issues in the process */
	bool success = RenameProfile(Str("RenameProfile.Title"), Str("AddProfile.Text"), curName.c_str(), curDir.c_str());
	if (success) {
		RefreshProfiles();
	}

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
	}
}

void PLSBasic::on_actionRemoveProfile_triggered()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	if (!action)
		return;

	std::string path = QT_TO_UTF8(action->property("file_name").value<QString>());
	if (path.empty())
		return;
	std::string oldDir = strrchr(path.c_str(), '/') + 1;

	std::string oldName = QT_TO_UTF8(action->property("name").value<QString>());
	if (oldName.empty())
		return;

	std::string newName;
	std::string newPath;
	ConfigFile config;

	auto cb = [&](const char *name, const char *filePath) {
		if (strcmp(oldName.c_str(), name) != 0) {
			newName = name;
			newPath = filePath;
			return false;
		}

		return true;
	};

	EnumProfiles(cb);

	QString text = QTStr("ConfirmRemove.Text.title");
	PLSAlertView::Button button = PLSAlertView::Button::NoButton;
	if (0 == strcmp(App()->GetLocale(), "ko-KR")) {
		button = PLSMessageBox::question(this, QTStr("ConfirmRemove.Title"), QString::fromStdString(oldName), text, PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel);
	} else {
		button = PLSMessageBox::question(this, QTStr("ConfirmRemove.Title"), text, QString::fromStdString(oldName), PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel);
	}

	if (PLSAlertView::Button::Ok != button) {
		return;
	}

	/* this should never be true due to menu item being grayed out */
	if (newPath.empty()) {
		AddProfileImpl(true, Str("Untitled"), Str("Untitled"));
		return;
	}

	const char *profile = config_get_string(App()->GlobalConfig(), "Basic", "Profile");
	const char *profileDir = config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");
	if (0 != strcmp(oldDir.c_str(), profileDir) || 0 != strcmp(oldName.c_str(), profile)) {
		DeleteProfile(oldName.c_str(), oldDir.c_str());
		RefreshProfiles();
		goto end;
	}

	size_t newPath_len = newPath.size();
	newPath += "/basic.ini";

	if (config.Open(newPath.c_str(), CONFIG_OPEN_ALWAYS) != 0) {
		PLS_ERROR(MAINMENU_MODULE, "ChangeProfile: Failed to load file.");
		RefreshProfiles();
		return;
	}

	newPath.resize(newPath_len);

	const char *newDir = strrchr(newPath.c_str(), '/') + 1;

	config_set_string(App()->GlobalConfig(), "Basic", "Profile", newName.c_str());
	config_set_string(App()->GlobalConfig(), "Basic", "ProfileDir", newDir);

	Auth::Save();
	auth.reset();
	DeleteCookies();
	DestroyPanelCookieManager();

	config.Swap(basicConfig);
	InitBasicConfigDefaults();
	InitBasicConfigDefaults2();
	ResetProfileData();
	DeleteProfile(oldName.c_str(), oldDir.c_str());
	RefreshProfiles();
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);

end:
	newName = config_get_string(App()->GlobalConfig(), "Basic", "Profile");
	newDir = config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");
	PLS_INFO(MAINMENU_MODULE, "Switched to profile.");
	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");

	UpdateTitleBar();

	Auth::Load();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
	}
}

void PLSBasic::ImportProfile(const QString &dir)
{
	QString inputPath = pls_get_user_path("PRISMLiveStudio/basic/profiles/");
	QFileInfo finfo(dir);
	QString directory = finfo.fileName();
	QString profileDir = inputPath + directory;
	QDir folder(profileDir);

	if (!folder.exists() || !QFile::exists(profileDir + "/basic.ini")) {
		folder.mkpath(profileDir);
		QFile::copy(dir + "/basic.ini", profileDir + "/basic.ini");
		//QFile::copy(dir + "/service.json", profileDir + "/service.json");
		QFile::copy(dir + "/streamEncoder.json", profileDir + "/streamEncoder.json");
		QFile::copy(dir + "/recordEncoder.json", profileDir + "/recordEncoder.json");
		RefreshProfiles();
	} else {
		PLSMessageBox::warning(this, QTStr("Basic.MainMenu.Profile.Import"), QTStr("Basic.MainMenu.Profile.Exists.Import"));
	}
}

void PLSBasic::on_actionImportProfile_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Profile Import", ACTION_CLICK);

	if (lastProfilePath.isEmpty()) {
		lastProfilePath = QDir::homePath();
	}

	QString dir = QFileDialog::getExistingDirectory(this, QTStr("Basic.MainMenu.Profile.Import"), lastProfilePath, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isEmpty() && !dir.isNull()) {
		lastProfilePath = dir;
		ImportProfile(dir);
	}
}

bool PLSBasic::ExportProfile(QString &exportDir)
{
	char path[512];
	QString currentProfile = QString::fromUtf8(config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir"));

	int ret = GetConfigPath(path, 512, "PRISMLiveStudio/basic/profiles/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get profile config path");
		return false;
	}

	if (lastProfilePath.isEmpty()) {
		lastProfilePath = QDir::homePath();
	}
	QString dir = QFileDialog::getExistingDirectory(this, QTStr("Basic.MainMenu.Profile.Export"), lastProfilePath, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isEmpty() && !dir.isNull()) {
		lastProfilePath = dir;
		QString outputDir = dir + "/";
		QString inputPath = QString::fromUtf8(path);
		QDir folder(outputDir);

		if (!folder.exists()) {
			folder.mkpath(outputDir);
		} else {
			if (QFile::exists(outputDir + "/basic.ini"))
				QFile::remove(outputDir + "/basic.ini");

			if (QFile::exists(outputDir + "/service.json"))
				QFile::remove(outputDir + "/service.json");

			if (QFile::exists(outputDir + "/streamEncoder.json"))
				QFile::remove(outputDir + "/streamEncoder.json");

			if (QFile::exists(outputDir + "/recordEncoder.json"))
				QFile::remove(outputDir + "/recordEncoder.json");
		}

		QFile::copy(inputPath + currentProfile + "/basic.ini", outputDir + "/basic.ini");
		//QFile::copy(inputPath + currentProfile + "/service.json", outputDir + "/service.json");
		QFile::copy(inputPath + currentProfile + "/streamEncoder.json", outputDir + "/streamEncoder.json");
		QFile::copy(inputPath + currentProfile + "/recordEncoder.json", outputDir + "/recordEncoder.json");

		// rename export profile general name
		QString iniPath = outputDir + "/basic.ini";
		ConfigFile basicConfigTemp;
		int ret = basicConfigTemp.Open(iniPath.toStdString().c_str(), CONFIG_OPEN_ALWAYS);
		if (ret != CONFIG_SUCCESS) {
			return false;
		}
		config_set_string(basicConfigTemp, "General", "Name", strrchr(dir.toStdString().c_str(), '/') + 1);
		basicConfigTemp.SaveSafe("tmp");

		exportDir = dir;
		return currentProfile == strrchr(dir.toStdString().c_str(), '/') + 1;

	} else {
		return false;
	}
}

void PLSBasic::on_actionExportProfile_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Profile Export", ACTION_CLICK);

	QString exportFile;
	bool samePath = ExportProfile(exportFile);
	if (exportFile.isEmpty()) {
		return;
	}

	if (samePath) {
		PLSMessageBox::warning(this, QTStr("Basic.MainMenu.Profile.Import"), QTStr("Basic.MainMenu.Profile.Exists.Import"));
		return;
	}

	QList<QAction *> menuActions = ui->profileMenu->actions();
	for (int i = 0; i < menuActions.count(); i++) {
		QMenu *menu = menuActions[i]->menu();
		if (!menu)
			continue;

		QVariant v = menu->property("file_name");
		if (v.typeName() == nullptr)
			continue;
		QString existingName = strrchr(v.toString().toLower().toStdString().c_str(), '/') + 1;
		QString exportName = strrchr(exportFile.toLower().toStdString().c_str(), '/') + 1;
		if (existingName == exportName) {
			PLSMessageBox::warning(this, QTStr("Basic.MainMenu.Profile.Import"), QTStr("Basic.MainMenu.Profile.Exists.Import"));
			return;
		}
	}

	ImportProfile(exportFile);
}

void PLSBasic::ChangeProfile()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	ConfigFile config;
	std::string path;

	if (!action)
		return;

	path = QT_TO_UTF8(action->property("file_name").value<QString>());
	if (path.empty())
		return;

	std::string name = QT_TO_UTF8(action->property("name").value<QString>());
	if (name.empty())
		return;

	const char *oldName = config_get_string(App()->GlobalConfig(), "Basic", "Profile");
	if (name.compare(oldName) == 0) {
		action->setChecked(true);
		return;
	}

	size_t path_len = path.size();
	path += "/basic.ini";

	if (config.Open(path.c_str(), CONFIG_OPEN_ALWAYS) != 0) {
		PLS_ERROR(MAINMENU_MODULE, "ChangeProfile: Failed to load file.");
		if (GetProfilesCount() <= 0) {
			AddProfileImpl(true, name.c_str(), name.c_str());
		}
		RefreshProfiles();
		return;
	}

	path.resize(path_len);

	const char *newName = config_get_string(config, "General", "Name");
	const char *newDir = strrchr(path.c_str(), '/') + 1;

	if (newName) {
		config_set_string(App()->GlobalConfig(), "Basic", "Profile", newName);
	} else {
		config_set_string(config, "General", "Name", newDir);
		config_set_string(App()->GlobalConfig(), "Basic", "Profile", newDir);
	}

	config_set_string(App()->GlobalConfig(), "Basic", "ProfileDir", newDir);

	Auth::Save();
	auth.reset();
	DestroyPanelCookieManager();

	config.Swap(basicConfig);
	InitBasicConfigDefaults();
	InitBasicConfigDefaults2();
	ResetProfileData();
	RefreshProfiles();
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
	UpdateTitleBar();

	Auth::Load();

	CheckForSimpleModeX264Fallback();

	PLS_INFO(MAINMENU_MODULE, "Switched to profile.");
	PLS_INFO(MAINMENU_MODULE, "------------------------------------------------");

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
}

void PLSBasic::CheckForSimpleModeX264Fallback()
{
	const char *curStreamEncoder = config_get_string(basicConfig, "SimpleOutput", "StreamEncoder");
	const char *curRecEncoder = config_get_string(basicConfig, "SimpleOutput", "RecEncoder");
	bool qsv_supported = false;
	bool amd_supported = false;
	bool nve_supported = false;
	bool changed = false;
	size_t idx = 0;
	const char *id;

	while (obs_enum_encoder_types(idx++, &id)) {
		if (strcmp(id, "amd_amf_h264") == 0)
			amd_supported = true;
		else if (strcmp(id, "obs_qsv11") == 0)
			qsv_supported = true;
		else if (strcmp(id, "ffmpeg_nvenc") == 0)
			nve_supported = true;
	}

	auto CheckEncoder = [&](const char *&name) {
		if (strcmp(name, SIMPLE_ENCODER_QSV) == 0) {
			if (!qsv_supported) {
				changed = true;
				name = SIMPLE_ENCODER_X264;
				return false;
			}
		} else if (strcmp(name, SIMPLE_ENCODER_NVENC) == 0) {
			if (!nve_supported) {
				changed = true;
				name = SIMPLE_ENCODER_X264;
				return false;
			}
		} else if (strcmp(name, SIMPLE_ENCODER_AMD) == 0) {
			if (!amd_supported) {
				changed = true;
				name = SIMPLE_ENCODER_X264;
				return false;
			}
		}

		return true;
	};

	if (!CheckEncoder(curStreamEncoder))
		config_set_string(basicConfig, "SimpleOutput", "StreamEncoder", curStreamEncoder);
	if (!CheckEncoder(curRecEncoder))
		config_set_string(basicConfig, "SimpleOutput", "RecEncoder", curRecEncoder);
	if (changed)
		config_save_safe(basicConfig, "tmp", nullptr);
}
