#ifndef PLSPRISMSTICKER_H
#define PLSPRISMSTICKER_H

#include "PLSDialogView.h"
#include "PLSFloatScrollBarScrollArea.h"
#include "pls-common-define.hpp"
#include "loading-event.hpp"
#include "PLSFileDownloader.h"
#include "giphy/PLSGiphyStickerView.h"
#include "PLSThumbnailLabel.hpp"
#include "PLSStickerDataHandler.h"
#include "common/PLSToastMsgFrame.h"
#include "obs-frontend-api.h"
#include <QMutex>
#include <QPainter>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QSvgRenderer>
#include "PLSPushButton.h"
#include "PrismStickerResourceMgr.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace Ui {
class PLSPrismSticker;
}

class ScrollAreaWithNoDataTip;
class FlowLayout;
class PLSToastMsgFrame;
class QStackedWidget;

class PLSPrismSticker : public PLSSideBarDialogView {
	Q_OBJECT

	enum RetryType { Timeout = 0, NoNetwork };
	using StickerPointer = QPointer<PLSThumbnailLabel>;

public:
	explicit PLSPrismSticker(QWidget *parent = nullptr);
	~PLSPrismSticker() final;

	bool WriteDownloadCache() const;
	void SetExitFlag(bool exit_)
	{
		exit = exit_;
		PLSFileDownloader::instance()->Stop();
	};

private:
	void InitScrollView();
	void InitCategory();
	void SwitchToCategory(const QString &categoryId);
	void ApplySticker(const pls::rsm::Item &item, StickerPointer label);
	void AdjustCategoryTab();
	void UpdateDpi(double dpi) const;
	void ShowToast(const QString &tips);
	void HideToast();
	void UpdateToastPos();
	void DownloadResource(const StickerData &data, StickerPointer loadingLabel);
	bool UpdateRecentList(const pls::rsm::Item &item);

	void ShowNoNetworkPage(const QString &tips, RetryType type);
	void HideNoNetworkPage();

	void UpdateNodataPageGeometry();
	void ShowLoading(QWidget *parent);

	void DownloadCategoryJson();
	bool NetworkAccessible() const;
	void DoDownloadJsonFile();

	QLayout *GetFlowlayout(const QString &categoryId);
	void SelectTab(const QString &categoryId) const;
	void CleanPage(const QString &categoryId);
	bool LoadStickers(QLayout *layout, ScrollAreaWithNoDataTip *targetSa, QWidget *parent, const std::list<pls::rsm::Item> &stickerList);
	bool LoadViewPage(const QString &categoryId, const QWidget *page, QLayout *layout);
	std::list<pls::rsm::Item> GetRecentUsedItem();
	bool CategoryTabsVisible();

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	bool eventFilter(QObject *watcher, QEvent *event) override;

public slots:
	void OnDownloadItemResult(const pls::rsm::Item &item, bool ok, bool timeout);
	void OnDownloadJsonFailed(bool timeout);
	void OnAppExit();

private slots:
	void HandleStickerData();
	void OnCategoryBtnClicked(QAbstractButton *button);
	void OnBtnMoreClicked();
	void OnHandleStickerDataFinished();
	void OnNetworkAccessibleChanged(bool accessible);

	void OnRetryOnTimeOut();
	void OnRetryOnNoNetwork();

	void HideLoading();
	void LoadStickerAsync(const StickerData &data, QWidget *parent, QLayout *layout);

signals:
	void StickerApplied(const StickerHandleResult &data);
	void HandleStickerResult(const StickerData &data, bool success);

private:
	Ui::PLSPrismSticker *ui;
	QButtonGroup *categoryBtn = nullptr;
	QPushButton *btnMore = nullptr;
	ScrollAreaWithNoDataTip *contentView = nullptr;
	PLSToastMsgFrame *toastTip = nullptr;
	QStackedWidget *stackedWidget = nullptr;
	QPointer<PLSThumbnailLabel> lastClicked = nullptr;
	QMap<QString, QPointer<PLSThumbnailLabel>> requestDownloadLabels;
	std::map<QString, QWidget *> categoryViews;
	QString categoryTabId = "";
	bool showMoreBtn = true;
	bool needUpdateRecent = false;
	bool isDataReady = false;
	bool isShown = false;
	std::atomic_bool exit = false;
	QTimer *timerLoading = nullptr;
	QTimer *timerTimeout = nullptr;
	QMutex mutex;
	QJsonObject downloadCache;

	PLSLoadingEvent m_loadingEvent;
	QWidget *m_pWidgetLoadingBG = nullptr;
	QPointer<NoDataPage> m_pNodataPage;
};

#endif // PLSPRISMSTICKER_H
