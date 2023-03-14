#ifndef PLSIMAGELISTVIEW_H
#define PLSIMAGELISTVIEW_H

#include <QFrame>
#include <QPointer>
#include <QMap>
#include <functional>
#include "PLSMotionDefine.h"

class FlowLayout;
class QScrollArea;
class PLSMotionItemView;
class PLSMotionImageListView;
class QPushButton;
class PLSMotionAddButton;

namespace Ui {
class PLSImageListView;
}

class PLSImageListView : public QFrame {

	Q_OBJECT
	Q_PROPERTY(int flowLayoutHSpacing READ flowLayoutHSpacing WRITE setFlowLayoutHSpacing)
	Q_PROPERTY(int flowLayoutVSpacing READ flowLayoutVSpacing WRITE setFlowLayoutVSpacing)

public:
	explicit PLSImageListView(QWidget *parent = nullptr);
	~PLSImageListView();

public:
	int flowLayoutHSpacing() const;
	void setFlowLayoutHSpacing(int flowLayoutHSpacing);
	int flowLayoutVSpacing() const;
	void setFlowLayoutVSpacing(int flowLayoutVSpacing);
	void insertAddFileItem();
	bool setSelectedItem(const QString &itemId);
	int getShowViewListCount() const;
	void setItemSelectedEnabled(bool enabled);
	void clearSelectedItem();
	void deleteItem(const QString &itemId, bool isRecent);
	void deleteAll();
	void setFilterButtonVisible(bool visible);
	void setDeleteAllButtonVisible(bool visible);
	void itemViewSelectedStateChanged(PLSMotionItemView *item);
	int getVerticalScrollBarPosition() const;
	void setVerticalScrollBarPosition(int value);

	bool isForProperites();
	void updateItems(const QList<MotionData> &mds);
	PLSMotionItemView *findItem(const QString &itemId);
	void triggerSelectedSignal();

	void scrollToItem(const QString &itemId);

	void setAutoUpdateCloseButtons(bool autoUpdateCloseButtons);
	void updateCloseButtons();

	void showBottomMargin();

private:
	void initScrollArea();
	void initScrollAreaLayout();
	PLSMotionImageListView *getListView();
	void scrollToSelectedItemIndex();

signals:
	void addFileButtonClicked();

public slots:
	void movieViewClicked(PLSMotionItemView *movieView);
	void filterButtonClicked();
	void deleteAllButtonClicked();
	void deleteFileButtonClicked(const MotionData &data);

protected:
	bool eventFilter(QObject *obj, QEvent *evt) override;

private:
	Ui::PLSImageListView *ui;
	FlowLayout *m_FlowLayout{nullptr};
	QScrollArea *m_scrollList{nullptr};
	QWidget *m_scrollAreaWidget{nullptr};
	QWidget *m_pWidgetLoadingBG;
	int m_flowLayoutHSpacing{0};
	int m_flowLayoutVSpacing{0};
	QString m_itemId;
	bool m_enabled;
	QWidget *m_filterWidget{nullptr};
	QWidget *m_deleteAllWidget{nullptr};
	QPointer<PLSMotionAddButton> m_addButton;
	int m_scrollBarPosition = 0;

	QList<MotionData> mds;
	QList<PLSMotionItemView *> itemViews;
	bool autoUpdateCloseButtons{false};
};

#endif // PLSIMAGELISTVIEW_H
