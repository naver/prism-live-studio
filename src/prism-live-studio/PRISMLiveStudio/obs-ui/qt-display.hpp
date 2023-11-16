#pragma once

#include <QWidget>
#include <QLabel>
#include <obs.hpp>

#define GREY_COLOR_BACKGROUND 0xFF4C4C4C

class OBSQTDisplay : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QColor displayBackgroundColor MEMBER backgroundColor READ
			   GetDisplayBackgroundColor WRITE
				   SetDisplayBackgroundColor)

	OBSDisplay display;
	QLabel *displayText;
	QMetaObject::Connection screenConnection;

	virtual void paintEvent(QPaintEvent *event) override;
	virtual void moveEvent(QMoveEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	virtual bool nativeEvent(const QByteArray &eventType, void *message,
				 qintptr *result) override;
#else
	virtual bool nativeEvent(const QByteArray &eventType, void *message,
				 long *result) override;
#endif

signals:
	void DisplayCreated(OBSQTDisplay *window);
	void DisplayResized();
	void AdjustResizeView(QLabel *screen, QLabel *view, bool &handled);

public:
	OBSQTDisplay(QWidget *parent = nullptr,
		     Qt::WindowFlags flags = Qt::WindowFlags());
	~OBSQTDisplay() { display = nullptr; }
	virtual QSize GetWidgetSize();
	virtual QPaintEngine *paintEngine() const override;

	inline obs_display_t *GetDisplay() const { return display; }

	uint32_t backgroundColor = GREY_COLOR_BACKGROUND;

	QColor GetDisplayBackgroundColor() const;
	void SetDisplayBackgroundColor(const QColor &color);
	void UpdateDisplayBackgroundColor();
	void CreateDisplay(bool force = false);
	void DestroyDisplay() { display = nullptr; };

	void OnMove();
	void OnDisplayChange();
	void showGuideText(const QString &guideText);
};
