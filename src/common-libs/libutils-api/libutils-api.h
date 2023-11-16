#if !defined(_PRISM_COMMON_LIBUTILSAPI_LIBUTILSAPI_H)
#define _PRISM_COMMON_LIBUTILSAPI_LIBUTILSAPI_H

#include <string.h>
#include <stdint.h>

#include <array>
#include <initializer_list>
#include <map>
#include <mutex>
#include <type_traits>
#include <utility>
#include <optional>

#include <qcoreapplication.h>
#include <qbytearray.h>
#include <qstring.h>
#include <qobject.h>
#include <qpointer.h>
#include <qfile.h>
#include <qlist.h>
#include <qthread.h>
#include <qhash.h>
#include <qmap.h>
#include <qvariant.h>
#include <qhostaddress.h>
#include <qregularexpression.h>
#include <qjsondocument.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qcborvalue.h>
#include <qset.h>

#if defined(Q_OS_WIN)
#include <malloc.h>
#elif defined(Q_OS_MACOS)
#include <malloc/malloc.h>
#include "libutils-api-mac.h"
#endif
#include "libutils-export.h"
#include "libutils-gpu-cpu-monitor-info.h"

LIBUTILSAPI_API std::recursive_mutex &pls_global_mutex();

LIBUTILSAPI_API uint32_t pls_last_error();
LIBUTILSAPI_API void pls_set_last_error(uint32_t last_error);

LIBUTILSAPI_API bool pls_object_is_valid(const QObject *object);
LIBUTILSAPI_API bool pls_object_is_valid(const QObject *object, const std::function<bool(const QObject *object)> &is_valid);
LIBUTILSAPI_API void pls_object_remove(const QObject *object);

LIBUTILSAPI_API bool pls_open_file(std::optional<QFile> &file, const QString &file_path, QFile::OpenMode mode);
LIBUTILSAPI_API bool pls_open_file(std::optional<QFile> &file, const QString &file_path, QFile::OpenMode mode, QString &error);
LIBUTILSAPI_API bool pls_mkdir(const QString &dir_path);

LIBUTILSAPI_API QVariantHash pls_map_to_hash(const QMap<QString, QString> &map);
LIBUTILSAPI_API QMap<QString, QString> pls_hash_to_map(const QVariantHash &hash);

LIBUTILSAPI_API QByteArray pls_read_data(const QString &file_path);
LIBUTILSAPI_API bool pls_read_data(QByteArray &data, const QString &file_path);
LIBUTILSAPI_API bool pls_write_data(const QString &file_path, const QByteArray &data);
LIBUTILSAPI_API bool pls_read_cbor(QCborValue &cbor, const QString &file_path);
LIBUTILSAPI_API bool pls_write_cbor(const QString &file_path, QCborValue &cbor);
LIBUTILSAPI_API QByteArray pls_remove_utf8_bom(const QByteArray &utf8);

struct pls_cmdline_args_t {
	int argc;
	char **argv;
};
LIBUTILSAPI_API void pls_set_cmdline_args(int argc, char *argv[]);
LIBUTILSAPI_API pls_cmdline_args_t pls_get_cmdline_args();
LIBUTILSAPI_API QStringList pls_cmdline_args();

//xxx=?, name:xxx=
LIBUTILSAPI_API std::optional<const char *> pls_cmdline_get_arg(int argc, char **gv, const char *name);
LIBUTILSAPI_API std::optional<QString> pls_cmdline_get_arg(const QStringList &args, const QString &name);
LIBUTILSAPI_API std::optional<qint16> pls_cmdline_get_int16_arg(const QStringList &args, const QString &name);
LIBUTILSAPI_API std::optional<quint16> pls_cmdline_get_uint16_arg(const QStringList &args, const QString &name);
LIBUTILSAPI_API std::optional<qint32> pls_cmdline_get_int32_arg(const QStringList &args, const QString &name);
LIBUTILSAPI_API std::optional<quint32> pls_cmdline_get_uint32_arg(const QStringList &args, const QString &name);
LIBUTILSAPI_API std::optional<qint64> pls_cmdline_get_int64_arg(const QStringList &args, const QString &name);
LIBUTILSAPI_API std::optional<quint64> pls_cmdline_get_uint64_arg(const QStringList &args, const QString &name);

LIBUTILSAPI_API bool pls_read_json(QJsonDocument &doc, const QString &file_path);
LIBUTILSAPI_API bool pls_read_json(QJsonArray &array, const QString &file_path);
LIBUTILSAPI_API bool pls_read_json(QJsonObject &object, const QString &file_path);
LIBUTILSAPI_API bool pls_write_json(const QString &file_path, const QJsonDocument &doc);
LIBUTILSAPI_API bool pls_write_json(const QString &file_path, const QJsonArray &array);
LIBUTILSAPI_API bool pls_write_json(const QString &file_path, const QJsonObject &object);
LIBUTILSAPI_API bool pls_read_json_cbor(QJsonDocument &doc, const QString &file_path);
LIBUTILSAPI_API bool pls_read_json_cbor(QJsonArray &array, const QString &file_path);
LIBUTILSAPI_API bool pls_read_json_cbor(QJsonObject &object, const QString &file_path);
LIBUTILSAPI_API bool pls_write_json_cbor(const QString &file_path, const QJsonDocument &doc);
LIBUTILSAPI_API bool pls_write_json_cbor(const QString &file_path, const QJsonArray &array);
LIBUTILSAPI_API bool pls_write_json_cbor(const QString &file_path, const QJsonObject &object);

LIBUTILSAPI_API QStringList pls_to_string_list(const QJsonArray &array);
LIBUTILSAPI_API QJsonArray pls_to_json_array(const QStringList &string_list);
LIBUTILSAPI_API QString pls_to_string(const QJsonArray &array);
LIBUTILSAPI_API QString pls_to_string(const QJsonObject &object);
LIBUTILSAPI_API QJsonArray pls_to_json_array(const QString &jsonArray);
LIBUTILSAPI_API QJsonObject pls_to_json_object(const QString &jsonObject);
LIBUTILSAPI_API bool pls_save_file(const QString &path, const QByteArray &data);

LIBUTILSAPI_API QString pls_get_app_dir();
LIBUTILSAPI_API QString pls_get_dll_dir(const QString &dll_name);
LIBUTILSAPI_API QString pls_get_temp_dir(const QString &name);
LIBUTILSAPI_API QString pls_get_app_data_dir(const QString &name = QString());
LIBUTILSAPI_API QString pls_get_app_data_dir_pn(const QString &name); // with process name
LIBUTILSAPI_API QString pls_get_app_pn();                             // process name
LIBUTILSAPI_API bool pls_is_abs_path(const QString &path);
LIBUTILSAPI_API QString pls_to_abs_path(const QString &path);
LIBUTILSAPI_API bool pls_is_path_sep(char ch);
LIBUTILSAPI_API bool pls_is_path_sep(wchar_t ch);
LIBUTILSAPI_API const char *pls_get_path_file_name(const char *path);
LIBUTILSAPI_API const wchar_t *pls_get_path_file_name(const wchar_t *path);
LIBUTILSAPI_API QString pls_get_path_file_name(const QString &path);
LIBUTILSAPI_API bool pls_remove_file(const QString &path, QString *error = nullptr);
LIBUTILSAPI_API bool pls_rename_file(const QString &old_file, const QString &new_file, QString *error = nullptr);
LIBUTILSAPI_API QString pls_get_prism_subpath(const QString &subpath, bool creatIfNotExist = false);
LIBUTILSAPI_API QString pls_get_installed_obs_version();

class pls_process_t;
LIBUTILSAPI_API pls_process_t *pls_process_create(const QString &program, const QStringList &arguments = QStringList(), bool run_as = false);
LIBUTILSAPI_API pls_process_t *pls_process_create(const QString &program, const QStringList &arguments, const QString &work_dir, bool run_as = false);
LIBUTILSAPI_API pls_process_t *pls_process_create(uint32_t process_id);
LIBUTILSAPI_API void pls_process_destroy(pls_process_t *process);
LIBUTILSAPI_API int pls_process_wait(pls_process_t *process, int timeout = -1); // >0: process exited, =0: timeout, <0: failed
LIBUTILSAPI_API uint32_t pls_process_id(pls_process_t *process);
LIBUTILSAPI_API uint32_t pls_process_exit_code(pls_process_t *process, uint32_t default_exit_code = -1);
LIBUTILSAPI_API bool pls_process_exit_code(uint32_t *exit_code, pls_process_t *process);
LIBUTILSAPI_API bool pls_process_terminate(pls_process_t *process, int exit_code);
LIBUTILSAPI_API uint32_t pls_current_process_id();
LIBUTILSAPI_API bool pls_is_valid_process_id(uint32_t process_id);
LIBUTILSAPI_API bool pls_process_terminate(uint32_t process_id, int exit_code);
LIBUTILSAPI_API bool pls_process_terminate(int exit_code);

