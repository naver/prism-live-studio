#ifndef PLSLIVEINFONAVERSHOPPINGLIVE_H
#define PLSLIVEINFONAVERSHOPPINGLIVE_H

#include <vector>

#include "../PLSLiveInfoBase.h"
#include "PLSScheduleCombox.h"
#include "PLSPlatformNaverShoppingLIVE.h"
#include "PLSLiveInfoNaverShoppingLIVEProductList.h"
#include "PLSMotionFileManager.h"

enum class PrepareRequestType { GoLivePrepareRequest, LiveBeforeOkButtonPrepareRequest, UpdateLivingPrepareRequest, RehearsalPrepareRequest, LiveBeforeUpdateSchedulePrepareRequest };

namespace Ui {
class PLSLiveInfoNaverShoppingLIVE;
}

constexpr auto MODULE_NAVER_SHOPPING_LIVE_LIVEINFO = "NaverShoppingLIVE/LiveInfo";

class PLSNaverShoppingImageScaleProcessor : public QObject {
	Q_OBJECT

public:
	PLSNaverShoppingImageScaleProcessor() = default;
	~PLSNaverShoppingImageScaleProcessor() override = default;
	void process(PLSPlatformNaverShoppingLIVE *platform, double dpi, const QString &imagePath, const QSize &imageSize) const;
};

class PLSPLSNaverShoppingLIVEImageScaleThread : public PLSResourcesProcessThread<PLSNaverShoppingImageScaleProcessor, std::tuple<PLSPlatformNaverShoppingLIVE *, double, QString, QSize>> {

public:
	PLSPLSNaverShoppingLIVEImageScaleThread() = default;
	~PLSPLSNaverShoppingLIVEImageScaleThread() override = default;

protected:
	void process() override;
};

class PLSLiveInfoNaverShoppingLIVE : public PLSLiveInfoBase {
	Q_OBJECT

public:
	explicit PLSLiveInfoNaverShoppingLIVE(PLSPlatformBase *pPlatformBase, const QVariantMap &info, QWidget *parent = nullptr);
	~PLSLiveInfoNaverShoppingLIVE() override;

private:
	void setupData();
	void setupGuideButton();
	void setupLineEdit();
	void setupThumbnailButton() const;
	void setupScheduleComboBox();
	void setupDateComboBox();
	void setupSearchRadio();
	void setupProductList();
	void setupEventFilter();
	void initSetupUI(bool requestFinished = false);
	void updateRequest();
	void getCategoryListRequest();
	void updateSetupUI();
	void doUpdateOkState();
	bool isModified(const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &info);
	bool isOkButtonEnabled();
	void createToastWidget();
	void adjustToastSize();
	bool isScheduleLive(const PLSScheComboxItemData &itemData) const;
	void prepareLiving(const std::function<void(bool)> &callback);
	void saveLiveInfo();
	void startLiving(const std::function<void(bool)> &callback);
	bool isProductListChanged(const QList<PLSNaverShoppingLIVEAPI::ProductInfo> &oldProductList) const;
	void getPrepareInfoFromScheduleInfo(PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &prepareInfo, const PLSNaverShoppingLIVEAPI::ScheduleInfo &scheduleInfo) const;
	void updateContentMargins(double dpi);
	bool isTitleTooLong(QString &title, int maxWordCount) const;
	void updateLiveTitleUI();
	void updateLivePhotoUI();
	void updateLiveSummaryUI();
	void updateLiveDateUI();
	void updateSearchUI();
	void updateNotifyUI();
	void switchNewScheduleItem(PLSScheComboxItemType type, QString id);
	void updateScheduleLiveInfoRequest(const std::function<void(bool)> &callback);
	void createLivingRequest(const std::function<void(bool)> &callback);
	int apIndexForString(const QString &apString) const;
	void showToast(const QString &str);
	void scheduleListLoadingFinished(PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList, const QByteArray &data);
	void checkSwitchNewScheduleItem(const PLSScheComboxItemData &selelctData);
	void updateSelectedScheduleItenInfo(const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList);

protected:
	bool eventFilter(QObject *i_Object, QEvent *i_Event) override;
#if defined(Q_OS_MACOS)
	QList<QWidget *> moveContentExcludeWidgetList() override;
#endif

private slots:
	void on_cancelButton_clicked();
	void on_okButton_clicked();
	void on_rehearsalButton_clicked();
	void on_linkButton_clicked();
	void scheduleButtonClicked();
	void scheduleItemClick(const QString selectID);

public:
	PLSPlatformNaverShoppingLIVE *getPlatform();
	void titleEdited();
	void summaryEdited();
	void imageSelected(const QString &imageFilePath);

private:
	Ui::PLSLiveInfoNaverShoppingLIVE *ui;
	PLSPlatformNaverShoppingLIVE *platform;
	QVariantMap srcInfo;
	std::vector<PLSScheComboxItemData> m_vecItemDatas;
	QScrollBar *m_verticalScrollBar{nullptr};
	QPointer<QLabel> m_pLabelToast;
	QPushButton *m_pButtonToastClose = nullptr;
	PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo m_TempPrepareInfo;
	PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo m_normalTempPrepareInfo;
	PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo m_scheduleTempPrepareInfo;
	PLSPLSNaverShoppingLIVEImageScaleThread *imageScaleThread = nullptr;
	bool m_refreshProductList;
	PrepareRequestType m_prepareLivingType;
	QTimer *m_closeGuideTimer{nullptr};
	uint64_t m_requestFlag = 0;
};

#endif // PLSLIVEINFONAVERSHOPPINGLIVE_H
