#include "PLSRegionCapture.h"
#include "libutils-api.h"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "libui.h"
#include <QLocalSocket>
#include <QTimer>
#include <QThread>
#include <QCoreApplication>
#include <QPointer>
#include <memory>

#if defined(_WIN32)
#include <Windows.h>

static std::shared_ptr<int> s_regionCaptureServiceJobHolder;

static void releaseRegionCaptureServiceJob()
{
	s_regionCaptureServiceJobHolder.reset();
}

static bool assignRegionCaptureServiceProcessToJob(QProcess *proc)
{
	if (!proc)
		return false;
	const qint64 qpid = proc->processId();
	if (qpid <= 0)
		return false;
	const DWORD pid = static_cast<DWORD>(qpid);

	releaseRegionCaptureServiceJob();

	HANDLE job = CreateJobObjectW(nullptr, nullptr);
	if (!job) {
		PLS_WARN(MAIN_AREA_CAPTURE, "region-capture CreateJobObject failed, last error: %u", GetLastError());
		return false;
	}

	HANDLE hProcess = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, pid);
	if (!hProcess) {
		PLS_WARN(MAIN_AREA_CAPTURE, "region-capture OpenProcess failed, pid:%lu last error:%u", static_cast<unsigned long>(pid), GetLastError());
		CloseHandle(job);
		return false;
	}

	if (!AssignProcessToJobObject(job, hProcess)) {
		PLS_WARN(MAIN_AREA_CAPTURE, "region-capture AssignProcessToJobObject failed, last error:%u", GetLastError());
		CloseHandle(hProcess);
		CloseHandle(job);
		return false;
	}
	CloseHandle(hProcess);

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info = {};
	limit_info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limit_info, sizeof(limit_info))) {
		PLS_WARN(MAIN_AREA_CAPTURE, "region-capture SetInformationJobObject failed, last error:%u", GetLastError());
		CloseHandle(job);
		return false;
	}

	s_regionCaptureServiceJobHolder = std::shared_ptr<int>(nullptr, [job](int *) {
		if (job)
			CloseHandle(job);
	});
	PLS_INFO(MAIN_AREA_CAPTURE, "region-capture service in job (KILL_ON_JOB_CLOSE), pid:%lu", static_cast<unsigned long>(pid));
	return true;
}

static void wireRegionCaptureServiceJob(QProcess *proc)
{
	if (!proc)
		return;
	QObject::connect(proc, &QProcess::started, proc,
			 [proc]() {
				 assignRegionCaptureServiceProcessToJob(proc);
			 },
			 Qt::SingleShotConnection);
}
#endif

#if defined(_WIN32) && defined(PLS_UI_ACTION_STATS)
#include "region-capture/win-signal.h"
namespace {
void startShowRegionPollTimer(QObject *parent)
{
	static std::shared_ptr<CSignalEvent> signalEvent = std::make_shared<CSignalEvent>(SELECT_REGION_SHOW_SIGNAL, true);
	signalEvent->set_sign(false);
	QTimer *timer = new QTimer(parent);
	QObject::connect(timer, &QTimer::timeout, parent, [timer]() mutable {
		if (signalEvent->get_sign()) {
			PLS_UI_ACTION("show select region");
			timer->stop();
		}
	});
	timer->start(10);
}
}
#endif

QProcess *PLSRegionCapture::s_serviceProcess = nullptr;
QString PLSRegionCapture::s_pipeName;
bool PLSRegionCapture::s_shutdownConnected = false;
bool PLSRegionCapture::s_captureInProgress = false;
bool PLSRegionCapture::s_serviceReady = false;
QThread *PLSRegionCapture::s_watchdogThread = nullptr;
std::atomic<bool> PLSRegionCapture::s_watchdogStop{false};

static constexpr int kRegionCaptureWatchdogIntervalMs = 10000;
/** Join watchdog thread on shutdown (same order as PLSChannelDataAPI::exitApi: exit + bounded wait). */
static constexpr unsigned long kRegionCaptureWatchdogJoinMs = 10000;
/** Max blocking wait on main/UI thread for sockets and QProcess (ms). */
static constexpr int kRegionCaptureUiWaitMs = 100;

