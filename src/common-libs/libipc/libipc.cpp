#include "libipc.h"

#include <stdarg.h>
#include <stdio.h>
//#include <process.h>

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>

#include <qcoreapplication.h>
#include <quuid.h>
#include <qtcpserver.h>
#include <qtcpsocket.h>
#include <qthread.h>
#include <qjsondocument.h>
#include <qtimer.h>
#include <qsemaphore.h>
#include <qdir.h>

#include <libutils-api.h>
#include <libhttp-client.h>
#include <liblog.h>

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <malloc.h>
#elif defined(Q_OS_MACOS)
#include <sys/malloc.h>
#endif

namespace pls {
namespace ipc {

#define QCSTR_name QStringLiteral("name")
#define QCSTR_exitCode QStringLiteral("exitCode")
#define QCSTR_exitAttrs QStringLiteral("exitAttrs")
#define QCSTR_exitMsg QStringLiteral("exitMsg")
#define QCSTR_type QStringLiteral("type")
#define QCSTR_msg QStringLiteral("msg")
#define QCSTR_percent QStringLiteral("percent")
#define QCSTR_standard QStringLiteral("standard")
#define QCSTR_content QStringLiteral("content")
#define QCSTR_status QStringLiteral("status")
#define QCSTR_old QStringLiteral("old")
#define QCSTR_allow QStringLiteral("allow")
#define QCSTR_normal QStringLiteral("normal")

#if defined(Q_OS_WIN)
#define log_info(format, ...) pls_info(false, "libipc", __FILE__, __LINE__, format, __VA_ARGS__)
#define log_info_kr(format, ...) pls_info(true, "libipc", __FILE__, __LINE__, format, __VA_ARGS__)
#define log_warn(format, ...) pls_warn(false, "libipc", __FILE__, __LINE__, format, __VA_ARGS__)
#define log_error(format, ...) pls_error(false, "libipc", __FILE__, __LINE__, format, __VA_ARGS__)
#else
#define log_info(format, args...) pls_info(false, "libipc", __FILE__, __LINE__, format, ##args)
#define log_info_kr(format, args...) pls_info(true, "libipc", __FILE__, __LINE__, format, ##args)
#define log_warn(format, args...) pls_warn(false, "libipc", __FILE__, __LINE__, format, ##args)
#define log_error(format, args...) pls_error(false, "libipc", __FILE__, __LINE__, format, ##args)
#endif

namespace protocol {
const int MsgType0KeepliveReq = 1;
const int MsgType0KeepliveRsp = 100001;
const int MsgType0AttrsReq = 2;
const int MsgType0AttrsRsp = 100002;
const int MsgType0OkReq = 3;
const int MsgType0OkRsp = 100003;
const int MsgType0ProgressReq = 4;
const int MsgType0ProgressRsp = 100004;
const int MsgType0ExitInfoReq = 5;
const int MsgType0ExitInfoRsp = 100005;
const int MsgType0CloseReq = 6;
const int MsgType0CloseReceivedRsp = 100006;
const int MsgType0CloseKnownRsp = 100007;
const int MsgType0Custom = 800000;

struct MsgHead {
	int type0;
	int type;
	int totalLength;
};

// MsgType0KeepliveReq sub->main
//	no body
// MsgType0KeepliveRsp main->sub
//	no body

// MsgType0AttrsReq sub->main
//	body: json object
//		name: QString
// MsgType0AttrsRsp main->sub
//	body: json object => attrs

// MsgType0OkReq sub->main
//	no body
// MsgType0OkRsp main->sub
//	no body

// MsgType0ProgressReq sub->main
//	body: json object
//		type: int
// 		percent: int
//		msg: QString
// MsgType0ProgressRsp main->sub
//	no body

// MsgType0ExitInfoReq sub->main
//	body: json object
//		exitCode: int
// 		exitAttrs: QVariantHash
//		exitMsg: QString
// MsgType0ExitInfoRsp main->sub
//	no body

// MsgType0CloseReq sub->main
//	no body
// MsgType0CloseRsp main->sub
//	body: json object
//		allow: bool
}

enum class CloseState { None, Request, Received, Known, Closed };

class TcpSocket : public QTcpSocket {
	Q_OBJECT

public:
	explicit TcpSocket(bool isMain, const QString &name = QString()) : m_isMain(isMain), m_proc(isMain ? "main" : "sub"), m_name(name)
	{
		QObject::connect(this, &TcpSocket::readyRead, this, &TcpSocket::onReadData);
		QObject::connect(this, &TcpSocket::connected, this, [this]() {
			log_info("%s: %s process socket connected", m_name.toUtf8().constData(), m_proc);
			socketConnected(this, m_name);
		});
		QObject::connect(this, &TcpSocket::disconnected, this, [this]() {
			log_error("%s: %s process socket disconnected", m_name.toUtf8().constData(), m_proc);
			stopKeeplive();
			socketDisconnected(m_name);
			deleteLater();
		});
	}
	~TcpSocket() override
	{
		stopKeeplive();
		socketDestroyed(m_name);
	}

	QString name() const { return m_name; }

	bool sendMsg(const QByteArray &msg)
	{
		if (pls_object_is_valid(this)) {
			return QMetaObject::invokeMethod(this, "onWriteData", Qt::QueuedConnection, Q_ARG(QByteArray, msg));
		}
		return false;
	}
	bool sendInnerMsg(int type0, const QJsonObject &msg = QJsonObject()) { return sendMsg(buildInnerMsg(type0, msg)); }
	bool sendCustomMsg(int type, const QJsonObject &msg = QJsonObject()) { return sendMsg(buildCustomMsg(type, msg)); }

	bool sendAttrsReq()
	{
		log_info("%s: %s process send attrs req", m_name.toUtf8().constData(), m_proc);
		return sendInnerMsg(protocol::MsgType0AttrsReq, {{QCSTR_name, m_name}});
	}
	bool sendAttrsRsp(const QVariantHash &attrs)
	{
		log_info("%s: %s process send attrs rsp", m_name.toUtf8().constData(), m_proc);
		return sendInnerMsg(protocol::MsgType0AttrsRsp, QJsonObject::fromVariantHash(attrs));
	}
	bool sendOkReq()
	{
		log_info("%s: %s process send ok req", m_name.toUtf8().constData(), m_proc);
		return sendInnerMsg(protocol::MsgType0OkReq);
	}
	bool sendOkRsp()
	{
		log_info("%s: %s process send ok rsp", m_name.toUtf8().constData(), m_proc);
		return sendInnerMsg(protocol::MsgType0OkRsp);
	}
	bool sendProgressReq(int type, int percent, const QString &msg)
	{
		log_info("%s: %s process send progress req", m_name.toUtf8().constData(), m_proc);
		return sendInnerMsg(protocol::MsgType0ProgressReq, {{QCSTR_type, type}, {QCSTR_percent, percent}, {QCSTR_msg, msg}});
	}
	bool sendProgressRsp()
	{
		log_info("%s: %s process send progress rsp", m_name.toUtf8().constData(), m_proc);
		return sendInnerMsg(protocol::MsgType0ProgressRsp);
	}
	bool sendExitInfoReq(int exitCode, const QVariantHash &exitAttrs, const QString &exitMsg)
	{
		log_info("%s: %s process send exit info req", m_name.toUtf8().constData(), m_proc);
		return sendInnerMsg(protocol::MsgType0ExitInfoReq, {{QCSTR_exitCode, exitCode}, {QCSTR_exitAttrs, QJsonObject::fromVariantHash(exitAttrs)}, {QCSTR_exitMsg, exitMsg}});
	}
	bool sendExitInfoRsp()
	{
		log_info("%s: %s process send exit info rsp", m_name.toUtf8().constData(), m_proc);
		return sendInnerMsg(protocol::MsgType0ExitInfoRsp);
	}
	bool sendCloseReq()
	{
		log_info("%s: %s process send close req", m_name.toUtf8().constData(), m_proc);
		return sendInnerMsg(protocol::MsgType0CloseReq);
	}
	bool sendCloseReceivedRsp()
	{
		log_info("%s: %s process send close received rsp", m_name.toUtf8().constData(), m_proc);
		return sendInnerMsg(protocol::MsgType0CloseReceivedRsp);
	}
	bool sendCloseKnownRsp(bool allow)
	{
		log_info("%s: %s process send close known rsp", m_name.toUtf8().constData(), m_proc);
		return sendInnerMsg(protocol::MsgType0CloseKnownRsp, {{QCSTR_allow, allow}});
	}

