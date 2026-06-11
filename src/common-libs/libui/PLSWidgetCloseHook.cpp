
#include "PLSWidgetCloseHook.h"

void PLSWidgetCloseHook::addCanCloseChecker(const CanCloseChecker &canCloseChecker)
{
	m_canCloseCheckers.append(canCloseChecker);
}
bool PLSWidgetCloseHook::canCloseCheck() const
{
	for (const auto &canCloseChecker : m_canCloseCheckers)
		if (!canCloseChecker())
			return false;
	return true;
}

void PLSWidgetCloseHook::addCloseListener(const CloseListener &closeListener)
{
	m_closeListeners.append(closeListener);
}
void PLSWidgetCloseHook::notifyClose() const
{
	for (const auto &closeListener : m_closeListeners)
		closeListener();
}