LIBUTILSAPI_API uint32_t pls_current_thread_id();

LIBUTILSAPI_API void pls_sleep(uint32_t seconds);
LIBUTILSAPI_API void pls_sleep_ms(uint32_t milliseconds);

class pls_sem_t;
LIBUTILSAPI_API pls_sem_t *pls_sem_create(int initial_count = 0);
LIBUTILSAPI_API void pls_sem_destroy(pls_sem_t *sem);
LIBUTILSAPI_API bool pls_sem_release(pls_sem_t *sem, int count = 1);
LIBUTILSAPI_API bool pls_sem_try_acquire(pls_sem_t *sem);
LIBUTILSAPI_API bool pls_sem_acquire(pls_sem_t *sem);

class pls_shm_t;
LIBUTILSAPI_API pls_shm_t *pls_shm_create(const QString &name, int max_data_size, bool for_send);
LIBUTILSAPI_API void pls_shm_destroy(pls_shm_t *shm);
LIBUTILSAPI_API void pls_shm_close(pls_shm_t *shm);
struct pls_shm_msg_t {
	int length;
	char *data;
};
using pls_shm_msg_destroy_cb_t = std::function<void(pls_shm_msg_t *msg)>;
using pls_shm_get_msg_cb_t = std::function<bool(pls_shm_msg_t *msg, int remaining, int max_data_size)>;
using pls_shm_result_cb_t = std::function<void(int count)>;
LIBUTILSAPI_API void pls_shm_send_msg(pls_shm_t *shm, const pls_shm_msg_destroy_cb_t &msg_destroy_cb, const pls_shm_get_msg_cb_t &get_msg_cb, const pls_shm_result_cb_t &result_cb = nullptr);
LIBUTILSAPI_API void pls_shm_receive_msg(pls_shm_t *shm, const std::function<void(pls_shm_msg_t *msg)> &proc_msg_cb, const pls_shm_result_cb_t &result_cb = nullptr);
LIBUTILSAPI_API bool pls_shm_is_for_send(pls_shm_t *shm);
LIBUTILSAPI_API bool pls_shm_is_for_receive(pls_shm_t *shm);
LIBUTILSAPI_API int pls_shm_get_max_data_size(pls_shm_t *shm);
LIBUTILSAPI_API bool pls_shm_is_online(pls_shm_t *shm);

struct pls_datetime_t {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int milliseconds;
	int timezone;
};
LIBUTILSAPI_API void pls_get_current_datetime(pls_datetime_t &datetime);
LIBUTILSAPI_API QString pls_datetime_to_string(const pls_datetime_t &datetime); // yyyy-mm-dd hh:mm:ss.zzz+00:00

//key=value;key=value
//key=value\nkey=value
LIBUTILSAPI_API QMap<QString, QString> pls_parse(const QString &str, const QRegularExpression &delimiter);

using pls_qapp_cb_t = std::function<void()>;
LIBUTILSAPI_API void pls_qapp_construct_add_cb(const pls_qapp_cb_t &qapp_cb);
LIBUTILSAPI_API void pls_qapp_deconstruct_add_cb(const pls_qapp_cb_t &qapp_cb);
LIBUTILSAPI_API void pls_qapp_construct();
LIBUTILSAPI_API void pls_qapp_deconstruct();

LIBUTILSAPI_API bool pls_is_main_thread(const QThread *thread);
LIBUTILSAPI_API bool pls_current_is_main_thread();

LIBUTILSAPI_API QString pls_gen_uuid();

LIBUTILSAPI_API bool pls_is_debugger_present();
LIBUTILSAPI_API void pls_printf(const wchar_t *text);
LIBUTILSAPI_API void pls_printf(const std::wstring &text);
LIBUTILSAPI_API void pls_printf(const QString &text);
LIBUTILSAPI_API void pls_printf(const QByteArray &utf8);
LIBUTILSAPI_API void pls_printf(const std::string &utf8);
LIBUTILSAPI_API void pls_mac_printf(const char *format, ...);

LIBUTILSAPI_API void pls_sync_invoke(QObject *object, const std::function<void()> &fn);
LIBUTILSAPI_API void pls_async_invoke(QObject *object, const std::function<void()> &fn);
LIBUTILSAPI_API void pls_async_invoke(QObject *object, const char *fn);

LIBUTILSAPI_API bool pls_prism_is_dev();

LIBUTILSAPI_API void pls_prism_set_locale(const std::string &locale);
LIBUTILSAPI_API QString pls_prism_get_locale();
LIBUTILSAPI_API QString pls_get_local_ip();
LIBUTILSAPI_API QString pls_get_local_mac();
LIBUTILSAPI_API QList<QHostAddress> pls_get_valid_hosts();

LIBUTILSAPI_API bool pls_show_in_graphical_shell(const QString &pathIn);
struct pls_win_ver_t {
	int major;
	int minor;
	int build;
	int revis;
};

#ifdef Q_OS_WIN
LIBUTILSAPI_API bool pls_get_win_dll_ver(pls_win_ver_t &ver, const QString &dll);
LIBUTILSAPI_API pls_win_ver_t pls_get_win_ver();
LIBUTILSAPI_API bool pls_is_after_win10();
LIBUTILSAPI_API QString pls_get_win_name();
LIBUTILSAPI_API QString pls_get_win_name_with_arch();
#elif defined(Q_OS_MACOS)
LIBUTILSAPI_API pls_mac_ver_t pls_get_mac_systerm_ver();
LIBUTILSAPI_API bool pls_unZipFile(const QString &dstDirPath, const QString &srcFilePath);
LIBUTILSAPI_API bool pls_copy_file_with_error_code(const QString &fileName, const QString &newName, bool overwrite, int &errorCode);
LIBUTILSAPI_API QString pls_get_system_identifier();
LIBUTILSAPI_API bool pls_file_is_existed(const QString &filePath);
LIBUTILSAPI_API bool pls_install_mac_package(const QString &unzipFolderPath, const QString &destBundlePath);
LIBUTILSAPI_API bool pls_restart_mac_app(const char *restartType);
LIBUTILSAPI_API QString pls_get_existed_downloaded_mac_app(const QString &downloadedBundleDir, const QString &downloadedVersion, bool deleteBundleExceptUpdatedBundle);
LIBUTILSAPI_API bool pls_remove_all_downloaded_mac_app_small_equal_version(const QString &downloadedBundleDir, const QString &softwareVersion);
LIBUTILSAPI_API void pls_bring_mac_window_to_front(WId winId);
LIBUTILSAPI_API QUrl pls_mac_hmac_url(const QUrl &url, const QByteArray &hmacKey);
LIBUTILSAPI_API QString pls_get_app_resource_dir();
LIBUTILSAPI_API QString pls_get_app_plugin_dir();
LIBUTILSAPI_API QString pls_get_app_macos_dir();
LIBUTILSAPI_API QString pls_get_bundle_dir();
LIBUTILSAPI_API bool pls_is_install_app(const QString &identifier, QString &appPath);
LIBUTILSAPI_API void pls_get_install_app_list(const QString &identifier, QStringList &appList);
LIBUTILSAPI_API bool pls_check_mac_app_is_existed(const wchar_t *executableName);
LIBUTILSAPI_API QString pls_get_mac_app_data_dir();
LIBUTILSAPI_API bool pls_activiate_mac_app();
LIBUTILSAPI_API bool pls_activiate_mac_app_except_self();
LIBUTILSAPI_API bool pls_activiate_mac_app(int pid);
LIBUTILSAPI_API void pls_add_monitor_process_exit_event(pls_process_t *process);
LIBUTILSAPI_API void pls_remove_monitor_process_exit_event(pls_process_t *process);
LIBUTILSAPI_API void pls_application_show_dock_icon(bool isShow);
LIBUTILSAPI_API QString pls_get_app_content_dir();
LIBUTILSAPI_API bool pls_is_app_running(const char *bundle_id);
LIBUTILSAPI_API bool pls_launch_app(const char *bundle_id, const char *app_name);
LIBUTILSAPI_API QString pls_get_current_system_language_id();
#endif

LIBUTILSAPI_API std::wstring pls_utf8_to_unicode(const char *utf8);
LIBUTILSAPI_API std::string pls_unicode_to_utf8(const wchar_t *unicode);

#define pls_is_app_exiting() pls_get_app_exiting()
#define pls_is_app_closing() pls_get_app_exiting()
LIBUTILSAPI_API bool pls_get_app_exiting();
LIBUTILSAPI_API void pls_set_app_exiting(bool);

