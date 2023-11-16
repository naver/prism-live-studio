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
#include <QMessageBox>
#include <QVariant>
#include <QFileDialog>
#include "window-basic-main.hpp"
#include "window-basic-auto-config.hpp"
#include "PLSNameDialog.hpp"
#include "qt-wrappers.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSMessageBox.h"

extern void DestroyPanelCookieManager();
extern void DuplicateCurrentCookieProfile(ConfigFile &config);
extern void CheckExistingCookieId();
extern void DeleteCookies();

static int GetProfilesCount()
{
	int profileCount = 0;
	QDir sourceDir(pls_get_user_path("PRISMLiveStudio/basic/profiles/"));
	QFileInfoList fileInfoList = sourceDir.entryInfoList(
		QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
	for (const auto &fileInfo : fileInfoList) {
		QString fileName = fileInfo.absolutePath() + "/" +
				   fileInfo.fileName() + "/basic.ini";
		ConfigFile config;
		int ret = config.Open(fileName.toStdString().c_str(),
				      CONFIG_OPEN_EXISTING);
		if (ret != CONFIG_SUCCESS)
			continue;
		profileCount++;
	}
	return profileCount;
}

template<typename CallbackFunction>
bool processProfile(bool &curExisted, const CallbackFunction &cb,
		    const std::pair<qint64, QString> &file_path)
{
	auto filePath = file_path.second;
	QString fileName = filePath + "/basic.ini";
	if (!QFile::exists(fileName)) {
		QDir dir(filePath);
		dir.removeRecursively();
		return false;
	}

	ConfigFile config;
	int open_result = config.Open(fileName.toStdString().c_str(),
				      CONFIG_OPEN_EXISTING);
	if (open_result != CONFIG_SUCCESS)
		return false;

	const char *name = config_get_string(config, "General", "Name");
	if (!name)
		name = strrchr(filePath.toStdString().c_str(), '/') + 1;

	if (!curExisted) {
		config_set_string(App()->GlobalConfig(), "Basic", "Profile",
				  name);
		config_set_string(App()->GlobalConfig(), "Basic", "ProfileDir",
				  strrchr(filePath.toUtf8().constData(), '/') +
					  1);
		curExisted = true;
	}

	if (!cb(name, filePath.toStdString().c_str()))
		return true;

	return false;
}

void EnumProfiles(std::function<bool(const char *, const char *)> &&cb)
{
	pls::chars<512> path;
	int ret = GetConfigPath(path, sizeof(path),
				"PRISMLiveStudio/basic/profiles/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get profiles config path");
		return;
	}

	const char *curDir =
		config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");

	// order by created time
	std::map<qint64, QString> filePathMap;
	QDir sourceDir(path.toString());
	bool curExisted = false;
	QFileInfoList fileInfoList = sourceDir.entryInfoList(
		QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
	for (const auto &fileInfo : fileInfoList) {
		if (0 ==
		    strcmp(fileInfo.fileName().toUtf8().constData(), curDir)) {
			curExisted = true;
		}
		filePathMap.insert(
			std::make_pair(fileInfo.birthTime().toMSecsSinceEpoch(),
				       path + fileInfo.fileName()));
	}

	for (const auto &file_path : filePathMap) {
		if (processProfile(curExisted, cb, file_path))
			break;
	}
}
#if 0
void EnumProfiles(std::function<bool(const char *, const char *)> &&cb)
{
	char path[512];
	os_glob_t *glob;

	int ret = GetConfigPath(path, sizeof(path),
				"PRISMLiveStudio/basic/profiles/*");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get profiles config path");
		return;
	}

	if (os_glob(path, 0, &glob) != 0) {
		blog(LOG_WARNING, "Failed to glob profiles");
		return;
	}

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		const char *filePath = glob->gl_pathv[i].path;
		const char *dirName = strrchr(filePath, '/') + 1;

		if (!glob->gl_pathv[i].directory)
			continue;

		if (strcmp(dirName, ".") == 0 || strcmp(dirName, "..") == 0)
			continue;

		std::string file = filePath;
		file += "/basic.ini";

		ConfigFile config;
		int ret = config.Open(file.c_str(), CONFIG_OPEN_EXISTING);
		if (ret != CONFIG_SUCCESS)
			continue;

		const char *name = config_get_string(config, "General", "Name");
		if (!name)
			name = strrchr(filePath, '/') + 1;

		if (!cb(name, filePath))
			break;
	}

	os_globfree(glob);
}
#endif
static bool GetProfileDir(const char *findName, const char *&profileDir)
{
	bool found = false;
	auto func = [&](const char *name, const char *path) {
		if (strcmp(name, findName) == 0) {
			found = true;
			profileDir = strrchr(path, '/') + 1;
			return false;
		}
		return true;
	};

	EnumProfiles(func);
	return found;
}

static bool ProfileExists(const char *findName)
{
	const char *profileDir = nullptr;
	return GetProfileDir(findName, profileDir);
}

