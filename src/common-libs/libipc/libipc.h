#if !defined(_PRISM_COMMON_LIBIPC_LIBIPC_H)
#define _PRISM_COMMON_LIBIPC_LIBIPC_H

#include <cstdint>
#include <functional>
#include <string>
#include <memory>
#include <qstring.h>
#include <qstringlist.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qvariant.h>

#ifdef Q_OS_WIN

#ifdef LIBIPC_LIB
#define LIBIPC_API __declspec(dllexport)
#else
#define LIBIPC_API __declspec(dllimport)
#endif

#else
#define LIBIPC_API

#endif

namespace pls {
namespace ipc {

enum class Event {
	MainprocExited,         // sub process: "normal"->bool, "exitCode"->int
	SocketConnected,        // both
	SocketDisconnected,     // both
	SubprocExiting,         // main process: "exitCode"->int, "exitAttrs"->QVariantHash, "exitMsg"->QString
	SubprocExited,          // main process: "normal"->bool, "exitCode"->int
	SubprocStatusChanged,   // both: "status"->int(Status), "old"->int(Status)
	SubprocProgressChanged, // main process: "type"->int(custom), "percent"->int(0~100), "msg"->QString
	RequireClose,           // both
	MsgReceived             // both: "type"->int, "msg"->QJsonObject
};

enum class Status {
	Starting = 1, // sub process starting
	Running,      // sub process running
	Closing,      // sub process closing
	Stopping,     // sub process stopping
	Stopped       // sub process stopped
};

enum class ExitStatus {
	NormalExit = 1, // process normal exit
	ExceptExit,     // process exception exit
	KilledExit      // process was killed
};

enum class WaitWay {
	Received = 1, // close request received
	Known,        // close request known
	Closed        // closed
};

namespace mainproc {
enum class CloseWay { Close, Kill };

struct Params;
struct Subproc;

struct ParamsImpl;
struct SubprocImpl;

using ParamsImplPtr = std::shared_ptr<ParamsImpl>;
using SubprocImplPtr = std::shared_ptr<SubprocImpl>;

using Listener = std::function<void(Subproc subproc, Event event, const QVariantHash &params)>;

struct LIBIPC_API Params {
	mutable ParamsImplPtr m_impl;

	Params();
	Params(const ParamsImplPtr &paramsImpl);

	Params &operator=(const ParamsImplPtr &paramsImpl);

	QString name() const;
	const Params &name(const QString &name) const;

	QString program() const;
	const Params &program(const QString &program) const;

	QStringList args() const;
	const Params &args(const QStringList &args) const;
	const Params &arg(const QString &arg) const;

	QVariantHash attrs() const;
	const Params &attrs(const QVariantHash &attrs) const;

	QVariant attr(const QString &name) const;
	const Params &attr(const QString &name, const QVariant &value) const;

	bool autoClose() const;
	const Params &autoClose(bool autoClose, CloseWay closeWay = CloseWay::Kill, int waitTimeout = 1000, int killExitCode = 0) const;

	const Params &listen(QObject *receiver, const Listener &listener) const;
};

struct LIBIPC_API Subproc {
	mutable SubprocImplPtr m_impl;

	Subproc() = default;
	Subproc(const SubprocImplPtr &impl);
	Subproc &operator=(const SubprocImplPtr &impl);

	explicit operator bool() const;
	bool operator==(const Subproc &subproc) const;
	bool operator==(const QString &name) const;
	bool operator!=(const Subproc &subproc) const;
	bool operator!=(const QString &name) const;

	Params params() const;

	const Subproc &saveDefaultProgram() const;
	const Subproc &resetProgram() const;

	const Subproc &saveDefaultArgs() const;
	const Subproc &resetArgs() const;

	const Subproc &saveDefaultAttrs() const;
	const Subproc &resetAttrs() const;

	Status status() const;
	QString errors() const;

	QVariantHash exitAttrs() const;
	QVariant exitAttr(const QString &name) const;
	int exitCode() const;
	QString exitMsg() const;

	bool allowClose(bool allow = true) const;

	bool open() const;
	bool close() const;
	void kill(int exitCode) const;
	bool wait(int timeout = -1) const;

	bool send(int type, const QJsonObject &msg) const;
};

LIBIPC_API quint16 listenPort();

LIBIPC_API Subproc create(const Params &params);
LIBIPC_API void closeAll();
LIBIPC_API void killAll(int exitCode);
LIBIPC_API void waitAll(int timeout = -1);
}

namespace subproc {
struct Params;
struct Subproc;

struct ParamsImpl;
struct SubprocImpl;

using ParamsImplPtr = std::shared_ptr<ParamsImpl>;
using SubprocImplPtr = std::shared_ptr<SubprocImpl>;

using Listener = std::function<void(Subproc subproc, Event event, const QVariantHash &params)>;

struct LIBIPC_API Params {
	mutable ParamsImplPtr m_impl;

	Params();
	Params(const ParamsImplPtr &impl);
	Params &operator=(const ParamsImplPtr &impl);

	QString name() const;
	const Params &name(const QString &name) const;

	quint16 serverPort() const;
	const Params &serverPort(quint16 serverPort) const;

	const Params &listen(QObject *receiver, const Listener &listener) const;
};

struct LIBIPC_API Subproc {
	mutable SubprocImplPtr m_impl;

	Subproc(const SubprocImplPtr &impl);
	Subproc &operator=(const SubprocImplPtr &impl);

	explicit operator bool() const;

	QStringList args() const;
	QString env(const char *name, const QString &defaultValue = QString()) const;
	QVariantHash attrs() const;
	QVariant attr(const QString &name, const QVariant &defaultValue = QVariant()) const;

	Status status() const;
	QString errors() const;

	bool reopen(qint32 mainProcessId, quint16 serverPort = 0) const;
	bool allowClose(bool allow = true) const;
	bool progress(int type, int percent, const QString &msg) const;
	void exit(int exitCode, const QVariantHash &exitAttrs = QVariantHash(), const QString &exitMsg = QString()) const;
	bool send(int type, const QJsonObject &msg) const;

	// must be call in main thread
	bool isMainClosed(int timeout = -1) const;
	bool closeMain(int timeout = -1, WaitWay waitWay = WaitWay::Closed) const;
	bool killMain() const;
};

LIBIPC_API Subproc init(const Params &params);

}

}
}

#endif // _PRISM_COMMON_LIBIPC_LIBIPC_H
