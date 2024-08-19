#ifndef PLSNAVERSHOPPINGLIVEPRODUCTIMAGEVIEW_H
#define PLSNAVERSHOPPINGLIVEPRODUCTIMAGEVIEW_H

#include <QLabel>
#include <QPixmap>

#include "PLSNaverShoppingLIVEImageProcessFinished.h"

class PLSNaverShoppingLIVEProductImageView : public QLabel, public PLSNaverShoppingLIVEImageProcessFinished {
	Q_OBJECT

public:
	explicit PLSNaverShoppingLIVEProductImageView(QWidget *parent = nullptr);
	~PLSNaverShoppingLIVEProductImageView() override = default;

	void setImage(const QString &url, const QString &imageUrl, const QString &imagePath, bool hasDiscountIcon, bool isInLiveinfo, double opacity = 1.0);
	void setImage(const QString &url, const QPixmap &normalPixmap, const QPixmap &hoveredPixmap, bool hasDiscountIcon, bool isInLiveinfo, double opacity = 1.0);

	void mouseEnter();
	void mouseLeave();

protected:
	bool event(QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void processFinished(bool ok, QThread *thread, const QString &imageUrl, const QPixmap &normalPixmap, const QPixmap &hoveredPixmap, const QPixmap &livePixmap,
			     const QPixmap &liveHoveredPixmap) override;

private:
	bool hasDiscountIcon = false;
	bool hovered = false;
	bool isInLiveinfo = false;
	double opacity = 1.0;
	QString url;
	QString imageUrl;
	QString imagePath;
	QPixmap normalPixmap;
	QPixmap hoveredPixmap;
	QPixmap livePixmap;
	QPixmap liveHoveredPixmap;
	QPixmap liveDiscountPixmap;
	QLabel *borderLabel{nullptr};
};

#endif // PLSNAVERSHOPPINGLIVEPRODUCTIMAGEVIEW_H