static bool FindSafeProfileDirName(const std::string &profileName,
				   std::string &dirName)
{
	char path[512];
	int ret;

	if (ProfileExists(profileName.c_str())) {
		blog(LOG_WARNING, "Profile '%s' exists", profileName.c_str());
		return false;
	}

	if (!GetFileSafeName(profileName.c_str(), dirName)) {
		blog(LOG_WARNING, "Failed to create safe file name for '%s'",
		     profileName.c_str());
		return false;
	}

	ret = GetConfigPath(path, sizeof(path),
			    "PRISMLiveStudio/basic/profiles/");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get profiles config path");
		return false;
	}

	dirName.insert(0, path);

	if (!GetClosestUnusedFileName(dirName, nullptr)) {
		blog(LOG_WARNING, "Failed to get closest file name for %s",
		     dirName.c_str());
		return false;
	}

	dirName.erase(0, ret);
	return true;
}

static bool AskForProfileName(QWidget *parent, std::string &name,
			      std::string &file, const char *title,
			      const char *text, const bool showWizard,
			      bool &wizardChecked,
			      const char *oldName = nullptr)
{
	for (;;) {
		bool success = false;

		if (showWizard) {
			success = PLSNameDialog::AskForNameWithOption(
				parent, title, text, name,
				QTStr("AddProfile.WizardCheckbox"),
				wizardChecked, QT_UTF8(oldName));
		} else {
			success = PLSNameDialog::AskForName(
				parent, title, text, name, QT_UTF8(oldName));
		}

		if (!success) {
			return false;
		}
		if (name.empty()) {
			OBSMessageBox::warning(parent, QTStr("Alert.Title"),
					       QTStr("NoNameEntered.Text"));
			continue;
		}
		if (ProfileExists(name.c_str())) {
			OBSMessageBox::warning(parent, QTStr("Alert.Title"),
					       QTStr("NameExists.Text"));
			continue;
		}
		break;
	}

	if (!FindSafeProfileDirName(name, file))
		return false;
	return true;
}

static bool CopyProfile(const char *fromPartial, const char *to)
{
	os_glob_t *glob;
	char path[514];
	char dir[512];
	int ret;

	ret = GetConfigPath(dir, sizeof(dir),
			    "PRISMLiveStudio/basic/profiles/");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get profiles config path");
		return false;
	}

	snprintf(path, sizeof(path), "%s%s/*", dir, fromPartial);

	if (os_glob(path, 0, &glob) != 0) {
		blog(LOG_WARNING, "Failed to glob profile '%s'", fromPartial);
		return false;
	}

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		const char *filePath = glob->gl_pathv[i].path;
		if (glob->gl_pathv[i].directory)
			continue;

		ret = snprintf(path, sizeof(path), "%s/%s", to,
			       strrchr(filePath, '/') + 1);
		if (ret > 0) {
			if (os_copyfile(filePath, path) != 0) {
				blog(LOG_WARNING,
				     "CopyProfile: Failed to "
				     "copy file %s to %s",
				     filePath, path);
			}
		}
	}

	os_globfree(glob);

	return true;
}

static bool ProfileNeedsRestart(config_t *newConfig, QString &settings)
{
	OBSBasic *main = OBSBasic::Get();

	const char *oldSpeakers =
		config_get_string(main->Config(), "Audio", "ChannelSetup");
	uint oldSampleRate =
		config_get_uint(main->Config(), "Audio", "SampleRate");

	const char *newSpeakers =
		config_get_string(newConfig, "Audio", "ChannelSetup");
	uint newSampleRate = config_get_uint(newConfig, "Audio", "SampleRate");

	auto appendSetting = [&settings](const char *name) {
		settings += QStringLiteral("\n") + QTStr(name);
	};

	bool result = false;
	if (oldSpeakers != NULL && newSpeakers != NULL) {
		result = strcmp(oldSpeakers, newSpeakers) != 0;
		appendSetting("Basic.Settings.Audio.Channels");
	}
	if (oldSampleRate != 0 && newSampleRate != 0) {
		result |= oldSampleRate != newSampleRate;
		appendSetting("Basic.Settings.Audio.SampleRate");
	}

	return result;
}

bool OBSBasic::AddProfile(bool create_new, const char *title, const char *text,
			  const char *init_text, bool rename)
{
	std::string name = Str("Untitled");
	std::string dir = Str("Untitled");

	bool showWizardChecked = config_get_bool(App()->GlobalConfig(), "Basic",
						 "ConfigOnNewProfile");

	if (!AskForProfileName(this, name, dir, title, text, create_new,
			       showWizardChecked, init_text))
		return false;

	return CreateProfile(name, dir, create_new, showWizardChecked, rename);
}

