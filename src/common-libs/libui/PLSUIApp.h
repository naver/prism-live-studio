#ifndef PLSUIAPP_H
#define PLSUIAPP_H

#include "libui-globals.h"

#include <qapplication.h>
#include <libutils-api.h>

class LIBUI_API PLSUiApp : public pls::Application<QApplication> {
	Q_OBJECT

public:
	PLSUiApp(int &argc, char **argv);
	~PLSUiApp();

public:
	static PLSUiApp *instance();

public:
	enum Icon { //
		CheckedNormal,
		CheckedHover,
		CheckedPressed,
		CheckedDisabled,
		UncheckedNormal,
		UncheckedHover,
		UncheckedPressed,
		UncheckedDisabled,
		IconMax
	};

public:
	void setAppState(bool actived);

signals:
	void appStateChanged(bool actived);

public:
	QPixmap m_checkBoxIcons[IconMax];
	QPixmap m_radioButtonIcons[IconMax];
	QPixmap m_switchButtonIcons[IconMax];
	bool m_appActived = false;
};

#endif // !PLSUIAPP_H
