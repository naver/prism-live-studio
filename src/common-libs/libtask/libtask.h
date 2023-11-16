#if !defined(_PRISM_COMMON_LIBTASK_LIBTASK_H)
#define _PRISM_COMMON_LIBTASK_LIBTASK_H

#include <optional>
#include <libipc.h>



#ifdef Q_OS_WIN

#ifdef LIBTASK_LIB
#define LIBTASK_API __declspec(dllexport)
#else
#define LIBTASK_API __declspec(dllimport)
#endif

#else
#define LIBTASK_API

#endif


namespace pls {
namespace task {

enum class Event {
	TaskProgressChanged, // main process: "name"->QString, "percent"->int(0~100), "msg"->QString
	TaskFinished,        // main process: "name"->QString, "normal": bool, "exitCode"->int, "exitAttrs"->QVariantHash, "msg"->QString
	AllTaskFinished,     // main process: "ok"->bool, "tasks"->[{"name"->QString, "normal": bool, "exitCode"->int, "exitAttrs"->QVariantHash, "msg"->QString}]
	RequireCloseTask,    // sub process
};

const int OpenFailedExitCode = 20000;
const int OpenFailedWithDepsExitCode = 20001;

namespace mainproc {
struct Task;
struct TaskImpl;
using TaskImplPtr = std::shared_ptr<TaskImpl>;
using Listener = std::function<void(Task task, Event event, const QVariantHash &params)>;

struct LIBTASK_API Task {
	mutable TaskImplPtr m_impl;

	Task() = default;
	Task(const TaskImplPtr &impl);

	operator bool() const;

	static Task from(const QString &name);

	QString name() const;
	bool parallel() const;
	int timeout() const;
	bool must() const;
	bool silent() const;
	QStringList dependencies() const;
	QString exe() const;
	QStringList params() const;
	QString tr(const char *key) const;
	QString tr(const QString &key) const;

	bool isRunning() const;
	bool isFinished() const;

	bool isOk() const;
	bool isFailed() const;
	bool isFailedWithDeps() const;
	bool isTimeout() const;

	bool open(const QStringList &args = QStringList(), const QVariantHash &attrs = QVariantHash()) const;
	void close() const;
	void wait(int timeout = -1) const;
};

// return task count, -1 failed
LIBTASK_API int load(const QString &tasksDir, const QString &locale);
LIBTASK_API int load(const QString &tasksJsonDir, const QString &taskExeDir, const QString &locale);

LIBTASK_API int run(const QStringList &args = QStringList(), const QVariantHash &attrs = QVariantHash(), bool skipFinished = true);
LIBTASK_API int run(const QStringList &runTasks, const QStringList &args = QStringList(), const QVariantHash &attrs = QVariantHash(), bool skipFinished = true);
LIBTASK_API void close();
LIBTASK_API void wait(int timeout = -1);

LIBTASK_API bool listen(const Listener &listener);

}

namespace subproc {
struct Task;
struct ParamsImpl;
struct TaskImpl;
using ParamsImplPtr = std::shared_ptr<ParamsImpl>;
using TaskImplPtr = std::shared_ptr<TaskImpl>;
using Listener = std::function<void(Task task, Event event, const QVariantHash &params)>;

struct LIBTASK_API Params {
	mutable ParamsImplPtr m_impl;

	Params();
	Params(const ParamsImplPtr &impl);

	const Params &listen(const Listener &listener) const;
};

struct LIBTASK_API Task {
	mutable TaskImplPtr m_impl;

	Task(const TaskImplPtr &impl);

	QStringList args() const;
	QString env(const char *name, const QString &defaultValue = QString()) const;
	QVariantHash attrs() const;
	QVariant attr(const QString &name, const QVariant &defaultValue = QVariant()) const;
	QString tr(const char *key) const;
	QString tr(const QString &key) const;

	bool progress(int percent, const QString &msg) const;
	void exit(int exitCode, const QVariantHash &exitAttrs = QVariantHash(), const QString &exitMsg = QString()) const;
};

LIBTASK_API Task init(const Params &params);

}

}
}

#endif // _PRISM_COMMON_LIBTASK_LIBTASK_H