bool OBSBasic::CreateProfile(const std::string &newName,
			     const std::string &newDir, bool create_new,
			     bool showWizardChecked, bool rename,
			     const char *init_text)
{
	std::string newPath;
	ConfigFile config;

	if (create_new) {
		config_set_bool(App()->GlobalConfig(), "Basic",
				"ConfigOnNewProfile", showWizardChecked);
	}

	std::string curDir =
		config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");
	if (init_text) {
		curDir = init_text;
	}
	char baseDir[512];
	int ret = GetConfigPath(baseDir, sizeof(baseDir),
				"PRISMLiveStudio/basic/profiles/");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get profiles config path");
		return false;
	}

	newPath = baseDir;
	newPath += newDir;

	if (os_mkdir(newPath.c_str()) < 0) {
		blog(LOG_WARNING, "Failed to create profile directory '%s'",
		     newDir.c_str());
		return false;
	}

	if (!create_new)
		CopyProfile(curDir.c_str(), newPath.c_str());

	newPath += "/basic.ini";

	if (config.Open(newPath.c_str(), CONFIG_OPEN_ALWAYS) != 0) {
		blog(LOG_ERROR, "Failed to open new config file '%s'",
		     newDir.c_str());
		return false;
	}

	if (api && !rename)
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_CHANGING);

	config_set_string(App()->GlobalConfig(), "Basic", "Profile",
			  newName.c_str());
	config_set_string(App()->GlobalConfig(), "Basic", "ProfileDir",
			  newDir.c_str());

	Auth::Save();
	if (create_new) {
		auth.reset();
		DestroyPanelCookieManager();
	} else if (!rename) {
		DuplicateCurrentCookieProfile(config);
	}

	config_set_string(config, "General", "Name", newName.c_str());
	basicConfig.SaveSafe("tmp");
	config.SaveSafe("tmp");
	config.Swap(basicConfig);
	InitBasicConfigDefaults();
	InitBasicConfigDefaults2();
	RefreshProfiles();

	if (create_new)
		ResetProfileData();

	blog(LOG_INFO, "Created profile '%s' (%s, %s)", newName.c_str(),
	     create_new ? "clean" : "duplicate", newDir.c_str());
	blog(LOG_INFO, "------------------------------------------------");

	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
	UpdateTitleBar();
	UpdateVolumeControlsDecayRate();

	Auth::Load();

#if 0
	// Run auto configuration setup wizard when a new profile is made to assist
	// setting up blank settings
	if (create_new && showWizardChecked) {
		AutoConfig wizard(this);
		wizard.setModal(true);
		wizard.show();
		wizard.exec();
	}
#endif

	if (api && !rename) {
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
	}
	return true;
}

bool OBSBasic::NewProfile(const QString &name)
{
	std::string dir;
	if (!FindSafeProfileDirName(name.toStdString(), dir))
		return false;

	return CreateProfile(name.toStdString(), dir, true, false, false);
}

bool OBSBasic::DuplicateProfile(const QString &name)
{
	std::string dir;
	if (!FindSafeProfileDirName(name.toStdString(), dir))
		return false;

	return CreateProfile(name.toStdString(), dir, false, false, false);
}

void OBSBasic::DeleteProfile(const char *profileName, const char *profileDir)
{
	char profilePath[512];
	char basePath[512];

	int ret = GetConfigPath(basePath, sizeof(basePath),
				"PRISMLiveStudio/basic/profiles");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get profiles config path");
		return;
	}

	ret = snprintf(profilePath, sizeof(profilePath), "%s/%s/*", basePath,
		       profileDir);
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get path for profile dir '%s'",
		     profileDir);
		return;
	}

	os_glob_t *glob;
	if (os_glob(profilePath, 0, &glob) != 0) {
		blog(LOG_WARNING, "Failed to glob profile dir '%s'",
		     profileDir);
		return;
	}

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		const char *filePath = glob->gl_pathv[i].path;

		if (glob->gl_pathv[i].directory)
			continue;

		os_unlink(filePath);
	}

	os_globfree(glob);

	ret = snprintf(profilePath, sizeof(profilePath), "%s/%s", basePath,
		       profileDir);
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get path for profile dir '%s'",
		     profileDir);
		return;
	}

	os_rmdir(profilePath);

	blog(LOG_INFO, "------------------------------------------------");
	blog(LOG_INFO, "Removed profile '%s' (%s)", profileName, profileDir);
	blog(LOG_INFO, "------------------------------------------------");
}

void OBSBasic::DeleteProfile(const QString &profileName)
{
	std::string profileDir;
	QList<QAction *> menuActions = ui->profileMenu->actions();
	for (int i = 0; i < menuActions.count(); i++) {
		QAction *action = menuActions[i];
		QVariant v = action->property("file_name");
		if (v.typeName() != nullptr && action->text() == profileName) {
			std::string dir = v.toString().toStdString();
			profileDir = strrchr(dir.c_str(), '/') + 1;
			break;
		}
	}

	DeleteProfile(profileName.toUtf8().constData(), profileDir.c_str());
	RefreshProfiles();
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED);
}

