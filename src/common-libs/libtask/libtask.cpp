#include "libtask.h"

#include <chrono>

#include <qcoreapplication.h>
#include <qjsondocument.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qfile.h>
#include <qsettings.h>
#include <qtranslator.h>
#include <qtimer.h>

#include <libutils-api.h>
#include <liblog.h>
#include <libipc.h>
#include <libhttp-client.h>

namespace pls {
namespace task {

#define QCSTR_name QStringLiteral("name")
#define QCSTR_parallel QStringLiteral("parallel")
#define QCSTR_timeout QStringLiteral("timeout")
#define QCSTR_must QStringLiteral("must")
#define QCSTR_silent QStringLiteral("silent")
#define QCSTR_dependencies QStringLiteral("dependencies")
#define QCSTR_exe QStringLiteral("exe")
#define QCSTR_params QStringLiteral("params")
#define QCSTR_localeDir QStringLiteral("localeDir")
#define QCSTR_locale QStringLiteral("locale")
#define QCSTR_path QStringLiteral("path")
#define QCSTR_msg QStringLiteral("msg")
#define QCSTR_percent QStringLiteral("percent")
#define QCSTR_exitCode QStringLiteral("exitCode")
#define QCSTR_normal QStringLiteral("normal")
#define QCSTR_exitAttrs QStringLiteral("exitAttrs")
#define QCSTR_ok QStringLiteral("ok")
#define QCSTR_tasks QStringLiteral("tasks")
#define QCSTR_attrs QStringLiteral("attrs")
#define QCSTR_close QStringLiteral("close")
#define QCSTR_way QStringLiteral("way")

#define log_info(format, ...) pls_debug(false, "libtask", __FILE__, __LINE__, format, __VA_ARGS__)

bool getBool(const QJsonObject &obj, const QString &name, bool defaultValue)
{
	auto value = obj[name];
	if (value.isNull() || value.isUndefined()) {
		return defaultValue;
	} else if (value.isBool()) {
		return value.toBool(defaultValue);
	}
	return defaultValue;
}
int getInt(const QJsonObject &obj, const QString &name, int defaultValue)
{
	auto value = obj[name];
	if (value.isNull() || value.isUndefined()) {
		return defaultValue;
	} else if (value.isDouble()) {
		return value.toInt(defaultValue);
	}
	return defaultValue;
}
int getInt(const QJsonObject &obj, const QStringList &names, int defaultValue)
{
	if (int size = names.size(); size > 1) {
		if (auto value = obj[names.first()]; value.isObject()) {
			return getInt(value.toObject(), names.mid(1), defaultValue);
		} else {
			return defaultValue;
		}
	} else if (size == 1) {
		return getInt(obj, names.first(), defaultValue);
	}
	return defaultValue;
}
ipc::mainproc::CloseWay getCloseWay(const QString &value, ipc::mainproc::CloseWay defaultValue)
{
	if (value == QStringLiteral("close")) {
		return ipc::mainproc::CloseWay::Close;
	} else if (value == QStringLiteral("kill")) {
		return ipc::mainproc::CloseWay::Kill;
	}
	return defaultValue;
}
ipc::mainproc::CloseWay getCloseWay(const QJsonObject &obj, const QString &name, ipc::mainproc::CloseWay defaultValue)
{
	auto value = obj[name];
	if (value.isNull() || value.isUndefined()) {
		return defaultValue;
	} else if (value.isString()) {
		return getCloseWay(value.toString().toLower(), defaultValue);
	}
	return defaultValue;
}
ipc::mainproc::CloseWay getCloseWay(const QJsonObject &obj, const QStringList &names, ipc::mainproc::CloseWay defaultValue)
{
	if (int size = names.size(); size > 1) {
		if (auto value = obj[names.first()]; value.isObject()) {
			return getCloseWay(value.toObject(), names.mid(1), defaultValue);
		} else {
			return defaultValue;
		}
	} else if (size == 1) {
		return getCloseWay(obj, names.first(), defaultValue);
	}
	return defaultValue;
}

namespace mainproc {
#define g_main Main::s_main
#define g_tasks Main::s_main.m_tasks
#define g_listener Main::s_main.m_listener

enum class Status { Initial, Running, Ok, Failed, FailedWithDeps, Timeout };

int runTasks(const QStringList &runTasks, const QStringList &args, const QVariantHash &attrs, bool skipFinished);
int continueRunTasks();

struct TaskImpl {
	Status m_status = Status::Initial;

