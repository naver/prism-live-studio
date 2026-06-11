#include "LogHelper.h"

#include <iostream>
#include <vector>
#include <cstdlib>
#include <random>

#include <libutils-api.h>
#include <frontend-api.h>

#include <qdatetime.h>
#include <qtimezone.h>
#include <qdir.h>
#include <chrono>

#include "Utils.h"
#include "Logger.h"
#include "RemoteControlDefine.h"

LogHelper *LogHelper::_instance = nullptr;

LogHelper::LogHelper()
{
	std::default_random_engine generator;
	generator.seed((unsigned int)std::chrono::system_clock::now().time_since_epoch().count());
	std::uniform_int_distribution<int> distribution(1, 10000);
	int random = distribution(generator);
	auto date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
	m_logPath = pls_get_user_path("PRISMLiveStudio/remotecontrol/log");
#if defined(Q_OS_WIN)
	m_logPath = m_logPath.replace("/", "\\");
	m_currentLogFile = QString("%1\\%2_%3.log").arg(m_logPath, date, QString::number(random));
#elif defined(Q_OS_MACOS)
	m_currentLogFile = QString("%1/%2_%3.log").arg(m_logPath, date, QString::number(random));
#endif

	pls_set_remote_control_log_file(m_currentLogFile);

	QFile file(m_currentLogFile);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
		return;
	}
	QTextStream out(&file);
	out << "" << Qt::endl << Qt::flush;
	file.close();
}

LogHelper *LogHelper::getInstance()
{
	if (_instance)
		return _instance;
	_instance = pls_new<LogHelper>();
	return _instance;
}

void LogHelper::printMessageIO(const void *obj, const char *fromFunc, uint32_t length, uint32_t tag, const char *body)
{
#if 0 // only enable it during debugging
	if (length > 0 && body != nullptr && body[0] != 0) {
		std::string cmd = rc::parseCommandString(body);
		RC_LOG_INFO("[RemoteControl io %p] %s length=%u tag=%u cmd=%s json:\n%s", obj, fromFunc, length, tag, cmd.c_str(), body);
	} else {
		RC_LOG_INFO("[RemoteControl io %p] %s length=%u tag=%u ", obj, fromFunc, length, tag);
	}
#endif
}

void LogHelper::writeMessage(const QString &deviceInfo, const QString &message, bool isNelo) const
{
	if (isNelo)
		PLS_LOGEX(PLS_LOG_INFO, FRONTEND_PLUGINS_REMOTE_CONTROL, {{"X-RC-SHORTCUT", message.toStdString().c_str()}}, "%s", message.toStdString().c_str());

	QString name = deviceInfo;

	if (!QFile(m_currentLogFile).exists()) {
		QFileInfo fi(m_currentLogFile);
		auto path = fi.absolutePath();
		if (path.isEmpty() || path.isNull())
			return;
		auto dir = QDir(path);
		if (!dir.exists() && !dir.mkpath(dir.absolutePath()))
			return;
	}

	QFile file(m_currentLogFile);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
		return;

	auto dt = QDateTime::currentDateTime();
	QTextStream out(&file);
	auto m = message;
	out << dt.toString("yyyy-MM-dd_hh:mm:ss.zzz") + Utils::utc_offset_description(dt) << "\n"
	    << name.remove(",").remove("\n") << "\n"
	    << m.remove(",").remove("\n") << "\n"
	    << Qt::endl
	    << Qt::flush;
	file.close();
}
