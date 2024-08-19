#include <qglobal.h>
#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

#include "PLSPressRestoreButton.hpp"
#include <QDebug>

bool isButtonStateNeedUpdate()
{
#if defined(Q_OS_WIN)
	return (GetAsyncKeyState(VK_LBUTTON) >= 0);
#else
	return false;
#endif
}