	QString m_name;
	bool m_parallel = true;
	int m_timeout = -1;
	bool m_must = false;
	bool m_silent = true;
	ipc::mainproc::CloseWay m_closeWay = ipc::mainproc::CloseWay::Kill;
	int m_closeTimeout = 1000;
	int m_killExitCode = 0;
	QStringList m_dependencies;
	QString m_exe;
	QStringList m_params;
	QVariantHash m_attrs;
	QMap<QByteArray, QString> m_texts;
	bool m_notified = false;

	ipc::mainproc::Subproc m_subproc;

	//result
	bool m_normal = false;
	std::optional<int> m_exitCode;
	std::optional<QVariantHash> m_exitAttrs;
	QString m_msg;

	TaskImpl(const QString &name, bool parallel, int timeout, bool must, bool silent, const QStringList &dependencies, const QString &exe, const QStringList &params, const QVariantHash &attrs,
		 ipc::mainproc::CloseWay closeWay, int closeTimeout, int killExitCode)
		: m_name(name),
		  m_parallel(parallel),
		  m_timeout(timeout),
		  m_must(must),
		  m_silent(silent),
		  m_closeWay(closeWay),
		  m_closeTimeout(closeTimeout),
		  m_killExitCode(killExitCode),
		  m_dependencies(dependencies),
		  m_exe(exe),
		  m_params(params),
		  m_attrs(attrs)
	{
	}

	static bool isRunning(Status status) { return status == Status::Running; }
	static bool isFinished(Status status) { return isOk(status) || isFailed(status) || isFailedWithDeps(status) || isTimeout(status); }
	static bool isOk(Status status) { return status == Status::Ok; }
	static bool isFailed(Status status) { return status == Status::Failed; }
	static bool isFailedWithDeps(Status status) { return status == Status::FailedWithDeps; }
	static bool isTimeout(Status status) { return status == Status::Timeout; }

	bool isRunning() const { return isRunning(m_status); }
	bool isFinished() const { return isFinished(m_status); }
	bool isOk() const { return isOk(m_status); }
	bool isFailed() const { return isFailed(m_status); }
	bool isFailedWithDeps() const { return isFailedWithDeps(m_status); }
	bool isTimeout() const { return isTimeout(m_status); }
	bool setStatus(Status status)
	{
		if (!isRunning(m_status) && isRunning(status)) {
			m_status = Status::Running;
			return true;
		} else if (!isFinished(m_status) && isFinished(status)) {
			m_status = status;
			return true;
		}
		return false;
	}

	bool loadLocale()
	{
		QString localeDir = m_subproc.params().attr(QCSTR_localeDir).toString();
		QString locale = m_subproc.params().attr(QCSTR_locale).toString();
		QString localeFile = localeDir + '/' + locale + ".ini";
		if (!QFile::exists(localeFile)) {
			return false;
		}

		QSettings settings(localeFile, QSettings::IniFormat);
		for (const QString &key : settings.allKeys()) {
			m_texts[key.toUtf8()] = settings.value(key).toString();
		}
		return true;
	}
	QString tr(const QByteArray &source) const
	{
		if (auto iter = m_texts.find(source); iter != m_texts.end()) {
			return iter.value();
		}
		return QString();
	}

	bool isMust() const { return m_must; }
	bool isOptional() const { return !m_must; }

