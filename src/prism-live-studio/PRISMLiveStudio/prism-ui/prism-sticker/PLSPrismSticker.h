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

	bool SaveStickerJsonData() const;
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
	void UserApplySticker(const StickerData &data, StickerPointer label);
	void AdjustCategoryTab();
	void UpdateDpi(double dpi) const;
	void ShowToast(const QString &tips);
	void HideToast();
	void UpdateToastPos();
	void DownloadResource(const StickerData &data, StickerPointer loadingLabel);

	bool LoadLocalJsonFile(const QString &fileName);
	bool CreateRecentJsonFile() const;
	bool InitRecentSticker();
	bool UpdateRecentList(const StickerData &data);

	void ShowNoNetworkPage(const QString &tips, RetryType type);
	void HideNoNetworkPage();

	void UpdateNodataPageGeometry();
	StickerHandleResult RemuxFile(const StickerData &data, StickerPointer label, QString path, QString configFile);
	void ShowLoading(QWidget *parent);

	void DownloadCategoryJson();
	bool NetworkAccessible() const;
	void DoDownloadJsonFile();

	QLayout *GetFlowlayout(const QString &categoryId);
	static void OnFrontendEvent(obs_frontend_event event, void *param);

	QString getTargetImagePath(QString resourcePath, QString category, QString id, bool landscape) const;

	void SelectTab(const QString &categoryId) const;
	void CleanPage(const QString &categoryId);
	bool LoadStickers(QLayout *layout, ScrollAreaWithNoDataTip *targetSa, QWidget *parent, const std::vector<StickerData> &stickerList, const StickerDataIndex &index, bool /*async*/);
	bool LoadViewPage(const QString &categoryId, const QWidget *page, QLayout *layout);

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	bool eventFilter(QObject *watcher, QEvent *event) override;

public slots:
	void HandleDownloadResult(const TaskResponData &result, const StickerData &data, StickerPointer label);
	void DownloadJsonFileTimeOut();
	void OnDownloadJsonFailed();
	void OnAppExit();

private slots:
	void HandleStickerData();
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
