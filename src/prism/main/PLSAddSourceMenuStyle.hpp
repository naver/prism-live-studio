#pragma once

#include <QProxyStyle>

class PLSAddSourceMenuStyle : public QProxyStyle {
public:
	explicit PLSAddSourceMenuStyle(QStyle *baseStyle = Q_NULLPTR);
	virtual ~PLSAddSourceMenuStyle() {}

	virtual int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const;
};