void OBSBasic::RefreshProfiles()
{
	QList<QAction *> menuActions = ui->profileMenu->actions();

	for (int i = 0, count = menuActions.count(); i < count; i++) {
		const QMenu *menu = menuActions[i]->menu();
		if (!menu)
			continue;

		QVariant v = menu->property("name");
		if (v.typeName() == nullptr)
			continue;
		pls_delete(menuActions[i]);
	}

	int count = 0;
	QDir sourceDir(pls_get_user_path("PRISMLiveStudio/basic/profiles/"));
	EnumProfiles([this, &count](const char *name, const char *path) {
		return addProfile(count, name, path);
	});
}

bool OBSBasic::addProfile(int &count, const char *name, const char *path)
{
	const char *curName =
		config_get_string(App()->GlobalConfig(), "Basic", "Profile");

	std::string file = strrchr(path, '/') + 1;
	auto menu = pls_new<QMenu>(QT_UTF8(name), this);
	menu->setProperty("name", QT_UTF8(name));
	menu->setProperty("file_name", QT_UTF8(path));
	menu->menuAction()->setProperty("file_name", QT_UTF8(path));
	menu->menuAction()->setCheckable(strcmp(name, curName) == 0);
	menu->menuAction()->setChecked(strcmp(name, curName) == 0);
	QObject::connect(
		menu->menuAction(), &QAction::triggered, this,
		[menu](bool checked) {
			if (!checked)
				menu->menuAction()->setChecked(
					menu->menuAction()->isCheckable());
		});
	// apply
	auto applyAction = pls_new<QAction>(QTStr("Apply"), menu);
	applyAction->setProperty("file_name", QT_UTF8(path));
	applyAction->setProperty("name", QT_UTF8(name));
	menu->addAction(applyAction);
	QObject::connect(applyAction, &QAction::triggered, this,
			 &OBSBasic::ChangeProfile);

	// open profile folder
	auto actionShowProfileFolder = pls_new<QAction>(
		QTStr("Basic.MainMenu.Profile.ShowProfileFolder"), menu);
	actionShowProfileFolder->setProperty("file_name", QT_UTF8(path));
	actionShowProfileFolder->setProperty("name", QT_UTF8(name));
	menu->addAction(actionShowProfileFolder);
	QObject::connect(actionShowProfileFolder, &QAction::triggered, this,
			 &OBSBasic::on_actionShowProfileFolder_triggered);

	// rename
	auto renameAction = pls_new<QAction>(QTStr("Rename"), menu);
	menu->addAction(renameAction);
	renameAction->setProperty("file_name", QT_UTF8(path));
	renameAction->setProperty("name", QT_UTF8(name));
	QObject::connect(renameAction, &QAction::triggered, this,
			 &OBSBasic::on_actionRenameProfile_triggered);

	// delete
	auto deleteAction = pls_new<QAction>(QTStr("Delete"), menu);
	menu->addAction(deleteAction);
	deleteAction->setProperty("file_name", QT_UTF8(path));
	deleteAction->setProperty("name", QT_UTF8(name));
	deleteAction->setEnabled(GetProfilesCount() > 1);

	QObject::connect(deleteAction, &QAction::triggered, this,
			 &OBSBasic::on_actionRemoveProfile_triggered);

	// duplicate
	auto dupAction = pls_new<QAction>(QTStr("Duplicate"), menu);
	menu->addAction(dupAction);
	dupAction->setProperty("file_name", QT_UTF8(path));
	dupAction->setProperty("name", QT_UTF8(name));
	QObject::connect(dupAction, &QAction::triggered, this,
			 &OBSBasic::on_actionDupProfile_triggered);
	count++;
	ui->profileMenu->addMenu(menu);
	return true;
}

bool OBSBasic::RenameProfile(const char *title, const char *text,
			     const char *old_name, const char *old_dir)
{
	std::string newName = Str("Untitled");
	std::string newDir = Str("Untitled");
	std::string newPath;
	ConfigFile config;
	bool wizardChecked = false;
	if (!AskForProfileName(this, newName, newDir, title, text, false,
			       wizardChecked, old_name))
		return false;

	std::array<char, 512> baseDir;
	int ret = GetConfigPath(baseDir.data(), baseDir.size(),
				"PRISMLiveStudio/basic/profiles/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get profiles config path");
		return false;
	}

	newPath = baseDir.data();
	newPath += newDir;
	std::string curDir = std::string(baseDir.data()) + old_dir;
	QFile::rename(curDir.c_str(), newPath.c_str());

	newPath += "/basic.ini";

	if (config.Open(newPath.c_str(), CONFIG_OPEN_ALWAYS) != 0) {
		PLS_ERROR(MAINMENU_MODULE, "Failed to open new config file.");
		return false;
	}

	Auth::Save();

	config_set_string(config, "General", "Name", newName.c_str());
	config.SaveSafe("tmp");

	const char *profile =
		config_get_string(App()->GlobalConfig(), "Basic", "Profile");
	const char *profileDir =
		config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");

	if (0 == strcmp(old_dir, profileDir) &&
	    0 == strcmp(old_name, profile)) {
		config_set_string(App()->GlobalConfig(), "Basic", "Profile",
				  newName.c_str());
		config_set_string(App()->GlobalConfig(), "Basic", "ProfileDir",
				  newDir.c_str());
		config.Swap(basicConfig);
	}

	InitBasicConfigDefaults();
	InitBasicConfigDefaults2();
	RefreshProfiles();

	PLS_INFO(MAINMENU_MODULE, "Rename profile.");
	PLS_INFO(MAINMENU_MODULE,
		 "------------------------------------------------");

	UpdateTitleBar();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
	}
	return true;
}

