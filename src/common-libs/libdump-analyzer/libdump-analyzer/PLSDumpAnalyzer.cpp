#include "PLSDumpAnalyzer.h"
#include "libutils-api.h"
#include "PLSUtil.h"

#include <qstring.h>
#include <qdir.h>
#include <qsettings.h>

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <ShlObj.h>
#endif

#include "PLSAnalysisStack.h"

#if defined(Q_OS_WIN)

static bool find_dump_file(QString &dump_file_path, qint64 &file_size, const QString &dump_folder, const QString &process_name, const QString &process_pid)
{
	QString prefix = QString("%1.%2").arg(process_name, process_pid);

	QDir dir(dump_folder);
	for (const QFileInfo &fi : dir.entryInfoList(QDir::Files, QDir::Time)) {
		if (fi.fileName().startsWith(prefix)) {
			dump_file_path = fi.filePath();
			file_size = fi.size();
			return true;
		}
	}
	return false;
}

static bool wait_for_dump_file_generated(QString &dump_file_path, qint64 &file_size, const QString &dump_folder, const QString &process_name, const QString &process_pid)
{
	using namespace std::chrono;
	for (auto start = steady_clock::now(); duration_cast<seconds>(steady_clock::now() - start) < 60s;) { // wait 60s
		if (find_dump_file(dump_file_path, file_size, dump_folder, process_name, process_pid)) {
			return true;
		}

		Sleep(1000);
	}
	return false;
}

static bool check_disappear_dump(QString &dump_file_path, qint64 &file_size, const QString &process_name, const QString &process_pid)
{
	QSettings settings(QString("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps\\%1").arg(process_name), QSettings::NativeFormat);
	QString dump_folder = settings.value("DumpFolder").toString();
	if (dump_folder.isEmpty())
		return false;

	if (!wait_for_dump_file_generated(dump_file_path, file_size, dump_folder, process_name, process_pid))
		return false;
	return true;
}

static bool check_crash_dump(QString &dump_file_path, qint64 &file_size, const QString &process_name, const QString &process_pid)
{
	auto dump_folder = pls::get_app_data_dir("PRISMLiveStudio\\crashDump");
	if (dump_folder.isEmpty())
		return false;

	if (find_dump_file(dump_file_path, file_size, dump_folder, process_name, process_pid)) {
		return true;
	}
	return false;
}

static bool check_dump_file_win(ProcessInfo &info)
{
	DumpType dump_type = DumpType::DT_NONE;
	QString dump_file_path;
	qint64 file_size = 0;
	if (check_crash_dump(dump_file_path, file_size, QString::fromStdString(info.process_name), QString::fromStdString(info.pid))) {
		dump_type = DumpType::DT_CRASH;
		if (info.found_dumo_func) {
			if (file_size == 0)
				info.found_dumo_func("CrashedWithoutDump");
			else
				info.found_dumo_func("Crashed");
		}
	} else if (check_disappear_dump(dump_file_path, file_size, QString::fromStdString(info.process_name), QString::fromStdString(info.pid))) {
		dump_type = DumpType::DT_DISAPPEAR;
		if (info.found_dumo_func)
			info.found_dumo_func("DisappearWithDump");
	} else {
		if (info.found_dumo_func)
			info.found_dumo_func("DisappearWithoutDump");
		return false;
	}
	info.dump_file = dump_file_path.toLocal8Bit().constData();
	info.dump_type = dump_type;
	return true;
}

#elif defined(Q_OS_MACOS)
// TODO: - mac next

#endif


LIBDUMPANALUZER_API void pls_catch_unhandled_exceptions(const std::string &process_name, const std::string &dump_path)
{
	pls::catch_unhandled_exceptions(process_name, dump_path);
}

LIBDUMPANALUZER_API void pls_catch_unhandled_exceptions_and_send_dump(const ProcessInfo &info)
{
	pls::catch_unhandled_exceptions_and_send_dump(info);
}

LIBDUMPANALUZER_API void pls_set_prism_user_id(const std::string &user_id)
{
	pls::set_prism_user_id(user_id);
}

LIBDUMPANALUZER_API void pls_set_prism_session(const std::string &prism_session)
{
	pls::set_prism_session(prism_session);
}

LIBDUMPANALUZER_API void pls_set_prism_video_adapter(const std::string &video_adapter)
{
	pls::set_prism_video_adapter(video_adapter);
}

LIBDUMPANALUZER_API void pls_set_setup_session(const std::string &session)
{
	pls::set_setup_session(session);
}

LIBDUMPANALUZER_API void pls_set_prism_sub_session(const std::string &session)
{
	pls::set_prism_sub_session(session);
}


LIBDUMPANALUZER_API bool pls_wait_send_dump(ProcessInfo info)
{
#if _WIN32
	bool ret = check_dump_file_win(info);
	return ret ? pls::analysis_stack_and_send_dump(info) : false;
#else
	return pls::analysis_stack_and_send_dump(info);
#endif
}

#if defined(Q_OS_WIN)
LIBDUMPANALUZER_API bool pls_send_block_dump(ProcessInfo info)
{
	return pls::analysis_stack_and_send_dump(info);
}

LIBDUMPANALUZER_API std::vector<SoftInfo> pls_installed_software()
{
	return PLSSoftStatistic::GetInstalledSoftware();
}

#elif defined(Q_OS_MACOS)

// TODO: - mac next

#endif
