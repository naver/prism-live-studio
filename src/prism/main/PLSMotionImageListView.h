#ifndef PLSMOTIONIMAGELISTVIEW_H
#define PLSMOTIONIMAGELISTVIEW_H

#include <functional>
#include <QFrame>
#include "PLSMotionDefine.h"

class QButtonGroup;
class PLSMotionErrorLayer;
class PLSImageListView;

enum class PLSMotionViewType { PLSMotionDetailView, PLSMotionPropertyView };

namespace Ui {
class PLSMotionImageListView;
}

class PLSMotionImageListView : public QFrame {
	Q_OBJECT

public:
	explicit PLSMotionImageListView(QWidget *parent = nullptr, PLSMotionViewType type = PLSMotionViewType::PLSMotionDetailView, const std::function<void(QWidget *)> &init = nullptr);
	~PLSMotionImageListView();
	PLSMotionViewType viewType();

public:
	void clickItemWithSendSignal(const MotionData &motionData);
	void setSelectedItem(const QString &itemId);
	void deleteItemWithSendSignal(const MotionData &motionData, bool isVbUsed, bool isSourceUsed, bool isRecent);
	void setItemSelectedEnabled(bool enabled);
	void clearSelectedItem();
	void setCheckState(bool checked);
	void setCheckBoxEnabled(bool enabled);
	void setFilterButtonVisible(bool visible);
	void showApplyErrorToast();
	void switchToSelectItem(const QString &itemId);
	bool isRecentTab(PLSImageListView *view) const;
	void adjustErrTipSize(QSize superSize = {});

signals:
	void clickCategoryIndex(int index);
	void checkState(bool checked);
	void currentResourceChanged(const QString &itemId, int type, const QString &resourcePath, const QString &staticImgPath, const QString &thumbnailPath, bool prismResource,
				    const QString &foregroundPath, const QString &foregroundStaticImgPath);
	void setSelectedItemFailed(const QString &itemId);
	void deleteCurrentResource(const QString &itemId);
	void filterButtonClicked();
	void removeAllMyResource(const QStringList &list);

protected:
	virtual bool eventFilter(QObject *obj, QEvent *event) override;

private:
	void initCheckBox();
	void initButton();
	void initScrollArea();
	void initCategoryIndex();
	PLSImageListView *getImageListView(int index);
	void onCheckedRemoved(const MotionData &md, bool isVbUsed, bool isSourceUsed);

public slots:
	void clickItemWithSendSignal(int groupIndex, int itemIndex);
	void deleteAllResources();
	void buttonGroupSlot(int buttonId);

private:
	Ui::PLSMotionImageListView *ui;
	QButtonGroup *m_buttonGroup;
	int m_buttonId;
	PLSMotionViewType m_viewType;
	PLSMotionErrorLayer *m_errorToast{nullptr};
	bool m_itemSelectedEnabled = true;
	MotionData m_currentMotionData;
};

#endif // PLSMOTIONIMAGELISTVIEW_H