bool OBSBasic::ExportProfile(QString &exportDir)
{
	pls::chars<512> path;
	QString currentProfile = QString::fromUtf8(config_get_string(
		App()->GlobalConfig(), "Basic", "ProfileDir"));

	int ret = GetConfigPath(path, 512, "PRISMLiveStudio/basic/profiles/");
	if (ret <= 0) {
		PLS_WARN(MAINMENU_MODULE, "Failed to get profile config path");
		return false;
	}

	if (lastProfilePath.isEmpty()) {
		lastProfilePath = QDir::homePath();
	}
	QString dir = QFileDialog::getExistingDirectory(
		this, QTStr("Basic.MainMenu.Profile.Export"), lastProfilePath,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isEmpty() && !dir.isNull()) {
		lastProfilePath = dir;
		QString outputDir = dir;
		QString inputPath = path.toString();
		QDir folder(outputDir);

		if (!folder.exists()) {
			folder.mkpath(outputDir);
		} else {
			if (QFile::exists(outputDir + "/basic.ini"))
				QFile::remove(outputDir + "/basic.ini");

			if (QFile::exists(outputDir + "/service.json"))
				QFile::remove(outputDir + "/service.json");

			if (QFile::exists(outputDir + "/streamEncoder.json"))
				QFile::remove(outputDir +
					      "/streamEncoder.json");

			if (QFile::exists(outputDir + "/recordEncoder.json"))
				QFile::remove(outputDir +
					      "/recordEncoder.json");
		}

		QFile::copy(inputPath + currentProfile + "/basic.ini",
			    outputDir + "/basic.ini");
		QFile::copy(inputPath + currentProfile + "/streamEncoder.json",
			    outputDir + "/streamEncoder.json");
		QFile::copy(inputPath + currentProfile + "/recordEncoder.json",
			    outputDir + "/recordEncoder.json");
		QFile::copy(inputPath + currentProfile + "/service.json",
			    outputDir + "/service.json");

		// rename export profile general name
		QString iniPath = outputDir + "/basic.ini";
		ConfigFile basicConfigTemp;
		int open_result = basicConfigTemp.Open(
			iniPath.toStdString().c_str(), CONFIG_OPEN_ALWAYS);
		if (open_result != CONFIG_SUCCESS) {
			return false;
		}
		config_set_string(basicConfigTemp, "General", "Name",
				  strrchr(dir.toStdString().c_str(), '/') + 1);
		basicConfigTemp.SaveSafe("tmp");

		exportDir = dir;
		return currentProfile ==
		       strrchr(dir.toStdString().c_str(), '/') + 1;

	} else {
		return false;
	}
}

void OBSBasic::ImportProfile(const QString &importDir)
{
	QString inputPath =
		pls_get_user_path("PRISMLiveStudio/basic/profiles/");
	QFileInfo finfo(importDir);
	QString directory = finfo.fileName();
	QString profileDir = inputPath + directory;
	QDir folder(profileDir);

	if (!folder.exists() || !QFile::exists(profileDir + "/basic.ini")) {
		folder.mkpath(profileDir);
		QFile::copy(importDir + "/basic.ini",
			    profileDir + "/basic.ini");
		QFile::copy(importDir + "/streamEncoder.json",
			    profileDir + "/streamEncoder.json");
		QFile::copy(importDir + "/recordEncoder.json",
			    profileDir + "/recordEncoder.json");
		QFile::copy(importDir + "/service.json",
			    profileDir + "/service.json");
		RefreshProfiles();
	} else {
		pls_alert_error_message(
			this, QTStr("Basic.MainMenu.Profile.Import"),
			QTStr("Basic.MainMenu.Profile.Exists.Import"));
	}
}