	void sendKeepliveReq() { sendInnerMsg(protocol::MsgType0KeepliveReq); }
	void sendKeepliveRsp() { sendInnerMsg(protocol::MsgType0KeepliveRsp); }
	void checkKeeplive()
	{
		using namespace std::chrono;
		if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_livetp) >= 5s) {
			log_info("%s: %s process no data in 5s, close socket", m_name.toUtf8().constData(), m_proc);
			close();
		}
	}
	void startKeeplive()
	{
		stopKeeplive();
#if 0
		m_livetp = std::chrono::steady_clock::now();
		m_keeplive = pls_new<QTimer>();
		QObject::connect(m_keeplive, &QTimer::timeout, this, m_isMain ? &TcpSocket::checkKeeplive : &TcpSocket::sendKeepliveReq);
		m_keeplive->start(3000);
#endif
	}
	void stopKeeplive()
	{
		if (m_keeplive) {
			m_keeplive->stop();
			pls_delete(m_keeplive, nullptr);
		}
	}

private:
	QByteArray buildMsg(int type0, int type, const QJsonObject &msg) const
	{
		QByteArray body = QJsonDocument(msg).toJson();

		auto totalLength = sizeof(protocol::MsgHead) + body.length();
		QByteArray innerMsg(totalLength, Qt::Uninitialized);
		auto head = (protocol::MsgHead *)innerMsg.data();
		head->type0 = type0;
		head->type = type;
		head->totalLength = (int)totalLength;
		memcpy(head + 1, body.constData(), body.length());
		return innerMsg;
	}
	QByteArray buildInnerMsg(int type0, const QJsonObject &msg) const { return buildMsg(type0, 0, msg); }
	QByteArray buildCustomMsg(int type, const QJsonObject &msg) const { return buildMsg(protocol::MsgType0Custom, type, msg); }

private slots:
	void onReadData()
	{
		m_buf.append(this->readAll());

		while (true) {
			auto head = (protocol::MsgHead *)m_buf.data();
			if ((m_buf.length() >= sizeof(protocol::MsgHead)) && (m_buf.length() >= head->totalLength)) {
				QByteArray buf = m_buf.left(head->totalLength);
				onMsg((protocol::MsgHead *)buf.data());
				m_buf = m_buf.mid(head->totalLength);
			} else {
				break;
			}
		}
	}
	void onWriteData(const QByteArray &msg) { write(msg); }

private:
	void onMsg(const protocol::MsgHead *head)
	{
		m_livetp = std::chrono::steady_clock::now();

		if (head->type0 == protocol::MsgType0KeepliveReq) {
			sendKeepliveRsp();
			return;
		} else if (head->type0 == protocol::MsgType0KeepliveRsp) {
			return;
		}

		auto json = (const char *)(head + 1);

		QJsonParseError error;
		QJsonDocument doc = QJsonDocument::fromJson(json, &error);
		if (error.error != QJsonParseError::NoError) {
			log_error("%s process receive msg parse json failed. error: %s", m_proc, error.errorString().toUtf8().constData());
			return;
		}

		QJsonObject object = doc.object();
		switch (head->type0) {
		case protocol::MsgType0AttrsReq:
			m_name = object[QCSTR_name].toString();
			log_info("%s: %s process receive attrs req: %s", m_name.toUtf8().constData(), m_proc, json);
			attrsReq(this);
			break;
		case protocol::MsgType0AttrsRsp:
			log_info("%s: %s process receive attrs rsp: %s", m_name.toUtf8().constData(), m_proc, json);
			attrsRsp(this, object.toVariantHash());
			break;
		case protocol::MsgType0OkReq:
			log_info("%s: %s process receive ok req", m_name.toUtf8().constData(), m_proc);
			okReq(this);
			break;
		case protocol::MsgType0OkRsp:
			log_info("%s: %s process receive ok rsp", m_name.toUtf8().constData(), m_proc);
			okRsp(this);
			break;
		case protocol::MsgType0ProgressReq:
			log_info("%s: %s process receive progress req: %s", m_name.toUtf8().constData(), m_proc, json);
			progressReq(this, object[QCSTR_type].toInt(), object[QCSTR_percent].toInt(), object[QCSTR_msg].toString());
			break;
		case protocol::MsgType0ProgressRsp:
			log_info("%s: %s process receive progress rsp", m_name.toUtf8().constData(), m_proc);
			progressRsp(this);
			break;
		case protocol::MsgType0ExitInfoReq:
			log_info("%s: %s process receive exit info req: %s", m_name.toUtf8().constData(), m_proc, json);
			exitInfoReq(this, object[QCSTR_exitCode].toInt(), object[QCSTR_exitAttrs].toObject().toVariantHash(), object[QCSTR_exitMsg].toString());
			break;
		case protocol::MsgType0ExitInfoRsp:
			log_info("%s: %s process receive exit info rsp", m_name.toUtf8().constData(), m_proc);
			exitInfoRsp(this);
			break;
		case protocol::MsgType0CloseReq:
			log_info("%s: %s process receive close req", m_name.toUtf8().constData(), m_proc);
			closeReq(this);
			break;
		case protocol::MsgType0CloseReceivedRsp:
			log_info("%s: %s process receive close received rsp", m_name.toUtf8().constData(), m_proc);
			closeReceivedRsp(this);
			break;
		case protocol::MsgType0CloseKnownRsp:
			log_info("%s: %s process receive close known rsp: %s", m_name.toUtf8().constData(), m_proc, json);
			closeKnownRsp(this, object[QCSTR_allow].toBool());
			break;
		case protocol::MsgType0Custom:
			log_info("%s: %s process receive custom msg: %s", m_name.toUtf8().constData(), m_proc, json);
			custom(this, head->type, object);
			break;
		default:
			log_error("%s process receive unknown msg", m_proc);
			close();
			break;
		}
	}

signals:
	void attrsReq(TcpSocket *socket);
	void attrsRsp(TcpSocket *socket, const QVariantHash &attrs);
	void okReq(TcpSocket *socket);
	void okRsp(TcpSocket *socket);
	void progressReq(TcpSocket *socket, int type, int percent, const QString &msg);
	void progressRsp(TcpSocket *socket);
	void exitInfoReq(TcpSocket *socket, int exitCode, const QVariantHash &exitAttrs, const QString &exitMsg);
	void exitInfoRsp(TcpSocket *socket);
	void closeReq(TcpSocket *socket);
	void closeReceivedRsp(TcpSocket *socket);
	void closeKnownRsp(TcpSocket *socket, bool allow);
	void custom(TcpSocket *socket, int type, const QJsonObject &msg);
	void socketConnected(TcpSocket *socket, const QString &name);
	void socketDisconnected(const QString &name);
	void socketDestroyed(const QString &name);

private:
	bool m_isMain = true;
	const char *m_proc = nullptr;
	QString m_name;
	std::chrono::steady_clock::time_point m_livetp;
	QByteArray m_buf;
	QTimer *m_keeplive = nullptr;
};

namespace mainproc {

#define g_mainproc Main::s_main
#define g_mainprocServerPort Main::s_main->m_server->serverPort()

void asyncCallInMain(const std::function<void()> &cb);
quint16 getServerPort();

struct DefaultParamsImpl {
	QString m_program;
	QStringList m_args;
	QVariantHash m_attrs;
};
struct ParamsImpl {
	QString m_name;
	QString m_program;
	QStringList m_args;
	QVariantHash m_attrs;
	bool m_autoClose = true;
	CloseWay m_closeWay = CloseWay::Kill;
	int m_waitTimeout = 1000;
	int m_killExitCode = 0;
	QObject *m_receiver = nullptr;
	Listener m_listener;
};
struct SubprocImpl {
	DefaultParamsImpl m_defaultParamsImpl;
	ParamsImplPtr m_paramsImpl;

