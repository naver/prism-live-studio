#pragma once

#include <string>
#include <qstring.h>

struct LogRecord {
	QString TimeDesp;
	QString DeviceName;
	QString Content;
};

class LogHelper {
public:
	static LogHelper *getInstance();
	static void printMessageIO(const void *obj, const char *fromFunc, uint32_t length, uint32_t tag, const char *body);

	LogHelper();
	~LogHelper() = default;

	void writeMessage(const QString &deviceInfo, const QString &message, bool isNelo = true) const;
	QString currentLogFile() const { return m_currentLogFile; }
	QString logFolder() const { return m_logPath; }

	LogHelper(LogHelper const &) = delete;
	void operator=(LogHelper const &) = delete;

private:
	static LogHelper *_instance;
	QString m_currentLogFile;
	QString m_logPath;
};
