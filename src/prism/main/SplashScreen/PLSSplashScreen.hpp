#ifndef PLSSPLASHSCREEN_HPP
#define PLSSPLASHSCREEN_HPP

#include <dialog-view.hpp>
#include "PLSDpiHelper.h"
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSSplashScreen;
}
QT_END_NAMESPACE

class PLSSplashScreen : public PLSDialogView {
	Q_OBJECT

public:
	PLSSplashScreen(int pid, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSSplashScreen();

private:
	Ui::PLSSplashScreen *ui;
	QTimer m_timerCheckMain;
};
#endif // PLSSPLASHSCREEN_HPP
