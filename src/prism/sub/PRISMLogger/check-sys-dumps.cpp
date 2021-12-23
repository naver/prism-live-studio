#include <Windows.h>
#include <qstring.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <chrono>
#include <qsettings.h>
#include <thread>

#include "send-dump.hpp"

typedef void (*found_dump_callback)(bool found);

static bool find_dump_file(QString &dump_file_path, const QString &dump_folder, const QString &process_name, const QString &process_pid)
{
	QString prefix = QString("%1.%2").arg(process_name, process_pid);

	QDir dir(dump_folder);
	for (const QFileInfo &fi : dir.entryInfoList(QDir::Files, QDir::Time)) {
		if (fi.fileName().startsWith(prefix)) {
			dump_file_path = fi.filePath();
			return true;
		}
	}
	return false;
}

static bool wait_for_dump_file_generated(QString &dump_file_path, const QString &dump_folder, const QString &process_name, const QString &process_pid)
{
	using namespace std::chrono;
	for (auto start = steady_clock::now(); duration_cast<seconds>(steady_clock::now() - start) < 60s;) { // wait 60s
		if (find_dump_file(dump_file_path, dump_folder, process_name, process_pid)) {
			return true;
		}

		Sleep(1000);
	}
	return false;
}

static bool send_dump(const QString &process_name, const QString &process_pid, const QString &src, const char *user_id, const char *prism_version, const char *prism_session, const char *project_name,
		      const char *cpu_name, const char *video_adapter_name, DumpType dump_type, found_dump_callback callback = nullptr)
{
	QSettings settings(QString("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps\\%1").arg(process_name), QSettings::NativeFormat);
	QString dump_folder = settings.value("DumpFolder").toString();
	if (dump_folder.isEmpty()) {
		if (callback)
			callback(false);
		return false;
	}

	QString dump_file_path;
	if (!wait_for_dump_file_generated(dump_file_path, dump_folder, process_name, process_pid)) {
		if (callback)
			callback(false);
		return false;
	} else if (send_to_nelo(dump_file_path.toLocal8Bit().constData(), user_id, prism_version, prism_session, project_name, cpu_name, video_adapter_name, dump_type,
				process_pid.toUtf8().constData(), src.toUtf8().constData())) {
		QFile::remove(dump_file_path);
	}
	if (callback)
		callback(true);
	return true;
}

bool wait_send_prism_dump(const std::string &prism_pid, const std::string &user_id, const std::string &prism_version, const std::string &prism_session, const std::string &project_name,
			  const std::string &cpu_name, const std::string &video_adapter_name)
{
	return send_dump(QString::fromUtf8("PRISMLiveStudio.exe"), QString::fromStdString(prism_pid), QString(), user_id.c_str(), prism_version.c_str(), prism_session.c_str(), project_name.c_str(),
			 cpu_name.c_str(), video_adapter_name.c_str(), DumpType::DT_CRASH_MAIN);
}

void send_subprocess_dump(const std::string &subprocess_name, const std::string &subprocess_pid, const std::string &subprocess_src, const std::string &user_id, const std::string &prism_version,
			  const std::string &prism_session, const std::string &project_name, const std::string &cpu_name, std::string &video_adapter_name, found_dump_callback callback)
{
	std::thread send_thread([=]() {
		send_dump(QString::fromStdString(subprocess_name), QString::fromStdString(subprocess_pid), QString::fromStdString(subprocess_src), user_id.c_str(), prism_version.c_str(),
			  prism_session.c_str(), project_name.c_str(), cpu_name.c_str(), video_adapter_name.c_str(), DumpType::DT_CRASH_SUB, callback);
	});
	send_thread.detach();
}
