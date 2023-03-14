#ifndef PLSMOTIONITEMVIEW_H
#define PLSMOTIONITEMVIEW_H

#include <QLabel>
#include <QWidget>
#include <QMouseEvent>
#include "PLSMotionDefine.h"
#include "PLSMotionFileManager.h"

namespace Ui {
class PLSMotionItemView;
}

class PLSMotionItemView : public QLabel, public PLSResourcesThumbnailProcessFinished {
	Q_OBJECT
	Q_PROPERTY(QSize imageSize READ imageSize WRITE setImageSize)

private:
	explicit PLSMotionItemView(QWidget *parent = nullptr);
	~PLSMotionItemView();

public:
	const MotionData &motionData() const;
	void setMotionData(const MotionData &motionData, bool properties);

	QSize imageSize() const;
	void setImageSize(const QSize &size);

	void setSelected(bool selected);
	bool selected() const;
	void setIconHidden(bool hidden);
	void setCloseButtonHidden(bool hidden);
	void setCloseButtonHiddenByCursorPosition(const QPoint &cursorPosition);
	bool eventFilter(QObject *watched, QEvent *event);

public:
	static void addBatchCache(int batchCount, bool check = false);
	static void cleaupCache();
	static PLSMotionItemView *alloc(const MotionData &data, bool properties);
	template<typename Object, typename Clicked, typename DelBtnClicked>
	static PLSMotionItemView *alloc(const MotionData &data, bool properties, Object *object, Clicked clicked, DelBtnClicked delBtnClicked)
	{
		PLSMotionItemView *view = alloc(data, properties);
		view->clickedConn = connect(view, &PLSMotionItemView::clicked, object, clicked);
		view->delBtnClickedConn = connect(view, &PLSMotionItemView::deleteFileButtonClicked, object, delBtnClicked);
		return view;
	}
	static void dealloc(PLSMotionItemView *view);

signals:
	void clicked(PLSMotionItemView *view);
	void deleteFileButtonClicked(const MotionData &data);

private:
	double getDpi();

protected:
	void mousePressEvent(QMouseEvent *event);
	void resizeEvent(QResizeEvent *event);
	void paintEvent(QPaintEvent *) override;
	void processThumbnailFinished(QThread *thread, const QString &itemId, const QPixmap &normalPixmap, const QPixmap &selectedPixmap) override;

private:
	Ui::PLSMotionItemView *ui;
	MotionData m_data;
	bool m_isInUsing;
	bool m_properties;
	bool m_selected;
	bool m_hasMotionIcon;
	QPixmap m_normalPixmap;
	QPixmap m_selectedPixmap;
	QMetaObject::Connection clickedConn;
	QMetaObject::Connection delBtnClickedConn;
	QSize m_imageSize{1, 1};
};

#endif // PLSMOTIONITEMVIEW_H
