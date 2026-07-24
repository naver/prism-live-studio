#include "libutils-api.h"

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <Shlwapi.h>
#include <Psapi.h>
#include <TlHelp32.h>
#include <shlobj.h>
#elif defined(Q_OS_MACOS)
#include <malloc/malloc.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <pthread.h>
#include <notify.h>
#endif

#include <array>
#include <iostream>
#include <shared_mutex>
#include <set>
#include <thread>

#include <qfileinfo.h>
#include <qdir.h>
#include <private/qhooks_p.h>
#include <qcoreapplication.h>
#include <quuid.h>
#include <qstandardpaths.h>
#include <qsettings.h>
#include <qprocess.h>
#include <qnetworkinterface.h>
#include <qmetaobject.h>
#include <qsharedmemory.h>
#include <qsystemsemaphore.h>
#include <qsemaphore.h>
#include <qdebug.h>
#include <qhostinfo.h>
#include <qhostaddress.h>
#include <regex>
#include <qlocalserver.h>
#include <qlocalsocket.h>
#include <qbuffer.h>
#include <qstack.h>

#include "network-state.h"
#include "libutils-api-log.h"

#if defined(Q_OS_WIN)
#elif defined(Q_OS_MACOS)
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#else
#include <semaphore.h>
#endif

#if defined(Q_OS_WIN)
#pragma comment(lib, "Shlwapi.lib")
#endif
#include "pls-shared-values.h"

namespace pls {
bool network_state_start();
void network_state_stop();

const char *KIdentifier_PRISM = "com.prismlive.prismlivestudio";
const char *KIdentifier_LENS = "com.prismlive.camstudio";
}

namespace {
constexpr auto UTILS_API_MODULE = "libutils-api";

#define g_hook hook_t::s_hook

struct LocalGlobalVars {
	static int g_argc;
	static char **g_argv;
	static QStringList g_cmdline_args;
	static std::optional<bool> g_is_dev;
	static std::optional<bool> g_is_local_log;
	static std::atomic<uint64_t> g_app_exiting;
	static qulonglong g_qobject_id;
	static uint64_t g_prism_version;
	static std::optional<pls_product_type_t> g_product_type;
	static std::optional<pls_process_type_t> g_process_type;
	static thread_local uint32_t g_last_error;
	static QString g_gcc;
	static QString g_user_id;
	static QString g_hash_user_id;
	static QString g_prism_session;
	static QString g_prism_subsession;
	static QList<QPointer<QThread>> g_async_invoke_threads;
	static std::set<pls_config_t *> g_configs;
	static pls_config_t *g_app_config;
	static pls_config_t *g_user_config;
	static QMap<QString /* en-US, ko-KR, pt-BR, ... */, QMap<pls_locale_attr_t, QVariant>> g_locales;
	static std::optional<QString> g_locale;
};
int LocalGlobalVars::g_argc = 0;
char **LocalGlobalVars::g_argv = nullptr;
QStringList LocalGlobalVars::g_cmdline_args;
std::optional<bool> LocalGlobalVars::g_is_dev = std::nullopt;
std::optional<bool> LocalGlobalVars::g_is_local_log = std::nullopt;
std::atomic<uint64_t> LocalGlobalVars::g_app_exiting = 0;
qulonglong LocalGlobalVars::g_qobject_id = 1;
uint64_t LocalGlobalVars::g_prism_version = 0;
std::optional<pls_product_type_t> LocalGlobalVars::g_product_type = std::nullopt;
std::optional<pls_process_type_t> LocalGlobalVars::g_process_type = std::nullopt;
thread_local uint32_t LocalGlobalVars::g_last_error = 0;
QString LocalGlobalVars::g_gcc;
QString LocalGlobalVars::g_user_id;
QString LocalGlobalVars::g_hash_user_id;
QString LocalGlobalVars::g_prism_session;
QString LocalGlobalVars::g_prism_subsession;
QList<QPointer<QThread>> LocalGlobalVars::g_async_invoke_threads;
std::set<pls_config_t *> LocalGlobalVars::g_configs;
pls_config_t *LocalGlobalVars::g_app_config = nullptr;
pls_config_t *LocalGlobalVars::g_user_config = nullptr;
QMap<QString /* en-US, ko-KR, pt-BR, ... */, QMap<pls_locale_attr_t, QVariant>> LocalGlobalVars::g_locales;
std::optional<QString> LocalGlobalVars::g_locale = std::nullopt;

class object_pool_t {
	using getters = std::map<QByteArray, pls_getter_t>;
	using setters = std::map<QByteArray, pls_setter_t>;
	using objects = std::map<const QObject *, std::tuple<qulonglong, getters, setters>>;
	mutable std::shared_mutex m_objects_mutex;
	objects m_objects;

public:
	bool contains(const QObject *object) const
	{
		std::shared_lock locker(m_objects_mutex);
		return m_objects.find(object) != m_objects.end();
	}
	qulonglong get_qobject_id(const QObject *object) const
	{
		std::shared_lock locker(m_objects_mutex);
		if (auto iter = m_objects.find(object); iter != m_objects.end())
			return std::get<0>(iter->second);
		return 0;
	}

	template<typename Fn> static Fn get_gs(const std::map<QByteArray, Fn> &gs, const QByteArray &name)
	{
		if (auto iter = gs.find(name); iter != gs.end())
			return iter->second;
		return nullptr;
	}
	pls_getter_t get_getter(const QObject *object, const QByteArray &name)
	{
		std::shared_lock locker(m_objects_mutex);
		if (auto iter = m_objects.find(object); iter != m_objects.end())
			return get_gs(std::get<1>(iter->second), name);
		return nullptr;
	}
	bool add_getter(const QObject *object, const QByteArray &name, const pls_getter_t &getter)
	{
		std::unique_lock locker(m_objects_mutex);
		if (auto iter = m_objects.find(object); iter != m_objects.end()) {
			std::get<1>(iter->second)[name] = getter;
			return true;
		}
		return false;
	}
	void remove_getter(const QObject *object, const QByteArray &name)
	{
		std::unique_lock locker(m_objects_mutex);
		if (auto iter = m_objects.find(object); iter != m_objects.end())
			std::get<1>(iter->second).erase(name);
	}
	pls_setter_t get_setter(const QObject *object, const QByteArray &name)
	{
		std::shared_lock locker(m_objects_mutex);
		if (auto iter = m_objects.find(object); iter != m_objects.end())
			return get_gs(std::get<2>(iter->second), name);
		return nullptr;
	}
	bool add_setter(const QObject *object, const QByteArray &name, const pls_setter_t &setter)
	{
		std::unique_lock locker(m_objects_mutex);
		if (auto iter = m_objects.find(object); iter != m_objects.end()) {
			std::get<2>(iter->second)[name] = setter;
			return true;
		}
		return false;
	}
	void remove_setter(const QObject *object, const QByteArray &name)
	{
		std::unique_lock locker(m_objects_mutex);
		if (auto iter = m_objects.find(object); iter != m_objects.end())
			std::get<2>(iter->second).erase(name);
	}
	void add(QObject *object)
	{
		std::unique_lock locker(m_objects_mutex);
		qulonglong object_id = LocalGlobalVars::g_qobject_id++;
		pls_abort_assert(m_objects.insert(objects::value_type(object, std::make_tuple(object_id, getters(), setters()))).second);
	}
	void remove(const QObject *object)
	{
		std::unique_lock locker(m_objects_mutex);
		m_objects.erase(object);
	}
};

object_pool_t *object_pool()
{
	static object_pool_t s_object_pool;
	return &s_object_pool;
}

class hook_t {
public:
	hook_t()
	{
		qtHookData[QHooks::AddQObject] = (quintptr)(void *)&add_qobject;
		qtHookData[QHooks::RemoveQObject] = (quintptr)(void *)&remove_qobject;
	}
	~hook_t()
	{
		qtHookData[QHooks::AddQObject] = 0;
		qtHookData[QHooks::RemoveQObject] = 0;
	}

	static const hook_t s_hook;

	void call_qapp_cbs(const std::list<pls_qapp_cb_t> &qapp_cbs) const
	{
		std::shared_lock locker(m_qapp_cbs_mutex);
		std::for_each(qapp_cbs.begin(), qapp_cbs.end(), [](const auto &qapp_cb) { pls_invoke_safe(qapp_cb); });
	}
	void call_qapp_cbs_r(const std::list<pls_qapp_cb_t> &qapp_cbs) const
	{
		std::shared_lock locker(m_qapp_cbs_mutex);
		std::for_each(qapp_cbs.rbegin(), qapp_cbs.rend(), [](const auto &qapp_cb) { pls_invoke_safe(qapp_cb); });
	}
	void add_qapp_cbs(std::list<pls_qapp_cb_t> &qapp_cbs, const pls_qapp_cb_t &qapp_cb) const
	{
		std::unique_lock locker(m_qapp_cbs_mutex);
		qapp_cbs.push_back(qapp_cb);
	}

	static void add_qobject(QObject *object) { object_pool()->add(object); }
	static void remove_qobject(const QObject *object) { object_pool()->remove(object); }

	mutable std::shared_mutex m_qapp_cbs_mutex;
	mutable std::list<pls_qapp_cb_t> m_qapp_construct_cbs;
	mutable std::list<pls_qapp_cb_t> m_qapp_deconstruct_cbs;
};

const hook_t hook_t::s_hook;

void destroy_async_invoke_threads()
{
	while (!LocalGlobalVars::g_async_invoke_threads.isEmpty())
		pls_delete_thread(LocalGlobalVars::g_async_invoke_threads.takeLast());
}
std::shared_mutex &global_shared_mutex()
{
	static std::shared_mutex global_shared_mutex;
	return global_shared_mutex;
}
QString get_product_name()
{
	switch (pls_product_type()) {
	case pls_product_type_t::Prism:
		return QStringLiteral("PRISM Live Studio");
	case pls_product_type_t::Lens:
		return QStringLiteral("PRISM Lens");
	case pls_product_type_t::Installer:
		return QStringLiteral("PRISM Installer");
	default:
		return QString();
	}
}
QString get_user_data_product_name()
{
	switch (pls_product_type()) {
	case pls_product_type_t::Prism:
		return QStringLiteral("PRISMLiveStudio");
	case pls_product_type_t::Lens:
		return QStringLiteral("PRISMLens");
	case pls_product_type_t::Installer:
		return QStringLiteral("PRISMInstaller");
	default:
		return QString();
	}
}
QString get_qsetting_product_name(pls_product_type_t product_type)
{
	switch (product_type) {
	case pls_product_type_t::Prism:
#if defined(Q_OS_WIN)
		return QStringLiteral("Prism Live Studio");
#elif defined(Q_OS_MACOS)
		return QStringLiteral("prismlivestudio");
#endif
	case pls_product_type_t::Lens:
#if defined(Q_OS_WIN)
		return QStringLiteral("PRISM Lens");
#elif defined(Q_OS_MACOS)
		return QStringLiteral("camstudio");
#endif
	case pls_product_type_t::Installer:
#if defined(Q_OS_WIN)
		return QStringLiteral("PRISM Installer");
#elif defined(Q_OS_MACOS)
		return QString();
#endif
	default:
		return QString();
	}
}

}

LIBUTILSAPI_API std::recursive_mutex &pls_global_mutex()
{
	static std::recursive_mutex global_mutex;
	return global_mutex;
}

LIBUTILSAPI_API pls_product_type_t pls_product_type()
{
	if (!LocalGlobalVars::g_product_type) {
		std::lock_guard guard(pls_global_mutex());
		if (!LocalGlobalVars::g_product_type) {
#if defined(Q_OS_WIN)
			if (QDir app_dir(pls_get_app_dir()); app_dir.exists(QStringLiteral("PRISMLiveStudio.exe")) || app_dir.exists(QStringLiteral("PrismSetup.exe")))
				LocalGlobalVars::g_product_type = pls_product_type_t::Prism;
			else if (app_dir.exists(QStringLiteral("PRISMLens.exe")) || app_dir.exists(QStringLiteral("PrismLensSetup.exe")))
				LocalGlobalVars::g_product_type = pls_product_type_t::Lens;
			else if (app_dir.exists(QStringLiteral("PRISMInstaller.exe")))
				LocalGlobalVars::g_product_type = pls_product_type_t::Installer;
			else
				LocalGlobalVars::g_product_type = pls_product_type_t::Unknown;
#elif defined(Q_OS_MACOS)
			if (QDir app_dir(pls_get_app_dir()); app_dir.exists(QStringLiteral("PRISMLiveStudio")))
				LocalGlobalVars::g_product_type = pls_product_type_t::Prism;
			else if (app_dir.exists(QStringLiteral("PRISMLens")))
				LocalGlobalVars::g_product_type = pls_product_type_t::Lens;
			else if (app_dir.exists(QStringLiteral("PRISMInstaller")))
				LocalGlobalVars::g_product_type = pls_product_type_t::Installer;
			else
				LocalGlobalVars::g_product_type = pls_product_type_t::Unknown;
#endif
		}
	}
	return LocalGlobalVars::g_product_type.value();
}
LIBUTILSAPI_API pls_process_type_t pls_process_type()
{
	if (!LocalGlobalVars::g_process_type) {
		std::lock_guard guard(pls_global_mutex());
		if (!LocalGlobalVars::g_process_type) {
#if defined(Q_OS_WIN)
			if (auto pn = pls_get_app_pn(); pn == QStringLiteral("PRISMLiveStudio.exe"))
				LocalGlobalVars::g_process_type = pls_process_type_t::Daemon;
			else if (pn == QStringLiteral("obs64.exe"))
				LocalGlobalVars::g_process_type = pls_process_type_t::Prism;
			else if (pn == QStringLiteral("PRISMLens.exe"))
				LocalGlobalVars::g_process_type = pls_process_type_t::Lens;
			else if (pn == QStringLiteral("PRISMLogger.exe"))
				LocalGlobalVars::g_process_type = pls_process_type_t::Logger;
			else if (pn == QStringLiteral("PRISMDaemon.exe"))
				LocalGlobalVars::g_process_type = pls_process_type_t::Daemon;
			else if (pn == QStringLiteral("PRISMInstaller.exe"))
				LocalGlobalVars::g_process_type = pls_process_type_t::Installer;
			else
				LocalGlobalVars::g_process_type = pls_process_type_t::Unknown;
#elif defined(Q_OS_MACOS)
			if (auto pn = pls_get_app_pn(); pn == QStringLiteral("PRISMLiveStudio"))
				LocalGlobalVars::g_process_type = pls_process_type_t::Prism;
			else if (pn == QStringLiteral("PRISMLens"))
				LocalGlobalVars::g_process_type = pls_process_type_t::Lens;
			else if (pn == QStringLiteral("PRISMLogger"))
				LocalGlobalVars::g_process_type = pls_process_type_t::Logger;
			else if (pn == QStringLiteral("PRISMDaemon"))
				LocalGlobalVars::g_process_type = pls_process_type_t::Daemon;
			else if (pn == QStringLiteral("PRISMInstaller"))
				LocalGlobalVars::g_process_type = pls_process_type_t::Installer;
			else
				LocalGlobalVars::g_process_type = pls_process_type_t::Unknown;
#endif
		}
	}
	return LocalGlobalVars::g_process_type.value();
}

LIBUTILSAPI_API uint32_t pls_last_error()
{
	return LocalGlobalVars::g_last_error;
}
LIBUTILSAPI_API void pls_set_last_error(uint32_t last_error)
{
	LocalGlobalVars::g_last_error = last_error;
}

LIBUTILSAPI_API QString pls_get_gcc()
{
	std::shared_lock guard(global_shared_mutex());
	if (!LocalGlobalVars::g_gcc.isEmpty())
		return LocalGlobalVars::g_gcc;
	else if (auto gcc = pls_get_qsetting_value(QStringLiteral("GCC")).toString(); !gcc.isEmpty())
		return gcc;
	return QStringLiteral("KR");
}
LIBUTILSAPI_API void pls_set_gcc(const QString &gcc)
{
	std::unique_lock guard(global_shared_mutex());
	LocalGlobalVars::g_gcc = gcc;
	pls_set_qsetting_value(QStringLiteral("GCC"), gcc);
}

LIBUTILSAPI_API QString pls_get_user_id()
{
	std::shared_lock guard(global_shared_mutex());
	return LocalGlobalVars::g_user_id;
}
LIBUTILSAPI_API void pls_set_user_id(const QString &user_id)
{
	std::unique_lock guard(global_shared_mutex());
	LocalGlobalVars::g_user_id = user_id;
}
LIBUTILSAPI_API QString pls_get_hash_user_id()
{
	std::shared_lock guard(global_shared_mutex());
	return LocalGlobalVars::g_hash_user_id;
}
LIBUTILSAPI_API void pls_set_hash_user_id(const QString &hash_user_id)
{
	std::unique_lock guard(global_shared_mutex());
	LocalGlobalVars::g_hash_user_id = hash_user_id;
}

LIBUTILSAPI_API QString pls_get_prism_session()
{
	std::shared_lock guard(global_shared_mutex());
	return LocalGlobalVars::g_prism_session;
}
LIBUTILSAPI_API void pls_set_prism_session(const QString &prism_session)
{
	std::unique_lock guard(global_shared_mutex());
	LocalGlobalVars::g_prism_session = prism_session;
}
LIBUTILSAPI_API QString pls_get_prism_subsession()
{
	std::shared_lock guard(global_shared_mutex());
	return LocalGlobalVars::g_prism_subsession;
}
LIBUTILSAPI_API void pls_set_prism_subsession(const QString &prism_subsession)
{
	std::unique_lock guard(global_shared_mutex());
	LocalGlobalVars::g_prism_subsession = prism_subsession;
}

LIBUTILSAPI_API bool pls_object_is_valid(const QObject *object)
{
	if (object) {
		return object_pool()->contains(object);
	}
	return false;
}
LIBUTILSAPI_API bool pls_object_is_valid(const QObject *object, const std::function<bool(const QObject *object)> &is_valid)
{
	if (!pls_object_is_valid(object)) {
		return false;
	} else if (!is_valid || is_valid(object)) {
		return true;
	}
	return false;
}
LIBUTILSAPI_API void pls_object_remove(const QObject *object)
{
	if (object) {
		object_pool()->remove(object);
	}
}

bool pls_mkdir(const QDir &dir)
{
	if (!dir.exists()) {
		return dir.mkpath(dir.absolutePath());
	}
	return true;
}
LIBUTILSAPI_API bool pls_open_file(std::optional<QFile> &file, const QString &file_path, QFile::OpenMode mode)
{
	QString error;
	return pls_open_file(file, file_path, mode, error);
}
LIBUTILSAPI_API bool pls_open_file(std::optional<QFile> &file, const QString &file_path, QFile::OpenMode mode, QString &error)
{
	file.reset();

	if (file_path.isEmpty()) {
		error = "file path is empty";
		return false;
	}

	if (QDir dir = QFileInfo(file_path).dir(); !pls_mkdir(dir)) {
		error = QString("failed to create folder, dir: %1").arg(dir.absolutePath());
		return false;
	}

	file.emplace(file_path);
	if (!file.value().open(mode)) {
		error = QString("fail to open the file, file path: %1, reason: %2").arg(file_path, file.value().errorString());
		file.reset();
		return false;
	}
	return true;
}
LIBUTILSAPI_API bool pls_mkdir(const QString &dir_path)
{
	return pls_mkdir(QDir(dir_path));
}

LIBUTILSAPI_API bool pls_mkfiledir(const QString &file_path)
{
	return pls_mkdir(QFileInfo(file_path).dir());
}

LIBUTILSAPI_API QVariantHash pls_map_to_hash(const QMap<QString, QString> &map)
{
	QVariantHash hash;
	pls_for_each(map, [&hash](const auto &key, const auto &value) { hash.insert(key, value); });
	return hash;
}

LIBUTILSAPI_API QMap<QString, QString> pls_hash_to_map(const QVariantHash &hash)
{
	QMap<QString, QString> map;
	pls_for_each(hash, [&map](const auto &key, const auto &value) { map.insert(key, value.toString()); });
	return map;
}

LIBUTILSAPI_API QByteArray pls_read_data(const QString &file_path, QString *error)
{
	if (QByteArray data; pls_read_data(data, file_path, error)) {
		return data;
	}
	return {};
}
LIBUTILSAPI_API bool pls_read_data(QByteArray &data, const QString &file_path, QString *error)
{
	if (QFile file(file_path); file.open(QFile::ReadOnly)) {
		data = file.readAll();
		return true;
	} else {
		pls_set_value(error, file.errorString());
		return false;
	}
}
LIBUTILSAPI_API bool pls_write_data(const QString &file_path, const QByteArray &data, QString *error)
{
	if (!pls_mkfiledir(file_path)) {
		pls_set_value(error, QStringLiteral("create file directory failed"));
		return false;
	} else if (QFile file(file_path); file.open(QFile::WriteOnly)) {
		file.write(data);
		return true;
	} else {
		pls_set_value(error, file.errorString());
		return false;
	}
}
LIBUTILSAPI_API bool pls_read_cbor(QCborValue &cbor, const QString &file_path, QString *error)
{
	QByteArray bytes;
	if (!pls_read_data(bytes, file_path, error)) {
		return false;
	}

	QCborParserError cborError;
	cbor = QCborValue::fromCbor(bytes, &cborError);
	if (cborError.error == QCborError::NoError) {
		return true;
	} else {
		pls_set_value(error, cborError.errorString());
		return false;
	}
}
LIBUTILSAPI_API bool pls_write_cbor(const QString &file_path, QCborValue &cbor, QString *error)
{
	return pls_write_data(file_path, cbor.toCbor(), error);
}
LIBUTILSAPI_API QByteArray pls_remove_utf8_bom(const QByteArray &utf8)
{
	if (utf8.length() < 3) {
		return utf8;
	} else if (auto data = (const quint8 *)utf8.data(); data[0] == quint8(0xEF) && data[1] == quint8(0xBB) && data[2] == quint8(0xBF)) {
		return utf8.mid(3);
	}
	return utf8;
}

LIBUTILSAPI_API void pls_set_cmdline_args(int argc, char *argv[])
{
	LocalGlobalVars::g_argc = argc;
	LocalGlobalVars::g_argv = argv;
	LocalGlobalVars::g_cmdline_args.clear();
#if defined(Q_OS_WIN)
	auto cmdline = ::GetCommandLineW();

	int wargc = 0;
	auto wargv = ::CommandLineToArgvW(cmdline, &wargc);
	for (int i = 0; i < wargc; ++i) {
		auto arg = QString::fromWCharArray(wargv[i]);
		PLS_INFO("cmdline", "cmd = %s", arg.toUtf8().constData());
		LocalGlobalVars::g_cmdline_args.append(arg);
	}
	::LocalFree(wargv);
#else
	for (int i = 0; i < argc; ++i) {
		PLS_INFO("cmdline", "cmd = %s", argv[i]);
		LocalGlobalVars::g_cmdline_args.append(QString::fromLocal8Bit(argv[i]));
	}
#endif
}
LIBUTILSAPI_API pls_cmdline_args_t pls_get_cmdline_args()
{
	return {LocalGlobalVars::g_argc, LocalGlobalVars::g_argv};
}
LIBUTILSAPI_API QStringList pls_cmdline_args()
{
	return LocalGlobalVars::g_cmdline_args;
}

