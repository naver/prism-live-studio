#ifndef PLSNOTICEUPDATECENTERDIALOG_H
#define PLSNOTICEUPDATECENTERDIALOG_H

#include "PLSDialogView.h"
#include "PLSNoticeUpdateTypes.hpp"
#include <QButtonGroup>
#include <QEvent>
#include <QMap>
#include <QPointer>

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSNoticeUpdateCenterDialog;
}
QT_END_NAMESPACE

class PLSNoticeUpdateRepository;
class PLSTextLoadingView;

class PLSNoticeUpdateCenterDialog : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSNoticeUpdateCenterDialog(QWidget *parent = nullptr);
	~PLSNoticeUpdateCenterDialog() override;

	void openWithState();
	void openWithState(PLSNoticeFilter initialFilter, PLSNoticeCategory initialCategory, bool isApiRequest, bool prefetchPeerCache = false);

signals:
	void centerClosed();

private slots:
	void onB2BTabClicked();
	void onPrismTabClicked();
	void onNoticeSubClicked();
	void onUpdateSubClicked();
	void onCloseClicked();

protected:
	bool eventFilter(QObject *watcher, QEvent *event) override;

private:
	void updateSubHeaderVisibility();
	void refreshList(PLSNoticeFilter filter, PLSNoticeCategory category = PLSNoticeCategory::Notice, bool isApiRequest = false, bool prefetchPeerCache = false, bool preserveCurrentList = false);
	void loadMore(PLSNoticeFilter filter, PLSNoticeCategory category = PLSNoticeCategory::Notice);
	void displayListFromCache(PLSNoticeFilter filter, PLSNoticeCategory category = PLSNoticeCategory::Notice);
	QList<PLSNoticeUpdateItem> getFilteredItemsFromCache(PLSNoticeFilter filter, PLSNoticeCategory category) const;
	void appendVisibleItems(const QList<PLSNoticeUpdateItem> &items, int startIndex, int endIndex);
	void clearList();
	void showEmptyView();
	void showFailedView();
	void onListItemClicked(const PLSNoticeUpdateItem &item);
	void showListTextLoading();
	void hideListTextLoading();

	Ui::PLSNoticeUpdateCenterDialog *ui{nullptr};
	QPointer<PLSNoticeUpdateRepository> m_repository;
	QButtonGroup *m_mainTabGroup{nullptr};
	QButtonGroup *m_subTabGroup{nullptr};

	PLSNoticeFilter m_filter{PLSNoticeFilter::All};
	PLSNoticeCategory m_category{PLSNoticeCategory::Notice};
	bool m_b2bMode{false};
	QString m_b2bName;

	bool m_loading{false};
	int m_visibleCount{0};
	int m_refreshToken{0};
	bool m_isOnlyPrismNotice = true;

	QPointer<PLSTextLoadingView> m_listTextLoading;
};

#endif // PLSNOTICEUPDATECENTERDIALOG_H
