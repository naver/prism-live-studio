#ifndef COMPLEXHEADERICON_H
#define COMPLEXHEADERICON_H

#include <QLabel>

/* this is a complex UI for user profile header */
class ComplexHeaderIcon : public QLabel {
	Q_OBJECT
	Q_PROPERTY(bool Active READ isActive WRITE setActive)

private:
	struct PaintObject {

		QString mPixPath;
		QString mPlatPixPath;
		QString mDefaultIconPath;
		QSize mPixSize;
		QSize mPlatPixSize;
		bool mPixSharp = true;
		bool mPlatPixSharp = true;
		QPixmap mBigPix;
		QPixmap mSmallPix;
		bool mActive = false;
		bool mUseContentsRect = false;
		int mPadding = 6;
		QPixmap mViewPixmap;

		bool isEnabled = true;
		QRect mContentRect;
		QRect mRect;
		int width;
		int height;
		double dpi;
	};

public:
	explicit ComplexHeaderIcon(QWidget *parent = nullptr);
	~ComplexHeaderIcon() override;

	void setPixmap(const QString &pix, const QSize &pixSize, bool sharp = true, double showDpiValue = 0.0);
	void setDefaultIconPath(const QString &pix);
	void setPlatformPixmap(const QString &pix, const QSize &pixSize, bool sharp = true);
	void setActive(bool isActive = true);
	bool isActive() { return mPaintObj.mActive; }
	void setUseContentsRect(bool isUseContentsRect = true);
	bool isUseContentsRect() { return mPaintObj.mUseContentsRect; }
	void setPadding(int padding);
	int getPadding() { return mPaintObj.mPadding; }

signals:
	void paintingFinished();

protected:
	void paintEvent(QPaintEvent *event) override;

	static void drawOnDevice(PaintObject &painterObj);

private:
	void delayDraw();
	static void updateBixPix(double dpi, PaintObject &painterObj);
	static void updateSmallPix(double dpi, PaintObject &painterObj);

private:
	PaintObject mPaintObj;
	QTimer *mDelayTimer;
};

#endif // COMPLEXHEADERICON_H