LIBUTILSAPI_API void pls_env_add_path(const QByteArray &path);
LIBUTILSAPI_API void pls_env_add_paths(const QByteArrayList &paths);
LIBUTILSAPI_API void pls_env_add_path(const QString &path);
LIBUTILSAPI_API void pls_env_add_paths(const QStringList &paths);

#if defined(Q_OS_WIN)
LIBUTILSAPI_API bool pls_is_process_running(const wchar_t *executableName, int &pid);
#elif defined(Q_OS_MACOS)
LIBUTILSAPI_API bool pls_is_process_running(const char *executableName, int &pid);
#endif

LIBUTILSAPI_API uint64_t pls_get_prism_version();
LIBUTILSAPI_API void pls_set_prism_version(uint64_t version);
LIBUTILSAPI_API void pls_set_prism_version(uint16_t major, uint16_t minor, uint16_t patch, uint16_t build);
LIBUTILSAPI_API uint16_t pls_get_prism_version_major();
LIBUTILSAPI_API uint16_t pls_get_prism_version_minor();
LIBUTILSAPI_API uint16_t pls_get_prism_version_patch();
LIBUTILSAPI_API uint16_t pls_get_prism_version_build();

#define pls_check_app_exiting(...)          \
	do {                                \
		if (pls_is_app_exiting()) { \
			return __VA_ARGS__; \
		}                           \
	} while (0)
#define pls_check_app_closing pls_check_app_exiting
#define pls_modal_check_app_closing pls_check_app_exiting
#define pls_modal_check_app_exiting pls_check_app_exiting

LIBUTILSAPI_API qulonglong pls_get_qobject_id(const QObject *object);
LIBUTILSAPI_API bool pls_check_qobject_id(const QObject *object, qulonglong object_id);
LIBUTILSAPI_API bool pls_check_qobject_id(const QObject *object, qulonglong object_id, const std::function<bool(const QObject *object)> &is_valid);

LIBUTILSAPI_API int pls_get_platform_window_height_by_windows_height(int windowsHeight);

struct pls_dl_t {
	void *handle = nullptr;
	bool close = true;
};
using pls_fn_ptr_t = void (*)();
LIBUTILSAPI_API bool pls_dlopen(pls_dl_t &dl, const QString &dl_path);
LIBUTILSAPI_API pls_fn_ptr_t pls_dlsym(pls_dl_t &dl, const char *func);
LIBUTILSAPI_API void pls_dlclose(pls_dl_t &dl);

LIBUTILSAPI_API std::string pls_get_cpu_name();

enum class pls_language_t { //
	NotImplement = -1,
	SimpleChinese = 0,
	TraditionalChinese,
	English,
	Korean,
	Vietnamese,
	Spanish,
	Indonesian,
	Japanese,
	Portuguese,
	Other
};
LIBUTILSAPI_API pls_language_t pls_get_os_language();
// return zh-CN en-US ko-KR ...
LIBUTILSAPI_API QString pls_get_os_language(pls_language_t language);

#if defined(Q_OS_WIN)
LIBUTILSAPI_API pls_language_t pls_get_os_language_from_lid(uint16_t lid);
#elif defined(Q_OS_MACOS)
#endif

LIBUTILSAPI_API bool pls_is_mouse_pressed(Qt::MouseButton button);

namespace pls {
template<typename QtApp> class Application : public QtApp {
public:
	Application(int &argc, char **argv) : QtApp(argc, argv) { pls_qapp_construct(); }
	~Application() override { pls_qapp_deconstruct(); }
};

template<typename T> struct Initializer {
	static T s_initializer;
};
template<typename T> T Initializer<T>::s_initializer;

template<typename Key, typename Value> using map = std::map<Key, Value>;

template<size_t Size> class chars : public std::array<char, Size> {
public:
	chars() { std::array<char, Size>::fill(0); }

	operator char *() { return this->data(); }
	operator const char *() const { return this->data(); }

	const char *c_str() const { return this->data(); }

	QString toString() const { return QString::fromUtf8(this->data()); }
	QByteArray toByteArray() const { return this->data(); }

	std::string operator+(const std::string &s) const { return this->data() + s; }
	QString operator+(const QString &s) const { return toString() + s; }
	QByteArray operator+(const QByteArray &s) const { return toByteArray() + s; }

	size_t capacity() const { return this->size() - 1; }
};

template<size_t Size> class wchars : public std::array<wchar_t, Size> {
public:
	wchars() { std::array<wchar_t, Size>::fill(0); }

	operator wchar_t *() { return this->data(); }
	operator const wchar_t *() const { return this->data(); }

	const wchar_t *c_str() const { return this->data(); }

	QString toString() const { return QString::fromWCharArray(this->data()); }
	QByteArray toByteArray() const { return toString().toUtf8(); }

	std::wstring operator+(const std::wstring &s) const { return this->data() + s; }
	QString operator+(const QString &s) const { return toString() + s; }
	QByteArray operator+(const QByteArray &s) const { return toByteArray() + s; }

	size_t capacity() const { return this->size() - 1; }
};

template<typename T> struct JsonDocument;
template<> struct JsonDocument<QJsonObject> {
	mutable QJsonObject m_object;

	JsonDocument() = default;
	explicit JsonDocument(const QJsonObject &object) : m_object(object) {}

	const QJsonObject &object() const { return m_object; }
	QJsonDocument toJsonDocument() const { return QJsonDocument(m_object); }
	QByteArray toByteArray() const { return toJsonDocument().toJson(); }

	const JsonDocument &add(const QString &name, const QJsonValue &value) const
	{
		m_object.insert(name, value);
		return *this;
	}
};
template<> struct JsonDocument<QJsonArray> {
	mutable QJsonArray m_array;

	JsonDocument() = default;
	explicit JsonDocument(const QJsonArray &array) : m_array(array) {}

	const QJsonArray &array() const { return m_array; }
	QJsonDocument toJsonDocument() const { return QJsonDocument(m_array); }
	QByteArray toByteArray() const { return toJsonDocument().toJson(); }

	const JsonDocument &add(const QJsonValue &value) const
	{
		m_array.append(value);
		return *this;
	}
};

template<typename Object> class QObjectPtr {
	qulonglong m_object_id;
	Object *m_object;

public:
	QObjectPtr(const Object *object = nullptr) : m_object_id(pls_get_qobject_id(object)), m_object(pls_ptr(object)) {}

	bool valid() const { return pls_check_qobject_id(m_object, m_object_id); }
	template<typename IsValid> bool valid(const IsValid &is_valid) const { return pls_check_qobject_id(m_object, m_object_id, is_valid); }

	qulonglong object_id() const { return m_object_id; }

	Object *object() { return valid() ? m_object : nullptr; }
	const Object *object() const { return valid() ? m_object : nullptr; }

	Object *operator=(const Object *object)
	{
		m_object_id = pls_get_qobject_id(object);
		m_object = pls_ptr(object);
		return m_object;
	}
	Object *operator=(std::nullptr_t)
	{
		m_object_id = 0;
		m_object = nullptr;
		return m_object;
	}

	bool operator==(const QObjectPtr &object) const { return m_object_id == object.m_object_id && m_object == object.m_object; }
	bool operator!=(const QObjectPtr &object) const { return !operator==(object); }

	bool operator<(const QObjectPtr &object) const { return m_object_id < object.m_object_id; }
	bool operator<=(const QObjectPtr &object) const { return m_object_id <= object.m_object_id; }
	bool operator>(const QObjectPtr &object) const { return m_object_id > object.m_object_id; }
	bool operator>=(const QObjectPtr &object) const { return m_object_id >= object.m_object_id; }

	Object *operator->() { return object(); }
	const Object *operator->() const { return object(); }

	template<typename T> friend qulonglong get_object_id(const QObjectPtr<T> &object);
	template<typename T> friend T *get_object(const QObjectPtr<T> &object);
};

template<typename T, typename Deleter> class AutoObject {
	T m_object{};
	Deleter m_deleter{};

public:
	AutoObject() = default;
	explicit AutoObject(const T &object) : m_object{object} {}
	explicit AutoObject(const T &object, Deleter deleter) : m_object{object}, m_deleter{deleter} {}
	~AutoObject() { m_deleter(m_object); }

	AutoObject(const AutoObject &) = delete;
	void operator=(const AutoObject &) = delete;

	T &object() { return m_object; }
	const T &object() const { return m_object; }

	void reset(const T &value)
	{
		m_deleter(m_object);
		m_object = value;
	}
};

template<typename T> struct AutoPtrDeleter {
	void operator()(T *&ptr) const { pls_delete(ptr, nullptr); }
};
template<typename T, typename Deleter = AutoPtrDeleter<T>> class AutoPtr {
	T *m_ptr{};
	Deleter m_deleter{};

public:
	AutoPtr() = default;
	explicit AutoPtr(T *ptr) : m_ptr{ptr} {}
	explicit AutoPtr(T *ptr, Deleter deleter) : m_ptr{ptr}, m_deleter{deleter} {}
	~AutoPtr() { m_deleter(m_ptr); }

	AutoPtr(const AutoPtr &) = delete;
	void operator=(const AutoPtr &) = delete;

	T *&ptr() { return m_ptr; }
	const T *&ptr() const { return m_ptr; }

	void reset(T *ptr)
	{
		m_deleter(m_ptr);
		m_ptr = ptr;
	}
};

template<typename Handle, typename Deleter> class AutoHandle {
	Handle m_handle{};
	Deleter m_deleter{};

public:
	AutoHandle() = default;
	explicit AutoHandle(Handle handle) : m_handle{handle} {}
	explicit AutoHandle(Handle handle, Deleter deleter) : m_handle{handle}, m_deleter{deleter} {}
	~AutoHandle() { m_deleter(m_handle); }

	AutoHandle(const AutoHandle &) = delete;
	void operator=(const AutoHandle &) = delete;

	Handle &handle() { return m_handle; }
	const Handle &handle() const { return m_handle; }

	void reset(Handle handle)
	{
		m_deleter(m_handle);
		m_handle = handle;
	}
};

struct QMetaObjectConnectionDeleter {
	inline void operator()(const QMetaObject::Connection &connection) const { QObject::disconnect(connection); }
};
using MetaObjectConnection = AutoObject<QMetaObject::Connection, QMetaObjectConnectionDeleter>;
}

