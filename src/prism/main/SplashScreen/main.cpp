#include <Windows.h>
#include <qapplication.h>
#include <qpluginloader.h>

#include "PLSSplashScreen.hpp"

static bool loadTheme(QApplication *a, QPluginLoader *themeLoader);

void closeHandle(HANDLE hEvent)
{
	if (hEvent) {
		CloseHandle(hEvent);
	}
}

int main(int argc, char *argv[])
{
	const wchar_t *eventName = L"PRISSplashScreen";

	HANDLE hEvent = OpenEventW(EVENT_ALL_ACCESS, false, eventName);
	bool already_running = !!hEvent;

	if (!already_running) {
		hEvent = CreateEventW(NULL, TRUE, FALSE, eventName);
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			closeHandle(hEvent);
			return -1;
		}
	}

	if (already_running) {
		closeHandle(hEvent);
		return -1;
	}

	auto hEventMain = OpenEvent(EVENT_ALL_ACCESS, false, TEXT("PRISMLiveStudio"));
	if (NULL != hEventMain) {
		CloseHandle(hEventMain);
		return -1;
	}

	// argv[1] prism pid
	QApplication a(argc, argv);

	QString pid = (argc > 1) ? argv[1] : "0";

	QPluginLoader themeLoader;
	loadTheme(&a, &themeLoader);

	PLSSplashScreen w(pid.toInt());
	w.show();

	return a.exec();
}

static bool loadTheme(QApplication *a, QPluginLoader *themeLoader)
{
	QString themePath = QApplication::applicationDirPath() + "/themes/" + "dark-theme.dll";
	themeLoader->setFileName(themePath);
	if (!themeLoader->load()) {
		return false;
	}

	QString mpath = QString("file:///") + QApplication::applicationDirPath() + "/data/prism-studio/themes/Dark.qss";
	a->setStyleSheet(mpath);
	return true;
}