	bool open(const QStringList &runArgs, const QVariantHash &runAttrs, const std::list<TaskImplPtr> &deps) const
	{
		if (isRunning()) {
			return true;
		}

		QStringList args;
		args.append(m_subproc.resetArgs().params().args());
		args.append(runArgs);
		m_subproc.params().args(args);

		QVariantHash attrs;
		attrs.insert(m_subproc.resetAttrs().params().attrs());
		attrs.insert(m_attrs);
		attrs.insert(runAttrs);
		for (const auto &taskImpl : deps) {
			if (taskImpl->isOk() && taskImpl->m_exitAttrs.has_value()) {
				attrs.insert(taskImpl->m_exitAttrs.value());
			}
		}
		m_subproc.params().attrs(attrs);

		return m_subproc.open();
	}
	void close() const
	{
		if (m_closeWay == ipc::mainproc::CloseWay::Kill) {
			m_subproc.kill(m_killExitCode);
		} else {
			m_subproc.close();
		}
	}
	void wait(int timeout = -1) const { m_subproc.wait(timeout); }
	QVariantHash result() const
	{
		QVariantHash hash;
		hash[QCSTR_name] = m_name;
		hash[QCSTR_normal] = m_normal;
		if (m_exitCode.has_value())
			hash[QCSTR_exitCode] = m_exitCode.value();
		if (m_exitAttrs.has_value())
			hash[QCSTR_exitAttrs] = m_exitAttrs.value();
		hash[QCSTR_msg] = m_msg;
		return hash;
	}
};

struct Main {
	std::list<TaskImplPtr> m_tasks;
	Listener m_listener;

	mutable QStringList m_runArgs;
	mutable QVariantHash m_runAttrs;
	mutable std::list<TaskImplPtr> m_runTasks;

	static Main s_main;

	bool hasTask(const QString &name) const
	{
		for (const auto &taskImpl : m_tasks) {
			if (taskImpl->m_name == name) {
				return true;
			}
		}
		return false;
	}
	TaskImplPtr getTask(const QString &name) const
	{
		for (const auto &taskImpl : m_tasks) {
			if (taskImpl->m_name == name) {
				return taskImpl;
			}
		}
		return nullptr;
	}
	QStringList getAllTaskNames() const
	{
		QStringList taskNames;
		for (const auto &taskImpl : m_tasks) {
			taskNames.append(taskImpl->m_name);
		}
		return taskNames;
	}
	void continueRun(const TaskImplPtr &taskImpl) const
	{
		if (m_runTasks.empty() || (mainproc::continueRunTasks() != 0)) {
			return;
		}

		bool ok = true;
		QVariantList tasks;
		for (auto task : m_runTasks) {
			if (!task->isFinished()) {
				if (task->setStatus(Status::FailedWithDeps)) {
					log_info("task failed with dependencies, name: %s", task->m_name.toUtf8().constData());
					trigger(taskImpl, false, OpenFailedWithDepsExitCode, QVariantHash(), QStringLiteral("failed with dependencies"));
				}
			}

			ok = ok && task->isOk();
			tasks.append(task->result());
		}

		m_runArgs.clear();
		m_runAttrs.clear();
		m_runTasks.clear();

		//"ok"->bool, "tasks"->[{"name"->QString, "normal": bool, "exitCode"->int, "exitAttrs"->QVariantHash, "msg"->QString}]
		QVariantHash params;
		params[QCSTR_ok] = ok;
		params[QCSTR_tasks] = tasks;
		pls_invoke_safe(m_listener, TaskImplPtr(), Event::AllTaskFinished, params);
	}
	void trigger(const TaskImplPtr &taskImpl, int percent, const QString &msg) const
	{
		if (!taskImpl) {
			return;
		}

		QVariantHash params;
		params[QCSTR_name] = taskImpl->m_name;
		params[QCSTR_percent] = percent;
		params[QCSTR_msg] = msg;
		pls_invoke_safe(m_listener, taskImpl, Event::TaskProgressChanged, params);
	}
	void trigger(const TaskImplPtr &taskImpl, const QString &msg) const
	{
		if (!taskImpl || taskImpl->m_notified) {
			return;
		}

		taskImpl->m_notified = true;
		taskImpl->setStatus(Status::Failed);
		taskImpl->m_normal = false;
		taskImpl->m_msg = msg;

		QVariantHash params;
		params[QCSTR_name] = taskImpl->m_name;
		params[QCSTR_normal] = false;
		params[QCSTR_msg] = msg;
		pls_invoke_safe(m_listener, taskImpl, Event::TaskFinished, params);
		continueRun(taskImpl);
	}
	void trigger(const TaskImplPtr &taskImpl, bool normal, int exitCode, const QVariantHash &exitAttrs, const QString &msg) const
	{
		if (!taskImpl || taskImpl->m_notified) {
			return;
		}

		taskImpl->m_notified = true;
		taskImpl->setStatus(normal && (exitCode == 0) ? Status::Ok : Status::Failed);
		taskImpl->m_normal = normal;
		taskImpl->m_exitCode = exitCode;
		taskImpl->m_exitAttrs = exitAttrs;
		taskImpl->m_msg = msg;

		QVariantHash params;
		params[QCSTR_name] = taskImpl->m_name;
		params[QCSTR_normal] = normal;
		params[QCSTR_exitCode] = exitCode;
		params[QCSTR_exitAttrs] = exitAttrs;
		params[QCSTR_msg] = msg;
		pls_invoke_safe(m_listener, taskImpl, Event::TaskFinished, params);
		continueRun(taskImpl);
	}
	bool checkDependencies(std::list<TaskImplPtr> &deps, const QStringList &dependencies) const
	{
		for (const QString &dependency : dependencies) {
			if (auto taskImpl = getTask(dependency); taskImpl && (taskImpl->isOptional() || taskImpl->isOk())) {
				deps.push_back(taskImpl);
			} else {
				return false;
			}
		}
		return true;
	}
	int open(const TaskImplPtr &taskImpl) const
	{
		std::list<TaskImplPtr> deps;
		if (QStringList dependencies = taskImpl->m_subproc.params().attr(QCSTR_dependencies).toStringList(); !checkDependencies(deps, dependencies)) {
			return 0;
		} else if (!taskImpl->open(m_runArgs, m_runAttrs, deps)) {
			trigger(taskImpl, false, OpenFailedExitCode, QVariantHash(), QStringLiteral("open failed"));
		} else {
			taskImpl->setStatus(Status::Running);
			if (taskImpl->m_timeout >= 0) {
				std::weak_ptr<TaskImpl> weakTaskImpl = taskImpl;
				QTimer::singleShot(taskImpl->m_timeout, [weakTaskImpl]() {
					if (auto impl = weakTaskImpl.lock(); impl && impl->isRunning()) {
						log_info("%s task timeout and will be killed", impl->m_name.toUtf8().constData());
						impl->setStatus(Status::Timeout);
						impl->close();
					}
				});
			}
		}
		return 1;
	}
};
class Initializer {
public:
	Initializer()
	{
		pls_qapp_deconstruct_add_cb([this]() { this->cleanup(); });
	}

