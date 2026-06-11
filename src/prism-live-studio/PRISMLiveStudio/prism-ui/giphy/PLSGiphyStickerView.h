#ifndef PLSGIPYSTICKERVIEW_H
#define PLSGIPYSTICKERVIEW_H

#include "PLSSideBarDialogView.h"
#include "GiphyDefine.h"
#include "PLSFloatScrollBarScrollArea.h"
#include "loading-event.hpp"
#include "PLSSearchLineEdit.h"
#include "obs.hpp"
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMovie>
#include <QMutex>
#include <QProcess>
#include <QThread>

namespace Ui {
class PLSGiphyStickerView;
}

class FlowLayout;
class QButtonGroup;
class PLSFloatScrollBarScrollArea;
class ScrollAreaWithNoDataTip;
class GiphyDownloader;
class GiphyWebHandler;
class MovieLabel;
class NoDataPage;
class PLSSearchPopupMenu;
class PLSToastMsgFrame;
class QToolButton;

const static QEvent::Type customType = (QEvent::Type)(QEvent::User + 1);
class AddStickerEvent : public QEvent {
public:
	explicit AddStickerEvent(const ResponData &dataIn) : QEvent(customType), data(dataIn) {}
	~AddStickerEvent() final = default;

	ResponData GetData() const { return data; }

private:
	ResponData data;
};

class PLSGiphyStickerView : public PLSSideBarDialogView {
	Q_OBJECT

	enum TabType { None = -1, RecentTab = 1, TrendingTab = 2 };

public:
	explicit PLSGiphyStickerView(DialogInfo info, QWidget *parent = nullptr);
	~PLSGiphyStickerView() final;

	bool IsSignalConnected(const QMetaMethod &signal) const;

	static PointerValue ConvertPointer(void *ptr);

	void SaveStickerJsonData() const;

protected:
	void hideEvent(QHideEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	bool event(QEvent *event) override;

private:
	void Init();
	void InitCommonForList(ScrollAreaWithNoDataTip *scrollList) const;
	void InitMenuList();
	void InitRecentList();
	void InitTrendingList();
	void InitSearchResultList();
	void ShowTabList(int tabType);
	void StartWebHandler();
	void StartDownloader() const;
	QString GetRequestUrl(const QString &apiType, int limit, int offset) const;
	void ClearTrendingList();
	void ClearRecentList();
	void ClearSearchList();
	void ShowPageWidget(QWidget *widget) const;
	void AddHistorySearchItem(const QString &keyword);
	bool WriteRecentStickerDataToFile(const GiphyData &data, bool del = false);
	bool WriteSearchHistoryToFile(const QString &keyword, bool del = false);
	void WriteTabIndexToConfig(int index) const;
	bool LoadArrayData(const QString &key, QJsonArray &array) const;
	bool CreateJsonFile() const;
	void InitStickerJson();
	void AddRecentStickerList(const QJsonArray &array);
	void UpdateRecentList();
	void UpdateMenuList();
	void DeleteHistoryItem(const QString &key);

	void QuerySearch(const QString &key, int limit, int offset);
	void QueryTrending(int limit, int offset);
	void ShowSearchPage();
	void ShowErrorToast(const QString &tips);
	void UpdateErrorToastPos();
	void HideErrorToast();
	void AutoFillContent();
	void UpdateLimit();
	inline int GetColumCountByWidth(int width, int marginLeftRight) const;
	inline int GetRowCountByHeight(int height, int marginTopBottom) const;
	void UpdateLayoutSpacing(FlowLayout *layout, double dpi) const;
	int GetItemCountToFillContent(const QSize &sizeContent, int leftRightMargin, int topBottomMargin, int &rows, int &colums);
	void AutoFillSearchList(int pagenation);
	void AutoFillTrendingList(bool oneMoreRow = true);
	void GetSearchListGeometry(QSize &sizeContent, QPoint &position) const;
	void AddSearchItem(const GiphyData &data);
	void ShowRetryPage();
	bool IsNetworkAccessible() const;
	void retrySearchList();
	void retryTrendingList();
	void retryRencentList();
	void SetSearchData(const ResponData &data);

	void ShowRecentTab();
	void ShowTrendingTab();

	void ClickRecentTab();
	void ClickTrendingTab();

	bool GetOtherNetworkErrorString(QNetworkReply::NetworkError networkError, QString &errorString);
	bool SearchResultListShowing() const;

private slots:
	void OnTabClicked(int index);
	void requestRespon(const ResponData &data);
	void OnFetchError(const RequestTaskData &task, const RequestErrorInfo &errorInfo);
	void OnSearchTigger(const QString &keyword);
	void OnSearchMenuRequested(bool show);
	void on_btn_cancel_clicked();
	void OriginalDownload(const GiphyData &taskData, const QString &fileName, const QVariant &extra);
	void OriginalDownLoadFailed();
	void OnNetworkAccessibleChanged(bool accessible);
	void OnLoadingVisible(const RequestTaskData &task, bool visible);
	void CreateMovieLabels(QVector<GiphyData> datas, int index, int total);
	void OnAppExit();

signals:
	void request(const RequestTaskData &task);
	void StickerApply(const QString &file, const GiphyData &taskData, const QVariant &extra);
	void StickerApplyForProperty(const QString &fileName);
	void StickerApplyForAddingSource(const QString &fileName);
	void visibleChanged(bool visible);

private:
	Ui::PLSGiphyStickerView *ui;
	QButtonGroup *btnGroup;
	ScrollAreaWithNoDataTip *recentScrollList{nullptr};
	ScrollAreaWithNoDataTip *trendingScrollList{nullptr};
	ScrollAreaWithNoDataTip *searchResultList{nullptr};

	FlowLayout *recentFlowLayout{nullptr};
	FlowLayout *trendingFlowLayout{nullptr};
	FlowLayout *searchResultFlowLayout{nullptr};

	PLSSearchPopupMenu *searchMenu{nullptr};
	PLSToastMsgFrame *networkErrorToast{nullptr};

	QThread threadRequest;
	GiphyDownloader *downloader;
	GiphyWebHandler *webHandler;
	QTimer timerAutoLoad;
	QTimer timerSearch;
	QTimer timerScrollSearch;
	QTimer timerScrollTrending;

	QList<QWidget *> listWidgets;
	QEventLoop eventLoop;
	TabType currentType{None};

	bool network_accessible{true};

	bool showFromProperty{false};
	bool isSearchMode{false};
	bool needUpdateRecentList{false};
	bool needUpdateTrendingList{false};
	bool isFirstShow{true};
	bool isSearching{false};
	int pageLimit{0};
	int lastSearchResponOffset{-1};
	bool stopProcess{false};
	bool showDefaultKeyWord = true;

	QJsonObject jsonObjSticker;
	QString searchKeyword;

	QList<MovieLabel *> listTrending;
	QList<MovieLabel *> listRecent;
	QList<MovieLabel *> listSearch;
};

class ScrollAreaWithNoDataTip : public PLSFloatScrollBarScrollArea {
	Q_OBJECT

public:
	explicit ScrollAreaWithNoDataTip(QWidget *parent = nullptr);
	~ScrollAreaWithNoDataTip() override = default;

