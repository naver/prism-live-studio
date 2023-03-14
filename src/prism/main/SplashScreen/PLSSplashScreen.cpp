#include <Windows.h>
#include <psapi.h>
#include "PLSSplashScreen.hpp"
#include "ui_PLSSplashScreen.h"
#include "ui-config.h"
#include "pls-common-language.hpp"
#include <QProcess>

PLSSplashScreen::PLSSplashScreen(int pid, PLSDpiHelper dpiHelper) : PLSDialogView(nullptr, dpiHelper), ui(new Ui::PLSSplashScreen)
{
	setWindowIcon(QIcon(":/images/PRISMLiveStudio.ico"));
	setResizeEnabled(false);
	dpiHelper.setCss(this, {PLSCssIndex::PLSSplashScreen});
	dpiHelper.setFixedSize(this, {342, 360});
	ui->setupUi(this->content());
	QString version = QString::asprintf("Current Version %s", PLS_VERSION);
	ui->versionLabel->setText(version);
	connect(this, &PLSSplashScreen::rejected, this, [=]() {
		if (pid <= 0) {
			return;
		}

		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		if (hProcess) {
			QProcess::execute("taskkill", {"/F", "/IM", "PRISMLogger.exe"});
			TerminateProcess(hProcess, -1);
		}
	});

	connect(&m_timerCheckMain, &QTimer::timeout, this, [=] {
		if (pid > 0) {
			BOOL bFind = false;

			DWORD aProcesses[2048], cbNeeded;
			if (EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
				for (auto i = 0; i < cbNeeded / sizeof(DWORD); ++i) {
					if (pid == aProcesses[i]) {
						bFind = true;
						break;
					}
				}
			}

			if (!bFind) {
				m_timerCheckMain.stop();
				accept();
			}
		}
	});
	m_timerCheckMain.start(500);
}

PLSSplashScreen::~PLSSplashScreen()
{
	delete ui;
}
