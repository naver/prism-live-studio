#ifndef PLSLOADINGVIEW_H
#define PLSLOADINGVIEW_H

#include <array>
#include <functional>

#include <QFrame>
#include <QSvgRenderer>
#include <QTimer>

#define newLoadingViewEx(loadingView, ...) newLoadingView(loadingView, __VA_ARGS__)->setObjectName(#loadingView)

class PLSLoadingView : public QFrame {
	Q_OBJECT

public:
	explicit PLSLoadingView(QWidget *parent = nullptr, QString pathImage = QString());
	~PLSLoadingView() override;

	using PfnGetViewRect = std::function<bool(QRect &geometry, PLSLoadingView *loadingView)>;

	static PLSLoadingView *newLoadingView(QWidget *parent, const PfnGetViewRect &getViewRect = nullptr);
	static PLSLoadingView *newLoadingView(PLSLoadingView *&loadingView, QWidget *parent, const PfnGetViewRect &getViewRect = nullptr);
	static PLSLoadingView *newLoadingView(QWidget *parent, int absoluteTop, const PfnGetViewRect &getViewRect = nullptr, const QString &pathImage = QString(),
					      std::optional<QColor> colorBackground = std::nullopt);
	static PLSLoadingView *newLoadingView(PLSLoadingView *&loadingView, QWidget *parent, int absoluteTop, const PfnGetViewRect &getViewRect = nullptr);
	static PLSLoadingView *newLoadingView(PLSLoadingView *&loadingView, bool condition, QWidget *parent, int absoluteTop, const PfnGetViewRect &getViewRect = nullptr);
	static void deleteLoadingView(PLSLoadingView *&loadingView);

	void setAutoResizeFrom(QWidget *autoResizeFrom);
	void setAbsoluteTop(int absoluteTop);
	void updateViewGeometry();

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
	QWidget *m_autoResizeFrom = nullptr;
	PfnGetViewRect m_getViewRect = nullptr;
	std::array<QSvgRenderer, 8> m_svgRenderers;
	int m_timer = -1;
	int m_curPixmap = 0;
	int m_absoluteTop = -1;
	double m_dpi = 1.0;
	std::optional<QColor> m_colorBackground;
};

#endif // PLSLOADINGVIEW_H