	static Initializer *initializer() { return &pls::Initializer<Initializer>::s_initializer; }

	void init() const
	{
		pls_env_add_path(pls_get_app_dir());
		pls_env_add_path(pls_get_app_dir() + QStringLiteral("/prism-plugins"));
	}
	void cleanup() const
	{
		close();
		for (const auto &taskImpl : g_tasks) {
			if (taskImpl->isRunning()) {
				taskImpl->wait(taskImpl->m_closeTimeout);
			}
		}
		g_tasks.clear();
	}
};

Main Main::s_main;

int runTasks(const QStringList &runTasks, const QStringList &args, const QVariantHash &attrs, bool skipFinished)
{
	if (!pls_current_is_main_thread()) {
		return -1;
	} else if (!g_main.m_runTasks.empty()) {
		return -2;
	} else if (runTasks.isEmpty()) {
		return 0;
	}

	QStringList names;
	for (const QString &runTask : runTasks) {
		if (auto taskImpl = g_main.getTask(runTask); taskImpl && !names.contains(runTask)) {
			if (skipFinished && taskImpl->isFinished()) {
				continue;
			}

			taskImpl->m_status = Status::Initial;
			g_main.m_runTasks.push_back(taskImpl);
			names.append(runTask);
		}
	}

	if (!names.isEmpty()) {
		g_main.m_runArgs = args;
		g_main.m_runAttrs = attrs;
		return continueRunTasks();
	}
	return 0;
}
int continueRunTasks()
{
	if (g_main.m_runTasks.empty()) {
		return -1;
	}

	int running = 0;
	for (TaskImplPtr taskImpl : g_main.m_runTasks) {
		if (taskImpl->isFinished()) {
			continue;
		} else if (taskImpl->isRunning()) {
			++running;
		} else if (taskImpl->m_subproc.params().attr(QCSTR_parallel).toBool()) {
			running += g_main.open(taskImpl);
		} else if (!running) {
			running += g_main.open(taskImpl);
			break;
		}
	}
	return running;
}

Task::Task(const TaskImplPtr &impl) : m_impl(impl) {}

Task::operator bool() const
{
	return m_impl.operator bool();
}

Task Task::from(const QString &name)
{
	if (!pls_current_is_main_thread()) {
		return {};
	}

	for (auto task : g_tasks) {
		if (task->m_name == name) {
			return task;
		}
	}
	return {};
}

QString Task::name() const
{
	return m_impl->m_name;
}
bool Task::parallel() const
{
	return m_impl->m_parallel;
}
int Task::timeout() const
{
	return m_impl->m_timeout;
}
bool Task::must() const
{
	return m_impl->m_must;
}
bool Task::silent() const
{
	return m_impl->m_silent;
}
QStringList Task::dependencies() const
{
	return m_impl->m_dependencies;
}
QString Task::exe() const
{
	return m_impl->m_exe;
}
QStringList Task::params() const
{
	return m_impl->m_params;
}
QString Task::tr(const char *key) const
{
	return m_impl->tr(QByteArray::fromRawData(key, (int)strlen(key)));
}
QString Task::tr(const QString &key) const
{
	return m_impl->tr(key.toUtf8());
}

bool Task::isRunning() const
{
	return m_impl->isRunning();
}
bool Task::isFinished() const
{
	return m_impl->isFinished();
}
bool Task::isOk() const
{
	return m_impl->isOk();
}
bool Task::isFailed() const
{
	return m_impl->isFailed();
}
bool Task::isFailedWithDeps() const
{
	return m_impl->isFailedWithDeps();
}
bool Task::isTimeout() const
{
	return m_impl->isTimeout();
}

bool Task::open(const QStringList &args, const QVariantHash &attrs) const
{
	if (!pls_current_is_main_thread()) {
		return false;
	} else if (!isRunning()) {
		return mainproc::runTasks({m_impl->m_name}, args, attrs, false) == 1;
	}
	return true;
}
void Task::close() const
{
	if (isRunning()) {
		m_impl->close();
	}
}
void Task::wait(int timeout) const
{
	if (isRunning()) {
		m_impl->wait(timeout);
	}
}

// return task count, -1 failed
LIBTASK_API int load(const QString &tasksDir, const QString &locale)
{
    return load(tasksDir, tasksDir, locale);
}
LIBTASK_API int load(const QString &tasksJsonDir, const QString &taskExeDir, const QString &locale)
{
	if (!pls_current_is_main_thread()) {
		return -1;
	}

	QJsonDocument doc;
	if (!pls_read_json(doc, tasksJsonDir + QStringLiteral("/tasks.json"))) {
		return -1;
	}

	pls_env_add_path(taskExeDir);

	int count = 0;
	for (const QJsonValue task : doc.array()) {
		QJsonObject obj = task.toObject();
		QString name = obj[QCSTR_name].toString();
		if (g_main.hasTask(name)) {
			continue;
		}

		bool parallel = getBool(obj, QCSTR_parallel, true);
		int timeout = getInt(obj, QCSTR_timeout, -1);
		bool must = getBool(obj, QCSTR_must, true);
		bool silent = getBool(obj, QCSTR_silent, true);
		QStringList dependencies = pls_to_string_list(obj[QCSTR_dependencies].toArray());
		QString exe = obj[QCSTR_exe].toString();
		QStringList params = pls_to_string_list(obj[QCSTR_params].toArray());
		QVariantHash attrs = obj[QCSTR_attrs].toObject().toVariantHash();
		ipc::mainproc::CloseWay closeWay = getCloseWay(obj, QStringList{QCSTR_close, QCSTR_way}, ipc::mainproc::CloseWay::Kill);
		int closeTimeout = getInt(obj, QStringList{QCSTR_close, QCSTR_timeout}, 1000);
		int killExitCode = getInt(obj, QStringList{QCSTR_close, QCSTR_exitCode}, 0);

		auto taskImpl = std::make_shared<TaskImpl>(name, parallel, timeout, must, silent, dependencies, exe, params, attrs, closeWay, closeTimeout, killExitCode);
		std::weak_ptr<TaskImpl> weakTaskImpl = taskImpl;
		taskImpl->m_subproc =
			ipc::mainproc::create(ipc::mainproc::Params()
						      .name(name)
						      .program(taskExeDir + '/' + exe)
						      .args(params)
						      .attr(QCSTR_parallel, parallel)
						      .attr(QCSTR_timeout, timeout)
						      .attr(QCSTR_must, must)
						      .attr(QCSTR_silent, silent)
						      .attr(QCSTR_dependencies, dependencies)
						      .attr(QCSTR_params, params)
						      .attr(QCSTR_localeDir, taskExeDir + '/' + name + QStringLiteral("/locale"))
						      .attr(QCSTR_locale, locale)
						      .autoClose(false)
						      .listen(qApp,
							      [weakTaskImpl](const ipc::mainproc::Subproc &subproc, ipc::Event event, const QVariantHash &paras) {
								      switch (event) {
								      case pls::ipc::Event::SubprocExiting:
									      log_info("task process Event::SubprocExiting, name: %s", subproc.params().name().toUtf8().constData());
									      g_main.trigger(weakTaskImpl.lock(), true, paras["exitCode"].toInt(), paras["exitAttrs"].toHash(),
											     paras["exitMsg"].toString());
									      break;
								      case pls::ipc::Event::SubprocExited:
									      log_info("task process Event::SubprocExited, name: %s", subproc.params().name().toUtf8().constData());
									      g_main.trigger(weakTaskImpl.lock(), paras["normal"].toBool(), subproc.exitCode(), subproc.exitAttrs(), subproc.exitMsg());
									      break;
								      case pls::ipc::Event::SubprocProgressChanged:
									      log_info("task process Event::SubprocProgressChanged, name: %s", subproc.params().name().toUtf8().constData());
									      g_main.trigger(weakTaskImpl.lock(), paras["percent"].toInt(), paras["msg"].toString());
									      break;
								      default:
									      break;
								      }
							      }))
				.saveDefaultProgram()
				.saveDefaultArgs()
				.saveDefaultAttrs();
		g_tasks.push_back(taskImpl);
		taskImpl->loadLocale();
		++count;
	}
	return count;
}
LIBTASK_API int run(const QStringList &args, const QVariantHash &attrs, bool skipFinished)
{
	if (pls_current_is_main_thread()) {
		return mainproc::runTasks(g_main.getAllTaskNames(), args, attrs, skipFinished);
	}
	return -1;
}
LIBTASK_API int run(const QStringList &runTasks, const QStringList &args, const QVariantHash &attrs, bool skipFinished)
{
	if (!pls_current_is_main_thread()) {
		return -1;
	} else if (!runTasks.isEmpty()) {
		return mainproc::runTasks(runTasks, args, attrs, skipFinished);
	}
	return 0;
}
LIBTASK_API void close()
{
	if (!pls_current_is_main_thread()) {
		return;
	}

	for (const auto &taskImpl : g_tasks) {
		if (taskImpl->isRunning()) {
			taskImpl->close();
		}
	}
}
LIBTASK_API void wait(int timeout)
{
	if (!pls_current_is_main_thread()) {
		return;
	}

	if (timeout < 0) {
		for (const auto &taskImpl : g_tasks) {
			if (taskImpl->isRunning()) {
				taskImpl->wait();
			}
		}
		return;
	}

	using namespace std::chrono;
	int rest = timeout;
	for (const auto &taskImpl : g_tasks) {
		if (!taskImpl->isRunning()) {
			continue;
		}

		auto st = steady_clock::now();
		taskImpl->wait(rest);
		if (rest -= (int)duration_cast<milliseconds>(steady_clock::now() - st).count(); rest <= 0) {
			break;
		}
	}
}

LIBTASK_API bool listen(const Listener &listener)
{
	if (!pls_current_is_main_thread()) {
		return false;
	}

	g_listener = listener;
	return true;
}

}

namespace subproc {
#define g_subproc TaskImpl::s_subproc

class Translator : public QTranslator {
	Q_OBJECT

public:
	QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr, int n = -1) const override
	{
		return translate(QByteArray::fromRawData(sourceText, (int)strlen(sourceText)));
	}

