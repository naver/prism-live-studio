#pragma once
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <vector>
#include "util/config-file.h"
#include "log/module_names.h"

static std::vector<QString> ignore_sections = {"Hotkeys", "General"};
static void LogBasicProfile(const char *filePath, const char *fromFunc)
{
	if (!filePath)
		return;

	// Ensure this log won't be too frequent since
	static qint64 pre_time_sec = 0;
	qint64 current_sec = QDateTime::currentDateTime().toSecsSinceEpoch();
	if (current_sec - pre_time_sec < 5) // keep 5 seconds interval
		return;
	pre_time_sec = current_sec;

	// Start read sections in ini file
	QSettings settings(QString::fromUtf8(filePath), QSettings::IniFormat);
	QStringList sections = settings.childGroups();

	bool empty_ini = true;
	for (const QString &section : sections) { // read each section
		auto itr = std::find(ignore_sections.begin(), ignore_sections.end(), section);
		if (itr != ignore_sections.end())
			continue;

		QString body;
		body.reserve(2048);

		settings.beginGroup(section); // get all keys in this section
		QStringList keys = settings.childKeys();
		for (const QString &key : keys) {
			QString value = settings.value(key).toString();
			QString temp = QString("%1=%2\n").arg(key).arg(value);
			body += temp;
		}
		settings.endGroup();

		if (!body.isEmpty()) {
			empty_ini = false;
			PLS_LOG_KR(PLS_LOG_INFO, MAIN_OUTPUT, "%s profile section [%s]\n%s", fromFunc, section.toStdString().c_str(), body.toStdString().c_str());
		}
	}

	if (empty_ini) {
		// after "new profile", if user didn't change any settings, QSettings will fail to read the ini file
		PLS_LOG_KR(PLS_LOG_INFO, MAIN_OUTPUT, "%s profile section is not changed for output", fromFunc);
	}
}