	void ResetPagination();
	void SetPagintion(int page);
	int GetPagination() const;

	void ShowNoDataPage();
	void HideNoDataPage();
	void SetNoDataPageVisible(bool visible);
	void SetNoDataTipText(const QString &tipText);

	void SetShowRetryPage();
	void HideRetryPage();
	void SetRetryPageVisible(bool visible);
	void SetRetryTipText(const QString &tipText);
	bool IsNoDataPageVisible() const;

	void ShowLoading();
	void HideLoading();
	void SetLoadingVisible(bool visible);

private:
	void UpdateLayerGeometry();
	void UpdateNodataPageGeometry();

signals:
	void retryClicked();

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	NoDataPage *pageNoData{nullptr};
	QFrame *loadingLayer{nullptr};
	PLSLoadingEvent loadingEvent;
	int pagination{0};
};

class MovieLabel : public QLabel {
	Q_OBJECT
public:
	explicit MovieLabel(QWidget *parent);
	~MovieLabel() override;

	void SetData(const GiphyData &taskData);
	void SetShowLoad(bool visible);

	void SetClicked(bool clicked);
	bool Clicked() const;
	static void DownloadCallback(const TaskResponData &result, void *param);
	void DownloadPreview();

private:
	void DownloadOriginal();
	void LoadFrames();
	void ResizeScale();
	void UpdateRect();

signals:
	void excuteTask(const DownloadTaskData &taskData);
	void Clicked(const GiphyData &taskData);
	void OriginalDownloaded(const GiphyData &taskData, const QString &fileName, const QVariant &extraData = QVariant());
	void DownLoadError();

protected:
	void mousePressEvent(QMouseEvent *ev) override;
	void resizeEvent(QResizeEvent *ev) override;
	void timerEvent(QTimerEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

public slots:
	void downloadCallback(const TaskResponData &responData);

private:
	GiphyData giphyData;
	QLabel *labelLoad;
	QObject *downloadOriginalObj{nullptr};
	TaskResponData giphyFileInfo;
	bool clicked{false};
	QBasicTimer timer;
	int step{-1};
	int imageDelay{0};
	bool needRetry{true};
	std::vector<QPixmap> imageFrames;
	QSize displaySize;
	QSize scaledSize;
	QRect rectTarget = QRect();
	QPointer<PLSLoadingEvent> loadingEvent{nullptr};
};

class NoDataPage : public QFrame {
	Q_OBJECT
public:
	enum class PageType {
		Page_Nodata = 0,
		Page_NetworkError,
	};

	explicit NoDataPage(QWidget *parent = nullptr);
	~NoDataPage() override = default;

	void SetPageType(PageType pageType);
	void SetNoDataTipText(const QString &tipText);

signals:
	void retryClicked();

private:
	QLabel *iconNoData{nullptr};
	QLabel *textNoDataTip{nullptr};
	QPushButton *btnRetry{nullptr};
	QLabel *spaceBottom{nullptr};
	int topPadding{0};
};

#endif // PLSGIPYSTICKERVIEW_H