	bool isEmpty() const override { return m_texts.isEmpty(); }

	QString translate(const QByteArray &source) const
	{
		if (auto iter = m_texts.find(source); iter != m_texts.end()) {
			return iter.value();
		}
		return QString();
	}

	QMap<QByteArray, QString> m_texts;
};
struct ParamsImpl {
	Listener m_listener;
};
struct TaskImpl {
	ParamsImplPtr m_paramsImpl;
	ipc::subproc::Subproc m_subproc;
	Translator m_translator;

	static TaskImplPtr s_subproc;

	explicit TaskImpl(const ParamsImplPtr &paramsImpl, const ipc::subproc::Subproc &subproc) : m_paramsImpl(paramsImpl), m_subproc(subproc) {}

	bool loadLocale()
	{
		QString localeDir = m_subproc.attr(QCSTR_localeDir).toString();
		QString locale = m_subproc.attr(QCSTR_locale).toString();
		QString localeFile = localeDir + '/' + locale + ".ini";
		if (!QFile::exists(localeFile)) {
			return false;
		}

		QSettings settings(localeFile, QSettings::IniFormat);
		for (const QString &key : settings.allKeys()) {
			m_translator.m_texts[key.toUtf8()] = settings.value(key).toString();
		}

		if (!QCoreApplication::installTranslator(&m_translator)) {
			return false;
		}
		return true;
	}
};
class Initializer {
public:
	Initializer()
	{
		pls_qapp_deconstruct_add_cb([this]() { this->cleanup(); });
	}

