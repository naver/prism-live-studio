#ifndef PLSPRISMSTICKER_H
#define PLSPRISMSTICKER_H

#include "frontend-api/dialog-view.hpp"
#include "PLSFloatScrollBarScrollArea.h"
#include "pls-common-define.hpp"
#include "loading-event.hpp"
#include "PLSHttpApi/PLSFileDownloader.h"
#include "PLSGipyStickerView.h"
#include "PLSThumbnailLabel.hpp"
#include "PLSStickerDataHandler.h"
#include <QMutex>
#include <QPainter>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QSvgRenderer>
#include <Windows.h>

namespace Ui {
class PLSPrismSticker;
}

class ScrollAreaWithNoDataTip;
class FlowLayout;
class PLSToastMsgFrame;
class QStackedWidget;

class PLSPrismSticker : public PLSDialogView {
	Q_OBJECT

	enum RetryType { Timeout = 0, NoNetwork };
	using StickerPointer = QPointer<PLSThumbnailLabel>;

public:
	explicit PLSPrismSticker(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSPrismSticker();

	bool SaveStickerJsonData();
	bool WriteDownloadCache();
	void SetExitFlag(bool exit_)
	{
		exit = exit_;
		PLSFileDownloader::instance()->Stop();
	};

private:
	void InitScrollView();
	void InitCategory();
	void HandleStickerData();
	void SwitchToCategory(const QString &categoryId);
	void UserApplySticker(const StickerData &data, StickerPointer label);
	void AdjustCategoryTab();
	void UpdateDpi(double dpi);
	void ShowToast(const QString &tips);
	void HideToast();
	void UpdateToastPos();
	void DownloadResource(const StickerData &data, StickerPointer loadingLabel);

	bool LoadLocalJsonFile(const QString &fileName);
	bool CreateRecentJsonFile();
	bool InitRecentSticker();
	bool UpdateRecentList(const StickerData &data);

	void ShowNoNetworkPage(const QString &tips, RetryType type);
	void HideNoNetworkPage();

	void UpdateNodataPageGeometry();
	void ShowLoading(QWidget *parent);

	bool DownloadCategoryJson();
	bool NetworkAccessible();

	QLayout *GetFlowlayout(const QString &categoryId);
	static void OnFrontendEvent(enum obs_frontend_event event, void *param);

	QString getTargetImagePath(QString resourcePath, QString category, QString id, bool landscape);

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	bool eventFilter(QObject *watcher, QEvent *event) override;

public slots:
	void HandleDownloadResult(const TaskResponData &result, const StickerData &data, StickerPointer label);

private slots:
	void OnCategoryBtnClicked(QAbstractButton *button);
	void OnBtnMoreClicked();
	void OnHandleStickerDataFinished();
	void HandleDownloadInitJson(const TaskResponData &result);
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
	std::vector<std::pair<QString, StickerDataIndex>> stickers;
	std::vector<StickerData> allStickerData;
	std::vector<StickerData> recentStickerData;
	std::map<QString, QWidget *> categoryViews;
	QString categoryTabId = "";
	bool showMoreBtn = true;
	bool needUpdateRecent = false;
	bool isDataReady = false;
	bool isShown = false;
	volatile bool exit = false;
	QTimer *timerLoading = nullptr;
	QTimer *timerTimeout = nullptr;
	QMutex mutex;
	QJsonObject downloadCache;

	PLSLoadingEvent m_loadingEvent;
	QWidget *m_pWidgetLoadingBG = nullptr;
	NoDataPage *m_pNodataPage = nullptr;
};

#endif // PLSPRISMSTICKER_H
