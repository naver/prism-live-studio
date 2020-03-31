#include "PLSAddSourceMenuStyle.hpp"

#define ADD_SOURCE_MENU_ICON_SIZE 30

PLSAddSourceMenuStyle::PLSAddSourceMenuStyle(QStyle *baseStyle) : QProxyStyle(baseStyle) {}

int PLSAddSourceMenuStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
	if (metric == QStyle::PM_SmallIconSize)
		return ADD_SOURCE_MENU_ICON_SIZE;
	else
		return QCommonStyle::pixelMetric(metric, option, widget);
}