void PLSRegionCapture::onServiceProcessStdout()
{
	if (!s_serviceProcess)
		return;
	const QByteArray out = s_serviceProcess->readAllStandardOutput();
	if (!s_serviceReady && out.contains(QByteArrayLiteral("READY")))
		s_serviceReady = true;
}

void PLSRegionCapture::tearDownServiceProcess()
{
	s_serviceReady = false;
#if defined(_WIN32)
	releaseRegionCaptureServiceJob();
#endif
	if (s_serviceProcess) {
		if (s_serviceProcess->state() != QProcess::NotRunning) {
			s_serviceProcess->terminate();
			s_serviceProcess->waitForFinished(kRegionCaptureUiWaitMs);
			if (s_serviceProcess->state() != QProcess::NotRunning)
				s_serviceProcess->kill();
		}
		s_serviceProcess->deleteLater();
		s_serviceProcess = nullptr;
	}
}

bool PLSRegionCapture::startRegionCaptureServiceSync()
{
	s_serviceReady = false;
	tearDownServiceProcess();

	s_pipeName = QString("prism-region-capture-%1").arg(QCoreApplication::applicationPid());

	const int maxRetries = 2;
	for (int attempt = 0; attempt < maxRetries; ++attempt) {
		if (attempt > 0) {
			PLS_INFO(MAIN_AREA_CAPTURE, "Retrying region-capture service start (attempt %d)", attempt + 1);
			QThread::msleep(kRegionCaptureUiWaitMs);
		}

		s_serviceProcess = pls_new<QProcess>(qApp);
#if defined(_WIN32)
		wireRegionCaptureServiceJob(s_serviceProcess);
#endif
		const QPointer<QProcess> self(s_serviceProcess);
		QObject::connect(s_serviceProcess, &QProcess::errorOccurred, s_serviceProcess, [self](QProcess::ProcessError) {
			if (!self || self != s_serviceProcess)
				return;
			s_serviceReady = false;
		});
		QObject::connect(s_serviceProcess, &QProcess::finished, s_serviceProcess, [self](int, QProcess::ExitStatus) {
			if (!self || self != s_serviceProcess)
				return;
			s_serviceReady = false;
		});

		s_serviceProcess->start("region-capture.exe", {QString("--pipe=%1").arg(s_pipeName)});

		if (!s_serviceProcess->waitForStarted(kRegionCaptureUiWaitMs)) {
			PLS_WARN(MAIN_AREA_CAPTURE, "Failed to start region-capture service");
			s_serviceProcess->deleteLater();
			s_serviceProcess = nullptr;
			continue;
		}

		if (!s_serviceProcess->waitForReadyRead(kRegionCaptureUiWaitMs)) {
			PLS_WARN(MAIN_AREA_CAPTURE, "region-capture service did not become ready");
			s_serviceProcess->terminate();
			s_serviceProcess->waitForFinished(kRegionCaptureUiWaitMs);
			s_serviceProcess->deleteLater();
			s_serviceProcess = nullptr;
			continue;
		}

		QString output = s_serviceProcess->readAllStandardOutput();
		if (!output.trimmed().startsWith("READY")) {
			PLS_WARN(MAIN_AREA_CAPTURE, "Unexpected output from region-capture service: %s", output.toUtf8().constData());
			s_serviceProcess->terminate();
			s_serviceProcess->waitForFinished(kRegionCaptureUiWaitMs);
			s_serviceProcess->deleteLater();
			s_serviceProcess = nullptr;
			continue;
		}
		s_serviceReady = true;
		QObject::connect(s_serviceProcess, &QProcess::readyReadStandardOutput, s_serviceProcess, [self]() {
			if (!self || self != s_serviceProcess)
				return;
			PLSRegionCapture::onServiceProcessStdout();
		});
		break;
	}

	if (!s_serviceProcess || s_serviceProcess->state() != QProcess::Running || !s_serviceReady) {
#if defined(_WIN32)
		releaseRegionCaptureServiceJob();
#endif
		return false;
	}

	if (!s_shutdownConnected) {
		s_shutdownConnected = true;
		QObject::connect(qApp, &QCoreApplication::aboutToQuit, []() { PLSRegionCapture::ShutdownService(); });
	}

	PLS_INFO(MAIN_AREA_CAPTURE, "region-capture service started on pipe: %s", s_pipeName.toUtf8().constData());
	return true;
}

