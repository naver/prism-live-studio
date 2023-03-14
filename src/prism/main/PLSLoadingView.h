#ifndef PLSLOADINGVIEW_H
#define PLSLOADINGVIEW_H

#include <functional>

#include <QFrame>
#include <QSvgRenderer>
#include <QTimer>

#define newLoadingViewEx(loadingView, ...) newLoadingView(loadingView, __VA_ARGS__)->setObjectName(#loadingView)

class PLSLoadingView : public QFrame {
	Q_OBJECT

public:
	explicit PLSLoadingView(QWidget *parent = nullptr);
	~PLSLoadingView();

public:
	using PfnGetViewRect = std::function<bool(QRect &geometry, PLSLoadingView *loadingView)>;

public:
	static PLSLoadingView *newLoadingView(QWidget *parent, PfnGetViewRect &&getViewRect = nullptr);
	static PLSLoadingView *newLoadingView(PLSLoadingView *&loadingView, QWidget *parent, PfnGetViewRect &&getViewRect = nullptr);
	static PLSLoadingView *newLoadingView(QWidget *parent, int absoluteTop, PfnGetViewRect &&getViewRect = nullptr);
	static PLSLoadingView *newLoadingView(PLSLoadingView *&loadingView, QWidget *parent, int absoluteTop, PfnGetViewRect &&getViewRect = nullptr);
	static PLSLoadingView *newLoadingView(PLSLoadingView *&loadingView, bool condition, QWidget *parent, int absoluteTop, PfnGetViewRect &&getViewRect = nullptr);
	static void deleteLoadingView(PLSLoadingView *&loadingView);

public:
	void setAutoResizeFrom(QWidget *autoResizeFrom);
	void setAbsoluteTop(int absoluteTop);
	void updateGeometry();

private:
	void setSuggestedGeometry(const QRect &suggested);
	void setSuggestedGeometry(const QPoint &position, const QSize &size);
	QSize calcSuggestedSize(const QSize &size) const;

protected:
	void timerEvent(QTimerEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	QWidget *autoResizeFrom = nullptr;
	PfnGetViewRect getViewRect = nullptr;
	QSvgRenderer svgRenderers[8];
	int timer = -1;
	int curPixmap = 0;
	int absoluteTop = -1;
	double dpi = 1.0;
};

#endif // PLSLOADINGVIEW_H
