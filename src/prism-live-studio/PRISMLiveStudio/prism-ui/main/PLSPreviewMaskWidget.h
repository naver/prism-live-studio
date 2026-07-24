#ifndef PLSPREVIEWMASKWIDGET_H
#define PLSPREVIEWMASKWIDGET_H

#include <QWidget>
#include <QRectF>
#include "qt-display.hpp"

class PLSPreviewMaskWidget : public QWidget {
	Q_OBJECT

public:
	explicit PLSPreviewMaskWidget(OBSQTDisplay *display, QWidget *parent = nullptr);
	explicit PLSPreviewMaskWidget(QWidget *parent = nullptr);
	~PLSPreviewMaskWidget() override = default;

	void setUpdatePreviewRectCallback(std::function<void()> &&callback);
	void setPreviewRect(qreal topLeftX, qreal topLeftY, qreal width, qreal height);
	void setPreviewRect(const QRectF &rect);

	void setBgColor(const QColor &color);
	void setRenderColor(const QColor &color);

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void initUi();

private:
	QRectF m_rect{0, 0, 0, 0};
	QColor m_bgColor{"#151515"};
	QColor m_renderColor{"#000000"};
	bool m_drawBorder{false};
	OBSQTDisplay *m_display{nullptr};
	std::function<void()> m_updatePreviewRectCallback{nullptr};
};
#endif // PLSPREVIEWMASKWIDGET_H
