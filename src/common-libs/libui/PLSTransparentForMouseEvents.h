#pragma once

#include <functional>
#include <qlabel.h>

#include "libui-globals.h"

namespace pls {
namespace ui {
enum class CheckResult {
	Unknown,
	Include,
	Exclude,
};

using CustomChecker = std::function<CheckResult(QWidget *parentWidget, QWidget *childWidget)>;

LIBUI_API bool transparentForMouseEvents_nativeEvent(QWidget *widget, const QByteArray &eventType, void *message, qintptr *result, const CustomChecker &customChecker);
LIBUI_API bool transparentForMouseEvents_moveInContentIncludeChild(QWidget *parentWidget, QWidget *childWidget);
LIBUI_API bool transparentForMouseEvents_moveInContentExcludeChild(QWidget *parentWidget, QWidget *childWidget);

}
}

template<typename QtType> class PLSTransparentForMouseEvents : public QtType {
public:
	template<typename... Args> explicit PLSTransparentForMouseEvents(Args &&...args) : QtType(std::forward<Args>(args)...)
	{
#if defined(Q_OS_WIN)
		QtType::setAttribute(Qt::WA_NativeWindow, true);
#endif
	}
	~PLSTransparentForMouseEvents() = default;

public:
	void setCustomChecker(const pls::ui::CustomChecker &customChecker) { m_customChecker = customChecker; }

protected:
	bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override
	{
		if (!pls::ui::transparentForMouseEvents_nativeEvent(this, eventType, message, result, m_customChecker)) {
			return QtType::nativeEvent(eventType, message, result);
		}
		return true;
	}

private:
	pls::ui::CustomChecker m_customChecker = nullptr;
};

using PLSTransparentForMouseEventsWidget = PLSTransparentForMouseEvents<QWidget>;
using PLSTransparentForMouseEventsFrame = PLSTransparentForMouseEvents<QFrame>;
using PLSTransparentForMouseEventsLabel = PLSTransparentForMouseEvents<QLabel>;