template<typename T, typename Deleter> using PLSAutoObject = pls::AutoObject<T, Deleter>;
template<typename T, typename Deleter = pls::AutoPtrDeleter<T>> using PLSAutoPtr = pls::AutoPtr<T, Deleter>;
template<typename Handle, typename Deleter> using PLSAutoHandle = pls::AutoHandle<Handle, Deleter>;
using PLSMetaObjectConnection = pls::MetaObjectConnection;

#define PLS_NEW_DELETE_FRIENDS                                                            \
	template<typename T, typename... Args> friend T *pls_new(Args &&...args);         \
	template<typename T, typename... Args> friend T *pls_new_nothrow(Args &&...args); \
	template<typename T> friend T *pls_new_array(size_t count);                       \
	template<typename T> friend void pls_delete(T *object);                           \
	template<typename T> friend void pls_delete(T *&object, std::nullptr_t);          \
	template<typename T> friend void pls_delete_array(T *array);                      \
	template<typename T> friend void pls_delete_array(T *&array, std::nullptr_t);

#define PLS_DISABLE_COPY(class)        \
	class(const class &) = delete; \
	class &operator=(const class &) = delete;
#define PLS_DISABLE_MOVE(class)   \
	class(class &&) = delete; \
	class &operator=(class &&) = delete;
#define PLS_DISABLE_COPY_AND_MOVE(class) \
	PLS_DISABLE_COPY(class)          \
	PLS_DISABLE_MOVE(class)

#define pls_abort_assert(condition) \
	do {                        \
		if (!(condition)) { \
			abort();    \
		}                   \
	} while (0)

template<typename T, typename... Args> T *pls_new(Args &&...args)
{
	return new T(std::forward<Args>(args)...);
}
template<typename T, typename... Args> T *pls_new_nothrow(Args &&...args)
{
	return new (std::nothrow) T(std::forward<Args>(args)...);
}
template<typename T> T *pls_new_array(size_t count)
{
	return new T[count];
}

template<typename T> void pls_delete(T *object)
{
	if (object) {
		delete object;
	}
}
template<typename T> void pls_delete(QPointer<T> &object)
{
	if (object) {
		delete object;
		object.clear();
	}
}
template<typename T> void pls_delete(T *&object, std::nullptr_t)
{
	if (object) {
		delete object;
		object = nullptr;
	}
}

template<typename T, typename Deleter, std::enable_if_t<!std::is_same_v<Deleter, std::nullptr_t>, int> = 0> void pls_delete(T *object, Deleter deleter)
{
	if (object) {
		deleter(object);
	}
}
template<typename T, typename Deleter> void pls_delete(T *&object, Deleter deleter, std::nullptr_t)
{
	if (object) {
		deleter(object);
		object = nullptr;
	}
}

template<typename T> void pls_delete_array(T *array)
{
	if (array) {
		delete[] array;
	}
}
template<typename T> void pls_delete_array(T *&array, std::nullptr_t)
{
	if (array) {
		delete[] array;
		array = nullptr;
	}
}

template<typename T> void pls_delete_later(T *object)
{
	if (object) {
		object->deleteLater();
	}
}
template<typename T> void pls_delete_later(T *&object, std::nullptr_t)
{
	if (object) {
		object->deleteLater();
		object = nullptr;
	}
}

template<typename T> void pls_delete_thread(T *thread)
{
	if (thread) {
		thread->quit();
		thread->wait();
		delete thread;
	}
}
template<typename T> void pls_delete_thread(T *&thread, std::nullptr_t)
{
	pls_delete_thread(thread);
	thread = nullptr;
}
template<typename T> void pls_destroy(T *object)
{
	if (object) {
		object->destroy();
	}
}
template<typename T> void pls_destroy(T *&object, std::nullptr_t)
{
	if (object) {
		object->destroy();
		object = nullptr;
	}
}

template<typename T> T *pls_malloc(size_t size)
{
	return (T *)malloc(size);
}
template<typename T> T *pls_realloc(void *object, size_t size)
{
	return (T *)realloc(object, size);
}

template<typename T> void pls_free(T *ptr)
{
	if (ptr) {
		free(ptr);
	}
}
template<typename T> void pls_free(T *&ptr, std::nullptr_t)
{
	if (ptr) {
		free(ptr);
		ptr = nullptr;
	}
}

template<typename T, typename... Args> constexpr auto pls_make_array(Args &&...vals) -> std::array<T, sizeof...(Args)>
{
	return {std::forward<Args>(vals)...};
}
template<typename T, typename Map, typename... Args> auto pls_make_array_map(Map map, Args &&...vals) -> std::array<T, sizeof...(Args)>
{
	return {map(std::forward<Args>(vals))...};
}

template<typename... T> void pls_used(T &&...) {}
template<typename... T> void pls_unused(T &&...) {}

template<typename T> T *pls_ptr(const T *ptr)
{
	return const_cast<T *>(ptr);
}
template<typename T> T &pls_ref(const T &ptr)
{
	return const_cast<T &>(ptr);
}
template<typename To, typename From> To *pls_dynamic_cast(From *object)
{
	return dynamic_cast<To *>(pls_ptr(object));
}
template<typename To, typename From> To *pls_dynamic_cast(const pls::QObjectPtr<From> &object)
{
	return dynamic_cast<To *>(pls_ptr(object.object()));
}

template<typename T> size_t qHash(const pls::QObjectPtr<T> &key, size_t seed = 0) noexcept
{
	return qHash(key.object_id(), seed);
}
template<typename To, typename From> auto pls_qobject_ptr(const From *object) -> std::enable_if_t<std::is_same_v<std::decay_t<To>, std::decay_t<From>>, pls::QObjectPtr<To>>
{
	return pls::QObjectPtr<To>{object};
}
template<typename To, typename From>
auto pls_qobject_ptr(const From *object) -> std::enable_if_t<!std::is_same_v<std::decay_t<To>, std::decay_t<From>> && std::is_base_of_v<std::decay_t<From>, std::decay_t<To>>, pls::QObjectPtr<To>>
{
	return pls::QObjectPtr<To>{pls_dynamic_cast<To>(object)};
}
template<typename To, typename From>
auto pls_qobject_ptr(const From *object) -> std::enable_if_t<!std::is_same_v<std::decay_t<To>, std::decay_t<From>> && std::is_base_of_v<std::decay_t<To>, std::decay_t<From>>, pls::QObjectPtr<To>>
{
	return pls::QObjectPtr<To>{object};
}
template<typename To, typename From> QSet<pls::QObjectPtr<To>> pls_qobject_ptr_set(const QSet<From> &objects)
{
	QSet<pls::QObjectPtr<To>> object_ptr_set;
	pls_for_each(objects, [&object_ptr_set](const auto &v) { object_ptr_set.insert(pls_qobject_ptr<To>(v)); });
	return object_ptr_set;
}
namespace pls {
template<typename T> inline qulonglong get_object_id(const QObjectPtr<T> &object)
{
	return object.m_object_id;
}
template<typename T> inline T *get_object(const pls::QObjectPtr<T> &object)
{
	return pls_ptr(object.m_object);
}
}