	static Initializer *initializer() { return &pls::Initializer<Initializer>::s_initializer; }

	void init(const Params &params) const
	{
		if (!g_subproc) {
			std::lock_guard guard(pls_global_mutex());
			if (!g_subproc) {
				g_subproc = std::make_shared<TaskImpl>(
					params.m_impl,
					ipc::subproc::init(ipc::subproc::Params() //
								   .listen(qApp, [paramsImpl = params.m_impl](const ipc::subproc::Subproc &subproc, ipc::Event event, const QVariantHash &) {
									   switch (event) {
									   case pls::ipc::Event::MainprocExited:
										   QCoreApplication::quit();
										   break;
									   case pls::ipc::Event::RequireClose:
										   subproc.allowClose(true);
										   pls_invoke_safe(paramsImpl->m_listener, g_subproc, Event::RequireCloseTask, QVariantHash());
										   break;
									   default:
										   break;
									   }
								   })));
				g_subproc->loadLocale();
			}
		}
	}
	void cleanup() const
	{
		if (g_subproc) {
			g_subproc.reset();
		}
	}
};

TaskImplPtr TaskImpl::s_subproc;

Params::Params() : Params(std::make_shared<ParamsImpl>()) {}
Params::Params(const ParamsImplPtr &impl) : m_impl(impl) {}

const Params &Params::listen(const Listener &listener) const
{
	m_impl->m_listener = listener;
	return *this;
}

Task::Task(const TaskImplPtr &impl) : m_impl(impl) {}

QStringList Task::args() const
{
	return m_impl->m_subproc.args();
}
QString Task::env(const char *name, const QString &defaultValue) const
{
	return m_impl->m_subproc.env(name, defaultValue);
}
QVariantHash Task::attrs() const
{
	return m_impl->m_subproc.attrs();
}
QVariant Task::attr(const QString &name, const QVariant &defaultValue) const
{
	return m_impl->m_subproc.attr(name, defaultValue);
}
QString Task::tr(const char *key) const
{
	return m_impl->m_translator.translate(QByteArray::fromRawData(key, (int)strlen(key)));
}
QString Task::tr(const QString &key) const
{
	return m_impl->m_translator.translate(key.toUtf8());
}

bool Task::progress(int percent, const QString &msg) const
{
	return m_impl->m_subproc.progress(0, percent, msg);
}
void Task::exit(int exitCode, const QVariantHash &exitAttrs, const QString &exitMsg) const
{
	m_impl->m_subproc.exit(exitCode, exitAttrs, exitMsg);
}

LIBTASK_API Task init(const Params &params)
{
	if (!pls_current_is_main_thread()) {
		return {nullptr};
	}

	if (g_subproc) {
		return g_subproc;
	}

	Initializer::initializer()->init(params);
	return g_subproc;
}

}

}
}

#include "libtask.moc"