//xxx=?
LIBUTILSAPI_API std::optional<const char *> pls_cmdline_get_arg(int argc, char **argv, const char *name)
{
	auto nameLength = strlen(name);
	for (int i = 1; i < argc; ++i) {
		if (!strncmp(argv[i], name, nameLength)) {
			return argv[i] + nameLength;
		}
	}
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<QString> pls_cmdline_get_arg(const QStringList &args, const QString &name)
{
	for (const QString &arg : args) {
		if (arg.startsWith(name)) {
			return arg.mid(name.length());
		}
	}
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<qint16> pls_cmdline_get_int16_arg(const QStringList &args, const QString &name)
{
	if (auto arg = pls_cmdline_get_arg(args, name); arg.has_value()) {
		return arg.value().toShort();
	}
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<quint16> pls_cmdline_get_uint16_arg(const QStringList &args, const QString &name)
{
	if (auto arg = pls_cmdline_get_arg(args, name); arg.has_value()) {
		return arg.value().toUShort();
	}
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<qint32> pls_cmdline_get_int32_arg(const QStringList &args, const QString &name)
{
	if (auto arg = pls_cmdline_get_arg(args, name); arg.has_value()) {
		return arg.value().toInt();
	}
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<quint32> pls_cmdline_get_uint32_arg(const QStringList &args, const QString &name)
{
	if (auto arg = pls_cmdline_get_arg(args, name); arg.has_value()) {
		return arg.value().toUInt();
	}
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<qint64> pls_cmdline_get_int64_arg(const QStringList &args, const QString &name)
{
	if (auto arg = pls_cmdline_get_arg(args, name); arg.has_value()) {
		return arg.value().toLongLong();
	}
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<quint64> pls_cmdline_get_uint64_arg(const QStringList &args, const QString &name)
{
	if (auto arg = pls_cmdline_get_arg(args, name); arg.has_value()) {
		return arg.value().toULongLong();
	}
	return std::nullopt;
}

LIBUTILSAPI_API bool pls_read_json(QJsonDocument &doc, const QString &file_path, QString *error)
{
	if (QByteArray bytes; !pls_read_data(bytes, file_path, error))
		return false;
	else if (!pls_parse_json(doc, bytes = pls_remove_utf8_bom(bytes), error))
		return false;
	return true;
}
LIBUTILSAPI_API bool pls_read_json(QJsonArray &array, const QString &file_path, QString *error)
{
	if (QJsonDocument doc; !pls_read_json(doc, file_path, error)) {
		return false;
	} else if (doc.isArray()) {
		array = doc.array();
		return true;
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}
LIBUTILSAPI_API bool pls_read_json(QVariantList &list, const QString &file_path, QString *error)
{
	if (QJsonArray array; pls_read_json(array, file_path, error)) {
		list = array.toVariantList();
		return true;
	}
	return false;
}
LIBUTILSAPI_API bool pls_read_json(QJsonObject &object, const QString &file_path, QString *error)
{
	if (QJsonDocument doc; !pls_read_json(doc, file_path, error)) {
		return false;
	} else if (doc.isObject()) {
		object = doc.object();
		return true;
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}
LIBUTILSAPI_API bool pls_read_json(QVariantMap &map, const QString &file_path, QString *error)
{
	if (QJsonObject object; pls_read_json(object, file_path, error)) {
		map = object.toVariantMap();
		return true;
	}
	return false;
}
LIBUTILSAPI_API bool pls_read_json(QVariantHash &hash, const QString &file_path, QString *error)
{
	if (QJsonObject object; pls_read_json(object, file_path, error)) {
		hash = object.toVariantHash();
		return true;
	}
	return false;
}

LIBUTILSAPI_API bool pls_write_json(const QString &file_path, const QJsonDocument &doc, QString *error)
{
	return pls_write_data(file_path, doc.toJson(), error);
}
LIBUTILSAPI_API bool pls_write_json(const QString &file_path, const QJsonArray &array, QString *error)
{
	return pls_write_json(file_path, QJsonDocument(array), error);
}

LIBUTILSAPI_API bool pls_write_json(const QString &file_path, const QVariantList &list, QString *error)
{
	return pls_write_json(file_path, QJsonArray::fromVariantList(list), error);
}
LIBUTILSAPI_API bool pls_write_json(const QString &file_path, const QJsonObject &object, QString *error)
{
	return pls_write_json(file_path, QJsonDocument(object), error);
}
LIBUTILSAPI_API bool pls_write_json(const QString &file_path, const QVariantMap &map, QString *error)
{
	return pls_write_json(file_path, QJsonObject::fromVariantMap(map), error);
}
LIBUTILSAPI_API bool pls_write_json(const QString &file_path, const QVariantHash &hash, QString *error)
{
	return pls_write_json(file_path, QJsonObject::fromVariantHash(hash), error);
}
LIBUTILSAPI_API bool pls_read_json_cbor(QJsonDocument &doc, const QString &file_path, QString *error)
{
	if (QCborValue cbor; !pls_read_cbor(cbor, file_path, error)) {
		return false;
	} else if (QJsonValue value = cbor.toJsonValue(); value.isObject()) {
		doc.setObject(value.toObject());
		return true;
	} else if (value.isArray()) {
		doc.setArray(value.toArray());
		return true;
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}
LIBUTILSAPI_API bool pls_read_json_cbor(QJsonArray &array, const QString &file_path, QString *error)
{
	if (QCborValue cbor; !pls_read_cbor(cbor, file_path, error)) {
		return false;
	} else if (QJsonValue value = cbor.toJsonValue(); value.isArray()) {
		array = value.toArray();
		return true;
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}
LIBUTILSAPI_API bool pls_read_json_cbor(QVariantList &list, const QString &file_path, QString *error)
{
	if (QJsonArray array; pls_read_json_cbor(array, file_path, error)) {
		list = array.toVariantList();
		return true;
	}
	return false;
}
LIBUTILSAPI_API bool pls_read_json_cbor(QJsonObject &object, const QString &file_path, QString *error)
{
	if (QCborValue cbor; !pls_read_cbor(cbor, file_path, error)) {
		return false;
	} else if (QJsonValue value = cbor.toJsonValue(); value.isObject()) {
		object = value.toObject();
		return true;
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}
LIBUTILSAPI_API bool pls_read_json_cbor(QVariantMap &map, const QString &file_path, QString *error)
{
	if (QJsonObject object; pls_read_json_cbor(object, file_path, error)) {
		map = object.toVariantMap();
		return true;
	}
	return false;
}
LIBUTILSAPI_API bool pls_read_json_cbor(QVariantHash &hash, const QString &file_path, QString *error)
{
	if (QJsonObject object; pls_read_json_cbor(object, file_path, error)) {
		hash = object.toVariantHash();
		return true;
	}
	return false;
}
LIBUTILSAPI_API bool pls_write_json_cbor(const QString &file_path, const QJsonDocument &doc, QString *error)
{
	if (doc.isObject()) {
		return pls_write_json_cbor(file_path, doc.object(), error);
	} else if (doc.isArray()) {
		return pls_write_json_cbor(file_path, doc.array(), error);
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}
LIBUTILSAPI_API bool pls_write_json_cbor(const QString &file_path, const QJsonArray &array, QString *error)
{
	QCborValue cbor = QCborValue::fromJsonValue(array);
	return pls_write_cbor(file_path, cbor, error);
}
LIBUTILSAPI_API bool pls_write_json_cbor(const QString &file_path, const QVariantList &list, QString *error)
{
	return pls_write_json_cbor(file_path, QJsonArray::fromVariantList(list), error);
}
LIBUTILSAPI_API bool pls_write_json_cbor(const QString &file_path, const QJsonObject &object, QString *error)
{
	QCborValue cbor = QCborValue::fromJsonValue(object);
	return pls_write_cbor(file_path, cbor, error);
}
LIBUTILSAPI_API bool pls_write_json_cbor(const QString &file_path, const QVariantMap &map, QString *error)
{
	return pls_write_json_cbor(file_path, QJsonObject::fromVariantMap(map), error);
}
LIBUTILSAPI_API bool pls_write_json_cbor(const QString &file_path, const QVariantHash &hash, QString *error)
{
	return pls_write_json_cbor(file_path, QJsonObject::fromVariantHash(hash), error);
}

LIBUTILSAPI_API bool pls_parse_json(QJsonDocument &doc, const QByteArray &json, QString *error)
{
	QJsonParseError jsonError;
	doc = QJsonDocument::fromJson(json, &jsonError);
	if (jsonError.error == QJsonParseError::NoError) {
		return true;
	} else {
		pls_set_value(error, jsonError.errorString());
		return false;
	}
}
LIBUTILSAPI_API bool pls_parse_json(QJsonArray &array, const QByteArray &json, QString *error)
{
	if (QJsonDocument doc; !pls_parse_json(doc, json, error)) {
		return false;
	} else if (doc.isArray()) {
		array = doc.array();
		return true;
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}
LIBUTILSAPI_API bool pls_parse_json(QVariantList &list, const QByteArray &json, QString *error)
{
	if (QJsonArray array; pls_parse_json(array, json, error)) {
		list = array.toVariantList();
		return true;
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}
LIBUTILSAPI_API bool pls_parse_json(QJsonObject &object, const QByteArray &json, QString *error)
{
	if (QJsonDocument doc; !pls_parse_json(doc, json, error)) {
		return false;
	} else if (doc.isObject()) {
		object = doc.object();
		return true;
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}
LIBUTILSAPI_API bool pls_parse_json(QVariantMap &map, const QByteArray &json, QString *error)
{
	if (QJsonObject object; pls_parse_json(object, json, error)) {
		map = object.toVariantMap();
		return true;
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}
LIBUTILSAPI_API bool pls_parse_json(QVariantHash &hash, const QByteArray &json, QString *error)
{
	if (QJsonObject object; pls_parse_json(object, json, error)) {
		hash = object.toVariantHash();
		return true;
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}

LIBUTILSAPI_API bool pls_encrypt_json(const QString &encrypt_file_path, const QJsonDocument &decrypt_doc, QString *error)
{
	return pls_write_json(encrypt_file_path, decrypt_doc, error);
}
LIBUTILSAPI_API bool pls_encrypt_json(const QString &encrypt_file_path, const QJsonArray &array, QString *error)
{
	return pls_encrypt_json(encrypt_file_path, QJsonDocument(array), error);
}
LIBUTILSAPI_API bool pls_encrypt_json(const QString &encrypt_file_path, const QVariantList &list, QString *error)
{
	return pls_encrypt_json(encrypt_file_path, QJsonArray::fromVariantList(list), error);
}
LIBUTILSAPI_API bool pls_encrypt_json(const QString &encrypt_file_path, const QJsonObject &object, QString *error)
{
	return pls_encrypt_json(encrypt_file_path, QJsonDocument(object), error);
}
LIBUTILSAPI_API bool pls_encrypt_json(const QString &encrypt_file_path, const QVariantMap &map, QString *error)
{
	return pls_encrypt_json(encrypt_file_path, QJsonObject::fromVariantMap(map), error);
}
LIBUTILSAPI_API bool pls_encrypt_json(const QString &encrypt_file_path, const QVariantHash &hash, QString *error)
{
	return pls_encrypt_json(encrypt_file_path, QJsonObject::fromVariantHash(hash), error);
}
LIBUTILSAPI_API bool pls_decrypt_json(QJsonDocument &doc, const QString &encrypt_file_path, QString *error)
{
	return pls_read_json(doc, encrypt_file_path, error);
}
LIBUTILSAPI_API bool pls_decrypt_json(QJsonArray &array, const QString &encrypt_file_path, QString *error)
{
	if (QJsonDocument doc; !pls_decrypt_json(doc, encrypt_file_path, error)) {
		return false;
	} else if (doc.isArray()) {
		array = doc.array();
		return true;
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}
LIBUTILSAPI_API bool pls_decrypt_json(QVariantList &list, const QString &encrypt_file_path, QString *error)
{
	if (QJsonArray array; pls_decrypt_json(array, encrypt_file_path, error)) {
		list = array.toVariantList();
		return true;
	}
	return false;
}
LIBUTILSAPI_API bool pls_decrypt_json(QJsonObject &object, const QString &encrypt_file_path, QString *error)
{
	if (QJsonDocument doc; !pls_decrypt_json(doc, encrypt_file_path, error)) {
		return false;
	} else if (doc.isObject()) {
		object = doc.object();
		return true;
	} else {
		pls_set_value(error, QStringLiteral("error format"));
		return false;
	}
}
LIBUTILSAPI_API bool pls_decrypt_json(QVariantMap &map, const QString &encrypt_file_path, QString *error)
{
	if (QJsonObject object; pls_decrypt_json(object, encrypt_file_path, error)) {
		map = object.toVariantMap();
		return true;
	}
	return false;
}
LIBUTILSAPI_API bool pls_decrypt_json(QVariantHash &hash, const QString &encrypt_file_path, QString *error)
{
	if (QJsonObject object; pls_decrypt_json(object, encrypt_file_path, error)) {
		hash = object.toVariantHash();
		return true;
	}
	return false;
}

LIBUTILSAPI_API QList<bool> pls_to_bool_list(const QJsonArray &array)
{
	QList<bool> bool_list;
	for (const QJsonValue v : array) {
		bool_list.append(v.toBool());
	}
	return bool_list;
}
LIBUTILSAPI_API QList<qint64> pls_to_int_list(const QJsonArray &array)
{
	QList<qint64> int_list;
	for (const QJsonValue v : array) {
		int_list.append(v.toInteger());
	}
	return int_list;
}
LIBUTILSAPI_API QList<double> pls_to_double_list(const QJsonArray &array)
{
	QList<double> double_list;
	for (const QJsonValue v : array) {
		double_list.append(v.toDouble());
	}
	return double_list;
}
LIBUTILSAPI_API QStringList pls_to_string_list(const QJsonArray &array)
{
	QStringList string_list;
	for (const QJsonValue v : array) {
		string_list.append(v.toString());
	}
	return string_list;
}
LIBUTILSAPI_API QVariantList pls_to_variant_list(const QJsonArray &array)
{
	return array.toVariantList();
}
LIBUTILSAPI_API QJsonArray pls_to_json_array(const QList<bool> &bool_list)
{
	QJsonArray array;
	for (const auto &s : bool_list) {
		array.append(s);
	}
	return array;
}
LIBUTILSAPI_API QJsonArray pls_to_json_array(const QList<qint64> &int_list)
{
	QJsonArray array;
	for (const auto &s : int_list) {
		array.append(s);
	}
	return array;
}
LIBUTILSAPI_API QJsonArray pls_to_json_array(const QList<double> &double_list)
{
	QJsonArray array;
	for (const auto &s : double_list) {
		array.append(s);
	}
	return array;
}
LIBUTILSAPI_API QJsonArray pls_to_json_array(const QStringList &string_list)
{
	QJsonArray array;
	for (const auto &s : string_list) {
		array.append(s);
	}
	return array;
}
LIBUTILSAPI_API QJsonArray pls_to_json_array(const QVariantList &variant_list)
{
	return QJsonArray::fromVariantList(variant_list);
}
LIBUTILSAPI_API QString pls_to_string(const QJsonArray &array)
{
	return QString::fromUtf8(QJsonDocument(array).toJson());
}
LIBUTILSAPI_API QString pls_to_string(const QJsonObject &object)
{
	return QString::fromUtf8(QJsonDocument(object).toJson());
}
LIBUTILSAPI_API QJsonArray pls_to_json_array(const QString &jsonArray)
{
	return QJsonDocument::fromJson(jsonArray.toUtf8()).array();
}
LIBUTILSAPI_API QJsonObject pls_to_json_object(const QString &jsonObject)
{
	return QJsonDocument::fromJson(jsonObject.toUtf8()).object();
}
LIBUTILSAPI_API QJsonObject pls_to_json_object(const QMap<QString, QString> &map)
{
	QJsonObject object;
	pls_for_each(map, [&object](const auto &key, const auto &value) { object.insert(key, value); });
	return object;
}
LIBUTILSAPI_API QJsonObject pls_to_json_object(const QHash<QString, QString> &hash)
{
	QJsonObject object;
	pls_for_each(hash, [&object](const auto &key, const auto &value) { object.insert(key, value); });
	return object;
}

struct pls_config_t {
	QString m_config_path;
	QJsonObject m_object;

	pls_config_t(const QString &config_path) : m_config_path(config_path) {}

	bool open(bool existing)
	{
		if (QString error; pls_read_json(m_object, m_config_path, &error)) {
			return true;
		} else {
			PLS_ERROR(UTILS_API_MODULE, "Failed to open config file: %s, error: %s", pls_get_path_file_name(m_config_path).toUtf8().constData(), error.toUtf8().constData());
			return (!existing) ? true : false;
		}
	}
	bool save()
	{
		if (QString error; pls_write_json(m_config_path, m_object, &error)) {
			return true;
		} else {
			PLS_ERROR(UTILS_API_MODULE, "Failed to save config file: %s, error: %s", pls_get_path_file_name(m_config_path).toUtf8().constData(), error.toUtf8().constData());
			return false;
		}
	}
	void clear() { m_object = QJsonObject(); }

	bool has_key(const QStringList &keys) const { return has_key(m_object, keys); }
	template<typename Result> Result get_value(const QStringList &keys, Result (*get)(const QJsonObject &object, const QStringList &keys)) { return get(m_object, keys); }
	template<typename T, typename V> bool set_value(const QStringList &keys, T &&value, bool (*set)(QJsonObject &object, const QStringList &keys, V value))
	{
		return set(m_object, keys, std::forward<T>(value));
	}
	void remove_key(const QStringList &keys) { remove_key(m_object, keys); }

	static bool has_key(const QJsonObject &object, const QStringList &keys)
	{
		if (!object.isEmpty() && !keys.isEmpty())
			return has_key(object, keys, 0, keys.length());
		return false;
	}
	static std::optional<QJsonValue> get_value(const QJsonObject &object, const QStringList &keys) { return pls_get_attr(object, keys); }
	static bool set_value(QJsonObject &object, const QStringList &keys, const QJsonValue &value)
	{
		if (!keys.isEmpty())
			return set_value(object, keys, value, 0, keys.length());
		return false;
	}
	static void remove_key(QJsonObject &object, const QStringList &keys)
	{
		if (!object.isEmpty() && !keys.isEmpty())
			remove_key(object, keys, 0, keys.length());
	}

private:
	static bool has_key(const QJsonObject &object, const QStringList &keys, qsizetype from, qsizetype to)
	{
		if (auto next = from + 1; next > to)
			return false;
		else if (const QString &key = keys[from]; next == to)
			return object.contains(key);
		else if (auto jv = object[key]; jv.isObject())
			return has_key(jv.toObject(), keys, next, to);
		return false;
	}
	static bool set_value(QJsonObject &object, const QString &key, const QJsonValue &value)
	{
		object[key] = value;
		return true;
	}
	static bool set_value(QJsonObject &object, const QStringList &keys, const QJsonValue &value, qsizetype from, qsizetype to)
	{
		if (auto next = from + 1; next > to) {
			return false;
		} else if (const QString &key = keys[from]; next == to) {
			return set_value(object, key, value);
		} else if (!object.contains(key)) {
			if (QJsonObject obj; set_value(obj, keys, value, next, to))
				return set_value(object, key, obj);
			return false;
		} else if (auto jv = object.value(key); jv.isObject()) {
			if (QJsonObject obj = jv.toObject(); set_value(obj, keys, value, next, to))
				return set_value(object, key, obj);
			return false;
		}
		return false;
	}
	static void remove_key(QJsonObject &object, const QStringList &keys, qsizetype from, qsizetype to)
	{
		if (auto next = from + 1; next > to) {
			return;
		} else if (const QString &key = keys[from]; next == to) {
			object.remove(key);
		} else if (auto jv = object.value(key); jv.isObject()) {
			auto obj = jv.toObject();
			remove_key(obj, keys, next, to);
			set_value(object, key, obj);
		}
	}
};

LIBUTILSAPI_API pls_config_t *pls_config_open(const QString &config_path, bool existing)
{
	if (auto config = pls_new<pls_config_t>(config_path); config->open(existing)) {
		LocalGlobalVars::g_configs.insert(config);
		return config;
	} else {
		pls_delete(config);
		return nullptr;
	}
}
LIBUTILSAPI_API void pls_config_save(pls_config_t *config)
{
	config->save();
}
LIBUTILSAPI_API bool pls_config_close(pls_config_t *config)
{
	LocalGlobalVars::g_configs.erase(config);
	pls_delete(config);
	return true;
}
LIBUTILSAPI_API bool pls_config_is_valid(pls_config_t *config)
{
	return config && LocalGlobalVars::g_configs.find(config) != LocalGlobalVars::g_configs.end();
}
LIBUTILSAPI_API void pls_config_close_all(bool save)
{
	for (auto config : LocalGlobalVars::g_configs) {
		if (save)
			pls_config_save(config);
		pls_delete(config, nullptr);
	}
	LocalGlobalVars::g_configs.clear();
}

LIBUTILSAPI_API bool pls_app_config_open(const QString &config_path, bool existing)
{
	pls_app_config_close();
	LocalGlobalVars::g_app_config = pls_config_open(config_path, existing);
	return LocalGlobalVars::g_app_config;
}
LIBUTILSAPI_API void pls_app_config_save()
{
	if (pls_config_is_valid(LocalGlobalVars::g_app_config))
		pls_config_save(LocalGlobalVars::g_app_config);
}
LIBUTILSAPI_API void pls_app_config_close()
{
	pls_delete(LocalGlobalVars::g_app_config, pls_config_close, nullptr);
}
LIBUTILSAPI_API pls_config_t *pls_app_config()
{
	return LocalGlobalVars::g_app_config;
}

LIBUTILSAPI_API bool pls_user_config_open(const QString &config_path, bool existing)
{
	pls_user_config_close();
	LocalGlobalVars::g_user_config = pls_config_open(config_path, existing);
	return LocalGlobalVars::g_user_config;
}
LIBUTILSAPI_API void pls_user_config_save()
{
	if (pls_config_is_valid(LocalGlobalVars::g_user_config))
		pls_config_save(LocalGlobalVars::g_user_config);
}
LIBUTILSAPI_API void pls_user_config_close()
{
	pls_delete(LocalGlobalVars::g_user_config, pls_config_close, nullptr);
}
LIBUTILSAPI_API pls_config_t *pls_user_config()
{
	return LocalGlobalVars::g_user_config;
}

LIBUTILSAPI_API QStringList pls_config_keys(pls_config_t *config)
{
	return config->m_object.keys();
}
LIBUTILSAPI_API void pls_config_clear(pls_config_t *config)
{
	config->clear();
}
LIBUTILSAPI_API bool pls_config_has_key(pls_config_t *config, const QStringList &keys)
{
	return config->has_key(keys);
}
LIBUTILSAPI_API void pls_config_remove_key(pls_config_t *config, const QStringList &keys)
{
	config->remove_key(keys);
}

LIBUTILSAPI_API std::optional<QJsonArray> pls_config_get_array(pls_config_t *config, const QStringList &keys)
{
	return config->get_value(keys, pls_config_get_array);
}
LIBUTILSAPI_API std::optional<QJsonObject> pls_config_get_object(pls_config_t *config, const QStringList &keys)
{
	return config->get_value(keys, pls_config_get_object);
}
LIBUTILSAPI_API std::optional<bool> pls_config_get_bool(pls_config_t *config, const QStringList &keys)
{
	return config->get_value(keys, pls_config_get_bool);
}
LIBUTILSAPI_API std::optional<qint64> pls_config_get_int(pls_config_t *config, const QStringList &keys)
{
	return config->get_value(keys, pls_config_get_int);
}
LIBUTILSAPI_API std::optional<double> pls_config_get_double(pls_config_t *config, const QStringList &keys)
{
	return config->get_value(keys, pls_config_get_double);
}
LIBUTILSAPI_API std::optional<QString> pls_config_get_string(pls_config_t *config, const QStringList &keys)
{
	return config->get_value(keys, pls_config_get_string);
}
LIBUTILSAPI_API std::optional<QList<bool>> pls_config_get_bool_list(pls_config_t *config, const QStringList &keys)
{
	return config->get_value(keys, pls_config_get_bool_list);
}
LIBUTILSAPI_API std::optional<QList<qint64>> pls_config_get_int_list(pls_config_t *config, const QStringList &keys)
{
	return config->get_value(keys, pls_config_get_int_list);
}
LIBUTILSAPI_API std::optional<QList<double>> pls_config_get_double_list(pls_config_t *config, const QStringList &keys)
{
	return config->get_value(keys, pls_config_get_double_list);
}
LIBUTILSAPI_API std::optional<QStringList> pls_config_get_string_list(pls_config_t *config, const QStringList &keys)
{
	return config->get_value(keys, pls_config_get_string_list);
}

LIBUTILSAPI_API bool pls_config_set_array(pls_config_t *config, const QStringList &keys, const QJsonArray &value)
{
	return config->set_value(keys, value, pls_config_set_array);
}
LIBUTILSAPI_API bool pls_config_set_object(pls_config_t *config, const QStringList &keys, const QJsonObject &value)
{
	return config->set_value(keys, value, pls_config_set_object);
}
LIBUTILSAPI_API bool pls_config_set_bool(pls_config_t *config, const QStringList &keys, bool value)
{
	return config->set_value(keys, value, pls_config_set_bool);
}
LIBUTILSAPI_API bool pls_config_set_int(pls_config_t *config, const QStringList &keys, qint64 value)
{
	return config->set_value(keys, value, pls_config_set_int);
}
LIBUTILSAPI_API bool pls_config_set_double(pls_config_t *config, const QStringList &keys, double value)
{
	return config->set_value(keys, value, pls_config_set_double);
}
LIBUTILSAPI_API bool pls_config_set_string(pls_config_t *config, const QStringList &keys, const QString &value)
{
	return config->set_value(keys, value, pls_config_set_string);
}
LIBUTILSAPI_API bool pls_config_set_bool_list(pls_config_t *config, const QStringList &keys, const QList<bool> &value)
{
	return config->set_value(keys, value, pls_config_set_bool_list);
}
LIBUTILSAPI_API bool pls_config_set_int_list(pls_config_t *config, const QStringList &keys, const QList<qint64> &value)
{
	return config->set_value(keys, value, pls_config_set_int_list);
}
LIBUTILSAPI_API bool pls_config_set_double_list(pls_config_t *config, const QStringList &keys, const QList<double> &value)
{
	return config->set_value(keys, value, pls_config_set_double_list);
}
LIBUTILSAPI_API bool pls_config_set_string_list(pls_config_t *config, const QStringList &keys, const QStringList &value)
{
	return config->set_value(keys, value, pls_config_set_string_list);
}

LIBUTILSAPI_API QStringList pls_config_keys(const QJsonObject &object)
{
	return object.keys();
}
LIBUTILSAPI_API void pls_config_clear(QJsonObject &object)
{
	object = QJsonObject();
}
LIBUTILSAPI_API bool pls_config_has_key(const QJsonObject &object, const QStringList &keys)
{
	return pls_config_t::has_key(object, keys);
}
LIBUTILSAPI_API void pls_config_remove_key(QJsonObject &object, const QStringList &keys)
{
	pls_config_t::remove_key(object, keys);
}

LIBUTILSAPI_API std::optional<QJsonArray> pls_config_get_array(const QJsonObject &object, const QStringList &keys)
{
	if (auto attr = pls_config_t::get_value(object, keys); !attr)
		return std::nullopt;
	else if (const auto &value = attr.value(); value.isArray())
		return value.toArray();
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<QJsonObject> pls_config_get_object(const QJsonObject &object, const QStringList &keys)
{
	if (auto attr = pls_config_t::get_value(object, keys); !attr)
		return std::nullopt;
	else if (const auto &value = attr.value(); value.isObject())
		return value.toObject();
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<bool> pls_config_get_bool(const QJsonObject &object, const QStringList &keys)
{
	if (auto attr = pls_config_t::get_value(object, keys); !attr)
		return std::nullopt;
	else if (const auto &value = attr.value(); value.isBool())
		return value.toBool();
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<qint64> pls_config_get_int(const QJsonObject &object, const QStringList &keys)
{
	if (auto attr = pls_config_t::get_value(object, keys); !attr)
		return std::nullopt;
	else if (const auto &value = attr.value(); value.isDouble())
		return value.toInteger();
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<double> pls_config_get_double(const QJsonObject &object, const QStringList &keys)
{
	if (auto attr = pls_config_t::get_value(object, keys); !attr)
		return std::nullopt;
	else if (const auto &value = attr.value(); value.isDouble())
		return value.toDouble();
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<QString> pls_config_get_string(const QJsonObject &object, const QStringList &keys)
{
	if (auto attr = pls_config_t::get_value(object, keys); !attr)
		return std::nullopt;
	else if (const auto &value = attr.value(); value.isString())
		return value.toString();
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<QList<bool>> pls_config_get_bool_list(const QJsonObject &object, const QStringList &keys)
{
	if (auto value = pls_config_get_array(object, keys); value)
		return pls_to_bool_list(value.value());
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<QList<qint64>> pls_config_get_int_list(const QJsonObject &object, const QStringList &keys)
{
	if (auto value = pls_config_get_array(object, keys); value)
		return pls_to_int_list(value.value());
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<QList<double>> pls_config_get_double_list(const QJsonObject &object, const QStringList &keys)
{
	if (auto value = pls_config_get_array(object, keys); value)
		return pls_to_double_list(value.value());
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<QStringList> pls_config_get_string_list(const QJsonObject &object, const QStringList &keys)
{
	if (auto value = pls_config_get_array(object, keys); value)
		return pls_to_string_list(value.value());
	return std::nullopt;
}

LIBUTILSAPI_API bool pls_config_set_array(QJsonObject &object, const QStringList &keys, const QJsonArray &value)
{
	return pls_config_t::set_value(object, keys, value);
}
LIBUTILSAPI_API bool pls_config_set_object(QJsonObject &object, const QStringList &keys, const QJsonObject &value)
{
	return pls_config_t::set_value(object, keys, value);
}
LIBUTILSAPI_API bool pls_config_set_bool(QJsonObject &object, const QStringList &keys, bool value)
{
	return pls_config_t::set_value(object, keys, value);
}
LIBUTILSAPI_API bool pls_config_set_int(QJsonObject &object, const QStringList &keys, qint64 value)
{
	return pls_config_t::set_value(object, keys, value);
}
LIBUTILSAPI_API bool pls_config_set_double(QJsonObject &object, const QStringList &keys, double value)
{
	return pls_config_t::set_value(object, keys, value);
}
LIBUTILSAPI_API bool pls_config_set_string(QJsonObject &object, const QStringList &keys, const QString &value)
{
	return pls_config_t::set_value(object, keys, value);
}
LIBUTILSAPI_API bool pls_config_set_bool_list(QJsonObject &object, const QStringList &keys, const QList<bool> &value)
{
	return pls_config_t::set_value(object, keys, pls_to_json_array(value));
}
LIBUTILSAPI_API bool pls_config_set_int_list(QJsonObject &object, const QStringList &keys, const QList<qint64> &value)
{
	return pls_config_t::set_value(object, keys, pls_to_json_array(value));
}
LIBUTILSAPI_API bool pls_config_set_double_list(QJsonObject &object, const QStringList &keys, const QList<double> &value)
{
	return pls_config_t::set_value(object, keys, pls_to_json_array(value));
}
LIBUTILSAPI_API bool pls_config_set_string_list(QJsonObject &object, const QStringList &keys, const QStringList &value)
{
	return pls_config_t::set_value(object, keys, pls_to_json_array(value));
}

static bool is_same_type(const std::optional<QJsonValue> &attr, const std::optional<QJsonValue::Type> &type)
{
	if ((!type) || (attr.value().type() == type.value()))
		return true;
	return false;
}

LIBUTILSAPI_API std::optional<QJsonValue> pls_find_attr(const QJsonObject &object, const QString &name, bool recursion, const std::optional<QJsonValue::Type> &type)
{
	if (auto attr = pls_get_attr(object, name); attr && is_same_type(attr, type))
		return attr;
	else if (!recursion)
		return std::nullopt;

	for (auto iter = object.begin(), end = object.end(); iter != end; ++iter) {
		if (auto value = iter.value(); !value.isObject()) {
			continue;
		} else if (auto attr = pls_find_attr(value.toObject(), name, recursion, type); attr) {
			return attr;
		}
	}
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<QJsonValue> pls_get_attr(const QJsonObject &object, const QString &name)
{
	if (object.isEmpty())
		return std::nullopt;
	else if (auto attr = object.value(name); !attr.isUndefined())
		return attr;
	return std::nullopt;
}
static std::optional<QJsonValue> get_attr(const QJsonObject &object, const QStringList &names, qsizetype from, qsizetype to)
{
	if (auto next = from + 1; next == to)
		return pls_get_attr(object, names[from]);
	else if (next > to)
		return std::nullopt;
	else if (auto val = pls_get_attr(object, names[from]); !val)
		return std::nullopt;
	else if (val.value().isObject())
		return get_attr(val.value().toObject(), names, from + 1, to);
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<QJsonValue> pls_get_attr(const QJsonObject &object, const QStringList &names, qsizetype from, qsizetype n)
{
	if (object.isEmpty() || names.isEmpty() || (from < 0) || (n == 0))
		return std::nullopt;
	else if (auto size = names.size(); size > from)
		return get_attr(object, names, from, from + ((n > 0) ? qMin(n, size - from) : (size - from)));
	return std::nullopt;
}

LIBUTILSAPI_API QList<pls_attr_name_t> pls_to_attr_names(const QStringList &names, int index)
{
	QList<pls_attr_name_t> attr_names;
	for (const auto &name : names)
		attr_names.append(pls_attr_name_t(name, index));
	return attr_names;
}

LIBUTILSAPI_API std::optional<QJsonValue> pls_get_attr(const QJsonValue &json, const pls_attr_name_t &name)
{
	if (auto attrs = pls_get_attrs(json, name); !attrs.isEmpty())
		return attrs.first();
	return std::nullopt;
}
static QList<QJsonValue> get_attr(const QJsonObject &object, const pls_attr_name_t &name)
{
	if (object.isEmpty())
		return {};
	else if (auto attr = object.value(name.m_name); !attr.isUndefined())
		return {attr};
	return {};
}
static QList<QJsonValue> get_attr(const QJsonArray &array, const pls_attr_name_t &name)
{
	if (array.isEmpty() || (name.m_name != QStringLiteral("[]"))) {
		return {};
	} else if (name.m_index == pls_attr_name_t::First) {
		return {array.first()};
	} else if (name.m_index == pls_attr_name_t::Last) {
		return {array.last()};
	} else if (name.m_index == pls_attr_name_t::All) {
		QList<QJsonValue> attrs;
		for (auto i : array)
			attrs.append(i);
		return attrs;
	} else if ((name.m_index > 0) && (name.m_index < array.size())) {
		return {array[name.m_index]};
	}
	return {};
}
LIBUTILSAPI_API QList<QJsonValue> pls_get_attrs(const QJsonValue &json, const pls_attr_name_t &name)
{
	if (json.isObject())
		return get_attr(json.toObject(), name);
	else if (json.isArray())
		return get_attr(json.toArray(), name);
	return {};
}
LIBUTILSAPI_API std::optional<QJsonValue> pls_get_attr(const QJsonValue &json, const QList<pls_attr_name_t> &names, qsizetype from, qsizetype n)
{
	if (auto attrs = pls_get_attrs(json, names, from, n); !attrs.isEmpty())
		return attrs.first();
	return std::nullopt;
}
static void get_attrs(QList<QJsonValue> &attrs, const QJsonValue &json, const QList<pls_attr_name_t> &names, qsizetype from, qsizetype to)
{
	if (json.isNull() || json.isUndefined())
		return;
	else if (auto next = from + 1; next == to) {
		attrs.append(pls_get_attrs(json, names[from]));
	} else if (next < to) {
		for (auto attr : pls_get_attrs(json, names[from]))
			get_attrs(attrs, attr, names, from + 1, to);
	}
}
LIBUTILSAPI_API QList<QJsonValue> pls_get_attrs(const QJsonValue &json, const QList<pls_attr_name_t> &names, qsizetype from, qsizetype n)
{
	if (json.isNull() || json.isUndefined() || names.isEmpty() || (from < 0) || (n == 0))
		return {};
	else if (auto size = names.size(); from < size) {
		QList<QJsonValue> attrs;
		get_attrs(attrs, json, names, from, from + ((n > 0) ? qMin(n, size - from) : (size - from)));
		return attrs;
	}
	return {};
}

template<typename Container>
static auto get_attr(const Container &attrs, const QStringList &names, qsizetype from,
		     qsizetype to) -> std::enable_if_t<std::is_same_v<Container, QVariantMap> || std::is_same_v<Container, QVariantHash>, std::optional<QVariant>>
{
	if (auto next = from + 1; next == to)
		return pls_get_attr(attrs, names[from]);
	else if (next > to)
		return std::nullopt;

	auto val = pls_get_attr(attrs, names[from]);
	if (!val)
		return std::nullopt;

	switch (val.value().typeId()) {
	case QMetaType::QVariantMap:
		return get_attr(val.value().toMap(), names, from + 1, to);
	case QMetaType::QVariantHash:
		return get_attr(val.value().toHash(), names, from + 1, to);
	default:
		return std::nullopt;
	}
}
LIBUTILSAPI_API std::optional<QVariant> pls_get_attr(const QVariantHash &attrs, const QString &name)
{
	if (auto iter = attrs.find(name); iter != attrs.end())
		return iter.value();
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<QVariant> pls_get_attr(const QVariantMap &attrs, const QString &name)
{
	if (auto iter = attrs.find(name); iter != attrs.end())
		return iter.value();
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<QVariant> pls_get_attr(const QVariantHash &attrs, const QStringList &names, qsizetype from, qsizetype n)
{
	if (attrs.isEmpty() || names.isEmpty() || (from < 0) || (n == 0))
		return std::nullopt;
	else if (auto size = names.size(); size > from)
		return get_attr(attrs, names, from, from + ((n > 0) ? qMin(n, size - from) : (size - from)));
	return std::nullopt;
}
LIBUTILSAPI_API std::optional<QVariant> pls_get_attr(const QVariantMap &attrs, const QStringList &names, qsizetype from, qsizetype n)
{
	if (attrs.isEmpty() || names.isEmpty() || (from < 0) || (n == 0))
		return std::nullopt;
	else if (auto size = names.size(); size > from)
		return get_attr(attrs, names, from, from + ((n > 0) ? qMin(n, size - from) : (size - from)));
	return std::nullopt;
}

LIBUTILSAPI_API QString pls_get_app_dir()
{
#if defined(Q_OS_WIN)
	std::array<wchar_t, 512> dir;
	GetModuleFileNameW(nullptr, dir.data(), dir.size());
	PathRemoveFileSpecW(dir.data());
	return QString::fromWCharArray(dir.data());
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_get_app_executable_dir();
#endif
}
LIBUTILSAPI_API QString pls_get_app_data_path(const QString &name)
{
#if defined(Q_OS_WIN)
	if (name.isEmpty())
		return pls_get_app_dir() + QStringLiteral("/../../data");
	else if (name.startsWith('/') || name.startsWith('\\'))
		return pls_get_app_dir() + QStringLiteral("/../../data") + name;
	else
		return pls_get_app_dir() + QStringLiteral("/../../data/") + name;
#elif defined(Q_OS_MACOS)
	if (name.isEmpty())
		return pls_get_app_resource_dir() + QStringLiteral("/data");
	else if (name.startsWith('/'))
		return pls_get_app_resource_dir() + QStringLiteral("/data") + name;
	else
		return pls_get_app_resource_dir() + QStringLiteral("/data/") + name;
#endif
}
LIBUTILSAPI_API QString pls_get_app_data_path_pn(const QString &name)
{
	if (name.isEmpty())
		return pls_get_app_data_path(pls_product_is_prism() ? QStringLiteral("/prism-studio") : QStringLiteral("/prism-lens"));
#if defined(Q_OS_WIN)
	else if (name.startsWith('/') || name.startsWith('\\'))
#elif defined(Q_OS_MACOS)
	else if (name.startsWith('/'))
#endif
		return pls_get_app_data_path((pls_product_is_prism() ? QStringLiteral("/prism-studio") : QStringLiteral("/prism-lens")) + name);
	else
		return pls_get_app_data_path((pls_product_is_prism() ? QStringLiteral("/prism-studio/") : QStringLiteral("/prism-lens/")) + name);
}

static QString get_app_user_data_path(const QString &name)
{
#if defined(Q_OS_WIN)
	wchar_t wpath[MAX_PATH];
	memset(wpath, 0, sizeof(wpath));
	SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, wpath);
	QString dirPath = QDir::fromNativeSeparators(QString::fromUtf8(pls_unicode_to_utf8(wpath)));
#elif defined(Q_OS_MACOS)
	QString dirPath = pls_get_mac_app_data_dir();
#endif

	if (name.isEmpty())
		return dirPath;
#if defined(Q_OS_WIN)
	else if (name.startsWith('/') || name.startsWith('\\'))
#elif defined(Q_OS_MACOS)
	else if (name.startsWith('/'))
#endif
		return dirPath + name;
	return dirPath + '/' + name;
}
LIBUTILSAPI_API QString pls_get_app_user_data_dir_path(const QString &name, bool mkdir)
{
	auto dir_path = get_app_user_data_path(name);
	if (mkdir)
		pls_mkdir(dir_path);
	return dir_path;
}
LIBUTILSAPI_API QString pls_get_app_user_data_dir_path_pn(const QString &name, bool mkdir)
{
	if (name.isEmpty())
		return pls_get_app_user_data_dir_path(get_user_data_product_name(), mkdir);
#if defined(Q_OS_WIN)
	else if (name.startsWith('/') || name.startsWith('\\'))
#elif defined(Q_OS_MACOS)
	else if (name.startsWith('/'))
#endif
		return pls_get_app_user_data_dir_path(get_user_data_product_name() + name, mkdir);
	else
		return pls_get_app_user_data_dir_path(get_user_data_product_name() + '/' + name, mkdir);
}
LIBUTILSAPI_API QString pls_get_app_user_data_file_path(const QString &name, bool mkdir)
{
	auto file_path = get_app_user_data_path(name);
	if (mkdir)
		pls_mkfiledir(file_path);
	return file_path;
}
LIBUTILSAPI_API QString pls_get_app_user_data_file_path_pn(const QString &name, bool mkdir)
{
	if (name.isEmpty())
		return pls_get_app_user_data_file_path(get_user_data_product_name(), mkdir);
#if defined(Q_OS_WIN)
	else if (name.startsWith('/') || name.startsWith('\\'))
#elif defined(Q_OS_MACOS)
	else if (name.startsWith('/'))
#endif
		return pls_get_app_user_data_file_path(get_user_data_product_name() + name, mkdir);
	else
		return pls_get_app_user_data_file_path(get_user_data_product_name() + '/' + name, mkdir);
}
LIBUTILSAPI_API QString pls_get_app_pn()
{
#if defined(Q_OS_WIN)
	pls::wchars<512> processName;
	GetModuleFileNameW(nullptr, processName, (DWORD)processName.capacity());
	return QString::fromWCharArray(PathFindFileNameW(processName));
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_get_app_pn();
#endif
}

LIBUTILSAPI_API QString pls_get_dll_dir(const QString &dll_name)
{
#if defined(Q_OS_WIN)
	if (auto hmodule = GetModuleHandleW(dll_name.toStdWString().c_str()); hmodule) {
		std::array<wchar_t, 512> dir;
		GetModuleFileNameW(hmodule, dir.data(), sizeof(dir));
		PathRemoveFileSpecW(dir.data());
		return QString::fromWCharArray(dir.data());
	}
	return QString();
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_get_dll_dir(dll_name);
#endif
}

LIBUTILSAPI_API QString pls_get_temp_dir(const QString &name)
{
	auto dirs = QStandardPaths::standardLocations(QStandardPaths::TempLocation);
	QString dirPath = dirs.first() + '/' + name;
	pls_mkdir(dirPath);
	return dirPath;
}

#ifdef Q_OS_WIN
static bool is_win_abs_path(const QString &path)
{
	if (path.size() < 2)
		return false;
	else if (path[0].isLetter() && (path[1] == ':'))
		return true;
	return false;
}
#else
static bool is_linux_abs_path(const QString &path)
{
	if (!path.isEmpty())
		return path[0] == '/';
	return false;
}
#endif
LIBUTILSAPI_API bool pls_is_abs_path(const QString &path)
{
#ifdef Q_OS_WIN
	if (is_win_abs_path(path))
		return true;
#else
	if (is_linux_abs_path(path))
		return true;
#endif
	return false;
}
LIBUTILSAPI_API QString pls_to_abs_path(const QString &path)
{
	if (!pls_is_abs_path(path))
		return QDir::currentPath() + '/' + path;
	return path;
}
LIBUTILSAPI_API bool pls_is_path_sep(char ch)
{
#ifdef Q_OS_WIN
	return ch == '\\' || ch == '/';
#else
	return ch == '/';
#endif
}
LIBUTILSAPI_API bool pls_is_path_sep(wchar_t ch)
{
#ifdef Q_OS_WIN
	return ch == L'\\' || ch == L'/';
#else
	return ch == '/';
#endif
}
LIBUTILSAPI_API const char *pls_get_path_file_name(const char *path)
{
	auto length = strlen(path);
	for (auto pos = path + length - 1; pos >= path; --pos) {
		if (pls_is_path_sep(*pos)) {
			return pos + 1;
		}
	}
	return path;
}
LIBUTILSAPI_API const wchar_t *pls_get_path_file_name(const wchar_t *path)
{
	auto length = wcslen(path);
	for (auto pos = path + length - 1; pos >= path; --pos) {
		if (pls_is_path_sep(*pos)) {
			return pos + 1;
		}
	}
	return path;
}
LIBUTILSAPI_API QString pls_get_path_file_name(const QString &path)
{
	if (path.isEmpty()) {
		return {};
	} else if (auto off1 = path.lastIndexOf('/'); off1 >= 0) {
		return path.mid(off1 + 1);
	} else if (auto off2 = path.lastIndexOf('\\'); off2 >= 0) {
		return path.mid(off2 + 1);
	}
	return path;
}

LIBUTILSAPI_API QString pls_get_path_file_suffix(const QString &path)
{
	if (auto file_name = pls_get_path_file_name(path); file_name.isEmpty()) {
		return {};
	} else if (auto off = file_name.lastIndexOf('.'); off >= 0) {
		return file_name.mid(off);
	}
	return {};
}
LIBUTILSAPI_API QString pls_remove_path_file_name(const QString &path)
{
	if (path.isEmpty()) {
		return {};
	} else if (path.endsWith('/') || path.endsWith('\\')) {
		return pls_remove_path_file_name(path.left(path.length() - 1));
	} else if (auto off1 = path.lastIndexOf('/'); off1 >= 0) {
		return path.left(off1);
	} else if (auto off2 = path.lastIndexOf('\\'); off2 >= 0) {
		return path.left(off2);
	}
	return {};
}

static bool enum_dir(const QString &dir, const std::function<bool(const QString &reldir, const QFileInfo &fi)> &result, const std::function<bool(const QString &reldir, const QFileInfo &fi)> &filter,
		     bool recursion, QDir::Filters filters, QDir::SortFlags sort, const QString &reldir = QString())
{
	for (const auto &fi : QDir(dir).entryInfoList(filters, sort)) {
		if (pls_invoke_safe(false, filter, reldir, fi))
			continue;
		else if ((!pls_invoke_safe(result, reldir, fi)) ||
			 (recursion && fi.isDir() && !enum_dir(fi.filePath(), result, filter, recursion, filters, sort, reldir.isEmpty() ? fi.fileName() : (reldir + '/' + fi.fileName()))))
			return false;
	}
	return true;
}
LIBUTILSAPI_API bool pls_enum_dir(const QString &dir, const std::function<bool(const QString &reldir, const QFileInfo &fi)> &result,
				  const std::function<bool(const QString &reldir, const QFileInfo &fi)> &filter, bool recursion, QDir::Filters filters, QDir::SortFlags sort)
{
	return enum_dir(dir, result, filter, recursion, filters, sort);
}
LIBUTILSAPI_API QStringList pls_enum_dirs(const QString &dir, const std::function<bool(const QString &reldir, const QString &name)> &filter, bool recursion, bool relative_dir)
{
	QStringList dirs;
	pls_enum_dir(
		dir,
		[&dirs, relative_dir](const QString &reldir, const QFileInfo &fi) {
			if (!fi.isDir())
				return true;
			else if (!relative_dir)
				dirs.append(fi.filePath());
			else if (!reldir.isEmpty())
				dirs.append(reldir + '/' + fi.fileName());
			else
				dirs.append(fi.fileName());
			return true;
		},
		[filter](const QString &reldir, const QFileInfo &fi) {
			if (fi.isFile() && pls_invoke_safe(false, filter, reldir, fi.fileName()))
				return true;
			return false;
		},
		recursion,                         //
		QDir::Dirs | QDir::NoDotAndDotDot, //
		QDir::Name);
	return dirs;
}
LIBUTILSAPI_API QStringList pls_enum_files(const QString &dir, const std::function<bool(const QString &reldir, const QString &name)> &filter, bool recursion, bool relative_dir)
{
	QStringList files;
	pls_enum_dir(
		dir,
		[&files, relative_dir](const QString &reldir, const QFileInfo &fi) {
			if (!fi.isFile())
				return true;
			else if (!relative_dir)
				files.append(fi.filePath());
			else if (!reldir.isEmpty())
				files.append(reldir + '/' + fi.fileName());
			else
				files.append(fi.filePath());
			return true;
		},
		[filter](const QString &reldir, const QFileInfo &fi) {
			if (fi.isFile() && pls_invoke_safe(false, filter, reldir, fi.fileName()))
				return true;
			return false;
		},
		recursion,                                                                     //
		recursion ? (QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot) : (QDir::Files), //
		QDir::Name | QDir::DirsLast);
	return files;
}

static std::optional<QString> find_subdir_contains_spec_file(const QFileInfo &fi, const QString &file_name, Qt::CaseSensitivity cs)
{
	if (fi.isDir()) {
		return pls_find_subdir_contains_spec_file(fi.absoluteFilePath(), file_name, cs);
	} else if (!fi.fileName().compare(file_name, cs)) {
		return fi.absolutePath();
	} else {
		return std::nullopt;
	}
}
LIBUTILSAPI_API std::optional<QString> pls_find_subdir_contains_spec_file(const QString &dir, const QString &file_name, Qt::CaseSensitivity cs)
{
	for (const auto &fi : QDir(dir).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsLast)) {
		if (auto subdir = find_subdir_contains_spec_file(fi, file_name, cs); subdir) {
			return subdir;
		}
	}
	return std::nullopt;
}

static std::optional<QString> find_subdir_contains_spec_filename(const QFileInfo &fi, const QString &file_name, Qt::CaseSensitivity cs)
{
	if (fi.isDir()) {
		return pls_find_subdir_contains_spec_filename(fi.absoluteFilePath(), file_name, cs);
	} else if (fi.fileName().contains(file_name, cs)) {
		return fi.filePath();
	} else {
		return std::nullopt;
	}
}
LIBUTILSAPI_API std::optional<QString> pls_find_subdir_contains_spec_filename(const QString &dir, const QString &file_name, Qt::CaseSensitivity cs)
{
	for (const auto &fi : QDir(dir).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsLast)) {
		if (auto subdir = find_subdir_contains_spec_filename(fi, file_name, cs); subdir) {
			return subdir;
		}
	}
	return std::nullopt;
}

LIBUTILSAPI_API bool pls_remove_file(const QString &path, QString *error)
{
	if (QFile file(path); (!file.exists()) || file.remove()) {
		return true;
	} else {
		pls_set_value(error, file.errorString());
		return false;
	}
}

LIBUTILSAPI_API bool pls_remove_dir(const QString &path, QString *error)
{
	if (QDir dir(path); (!dir.exists()) || dir.removeRecursively()) {
		return true;
	} else {
		return false;
	}
}

LIBUTILSAPI_API bool pls_rename_file(const QString &old_file, const QString &new_file, QString *error)
{
	if (!pls_remove_file(new_file, error)) {
		return false;
	} else if (QFile file(old_file); file.rename(new_file)) {
		return true;
	} else {
		pls_set_value(error, file.errorString());
		return false;
	}
}

LIBUTILSAPI_API bool pls_copy_file(const QString &src_file, const QString &dest_file, QString *error)
{
	if (QFile src(src_file); !src.exists()) {
		pls_set_value(error, "source file does not exist");
		return false;
	} else if (!pls_remove_file(dest_file, error)) {
		return false;
	} else if (!pls_mkfiledir(dest_file)) {
		pls_set_value(error, "create destination file directory failed");
		return false;
	} else if (!src.copy(dest_file)) {
		pls_set_value(error, src.errorString());
		return false;
	}
	return true;
}
static bool copy_dir(const QFileInfo &fi, const QDir &dest, QString *error)
{
	return pls_copy_dir(fi.filePath(), dest.filePath(fi.fileName()), error);
}
static bool copy_file(const QFileInfo &fi, const QDir &dest, QString *error)
{
	return pls_copy_file(fi.filePath(), dest.filePath(fi.fileName()), error);
}
LIBUTILSAPI_API bool pls_copy_dir(const QString &src_dir, const QString &dest_dir, QString *error)
{
	if (QDir src(src_dir); !src.exists()) {
		pls_set_value(error, "source directory does not exist");
		return false;
	} else if (QDir dest(dest_dir); !pls_mkdir(dest)) {
		pls_set_value(error, "create destination directory failed");
		return false;
	} else {
		for (const auto &fi : src.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::DirsLast))
			if (auto dir = fi.isDir(); (dir && !copy_dir(fi, dest, error)) || (!dir && !copy_file(fi, dest, error)))
				return false;
		return true;
	}
}

LIBUTILSAPI_API QString pls_get_installed_obs_version()
{
#ifdef Q_OS_WIN
	return "";
#else
	return pls_libutil_api_mac::pls_get_app_version_by_identifier("com.obsproject.obs-studio");
#endif
}

class pls_process_t {
public:
#if defined(Q_OS_WIN)
	HANDLE m_process;
	pls_process_t(HANDLE process) : m_process(process) {}
	~pls_process_t() { pls_delete(m_process, CloseHandle, nullptr); }
#elif defined(Q_OS_MACOS)
	MacHandle m_process;
	pls_process_t(MacHandle process) : m_process(process) {}
	~pls_process_t() { pls_delete(m_process, nullptr); }
#endif
};
#if defined(Q_OS_WIN)
static pls_process_t *new_process(HANDLE hProcess, bool terminate = true)
{
	if (auto process = pls_new_nothrow<pls_process_t>(hProcess); process) {
		return process;
	}

	if (terminate) {
		TerminateProcess(hProcess, 0xffff);
		WaitForSingleObject(hProcess, INFINITE);
	}

	pls_delete(hProcess, CloseHandle);
	return nullptr;
}
#elif defined(Q_OS_MACOS)
static pls_process_t *new_process(MacHandle hProcess, bool terminate = true)
{
	if (hProcess->pid == 0) {
		return nullptr;
	}
	if (auto process = pls_new_nothrow<pls_process_t>(hProcess); process) {
		return process;
	}

	if (terminate) {
		pls_libutil_api_mac::pls_process_destroy(hProcess);
	}
	return nullptr;
}
#endif
LIBUTILSAPI_API pls_process_t *pls_process_create(const QString &program, const QStringList &arguments, bool run_as)
{
	return pls_process_create(program, arguments, QString(), run_as);
}
LIBUTILSAPI_API pls_process_t *pls_process_create(const QString &program, const QStringList &arguments, const QString &work_dir, bool run_as)
{
	QFileInfo appFi(program);
	auto name = appFi.completeBaseName();

#if defined(Q_OS_WIN)
	std::wstring workdir;
	if (!work_dir.isEmpty()) {
		workdir = work_dir.toStdWString();
	} else {
		workdir = appFi.dir().absolutePath().toStdWString();
	}

	auto app = pls_add_quotes(program).toStdWString();
	auto args = pls_map<QList>(pls_filter(pls_unique(arguments), pls_is_not_empty<QString>), pls_add_quotes).join(' ').toStdWString();
	auto command = app + L' ' + args;

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, workdir.c_str(), &si, &pi)) {
		pls_delete(pi.hThread, CloseHandle, nullptr);
		return new_process(pi.hProcess);
	}

	auto errorCode = GetLastError();
	PLS_WARN_LOGEX(UTILS_API_MODULE, {{"createprocessName", name.toUtf8().constData()}, {"createprocessErrorCode", std::to_string(errorCode).c_str()}}, "CreateProcess failed for %s: %d",
		       name.toUtf8().constData(), errorCode);
	if (!run_as || errorCode != ERROR_ELEVATION_REQUIRED) {
		pls_set_last_error(errorCode);
		return nullptr;
	}

	SHELLEXECUTEINFOW sei;
	ZeroMemory(&sei, sizeof(sei));
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb = L"runas";
	sei.lpFile = app.data();
	sei.lpParameters = args.data();
	sei.lpDirectory = workdir.data();
	sei.nShow = SW_SHOWNORMAL;
	if (ShellExecuteExW(&sei)) {
		return new_process(sei.hProcess);
	}

	errorCode = (DWORD)(uintptr_t)sei.hInstApp;
	PLS_WARN_LOGEX(UTILS_API_MODULE, {{"createprocessName", name.toUtf8().constData()}, {"createprocessErrorCode", std::to_string(errorCode).c_str()}}, "ShellExecuteEx failed for %s: %d",
		       name.toUtf8().constData(), errorCode);
	pls_set_last_error(errorCode);
	return nullptr;
#else
	QString workdir = work_dir;
	if (work_dir.isEmpty()) {
		workdir = appFi.dir().absolutePath();
	}
	MacHandle handle = pls_libutil_api_mac::pls_process_create(program, arguments, workdir);
	pls_process_t *process = new_process(handle);
	pls_add_monitor_process_exit_event(process);
	return process;
#endif
}
LIBUTILSAPI_API pls_process_t *pls_process_create(uint32_t process_id)
{
#if defined(Q_OS_WIN)
	if (auto hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id); hProcess) {
		return new_process(hProcess, false);
	} else if (auto hLimitedProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | PROCESS_TERMINATE, FALSE, process_id); hLimitedProcess) {
		return new_process(hLimitedProcess, false);
	}
	return nullptr;
#elif defined(Q_OS_MACOS)
	MacHandle handle = pls_libutil_api_mac::pls_process_create(process_id);
	pls_process_t *process = new_process(handle);
	pls_add_monitor_process_exit_event(process);
	return process;
#endif
}
LIBUTILSAPI_API void pls_process_destroy(pls_process_t *process)
{
#if defined(Q_OS_MACOS)
	pls_remove_monitor_process_exit_event(process);
#endif
	pls_delete(process);
}

LIBUTILSAPI_API int pls_process_wait(pls_process_t *process, int timeout)
{
#if defined(Q_OS_WIN)
	if (process && process->m_process) {
		return WaitForSingleObject(process->m_process, (timeout >= 0) ? timeout : INFINITE) == WAIT_OBJECT_0 ? 1 : 0;
	}
	return -1;
#elif defined(Q_OS_MACOS)
	if (process && process->m_process) {
		return pls_libutil_api_mac::pls_process_wait(process->m_process, timeout);
	}
	return -1;
#endif
}
LIBUTILSAPI_API uint32_t pls_process_id(pls_process_t *process)
{
#if defined(Q_OS_WIN)
	if (process && process->m_process) {
		return GetProcessId(process->m_process);
	}
	return 0;
#elif defined(Q_OS_MACOS)
	if (process && process->m_process) {
		return pls_libutil_api_mac::pls_process_id(process->m_process);
	}
	return 0;
#endif
}
LIBUTILSAPI_API uint32_t pls_process_exit_code(pls_process_t *process, uint32_t default_exit_code)
{
	if (uint32_t exit_code = 0; pls_process_exit_code(&exit_code, process)) {
		return exit_code;
	}
	return default_exit_code;
}
LIBUTILSAPI_API bool pls_process_exit_code(uint32_t *exit_code, pls_process_t *process)
{
#if defined(Q_OS_WIN)
	if (process && process->m_process) {
		DWORD exitCode = 0;
		if (GetExitCodeProcess(process->m_process, &exitCode)) {
			*exit_code = exitCode;
			return true;
		}

		pls_set_last_error(GetLastError());
		return false;
	}
	return false;
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_process_exit_code(process->m_process, exit_code);
#endif
}

LIBUTILSAPI_API bool pls_process_terminate(pls_process_t *process, int exit_code)
{
#if defined(Q_OS_WIN)
	if (process && process->m_process) {
		if (TerminateProcess(process->m_process, exit_code)) {
			return true;
		}

		pls_set_last_error(GetLastError());
		return false;
	}
	return false;
#elif defined(Q_OS_MACOS)
	if (process && process->m_process) {
		return pls_libutil_api_mac::pls_process_destroy(process->m_process);
	}
	return false;
#endif
}

LIBUTILSAPI_API uint32_t pls_current_process_id()
{
#if defined(Q_OS_WIN)
	return GetCurrentProcessId();
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_current_process_id();
#endif
}

LIBUTILSAPI_API bool pls_is_valid_process_id(uint32_t process_id)
{
#if defined(Q_OS_WIN)
	bool is_valid = false;
	if (auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, process_id); snapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 entry;
		ZeroMemory(&entry, sizeof(entry));
		entry.dwSize = sizeof(PROCESSENTRY32);
		for (auto i = Process32First(snapshot, &entry); i; i = Process32Next(snapshot, &entry)) {
			if (entry.th32ProcessID == process_id) {
				is_valid = true;
				break;
			}
		}
		CloseHandle(snapshot);
	}
	return is_valid;
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_is_process_running(process_id);
#endif
}
LIBUTILSAPI_API bool pls_process_terminate(uint32_t process_id, int exit_code)
{
#if defined(Q_OS_WIN)
	if (auto hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, process_id); hProcess) {
		return TerminateProcess(hProcess, exit_code) ? true : false;
	}
	return false;
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_process_force_terminte(process_id, exit_code);
#endif
}
LIBUTILSAPI_API bool pls_process_terminate(int exit_code)
{
#if defined(Q_OS_WIN)
	return TerminateProcess(GetCurrentProcess(), exit_code) ? true : false;
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_process_force_terminte(exit_code);
#endif
}

LIBUTILSAPI_API uint32_t pls_current_thread_id()
{
#if defined(Q_OS_WIN)
	return GetCurrentThreadId();
#elif defined(Q_OS_MACOS)
	return pthread_mach_thread_np(pthread_self());
#endif
}

LIBUTILSAPI_API void pls_sleep(uint32_t seconds)
{
	pls_sleep_ms(seconds * 1000);
}
LIBUTILSAPI_API void pls_sleep_ms(uint32_t milliseconds)
{
	QThread::msleep(milliseconds);
}

class pls_sem_t : public QSemaphore {
public:
	using QSemaphore::QSemaphore;
};
LIBUTILSAPI_API pls_sem_t *pls_sem_create(int initial_count)
{
	return pls_new_nothrow<pls_sem_t>(initial_count);
}
LIBUTILSAPI_API void pls_sem_destroy(pls_sem_t *sem)
{
	pls_delete(sem);
}
LIBUTILSAPI_API bool pls_sem_release(pls_sem_t *sem, int count)
{
	if (sem) {
		sem->release(count);
		return true;
	}
	return false;
}
LIBUTILSAPI_API bool pls_sem_try_acquire(pls_sem_t *sem)
{
	return sem ? sem->try_acquire() : false;
}
LIBUTILSAPI_API bool pls_sem_acquire(pls_sem_t *sem)
{
	if (sem) {
		sem->acquire(1);
		return true;
	}
	return false;
}

LIBUTILSAPI_API QString pls_shm_generate_unique_name()
{
	return QStringLiteral("pls_shm_name_%1_%2").arg(pls_current_process_id()).arg(pls_gen_uuid());
}
LIBUTILSAPI_API QString pls_shm_generate_cache_file_path(const QString &name)
{
	return QDir::tempPath() + QStringLiteral("/pls_shm_cache_") + name + QStringLiteral(".dat");
}

class pls_shm_base_t {
	struct shm_header_t {
		int max_data_size;
		int data_length;
		/* data */
	};

	QString m_name;
	QSharedMemory m_sm;
	int m_max_data_size;

public:
	pls_shm_base_t(const QString &name, int max_data_size) //
		: m_name(name),                                //
		  m_sm(QStringLiteral("pls_shm_sm_") + name),
		  m_max_data_size(max_data_size)
	{
	}
	~pls_shm_base_t() { m_sm.detach(); }

	int max_data_size() const { return m_max_data_size; }

	qsizetype shm_size() const { return sizeof(shm_header_t) + m_max_data_size; }

	char *shm_memory() const { return (char *)m_sm.data(); }
	shm_header_t *shm_header() const { return (shm_header_t *)shm_memory(); }
	char *shm_data(shm_header_t *sh) const { return ((char *)sh) + sizeof(shm_header_t); }

	bool create()
	{
		if (m_sm.attach()) {
			std::lock_guard guard(m_sm);
			auto sh = shm_header();
			m_max_data_size = sh->max_data_size;
			return true;
		} else if (m_sm.create(shm_size())) {
			std::lock_guard guard(m_sm);
			auto sh = shm_header();
			sh->max_data_size = m_max_data_size;
			sh->data_length = 0;
			return true;
		}
		return false;
	}

	int read(char *data, int length)
	{
		if (!m_sm.isAttached())
			return -1;

		std::lock_guard lock(m_sm);
		auto sh = shm_header();
		if (sh->data_length <= 0)
			return 0;

		int read_length = qMin(sh->data_length, length);
		memcpy(data, shm_data(sh), read_length);
		return read_length;
	}
	int write(const char *data, int length)
	{
		if (!m_sm.isAttached())
			return -1;

		std::lock_guard lock(m_sm);
		auto sh = shm_header();
		auto write_length = qMin(m_max_data_size, length);
		sh->data_length = write_length;
		memcpy(shm_data(sh), data, write_length);
		return write_length;
	}
};
LIBUTILSAPI_API pls_shm_base_t *pls_shm_base_create(const QString &name, int max_data_size)
{
	pls_shm_base_t *shm = pls_new_nothrow<pls_shm_base_t>(name, max_data_size);
	if (!shm) {
		return nullptr;
	} else if (shm->create()) {
		return shm;
	}
	pls_shm_base_destroy(shm);
	return nullptr;
}
LIBUTILSAPI_API int pls_shm_base_read(pls_shm_base_t *shm, char *buf, int length)
{
	if (shm && buf && (length > 0)) {
		return shm->read(buf, length);
	}
	return -1;
}
LIBUTILSAPI_API int pls_shm_base_write(pls_shm_base_t *shm, const char *buf, int length)
{
	if (shm && buf && (length > 0)) {
		return shm->write(buf, length);
	}
	return -1;
}
LIBUTILSAPI_API void pls_shm_base_destroy(pls_shm_base_t *shm)
{
	pls_delete(shm);
}

LIBUTILSAPI_API int pls_shm_base_get_max_data_size(pls_shm_base_t *shm)
{
	return shm ? shm->max_data_size() : 0;
}

class pls_shm_t {
	enum class state_t : char { //
		Offline = 0,
		Online
	};
	struct shm_header_t {
		int max_data_size;
		int data_length;
		state_t sender_state;
		state_t receiver_state;
		/* data */
	};
	struct data_header_t {
		int data_length;
		/* data */
	};
	struct cache_t {
		QFile m_file;
		uchar *m_map_memory = nullptr;
		explicit cache_t(const QString &cache_file_path) : m_file(cache_file_path) {}

		bool exists() const
		{
			if (m_file.fileName().isEmpty())
				return false;
			else if (!m_file.exists())
				return false;
			return true;
		}
		bool init(bool &exists, bool for_send, qsizetype shm_size)
		{
			if (m_file.fileName().isEmpty())
				return false;

			exists = m_file.exists();
			if (!for_send && !exists)
				return false;
			else if (!m_file.open(QFile::ReadWrite))
				return false;

			if (exists)
				shm_size = m_file.size();
			else
				m_file.resize(shm_size);

			m_map_memory = m_file.map(0, shm_size);
			if (m_map_memory)
				return true;

			m_file.close();
			m_file.remove();
			return false;
		}
		void cleanup(bool remove)
		{
			if (m_map_memory) {
				m_file.unmap(m_map_memory);
				m_map_memory = nullptr;
				m_file.close();
				if (remove)
					m_file.remove();
			}
		}
	};

	QString m_name;
	QSharedMemory m_sm;
	cache_t m_cache;
	int m_max_data_size;
	bool m_for_send;
	bool m_is_first_receive = true;
	state_t m_state = state_t::Offline;

public:
	pls_shm_t(const QString &name, int max_data_size, bool for_send, const QString &cache_file_path) //
		: m_name(name),                                                                          //
		  m_sm(QStringLiteral("pls_shm_sm_") + name),
		  m_cache(cache_file_path),
		  m_max_data_size(max_data_size),
		  m_for_send(for_send)
	{
	}
	~pls_shm_t() { destroy(); }

	qsizetype shm_size() const { return sizeof(shm_header_t) + sizeof(data_header_t) + m_max_data_size; }

	bool is_for_send() const { return m_for_send; }
	bool is_for_receive() const { return !m_for_send; }

	bool is_online() const { return m_state == state_t::Online; }
	bool is_sender_online() const { return is_online() && shm_header()->sender_state == state_t::Online; }
	bool is_receiver_online() const { return is_online() && shm_header()->receiver_state == state_t::Online; }

	char *shm_memory() const { return (char *)m_sm.data(); }
	shm_header_t *shm_header() const { return (shm_header_t *)shm_memory(); }
	char *shm_data(shm_header_t *sh, int data_offset = 0) const { return ((char *)sh) + sizeof(shm_header_t) + data_offset; }
	template<typename T> T shm_data(shm_header_t *sh, int data_offset = 0) const { return (T)shm_data(sh, data_offset); }
	char *get_cache_file_path(shm_header_t *sh) const { return ((char *)sh) + sizeof(shm_header_t); }

	int max_data_size() const { return m_max_data_size; }
	int remaining(const shm_header_t *sh) const { return m_max_data_size - sh->data_length - sizeof(data_header_t); }

	void write_msg(shm_header_t *sh, const pls_shm_msg_t *msg) const
	{
		data_header_t dh{msg->length}; // header for split package
		memcpy(shm_data(sh, sh->data_length), &dh, sizeof(data_header_t));
		sh->data_length += sizeof(data_header_t);

		memcpy(shm_data(sh, sh->data_length), msg->data, msg->length);
		sh->data_length += msg->length;
	}
	bool read_msg(shm_header_t *sh, pls_shm_msg_t *msg, int &data_offset) const
	{
		if (data_offset < sh->data_length) {
			auto dh = shm_data<const data_header_t *>(sh, data_offset);
			msg->length = dh->data_length;
			data_offset += sizeof(data_header_t);

			msg->data = shm_data(sh, data_offset);
			data_offset += dh->data_length;
			return true;
		}
		return false;
	}

	bool init_shm()
	{
		if (m_sm.attach()) {
			std::lock_guard guard(m_sm);
			auto sh = shm_header();
			m_max_data_size = sh->max_data_size;
			return true;
		} else if (m_sm.create(shm_size())) {
			std::lock_guard guard(m_sm);
			auto sh = shm_header();
			sh->max_data_size = m_max_data_size;
			sh->data_length = 0;
			sh->sender_state = state_t::Offline;
			sh->receiver_state = state_t::Offline;
			return true;
		}
		return false;
	}
	bool create()
	{
		if (!init_shm())
			return false;

		std::lock_guard guard(m_sm);
		auto sh = shm_header();
		if (auto &state = is_for_send() ? sh->sender_state : sh->receiver_state; state == state_t::Offline) {
			state = state_t::Online;
			m_state = state_t::Online;
			return true;
		}
		return false;
	}
	void destroy()
	{
		close();
		m_sm.detach();
		m_cache.cleanup(false);
	}
	void close()
	{
		m_state = state_t::Offline;
		std::lock_guard guard(m_sm);
		if (auto sh = shm_header(); sh) {
			auto &state = is_for_send() ? sh->sender_state : sh->receiver_state;
			state = state_t::Offline;
		}
	}

	void get_cache(shm_header_t *&sh)
	{
		if (m_cache.m_map_memory) {
			sh = (shm_header_t *)m_cache.m_map_memory;
			return;
		}

		bool exists = false;
		if (!m_cache.init(exists, is_for_send(), shm_size()))
			return;

		sh = (shm_header_t *)m_cache.m_map_memory;
		if (!exists) {
			sh->max_data_size = m_max_data_size;
			sh->data_length = 0;
			sh->sender_state = state_t::Offline;
			sh->receiver_state = state_t::Offline;
		}
	}

	void send_msg(const pls_shm_msg_destroy_cb_t &msg_destroy_cb, const pls_shm_get_msg_cb_t &get_msg_cb, const pls_shm_result_cb_t &result_cb)
	{
		if (!is_for_send() || !is_online() || !m_sm.isAttached()) {
			pls_invoke_safe(result_cb, -1);
			return;
		}

		auto count = 0;

		if (m_sm.lock()) {
			auto sh = shm_header();
			if (sh->receiver_state != state_t::Online)
				get_cache(sh);

			pls_shm_msg_t msg;
			while (get_msg_cb(&msg, remaining(sh), m_max_data_size)) {
				write_msg(sh, &msg);
				msg_destroy_cb(&msg);
				++count;
			}

			m_cache.cleanup(false);
			m_sm.unlock();
		}

		pls_invoke_safe(result_cb, count);
	}
	void receive_msg(const std::function<void(pls_shm_msg_t *msg)> &proc_msg_cb, const pls_shm_result_cb_t &result_cb)
	{
		if (!is_for_receive() || !is_online() || !m_sm.isAttached()) {
			pls_invoke_safe(result_cb, -1);
			return;
		}

		auto count = 0;

		if (m_sm.lock()) {
			auto sh = shm_header();
			if (m_is_first_receive) {
				m_is_first_receive = false;
				if (m_cache.exists())
					get_cache(sh);
			}

			pls_shm_msg_t msg;
			for (int data_offset = 0; read_msg(sh, &msg, data_offset);) {
				proc_msg_cb(&msg);
				++count;
			}

			sh->data_length = 0;
			m_cache.cleanup(true);
			m_sm.unlock();
		}

		pls_invoke_safe(result_cb, count);
	}
};
LIBUTILSAPI_API pls_shm_t *pls_shm_create(const QString &name, int max_data_size, bool for_send, const QString &cache_file_path)
{
	auto shm = pls_new_nothrow<pls_shm_t>(name, max_data_size, for_send, cache_file_path);
	if (!shm) {
		return nullptr;
	} else if (shm->create()) {
		return shm;
	}
	pls_shm_destroy(shm);
	return nullptr;
}
LIBUTILSAPI_API void pls_shm_destroy(pls_shm_t *shm)
{
	pls_delete(shm);
}
LIBUTILSAPI_API void pls_shm_close(pls_shm_t *shm)
{
	if (shm) {
		shm->close();
	}
}
LIBUTILSAPI_API void pls_shm_send_msg(pls_shm_t *shm, const pls_shm_msg_destroy_cb_t &msg_destroy_cb, const pls_shm_get_msg_cb_t &get_msg_cb, const pls_shm_result_cb_t &result_cb)
{
	if (shm && msg_destroy_cb && get_msg_cb) {
		shm->send_msg(msg_destroy_cb, get_msg_cb, result_cb);
	}
}
LIBUTILSAPI_API void pls_shm_receive_msg(pls_shm_t *shm, const std::function<void(pls_shm_msg_t *msg)> &proc_msg_cb, const pls_shm_result_cb_t &result_cb)
{
	if (shm && proc_msg_cb) {
		shm->receive_msg(proc_msg_cb, result_cb);
	}
}
LIBUTILSAPI_API bool pls_shm_is_for_send(pls_shm_t *shm)
{
	return shm ? shm->is_for_send() : false;
}
LIBUTILSAPI_API bool pls_shm_is_for_receive(pls_shm_t *shm)
{
	return shm ? shm->is_for_receive() : false;
}
LIBUTILSAPI_API int pls_shm_get_max_data_size(pls_shm_t *shm)
{
	return shm ? shm->max_data_size() : 0;
}
LIBUTILSAPI_API bool pls_shm_is_online(pls_shm_t *shm)
{
	return shm && shm->is_online();
}
LIBUTILSAPI_API bool pls_shm_is_sender_online(pls_shm_t *shm)
{
	return shm && shm->is_sender_online();
}
LIBUTILSAPI_API bool pls_shm_is_receiver_online(pls_shm_t *shm)
{
	return shm && shm->is_receiver_online();
}

namespace {
constexpr int IPC_MESSAGE_BEGIN_MASK = 0x2b2c2d2e;
constexpr int IPC_MESSAGE_END_MASK = 0x3b3c3d3e;

constexpr int MIN_MESSAGE_LENGTH = sizeof(int) * 3;

constexpr int IPC_TYPE_PROCESS_ID = -10000;
constexpr int IPC_TYPE_WAKE_UP = -10001;
constexpr int IPC_TYPE_APP_ACTIVED = -10002;
constexpr int IPC_TYPE_APP_ANY_WINDOW_SHOW = -10003;
constexpr int IPC_TYPE_APP_MAIN_WINDOW_SHOW = -10004;
constexpr int IPC_TYPE_APP_ANY_WINDOW_ACTIVED = -10005;
constexpr int IPC_TYPE_APP_MAIN_WINDOW_ACTIVED = -10006;

enum class IpcConnectState { None, Connecting, Reconnecting, Connected, Disconnected };
struct IpcSocket : public QLocalSocket {
	using QLocalSocket::QLocalSocket;
	~IpcSocket() override { disconnect(); }
};
struct IpcBase {
	QString m_name;
	pls_ipc_on_connected_t m_on_connected;
	pls_ipc_on_disconnected_t m_on_disconnected;
	pls_ipc_on_message_t m_on_message;
	pls_ipc_on_inner_message_t m_on_inner_message;
	std::atomic<IpcConnectState> m_state = IpcConnectState::None;
	std::atomic<uint32_t> m_peer_process_id = -1;

	explicit IpcBase(const QString &name) : m_name(name) {}
	virtual ~IpcBase() = default;

	virtual IpcSocket *socket() = 0;

	virtual void init() = 0;
	virtual void destroy() = 0;

	bool isConnected() const { return m_state == IpcConnectState::Connected; }

	void send(int type, const QJsonValue &data)
	{
		pls_async_call(socket(), [this, type, data]() { sendSync(type, data); });
	}
	void sendSync(int type, const QJsonValue &data)
	{
		if (auto sock = socket(); !sock)
			return;
		else if (sock->state() == IpcSocket::LocalSocketState::ConnectedState) {
			int beginMask = IPC_MESSAGE_BEGIN_MASK;
			int endMask = IPC_MESSAGE_END_MASK;
			auto bytes = pls::JsonDocument<QJsonObject>().add(QStringLiteral("type"), type).add(QStringLiteral("data"), data).toJsonDocument().toJson(QJsonDocument::Compact);
			int length = static_cast<int>(bytes.length());
			sock->write((const char *)&beginMask, sizeof(beginMask));
			sock->write((const char *)&length, sizeof(length));
			sock->write(bytes);
			sock->write((const char *)&endMask, sizeof(endMask));
			sock->flush();
		} else {
			PLS_WARN(UTILS_API_MODULE, "IPC %s, Failed to send message because the connection has not been established yet", m_name.toUtf8().constData());
		}
	}

	int findBeginMask(const char *buffer, int offset, int end) const
	{
		for (; offset < end; ++offset) {
			if (int beginMask = *(int *)(buffer + offset); beginMask == IPC_MESSAGE_BEGIN_MASK) {
				return offset;
			}
		}
		return -1;
	}
	bool parse(pls_ipc_message_t &message, const QByteArray &bytes, int &offset, int end) const
	{
		if ((end - offset) < MIN_MESSAGE_LENGTH) {
			return false;
		}

		const char *buffer = bytes.data();
		if (int beginOffset = findBeginMask(buffer, offset, end - sizeof(int)); beginOffset >= 0) {
			offset = beginOffset;
		} else {
			offset = end - sizeof(int) + 1;
		}

		if ((end - offset) < MIN_MESSAGE_LENGTH) {
			return false;
		}

		// offset = begin mask offset
		int lengthOffset = offset + sizeof(int);
		int length = *(int *)(buffer + lengthOffset);
		int dataOffset = lengthOffset + sizeof(int);
		int endMaskOffset = dataOffset + length;
		if ((endMaskOffset + sizeof(int)) > end) {
			return false;
		} else if (int endMask = *(int *)(buffer + endMaskOffset); endMask != IPC_MESSAGE_END_MASK) {
			offset += sizeof(int); // error message, try find next message
			return false;
		}

		offset = endMaskOffset + sizeof(int);

		QJsonObject obj;
		if (QString error; !pls_parse_json(obj, QByteArray::fromRawData(buffer + dataOffset, length), &error)) {
			PLS_ERROR(UTILS_API_MODULE, "ipc parse json failed, error: %s", error.toUtf8().constData());
			return false;
		}

		message.type = obj[QStringLiteral("type")].toInt();
		message.data = obj[QStringLiteral("data")];
		PLS_INFO(UTILS_API_MODULE, "ipc receive message: type=%d", message.type);
		return true;
	}
	bool innerMessage(const pls_ipc_message_t &message)
	{
		switch (message.type) {
		case IPC_TYPE_PROCESS_ID:
			handleProcessIdMessage(message);
			return true;
		case IPC_TYPE_WAKE_UP:
			handleWakeUpMessage(message);
			return true;
		case IPC_TYPE_APP_ACTIVED:
			handleAppActivedMessage(message);
			return true;
		case IPC_TYPE_APP_ANY_WINDOW_SHOW:
			handleAnyWindowShowMessage(message);
			return true;
		case IPC_TYPE_APP_MAIN_WINDOW_SHOW:
			handleMainWindowShowMessage(message);
			return true;
		case IPC_TYPE_APP_ANY_WINDOW_ACTIVED:
			handleAnyWindowActivedMessage(message);
			return true;
		case IPC_TYPE_APP_MAIN_WINDOW_ACTIVED:
			handleMainWindowActivedMessage(message);
			return true;
		default:
			return false;
		}
	}

	void sendProcessIdMessage() { sendSync(IPC_TYPE_PROCESS_ID, QJsonObject{{QStringLiteral("processId"), static_cast<int>(pls_current_process_id())}}); }
	void handleProcessIdMessage(const pls_ipc_message_t &message)
	{
		uint32_t peer_process_id = m_peer_process_id;

		auto object = message.data.toObject();
		m_peer_process_id = object[QStringLiteral("processId")].toInt();
		if (peer_process_id != m_peer_process_id)
			pls_invoke_safe(m_on_inner_message, pls_ipc_inner_message_t::PeerProcessId, message.data);
	}
	void handleWakeUpMessage(const pls_ipc_message_t &message) { pls_invoke_safe(m_on_inner_message, pls_ipc_inner_message_t::WakeUp, message.data); }
	void handleAppActivedMessage(const pls_ipc_message_t &message) { pls_invoke_safe(m_on_inner_message, pls_ipc_inner_message_t::PeerAppActived, message.data); }
	void handleAnyWindowShowMessage(const pls_ipc_message_t &message) { pls_invoke_safe(m_on_inner_message, pls_ipc_inner_message_t::PeerAnyWindowShow, message.data); }
	void handleMainWindowShowMessage(const pls_ipc_message_t &message) { pls_invoke_safe(m_on_inner_message, pls_ipc_inner_message_t::PeerMainWindowShow, message.data); }
	void handleAnyWindowActivedMessage(const pls_ipc_message_t &message) { pls_invoke_safe(m_on_inner_message, pls_ipc_inner_message_t::PeerAnyWindowActived, message.data); }
	void handleMainWindowActivedMessage(const pls_ipc_message_t &message) { pls_invoke_safe(m_on_inner_message, pls_ipc_inner_message_t::PeerMainWindowActived, message.data); }
};
struct IpcClient : public IpcSocket, public IpcBase {
	int m_reconnect_timer = 0;
	QByteArray m_bytes;

	explicit IpcClient(const QString &name) : IpcBase(name) {}
	~IpcClient() override = default;

	IpcSocket *socket() override { return this; }

	void init() override
	{
		pls_async_call(this, [this]() {
			QObject::connect(this, &IpcSocket::connected, this, &IpcClient::onConnected);
			QObject::connect(this, &IpcSocket::disconnected, this, &IpcClient::onDisconnected);
			QObject::connect(this, &IpcSocket::errorOccurred, this, &IpcClient::onErrorOccurred);
			QObject::connect(this, &IpcSocket::readyRead, this, &IpcClient::onReadyRead);
			m_state = IpcConnectState::Connecting;
			m_peer_process_id = -1;
			m_bytes.clear();
			connectToServer(m_name);
		});
	}
	void destroy() override
	{
		auto thread = this->thread();
		deleteLater();
		pls_delete_thread(thread);
	}

	void connect()
	{
		if ((m_state == IpcConnectState::Connecting) || (m_state == IpcConnectState::Connected))
			return;
		pls_async_call(this, [this]() {
			m_state = IpcConnectState::Connecting;
			m_peer_process_id = -1;
			m_bytes.clear();
			connectToServer(m_name);
		});
	}

	void stopReconnectTimer()
	{
		if (m_reconnect_timer > 0) {
			killTimer(m_reconnect_timer);
			m_reconnect_timer = 0;
		}
	}

	void onConnected()
	{
		PLS_INFO(UTILS_API_MODULE, "ipc client %s connected", m_name.toUtf8().constData());
		stopReconnectTimer();

		m_state = IpcConnectState::Connected;
		sendProcessIdMessage();
		pls_invoke_safe(m_on_connected, this);
	}
	void onDisconnected()
	{
		PLS_INFO(UTILS_API_MODULE, "ipc client %s disconnected. error: %s", m_name.toUtf8().constData(), errorString().toUtf8().constData());
		stopReconnectTimer();

		m_state = IpcConnectState::Disconnected;
		m_peer_process_id = -1;
		m_bytes.clear();
		pls_invoke_safe(m_on_disconnected, this);

		PLS_DEBUG(UTILS_API_MODULE, "ipc client try connect %s again after 3s", m_name.toUtf8().constData());
		m_state = IpcConnectState::Reconnecting;
		m_reconnect_timer = startTimer(3000);
	}
	void onErrorOccurred(QLocalSocket::LocalSocketError)
	{
		stopReconnectTimer();
		switch (m_state) {
		case IpcConnectState::Connecting:
			PLS_DEBUG(UTILS_API_MODULE, "ipc client connect %s failed, error: %s, try again after 3s", m_name.toUtf8().constData(), errorString().toUtf8().constData());
			m_state = IpcConnectState::Reconnecting;
			m_reconnect_timer = startTimer(3000);
			break;
		default:
			PLS_DEBUG(UTILS_API_MODULE, "ipc client %s error occurred, error: %s", m_name.toUtf8().constData(), errorString().toUtf8().constData());
			break;
		}
	}
	void onReadyRead()
	{
		m_bytes.append(readAll());
		if (m_bytes.length() < MIN_MESSAGE_LENGTH)
			return;

		int offset = 0;
		int end = static_cast<int>(m_bytes.length());
		for (pls_ipc_message_t message; parse(message, m_bytes, offset, end);) {
			if (!innerMessage(message)) {
				pls_invoke_safe(m_on_message, this, message);
			}
		}

		if (offset >= end) {
			m_bytes.clear();
		} else if (offset > 0) {
			m_bytes = m_bytes.mid(offset);
		}
	}

	void timerEvent(QTimerEvent *event) override
	{
		if (m_reconnect_timer == event->timerId()) {
			stopReconnectTimer();
			if (!((m_state == IpcConnectState::Connecting) || (m_state == IpcConnectState::Connected))) {
				m_state = IpcConnectState::Connecting;
				m_peer_process_id = -1;
				m_bytes.clear();
				connectToServer(m_name);
			}
		}

		QObject::timerEvent(event);
	}
};
struct IpcServer : public QLocalServer, public IpcBase {
	QPointer<IpcSocket> m_client;
	QByteArray m_bytes;

	explicit IpcServer(const QString &name) : IpcBase(name) {}
	~IpcServer() override = default;

	IpcSocket *socket() override { return m_client; }

	void init() override
	{
		pls_async_call(this, [this]() {
			setMaxPendingConnections(1);
			removeServer(m_name);
			if (listen(m_name)) {
				PLS_INFO(UTILS_API_MODULE, "ipc server %s listen success", m_name.toUtf8().constData());
			} else {
				PLS_ERROR(UTILS_API_MODULE, "ipc server %s listen failed, error: %s", m_name.toUtf8().constData(), errorString().toUtf8().constData());
			}
		});
	}
	void destroy() override
	{
		auto thread = this->thread();
		if (m_client)
			m_client->deleteLater();
		deleteLater();
		pls_delete_thread(thread);
	}

	void incomingConnection(quintptr socketDescriptor) override
	{
		if (m_client)
			m_client->deleteLater();
		m_peer_process_id = -1;
		m_bytes.clear();

		auto client = pls_new<IpcSocket>();
		client->setSocketDescriptor(socketDescriptor);
		m_client = client;

		QObject::connect(client, &IpcSocket::disconnected, this, &IpcServer::onDisconnected);
		QObject::connect(client, &IpcSocket::errorOccurred, this, &IpcServer::onErrorOccurred);
		QObject::connect(client, &IpcSocket::readyRead, this, &IpcServer::onReadyRead);

		onConnected();
	}

	void onConnected()
	{
		PLS_INFO(UTILS_API_MODULE, "ipc server %s connected.", m_name.toUtf8().constData());
		m_state = IpcConnectState::Connected;
		sendProcessIdMessage();
		pls_invoke_safe(m_on_connected, this);
	}
	void onDisconnected()
	{
		PLS_INFO(UTILS_API_MODULE, "ipc server %s disconnected.", m_name.toUtf8().constData());
		m_state = IpcConnectState::Disconnected;
		m_peer_process_id = -1;
		m_bytes.clear();
		pls_invoke_safe(m_on_disconnected, this);

		if (m_client)
			m_client->deleteLater();
	}
	void onErrorOccurred(QLocalSocket::LocalSocketError)
	{
		PLS_DEBUG(UTILS_API_MODULE, "ipc server %s error occurred, error: %s", m_name.toUtf8().constData(), errorString().toUtf8().constData());
	}
	void onReadyRead()
	{
		if (!m_client)
			return;

		m_bytes.append(m_client->readAll());
		if (m_bytes.length() < MIN_MESSAGE_LENGTH)
			return;

		int offset = 0;
		int end = static_cast<int>(m_bytes.length());
		for (pls_ipc_message_t message; parse(message, m_bytes, offset, end);) {
			if (!innerMessage(message)) {
				pls_invoke_safe(m_on_message, this, message);
			}
		}

		if (offset >= end) {
			m_bytes.clear();
		} else if (offset > 0) {
			m_bytes = m_bytes.mid(offset);
		}
	}
};
template<typename T> T *pls_ipc_create(const QString &name)
{
	auto thread = pls_new<QThread>();
	thread->start();

	auto ipc = pls_new<T>(name);
	ipc->moveToThread(thread);
	ipc->init();
	return ipc;
}
}
LIBUTILSAPI_API pls_ipc_t pls_ipc_listen(const QString &name)
{
	return pls_ipc_create<IpcServer>(name);
}
LIBUTILSAPI_API pls_ipc_t pls_ipc_connect(const QString &name)
{
	return pls_ipc_create<IpcClient>(name);
}
LIBUTILSAPI_API void pls_ipc_destroy(pls_ipc_t ipc)
{
	if (auto base = dynamic_cast<IpcBase *>(ipc.data()); base)
		base->destroy();
}
LIBUTILSAPI_API void pls_ipc_connect(pls_ipc_t ipc)
{
	if (auto client = dynamic_cast<IpcClient *>(ipc.data()); client)
		client->connect();
}
LIBUTILSAPI_API bool pls_ipc_is_connected(pls_ipc_t ipc)
{
	if (auto base = dynamic_cast<IpcBase *>(ipc.data()); base)
		return base->isConnected();
	return false;
}
LIBUTILSAPI_API uint32_t pls_ipc_get_peer_process_id(pls_ipc_t ipc)
{
	if (auto base = dynamic_cast<IpcBase *>(ipc.data()); base)
		return base->m_peer_process_id;
	return -1;
}
LIBUTILSAPI_API void pls_ipc_on_connected(pls_ipc_t ipc, const pls_ipc_on_connected_t &on_connected)
{
	pls_async_call(ipc.data(), [ipc, on_connected]() {
		if (auto base = dynamic_cast<IpcBase *>(ipc.data()); base) {
			base->m_on_connected = on_connected;
		}
	});
}
LIBUTILSAPI_API void pls_ipc_on_disconnected(pls_ipc_t ipc, const pls_ipc_on_disconnected_t &on_disconnected)
{
	pls_async_call(ipc.data(), [ipc, on_disconnected]() {
		if (auto base = dynamic_cast<IpcBase *>(ipc.data()); base) {
			base->m_on_disconnected = on_disconnected;
		}
	});
}
LIBUTILSAPI_API void pls_ipc_on_message(pls_ipc_t ipc, const pls_ipc_on_message_t &on_message)
{
	pls_async_call(ipc.data(), [ipc, on_message]() {
		if (auto base = dynamic_cast<IpcBase *>(ipc.data()); base) {
			base->m_on_message = on_message;
		}
	});
}
LIBUTILSAPI_API void pls_ipc_on_inner_message(pls_ipc_t ipc, const pls_ipc_on_inner_message_t &on_inner_message)
{
	pls_async_call(ipc.data(), [ipc, on_inner_message]() {
		if (auto base = dynamic_cast<IpcBase *>(ipc.data()); base) {
			base->m_on_inner_message = on_inner_message;
		}
	});
}
LIBUTILSAPI_API void pls_ipc_send(pls_ipc_t ipc, int type, const QJsonValue &data)
{
	pls_async_call(ipc.data(), [ipc, type, data]() {
		if (auto base = dynamic_cast<IpcBase *>(ipc.data()); base) {
			base->send(type, data);
		}
	});
}
LIBUTILSAPI_API void pls_ipc_send_wake_up(pls_ipc_t ipc, const QStringList &args)
{
	pls_ipc_send(ipc, IPC_TYPE_WAKE_UP, pls_to_json_array(args));
}
LIBUTILSAPI_API void pls_ipc_send_app_actived(pls_ipc_t ipc, bool actived)
{
	pls_ipc_send(ipc, IPC_TYPE_APP_ACTIVED, actived);
}
LIBUTILSAPI_API void pls_ipc_send_any_window_show(pls_ipc_t ipc)
{
	pls_ipc_send(ipc, IPC_TYPE_APP_ANY_WINDOW_SHOW, QJsonValue());
}
LIBUTILSAPI_API void pls_ipc_send_main_window_show(pls_ipc_t ipc)
{
	pls_ipc_send(ipc, IPC_TYPE_APP_MAIN_WINDOW_SHOW, QJsonValue());
}
LIBUTILSAPI_API void pls_ipc_send_any_window_actived(pls_ipc_t ipc)
{
	pls_ipc_send(ipc, IPC_TYPE_APP_ANY_WINDOW_ACTIVED, QJsonValue());
}
LIBUTILSAPI_API void pls_ipc_send_main_window_actived(pls_ipc_t ipc)
{
	pls_ipc_send(ipc, IPC_TYPE_APP_MAIN_WINDOW_ACTIVED, QJsonValue());
}

LIBUTILSAPI_API void pls_get_current_datetime(pls_datetime_t &datetime)
{
#if defined(Q_OS_WIN)
	SYSTEMTIME st = {0};
	GetLocalTime(&st);
	datetime.year = st.wYear;
	datetime.month = st.wMonth;
	datetime.day = st.wDay;
	datetime.hour = st.wHour;
	datetime.minute = st.wMinute;
	datetime.second = st.wSecond;
	datetime.milliseconds = st.wMilliseconds;

	TIME_ZONE_INFORMATION tzi = {0};
	switch (GetTimeZoneInformation(&tzi)) {
	case TIME_ZONE_ID_STANDARD:
		datetime.timezone = tzi.Bias + tzi.StandardBias;
		break;
	case TIME_ZONE_ID_DAYLIGHT:
		datetime.timezone = tzi.Bias + tzi.DaylightBias;
		break;
	default:
		datetime.timezone = tzi.Bias;
		break;
	}
#else
	pls_mac_datetime_t mac_date_time;
	pls_libutil_api_mac::pls_get_current_datetime(mac_date_time);
	datetime.year = mac_date_time.year;
	datetime.month = mac_date_time.month;
	datetime.day = mac_date_time.day;
	datetime.hour = mac_date_time.hour;
	datetime.minute = mac_date_time.minute;
	datetime.second = mac_date_time.second;
	datetime.milliseconds = mac_date_time.milliseconds;
	datetime.timezone = mac_date_time.timezone;
#endif
}
LIBUTILSAPI_API QString pls_datetime_to_string(const pls_datetime_t &datetime) // yyyy-mm-dd hh:mm:ss.zzz+00:00
{
	pls::chars<40> buf;
#if defined(Q_OS_WIN)
	qsnprintf(buf.data(), buf.size(), "%04u-%02u-%02u %02u:%02u:%02u.%03u%+03d:%02d", datetime.year, datetime.month, datetime.day, datetime.hour, datetime.minute, datetime.second,
		  datetime.milliseconds, (0 - datetime.timezone / 60), (abs(datetime.timezone) % 60));
#else
	qsnprintf(buf.data(), buf.size(), "%04u-%02u-%02u %02u:%02u:%02u.%03u%+03d:%02d", datetime.year, datetime.month, datetime.day, datetime.hour, datetime.minute, datetime.second,
		  datetime.milliseconds, (datetime.timezone / 3600), (abs(datetime.timezone) % 3600));
#endif
	return QString::fromUtf8(buf.data());
}

LIBUTILSAPI_API time_t pls_datetime_to_time_t(const pls_datetime_t &datetime)
{
	struct tm tm = {0};
	tm.tm_year = datetime.year - 1900;
	tm.tm_mon = datetime.month - 1;
	tm.tm_mday = datetime.day;
	tm.tm_hour = datetime.hour;
	tm.tm_min = datetime.minute;
	tm.tm_sec = datetime.second;
	tm.tm_isdst = -1;
	return mktime(&tm);
}

LIBUTILSAPI_API uint64_t pls_datetime_to_ms(const pls_datetime_t &datetime)
{
	return static_cast<uint64_t>(pls_datetime_to_time_t(datetime)) * 1000 + datetime.milliseconds;
}

LIBUTILSAPI_API QMap<QString, QString> pls_parse(const QString &str, const QRegularExpression &delimiter)
{
	QMap<QString, QString> map;
	for (const QString &it : str.trimmed().split(delimiter, Qt::SkipEmptyParts)) {
		if (QString kv = it.trimmed(); !kv.isEmpty()) {
			if (auto pos = kv.indexOf('='); pos > 0) {
				map.insert(kv.left(pos), kv.mid(pos + 1));
			}
		}
	}
	return map;
}

LIBUTILSAPI_API void pls_qapp_construct_add_cb(const pls_qapp_cb_t &qapp_cb)
{
	g_hook.add_qapp_cbs(g_hook.m_qapp_construct_cbs, qapp_cb);
}
LIBUTILSAPI_API void pls_qapp_deconstruct_add_cb(const pls_qapp_cb_t &qapp_cb)
{
	g_hook.add_qapp_cbs(g_hook.m_qapp_deconstruct_cbs, qapp_cb);
}
LIBUTILSAPI_API void pls_qapp_construct()
{
	g_hook.call_qapp_cbs(g_hook.m_qapp_construct_cbs);
	pls::network_state_start();
}
LIBUTILSAPI_API void pls_qapp_deconstruct()
{
	pls::network_state_stop();
	g_hook.call_qapp_cbs_r(g_hook.m_qapp_deconstruct_cbs);
	destroy_async_invoke_threads();
}

LIBUTILSAPI_API bool pls_is_main_thread(const QThread *thread)
{
	if (auto app = qApp; app) {
		return thread == app->thread();
	}
	return false;
}
LIBUTILSAPI_API bool pls_current_is_main_thread()
{
	return pls_is_main_thread(QThread::currentThread());
}

LIBUTILSAPI_API QString pls_gen_uuid()
{
	return QUuid::createUuid().toString(QUuid::Id128);
}
#if defined(Q_OS_MACOS)
static bool projectIsDebugged(int pid)
{
	static std::unordered_map<int, bool> isGotMap;
	auto it = isGotMap.find(pid);
	if (it != isGotMap.end()) {
		return it->second;
	}

	int junk;
	int mib[4];
	struct kinfo_proc info;
	size_t size;

	info.kp_proc.p_flag = 0;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = (pid == -1) ? getpid() : pid;

	size = sizeof(info);
	junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
	assert(junk == 0);

	isGotMap[pid] = ((info.kp_proc.p_flag & P_TRACED) != 0);
	return isGotMap[pid];
}
#endif

LIBUTILSAPI_API bool pls_is_debugger_present(int pid)
{
#if defined(Q_OS_WIN)
	return IsDebuggerPresent();
#elif defined(Q_OS_MACOS)
	return projectIsDebugged(pid);
#endif
}

LIBUTILSAPI_API void pls_printf(const wchar_t *text)
{
#if defined(Q_OS_WIN)
	if (IsDebuggerPresent()) {
		OutputDebugStringW(text);
	} else {
		std::wcout << text;
	}
#elif defined(Q_OS_MACOS)
	Q_ASSERT(false); //use pls_mac_printf instead
	std::wcout << text;
#endif
}

LIBUTILSAPI_API void pls_printf(const std::wstring &text)
{
	pls_printf(text.c_str());
}
LIBUTILSAPI_API void pls_printf(const QString &text)
{
	pls_printf(text.toStdWString().c_str());
}
LIBUTILSAPI_API void pls_printf(const QByteArray &utf8)
{
	pls_printf(QString::fromUtf8(utf8));
}
LIBUTILSAPI_API void pls_printf(const std::string &utf8)
{
	pls_printf(QString::fromStdString(utf8));
}

LIBUTILSAPI_API void pls_mac_printf(const char *format, ...)
{

#if defined(Q_OS_WIN)
	Q_ASSERT(false); //use pls_printf instead
#elif defined(Q_OS_MACOS)
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
#endif
}

LIBUTILSAPI_API void pls_sync_invoke(QObject *object, const std::function<void()> &fn)
{
	if (QThread::currentThread() != object->thread()) {
		QMetaObject::invokeMethod(object, fn, Qt::BlockingQueuedConnection);
	} else {
		QMetaObject::invokeMethod(object, fn, Qt::DirectConnection);
	}
}
LIBUTILSAPI_API void pls_async_invoke(QObject *object, const std::function<void()> &fn)
{
	QMetaObject::invokeMethod(object, fn, Qt::QueuedConnection);
}
LIBUTILSAPI_API void pls_async_invoke(QObject *object, const char *fn)
{
	QMetaObject::invokeMethod(object, fn, Qt::QueuedConnection);
}
struct async_invoke_t : public QThread {
	std::function<void(QThread *thread)> m_fn;
	explicit async_invoke_t(const std::function<void(QThread *thread)> &fn) : m_fn(fn) { LocalGlobalVars::g_async_invoke_threads.append(this); }
	~async_invoke_t() final { LocalGlobalVars::g_async_invoke_threads.removeOne(this); }
	void run() override { pls_invoke_safe(m_fn, this); }
};
LIBUTILSAPI_API void pls_async_invoke(const std::function<void()> &fn)
{
	pls_async_invoke([fn](QThread *) { pls_invoke_safe(fn); });
}
LIBUTILSAPI_API void pls_async_invoke(const std::function<void(QThread *thread)> &fn)
{
	if (pls_current_is_main_thread()) {
		QPointer<QThread> thread = pls_new<async_invoke_t>(fn);
		QObject::connect(thread.get(), &QThread::finished, qApp, [thread]() { pls_delete_thread(thread); }, Qt::QueuedConnection);
		thread->start();
	} else {
		pls_async_call_mt([fn]() { pls_async_invoke(fn); });
	}
}

LIBUTILSAPI_API QVariant pls_get_qsetting_value(pls_product_type_t product_type, const QString &key, const QVariant &defaultVal)
{
	auto product_name = get_qsetting_product_name(product_type);
	if (product_name.isEmpty())
		return defaultVal;

#if defined(Q_OS_WIN)
	QSettings setting(QStringLiteral("NAVER Corporation"), product_name);
#elif defined(Q_OS_MACOS)
	QSettings setting(QStringLiteral("prismlive"), product_name);
#endif
	return setting.value(key, defaultVal);
}
LIBUTILSAPI_API void pls_set_qsetting_value(pls_product_type_t product_type, const QString &key, const QVariant &value)
{
	auto product_name = get_qsetting_product_name(product_type);
	if (product_name.isEmpty())
		return;

#if defined(Q_OS_WIN)
	QSettings setting(QStringLiteral("NAVER Corporation"), product_name);
#elif defined(Q_OS_MACOS)
	QSettings setting(QStringLiteral("prismlive"), product_name);
#endif
	setting.setValue(key, value);
}
LIBUTILSAPI_API QVariant pls_get_qsetting_value(const QString &key, const QVariant &defaultVal)
{
	return pls_get_qsetting_value(pls_product_type(), key, defaultVal);
}
LIBUTILSAPI_API void pls_set_qsetting_value(const QString &key, const QVariant &value)
{
	pls_set_qsetting_value(pls_product_type(), key, value);
}

LIBUTILSAPI_API bool pls_is_dev()
{
	if (!LocalGlobalVars::g_is_dev.has_value()) {
		std::lock_guard guard(pls_global_mutex());
		if (!LocalGlobalVars::g_is_dev.has_value()) {
			LocalGlobalVars::g_is_dev = pls_get_qsetting_value(QStringLiteral("DevServer")).toBool();
		}
	}
	return LocalGlobalVars::g_is_dev.value();
}

LIBUTILSAPI_API bool pls_save_local_log()
{
	if (!LocalGlobalVars::g_is_local_log.has_value()) {
		std::lock_guard guard(pls_global_mutex());
		if (!LocalGlobalVars::g_is_local_log.has_value()) {
			LocalGlobalVars::g_is_local_log = pls_get_qsetting_value(QStringLiteral("LocalLog")).toBool();
		}
	}
	return LocalGlobalVars::g_is_local_log.value();
}

static int locale2languageID(const QString &locale, int defaultLanguageID = 1033)
{
	const auto locales = pls_get_locales();
	if (auto iter = locales.find(locale); iter != locales.end()) {
		return iter.value()[pls_locale_attr_t::LID].toInt();
	}
	return defaultLanguageID;
}
#if defined(Q_OS_MACOS)
static QString findSystemMacthedlanguage(const QMap<QString /* en-US, ko-KR, pt-BR, ... */, QMap<pls_locale_attr_t, QVariant>> &locales)
{
	QString languageDesignator = pls_libutil_api_mac::pls_get_system_language().split("-").first();
	if (languageDesignator.isEmpty()) {
		return {};
	}
	for (const auto &item : locales.keys()) {
		if (item.startsWith(languageDesignator)) {
			return item;
		}
	}
	return {};
}
#endif

static QString languageID2Locale(int languageID, const QString &defaultLanguage = "en-US")
{
	const auto locales = pls_get_locales();
	for (auto iter = locales.begin(); iter != locales.end(); ++iter) {
		if (iter.value()[pls_locale_attr_t::LID].toInt() == languageID) {
			return iter.key();
		}
	}

#if defined(Q_OS_MACOS)
	QString syslanguage = findSystemMacthedlanguage(locales);
	if (!syslanguage.isEmpty()) {
		pls_set_locale(syslanguage);
		return syslanguage;
	}
#endif
	return defaultLanguage;
}

LIBUTILSAPI_API QMap<QString /* en-US, ko-KR, pt-BR, ... */, QMap<pls_locale_attr_t, QVariant>> pls_get_locales()
{
	if (LocalGlobalVars::g_locales.isEmpty()) {
		std::lock_guard guard(pls_global_mutex());
		if (LocalGlobalVars::g_locales.isEmpty()) {
			QSettings settings(pls_get_app_data_path_pn(QStringLiteral("/locale.ini")), QSettings::IniFormat);
			for (const auto &group : settings.childGroups()) {
				settings.beginGroup(group);
				auto lid = settings.value(QStringLiteral("LID")).toInt();
				auto name = settings.value(QStringLiteral("Name")).toString();
				settings.endGroup();

				auto &locale = LocalGlobalVars::g_locales[group];
				locale[pls_locale_attr_t::LID] = lid;
				locale[pls_locale_attr_t::Name] = name;
			}
		}
	}
	return LocalGlobalVars::g_locales;
}

LIBUTILSAPI_API void pls_set_locale(const QString &locale)
{
	LocalGlobalVars::g_locale = locale;
#if defined(Q_OS_WIN)
	QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") + get_product_name(), QSettings::NativeFormat);
	int languageID = locale2languageID(locale);
	settings.setValue("InstallLanguage", languageID);
	settings.sync();
#elif defined(Q_OS_MACOS)
	QSettings settings(QStringLiteral("prismlive"), pls_product_is_prism() ? QStringLiteral("prismlivestudio") : QStringLiteral("camstudio"));
	int languageID = locale2languageID(locale);
	settings.setValue("InstallLanguage", languageID);
	settings.sync();
#endif
}

LIBUTILSAPI_API QString pls_get_locale()
{
	if (!LocalGlobalVars::g_locale) {
		std::lock_guard guard(pls_global_mutex());
		if (!LocalGlobalVars::g_locale) {
#if defined(Q_OS_WIN)
			QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") + get_product_name(), QSettings::NativeFormat);
			int languageID = settings.value("InstallLanguage").toInt();
			LocalGlobalVars::g_locale = languageID2Locale(languageID);
#elif defined(Q_OS_MACOS)
			QSettings settings(QStringLiteral("prismlive"), pls_product_is_prism() ? QStringLiteral("prismlivestudio") : QStringLiteral("camstudio"));
			int languageID = settings.value("InstallLanguage").toInt();
			LocalGlobalVars::g_locale = languageID2Locale(languageID);
			if (LocalGlobalVars::g_locale.value().isEmpty()) {
				LocalGlobalVars::g_locale = QLocale::system().name();
			}
#endif
		}
	}

	return LocalGlobalVars::g_locale.value(); //en-US
}
LIBUTILSAPI_API QString pls_get_locale_dir()
{
	return pls_get_app_data_path(QStringLiteral("/locale"));
}

LIBUTILSAPI_API QString pls_get_current_language()
{
	return pls_get_locale();
}
LIBUTILSAPI_API QString pls_get_current_language_short_str()
{
	return pls_get_locale().section(QRegularExpression("\\W+"), 0, 0);
}
LIBUTILSAPI_API QString pls_get_current_country_short_str()
{
	return pls_get_locale().section(QRegularExpression("\\W+"), 1, 1);
}

LIBUTILSAPI_API QString pls_get_local_ip()
{
	static std::optional<QString> ipAddr = std::nullopt;
	static bool networkStaus = false;
	auto currentNetworkStatus = pls::NetworkState::instance()->isAvailable();
	if (!ipAddr.has_value() || networkStaus != currentNetworkStatus) {
		networkStaus = currentNetworkStatus;
		for (const auto &addr : QNetworkInterface::allAddresses()) {
			if (!addr.isLoopback() && (addr.protocol() == QAbstractSocket::IPv4Protocol)) {
				ipAddr = addr.toString();
				break;
			}
		}
	}
	return ipAddr.value_or(QString());
}

LIBUTILSAPI_API QString pls_get_local_mac()
{
	static std::optional<QString> macAddr = std::nullopt;
	static bool networkStaus = false;
	auto currentNetworkStatus = pls::NetworkState::instance()->isAvailable();
	if (!macAddr.has_value() || networkStaus != currentNetworkStatus) {
		networkStaus = currentNetworkStatus;
		for (const auto &ni : QNetworkInterface::allInterfaces()) {
			auto flags = ni.flags();
			if (flags.testFlag(QNetworkInterface::IsUp) && flags.testFlag(QNetworkInterface::IsRunning) && !flags.testFlag(QNetworkInterface::IsLoopBack)) {
				macAddr = ni.hardwareAddress();
				break;
			}
		}
	}
	return macAddr.value_or(QString());
}

LIBUTILSAPI_API QList<QHostAddress> pls_get_valid_hosts()
{
	auto interfaces = QNetworkInterface::allInterfaces();

	qDebug() << "net interface fond" << interfaces.size();

	QList<QHostAddress> validAddresses;
	for (const auto &inter : interfaces) {
		if (!inter.isValid()) {
			qDebug() << " invalid interface " << inter;
			continue;
		}
		auto type = inter.type();
		pls_used(type);
		if (type != QNetworkInterface::Ethernet && type != QNetworkInterface::Wifi) {
			qDebug() << " not wifi or ethernet " << inter;
			continue;
		}
		auto flags = inter.flags();
		pls_used(flags);
		if (flags.testFlag(QNetworkInterface::IsLoopBack) || !flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning) ||
		    !flags.testFlag(QNetworkInterface::CanBroadcast) || !flags.testFlag(QNetworkInterface::CanMulticast) || flags.testFlag(QNetworkInterface::IsPointToPoint)) {
			qDebug() << " flags not right " << inter;
			continue;
		}
		auto entries = inter.addressEntries();
		for (auto entry : entries) {

			auto dnsEli = entry.dnsEligibility();
			pls_used(dnsEli);
#if _WIN32
			if (dnsEli != QNetworkAddressEntry::DnsEligible) {
				qDebug() << " dns is not compable" << entry.ip();
				continue;
			}
#endif
			auto ip = entry.ip();
			if (ip.isLoopback() || !ip.isGlobal() || ip.isLinkLocal() || ip.isSiteLocal() || ip.isUniqueLocalUnicast() || ip.isMulticast() || ip.isBroadcast() ||
			    ip.protocol() != QAbstractSocket::IPv4Protocol) {
				qDebug() << " local address";
				continue;
			}
			validAddresses.append(ip);
		}
	}

	qDebug() << " available ips " << validAddresses;
	return validAddresses;
}

LIBUTILSAPI_API const char *pls_bool_2_string(bool b)
{
	return b ? "true" : "false";
}

LIBUTILSAPI_API bool pls_show_in_graphical_shell(const QString &pathIn)
{
	const QFileInfo fileInfo(pathIn);

	if (!QFile(pathIn).exists()) {
		QFileInfo fi(pathIn);
		auto path = fi.absolutePath();
		if (path.isEmpty() || path.isNull())
			return false;
		if (!pls_mkdir(path))
			return false;

		QFile file(pathIn);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
			return false;
		QTextStream out(&file);
		out << Qt::endl << Qt::flush;
		file.close();
	}

#if _WIN32
	QString cmd = "explorer.exe";
	QStringList param;
	param += QLatin1String("/select,");
	param += QDir::toNativeSeparators(fileInfo.canonicalFilePath());
	return QProcess::startDetached(cmd, param);
#else
	QStringList scriptArgs;
	scriptArgs << QLatin1String("-e") << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"").arg(fileInfo.canonicalFilePath());
	QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
	scriptArgs.clear();
	scriptArgs << QLatin1String("-e") << QLatin1String("tell application \"Finder\" to activate");
	QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
	return true;
#endif // _WIN32
}

LIBUTILSAPI_API void pls_bring_window_to_front(WId winId)
{
#if defined(Q_OS_WIN)
	pls_bring_win_window_to_front(winId);
#elif defined(Q_OS_MACOS)
	pls_bring_mac_window_to_front(winId);
#endif
}

#ifdef Q_OS_MACOS
LIBUTILSAPI_API pls_mac_ver_t pls_get_mac_systerm_ver()
{
	return pls_libutil_api_mac::pls_get_mac_systerm_ver();
}

LIBUTILSAPI_API bool pls_unZipFile(const QString &dstDirPath, const QString &srcFilePath)
{
	return pls_libutil_api_mac::unZip(dstDirPath, srcFilePath);
}

LIBUTILSAPI_API bool pls_zipFile(const QString &destZipPath, const QString &sourceDirName, const QString &sourceFolderPath)
{
	return pls_libutil_api_mac::zip(destZipPath, sourceDirName, sourceFolderPath);
}

LIBUTILSAPI_API bool pls_copy_file_with_error_code(const QString &fileName, const QString &newName, bool overwrite, int &errorCode)
{
	return pls_libutil_api_mac::pls_copy_file(fileName, newName, overwrite, errorCode);
}

LIBUTILSAPI_API QString pls_get_system_identifier()
{
	return pls_libutil_api_mac::pls_get_system_identifier();
}

LIBUTILSAPI_API bool pls_file_is_existed(const QString &filePath)
{
	return pls_libutil_api_mac::pls_file_is_existed(filePath);
}

LIBUTILSAPI_API bool pls_install_mac_package(const QString &unzipFolderPath, const QString &destBundlePath, const std::string &prismSession, const std::string &prismGcc, const char *version)
{
	return pls_libutil_api_mac::install_mac_package(unzipFolderPath, destBundlePath, prismSession, prismGcc, version);
}

LIBUTILSAPI_API bool pls_restart_mac_app(const QStringList &arguments)
{
	return pls_libutil_api_mac::pls_restart_mac_app(arguments);
}

LIBUTILSAPI_API QString pls_get_existed_downloaded_mac_app(const QString &downloadedBundleDir, const QString &downloadedVersion, bool deleteBundleExceptUpdatedBundle)
{
	return pls_libutil_api_mac::pls_get_existed_downloaded_mac_app(downloadedBundleDir, downloadedVersion, deleteBundleExceptUpdatedBundle);
}

LIBUTILSAPI_API bool pls_remove_all_downloaded_mac_app_small_equal_version(const QString &downloadedBundleDir, const QString &softwareVersion)
{
	return pls_libutil_api_mac::pls_remove_all_downloaded_mac_app_small_equal_version(downloadedBundleDir, softwareVersion);
}

LIBUTILSAPI_API void pls_bring_mac_window_to_front(WId winId)
{
	pls_libutil_api_mac::bring_mac_window_to_front(winId);
}

LIBUTILSAPI_API QUrl pls_mac_hmac_url(const QUrl &url, const QByteArray &hmacKey)
{
	return pls_libutil_api_mac::build_mac_hmac_url(url, hmacKey);
}

QString pls_get_app_resource_dir()
{
	return pls_libutil_api_mac::pls_get_app_resource_dir();
}

LIBUTILSAPI_API QString pls_get_app_plugin_dir()
{
	return pls_libutil_api_mac::pls_get_app_plugin_dir();
}

LIBUTILSAPI_API QString pls_get_app_macos_dir()
{
	return pls_libutil_api_mac::pls_get_app_macos_dir();
}

LIBUTILSAPI_API QString pls_get_bundle_dir()
{
	return pls_libutil_api_mac::pls_get_bundle_dir();
}

LIBUTILSAPI_API bool pls_is_install_app(const QString &identifier, QString &appPath)
{
	return pls_libutil_api_mac::pls_is_install_app(identifier, appPath);
}

LIBUTILSAPI_API QString pls_get_install_app(const QString &identifier)
{
	return pls_libutil_api_mac::pls_get_install_app(identifier);
}

bool pls_check_mac_app_is_existed(const wchar_t *executableName)
{
	return pls_libutil_api_mac::pls_check_mac_app_is_existed(executableName);
}

bool pls_activiate_mac_app()
{
	return pls_libutil_api_mac::pls_activiate_app();
}

bool pls_activiate_mac_app_except_self()
{
	return pls_libutil_api_mac::pls_activiate_mac_app_except_self();
}

bool pls_activiate_mac_app(int pid)
{
	return pls_libutil_api_mac::pls_activiate_app_pid(pid);
}

QString pls_get_mac_app_data_dir()
{
	return pls_libutil_api_mac::pls_get_mac_app_data_dir();
}

LIBUTILSAPI_API void pls_add_monitor_process_exit_event(pls_process_t *process)
{
	if (!process) {
		return;
	}
	pls_libutil_api_mac::addObserverProcessExit(process->m_process);
}

LIBUTILSAPI_API void pls_remove_monitor_process_exit_event(pls_process_t *process)
{
	pls_libutil_api_mac::removeObserverProcessExit(process->m_process);
}

LIBUTILSAPI_API void pls_application_show_dock_icon(bool isShow)
{
	pls_libutil_api_mac::pls_application_show_dock_icon(isShow);
}

LIBUTILSAPI_API QString pls_get_app_content_dir()
{
	return pls_libutil_api_mac::pls_get_app_content_dir();
}

LIBUTILSAPI_API bool pls_is_app_running(const char *bundle_id)
{
	return pls_libutil_api_mac::pls_is_app_running(bundle_id);
}

LIBUTILSAPI_API bool pls_launch_app(const char *bundle_id, const char *app_name)
{
	return pls_libutil_api_mac::pls_launch_app(bundle_id, app_name);
}

LIBUTILSAPI_API QString pls_get_current_system_language_id()
{
	return pls_libutil_api_mac::pls_get_current_system_language_id();
}

#endif

LIBUTILSAPI_API QString pls_get_os_ver_string()
{
#ifdef Q_OS_WIN
	auto wv = pls_get_win_ver();
	return QString("Windows %1.%2.%3.%4").arg(wv.major).arg(wv.minor).arg(wv.build).arg(wv.revis);
#else
	pls_mac_ver_t ver = pls_get_mac_systerm_ver();
	return QString("Mac %1.%2.%3").arg(ver.major).arg(ver.minor).arg(ver.patch);
#endif
}
LIBUTILSAPI_API QString pls_get_user_agent()
{
#ifdef Q_OS_WIN
	LANGID langId = GetUserDefaultUILanguage();
	auto mv = pls_get_win_ver();
	return QString("%1/%2 (Windows %3 Build %4 Architecture x64 Language %5)").arg(get_product_name()).arg(pls_get_prism_version_string()).arg(mv.major).arg(mv.build).arg(langId);
#else
	pls_mac_ver_t mv = pls_get_mac_systerm_ver();
	QString langId = pls_get_current_system_language_id();
	return QString("%1/%2 (MacOS %3 Build %4 Architecture arm Language %5)").arg(get_product_name()).arg(pls_get_prism_version_string()).arg(mv.major).arg(mv.buildNum.c_str()).arg(langId);
#endif
}

LIBUTILSAPI_API std::wstring pls_utf8_to_unicode(const char *utf8)
{
	if (pls_is_empty(utf8)) {
		return {};
	}
#if defined(Q_OS_WIN)
	if (int unicode_length = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0); unicode_length > 0) {
		std::wstring unicode(unicode_length, '\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, unicode.data(), unicode_length);
		unicode.pop_back();
		return unicode;
	}
	return {};
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_utf8_to_unicode(utf8);
#endif
}
LIBUTILSAPI_API std::string pls_unicode_to_utf8(const wchar_t *unicode)
{
	if (pls_is_empty(unicode)) {
		return {};
	}
#if defined(Q_OS_WIN)
	if (int utf8_length = WideCharToMultiByte(CP_UTF8, 0, unicode, -1, nullptr, 0, nullptr, nullptr); utf8_length > 0) {
		std::string utf8(utf8_length, '\0');
		WideCharToMultiByte(CP_UTF8, 0, unicode, -1, utf8.data(), utf8_length, nullptr, nullptr);
		utf8.pop_back();
		return utf8;
	}
	return {};
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_unicode_to_utf8(unicode);
#endif
}

LIBUTILSAPI_API bool pls_get_app_exiting()
{
	return LocalGlobalVars::g_app_exiting == 1;
}

LIBUTILSAPI_API void pls_set_app_exiting(bool value)
{
	LocalGlobalVars::g_app_exiting = value ? 1 : 0;
}

LIBUTILSAPI_API void pls_env_add_path(const QByteArray &path)
{
	if (auto paths = qgetenv("PATH").split(';'); !paths.contains(path)) {
		paths.prepend(path);
		qputenv("PATH", paths.join(';'));
	}
}
LIBUTILSAPI_API void pls_env_add_paths(const QByteArrayList &paths)
{
	for (const auto &path : paths) {
		pls_env_add_path(path);
	}
}
LIBUTILSAPI_API void pls_env_add_path(const QString &path)
{
	pls_env_add_path(path.toLocal8Bit());
}
LIBUTILSAPI_API void pls_env_add_paths(const QStringList &paths)
{
	for (const auto &path : paths) {
		pls_env_add_path(path);
	}
}

#if defined(Q_OS_WIN)
LIBUTILSAPI_API bool pls_is_process_running(const wchar_t *executableName, int &pid)
{
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (!Process32First(snapshot, &entry)) {
		CloseHandle(snapshot);
		return false;
	}

	do {
		if (!_wcsicmp(entry.szExeFile, executableName)) {
			pid = static_cast<int>(entry.th32ProcessID);
			CloseHandle(snapshot);
			return true;
		}
	} while (Process32Next(snapshot, &entry));

	CloseHandle(snapshot);
	return false;
}

const wchar_t *CAM_EXEC_NAMES[] = {
	L"PRISMLens.exe",
	L"PRISMSetupLauncher.exe",
#ifdef _DEBUG
	L"electron.exe",
#endif // _DEBUG
};

static void GetProcessTree(DWORD parentPid, std::set<DWORD> &processIds)
{
	processIds.insert(parentPid);

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		return;
	}

	std::vector<std::pair<DWORD, DWORD>> allProcesses;        // <process_id, parent_process_id>
	std::vector<std::pair<DWORD, std::wstring>> processNames; // <process_id, process_name>
	PROCESSENTRY32W pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32W);

	if (Process32FirstW(hSnapshot, &pe32)) {
		do {
			allProcesses.push_back({pe32.th32ProcessID, pe32.th32ParentProcessID});
			processNames.push_back({pe32.th32ProcessID, pe32.szExeFile});
		} while (Process32NextW(hSnapshot, &pe32));
	}
	CloseHandle(hSnapshot);

	for (const auto &processName : processNames) {
		DWORD processId = processName.first;
		const std::wstring &exeName = processName.second;

		for (size_t i = 0; i < sizeof(CAM_EXEC_NAMES) / sizeof(CAM_EXEC_NAMES[0]); i++) {
			if (_wcsicmp(exeName.c_str(), CAM_EXEC_NAMES[i]) == 0) {
				processIds.insert(processId);
				break;
			}
		}
	}

	bool foundNewProcesses = true;
	int iterations = 0;
	const int MAX_ITERATIONS = 10;
	while (foundNewProcesses && iterations < MAX_ITERATIONS) {
		foundNewProcesses = false;
		iterations++;

		for (const auto &process : allProcesses) {
			DWORD processId = process.first;
			DWORD parentProcessId = process.second;

			if (processIds.find(parentProcessId) != processIds.end() && processIds.find(processId) == processIds.end()) {
				processIds.insert(processId);
				foundNewProcesses = true;
			}
		}
	}
}

struct WindowEnumData {
	std::set<DWORD> processIds;
	bool hasVisibleWindow;
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	WindowEnumData *data = reinterpret_cast<WindowEnumData *>(lParam);
	DWORD windowProcessId;
	GetWindowThreadProcessId(hwnd, &windowProcessId);

	if (data->processIds.find(windowProcessId) != data->processIds.end()) {
		WINDOWPLACEMENT wp;
		wp.length = sizeof(WINDOWPLACEMENT);
		if (GetWindowPlacement(hwnd, &wp)) {
			//PLS_INFO(UTILS_API_MODULE, "Window handle: %p, ShowCmd: %d, ProcessId: %lu", hwnd, wp.showCmd, windowProcessId);
			if (wp.showCmd == SW_MINIMIZE || wp.showCmd == SW_SHOWMINIMIZED) {
				// Restore minimized window using PostMessage approach for high privilege compatibility

				// Method 1: Try standard ShowWindow first
				if (!ShowWindow(hwnd, SW_RESTORE)) {
					// Failed - use PostMessage approach for high privilege windows

					// Method 2: PostMessage to restore window (async, lower privilege requirement)
					if (!PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0)) {
						PLS_WARN(UTILS_API_MODULE, "PostMessage WM_SYSCOMMAND SC_RESTORE failed.");
					}
				}

				if (!SetForegroundWindow(hwnd)) {
					PLS_WARN(UTILS_API_MODULE, "SetForegroundWindow failed.");
				}

				data->hasVisibleWindow = true;
				return FALSE;
			}
		}

		if (IsWindowVisible(hwnd)) {

			if (!SetForegroundWindow(hwnd)) {
				PLS_WARN(UTILS_API_MODULE, "SetForegroundWindow failed.");
			}

			data->hasVisibleWindow = true;
			return FALSE;
		}
	}
	return TRUE;
}

LIBUTILSAPI_API LENS_WINDOWS_STATUS pls_is_cam_window_visible(uint32_t processId)
{
	WindowEnumData enumData;
	GetProcessTree(processId, enumData.processIds);
	enumData.hasVisibleWindow = false;
	EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&enumData));

	// Return appropriate LENS_WINDOWS_STATUS based on the result
	if (enumData.hasVisibleWindow) {
		return SHOWN; // Window has been displayed/is visible
	}
	// Check if any process in processIds has executable name matching CAM_EXEC_NAMES
	bool foundCamProcess = false;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32W pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32W);

		if (Process32FirstW(hSnapshot, &pe32)) {
			do {
				// Check if this process is in our processIds set
				if (enumData.processIds.find(pe32.th32ProcessID) != enumData.processIds.end()) {
					// Check if the executable name matches any CAM_EXEC_NAMES
					for (size_t i = 0; i < sizeof(CAM_EXEC_NAMES) / sizeof(CAM_EXEC_NAMES[0]); i++) {
						if (_wcsicmp(pe32.szExeFile, CAM_EXEC_NAMES[i]) == 0) {
							foundCamProcess = true;
							break;
						}
					}
					if (foundCamProcess) {
						break;
					}
				}
			} while (Process32NextW(hSnapshot, &pe32));
		}
		CloseHandle(hSnapshot);
	}

	if (foundCamProcess) {
		return NO_SHOW; // Process exists but window is not visible
	} else {
		return PROCESS_NOT_FOUND; // No CAM process found in the process tree
	}
}

#endif

#if defined(Q_OS_WIN)
#define PRISM_EVENT_REQUEST_EXIT L"prism_request_exit"
#define PRISM_EVENT_REQUEST_SHOW_SETTINGS L"prism_request_show_settings"

LIBUTILSAPI_API bool pls_request_lens_exit()
{
	HANDLE hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, PRISM_EVENT_REQUEST_EXIT);
	if (hEvent == NULL) {
		DWORD errorCode = GetLastError();
		PLS_WARN(UTILS_API_MODULE, "Failed to open event for lens exit request: error code %d", errorCode);
		pls_set_last_error(errorCode);
		return false;
	}

	if (!SetEvent(hEvent)) {
		DWORD errorCode = GetLastError();
		PLS_WARN(UTILS_API_MODULE, "Failed to set event for lens exit request: error code %d", errorCode);
		CloseHandle(hEvent);
		pls_set_last_error(errorCode);
		return false;
	}

	CloseHandle(hEvent);
	PLS_INFO(UTILS_API_MODULE, "Successfully sent lens exit request");
	return true;
}

LIBUTILSAPI_API bool pls_request_lens_show_settings()
{
	HANDLE hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, PRISM_EVENT_REQUEST_SHOW_SETTINGS);
	if (hEvent == NULL) {
		DWORD errorCode = GetLastError();
		PLS_WARN(UTILS_API_MODULE, "Failed to open event for lens show settings request: error code %d", errorCode);
		pls_set_last_error(errorCode);
		return false;
	}

	if (!SetEvent(hEvent)) {
		DWORD errorCode = GetLastError();
		PLS_WARN(UTILS_API_MODULE, "Failed to set event for lens show settings request: error code %d", errorCode);
		CloseHandle(hEvent);
		pls_set_last_error(errorCode);
		return false;
	}

	CloseHandle(hEvent);
	PLS_INFO(UTILS_API_MODULE, "Successfully sent lens show settings request");
	return true;
}

#endif

#if defined(Q_OS_MACOS)
#define PRISM_LENS_REQUEST_EXIT_NOTIFY "prism_request_exit"
#define PRISM_LENS_REQUEST_SHOW_SETTINGS_NOTIFY "prism_request_show_settings"

LIBUTILSAPI_API bool pls_is_process_running(const char *executableName, int &pid)
{
	return pls_libutil_api_mac::pls_is_process_running(executableName, pid);
}

LIBUTILSAPI_API bool pls_request_lens_exit()
{
	uint32_t status = notify_post(PRISM_LENS_REQUEST_EXIT_NOTIFY);

	if (status != NOTIFY_STATUS_OK) {
		PLS_WARN(UTILS_API_MODULE, "Failed to post notification for lens exit request: status %u", status);
		pls_set_last_error(status);
		return false;
	}

	PLS_INFO(UTILS_API_MODULE, "%s", "Successfully sent lens exit request via notification");
	return true;
}

LIBUTILSAPI_API bool pls_request_lens_show_settings()
{
	uint32_t status = notify_post(PRISM_LENS_REQUEST_SHOW_SETTINGS_NOTIFY);

	if (status != NOTIFY_STATUS_OK) {
		PLS_WARN(UTILS_API_MODULE, "Failed to post notification for lens show settings request: status %u", status);
		pls_set_last_error(status);
		return false;
	}

	PLS_INFO(UTILS_API_MODULE, "%s", "Successfully sent lens show settings request via notification");
	return true;
}

#endif

LIBUTILSAPI_API qulonglong pls_get_qobject_id(const QObject *object)
{
	if (pls_object_is_valid(object)) {
		return object_pool()->get_qobject_id(object);
	}
	return 0;
}
LIBUTILSAPI_API bool pls_check_qobject_id(const QObject *object, qulonglong object_id)
{
	if ((object_id > 0) && pls_object_is_valid(object)) {
		return object_pool()->get_qobject_id(object) == object_id;
	}
	return false;
}
LIBUTILSAPI_API bool pls_check_qobject_id(const QObject *object, qulonglong object_id, const std::function<bool(const QObject *object)> &is_valid)
{
	if ((object_id > 0) && pls_object_is_valid(object, is_valid)) {
		return object_pool()->get_qobject_id(object) == object_id;
	}
	return false;
}

LIBUTILSAPI_API bool pls_has_object_getter(const QObject *object, const QByteArray &name)
{
	if (auto getter = object_pool()->get_getter(object, name); getter)
		return true;
	return false;
}
LIBUTILSAPI_API bool pls_add_object_getter(const QObject *object, const QByteArray &name, const pls_getter_t &getter)
{
	return object_pool()->add_getter(object, name, getter);
}
LIBUTILSAPI_API void pls_remove_object_getter(const QObject *object, const QByteArray &name)
{
	object_pool()->remove_getter(object, name);
}
LIBUTILSAPI_API QVariant pls_call_object_getter(const QObject *object, const QByteArray &name)
{
	if (auto getter = object_pool()->get_getter(object, name); getter)
		return getter();
	return QVariant();
}
LIBUTILSAPI_API bool pls_call_object_getter(QVariant &result, const QObject *object, const QByteArray &name)
{
	if (auto getter = object_pool()->get_getter(object, name); getter) {
		result = getter();
		return true;
	}
	return false;
}
LIBUTILSAPI_API bool pls_has_object_setter(const QObject *object, const QByteArray &name)
{
	if (auto setter = object_pool()->get_setter(object, name); setter)
		return true;
	return false;
}
LIBUTILSAPI_API
bool pls_add_object_setter(const QObject *object, const QByteArray &name, const pls_setter_t &setter)
{
	return object_pool()->add_setter(object, name, setter);
}
LIBUTILSAPI_API void pls_remove_object_setter(const QObject *object, const QByteArray &name)
{
	object_pool()->remove_setter(object, name);
}
LIBUTILSAPI_API void pls_call_object_setter(const QObject *object, const QByteArray &name, const QVariant &value)
{
	if (auto setter = object_pool()->get_setter(object, name); setter)
		setter(value);
}

LIBUTILSAPI_API int pls_get_platform_window_height_by_windows_height(int windowsHeight)
{
#if defined(Q_OS_WIN)
	return windowsHeight;
#elif defined(Q_OS_MACOS)
	return windowsHeight - 40;
#else
	return windowsHeight;
#endif
}

bool pls_dlopen_i(pls_dl_t &dl, const QString &dl_path)
{
#if defined(Q_OS_WIN)
	auto path = dl_path.toStdWString();
	if (auto handle = GetModuleHandleW(path.c_str()); handle) {
		dl.handle = handle;
		dl.close = false;
		return true;
	} else if (handle = LoadLibraryW(path.c_str()); handle) {
		dl.handle = handle;
		dl.close = true;
		return true;
	}
	pls_set_last_error(GetLastError());
	PLS_WARN(UTILS_API_MODULE, "load library failed, path: %s, error code: %u", pls_get_path_file_name(dl_path).toUtf8().constData(), GetLastError());
#elif defined(Q_OS_MACOS)
	auto path = dl_path.toStdString();
	if (auto handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_FIRST); handle) {
		dl.handle = handle;
		dl.close = true;
		return true;
	}
	PLS_WARN(UTILS_API_MODULE, "load library failed, path: %s, error code: %s", pls_get_path_file_name(dl_path).toUtf8().constData(), dlerror());
#endif
	return false;
}
LIBUTILSAPI_API bool pls_dlopen(pls_dl_t &dl, const QString &dl_path)
{
	if (dl_path.isEmpty()) {
		return false;
	} else if (pls_dlopen_i(dl, dl_path)) {
		return true;
	}
#if defined(Q_OS_WIN)
	else if (dl_path.contains(QLatin1String(".dll"), Qt::CaseInsensitive)) {
		return false;
	} else if (pls_dlopen_i(dl, dl_path + ".dll")) {
		return true;
	}
#elif defined(Q_OS_MACOS)
	else if (dl_path.contains(QLatin1String(".framework"), Qt::CaseInsensitive) //
		 || dl_path.contains(QLatin1String(".plugin"), Qt::CaseInsensitive) //
		 || dl_path.contains(QLatin1String(".dylib"), Qt::CaseInsensitive)  //
		 || dl_path.contains(QLatin1String(".so"), Qt::CaseInsensitive)) {
		return false;
	} else if (pls_dlopen_i(dl, dl_path + ".framework")) {
		return true;
	} else if (pls_dlopen_i(dl, dl_path + ".plugin")) {
		return true;
	} else if (pls_dlopen_i(dl, dl_path + ".dylib")) {
		return true;
	} else if (pls_dlopen_i(dl, dl_path + ".so")) {
		return true;
	}
#endif
	return false;
}
LIBUTILSAPI_API pls_fn_ptr_t pls_dlsym(pls_dl_t &dl, const char *func)
{
#if defined(Q_OS_WIN)
	return dl.handle ? (pls_fn_ptr_t)GetProcAddress((HMODULE)dl.handle, func) : nullptr;
#else
	return dl.handle ? (pls_fn_ptr_t)dlsym(dl.handle, func) : nullptr;
#endif
}
LIBUTILSAPI_API void pls_dlclose(pls_dl_t &dl)
{
	if (dl.handle && dl.close) {
#if defined(Q_OS_WIN)
		pls_delete((HMODULE &)dl.handle, FreeLibrary, nullptr);
#else
		pls_delete(dl.handle, dlclose, nullptr);
#endif
	}
}

LIBUTILSAPI_API std::string pls_get_cpu_name()
{
	std::string cpu_name = "";
#if defined(Q_OS_WIN)
	HKEY key;
	std::array<wchar_t, 1024> data{0};
	LSTATUS status;

	status = RegOpenKeyW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", &key);
	if (status != ERROR_SUCCESS)
		return "";

	auto size = (DWORD)data.size();
	status = RegQueryValueExW(key, L"ProcessorNameString", nullptr, nullptr, (LPBYTE)data.data(), &size);
	if (status == ERROR_SUCCESS) {
		cpu_name = pls_unicode_to_utf8(data.data());
	}

	RegCloseKey(key);
#else
	return pls_libutil_api_mac::pls_get_cpu_name();
#endif
	return cpu_name;
}

LIBUTILSAPI_API pls_language_t pls_get_os_language()
{
#if defined(Q_OS_WIN)
	auto lid = GetUserDefaultUILanguage();
	return pls_get_os_language_from_lid(lid);
#elif defined(Q_OS_MACOS)
	return pls_language_t::NotImplement;
#endif
}
// return zh-CN en-US ko-KR ...
LIBUTILSAPI_API QString pls_get_os_language(pls_language_t language)
{
	switch (language) {
	case pls_language_t::SimpleChinese:
		return QStringLiteral("zh-CN");
	case pls_language_t::TraditionalChinese:
		return QStringLiteral("zh-TW");
	case pls_language_t::English:
		return QStringLiteral("en-US");
	case pls_language_t::Korean:
		return QStringLiteral("ko-KR");
	case pls_language_t::Vietnamese:
		return QStringLiteral("vi-VN");
	case pls_language_t::Spanish:
		return QStringLiteral("es-ES");
	case pls_language_t::Indonesian:
		return QStringLiteral("id-ID");
	case pls_language_t::Japanese:
		return QStringLiteral("ja-JP");
	case pls_language_t::Portuguese:
		return QStringLiteral("pt-BR");
	case pls_language_t::Other:
	default:
		return QStringLiteral("other");
	}
}

#if defined(Q_OS_WIN)
LIBUTILSAPI_API pls_language_t pls_get_os_language_from_lid(uint16_t lid)
{
	auto plid = PRIMARYLANGID(lid);
	auto slid = SUBLANGID(lid);

	if (plid == LANG_CHINESE) {
		if (slid == SUBLANG_CHINESE_SIMPLIFIED)
			return pls_language_t::SimpleChinese;
		else if (slid == SUBLANG_CHINESE_TRADITIONAL)
			return pls_language_t::TraditionalChinese;
	} else if (plid == LANG_ENGLISH) {
		return pls_language_t::English;
	} else if (plid == LANG_KOREAN) {
		return pls_language_t::Korean;
	} else if (plid == LANG_VIETNAMESE) {
		return pls_language_t::Vietnamese;
	} else if (plid == LANG_SPANISH) {
		return pls_language_t::Spanish;
	} else if (plid == LANG_INDONESIAN) {
		return pls_language_t::Indonesian;
	} else if (plid == LANG_JAPANESE) {
		return pls_language_t::Japanese;
	} else if (plid == LANG_PORTUGUESE) {
		return pls_language_t::Portuguese;
	}
	return pls_language_t::Other;
}
#elif defined(Q_OS_MACOS)
#endif

LIBUTILSAPI_API bool pls_is_mouse_pressed(Qt::MouseButton button)
{
#if defined(Q_OS_WIN)
	switch (button) {
	case Qt::LeftButton:
		return GetAsyncKeyState(VK_LBUTTON) < 0;
	case Qt::RightButton:
		return GetAsyncKeyState(VK_RBUTTON) < 0;
	case Qt::MiddleButton:
		return GetAsyncKeyState(VK_MBUTTON) < 0;
	default:
		return false;
	}
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_is_mouse_pressed_by_mac(button);
#endif
}

#if defined(Q_OS_MACOS)
LIBUTILSAPI_API void pls_set_current_lens(int index)
{
	pls_libutil_api_mac::pls_set_current_lens(index);
}

LIBUTILSAPI_API void pls_open_app(const QString &program, const QStringList &arguments, const std::function<void(const bool created, const bool launched)> &callback)
{
	auto data = pls_new<pls_open_app_cb_t>();
	data->callback = callback;
	pls_libutil_api_mac::pls_create_and_observe_process(program, arguments, data, [](void *user_data, bool created, bool launched) {
		if (user_data) {
			pls_open_app_cb_t *cb = static_cast<pls_open_app_cb_t *>(user_data);
			cb->callback(created, launched);
			pls_delete(cb);
		}
	});
}

#endif

LIBUTILSAPI_API bool pls_is_os_sys_macos()
{
#if defined(Q_OS_MACOS)
	return true;
#else
	return false;
#endif
}
LIBUTILSAPI_API bool pls_set_temp_sharememory(const QString &key, const QString &val)
{
	QSharedMemory objSharedMemory(key);
	if (!objSharedMemory.isAttached()) {
		if (!objSharedMemory.attach()) {
			PLS_INFO("sharememory", "Failed to attach shared memory to the process!!!!, %s", objSharedMemory.errorString().toUtf8().constData());
			return false;
		}
	}
	QBuffer buffer;
	buffer.open(QBuffer::WriteOnly);
	QDataStream out(&buffer);
	QString fileName = val;
	out << fileName;
	objSharedMemory.lock();
	auto to = objSharedMemory.data();
	const char *from = buffer.data().data();
	memcpy(to, from, qMin(static_cast<qint64>(objSharedMemory.size()), buffer.size()));
	objSharedMemory.unlock();
	buffer.close();

	objSharedMemory.detach();
	return true;
}

LIBUTILSAPI_API void pls_generate_prism_session_subsession(QString &session, QString &subsession)
{
	for (const auto &arg : pls_cmdline_args()) {
		if (arg.startsWith(shared_values::k_launcher_command_log_prism_session)) {
			session = arg.mid(shared_values::k_launcher_command_log_prism_session.length());
		} else if (arg.startsWith(shared_values::k_launcher_command_log_sub_prism_session)) {
			subsession = arg.mid(shared_values::k_launcher_command_log_sub_prism_session.length());
		}
	}

	if (session.isEmpty())
		session = QUuid::createUuid().toString();
	if (subsession.isEmpty())
		subsession = QUuid::createUuid().toString();
}
LIBUTILSAPI_API void pls_generate_prism_session_subsession(std::string &session, std::string &subsession)
{
	QString qsession, qsubsession;
	pls_generate_prism_session_subsession(qsession, qsubsession);
	session = qsession.toStdString();
	subsession = qsubsession.toStdString();
}

LIBUTILSAPI_API uint64_t pls_get_prism_version()
{
	return LocalGlobalVars::g_prism_version;
}
LIBUTILSAPI_API QString pls_get_prism_version_string()
{
	return QString("%1.%2.%3.%4").arg(pls_get_prism_version_major()).arg(pls_get_prism_version_minor()).arg(pls_get_prism_version_patch()).arg(pls_get_prism_version_build());
}
LIBUTILSAPI_API void pls_set_prism_version(uint64_t version)
{
	LocalGlobalVars::g_prism_version = version;
}
LIBUTILSAPI_API void pls_set_prism_version(uint16_t major, uint16_t minor, uint16_t patch, uint16_t build)
{
	uint64_t version = major;
	version = (version << 16) | minor;
	version = (version << 16) | patch;
	version = (version << 16) | build;
	pls_set_prism_version(version);
}
LIBUTILSAPI_API uint16_t pls_get_prism_version_major()
{
	return (uint16_t)((LocalGlobalVars::g_prism_version >> 48) & 0xffff);
}
LIBUTILSAPI_API uint16_t pls_get_prism_version_minor()
{
	return (uint16_t)((LocalGlobalVars::g_prism_version >> 32) & 0xffff);
}
LIBUTILSAPI_API uint16_t pls_get_prism_version_patch()
{
	return (uint16_t)((LocalGlobalVars::g_prism_version >> 16) & 0xffff);
}
LIBUTILSAPI_API uint16_t pls_get_prism_version_build()
{
	return (uint16_t)(LocalGlobalVars::g_prism_version & 0xffff);
}

class pls_thread_t {
public:
	pls_thread_t(const pls_thread_entry_t &entry) : m_entry(entry), m_sem(0) {}
	~pls_thread_t() {}

	void start(bool delay_start)
	{
		if (!delay_start && !m_running) {
			m_running = true;
			m_thread = std::thread(m_entry, this);
		}
	}

public:
	std::thread m_thread;
	pls_thread_entry_t m_entry;
	pls_locked_queue_t m_queue;
	QSemaphore m_sem;
	bool m_running = false;
};
LIBUTILSAPI_API pls_thread_t *pls_thread_create(const pls_thread_entry_t &entry, bool delay_start)
{
	pls_thread_t *thread = pls_new<pls_thread_t>(entry);
	thread->start(delay_start);
	return thread;
}
LIBUTILSAPI_API void pls_thread_destroy(pls_thread_t *thread)
{
	pls_thread_quit(thread);
	thread->m_sem.release(1);
	thread->m_thread.join();
	pls_delete(thread);
}
LIBUTILSAPI_API bool pls_thread_running(pls_thread_t *thread)
{
	return thread->m_running;
}
LIBUTILSAPI_API void pls_thread_start(pls_thread_t *thread)
{
	thread->start(false);
}
LIBUTILSAPI_API void pls_thread_quit(pls_thread_t *thread)
{
	thread->m_running = false;
}
LIBUTILSAPI_API bool pls_thread_joinable(pls_thread_t *thread)
{
	return thread->m_thread.joinable();
}
LIBUTILSAPI_API void pls_thread_join(pls_thread_t *thread)
{
	thread->m_thread.join();
}
LIBUTILSAPI_API bool pls_thread_queue_empty(const pls_thread_t *thread)
{
	return pls_locked_queue_empty(&thread->m_queue);
}
LIBUTILSAPI_API pls_queue_item_t *pls_thread_queue_top(pls_thread_t *thread)
{
	return pls_locked_queue_top(&thread->m_queue);
}
LIBUTILSAPI_API void pls_thread_queue_push(pls_thread_t *thread, pls_queue_item_t *new_one)
{
	pls_locked_queue_push(&thread->m_queue, new_one);
	thread->m_sem.release(1);
}
LIBUTILSAPI_API pls_queue_item_t *pls_thread_queue_pop(pls_thread_t *thread)
{
	thread->m_sem.acquire();
	return pls_locked_queue_pop(&thread->m_queue);
}
LIBUTILSAPI_API pls_queue_item_t *pls_thread_queue_try_pop(pls_thread_t *thread)
{
	return thread->m_sem.try_acquire() ? pls_locked_queue_pop(&thread->m_queue) : nullptr;
}
LIBUTILSAPI_API bool pls_open_url(const QString &url)
{
#if defined(Q_OS_WIN)
	bool isUrl = url.startsWith("http");

	SHELLEXECUTEINFO sei;
	ZeroMemory(&sei, sizeof(sei));
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.nShow = SW_SHOWNORMAL;
	sei.lpVerb = L"open";
	sei.lpParameters = reinterpret_cast<const wchar_t *>(url.utf16());

	std::wstring browserPath{};
	if (!isUrl) {
		WCHAR explorerPath[255] = {0};
		GetWindowsDirectory(explorerPath, 255);
		PathAppend(explorerPath, TEXT("explorer.exe"));
		browserPath = explorerPath;
	} else {
		QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice", QSettings::NativeFormat);
		auto proId = settings.value("Progid").toString();
		QSettings settings_2(QString("HKEY_CLASSES_ROOT\\%1\\shell\\open\\command").arg(proId), QSettings::NativeFormat);
		auto path = settings_2.value("Default").toString();
		QRegularExpression reg("\"([^\"]*)\"");
		path = reg.match(path).captured(0).remove('"');
		browserPath = path.toStdWString();
	}
	sei.lpFile = browserPath.c_str();
	bool ret = false;
	return ShellExecuteEx(&sei);
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_open_url_mac(url);
#endif
}

LIBUTILSAPI_API bool pls_lens_needs_reboot()
{
#if defined(Q_OS_WIN)
	return false;
#elif defined(Q_OS_MACOS)
	return pls_libutil_api_mac::pls_lens_needs_reboot();
#endif
}

QString pls_get_local_hostname()
{
	try {
		return QHostInfo::localHostName();
	} catch (const std::exception &e) {
		PLS_WARN(UTILS_API_MODULE, "Failed to get local hostname: %s", e.what());
		return QString();
	} catch (...) {
		PLS_WARN(UTILS_API_MODULE, "Unknown error when getting local hostname %s", "");
		return QString();
	}
}

LIBUTILSAPI_API bool pls_check_local_host()
{
	QString hostname = pls_get_local_hostname();
	if (hostname.isEmpty()) {
		return false;
	}
	std::string ipAddress;
	QHostInfo info = QHostInfo::fromName(hostname);
	foreach(QHostAddress address, info.addresses())
	{
		if (address.protocol() == QAbstractSocket::IPv4Protocol) {
			ipAddress = address.toString().toStdString();
			std::regex regExp("10.(34|25).*");
			auto matched = std::regex_match(ipAddress, regExp);
			if (matched) {
				return true;
			}
		}
	}
	return false;
}

struct Condition {
	enum class Op {
		Unknown,          // unknown
		And,              // &&
		Or,               // ||
		Equal,            // =
		LessThan,         // <
		LessThanEqual,    // <=
		GreaterThan,      // >
		GreaterThanEqual, // >=
		LeftBracket,      // (
		RightBracket      // )
	} op;
	enum class OS { //
		All,
		Windows,
		MacOS
	};
	bool value;

	static bool isLogic(Op op) { return op == Op::And || op == Op::Or; }
	static bool isCompare(Op op) { return op == Op::Equal || op == Op::LessThan || op == Op::LessThanEqual || op == Op::GreaterThan || op == Op::GreaterThanEqual; }
	static bool isBracket(Op op) { return op == Op::LeftBracket || op == Op::RightBracket; }

	static void skipSpace(const QByteArray &expression, int &pos, int length)
	{
		while (pos < length) {
			if (expression[pos] == ' ')
				++pos;
			else
				break;
		}
	}
	static Op getOp(const QByteArray &expression, int &pos, int length)
	{
		skipSpace(expression, pos, length);

		if (pos >= length)
			return Op::Unknown;

		if ((length - pos) > 1) {
			if (expression[pos] == '&' && expression[pos + 1] == '&') {
				pos += 2;
				return Op::And;
			} else if (expression[pos] == '|' && expression[pos + 1] == '|') {
				pos += 2;
				return Op::Or;
			} else if (expression[pos] == '<' && expression[pos + 1] == '=') {
				pos += 2;
				return Op::LessThanEqual;
			} else if (expression[pos] == '>' && expression[pos + 1] == '=') {
				pos += 2;
				return Op::GreaterThanEqual;
			}
		}

		if (expression[pos] == '<') {
			pos += 1;
			return Op::LessThan;
		} else if (expression[pos] == '>') {
			pos += 1;
			return Op::GreaterThan;
		} else if (expression[pos] == '=') {
			pos += 1;
			return Op::Equal;
		} else if (expression[pos] == '(') {
			pos += 1;
			return Op::LeftBracket;
		} else if (expression[pos] == ')') {
			pos += 1;
			return Op::RightBracket;
		} else {
			return Op::Unknown;
		}
	}
	static bool getVer(QPair<Condition::OS, QVersionNumber> &ver, const QByteArray &expression, int &pos, int length)
	{
		Condition::skipSpace(expression, pos, length);

		if (qstrnicmp("win", expression.data() + pos, 3) == 0) {
			ver.first = Condition::OS::Windows;
			pos += 3;
		} else if (qstrnicmp("mac", expression.data() + pos, 3) == 0) {
			ver.first = Condition::OS::MacOS;
			pos += 3;
		} else {
			ver.first = Condition::OS::All;
		}

		int start = pos;
		while (pos < length) {
			auto ch = expression[pos];
			if ((ch >= '0' && ch <= '9') || (ch == '.'))
				++pos;
			else
				break;
		}

		if (pos <= start)
			return false;

		auto version = expression.mid(start, pos - start);
		ver.second = QVersionNumber::fromString(QString::fromUtf8(version));
		return true;
	}

	static Op topOp(const QStack<Condition> &conditions)
	{
		if (!conditions.isEmpty())
			return conditions.top().op;
		return Op::Unknown;
	}
	static bool calc(Op op, const QVersionNumber &ver, const QVersionNumber &version)
	{
		switch (op) {
		case Op::Equal:
			return version == ver;
		case Op::LessThan:
			return version < ver;
		case Op::LessThanEqual:
			return version <= ver;
		case Op::GreaterThan:
			return version > ver;
		case Op::GreaterThanEqual:
			return version >= ver;
		default:
			return false;
		}
	}
	static bool calc(QStack<Condition> &conditions)
	{
		if (conditions.size() < 2)
			return false;

		auto v1 = conditions.top();
		conditions.pop();
		auto op = conditions.top();
		conditions.pop();
		switch (op.op) {
		case Op::Or:
			if (conditions.isEmpty()) {
				return false;
			} else if (auto &v2 = conditions.top(); isCompare(v2.op)) {
				v2.value = v2.value || v1.value;
				return calc(conditions);
			} else {
				return false;
			}
		case Op::LeftBracket:
			conditions.push(v1);
			return true;
		default:
			return false;
		}
	}
	static bool checkOS(Condition::OS os)
	{
		return (os == Condition::OS::All
#ifdef Q_OS_WIN
			|| os == Condition::OS::Windows
#else // mac os
			|| os == Condition::OS::MacOS
#endif
		);
	}
	static bool push(QStack<Condition> &conditions, const Condition &condition)
	{
		switch (condition.op) {
		case Condition::Op::And:
		case Condition::Op::Or:
			if (!isCompare(topOp(conditions)))
				return false;
			conditions.push(condition);
			return true;
		case Condition::Op::Equal:
		case Condition::Op::LessThan:
		case Condition::Op::LessThanEqual:
		case Condition::Op::GreaterThan:
		case Condition::Op::GreaterThanEqual:
			if (auto topOp = Condition::topOp(conditions); topOp == Op::Unknown || topOp == Op::Or || topOp == Op::LeftBracket) {
				conditions.push(condition);
				return true;
			} else if (topOp == Op::And) {
				conditions.pop();
				auto &top = conditions.top();
				top.value = top.value && condition.value;
				return true;
			} else {
				return false;
			}
		case Condition::Op::LeftBracket:
			if (!(conditions.isEmpty() || isLogic(topOp(conditions))))
				return false;
			conditions.push(condition);
			return true;
		case Condition::Op::RightBracket: {
			if (!isCompare(topOp(conditions)))
				return false;
			else if (!Condition::calc(conditions))
				return false;
			return true;
		}
		case Condition::Op::Unknown:
		default:
			return false;
		}
	}
};

std::optional<bool> pls_check_version(const QByteArray &expression, const QVersionNumber &version)
{
	int pos = 0;
	int length = expression.length();

	QStack<Condition> conditions;
	while (pos < length) {
		auto op = Condition::getOp(expression, pos, length);
		switch (op) {
		case Condition::Op::And:
		case Condition::Op::Or:
		case Condition::Op::LeftBracket:
		case Condition::Op::RightBracket:
			if (!Condition::push(conditions, {op, true}))
				return std::nullopt;
			break;
		case Condition::Op::Equal:
		case Condition::Op::LessThan:
		case Condition::Op::LessThanEqual:
		case Condition::Op::GreaterThan:
		case Condition::Op::GreaterThanEqual:
			if (QPair<Condition::OS, QVersionNumber> ver; !Condition::getVer(ver, expression, pos, length))
				return std::nullopt;
			else if (!Condition::push(conditions, {op, Condition::checkOS(ver.first) && Condition::calc(op, ver.second, version)}))
				return std::nullopt;
			break;
		case Condition::Op::Unknown:
		default:
			return std::nullopt;
		}
	}

	if (conditions.size() > 1)
		Condition::calc(conditions);

	if (conditions.size() != 1)
		return std::nullopt;
	else if (auto condition = conditions.top(); Condition::isCompare(condition.op))
		return condition.value;
	return std::nullopt;
}

LIBUTILSAPI_API bool pls_is_equal(const QVariant &v1, const QVariant &v2, Qt::CaseSensitivity cs)
{
	if (v1.isNull() && v2.isNull())
		return true;
	else if (v1.typeId() != v2.typeId())
		return false;
	switch (v1.typeId()) {
	case QMetaType::Bool:
		return v1.toBool() == v2.toBool();
	case QMetaType::Short:
	case QMetaType::Int:
	case QMetaType::Long:
	case QMetaType::LongLong:
		return v1.toLongLong() == v2.toLongLong();
	case QMetaType::UShort:
	case QMetaType::UInt:
	case QMetaType::ULong:
	case QMetaType::ULongLong:
		return v1.toULongLong() == v2.toULongLong();
	case QMetaType::Float:
	case QMetaType::Double:
		return pls_is_equal(v1.toDouble(), v2.toDouble());
	case QMetaType::QString:
		return pls_is_equal(v1.toString(), v2.toString(), cs);
	case QMetaType::QStringList:
		return pls_is_equal(v1.toStringList(), v2.toStringList(), cs);
	case QMetaType::QVariantList:
		return pls_is_equal(v1.toList(), v2.toList(), cs);
	case QMetaType::QVariantHash:
		return pls_is_equal(v1.toHash(), v2.toHash(), cs);
	case QMetaType::QVariantMap:
		return pls_is_equal(v1.toMap(), v2.toMap(), cs);
	}
	return true;
}

LIBUTILSAPI_API int pls_compare_version(const QString &v1, const QString &v2)
{
	QVersionNumber version1 = QVersionNumber::fromString(v1);
	QVersionNumber version2 = QVersionNumber::fromString(v2);

	if (version1 > version2) {
		return 1;
	} else if (version1 < version2) {
		return -1;
	} else {
		return 0;
	}
}

LIBUTILSAPI_API void pls_update_exit_time()
{
	QString stateFile = pls_get_app_user_data_file_path_pn(QStringLiteral("streaming_state.json"), false);
	QJsonObject stateJson;

	if (!pls_decrypt_json(stateJson, stateFile) || stateJson.isEmpty()) {
		return;
	}
	qint64 exitTime = QDateTime::currentSecsSinceEpoch();
	stateJson[QStringLiteral("exitTime")] = exitTime;
	pls_encrypt_json(stateFile, stateJson);
}