template<typename T> inline void pls_bzero(T *ptr, size_t size)
{
	memset(ptr, 0, size);
}
template<typename T> inline void pls_bzero(T *ptr)
{
	pls_bzero(ptr, sizeof(T));
}

inline bool pls_is_empty(const char *str)
{
	return !str || !str[0];
}
inline bool pls_is_equal(const char *str1, const char *str2)
{
	if (str1 == str2)
		return true;
	else if (!str1 || !str2)
		return false;
	else if (!strcmp(str1, str2))
		return true;
	return false;
}
inline bool pls_is_empty(const wchar_t *str)
{
	return !str || !str[0];
}
inline bool pls_is_equal(const wchar_t *str1, const wchar_t *str2)
{
	if (str1 == str2)
		return true;
	else if (!str1 || !str2)
		return false;
	else if (!wcscmp(str1, str2))
		return true;
	return false;
}

template<typename Fn, typename... Args> inline auto pls_invoke(void *func, Args &&...args) -> std::invoke_result_t<Fn, Args...>
{
	if constexpr (std::is_same_v<std::invoke_result_t<Fn, Args...>, void>) {
		(*(Fn *)func)(std::forward<Args>(args)...);
	} else {
		return (*(Fn *)func)(std::forward<Args>(args)...);
	}
}
template<typename Fn, typename... Args> inline auto pls_invoke_safe(void *func, Args &&...args) -> std::invoke_result_t<Fn, Args...>
{
	if constexpr (std::is_same_v<std::invoke_result_t<Fn, Args...>, void>) {
		if (func) {
			pls_invoke<Fn>(func, std::forward<Args>(args)...);
		}
	} else {
		if (func) {
			return pls_invoke<Fn>(func, std::forward<Args>(args)...);
		}
		return {};
	}
}
template<typename Fn, typename... Args> inline auto pls_invoke_safe(const std::invoke_result_t<Fn, Args...> &retval, void *func, Args &&...args) -> std::invoke_result_t<Fn, Args...>
{
	if (func) {
		return pls_invoke<Fn>(func, std::forward<Args>(args)...);
	}
	return {retval};
}

template<typename Fn, typename... Args> inline auto pls_invoke(Fn &fn, Args &&...args) -> std::invoke_result_t<Fn, Args...>
{
	if constexpr (std::is_same_v<std::invoke_result_t<Fn, Args...>, void>) {
		fn(std::forward<Args>(args)...);
	} else {
		return fn(std::forward<Args>(args)...);
	}
}
template<typename Fn, typename... Args> inline auto pls_invoke_safe(Fn &fn, Args &&...args) -> std::invoke_result_t<Fn, Args...>
{
	if constexpr (std::is_same_v<std::invoke_result_t<Fn, Args...>, void>) {
		if (fn) {
			pls_invoke(fn, std::forward<Args>(args)...);
		}
	} else {
		if (fn) {
			return pls_invoke(fn, std::forward<Args>(args)...);
		}
		return {};
	}
}
template<typename Fn, typename... Args> inline auto pls_invoke_safe(const std::invoke_result_t<Fn, Args...> &retval, Fn &fn, Args &&...args) -> std::invoke_result_t<Fn, Args...>
{
	if (fn) {
		return pls_invoke(fn, std::forward<Args>(args)...);
	}
	return {retval};
}

template<typename Fn, typename... Args> inline auto pls_invoke(const std::optional<Fn> &fn, Args &&...args) -> std::invoke_result_t<Fn, Args...>
{
	if constexpr (std::is_same_v<std::invoke_result_t<Fn, Args...>, void>) {
		fn.value()(std::forward<Args>(args)...);
	} else {
		return fn.value()(std::forward<Args>(args)...);
	}
}
template<typename Fn, typename... Args> inline auto pls_invoke_safe(const std::optional<Fn> &fn, Args &&...args) -> std::invoke_result_t<Fn, Args...>
{
	if constexpr (std::is_same_v<std::invoke_result_t<Fn, Args...>, void>) {
		if (fn.has_value()) {
			pls_invoke_safe(fn.value(), std::forward<Args>(args)...);
		}
	} else {
		if (fn.has_value()) {
			return pls_invoke_safe(fn.value(), std::forward<Args>(args)...);
		}
		return {};
	}
}
template<typename Fn, typename... Args> inline auto pls_invoke_safe(const std::invoke_result_t<Fn, Args...> &retval, const std::optional<Fn> &fn, Args &&...args) -> std::invoke_result_t<Fn, Args...>
{
	if (fn.has_value()) {
		return pls_invoke_safe(retval, fn.value(), std::forward<Args>(args)...);
	}
	return {retval};
}

template<typename T, typename Mf, typename... Args> inline auto pls_invoke(T *obj, Mf mf, Args &&...args) -> std::invoke_result_t<Mf, T *, Args...>
{
	if constexpr (std::is_same_v<std::invoke_result_t<Mf, T *, Args...>, void>) {
		(obj->*mf)(std::forward<Args>(args)...);
	} else {
		return (obj->*mf)(std::forward<Args>(args)...);
	}
}
template<typename T, typename Mf, typename... Args> inline auto pls_invoke_safe(T *obj, Mf mf, Args &&...args) -> std::invoke_result_t<Mf, T *, Args...>
{
	if constexpr (std::is_same_v<std::invoke_result_t<Mf, T *, Args...>, void>) {
		if (obj && mf) {
			(obj->*mf)(std::forward<Args>(args)...);
		}
	} else {
		if (obj && mf) {
			return (obj->*mf)(std::forward<Args>(args)...);
		}
		return {};
	}
}
template<typename T, typename Mf, typename... Args>
inline auto pls_invoke_safe(const std::invoke_result_t<Mf, T *, Args...> &retval, T *obj, Mf mf, Args &&...args) -> std::invoke_result_t<Mf, T *, Args...>
{
	if (obj && mf) {
		return (obj->*mf)(std::forward<Args>(args)...);
	}
	return {retval};
}

template<typename T> bool pls_object_is_valid(const pls::QObjectPtr<T> &object)
{
	return object.valid();
}
template<typename T, typename IsValid> bool pls_object_is_valid(const pls::QObjectPtr<T> &object, const IsValid &is_valid)
{
	return object.valid(is_valid);
}
template<typename T> bool pls_objects_is_valid(const QList<T> &objects)
{
	for (const auto &object : objects) {
		if (!pls_object_is_valid(object)) {
			return false;
		}
	}
	return true;
}
template<typename T, typename IsValid> bool pls_objects_is_valid(const QList<T> &objects, const IsValid &is_valid)
{
	for (const auto &object : objects) {
		if (!pls_object_is_valid(object, is_valid)) {
			return false;
		}
	}
	return true;
}
template<typename T> bool pls_objects_is_valid(const QSet<T> &objects)
{
	for (const auto &object : objects) {
		if (!pls_object_is_valid(object)) {
			return false;
		}
	}
	return true;
}
template<typename T, typename IsValid> bool pls_objects_is_valid(const QSet<T> &objects, const IsValid &is_valid)
{
	for (const auto &object : objects) {
		if (!pls_object_is_valid(object, is_valid)) {
			return false;
		}
	}
	return true;
}

