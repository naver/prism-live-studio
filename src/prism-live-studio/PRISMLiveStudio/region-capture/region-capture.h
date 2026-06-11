#ifndef PLSREGIONCAPTURE_H
#define PLSREGIONCAPTURE_H

#include <QDialog>
#include <QPen>
#include <QFrame>
#include <QRectF>
#include <QPixmap>
#include <QLabel>

class RegionCapture : public QWidget {
	Q_OBJECT

	struct ScreenShotData {
		QPixmap pixmap;
		QRect geometry;
	};

public:
	explicit RegionCapture(QWidget *parent = nullptr);
	~RegionCapture() final;
	void StartCapture(uint64_t maxRegionWidth = 0, uint64_t maxRegionHeight = 0);
	QRect GetSelectedRect() const;

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;

private:
	void UpdateSelectedRect();
	void drawBackground(QPainter *painter, const QRect &rect) const;
	void drawCenterLines(QPainter *painter) const;
	void showMenus();
	void hideMuenus();
	void Init();
	QRect CalculateFrameSize();
	void SetTipCoordinate(int tipTextWidth, int tipTextHeight);
	void SetMenuCoordinate();
	void SetCurrentPosDpi(const QPoint &posGlobal);
	void SetHook();
	void ReleaseHook() const;
	void SetCoordinateValue();
	void CheckMoveToDown(int &startX, int &startY) const;
	void CheckMoveToTop(int &startX, int &startY) const;

signals:
	void selectedRegion(const QRect &rect);

private:
	QFrame *menuFrame = nullptr;
	QLabel *labelTip = nullptr;
	QLabel *labelRect = nullptr;
	QPoint pointPressed;
	QPoint pointReleased;
	QPoint pointMoved;
	QRect menuGeometry;
	QPoint tipPositon;
	bool pressed = false;
	bool overWidth = false;
	bool overHeight = false;
	std::optional<double> screenDpi;
	QRect rectWhole;
	QRect rectSelected;
	QPen penLine;
	QPen penBorder;
	QList<ScreenShotData> screenShots;
	uint64_t maxRegionWidth = 0;
	uint64_t maxRegionHeight = 0;
	bool drawing = true;
};

#endif // PLSREGIONCAPTURE_H
