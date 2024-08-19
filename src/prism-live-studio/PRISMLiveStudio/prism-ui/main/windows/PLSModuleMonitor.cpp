#include "PLSModuleMonitor.h"
#include "libutils-api.h"
#include <Shlwapi.h>
#include <string>
#include <util/platform.h>
#include <QCryptographicHash>
#include <Windows.h>
#include <TlHelp32.h>
#include <locale.h>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include "obs-app.hpp"

#pragma comment(lib, "WinMM.lib")
#pragma comment(lib, "Shlwapi.lib")

PLSModuleMonitor *PLSModuleMonitor::Instance()
{
	static PLSModuleMonitor ins;
	return &ins;
}

void PLSModuleMonitor::StartMonitor()
{
	if (updateTimer) {
		return;
	}

	updateTimer = pls_new<QTimer>();
	updateTimer->setInterval(5000);

	auto func = [this]() {
		QtConcurrent::run([this]() { updateModuleList();});
	};

	connect(updateTimer, &QTimer::timeout, func);
	updateTimer->start();
}

void PLSModuleMonitor::StopMonitor()
{
	if (updateTimer) {
		updateTimer->stop();
		pls_delete(updateTimer);
		updateTimer = nullptr;
	}
}

static void write_file(const std::string &buf, int bufferSize, const std::string &pathStr)
{
#ifdef _WIN32
	int length = MultiByteToWideChar(CP_UTF8, 0, pathStr.c_str(), -1, nullptr, 0);
	if (length > 0) {
		std::vector<wchar_t> ptemp(length);
		MultiByteToWideChar(CP_UTF8, 0, pathStr.c_str(), -1, ptemp.data(), length);
		FILE *stream = nullptr;
		errno_t err = _wfopen_s(&stream, ptemp.data(), L"wb");
		if (err == 0 && stream != nullptr) {
			fwrite(buf.c_str(), 1, bufferSize, stream);
			fclose(stream);
		}
	}
#else
	FILE *stream = fopen(pathStr.c_str(), "wb");
	if (stream) {
		fwrite(buf.c_str(), 1, bufferSize, stream);
		fclose(stream);
	}
#endif
}

void PLSModuleMonitor::updateModuleList()
{
	DWORD pid = GetCurrentProcessId();
	HANDLE modules = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
	if (!modules || modules == INVALID_HANDLE_VALUE)
		return;

	MODULEENTRY32 me32 = {sizeof(MODULEENTRY32)};
	if (!Module32First(modules, &me32)) {
		CloseHandle(modules);
		return;
	}

	QJsonArray modulelist{};
	do {
		char *moduleName = nullptr;
		os_wcs_to_utf8_ptr(me32.szExePath, 0, &moduleName);

		std::array<char, 128> modBaseAddr{};
		snprintf(modBaseAddr.data(), 128, "%p", me32.modBaseAddr);
		std::array<char, 128> modEndAddr{};
		snprintf(modEndAddr.data(), 128, "%p", me32.modBaseAddr + me32.modBaseSize);

		QJsonObject moduleInfo;
		moduleInfo.insert("modBaseAddr", modBaseAddr.data());
		moduleInfo.insert("modEndAddr", modEndAddr.data());
		moduleInfo.insert("SizeOfImage", (int)me32.modBaseSize);
		moduleInfo.insert("ModuleName", moduleName);
		modulelist.append(moduleInfo);

	} while (Module32Next(modules, &me32));

	CloseHandle(modules);

	QString path = pls_get_app_data_dir_pn("") + "crashDump/modules.json";
	if (path.isEmpty())
		return;

	std::string data = pls_read_data(path).toStdString();
	QByteArray dataArray;
	size_t len = 0;

	dataArray.append(data.data(), (int)len);

	QByteArray beforeHash = QCryptographicHash::hash(dataArray, QCryptographicHash::Md5).toHex();

	QJsonObject modulesObj;
	modulesObj.insert("modules", modulelist);
	modulesObj.insert("prismSession", GlobalVars::prismSession.c_str());

	QJsonDocument doc;
	doc.setObject(modulesObj);
	QByteArray doc_data = doc.toJson();

	QByteArray nowHash = QCryptographicHash::hash(doc_data, QCryptographicHash::Md5).toHex();

	if (beforeHash != nowHash)
		write_file(doc_data.constData(), doc_data.size(), path.toStdString());

	return;
}