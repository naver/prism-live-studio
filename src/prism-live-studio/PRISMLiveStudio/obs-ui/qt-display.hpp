#pragma once

#include <QWidget>
#include <QLabel>
#include <obs.hpp>

#define GREY_COLOR_BACKGROUND 0xFF4C4C4C

class OBSQTDisplayFailedView;

#ifdef _WIN32
#define USE_WIN32_HWND
#define DISPLAY_CLASS_NAME L"prism_win32_preview_window"
#endif

class OBSQTDisplay : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QColor displayBackgroundColor MEMBER backgroundColor READ GetDisplayBackgroundColor WRITE
			   SetDisplayBackgroundColor)

	OBSDisplay display;
	bool destroying = false;

	QWidget *textOverlay = nullptr;
	QLabel *displayText;
	OBSQTDisplayFailedView *failedView;

	QMetaObject::Connection screenConnection;

	virtual void paintEvent(QPaintEvent *event) override;
	virtual void moveEvent(QMoveEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
	virtual bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
	void DisplayCreated(OBSQTDisplay *window);
	void DisplayResized();
	void AdjustResizeView(QLabel *screen, QLabel *view, bool &handled);

public:
	OBSQTDisplay(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
	~OBSQTDisplay();

	virtual QSize GetWidgetSize();
	virtual QPaintEngine *paintEngine() const override;

	inline obs_display_t *GetDisplay() const { return display; }

	uint32_t backgroundColor = GREY_COLOR_BACKGROUND;

	QColor GetDisplayBackgroundColor() const;
	void SetDisplayBackgroundColor(const QColor &color);
	void UpdateDisplayBackgroundColor();
	void CreateDisplay(bool force = false);
	void DestroyDisplay()
	{
		display = nullptr;
		destroying = true;
	};
	void ResizeDisplay();

	void OnMove();
	void OnDisplayChange();
	void showGuideText(const QString &guideText);
	void hideGuideText();
	void setFailedText(const QString &failedText, bool needLoading);

private:
	void hideFailedText();
	void showTextLabel(QWidget *label, bool isShow);

protected:
	/** Match obs_display (and Win32 preview HWND) to pixel size. If pixelWidth/pixelHeight are both <= 0, uses GetPixelSize(this). */
	void syncObsDisplaySurfacePixels(int pixelWidth = -1, int pixelHeight = -1);

#ifdef USE_WIN32_HWND
public:
	uint64_t hWndDisplay = 0;
	bool CreatePreviewWindow();
	bool CustomRegisterClass();
	void OnWindowDestroy(uint64_t hWnd);
	void ShowPreviewWindow(bool show);
#endif
};
