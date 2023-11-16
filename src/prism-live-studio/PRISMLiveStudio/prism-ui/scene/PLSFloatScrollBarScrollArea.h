#ifndef PLSFLOATSCROLLBARSCROLLAREA_H
#define PLSFLOATSCROLLBARSCROLLAREA_H

#include <QScrollArea>
#include <QWidget>
#include <QScrollBar>

// a scroll area whose scroll bar floats up.
class PLSFloatScrollBarScrollArea : public QScrollArea {
	Q_OBJECT
	//don't set 'hdpi' flag for following properties
	Q_PROPERTY(int topMargin READ topMargin WRITE setTopMargin)
	Q_PROPERTY(int bottomMargin READ bottomMargin WRITE setBottomMargin)
public:
	explicit PLSFloatScrollBarScrollArea(QWidget *parent = nullptr);
	~PLSFloatScrollBarScrollArea() override = default;

	QScrollBar *VerticalScrollBar() { return delegateScrollBar; };
	void SetScrollBarRightMargin(int rightMargin);

	void setTopMargin(int value);
	int topMargin() const;

	void setBottomMargin(int value);
	int bottomMargin() const;

signals:
	void ScrollBarRangChanged();
	void ScrolledToEnd();
	void ScrolledToTop();
	void ScrollBarVisibleChanged(bool);

protected:
	void resizeEvent(QResizeEvent *event) override;
	bool eventFilter(QObject *watcher, QEvent *e) override;

private slots:
	void OnVerticalScrollBarValueChanged(int value);
	void OnRangeChanged(int, int);

private:
	void UpdateSlider();
	void UpdateSliderGeometry();

	QScrollBar *delegateScrollBar{nullptr};
	int rightMargin{0};
	int topMarginValue{1};
	int bottomMarginValue{1};
};

#endif // PLSFLOATSCROLLBARSCROLLAREA_H
