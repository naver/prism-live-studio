#ifndef PLSNOTICEPOPUPDIALOG_H
#define PLSNOTICEPOPUPDIALOG_H

#include "PLSDialogView.h"
#include "PLSNoticeUpdateTypes.hpp"
#include "libbrowser.h"
#include <QButtonGroup>
#include <QPointer>
#include <QSet>

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSNoticePopupDialog;
}
QT_END_NAMESPACE

class PLSMainView;
class PLSTextLoadingView;

class PLSNoticePopupDialog : public PLSDialogView {
	Q_OBJECT

public:
	static QList<QList<PLSNoticeUpdateItem>> buildNoticePopupPayloads(const QList<PLSNoticeUpdateItem> &noticeInfos, bool isB2BMode);
	static bool showNoticePopupQueue(const QList<PLSNoticeUpdateItem> &noticeInfos, QWidget *parent, PLSMainView *mainView = nullptr,
					 PLSNoticeFilter *outOpenCenterFilter = nullptr);

	explicit PLSNoticePopupDialog(const QList<PLSNoticeUpdateItem> &items, PLSNoticeProvider provider, QWidget *parent = nullptr, bool showViewAllButton = true);
	explicit PLSNoticePopupDialog(const QString &contentUrl, bool hasUpdate, bool hasForceUpdate, const QString &detailUrl = QString(), QWidget *parent = nullptr);
	~PLSNoticePopupDialog() override;
	QList<PLSNoticeUpdateItem> visitedItems() const;
	bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
	void on_confirmButton_clicked();
	void on_learnMoreButton_clicked();
	void updateContent(const QString &url);

private:
	void createTopLayout();
	void initUI();
	void setupCommon();
	int indexForProvider(PLSNoticeProvider provider) const;
	void markCurrentNoticeVisited();
	void syncDualNoticeControls();
	void switchToNoticeIndex(int index);
	/** Notice popup (items ctor): topLabel text by provider/category; empty when B2B+Prism dual tab. */
	QString noticePopupTopLabelText() const;
	void showContentLoading();
	void hideContentLoading();
	void updateContentLoadingGeometry() const;

private:
	Ui::PLSNoticePopupDialog *ui;
	QList<PLSNoticeUpdateItem> m_items;
	int m_selectTab = 0;
	pls::browser::BrowserWidget *m_browserWidget{nullptr};
	QButtonGroup m_tabGroup;
	PLSNoticeProvider m_noticeProvider;
	bool m_hasUpdate = false;
	bool m_hasForceUpdate = false;
	bool m_openCenterRequested = false;
	bool m_showViewAllButton = true;
	PLSNoticeFilter m_requestedCenterFilter = PLSNoticeFilter::PrismOnly;
	QSet<int> m_visitedIndexes;
	QPointer<PLSTextLoadingView> m_contentLoading;
};

#endif // PLSNOTICEPOPUPDIALOG_H
