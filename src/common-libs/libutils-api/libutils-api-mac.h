
#pragma once

#include <QObject>
#include <qwindowdefs.h>

struct pls_mac_ver_t {
	int major;
	int minor;
	int patch;
	std::string buildNum;
};

struct pls_mac_datetime_t {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int milliseconds;
	int timezone;
};

#define MacHandle struct MacProcessInfo *

struct MacProcessInfo {
	uint32_t pid;
	int exitCode = -1;
	void *task = NULL;
};

namespace pls_libutil_api_mac {

QString pls_get_app_executable_dir();

QString pls_get_app_resource_dir();

QString pls_get_app_plugin_dir();

QString pls_get_app_macos_dir();

QString pls_get_bundle_dir();

bool pls_is_install_app(const QString &identifier, QString &appPath);

void pls_get_install_app_list(const QString &identifier, QStringList &appList);

std::string pls_get_cpu_name();

QString pls_get_dll_dir(const QString &pluginName);

QString pls_get_mac_app_data_dir();

QString pls_get_app_content_dir();

QString pls_get_app_pn();

std::wstring pls_utf8_to_unicode(const char *utf8);

std::string pls_unicode_to_utf8(const wchar_t *unicode);

bool pls_is_process_running(const char *process_name, int &process_id);

bool pls_is_process_running(unsigned int pid);

bool pls_is_dev_environment();
bool pls_save_local_log();

uint32_t pls_current_process_id();

QStringList pls_cmdlines();

pls_mac_ver_t pls_get_mac_systerm_ver();

bool unZip(const QString &dstDirPath, const QString &srcFilePath);

bool zip(const QString &destZipPath, const QString &sourceDirName, const QString &sourceFolderPath);

bool pls_removeZipFile(const QString &dzipFile);

bool pls_copy_file(const QString &fileName, const QString &newName, bool overwrite, int &errorCode);

QString pls_get_system_identifier();

bool pls_file_is_existed(const QString &filePath);

bool install_mac_package(const QString &sourceBundlePath, const QString &destBundlePath, const std::string &prismSession, const std::string &prismGcc, const char *version);

bool pls_restart_mac_app(const QStringList &arguments);

void pls_activate_prism_as_active_app();

QString pls_get_existed_downloaded_mac_app(const QString &downloadedBundleDir, const QString &downloadedVersion, bool deleteBundleExceptUpdatedBundle);

bool isLargeVersion(const QString &v1, const QString &v2);

bool pls_remove_all_downloaded_mac_app_small_equal_version(const QString &downloadedBundleDir, const QString &softwareVersion);

void bring_mac_window_to_front(WId winId);

NSString *getNSStringFromQString(const QString &qString);

MacHandle pls_process_create(const QString &program, const QStringList &arguments, const QString &work_dir);

MacHandle pls_process_create(uint32_t process_id);

//#2650 Use NSWorkspace to create sub process, This will not inherit all environment(camera permission and others) of the parent process. like NSTask.
typedef void (*PLSMacProcessCallback)(void *inUserData, bool isSucceed, int pid);
void pls_mac_create_process_with_not_inherit(const QString &program, const QStringList &arguments, void *receiver = nullptr, PLSMacProcessCallback callback = nullptr);
bool pls_process_destroy(MacHandle handle);

bool pls_process_force_terminte(uint32_t process_id, int &exit_code);

bool pls_process_force_terminte(int &exit_code);

int pls_process_wait(MacHandle handle, int timeout);

uint32_t pls_process_id(MacHandle handle);

bool pls_process_exit_code(MacHandle handle, uint32_t *exit_code);

void addObserverProcessExit(MacHandle handle);

void removeObserverProcessExit(MacHandle handle);

QUrl build_mac_hmac_url(const QUrl &url, const QByteArray &hmacKey);

bool pls_check_mac_app_is_existed(const wchar_t *executableName);

bool pls_activiate_app();

bool pls_activiate_mac_app_except_self();

bool pls_activiate_app_bundle_id(const char *identifier);

bool pls_activiate_app_pid(int pid);

bool pls_is_app_running(const char *bundle_id);

bool pls_launch_app(const char *bundle_id, const char *app_name);

QString pls_get_current_system_language_id();

void pls_get_current_datetime(pls_mac_datetime_t &datetime);

std::string pls_get_system_version();

std::string pls_get_machine_hardware_name();

void pls_application_show_dock_icon(bool isShow);

bool pls_is_mouse_pressed_by_mac(Qt::MouseButton button);

bool pls_is_lens_has_run();

QString pls_get_app_version_by_identifier(const char *bundleID);

void pls_set_current_lens(int index);
bool pls_get_is_app_quitting_by_dock();
bool pls_open_url_mac(const QString &url);

bool pls_lens_needs_reboot();
}