#if 0
void OBSBasic::RefreshProfiles()
{
	QList<QAction *> menuActions = ui->profileMenu->actions();
	int count = 0;

	for (int i = 0; i < menuActions.count(); i++) {
		QVariant v = menuActions[i]->property("file_name");
		if (v.typeName() != nullptr)
			delete menuActions[i];
	}

	const char *curName =
		config_get_string(App()->GlobalConfig(), "Basic", "Profile");

	auto addProfile = [&](const char *name, const char *path) {
		std::string file = strrchr(path, '/') + 1;

		QAction *action = new QAction(QT_UTF8(name), this);
		action->setProperty("file_name", QT_UTF8(path));
		connect(action, &QAction::triggered, this,
			&OBSBasic::ChangeProfile);
		action->setCheckable(true);

		action->setChecked(strcmp(name, curName) == 0);

		ui->profileMenu->addAction(action);
		count++;
		return true;
	};

	EnumProfiles(addProfile);

	ui->actionRemoveProfile->setEnabled(count > 1);
}
#endif
void OBSBasic::ResetProfileData()
{
	ResetVideo();
	service = nullptr;
	InitService();
	ResetOutputs();
	ClearHotkeys();
	CreateHotkeys();

	/* load audio monitoring */
	if (obs_audio_monitoring_available()) {
		const char *device_name = config_get_string(
			basicConfig, "Audio", "MonitoringDeviceName");
		const char *device_id = config_get_string(basicConfig, "Audio",
							  "MonitoringDeviceId");

		obs_set_audio_monitoring_device(device_name, device_id);

		blog(LOG_INFO, "Audio monitoring device:\n\tname: %s\n\tid: %s",
		     device_name, device_id);
	}
}

void OBSBasic::on_actionNewProfile_triggered()
{
	AddProfile(true, Str("AddProfile.Title"), Str("AddProfile.Text"));
}

void OBSBasic::on_actionDupProfile_triggered()
{
	auto action = dynamic_cast<QAction *>(sender());
	if (!action)
		return;

	std::string path =
		QT_TO_UTF8(action->property("file_name").value<QString>());
	if (path.empty())
		return;
	std::string curName =
		QT_TO_UTF8(action->property("name").value<QString>());
	if (curName.empty())
		return;
	AddProfile(false, Str("AddProfile.Title"),
			   Str("AddProfile.Text"), curName.c_str());
}

void OBSBasic::on_actionRenameProfile_triggered()
{
#if 0
	std::string curDir =
		config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");
	std::string curName =
		config_get_string(App()->GlobalConfig(), "Basic", "Profile");

	/* Duplicate and delete in case there are any issues in the process */
	bool success = AddProfile(false, Str("RenameProfile.Title"),
				  Str("AddProfile.Text"), curName.c_str(),
				  true);
	if (success) {
		DeleteProfile(curName.c_str(), curDir.c_str());
		RefreshProfiles();
	}
#endif
	auto action = dynamic_cast<QAction *>(sender());
	if (!action)
		return;

	std::string path =
		QT_TO_UTF8(action->property("file_name").value<QString>());
	if (path.empty())
		return;
	std::string curDir = strrchr(path.c_str(), '/') + 1;

	std::string curName =
		QT_TO_UTF8(action->property("name").value<QString>());
	if (curName.empty())
		return;
	/* Duplicate and delete in case there are any issues in the process */
	RenameProfile(Str("RenameProfile.Title"), Str("AddProfile.Text"),
		      curName.c_str(), curDir.c_str());
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_RENAMED);
}

void OBSBasic::on_actionRemoveProfile_triggered(bool skipConfirmation)
{
	auto action = dynamic_cast<QAction *>(sender());
	if (!action)
		return;

	std::string path =
		QT_TO_UTF8(action->property("file_name").value<QString>());
	if (path.empty())
		return;
	std::string oldDir = strrchr(path.c_str(), '/') + 1;

	std::string oldName =
		QT_TO_UTF8(action->property("name").value<QString>());
	if (oldName.empty())
		return;

	std::string newName;
	std::string newPath;
	ConfigFile config;

#if 0
	std::string oldDir =
		config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");
	std::string oldName =
		config_get_string(App()->GlobalConfig(), "Basic", "Profile");
#endif
	auto cb = [&](const char *name, const char *filePath) {
		if (strcmp(oldName.c_str(), name) != 0) {
			newName = name;
			newPath = filePath;
			return false;
		}

		return true;
	};

	EnumProfiles(cb);

	if (!skipConfirmation) {
		QString text = QTStr("ConfirmRemove.Text.title");
		PLSAlertView::Button button = PLSAlertView::Button::NoButton;
		if (0 == strcmp(App()->GetLocale(), "ko-KR")) {
			button = PLSMessageBox::question(
				this, QTStr("ConfirmRemove.Title"),
				QString::fromStdString(oldName), text,
				PLSAlertView::Button::Ok |
					PLSAlertView::Button::Cancel);
		} else {
			button = PLSMessageBox::question(
				this, QTStr("ConfirmRemove.Title"), text,
				QString::fromStdString(oldName),
				PLSAlertView::Button::Ok |
					PLSAlertView::Button::Cancel);
		}

		if (PLSAlertView::Button::Ok != button) {
			return;
		}
	}

	/* this should never be true due to menu item being grayed out */
	if (newPath.empty()) {
		bool showWizardChecked = config_get_bool(
			App()->GlobalConfig(), "Basic", "ConfigOnNewProfile");
		CreateProfile(Str("Untitled"), Str("Untitled"), true,
			      showWizardChecked);
		return;
	}
	bool needsRestart = false;
	QString settingsRequiringRestart;

	const char *profile =
		config_get_string(App()->GlobalConfig(), "Basic", "Profile");
	const char *profileDir =
		config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");
	if (0 != strcmp(oldDir.c_str(), profileDir) ||
	    0 != strcmp(oldName.c_str(), profile)) {
		DeleteProfile(oldName.c_str(), oldDir.c_str());
		RefreshProfiles();
	} else {

		size_t newPath_len = newPath.size();
		newPath += "/basic.ini";

		if (config.Open(newPath.c_str(), CONFIG_OPEN_ALWAYS) != 0) {
			blog(LOG_ERROR,
			     "ChangeProfile: Failed to load file '%s'",
			     newPath.c_str());
			RefreshProfiles();
			return;
		}

		if (api)
			api->on_event(OBS_FRONTEND_EVENT_PROFILE_CHANGING);

		newPath.resize(newPath_len);

		const char *newDir = strrchr(newPath.c_str(), '/') + 1;

		config_set_string(App()->GlobalConfig(), "Basic", "Profile",
				  newName.c_str());
		config_set_string(App()->GlobalConfig(), "Basic", "ProfileDir",
				  newDir);

		needsRestart =
			ProfileNeedsRestart(config, settingsRequiringRestart);

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
	}
	blog(LOG_INFO, "Switched to profile '%s' (%s)", newName.c_str(),
	     newPath.c_str());
	blog(LOG_INFO, "------------------------------------------------");

	UpdateTitleBar();
	UpdateVolumeControlsDecayRate();

	Auth::Load();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);
	}

	if (needsRestart) {
		QMessageBox::StandardButton button = OBSMessageBox::question(
			this, QTStr("Restart"),
			QTStr("LoadProfileNeedsRestart")
				.arg(settingsRequiringRestart));

		if (button == QMessageBox::Yes) {
			GlobalVars::restart = true;
			close();
		}
	}
}

