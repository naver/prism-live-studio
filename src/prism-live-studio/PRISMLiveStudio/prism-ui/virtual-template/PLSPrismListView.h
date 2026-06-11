#ifndef PLSPRISMLISTVIEW_H
#define PLSPRISMLISTVIEW_H

#include <QFrame>
#include <functional>
#include "PLSMotionDefine.h"
#include "loading-event.hpp"

class PLSImageListView;

namespace Ui {
class PLSPrismListView;
}

class PLSPrismListView : public QFrame {
	Q_OBJECT

public:
	explicit PLSPrismListView(const QString &groupId, QWidget *parent = nullptr);
	~PLSPrismListView() override;
	bool setSelectedItem(const QString &itemId);
	void setItemSelectedEnabled(bool enabled);
	void clearSelectedItem();
	void initListView();
	void setFilterButtonVisible(bool visible);
	void initLoadingView();
	PLSImageListView *getImageListView();
	void setForProperties(bool forProperties);

protected:
	bool eventFilter(QObject *watcher, QEvent *event) override;

private:
	void setRemoveRetainSizeWhenHidden(QWidget *widget) const;
	void initItemListView();
	void updateItem(const MotionData &md);
	void retryDownloadList();
	void itemListStarted();
	void itemListFinished();
	void showLoading();
	void hideLoading();
	QRect getLoadingBGRect();

	Ui::PLSPrismListView *ui;
	bool m_forProperties = false;
	PLSLoadingEvent m_loadingEvent;
	QWidget *m_pWidgetLoadingBG{nullptr};
	QList<MotionData> m_list;
	QString retryClickedId;
	QString groupId;
};

#endif // PLSPRISMLISTVIEW_H