template<typename P, typename Fn> auto pls_sync_call(const pls::QObjectPtr<P> &proxy, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn>, void>
{
	if (proxy.valid()) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, fn]() {
			if (proxy.valid()) {
				fn();
			}
		});
	}
}
template<typename P, typename Fn> auto pls_sync_call(const pls::QObjectPtr<P> &proxy, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn, P *>, void>
{
	if (proxy.valid()) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, fn]() {
			if (proxy.valid()) {
				fn(pls::get_object(proxy));
			}
		});
	}
}
template<typename P, typename Fn> auto pls_sync_call(const pls::QObjectPtr<P> &proxy, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn, pls::QObjectPtr<P>>, void>
{
	if (proxy.valid()) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, fn]() {
			if (proxy.valid()) {
				fn(proxy);
			}
		});
	}
}
template<typename P, typename Fn> void pls_sync_call(const P *proxy, const Fn &fn)
{
	pls_sync_call(pls_qobject_ptr<P>(proxy), fn);
}
template<typename P, typename Fn> auto pls_async_call(const pls::QObjectPtr<P> &proxy, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn>, void>
{
	if (proxy.valid()) {
		pls_async_invoke(pls::get_object(proxy), [proxy, fn]() {
			if (proxy.valid()) {
				fn();
			}
		});
	}
}
template<typename P, typename Fn> auto pls_async_call(const pls::QObjectPtr<P> &proxy, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn, P *>, void>
{
	if (proxy.valid()) {
		pls_async_invoke(pls::get_object(proxy), [proxy, fn]() {
			if (proxy.valid()) {
				fn(pls::get_object(proxy));
			}
		});
	}
}
template<typename P, typename Fn> auto pls_async_call(const pls::QObjectPtr<P> &proxy, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn, pls::QObjectPtr<P>>, void>
{
	if (proxy.valid()) {
		pls_async_invoke(pls::get_object(proxy), [proxy, fn]() {
			if (proxy.valid()) {
				fn(proxy);
			}
		});
	}
}
template<typename P, typename Fn> void pls_async_call(const P *proxy, const Fn &fn)
{
	pls_async_call(pls_qobject_ptr<P>(proxy), fn);
}
template<typename P, typename T, typename Fn> auto pls_sync_call(const pls::QObjectPtr<P> &proxy, const pls::QObjectPtr<T> &object, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn>, void>
{
	if (proxy.valid() && object.valid()) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object, fn]() {
			if (proxy.valid() && object.valid()) {
				fn();
			}
		});
	}
}
template<typename P, typename T, typename Fn>
auto pls_sync_call(const pls::QObjectPtr<P> &proxy, const pls::QObjectPtr<T> &object, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn, T *>, void>
{
	if (proxy.valid() && object.valid()) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object, fn]() {
			if (proxy.valid() && object.valid()) {
				fn(pls::get_object(object));
			}
		});
	}
}
template<typename P, typename T, typename Fn>
auto pls_sync_call(const pls::QObjectPtr<P> &proxy, const pls::QObjectPtr<T> &object, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn, P *, T *>, void>
{
	if (proxy.valid() && object.valid()) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object, fn]() {
			if (proxy.valid() && object.valid()) {
				fn(pls::get_object(proxy), pls::get_object(object));
			}
		});
	}
}
template<typename P, typename T, typename Fn>
auto pls_sync_call(const pls::QObjectPtr<P> &proxy, const pls::QObjectPtr<T> &object, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn, pls::QObjectPtr<T>>, void>
{
	if (proxy.valid() && object.valid()) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object, fn]() {
			if (proxy.valid() && object.valid()) {
				fn(object);
			}
		});
	}
}
template<typename P, typename T, typename Fn>
auto pls_sync_call(const pls::QObjectPtr<P> &proxy, const pls::QObjectPtr<T> &object, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn, pls::QObjectPtr<P>, pls::QObjectPtr<T>>, void>
{
	if (proxy.valid() && object.valid()) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object, fn]() {
			if (proxy.valid() && object.valid()) {
				fn(proxy, object);
			}
		});
	}
}
template<typename P, typename T, typename Fn> void pls_sync_call(const P *proxy, const pls::QObjectPtr<T> &object, const Fn &fn)
{
	pls_sync_call(pls_qobject_ptr<P>(proxy), object, fn);
}
template<typename P, typename T, typename Fn> void pls_sync_call(const pls::QObjectPtr<P> proxy, const T *object, const Fn &fn)
{
	pls_sync_call(proxy, pls_qobject_ptr<T>(object), fn);
}
template<typename P, typename T, typename Fn> void pls_sync_call(const P *proxy, const T *object, const Fn &fn)
{
	pls_sync_call(pls_qobject_ptr<P>(proxy), pls_qobject_ptr<T>(object), fn);
}
template<typename P, typename T, typename Fn> auto pls_async_call(const pls::QObjectPtr<P> &proxy, const pls::QObjectPtr<T> &object, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn>, void>
{
	if (proxy.valid() && object.valid()) {
		pls_async_invoke(pls::get_object(proxy), [proxy, object, fn]() {
			if (proxy.valid() && object.valid()) {
				fn();
			}
		});
	}
}
template<typename P, typename T, typename Fn>
auto pls_async_call(const pls::QObjectPtr<P> &proxy, const pls::QObjectPtr<T> &object, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn, T *>, void>
{
	if (proxy.valid() && object.valid()) {
		pls_async_invoke(pls::get_object(proxy), [proxy, object, fn]() {
			if (proxy.valid() && object.valid()) {
				fn(pls::get_object(object));
			}
		});
	}
}
template<typename P, typename T, typename Fn>
auto pls_async_call(const pls::QObjectPtr<P> &proxy, const pls::QObjectPtr<T> &object, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn, P *, T *>, void>
{
	if (proxy.valid() && object.valid()) {
		pls_async_invoke(pls::get_object(proxy), [proxy, object, fn]() {
			if (proxy.valid() && object.valid()) {
				fn(pls::get_object(proxy), pls::get_object(object));
			}
		});
	}
}
template<typename P, typename T, typename Fn>
auto pls_async_call(const pls::QObjectPtr<P> &proxy, const pls::QObjectPtr<T> &object, const Fn &fn) -> std::enable_if_t<std::is_invocable_v<Fn, pls::QObjectPtr<P>, pls::QObjectPtr<T>>, void>
{
	if (proxy.valid() && object.valid()) {
		pls_async_invoke(pls::get_object(proxy), [proxy, object, fn]() {
			if (proxy.valid() && object.valid()) {
				fn(proxy, object);
			}
		});
	}
}
template<typename P, typename T, typename Fn> void pls_async_call(const P *proxy, const pls::QObjectPtr<T> &object, const Fn &fn)
{
	pls_async_call(pls_qobject_ptr<P>(proxy), object, fn);
}
template<typename P, typename T, typename Fn> void pls_async_call(const pls::QObjectPtr<P> &proxy, const T *object, const Fn &fn)
{
	pls_async_call(proxy, pls_qobject_ptr<T>(object), fn);
}
template<typename P, typename T, typename Fn> void pls_async_call(const P *proxy, const T *object, const Fn &fn)
{
	pls_async_call(pls_qobject_ptr<P>(proxy), pls_qobject_ptr<T>(object), fn);
}
template<typename Fn> void pls_sync_call(const pls::QObjectPtr<QObject> &proxy, const QSet<const QObject *> &objects, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(objects)) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object_ptr_set = pls_qobject_ptr_set<QObject>(objects), fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptr_set)) {
				fn();
			}
		});
	}
}
template<typename Fn> void pls_async_call(const pls::QObjectPtr<QObject> &proxy, const QSet<const QObject *> &objects, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(objects)) {
		pls_async_invoke(pls::get_object(proxy), [proxy, object_ptr_set = pls_qobject_ptr_set<QObject>(objects), fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptr_set)) {
				fn();
			}
		});
	}
}
template<typename IsValid, typename Fn> void pls_sync_call(const pls::QObjectPtr<QObject> &proxy, const pls::QObjectPtr<QObject> &object, const IsValid &is_valid, const Fn &fn)
{
	if (proxy.valid() && pls_object_is_valid(object, is_valid)) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object, is_valid, fn]() {
			if (proxy.valid() && object.valid(is_valid)) {
				fn();
			}
		});
	}
}
template<typename IsValid, typename Fn> void pls_async_call(const pls::QObjectPtr<QObject> &proxy, const pls::QObjectPtr<QObject> &object, const IsValid &is_valid, const Fn &fn)
{
	if (proxy.valid() && object.valid(is_valid)) {
		pls_async_invoke(pls::get_object(proxy), [proxy, object, is_valid, fn]() {
			if (proxy.valid() && object.valid(is_valid)) {
				fn();
			}
		});
	}
}
template<typename IsValid, typename Fn> void pls_sync_call(const pls::QObjectPtr<QObject> &proxy, const QSet<const QObject *> &objects, const IsValid &is_valid, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(objects, is_valid)) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object_ptr_set = pls_qobject_ptr_set<QObject>(objects), is_valid, fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptr_set, is_valid)) {
				fn();
			}
		});
	}
}
template<typename IsValid, typename Fn> void pls_async_call(const pls::QObjectPtr<QObject> &proxy, const QSet<const QObject *> &objects, const IsValid &is_valid, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(objects, is_valid)) {
		pls_async_invoke(pls::get_object(proxy), [proxy, object_ptr_set = pls_qobject_ptr_set<QObject>(objects), is_valid, fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptr_set, is_valid)) {
				fn();
			}
		});
	}
}
template<typename T, typename Fn> void pls_sync_call(const pls::QObjectPtr<QObject> &proxy, const QSet<pls::QObjectPtr<T>> &object_ptrs, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(object_ptrs)) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object_ptrs, fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptrs)) {
				fn();
			}
		});
	}
}
template<typename T, typename Fn> void pls_async_call(const pls::QObjectPtr<QObject> &proxy, const QSet<pls::QObjectPtr<T>> &object_ptrs, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(object_ptrs)) {
		pls_async_invoke(pls::get_object(proxy), [proxy, object_ptrs, fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptrs)) {
				fn();
			}
		});
	}
}
template<typename T, typename Fn> void pls_sync_call(const pls::QObjectPtr<QObject> &proxy, const QSet<pls::QObjectPtr<T>> &object_ptrs, const pls::QObjectPtr<QObject> &object, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(object_ptrs) && object.valid()) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object_ptrs, object, fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptrs) && object.valid()) {
				fn();
			}
		});
	}
}
template<typename T, typename Fn> void pls_async_call(const pls::QObjectPtr<QObject> &proxy, const QSet<pls::QObjectPtr<T>> &object_ptrs, const pls::QObjectPtr<QObject> &object, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(object_ptrs) && object.valid()) {
		pls_async_invoke(pls::get_object(proxy), [proxy, object_ptrs, object, fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptrs) && object.valid()) {
				fn();
			}
		});
	}
}
template<typename T, typename Fn> void pls_sync_call(const pls::QObjectPtr<QObject> &proxy, const QSet<pls::QObjectPtr<T>> &object_ptrs, const QSet<const QObject *> &objects, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(object_ptrs) && pls_objects_is_valid(objects)) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object_ptrs, object_ptr_set = pls_qobject_ptr_set<QObject>(objects), fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptrs) && pls_objects_is_valid(object_ptr_set)) {
				fn();
			}
		});
	}
}
template<typename T, typename Fn> void pls_async_call(const pls::QObjectPtr<QObject> &proxy, const QSet<pls::QObjectPtr<T>> &object_ptrs, const QSet<const QObject *> &objects, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(object_ptrs) && pls_objects_is_valid(objects)) {
		pls_async_invoke(pls::get_object(proxy), [proxy, object_ptrs, object_ptr_set = pls_qobject_ptr_set<QObject>(objects), fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptrs) && pls_objects_is_valid(object_ptr_set)) {
				fn();
			}
		});
	}
}
template<typename T, typename IsValid, typename Fn>
void pls_sync_call(const pls::QObjectPtr<QObject> &proxy, const QSet<pls::QObjectPtr<T>> &object_ptrs, const pls::QObjectPtr<QObject> &object, const IsValid &is_valid, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(object_ptrs) && object.valid(is_valid)) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object_ptrs, object, is_valid, fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptrs) && object.valid(is_valid)) {
				fn();
			}
		});
	}
}
template<typename T, typename IsValid, typename Fn>
void pls_async_call(const pls::QObjectPtr<QObject> &proxy, const QSet<pls::QObjectPtr<T>> &object_ptrs, const pls::QObjectPtr<QObject> &object, const IsValid &is_valid, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(object_ptrs) && pls_object_is_valid(object, is_valid)) {
		pls_async_invoke(pls::get_object(proxy), [proxy, object_ptrs, object, is_valid, fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptrs) && object.valid(is_valid)) {
				fn();
			}
		});
	}
}
template<typename T, typename IsValid, typename Fn>
void pls_sync_call(const pls::QObjectPtr<QObject> &proxy, const QSet<pls::QObjectPtr<T>> &object_ptrs, const QSet<const QObject *> &objects, const IsValid &is_valid, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(object_ptrs) && pls_objects_is_valid(objects, is_valid)) {
		pls_sync_invoke(pls::get_object(proxy), [proxy, object_ptrs, object_ptr_set = pls_qobject_ptr_set<QObject>(objects), is_valid, fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptrs) && pls_objects_is_valid(object_ptr_set, is_valid)) {
				fn();
			}
		});
	}
}
template<typename T, typename IsValid, typename Fn>
void pls_async_call(const pls::QObjectPtr<QObject> &proxy, const QSet<pls::QObjectPtr<T>> &object_ptrs, const QSet<const QObject *> &objects, const IsValid &is_valid, const Fn &fn)
{
	if (proxy.valid() && pls_objects_is_valid(object_ptrs) && pls_objects_is_valid(objects, is_valid)) {
		pls_async_invoke(pls::get_object(proxy), [proxy, object_ptrs, object_ptr_set = pls_qobject_ptr_set<QObject>(objects), is_valid, fn]() {
			if (proxy.valid() && pls_objects_is_valid(object_ptrs) && pls_objects_is_valid(object_ptr_set, is_valid)) {
				fn();
			}
		});
	}
}