void PLSRegionCapture::serviceWatchdogTick()
{
	if (s_watchdogStop.load(std::memory_order_acquire))
		return;
	if (s_captureInProgress)
		return;

	const bool running = s_serviceProcess && s_serviceProcess->state() == QProcess::Running;
	if (!running || !s_serviceReady) {
		PLS_INFO(MAIN_AREA_CAPTURE, "region-capture watchdog: service not healthy, restarting");
		startRegionCaptureServiceSync();
		return;
	}

	QLocalSocket probe;
	probe.connectToServer(s_pipeName);
	if (!probe.waitForConnected(kRegionCaptureUiWaitMs)) {
		PLS_WARN(MAIN_AREA_CAPTURE, "region-capture watchdog: pipe probe failed, restarting");
		tearDownServiceProcess();
		startRegionCaptureServiceSync();
	} else {
		probe.disconnectFromServer();
	}
}

void PLSRegionCapture::ensureWatchdogRunning()
{
	if (s_watchdogThread)
		return;

	s_watchdogStop = false;
	s_watchdogThread = QThread::create([]() {
		while (!s_watchdogStop.load(std::memory_order_acquire)) {
			const int stepMs = 100;
			int waited = 0;
			while (waited < kRegionCaptureWatchdogIntervalMs && !s_watchdogStop.load(std::memory_order_acquire)) {
				QThread::msleep(stepMs);
				waited += stepMs;
			}
			if (s_watchdogStop.load(std::memory_order_acquire))
				break;
			QTimer::singleShot(0, qApp, []() { PLSRegionCapture::serviceWatchdogTick(); });
		}
	});
	s_watchdogThread->start();
}

bool PLSRegionCapture::isServiceReadyForCapture()
{
	ensureWatchdogRunning();
	return s_serviceProcess && s_serviceProcess->state() == QProcess::Running && s_serviceReady;
}

bool PLSRegionCapture::tryAcquireCaptureSlot()
{
	if (s_captureInProgress)
		return false;
	s_captureInProgress = true;
	m_holdCaptureToken = true;
	return true;
}

void PLSRegionCapture::releaseCaptureIfHolding()
{
	if (!m_holdCaptureToken)
		return;
	m_holdCaptureToken = false;
	s_captureInProgress = false;
}

PLSRegionCapture::PLSRegionCapture(QObject *parent) : QObject(parent)
{
#if defined(_WIN32) && defined(PLS_UI_ACTION_STATS)
	PLS_UI_ACTION("request select region");
#endif
}

PLSRegionCapture::~PLSRegionCapture()
{
	releaseCaptureIfHolding();
	if (m_socket) {
		QObject::disconnect(m_socket, nullptr, this, nullptr);
		m_socket->disconnectFromServer();
		m_socket->deleteLater();
		m_socket = nullptr;
	}
}

QRect PLSRegionCapture::GetSelectedRect() const
{
	return m_rectSelected;
}

