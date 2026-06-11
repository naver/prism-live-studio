#pragma once
#include <qobject.h>

using PFN_winrt_capture_on_display_changed = void (*)();

class PLSWinRTNotify : public QObject {
	Q_OBJECT
public:
	PLSWinRTNotify(QObject *parent = nullptr);
	~PLSWinRTNotify();

	void onDisplayChanged();
	PFN_winrt_capture_on_display_changed func_on_display_changed = nullptr;

private:
	void *winrt_module = nullptr;
};