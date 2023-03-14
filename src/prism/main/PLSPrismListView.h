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
	explicit PLSPrismListView(QWidget *parent = nullptr);
	~PLSPrismListView();
	bool setSelectedItem(const QString &itemId);
	void setItemSelectedEnabled(bool enabled);
	void clearSelectedItem();
	void initListView(bool freeView = false);
	void setFilterButtonVisible(bool visible);
	void initLoadingView();
	PLSImageListView *getImageListView();
	void setForProperties(bool forProperties);

protected:
	bool eventFilter(QObject *watcher, QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void setRemoveRetainSizeWhenHidden(QWidget *widget);
	void initItemListView();
	void retryDownloadList();
	void itemListStarted();
	void itemListFinished();
	void showLoading();
	void hideLoading();
	QRect getLoadingBGRect();

private:
	Ui::PLSPrismListView *ui;
	bool m_freeView;
	bool m_forProperties = false;
	PLSLoadingEvent m_loadingEvent;
	QWidget *m_pWidgetLoadingBG{nullptr};
	QList<MotionData> m_list;
};

#endif // PLSPRISMLISTVIEW_H