void OBSBasic::on_actionImportProfile_triggered()
{
	if (lastProfilePath.isEmpty()) {
		lastProfilePath = QDir::homePath();
	}

	QString dir = QFileDialog::getExistingDirectory(
		this, QTStr("Basic.MainMenu.Profile.Import"), lastProfilePath,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isEmpty() && !dir.isNull()) {
		lastProfilePath = dir;
		ImportProfile(dir);
	}
}

void OBSBasic::on_actionExportProfile_triggered()
{
	QString exportFile;
	bool samePath = ExportProfile(exportFile);
	if (exportFile.isEmpty()) {
		return;
	}

	if (samePath) {
		pls_alert_error_message(
			this, QTStr("Basic.MainMenu.Profile.Import"),
			QTStr("Basic.MainMenu.Profile.Exists.Import"));
		return;
	}

	QList<QAction *> menuActions = ui->profileMenu->actions();

	for (auto action : menuActions) {
		const QMenu *menu = action->menu();
		if (!menu)
			continue;

		QVariant v = menu->property("file_name");
		if (v.typeName() == nullptr)
			continue;
		QString existingName =
			strrchr(v.toString().toLower().toStdString().c_str(),
				'/') +
			1;
		QString exportName =
			strrchr(exportFile.toLower().toStdString().c_str(),
				'/') +
			1;
		if (existingName == exportName) {
			pls_alert_error_message(
				this, QTStr("Basic.MainMenu.Profile.Import"),
				QTStr("Basic.MainMenu.Profile.Exists.Import"));
			return;
		}
	}

	ImportProfile(exportFile);
}

void OBSBasic::ChangeProfile()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	ConfigFile config;
	std::string path;

	if (!action)
		return;

	path = QT_TO_UTF8(action->property("file_name").value<QString>());
	if (path.empty())
		return;

	std::string name =
		QT_TO_UTF8(action->property("name").value<QString>());
	if (name.empty())
		return;

	const char *oldName =
		config_get_string(App()->GlobalConfig(), "Basic", "Profile");
	if (name.compare(oldName) == 0) {
		action->setChecked(true);
		return;
	}

	size_t path_len = path.size();
	path += "/basic.ini";

	if (config.Open(path.c_str(), CONFIG_OPEN_ALWAYS) != 0) {
		blog(LOG_ERROR, "ChangeProfile: Failed to load file '%s'",
		     path.c_str());
		RefreshProfiles();
		return;
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_CHANGING);

	path.resize(path_len);

	const char *newName = config_get_string(config, "General", "Name");
	const char *newDir = strrchr(path.c_str(), '/') + 1;

	QString settingsRequiringRestart;
	bool needsRestart =
		ProfileNeedsRestart(config, settingsRequiringRestart);

	if (newName) {
		config_set_string(App()->GlobalConfig(), "Basic", "Profile",
				  newName);
	} else {
		config_set_string(config, "General", "Name", newDir);
		config_set_string(App()->GlobalConfig(), "Basic", "Profile",
				  newDir);
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
	UpdateVolumeControlsDecayRate();

	Auth::Load();

	CheckForSimpleModeX264Fallback();

	blog(LOG_INFO, "Switched to profile '%s' (%s)", newName, newDir);
	blog(LOG_INFO, "------------------------------------------------");

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_PROFILE_CHANGED);

	if (needsRestart) {
		QMessageBox::StandardButton button = OBSMessageBox::question(
			this, QTStr("Restart"),
			QTStr("LoadProfileNeedsRestart")
				.arg(settingsRequiringRestart));

		if (button == QMessageBox::Yes) {
			GlobalVars::restart = true;
			close();
		}
	}
}

