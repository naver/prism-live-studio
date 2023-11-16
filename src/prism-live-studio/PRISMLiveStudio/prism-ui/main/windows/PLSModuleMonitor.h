#pragma once
#include <Windows.h>
#include <QObject>
#include <QTimer>
#include <vector>
#include <array>
#include <QThread>

// %APPDATA%\PRISMLiveStudio\crashDump
class PLSModuleMonitor : public QObject {
	Q_OBJECT

	PLSModuleMonitor() = default;
	~PLSModuleMonitor() override = default;

public:
	static PLSModuleMonitor *Instance();

	PLSModuleMonitor(const PLSModuleMonitor &) = delete;
	PLSModuleMonitor &operator=(const PLSModuleMonitor &) = delete;
	PLSModuleMonitor(PLSModuleMonitor &&) = delete;
	PLSModuleMonitor &operator=(PLSModuleMonitor &&) = delete;

	void StartMonitor();
	void StopMonitor();

private slots:
	void updateModuleList();

private:
	QTimer *updateTimer = nullptr;
	QThread *thread = nullptr;
};