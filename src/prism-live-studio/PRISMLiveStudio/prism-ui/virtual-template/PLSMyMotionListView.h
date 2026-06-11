#ifndef PLSMYMOTIONLISTVIEW_H
#define PLSMYMOTIONLISTVIEW_H

#include <QFrame>
#include "PLSMotionDefine.h"
#include "PLSImageListView.h"
#include <functional>

class QLayoutItem;
class PLSMotionImageListView;

namespace Ui {
class PLSMyMotionListView;
}

class PLSMyMotionListView : public QFrame {
	Q_OBJECT

public:
	explicit PLSMyMotionListView(QWidget *parent = nullptr);
	~PLSMyMotionListView() override;
	void updateMotionList(const QList<MotionData> &list);
	void setFilterButtonVisible(bool visible);
	bool setSelectedItem(const QString &itemId);
	void setItemSelectedEnabled(bool enabled);
	void clearSelectedItem();
	void deleteItem(const QString &itemId);
	void deleteItemEx(const QString &itemId);
	void deleteAll();
	PLSImageListView *getImageListView();

private:
	void setRemoveRetainSizeWhenHidden(QWidget *widget) const;
	void chooseLocalFileDialog();
	PLSMotionImageListView *getListView();

	void addResourceFinished(const QObject *sourceUi, const MotionData &md, bool isLast);
	void addResourcesFinished(const QObject *sourceUi, int error);
	void deleteResourceFinished(const QObject *sourceUi, const QString &itemId, bool isVbUsed, bool isSourceUsed);

	Ui::PLSMyMotionListView *ui = nullptr;
	QList<MotionData> m_list;
	QList<MotionData> m_rmlist;
	bool itemSelectedEnabled = true;
};

#endif // PLSMYMOTIONLISTVIEW_H