void OBSBasic::CheckForSimpleModeX264Fallback()
{
	const char *curStreamEncoder =
		config_get_string(basicConfig, "SimpleOutput", "StreamEncoder");
	const char *curRecEncoder =
		config_get_string(basicConfig, "SimpleOutput", "RecEncoder");
	bool qsv_supported = false;
	bool qsv_av1_supported = false;
	bool amd_supported = false;
	bool nve_supported = false;
#ifdef ENABLE_HEVC
	bool amd_hevc_supported = false;
	bool nve_hevc_supported = false;
	bool apple_hevc_supported = false;
#endif
	bool amd_av1_supported = false;
	bool apple_supported = false;
	bool changed = false;
	size_t idx = 0;
	const char *id;

	while (obs_enum_encoder_types(idx++, &id)) {
		if (strcmp(id, "amd_amf_h264") == 0)
			amd_supported = true;
		else if (strcmp(id, "obs_qsv11") == 0)
			qsv_supported = true;
		else if (strcmp(id, "obs_qsv11_av1") == 0)
			qsv_av1_supported = true;
		else if (strcmp(id, "ffmpeg_nvenc") == 0)
			nve_supported = true;
#ifdef ENABLE_HEVC
		else if (strcmp(id, "h265_texture_amf") == 0)
			amd_hevc_supported = true;
		else if (strcmp(id, "ffmpeg_hevc_nvenc") == 0)
			nve_hevc_supported = true;
#endif
		else if (strcmp(id, "av1_texture_amf") == 0)
			amd_av1_supported = true;
		else if (strcmp(id,
				"com.apple.videotoolbox.videoencoder.ave.avc") ==
			 0)
			apple_supported = true;
#ifdef ENABLE_HEVC
		else if (strcmp(id,
				"com.apple.videotoolbox.videoencoder.ave.hevc") ==
			 0)
			apple_hevc_supported = true;
#endif
	}

	auto CheckEncoder = [&](const char *&name) {
		if (strcmp(name, SIMPLE_ENCODER_QSV) == 0) {
			if (!qsv_supported) {
				changed = true;
				name = SIMPLE_ENCODER_X264;
				return false;
			}
		} else if (strcmp(name, SIMPLE_ENCODER_QSV_AV1) == 0) {
			if (!qsv_av1_supported) {
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
		} else if (strcmp(name, SIMPLE_ENCODER_NVENC_AV1) == 0) {
			if (!nve_supported) {
				changed = true;
				name = SIMPLE_ENCODER_X264;
				return false;
			}
#ifdef ENABLE_HEVC
		} else if (strcmp(name, SIMPLE_ENCODER_AMD_HEVC) == 0) {
			if (!amd_hevc_supported) {
				changed = true;
				name = SIMPLE_ENCODER_X264;
				return false;
			}
		} else if (strcmp(name, SIMPLE_ENCODER_NVENC_HEVC) == 0) {
			if (!nve_hevc_supported) {
				changed = true;
				name = SIMPLE_ENCODER_X264;
				return false;
			}
#endif
		} else if (strcmp(name, SIMPLE_ENCODER_AMD) == 0) {
			if (!amd_supported) {
				changed = true;
				name = SIMPLE_ENCODER_X264;
				return false;
			}
		} else if (strcmp(name, SIMPLE_ENCODER_AMD_AV1) == 0) {
			if (!amd_av1_supported) {
				changed = true;
				name = SIMPLE_ENCODER_X264;
				return false;
			}
		} else if (strcmp(name, SIMPLE_ENCODER_APPLE_H264) == 0) {
			if (!apple_supported) {
				changed = true;
				name = SIMPLE_ENCODER_X264;
				return false;
			}
#ifdef ENABLE_HEVC
		} else if (strcmp(name, SIMPLE_ENCODER_APPLE_HEVC) == 0) {
			if (!apple_hevc_supported) {
				changed = true;
				name = SIMPLE_ENCODER_X264;
				return false;
			}
#endif
		}

		return true;
	};

	if (!CheckEncoder(curStreamEncoder))
		config_set_string(basicConfig, "SimpleOutput", "StreamEncoder",
				  curStreamEncoder);
	if (!CheckEncoder(curRecEncoder))
		config_set_string(basicConfig, "SimpleOutput", "RecEncoder",
				  curRecEncoder);
	if (changed)
		config_save_safe(basicConfig, "tmp", nullptr);
}