template<typename Fn> void pls_sync_call_mt(const Fn &fn)
{
	pls_sync_call(qApp, fn);
}
template<typename Fn> void pls_async_call_mt(const Fn &fn)
{
	pls_async_call(qApp, fn);
}
template<typename T, typename Fn> void pls_sync_call_mt(const pls::QObjectPtr<T> &object, const Fn &fn)
{
	pls_sync_call(qApp, object, fn);
}
template<typename T, typename Fn> void pls_sync_call_mt(const T *object, const Fn &fn)
{
	pls_sync_call(qApp, pls_qobject_ptr<T>(object), fn);
}
template<typename T, typename Fn> auto pls_async_call_mt(const pls::QObjectPtr<T> &object, const Fn &fn)
{
	pls_async_call(qApp, object, fn);
}
template<typename T, typename Fn> auto pls_async_call_mt(const T *object, const Fn &fn)
{
	pls_async_call(qApp, pls_qobject_ptr<T>(object), fn);
}
template<typename Fn> void pls_sync_call_mt(const QSet<const QObject *> &objects, const Fn &fn)
{
	pls_sync_call(qApp, objects, fn);
}
template<typename Fn> void pls_async_call_mt(const QSet<const QObject *> &objects, const Fn &fn)
{
	pls_async_call(qApp, objects, fn);
}
template<typename IsValid, typename Fn> void pls_sync_call_mt(const pls::QObjectPtr<QObject> &object, const IsValid &isValid, const Fn &fn)
{
	pls_sync_call(qApp, object, isValid, fn);
}
template<typename IsValid, typename Fn> void pls_async_call_mt(const pls::QObjectPtr<QObject> &object, const IsValid &isValid, const Fn &fn)
{
	pls_async_call(qApp, object, isValid, fn);
}
template<typename IsValid, typename Fn> void pls_sync_call_mt(const QSet<const QObject *> &objects, const IsValid &isValid, const Fn &fn)
{
	pls_sync_call(qApp, objects, isValid, fn);
}
template<typename IsValid, typename Fn> void pls_async_call_mt(const QSet<const QObject *> &objects, const IsValid &isValid, const Fn &fn)
{
	pls_async_call(qApp, objects, isValid, fn);
}
template<typename T, typename Fn> void pls_sync_call_mt(const QSet<pls::QObjectPtr<T>> &object_ptrs, const Fn &fn)
{
	pls_sync_call(qApp, object_ptrs, fn);
}
template<typename T, typename Fn> void pls_async_call_mt(const QSet<pls::QObjectPtr<T>> &object_ptrs, const Fn &fn)
{
	pls_async_call(qApp, object_ptrs, fn);
}
template<typename T, typename Fn> void pls_sync_call_mt(const QSet<pls::QObjectPtr<T>> &object_ptrs, const pls::QObjectPtr<QObject> &object, const Fn &fn)
{
	pls_sync_call(qApp, object_ptrs, object, fn);
}
template<typename T, typename Fn> void pls_async_call_mt(const QSet<pls::QObjectPtr<T>> &object_ptrs, const pls::QObjectPtr<QObject> &object, const Fn &fn)
{
	pls_async_call(qApp, object_ptrs, object, fn);
}
template<typename T, typename Fn> void pls_sync_call_mt(const QSet<pls::QObjectPtr<T>> &object_ptrs, const QSet<const QObject *> &objects, const Fn &fn)
{
	pls_sync_call(qApp, object_ptrs, objects, fn);
}
template<typename T, typename Fn> void pls_async_call_mt(const QSet<pls::QObjectPtr<T>> &object_ptrs, const QSet<const QObject *> &objects, const Fn &fn)
{
	pls_async_call(qApp, object_ptrs, objects, fn);
}
template<typename T, typename IsValid, typename Fn> void pls_sync_call_mt(const QSet<pls::QObjectPtr<T>> &object_ptrs, const pls::QObjectPtr<QObject> &object, const IsValid &isValid, const Fn &fn)
{
	pls_sync_call(qApp, object_ptrs, object, isValid, fn);
}
template<typename T, typename IsValid, typename Fn> void pls_async_call_mt(const QSet<pls::QObjectPtr<T>> &object_ptrs, const pls::QObjectPtr<QObject> &object, const IsValid &isValid, const Fn &fn)
{
	pls_async_call(qApp, object_ptrs, object, isValid, fn);
}
template<typename T, typename IsValid, typename Fn> void pls_sync_call_mt(const QSet<pls::QObjectPtr<T>> &object_ptrs, const QSet<const QObject *> &objects, const IsValid &isValid, const Fn &fn)
{
	pls_sync_call(qApp, object_ptrs, objects, isValid, fn);
}
template<typename T, typename IsValid, typename Fn> void pls_async_call_mt(const QSet<pls::QObjectPtr<T>> &object_ptrs, const QSet<const QObject *> &objects, const IsValid &isValid, const Fn &fn)
{
	pls_async_call(qApp, object_ptrs, objects, isValid, fn);
}

