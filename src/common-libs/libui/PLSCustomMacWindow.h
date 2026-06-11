#ifndef PLSCUSTOMMACWINDOW_H
#define PLSCUSTOMMACWINDOW_H

#include <QWidget>
#include <QPointer>
#include <iostream>
#include <objc/runtime.h>

//typedef struct objc_object WindowData;

class PLSCustomMacWindow {
public:
	void initWinId(QWidget *widget);
	~PLSCustomMacWindow() = default;
	void setWidthResizeEnabled(QWidget *widget, bool widthResizeEnabled);
	void setHeightResizeEnabled(QWidget *widget, bool heightResizeEnabled);
	float getTitlebarHeight(QWidget *widget);
	void updateTrafficButtonUI();
	bool hasTitleBar(QWidget *widget);
	void setWindowCornerRadius(QWidget *widget);
	PLSCustomMacWindow &setCornerRadius(bool hasCornerRadius);
	PLSCustomMacWindow &setCloseButtonHidden(bool hidden);
	PLSCustomMacWindow &setMinButtonHidden(bool hidden);
	PLSCustomMacWindow &setMaxButtonHidden(bool hidden);
	void setApplicationHidden(bool hidden);
	bool isApplicationHidden();
	static void removeMacTitleBar(QWidget *widget);
	static void moveToFullScreen(QWidget *widget, int screenIdx);
	static void addCurrentWindowToParentWindow(QWidget *widget);
	static void removeCurrentWindowFromParentWindow(QWidget *widget);

	static void clipsToBounds(QWidget *widget, bool isClips = true);

public:
	bool getCloseButtonHidden() const;
	bool getMinButtonHidden() const;
	bool getMaxButtonHidden() const;
	bool getTitleHidden() const;

private:
	QPointer<QWidget> m_widget;
	bool m_closeButtonHidden{false};
	bool m_minButtonHidden{true};
	bool m_maxButtonHidden{true};
	bool m_hasCornerRadius{false};
};

namespace pls {
namespace mac {
typedef void (*activeCallback)();
void notifyPrismActiveSignal(pls::mac::activeCallback callback);
void sendPrismActiveSignal();
}
}

#endif // PLSCUSTOMMACWINDOW_H