void PLSRegionCapture::PrewarmService()
{
	ensureWatchdogRunning();

	if (s_serviceProcess)
		return;

	s_pipeName = QString("prism-region-capture-%1").arg(QCoreApplication::applicationPid());
	s_serviceReady = false;

	s_serviceProcess = pls_new<QProcess>(qApp);
#if defined(_WIN32)
	wireRegionCaptureServiceJob(s_serviceProcess);
#endif
	const QPointer<QProcess> self(s_serviceProcess);
	QObject::connect(s_serviceProcess, &QProcess::readyReadStandardOutput, s_serviceProcess, [self]() {
		if (!self || self != s_serviceProcess)
			return;
		PLSRegionCapture::onServiceProcessStdout();
	});
	QObject::connect(s_serviceProcess, &QProcess::errorOccurred, s_serviceProcess, [self](QProcess::ProcessError) {
		if (!self || self != s_serviceProcess)
			return;
		s_serviceReady = false;
	});
	QObject::connect(s_serviceProcess, &QProcess::finished, s_serviceProcess, [self](int, QProcess::ExitStatus) {
		if (!self || self != s_serviceProcess)
			return;
		s_serviceReady = false;
	});

	s_serviceProcess->start("region-capture.exe", {QString("--pipe=%1").arg(s_pipeName)});

	if (!s_shutdownConnected) {
		s_shutdownConnected = true;
		QObject::connect(qApp, &QCoreApplication::aboutToQuit, []() { PLSRegionCapture::ShutdownService(); });
	}

	PLS_INFO(MAIN_AREA_CAPTURE, "region-capture service prewarming on pipe: %s", s_pipeName.toUtf8().constData());
}

void PLSRegionCapture::ShutdownService()
{
	s_watchdogStop.store(true, std::memory_order_release);
	if (s_watchdogThread) {
		if (s_watchdogThread->isRunning()) {
			s_watchdogThread->exit(0);
			s_watchdogThread->wait(kRegionCaptureWatchdogJoinMs);
		}
		if (!s_watchdogThread->isRunning()) {
			delete s_watchdogThread;
		} else {
			PLS_WARN(MAIN_AREA_CAPTURE,
				 "region-capture watchdog thread still running after %lu ms; not deleting QThread (undefined behavior if destroyed while running)",
				 kRegionCaptureWatchdogJoinMs);
		}
		s_watchdogThread = nullptr;
	}

	if (!s_serviceProcess)
		return;

	QLocalSocket socket;
	socket.connectToServer(s_pipeName);
	if (socket.waitForConnected(kRegionCaptureUiWaitMs)) {
		socket.write("EXIT\n");
		socket.flush();
		socket.disconnectFromServer();
	}

	if (s_serviceProcess->state() == QProcess::Running)
		s_serviceProcess->kill();

#if defined(_WIN32)
	releaseRegionCaptureServiceJob();
#endif
	s_serviceProcess->deleteLater();
	s_serviceProcess = nullptr;
}

void PLSRegionCapture::StartCapture(uint64_t maxRegionWidth, uint64_t maxRegionHeight)
{
	if (!tryAcquireCaptureSlot()) {
		PLS_WARN(MAIN_AREA_CAPTURE, "Region capture already in progress, ignoring request");
		emit selectedRegion(QRect());
		return;
	}

	m_rectSelected = QRect();
	m_finished = false;

	if (!isServiceReadyForCapture()) {
		PLS_WARN(MAIN_AREA_CAPTURE, "region-capture service not ready, falling back to legacy mode");
		startLegacy(maxRegionWidth, maxRegionHeight);
		return;
	}

	m_socket = pls_new<QLocalSocket>(this);

	QObject::connect(m_socket, &QLocalSocket::readyRead, this, &PLSRegionCapture::onServiceResponse);
	QObject::connect(m_socket, &QLocalSocket::disconnected, this, [this]() {
		if (!m_finished) {
			m_finished = true;
			releaseCaptureIfHolding();
			emit selectedRegion(m_rectSelected);
		}
	});
	QObject::connect(m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError error) {
		PLS_WARN(MAIN_AREA_CAPTURE, "Region capture socket error: %d", static_cast<int>(error));
		if (!m_finished) {
			m_finished = true;
			releaseCaptureIfHolding();
			emit selectedRegion(m_rectSelected);
		}
	});

	m_socket->connectToServer(s_pipeName);
	if (!m_socket->waitForConnected(kRegionCaptureUiWaitMs)) {
		PLS_WARN(MAIN_AREA_CAPTURE, "Failed to connect to region-capture service, falling back to legacy mode");
		m_socket->deleteLater();
		m_socket = nullptr;
		s_serviceReady = false;
		startLegacy(maxRegionWidth, maxRegionHeight);
		return;
	}

	QByteArray cmd = "START|" + QByteArray::number(maxRegionWidth) + "|" + QByteArray::number(maxRegionHeight) + "\n";
	m_socket->write(cmd);
	m_socket->flush();