template<typename K, typename V, typename Fn> void pls_for_each(const QHash<K, V> &hash, Fn fn)
{
	for (auto iter = hash.begin(), end_iter = hash.end(); iter != end_iter; ++iter) {
		pls_invoke(fn, iter.key(), iter.value());
	}
}
template<typename K, typename V, typename Fn> void pls_for_each(const QMap<K, V> &map, Fn fn)
{
	for (auto iter = map.begin(), end_iter = map.end(); iter != end_iter; ++iter) {
		pls_invoke(fn, iter.key(), iter.value());
	}
}
template<typename T, typename Fn> void pls_for_each(const QList<T> &list, Fn fn)
{
	for (const auto &v : list) {
		pls_invoke(fn, v);
	}
}
template<typename T, typename Fn> void pls_for_each(const QSet<T> &set, Fn fn)
{
	for (const auto &v : set) {
		pls_invoke(fn, v);
	}
}

template<typename T> inline T pls_conditional_select(bool condition, T a, T b)
{
	return condition ? a : b;
}
template<typename R, typename T> inline R pls_conditional_select(bool condition, T a, T b)
{
	return condition ? a : b;
}

inline bool pls_is_empty(const QString &str)
{
	return str.isEmpty();
}
template<typename T> inline bool pls_is_not_empty(const T &str)
{
	return !pls_is_empty(str);
}
inline QString pls_add_quotes(const QString &str)
{
	if (str.startsWith('"') && str.endsWith('"')) {
		return str;
	}

	return '"' + str + '"';
}
inline QString pls_remove_quotes(const QString &str)
{
	if (!str.startsWith('"') || !str.endsWith('"')) {
		return str;
	}

	return str.mid(1, str.length() - 2);
}
template<typename T> auto pls_unique(const QList<T> &list) -> QList<T>
{
	QList<T> result;
	for (const auto &v : list) {
		if (!result.contains(v)) {
			result.append(v);
		}
	}
	return result;
}
template<typename T, typename Fn> auto pls_filter(const QList<T> &list, Fn fn) -> QList<std::enable_if_t<std::is_same_v<bool, std::invoke_result_t<Fn, T>>, T>>
{
	QList<T> result;
	for (const auto &v : list) {
		if (fn(v)) {
			result.append(v);
		}
	}
	return result;
}
template<typename T, typename Fn> auto pls_map(const QList<T> &list, Fn fn) -> QList<std::invoke_result_t<Fn, T>>
{
	QList<std::invoke_result_t<Fn, T>> result;
	for (const auto &v : list) {
		result.append(fn(v));
	}
	return result;
}

struct pls_list_t {
	pls_list_t *prev;
	pls_list_t *next;
};
using pls_list_item_t = pls_list_t;
inline void pls_list_init(pls_list_t *list)
{
	list->next = list->prev = list;
}
inline void pls_list_item_init(pls_list_item_t *list_item)
{
	list_item->next = list_item->prev = nullptr;
}
inline bool pls_list_empty(const pls_list_t *list)
{
	return list->next == list;
}
inline void __list_add(pls_list_t *prev, pls_list_t *next, pls_list_item_t *new_one)
{
	prev->next = new_one;
	new_one->prev = prev;
	next->prev = new_one;
	new_one->next = next;
}
inline void __list_remove(pls_list_item_t *new_one)
{
	new_one->next->prev = new_one->prev;
	new_one->prev->next = new_one->next;
	new_one->prev = new_one->next = nullptr;
}
inline pls_list_item_t *pls_list_front(pls_list_t *list)
{
	if (!pls_list_empty(list))
		return list->next;
	return nullptr;
}
inline void pls_list_push_front(pls_list_t *list, pls_list_item_t *new_one)
{
	__list_add(list, list->next, new_one);
}
inline void pls_list_pop_front(pls_list_t *list)
{
	__list_remove(list->next);
}
inline pls_list_item_t *pls_list_back(pls_list_t *list)
{
	if (!pls_list_empty(list))
		return list->prev;
	return nullptr;
}
inline void pls_list_push_back(pls_list_t *list, pls_list_item_t *new_one)
{
	__list_add(list->prev, list, new_one);
}
inline void pls_list_pop_back(pls_list_t *list)
{
	__list_remove(list->prev);
}

using pls_queue_t = pls_list_t;
using pls_queue_item_t = pls_list_item_t;
inline void pls_queue_init(pls_queue_t *queue)
{
	pls_list_init(queue);
}
inline bool pls_queue_empty(const pls_queue_t *queue)
{
	return pls_list_empty(queue);
}
inline pls_queue_item_t *pls_queue_top(pls_queue_t *queue)
{
	return pls_list_front(queue);
}
inline void pls_queue_push(pls_queue_t *queue, pls_queue_item_t *new_one)
{
	pls_list_push_back(queue, new_one);
}
inline pls_queue_item_t *pls_queue_pop(pls_queue_t *queue)
{
	pls_queue_item_t *item = pls_list_front(queue);
	pls_list_pop_front(queue);
	return item;
}
inline bool pls_queue_pop(pls_queue_t *queue, pls_queue_item_t *item)
{
	if (pls_list_front(queue) == item) {
		pls_list_pop_front(queue);
		return true;
	}
	return false;
}

struct pls_locked_queue_t {
	mutable std::mutex lock;
	pls_queue_t queue;
	pls_locked_queue_t() { pls_queue_init(&queue); }
};
inline bool pls_locked_queue_empty(const pls_locked_queue_t *locked_queue)
{
	std::lock_guard guard(locked_queue->lock);
	return pls_queue_empty(&locked_queue->queue);
}
inline pls_queue_item_t *pls_locked_queue_top(pls_locked_queue_t *locked_queue)
{
	std::lock_guard guard(locked_queue->lock);
	return pls_queue_top(&locked_queue->queue);
}
inline void pls_locked_queue_push(pls_locked_queue_t *locked_queue, pls_queue_item_t *new_one)
{
	std::lock_guard guard(locked_queue->lock);
	pls_queue_push(&locked_queue->queue, new_one);
}
inline pls_queue_item_t *pls_locked_queue_pop(pls_locked_queue_t *locked_queue)
{
	std::lock_guard guard(locked_queue->lock);
	return pls_queue_pop(&locked_queue->queue);
}
inline bool pls_locked_queue_pop(pls_locked_queue_t *locked_queue, pls_queue_item_t *item)
{
	std::lock_guard guard(locked_queue->lock);
	return pls_queue_pop(&locked_queue->queue, item);
}

class pls_thread_t;
using pls_thread_entry_t = std::function<void(pls_thread_t *thread)>;
LIBUTILSAPI_API pls_thread_t *pls_thread_create(const pls_thread_entry_t &entry, bool delay_start = false);
LIBUTILSAPI_API void pls_thread_destroy(pls_thread_t *thread);
LIBUTILSAPI_API bool pls_thread_running(pls_thread_t *thread);
LIBUTILSAPI_API void pls_thread_start(pls_thread_t *thread);
LIBUTILSAPI_API void pls_thread_quit(pls_thread_t *thread);
LIBUTILSAPI_API bool pls_thread_joinable(pls_thread_t *thread);
LIBUTILSAPI_API void pls_thread_join(pls_thread_t *thread);
LIBUTILSAPI_API bool pls_thread_queue_empty(const pls_thread_t *thread);
LIBUTILSAPI_API pls_queue_item_t *pls_thread_queue_top(pls_thread_t *thread);
LIBUTILSAPI_API void pls_thread_queue_push(pls_thread_t *thread, pls_queue_item_t *new_one);
LIBUTILSAPI_API pls_queue_item_t *pls_thread_queue_pop(pls_thread_t *thread);
LIBUTILSAPI_API pls_queue_item_t *pls_thread_queue_try_pop(pls_thread_t *thread);

template<typename T, typename V> inline void pls_set_value(T *ptr, const V &value)
{
	if (ptr) {
		*ptr = value;
	}
}

#endif // _PRISM_COMMON_LIBUTILSAPI_LIBUTILSAPI_H
