#ifndef PLSRECENTLISTVIEW_H
#define PLSRECENTLISTVIEW_H

#include <QFrame>
#include <functional>
#include "PLSMotionDefine.h"
#include "PLSImageListView.h"

namespace Ui {
class PLSRecentListView;
}

class PLSRecentListView : public QFrame {
	Q_OBJECT

public:
	explicit PLSRecentListView(QWidget *parent = nullptr);
	~PLSRecentListView();
	void updateMotionList(QList<MotionData> &list);
	bool setSelectedItem(const QString &itemId);
	void setItemSelectedEnabled(bool enabled);
	void clearSelectedItem();
	void deleteItem(const QString &itemId);
	void setFilterButtonVisible(bool visible);
	PLSImageListView *getImageListView();

private:
	void setRemoveRetainSizeWhenHidden(QWidget *widget);
	void deleteResourceFinished(QObject *sourceUi, const QString &itemId, bool isVbUsed, bool isSourceUsed);

private:
	Ui::PLSRecentListView *ui;
	int m_lastID = 0;
	QList<MotionData> m_list;
	QList<MotionData> m_rmlist;
};

#endif // PLSRECENTLISTVIEW_H