	Status m_status = Status::Stopped;
	QString m_errors;

	QVariantHash m_exitAttrs;
	int m_exitCode0 = 0;
	int m_exitCode = 0;
	ExitStatus m_exitStatus{};
	QString m_exitMsg;

	std::mutex m_processExitCheckMutex;
	pls_process_t *m_process = nullptr;
	TcpSocket *m_socket = nullptr;

	std::atomic<CloseState> m_subCloseState = CloseState::None;

	std::list<std::function<bool()>> m_stoppedHooks;

	explicit SubprocImpl(const ParamsImplPtr &paramsImpl) : m_paramsImpl(paramsImpl) {}

	const QString &name() const { return m_paramsImpl->m_name; }
	bool autoClose() const { return m_paramsImpl->m_autoClose; }
	CloseWay closeWay() const { return m_paramsImpl->m_closeWay; }
	int waitTimeout() const { return m_paramsImpl->m_waitTimeout; }
	int killExitCode() const { return m_paramsImpl->m_killExitCode; }
	QObject *receiver() const { return m_paramsImpl->m_receiver; }
	const Listener &listener() const { return m_paramsImpl->m_listener; }

	bool isStarting() const { return m_status == Status::Starting; }
	bool isRunning() const { return m_status == Status::Running || m_status == Status::Closing; }
	bool isStopping() const { return m_status == Status::Stopping; }
	bool isStopped() const { return m_status == Status::Stopped; }
	void setStatus(const SubprocImplPtr &subprocImpl, Status status)
	{
		if (m_status == status) {
			return;
		}

		Status old = m_status;
		m_status = status;
		if (status == Status::Stopped) {
			m_subCloseState = CloseState::Closed;
		}

		trigger(subprocImpl, status, old);

		switch (status) {
		case pls::ipc::Status::Starting:
			log_info("main process %s starting", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Status::Running:
			log_info("main process %s running", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Status::Stopping:
			log_info("main process %s stopping", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Status::Stopped:
			log_info("main process %s stopped", subprocImpl->name().toUtf8().constData());
			break;
		default:
			break;
		}
	}
	bool isNormalExit() const { return m_exitStatus == ExitStatus::NormalExit; }
	void setExitStatus(ExitStatus exitStatus)
	{
		if (m_exitStatus == ExitStatus{}) {
			m_exitStatus = exitStatus;
		}
	}
	bool allowClose(bool allow)
	{
		if (pls_object_is_valid(m_socket)) {
			return m_socket->sendCloseKnownRsp(allow);
		}
		return false;
	}
	void callStoppedHooks()
	{
		for (auto iter = m_stoppedHooks.begin(); iter != m_stoppedHooks.end();) {
			auto hook = *iter;
			if (hook()) {
				iter = m_stoppedHooks.erase(iter);
			} else {
				++iter;
			}
		}
	}

	void closeSocket()
	{
		if (m_socket) {
			m_socket->close();
			m_socket = nullptr;
		}
	}
	bool processIsExited(const SubprocImplPtr &subprocImpl)
	{
		if (!m_process) {
			return true;
		}

		std::lock_guard guard(m_processExitCheckMutex);
		if (pls_process_wait(m_process, 0) > 0) {
			uint32_t exitCode = -1;
			bool ok = pls_process_exit_code(&exitCode, m_process);
			bool crash = ok && (exitCode >= 0x80000000) && (exitCode < 0xD0000000);
			log_info("main process check sub process is %s exited, name: %s, exitCode: %d", ok ? "normal" : "crashed", name().toUtf8().constData(), exitCode);

			setStatus(subprocImpl, Status::Stopped);
			setExitStatus(crash ? ExitStatus::ExceptExit : ExitStatus::NormalExit);
			m_exitCode = (int)exitCode;

			asyncCallInMain([subprocImpl, crash, exitCode]() { SubprocImpl::trigger(subprocImpl, crash, exitCode); });
			callStoppedHooks();
			closeProcess();
			return true;
		}
		return false;
	}
	void closeProcess()
	{
		if (m_process) {
			pls_process_destroy(m_process);
			m_process = nullptr;
		}
	}

	static void trigger(const SubprocImplPtr &subprocImpl, Event event, const QVariantHash &params = QVariantHash())
	{
		pls_async_call(subprocImpl->receiver(), [subprocImpl, event, params]() { pls_invoke_safe(subprocImpl->listener(), subprocImpl, event, params); });

		switch (event) {
		case pls::ipc::Event::MainprocExited:
			log_info("main process trigger Event::MainprocExited, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::SocketConnected:
			log_info("main process trigger Event::SocketConnected, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::SocketDisconnected:
			log_info("main process trigger Event::SocketDisconnected, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::SubprocExiting:
			log_info("main process trigger Event::SubprocExiting, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::SubprocExited:
			log_info("main process trigger Event::SubprocExited, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::SubprocStatusChanged:
			log_info("main process trigger Event::SubprocStatusChanged, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::SubprocProgressChanged:
			log_info("main process trigger Event::SubprocProgressChanged, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::RequireClose:
			log_info("main process trigger Event::RequireClose, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::MsgReceived:
			log_info("main process trigger Event::MsgReceived, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		default:
			break;
		}
	}
	static void trigger(const SubprocImplPtr &subprocImpl, Status status, Status old)
	{
		QVariantHash params;
		params[QCSTR_status] = static_cast<int>(status);
		params[QCSTR_old] = static_cast<int>(old);
		trigger(subprocImpl, Event::SubprocStatusChanged, params);
	}
	static void trigger(const SubprocImplPtr &subprocImpl, int type, const QJsonObject &msg)
	{
		QVariantHash params;
		params[QCSTR_type] = type;
		params[QCSTR_msg] = msg;
		trigger(subprocImpl, Event::MsgReceived, params);
	}
	static void trigger(const SubprocImplPtr &subprocImpl, int type, int percent, const QString &msg)
	{
		QVariantHash params;
		params[QCSTR_type] = type;
		params[QCSTR_percent] = percent;
		params[QCSTR_msg] = msg;
		trigger(subprocImpl, Event::SubprocProgressChanged, params);
	}
	static void trigger(const SubprocImplPtr &subprocImpl, bool normal, qint32 exitCode)
	{
		QVariantHash params;
		params[QCSTR_normal] = normal;
		params[QCSTR_exitCode] = exitCode;
		trigger(subprocImpl, Event::SubprocExited, params);
	}
	static void trigger(const SubprocImplPtr &subprocImpl, int exitCode, const QVariantHash &exitAttrs, const QString &exitMsg)
	{
		QVariantHash params;
		params[QCSTR_exitCode] = exitCode;
		params[QCSTR_exitAttrs] = exitAttrs;
		params[QCSTR_exitMsg] = exitMsg;
		trigger(subprocImpl, Event::SubprocExiting, params);
	}

	bool open(const SubprocImplPtr &subprocImpl)
	{
		if (isStarting() || isRunning()) {
			return true;
		}

		if (!isStopped()) {
			return false;
		}

		//reset
		m_errors.clear();
		m_exitAttrs.clear();
		m_exitCode = 0;
		m_exitMsg.clear();
		m_exitStatus = ExitStatus{};
		closeProcess();

		//starting
		log_info("main process ready to open subprocess, name: %s", m_paramsImpl->m_name.toUtf8().constData());
		log_info_kr("main process ready to open subprocess, name: %s, path: %s", m_paramsImpl->m_name.toUtf8().constData(), m_paramsImpl->m_program.toUtf8().constData());
		setStatus(subprocImpl, Status::Starting);

		auto program = m_paramsImpl->m_program;

		QStringList args = m_paramsImpl->m_args;
		args.append(QStringLiteral("--libipc-subproc-name=%1").arg(name()));
		args.append(QStringLiteral("--libipc-mainproc-server-port=%1").arg(getServerPort()));
		args.append(QStringLiteral("--libipc-mainproc-process-id=%1").arg(pls_current_process_id()));

		log_info_kr("main process open subprocess, name: %s, program: %s, args: %s", m_paramsImpl->m_name.toUtf8().constData(), program.toUtf8().constData(),
			    args.join(' ').toUtf8().constData());

		m_process = pls_process_create(program, args, true);
		if (m_process) {
			log_info("main process open subprocess ok, name: %s, pid: %u", m_paramsImpl->m_name.toUtf8().constData(), pls_process_id(m_process));
			setStatus(subprocImpl, Status::Running);
			return true;
		}

		auto errorString = QStringLiteral("pls_process_create failed, error code: %1").arg(pls_last_error());
		m_errors.append(errorString);
		log_error(errorString.toUtf8().constData());
		setStatus(subprocImpl, Status::Stopped);
		callStoppedHooks();
		return false;
	}
	bool close(const SubprocImplPtr &subprocImpl)
	{
		if (!isRunning()) {
			return false;
		}

		log_info("close sub process, name: %s", name().toUtf8().constData());
		setStatus(subprocImpl, Status::Closing);
		m_subCloseState = CloseState::Request;

		if (pls_object_is_valid(m_socket)) {
			return m_socket->sendCloseReq();
		}
		return false;
	}
	bool kill(const SubprocImplPtr &subprocImpl, int exitCode)
	{
		if (isStopped()) {
			return true;
		}

		log_info("kill sub process, name: %s", name().toUtf8().constData());
		setStatus(subprocImpl, Status::Stopping);
		m_subCloseState = CloseState::Closed;
		setExitStatus(ExitStatus::KilledExit);
		if (m_process && !processIsExited(subprocImpl)) {
			if (!pls_process_terminate(m_process, exitCode)) {
				closeProcess();

				log_warn("pls_process_terminate failed, name: %s, exitCode: %d, errorCode: %u", name().toUtf8().constData(), exitCode, pls_last_error());

				setStatus(subprocImpl, Status::Stopped);
				m_exitCode = exitCode;

				asyncCallInMain([subprocImpl, exitCode]() { SubprocImpl::trigger(subprocImpl, false, exitCode); });
				callStoppedHooks();
			} else {
				log_info("TerminateProcess success, name: %s, exitCode: %d", name().toUtf8().constData(), exitCode);
			}
		}
		return true;
	}

	bool send(int type, const QJsonObject &msg) const
	{
		if (isRunning() && pls_object_is_valid(m_socket)) {
			return m_socket->sendCustomMsg(type, msg);
		}
		return false;
	}
};
class TcpServer : public QTcpServer {
	Q_OBJECT

protected:
	void incomingConnection(qintptr handle) override { emit newSocket(handle); }

signals:
	void newSocket(qintptr handle);
};
struct Main : public http::ExclusiveWorker {
	Q_OBJECT

public:
	bool m_running = true;
	TcpServer *m_server = nullptr;
	mutable std::shared_mutex m_subprocsMutex;
	std::map<QString, std::weak_ptr<SubprocImpl>> m_subprocs;
	std::thread m_monitor;

	static Main *s_main;

	Main()
	{
		syncCall([this]() { startListen(); });
		startMonitor();
	}

	void run() override
	{
#if defined(Q_OS_WIN)
		auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		if (FAILED(hr)) {
			log_error(QString("CoInitializeEx failed, error code: %1").arg(hr).toUtf8().constData());
		}
#endif

		http::ExclusiveWorker::run();

		if (m_server) {
			m_server->close();
			pls_delete(m_server, nullptr);
		}

#if defined(Q_OS_WIN)
		if (SUCCEEDED(hr)) {
			CoUninitialize();
		}
#endif
	}

	static void destroy()
	{
		if (!g_mainproc) {
			return;
		}

		log_info("ipc main process destroy");
		g_mainproc->m_running = false;
		g_mainproc->stopMonitor();
		g_mainproc->destroyAllSubprocs();
		g_mainproc->quitAndWait();
		pls_delete(g_mainproc, nullptr);
	}

	void startListen()
	{
		m_server = pls_new<TcpServer>();
		QObject::connect(m_server, &TcpServer::newSocket, m_server, [this](qintptr handle) { newConnection(handle); });
		m_server->listen(QHostAddress::LocalHost, 0);
		log_info("main process listen %s, port: %u", m_server->isListening() ? "ok" : "failed", (quint32)m_server->serverPort());
	}
	void newConnection(qintptr handle) const
	{
		TcpSocket *socket = pls_new<TcpSocket>(true);
		QObject::connect(socket, &TcpSocket::socketDisconnected, m_server, [](const QString &name) {
			SubprocImplPtr subprocImpl = g_mainproc->findSubproc(name);
			if (!subprocImpl) {
				log_error("main process subprocess not found, name: %s", name.toUtf8().constData());
				return;
			}

			subprocImpl->m_socket = nullptr;
			SubprocImpl::trigger(subprocImpl, Event::SocketDisconnected);
		});
		QObject::connect(socket, &TcpSocket::attrsReq, m_server, [](TcpSocket *s) {
			SubprocImplPtr subprocImpl = g_mainproc->findSubproc(s->name());
			if (!subprocImpl) {
				log_error("main process subprocess not found, name: %s", s->name().toUtf8().constData());
				s->close();
				return;
			}

			subprocImpl->m_socket = s;
			SubprocImpl::trigger(subprocImpl, Event::SocketConnected);
			s->sendAttrsRsp(subprocImpl->m_paramsImpl->m_attrs);
		});
		QObject::connect(socket, &TcpSocket::okReq, m_server, [](TcpSocket *s) {
			SubprocImplPtr subprocImpl = g_mainproc->findSubproc(s->name());
			if (!subprocImpl) {
				log_error("main process subprocess not found, name: %s", s->name().toUtf8().constData());
				s->close();
				return;
			}

			s->sendOkRsp();
			subprocImpl->setStatus(subprocImpl, Status::Running);
		});
		QObject::connect(socket, &TcpSocket::progressReq, m_server, [](TcpSocket *s, int type, int percent, const QString &msg) {
			SubprocImplPtr subprocImpl = g_mainproc->findSubproc(s->name());
			if (!subprocImpl) {
				log_error("main process subprocess not found, name: %s", s->name().toUtf8().constData());
				s->close();
				return;
			}

			SubprocImpl::trigger(subprocImpl, type, percent, msg);
			s->sendProgressRsp();
		});
		QObject::connect(socket, &TcpSocket::exitInfoReq, m_server, [](TcpSocket *s, int exitCode, const QVariantHash &exitAttrs, const QString &exitMsg) {
			SubprocImplPtr subprocImpl = g_mainproc->findSubproc(s->name());
			if (!subprocImpl) {
				log_error("main process subprocess not found, name: %s", s->name().toUtf8().constData());
				s->close();
				return;
			}

			subprocImpl->setStatus(subprocImpl, Status::Stopping);
			s->sendExitInfoRsp();
			subprocImpl->m_exitCode0 = exitCode;
			subprocImpl->m_exitAttrs = exitAttrs;
			subprocImpl->m_exitMsg = exitMsg;
			SubprocImpl::trigger(subprocImpl, exitCode, exitAttrs, exitMsg);
		});
		QObject::connect(socket, &TcpSocket::closeReq, m_server, [](TcpSocket *s) {
			s->sendCloseReceivedRsp();

			SubprocImplPtr subprocImpl = g_mainproc->findSubproc(s->name());
			if (!subprocImpl) {
				log_error("main process subprocess not found, name: %s", s->name().toUtf8().constData());
				s->close();
				return;
			}

			SubprocImpl::trigger(subprocImpl, Event::RequireClose);
		});
		QObject::connect(socket, &TcpSocket::closeReceivedRsp, m_server, [](TcpSocket *s) {
			SubprocImplPtr subprocImpl = g_mainproc->findSubproc(s->name());
			if (!subprocImpl) {
				log_error("main process subprocess not found, name: %s", s->name().toUtf8().constData());
				s->close();
				return;
			}

			subprocImpl->setStatus(subprocImpl, Status::Closing);
			subprocImpl->m_subCloseState = CloseState::Received;
		});
		QObject::connect(socket, &TcpSocket::closeKnownRsp, m_server, [](TcpSocket *s, bool allow) {
			SubprocImplPtr subprocImpl = g_mainproc->findSubproc(s->name());
			if (!subprocImpl) {
				log_error("main process subprocess not found, name: %s", s->name().toUtf8().constData());
				s->close();
				return;
			}

			subprocImpl->setStatus(subprocImpl, allow ? Status::Stopping : Status::Running);
			subprocImpl->m_subCloseState = CloseState::Known;
		});
		QObject::connect(socket, &TcpSocket::custom, m_server, [](TcpSocket *s, int type, const QJsonObject &msg) {
			SubprocImplPtr subprocImpl = g_mainproc->findSubproc(s->name());
			if (!subprocImpl) {
				log_error("main process subprocess not found, name: %s", s->name().toUtf8().constData());
				s->close();
				return;
			}

			SubprocImpl::trigger(subprocImpl, type, msg);
		});
		socket->setSocketDescriptor(handle);
		socket->startKeeplive();
	}

	void startMonitor()
	{
		m_monitor = std::thread([]() {
			log_info("monitor thread start");

			while (g_mainproc && g_mainproc->m_running) {
				g_mainproc->checkSubprocs();
				pls_sleep_ms(10);
			}

			log_info("monitor thread stop");
		});
	}
	void stopMonitor()
	{
		if (m_monitor.joinable()) {
			m_monitor.join();
		}
	}
	void checkSubprocs()
	{
		std::shared_lock lock(m_subprocsMutex);
		for (const auto &subproc : m_subprocs) {
			if (auto sp = subproc.second.lock(); sp) {
				sp->processIsExited(sp);
			}
		}
	}

	void addSubproc(SubprocImplPtr subprocImpl)
	{
		std::unique_lock lock(m_subprocsMutex);
		m_subprocs[subprocImpl->m_paramsImpl->m_name] = subprocImpl;
	}
	void removeSubproc(const QString &name)
	{
		std::unique_lock lock(m_subprocsMutex);
		m_subprocs.erase(name);
	}
	SubprocImplPtr findSubproc(const QString &name) const
	{
		if (auto iter = m_subprocs.find(name); iter != m_subprocs.end()) {
			if (auto impl = iter->second.lock(); impl) {
				return impl;
			}
		}
		return nullptr;
	}

	Subproc createSubproc(const ParamsImplPtr &paramsImpl)
	{
		SubprocImplPtr subprocImpl;
		syncCall([this, &subprocImpl, paramsImpl]() {
			if (subprocImpl = findSubproc(paramsImpl->m_name); !subprocImpl) {
				subprocImpl = {pls_new<SubprocImpl>(paramsImpl), [this](SubprocImpl *impl) { asyncCall([this, impl]() { destroySubproc(impl); }); }};
				addSubproc(subprocImpl);
			}
		});
		return subprocImpl;
	}
	void destroySubproc(SubprocImpl *impl)
	{
		removeSubproc(impl->m_paramsImpl->m_name);
		impl->closeSocket();
		impl->closeProcess();
		pls_delete(impl);
	}

	bool openSubproc(const SubprocImplPtr &subprocImpl) const
	{
		bool result = false;
		syncCall([&result, subprocImpl]() { result = subprocImpl->open(subprocImpl); });
		return result;
	}
	bool closeSubproc(const SubprocImplPtr &subprocImpl) const
	{
		bool result = false;
		syncCall([&result, subprocImpl]() { result = subprocImpl->close(subprocImpl); });
		return result;
	}
	void killSubproc(const SubprocImplPtr &subprocImpl, int exitCode) const
	{
		syncCall([subprocImpl, exitCode]() { subprocImpl->kill(subprocImpl, exitCode); });
	}
	bool waitSubproc(const SubprocImplPtr &subprocImpl, int timeout) const
	{
		if (subprocImpl->isStopped()) {
			return true;
		}

		auto semaphore = std::make_shared<QSemaphore>();
		asyncCall([semaphore, subprocImpl]() {
			if (!subprocImpl->isStopped()) {
				subprocImpl->m_stoppedHooks.emplace_back(([semaphore]() {
					semaphore->release();
					return true;
				}));
			} else {
				semaphore->release();
			}
		});
		return semaphore->tryAcquire(1, timeout);
	}

	void closeAllSubprocs() const
	{
		std::shared_lock lock(m_subprocsMutex);
		for (const auto &subproc : m_subprocs) {
			if (auto subprocImpl = subproc.second.lock(); subprocImpl) {
				closeSubproc(subprocImpl);
			}
		}
	}
	void killAllSubprocs(int exitCode) const
	{
		std::shared_lock lock(m_subprocsMutex);
		for (const auto &subproc : m_subprocs) {
			if (auto subprocImpl = subproc.second.lock(); subprocImpl) {
				killSubproc(subprocImpl, exitCode);
			}
		}
	}
	void waitAllSubprocs(int timeout = -1, bool checkAutoClose = false) const
	{
		if (!timeout) {
			return;
		}

		if (timeout < 0) {
			std::shared_lock lock(m_subprocsMutex);
			for (const auto &subproc : m_subprocs) {
				if (auto subprocImpl = subproc.second.lock(); subprocImpl && (!checkAutoClose || subprocImpl->autoClose())) {
					waitSubproc(subprocImpl, timeout);
				}
			}
			return;
		}

		using namespace std::chrono;
		int rest = timeout;
		std::shared_lock lock(m_subprocsMutex);
		for (const auto &subproc : m_subprocs) {
			if (auto subprocImpl = subproc.second.lock(); subprocImpl && (!checkAutoClose || subprocImpl->autoClose())) {
				auto st = steady_clock::now();
				waitSubproc(subprocImpl, rest);
				if (rest -= (int)duration_cast<milliseconds>(steady_clock::now() - st).count(); rest <= 0) {
					break;
				}
			}
		}
	}

	void destroyAllSubprocs() const
	{
		std::shared_lock lock(m_subprocsMutex);
		for (const auto &subproc : g_mainproc->m_subprocs) {
			if (auto subprocImpl = subproc.second.lock(); subprocImpl && subprocImpl->autoClose()) {
				switch (subprocImpl->closeWay()) {
				case CloseWay::Close:
					closeSubproc(subprocImpl);
					break;
				case CloseWay::Kill:
					killSubproc(subprocImpl, subprocImpl->killExitCode());
					break;
				default:
					break;
				}
			}
		}

		for (const auto &subproc : g_mainproc->m_subprocs) {
			if (auto subprocImpl = subproc.second.lock(); subprocImpl && subprocImpl->autoClose()) {
				waitSubproc(subprocImpl, subprocImpl->waitTimeout());
			}
		}
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
		if (!g_mainproc) {
			g_mainproc = pls_new<Main>();
		}
	}
	void cleanup() const
	{
		if (g_mainproc) {
			Main::destroy();
		}
	}
};
Main *Main::s_main = nullptr;

void asyncCallInMain(const std::function<void()> &cb)
{
	if (g_mainproc) {
		g_mainproc->asyncCall(cb);
	}
}
quint16 getServerPort()
{
	return g_mainprocServerPort;
}

Params::Params() : Params(std::make_shared<ParamsImpl>())
{
	//constructor
}
Params::Params(const ParamsImplPtr &paramsImpl) : m_impl(paramsImpl)
{
	//constructor
}

Params &Params::operator=(const ParamsImplPtr &paramsImpl)
{
	m_impl = paramsImpl;
	return *this;
}

QString Params::name() const
{
	return m_impl->m_name;
}
const Params &Params::name(const QString &name) const
{
	m_impl->m_name = name;
	return *this;
}

QString Params::program() const
{
	return m_impl->m_program;
}
const Params &Params::program(const QString &program) const
{
	m_impl->m_program = program;
	return *this;
}

QStringList Params::args() const
{
	return m_impl->m_args;
}
const Params &Params::args(const QStringList &args) const
{
	m_impl->m_args = args;
	return *this;
}
const Params &Params::arg(const QString &arg) const
{
	m_impl->m_args.append(arg);
	return *this;
}

QVariantHash Params::attrs() const
{
	return m_impl->m_attrs;
}
const Params &Params::attrs(const QVariantHash &attrs) const
{
	m_impl->m_attrs = attrs;
	return *this;
}

QVariant Params::attr(const QString &name) const
{
	return m_impl->m_attrs.value(name);
}
const Params &Params::attr(const QString &name, const QVariant &value) const
{
	m_impl->m_attrs.insert(name, value);
	return *this;
}

bool Params::autoClose() const
{
	return m_impl->m_autoClose;
}
const Params &Params::autoClose(bool autoClose, CloseWay closeWay, int waitTimeout, int killExitCode) const
{
	m_impl->m_autoClose = autoClose;
	m_impl->m_closeWay = closeWay;
	m_impl->m_waitTimeout = waitTimeout;
	m_impl->m_killExitCode = killExitCode;
	return *this;
}

const Params &Params::listen(QObject *receiver, const Listener &listener) const
{
	m_impl->m_receiver = receiver;
	m_impl->m_listener = listener;
	return *this;
}

Subproc::Subproc(const SubprocImplPtr &impl) : m_impl(impl)
{
	//constructor
}
Subproc &Subproc::operator=(const SubprocImplPtr &impl)
{
	m_impl = impl;
	return *this;
}

Subproc::operator bool() const
{
	return m_impl != nullptr;
}
bool Subproc::operator==(const Subproc &subproc) const
{
	if (this == &subproc) {
		return true;
	} else if (m_impl == subproc.m_impl) {
		return true;
	}
	return false;
}

bool Subproc::operator==(const QString &name) const
{
	return m_impl->name() == name;
}
bool Subproc::operator!=(const Subproc &subproc) const
{
	return !operator==(subproc);
}
bool Subproc::operator!=(const QString &name) const
{
	return !operator==(name);
}

Params Subproc::params() const
{
	return m_impl->m_paramsImpl;
}

const Subproc &Subproc::saveDefaultProgram() const
{
	m_impl->m_defaultParamsImpl.m_program = m_impl->m_paramsImpl->m_program;
	return *this;
}
const Subproc &Subproc::resetProgram() const
{
	m_impl->m_paramsImpl->m_program = m_impl->m_defaultParamsImpl.m_program;
	return *this;
}

const Subproc &Subproc::saveDefaultArgs() const
{
	m_impl->m_defaultParamsImpl.m_args = m_impl->m_paramsImpl->m_args;
	return *this;
}
const Subproc &Subproc::resetArgs() const
{
	m_impl->m_paramsImpl->m_args = m_impl->m_defaultParamsImpl.m_args;
	return *this;
}

const Subproc &Subproc::saveDefaultAttrs() const
{
	m_impl->m_defaultParamsImpl.m_attrs = m_impl->m_paramsImpl->m_attrs;
	return *this;
}
const Subproc &Subproc::resetAttrs() const
{
	m_impl->m_paramsImpl->m_attrs = m_impl->m_defaultParamsImpl.m_attrs;
	return *this;
}

Status Subproc::status() const
{
	return m_impl->m_status;
}
QString Subproc::errors() const
{
	return m_impl->m_errors;
}

QVariantHash Subproc::exitAttrs() const
{
	return m_impl->m_exitAttrs;
}
QVariant Subproc::exitAttr(const QString &name) const
{
	return m_impl->m_exitAttrs.value(name);
}
int Subproc::exitCode() const
{
	return m_impl->m_exitCode;
}
QString Subproc::exitMsg() const
{
	return m_impl->m_exitMsg;
}

bool Subproc::allowClose(bool allow) const
{
	return m_impl->allowClose(allow);
}

bool Subproc::open() const
{
	return g_mainproc->openSubproc(m_impl);
}
bool Subproc::close() const
{
	return g_mainproc->closeSubproc(m_impl);
}
void Subproc::kill(int exitCode) const
{
	g_mainproc->killSubproc(m_impl, exitCode);
}
bool Subproc::wait(int timeout) const
{
	return g_mainproc->waitSubproc(m_impl, timeout);
}

bool Subproc::send(int type, const QJsonObject &msg) const
{
	return m_impl->send(type, msg);
}

LIBIPC_API quint16 listenPort()
{
	if (!pls_current_is_main_thread()) {
		return 0;
	}

	Initializer::initializer()->init();

	if (g_mainproc) {
		return g_mainprocServerPort;
	}
	return 0;
}

LIBIPC_API Subproc create(const Params &params)
{
	if (!pls_current_is_main_thread()) {
		return {nullptr};
	}

	Initializer::initializer()->init();

	if (g_mainproc && !params.name().isEmpty()) {
		return g_mainproc->createSubproc(params.m_impl);
	}
	return {nullptr};
}
LIBIPC_API void closeAll()
{
	if (g_mainproc) {
		g_mainproc->closeAllSubprocs();
	}
}
LIBIPC_API void killAll(int exitCode)
{
	if (g_mainproc) {
		g_mainproc->killAllSubprocs(exitCode);
	}
}
LIBIPC_API void waitAll(int timeout)
{
	if (g_mainproc) {
		g_mainproc->waitAllSubprocs(timeout);
	}
}

}

namespace subproc {
#define g_subproc SubprocImpl::s_subproc

struct ParamsImpl {
	std::optional<QString> m_name;
	std::optional<quint16> m_serverPort;
	QObject *m_receiver = nullptr;
	Listener m_listener;
};
struct SubprocImpl : public std::enable_shared_from_this<SubprocImpl> {
	Status m_status{};

	ParamsImplPtr m_paramsImpl;
	std::optional<quint32> m_mainProcessId;
	QStringList m_args;
	QVariantHash m_attrs;

	std::shared_ptr<QEventLoop> m_eventLoop = nullptr;

	http::ExclusiveWorker *m_worker = nullptr;
	TcpSocket *m_client = nullptr;
	std::optional<int> m_exitCode;

	pls_process_t *m_mainProcess = nullptr;
	std::optional<int> m_mainExitCode;
	std::atomic<CloseState> m_mainCloseState = CloseState::None;

	QStringList m_errors;

	static SubprocImplPtr s_subproc;

	SubprocImpl(const ParamsImplPtr &paramsImpl, const QStringList &args) : m_paramsImpl(paramsImpl), m_args(args)
	{
		//"--libipc-subproc-name=name"
		if (!m_paramsImpl->m_name.has_value()) {
			m_paramsImpl->m_name = pls_cmdline_get_arg(m_args, QStringLiteral("--libipc-subproc-name="));
		}

		//"--libipc-mainproc-server-port=port"
		if (!m_paramsImpl->m_serverPort.has_value()) {
			m_paramsImpl->m_serverPort = pls_cmdline_get_uint16_arg(m_args, QStringLiteral("--libipc-mainproc-server-port="));
		}

		//"--libipc-mainproc-process-id=%1"
		m_mainProcessId = pls_cmdline_get_uint32_arg(m_args, QStringLiteral("--libipc-mainproc-process-id="));
	}

	QString name() const { return m_paramsImpl->m_name.value_or(QString()); }
	quint16 serverPort() const { return m_paramsImpl->m_serverPort.value_or(0); }

	bool isSubproc() const { return m_paramsImpl->m_name.has_value() && m_paramsImpl->m_serverPort.has_value() && m_mainProcessId.has_value(); }

	QObject *receiver() const { return m_paramsImpl->m_receiver; }
	const Listener &listener() const { return m_paramsImpl->m_listener; }

	void init()
	{
		if (!isMainThread()) {
			m_errors.append(QStringLiteral("must be called on the main thread"));
			return;
		}

		if (!isSubproc()) {
			m_errors.append(QStringLiteral("no sub process info args"));
			return;
		} else if (!openMainProcess()) {
			return;
		}

		SubprocImplPtr subprocImpl = shared_from_this();
		setStatus(subprocImpl, Status::Starting);
		m_eventLoop = std::make_shared<QEventLoop>();
		m_worker = pls_new<http::ExclusiveWorker>();
		m_worker->asyncCall([subprocImpl]() { createClient(subprocImpl, subprocImpl->m_worker->callProxy()); });
		m_eventLoop->exec();
	}
	void cleanup()
	{
		if (!isMainThread() || !isSubproc()) {
			return;
		}

		auto subprocImpl = shared_from_this();
		setStatus(subprocImpl, Status::Stopping);

		if (m_worker) {
			if (m_client) {
				m_worker->asyncCall([subprocImpl]() {
					if (pls_object_is_valid(subprocImpl->m_client)) {
						subprocImpl->m_client->close();
					}
				});
				m_eventLoop->exec();
			}

			m_worker->quitAndWait();
			pls_delete(m_worker, nullptr);
		}

		m_eventLoop.reset();

		closeMainProcess();

		setStatus(subprocImpl, Status::Stopped);
	}
	bool reopen(qint32 mainProcessId, quint16 serverPort)
	{
		m_errors.clear();

		if (!isMainThread()) {
			m_errors.append(QStringLiteral("reconnect() and init() must be called on the same thread"));
			return false;
		}

		if (!m_worker || !m_eventLoop) {
			m_errors.append(QStringLiteral("inner state error"));
			return false;
		}

		if (!m_mainProcessId.has_value() || m_mainProcessId.value() != mainProcessId) {
			m_mainProcessId = mainProcessId;
			closeMainProcess();
		}

		if (serverPort > 0) {
			m_paramsImpl->m_serverPort = serverPort;
		}

		if (!isSubproc()) {
			m_errors.append(QStringLiteral("no sub process info args"));
			return false;
		} else if (!openMainProcess()) {
			return false;
		}

		SubprocImplPtr subprocImpl = shared_from_this();

		if (m_client) {
			m_worker->asyncCall([subprocImpl]() { subprocImpl->m_client->close(); });
			m_eventLoop->exec();
		}

		m_worker->asyncCall([subprocImpl]() { createClient(subprocImpl, subprocImpl->m_worker->callProxy()); });
		m_eventLoop->exec();
		return true;
	}
	void startCheckMainClosed()
	{
		if (m_status != Status::Stopping) {
			pls_async_call_mt([subprocImpl = shared_from_this()]() { subprocImpl->checkMainClosed(3000); });
		}
	}
	bool openMainProcess()
	{
		if (!m_mainProcessId.has_value()) {
			m_errors.append(QStringLiteral("main process id not found"));
			closeMainProcess();
			return false;
		} else if (m_mainProcess && (m_mainProcessId.value() == (int)pls_process_id(m_mainProcess))) {
			return true;
		}

		closeMainProcess();
		m_mainProcess = pls_process_create(m_mainProcessId.value());
		if (!m_mainProcess) {
			m_errors.append(QStringLiteral("main process not found"));
			return false;
		}
		return true;
	}
	bool checkMainClosed(int timeout)
	{
		if (!m_mainProcess) {
			m_mainCloseState = CloseState::Closed;
			return true;
		} else if (pls_process_wait(m_mainProcess, timeout) <= 0) {
			return false;
		}

		SubprocImplPtr subprocImpl = shared_from_this();

		uint32_t exitCode = pls_process_exit_code(m_mainProcess);
		m_mainCloseState = CloseState::Closed;
		m_mainExitCode = (int)exitCode;
		closeMainProcess();
		trigger(subprocImpl, Event::MainprocExited, {{QCSTR_normal, exitCode == 0}, {QCSTR_exitCode, m_mainExitCode.value()}});
		return true;
	}
	void closeMainProcess()
	{
		if (m_mainProcess) {
			pls_process_destroy(m_mainProcess);
			m_mainProcess = nullptr;
		}
	}
	int calcRestTimeout(const std::chrono::steady_clock::time_point &stp, int timeout) const
	{
		if (timeout <= 0) {
			return timeout;
		} else if (auto used = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - stp).count(); used <= timeout) {
			return timeout - used;
		}
		return 0;
	}
	bool waitMainCloseState(CloseState closeState, const std::chrono::steady_clock::time_point &stp, int timeout) const
	{
		for (timeout = calcRestTimeout(stp, timeout); (timeout != 0) && !checkMainCloseState(closeState); timeout = calcRestTimeout(stp, timeout)) {
			QThread::msleep(10);
		}

		if (checkMainCloseState(closeState)) {
			return true;
		}
		return false;
	}
	bool checkMainCloseState(CloseState closeState) const
	{
		switch (closeState) {
		case CloseState::Received:
			return m_mainCloseState == CloseState::Received || m_mainCloseState == CloseState::Known || m_mainCloseState == CloseState::Closed;
		case CloseState::Known:
			return m_mainCloseState == CloseState::Known || m_mainCloseState == CloseState::Closed;
		case CloseState::Closed:
			return m_mainCloseState == CloseState::Closed;
		default:
			return false;
		}
	}

	static void createClient(const SubprocImplPtr &subprocImpl, const QObject *proxy)
	{
		TcpSocket *socket = pls_new<TcpSocket>(false, subprocImpl->name());

		QObject::connect(socket, &TcpSocket::socketDestroyed, subprocImpl->m_eventLoop.get(), &QEventLoop::quit);

		QObject::connect(socket, &TcpSocket::errorOccurred, proxy, [subprocImpl, socket]() {
			log_error("sub process socket connect failed, name: %s", subprocImpl->name().toUtf8().constData());
			subprocImpl->m_errors.append(socket->errorString());
			socket->deleteLater();
		});
		QObject::connect(socket, &TcpSocket::socketConnected, proxy, [subprocImpl](TcpSocket *s) {
			subprocImpl->m_client = s;
			trigger(subprocImpl, Event::SocketConnected);
			s->sendAttrsReq();
		});
		QObject::connect(socket, &TcpSocket::socketDisconnected, proxy, [subprocImpl]() {
			subprocImpl->m_client = nullptr;
			trigger(subprocImpl, Event::SocketDisconnected);
			if (subprocImpl->m_status != Status::Stopping && subprocImpl->m_status != Status::Stopped && subprocImpl->m_mainCloseState == CloseState::None) {
				subprocImpl->startCheckMainClosed();
			}
		});
		QObject::connect(socket, &TcpSocket::attrsRsp, proxy, [subprocImpl](TcpSocket *s, const QVariantHash &attrs) {
			subprocImpl->m_attrs = attrs;
			s->sendOkReq();
		});
		QObject::connect(socket, &TcpSocket::okRsp, proxy, [subprocImpl](TcpSocket *s) {
			setStatus(subprocImpl, Status::Running);
			s->startKeeplive();
			pls_async_invoke(subprocImpl->m_eventLoop.get(), "quit");
		});
		QObject::connect(socket, &TcpSocket::exitInfoRsp, proxy, [subprocImpl](TcpSocket *s) {
			log_info("%s: sub process close socket", subprocImpl->name().toUtf8().constData());
			s->stopKeeplive();
			s->close();
		});
		QObject::connect(socket, &TcpSocket::closeReq, proxy, [subprocImpl](TcpSocket *s) {
			s->sendCloseReceivedRsp();
			setStatus(subprocImpl, Status::Closing);
			trigger(subprocImpl, Event::RequireClose);
		});
		QObject::connect(socket, &TcpSocket::closeReceivedRsp, proxy, [subprocImpl]() {
			subprocImpl->m_mainCloseState = CloseState::Received; //
		});
		QObject::connect(socket, &TcpSocket::closeKnownRsp, proxy, [subprocImpl]() {
			subprocImpl->m_mainCloseState = CloseState::Known; //
		});
		QObject::connect(socket, &TcpSocket::custom, proxy, [subprocImpl](const TcpSocket *, int type, const QJsonObject &msg) {
			QVariantHash params;
			params[QCSTR_type] = type;
			params[QCSTR_msg] = msg;
			SubprocImpl::trigger(subprocImpl, Event::MsgReceived, params);
		});

		socket->connectToHost(QHostAddress::LocalHost, subprocImpl->serverPort());
	}
	static void setStatus(const SubprocImplPtr &subprocImpl, Status status)
	{
		if (subprocImpl->m_status == status) {
			return;
		}

		Status old = subprocImpl->m_status;
		subprocImpl->m_status = status;
		SubprocImpl::trigger(subprocImpl, status, old);

		switch (status) {
		case pls::ipc::Status::Starting:
			log_info("sub process %s starting", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Status::Running:
			log_info("sub process %s running", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Status::Stopping:
			log_info("sub process %s stopping", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Status::Stopped:
			log_info("sub process %s stopped", subprocImpl->name().toUtf8().constData());
			break;
		default:
			break;
		}
	}
	static void trigger(const SubprocImplPtr &subprocImpl, Event event, const QVariantHash &params = QVariantHash())
	{
		pls_async_call(subprocImpl->receiver(), [subprocImpl, event, params]() { pls_invoke_safe(subprocImpl->listener(), subprocImpl, event, params); });

		switch (event) {
		case pls::ipc::Event::MainprocExited:
			log_info("sub process trigger Event::MainprocExited, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::SocketConnected:
			log_info("sub process trigger Event::SocketConnected, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::SocketDisconnected:
			log_info("sub process trigger Event::SocketDisconnected, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::SubprocExiting:
			log_info("sub process trigger Event::SubprocExiting, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::SubprocExited:
			log_info("sub process trigger Event::SubprocExited, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::SubprocStatusChanged:
			log_info("sub process trigger Event::SubprocStatusChanged, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::SubprocProgressChanged:
			log_info("sub process trigger Event::SubprocProgressChanged, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::RequireClose:
			log_info("sub process trigger Event::RequireClose, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		case pls::ipc::Event::MsgReceived:
			log_info("sub process trigger Event::MsgReceived, name: %s", subprocImpl->name().toUtf8().constData());
			break;
		default:
			break;
		}
	}
	static void trigger(const SubprocImplPtr &subprocImpl, Status status, Status old)
	{
		QVariantHash params;
		params[QCSTR_status] = static_cast<int>(status);
		params[QCSTR_old] = static_cast<int>(old);
		trigger(subprocImpl, Event::SubprocStatusChanged, params);
	}

	bool allowClose(const SubprocImplPtr &subprocImpl, bool allow) const
	{
		if (!allow) {
			setStatus(subprocImpl, Status::Running);
		}

		if (m_client) {
			return m_client->sendCloseKnownRsp(allow);
		}
		return false;
	}
	bool progress(int type, int percent, const QString &msg) const
	{
		if (m_client) {
			m_client->sendProgressReq(type, percent, msg);
			return true;
		}
		return false;
	}
	void exitApp(const SubprocImplPtr &subprocImpl, int exitCode, const QVariantHash &exitAttrs, const QString &exitMsg) const
	{
		log_info("sub process exit, name: %s, exitCode: %d, exitAttrs: %s, exitMsg: %s", subprocImpl->name().toUtf8().constData(), exitCode,
			 QJsonDocument(QJsonObject::fromVariantHash(exitAttrs)).toJson().constData(), exitMsg.toUtf8().constData());

		pls_async_call_mt([subprocImpl, exitCode, exitAttrs, exitMsg]() {
			subprocImpl->m_exitCode = exitCode;
			setStatus(subprocImpl, Status::Stopping);

			if (subprocImpl->m_client) {
				subprocImpl->m_client->sendExitInfoReq(exitCode, exitAttrs, exitMsg);
				subprocImpl->m_eventLoop->exec();
			}

			log_info("sub process exit main loop, name: %s", subprocImpl->name().toUtf8().constData());
			QCoreApplication::exit(exitCode);
		});
	}
	bool send(int type, const QJsonObject &msg) const
	{
		if (m_client) {
			return m_client->sendCustomMsg(type, msg);
		}
		return false;
	}
	bool isMainClosed(int timeout)
	{
		if (!isMainThread()) {
			return false;
		} else if (checkMainClosed(timeout)) {
			return true;
		}
		return false;
	}
	bool closeMain(int timeout, WaitWay waitWay)
	{
		if (!isMainThread()) {
			return false;
		} else if (!m_mainProcessId.has_value()) {
			return false;
		} else if (!m_mainProcess) {
			return true;
		}

		auto stp = std::chrono::steady_clock::now();
		m_mainCloseState = CloseState::Request;

		if (checkMainClosed(0)) {
			return true;
		} else if (!m_client || !m_client->sendCloseReq()) {
			return false;
		}

		switch (waitWay) {
		case WaitWay::Received:
			return waitMainCloseState(CloseState::Received, stp, timeout);
		case WaitWay::Known:
			return waitMainCloseState(CloseState::Known, stp, timeout);
		default:
			break;
		}

		timeout = calcRestTimeout(stp, timeout);
		if (checkMainClosed(timeout)) {
			return true;
		}
		return false;
	}
	bool killMain()
	{
		if (!isMainThread()) {
			return false;
		} else if (m_mainProcess) {
			pls_process_terminate(m_mainProcess, -1);
			return checkMainClosed(-1);
		}
		return false;
	}

	static bool isMainThread() { return QThread::currentThread() == qApp->thread(); }
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
				g_subproc = std::make_shared<SubprocImpl>(params.m_impl, pls_cmdline_args());
				g_subproc->init();
			}
		}
	}
	void cleanup() const
	{
		if (g_subproc) {
			g_subproc->cleanup();
			g_subproc.reset();
		}
	}
};
SubprocImplPtr SubprocImpl::s_subproc;

Params::Params() : Params(std::make_shared<ParamsImpl>())
{
	// constructor
}
Params::Params(const ParamsImplPtr &impl) : m_impl(impl)
{
	// constructor
}
Params &Params::operator=(const ParamsImplPtr &impl)
{
	m_impl = impl;
	return *this;
}

QString Params::name() const
{
	if (m_impl->m_name.has_value()) {
		return m_impl->m_name.value();
	}
	return {};
}
const Params &Params::name(const QString &name) const
{
	m_impl->m_name = name;
	return *this;
}

quint16 Params::serverPort() const
{
	return m_impl->m_serverPort.value_or(0);
}
const Params &Params::serverPort(quint16 serverPort) const
{
	m_impl->m_serverPort = serverPort;
	return *this;
}

const Params &Params::listen(QObject *receiver, const Listener &listener) const
{
	m_impl->m_receiver = receiver;
	m_impl->m_listener = listener;
	return *this;
}

Subproc::Subproc(const SubprocImplPtr &impl) : m_impl(impl)
{
	// constructor
}
Subproc &Subproc::operator=(const SubprocImplPtr &impl)
{
	m_impl = impl;
	return *this;
}

Subproc::operator bool() const
{
	return m_impl->isSubproc() && (status() == Status::Running || status() == Status::Closing);
}

QStringList Subproc::args() const
{
	return m_impl->m_args;
}
QString Subproc::env(const char *name, const QString &defaultValue) const
{
	return qEnvironmentVariable(name, defaultValue);
}
QVariantHash Subproc::attrs() const
{
	return m_impl->m_attrs;
}
QVariant Subproc::attr(const QString &name, const QVariant &defaultValue) const
{
	return m_impl->m_attrs.value(name, defaultValue);
}

Status Subproc::status() const
{
	return m_impl->m_status;
}
QString Subproc::errors() const
{
	return m_impl->m_errors.join(',');
}

bool Subproc::reopen(qint32 mainProcessId, quint16 serverPort) const
{
	return m_impl->reopen(mainProcessId, serverPort);
}
bool Subproc::allowClose(bool allow) const
{
	return m_impl->allowClose(m_impl, allow);
}
bool Subproc::progress(int type, int percent, const QString &msg) const
{
	return m_impl->progress(type, percent, msg);
}
void Subproc::exit(int exitCode, const QVariantHash &exitAttrs, const QString &exitMsg) const
{
	m_impl->exitApp(m_impl, exitCode, exitAttrs, exitMsg);
}
bool Subproc::send(int type, const QJsonObject &msg) const
{
	return m_impl->send(type, msg);
}
bool Subproc::isMainClosed(int timeout) const
{
	return m_impl->isMainClosed(timeout);
}
bool Subproc::closeMain(int timeout, WaitWay waitWay) const
{
	return m_impl->closeMain(timeout, waitWay);
}
bool Subproc::killMain() const
{
	return m_impl->killMain();
}

LIBIPC_API Subproc init(const Params &params)
{
	if (!SubprocImpl::isMainThread()) {
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

#include "libipc.moc"