#if defined(_WIN32) && defined(PLS_UI_ACTION_STATS)
	startShowRegionPollTimer(this);
#endif
}

void PLSRegionCapture::onServiceResponse()
{
	if (!m_socket)
		return;

	m_readBuffer.append(m_socket->readAll());
	int idx = m_readBuffer.indexOf('\n');
	if (idx < 0)
		return;

	QByteArray line = m_readBuffer.left(idx).trimmed();
	m_readBuffer.remove(0, idx + 1);

	handleResponse(line);
}

void PLSRegionCapture::handleResponse(const QByteArray &line)
{
	if (m_finished)
		return;
	m_finished = true;

	PLS_INFO(MAIN_AREA_CAPTURE, "region capture service response: %s", line.constData());

	auto parts = line.split('|');
	if (parts.size() == 4) {
		m_rectSelected.setX(parts.at(0).toInt());
		m_rectSelected.setY(parts.at(1).toInt());
		m_rectSelected.setWidth(parts.at(2).toInt());
		m_rectSelected.setHeight(parts.at(3).toInt());
	}

	if (m_socket) {
		m_socket->disconnectFromServer();
	}

	releaseCaptureIfHolding();
	emit selectedRegion(m_rectSelected);
}

void PLSRegionCapture::startLegacy(uint64_t maxRegionWidth, uint64_t maxRegionHeight)
{
	QProcess *process = pls_new<QProcess>(this);
	auto legacyHandled = std::make_shared<bool>(false);

	auto onLegacyFailedToStart = [this, process, legacyHandled](const char *reason) {
		if (*legacyHandled)
			return;
		*legacyHandled = true;
		PLS_WARN(MAIN_AREA_CAPTURE, "%s", reason);
		releaseCaptureIfHolding();
		emit selectedRegion(QRect());
		process->deleteLater();
	};

	QObject::connect(process, &QProcess::finished, this, [this, process, legacyHandled]() {
		if (*legacyHandled)
			return;
		*legacyHandled = true;
		QString param = process->readAllStandardOutput();
		PLS_INFO(MAIN_AREA_CAPTURE, "region capture process output (legacy): %s", param.toUtf8().constData());

		auto list = param.split("|");
		if (list.size() == 4) {
			m_rectSelected.setX(list.at(0).toInt());
			m_rectSelected.setY(list.at(1).toInt());
			m_rectSelected.setWidth(list.at(2).toInt());
			m_rectSelected.setHeight(list.at(3).toInt());
		}
		releaseCaptureIfHolding();
		emit selectedRegion(m_rectSelected);
		process->deleteLater();
	});

	QObject::connect(process, &QProcess::errorOccurred, this, [onLegacyFailedToStart](QProcess::ProcessError error) {
		if (error == QProcess::FailedToStart)
			onLegacyFailedToStart("region capture process failed to start (legacy, error signal)");
	});

	QStringList params;
	params << QString("--max-width=%1").arg(maxRegionWidth) << QString("--max-height=%1").arg(maxRegionHeight);
	process->start("region-capture.exe", params);

	if (process->state() == QProcess::NotRunning && process->error() == QProcess::FailedToStart)
		onLegacyFailedToStart("region capture process failed to start (legacy, sync check)");

#if defined(_WIN32) && defined(PLS_UI_ACTION_STATS)
	startShowRegionPollTimer(this);
#endif
}
