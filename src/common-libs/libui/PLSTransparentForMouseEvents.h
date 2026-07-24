#pragma once

#include <functional>
#include <qlabel.h>

#include "libui-globals.h"

namespace pls {
namespace ui {

using MoveExcludeChecker = std::function<bool(QWidget *child)>;
LIBUI_API bool transparentForMouseEvents_nativeEvent(QWidget *widget, const QByteArray &eventType, void *message, qintptr *result, const MoveExcludeChecker &moveExcludeChecker);
LIBUI_API bool transparentForMouseEvents_moveExcludeChild(QWidget *child);

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
	void setMoveExcludeChecker(const pls::ui::MoveExcludeChecker &moveExcludeChecker) { m_moveExcludeChecker = moveExcludeChecker; }

protected:
	bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override
	{
		if (!pls::ui::transparentForMouseEvents_nativeEvent(this, eventType, message, result, m_moveExcludeChecker))
			return QtType::nativeEvent(eventType, message, result);
		return true;
	}

private:
	pls::ui::MoveExcludeChecker m_moveExcludeChecker = nullptr;
};

using PLSTransparentForMouseEventsWidget = PLSTransparentForMouseEvents<QWidget>;
using PLSTransparentForMouseEventsFrame = PLSTransparentForMouseEvents<QFrame>;
using PLSTransparentForMouseEventsLabel = PLSTransparentForMouseEvents<QLabel>;
