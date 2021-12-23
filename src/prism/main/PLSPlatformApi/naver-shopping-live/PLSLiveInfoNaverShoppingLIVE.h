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

#define MODULE_NAVER_SHOPPING_LIVE_LIVEINFO "NaverShoppingLIVE/LiveInfo"

class PLSNaverShoppingImageScaleProcessor : public QObject {
	Q_OBJECT

public:
	PLSNaverShoppingImageScaleProcessor() {}
	~PLSNaverShoppingImageScaleProcessor() {}

public:
	void process(PLSPlatformNaverShoppingLIVE *platform, double dpi, const QString &imagePath, const QSize &imageSize);
};

class PLSPLSNaverShoppingLIVEImageScaleThread : public PLSResourcesProcessThread<PLSNaverShoppingImageScaleProcessor, std::tuple<PLSPlatformNaverShoppingLIVE *, double, QString, QSize>> {
public:
	PLSPLSNaverShoppingLIVEImageScaleThread() {}
	~PLSPLSNaverShoppingLIVEImageScaleThread() {}

protected:
	virtual void process();
};

class PLSLiveInfoNaverShoppingLIVE : public PLSLiveInfoBase {
	Q_OBJECT

public:
	explicit PLSLiveInfoNaverShoppingLIVE(PLSPlatformBase *pPlatformBase, const QVariantMap &info, QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSLiveInfoNaverShoppingLIVE();

private:
	void setupData();
	void setupGuideButton();
	void setupLineEdit();
	void setupThumbnailButton();
	void setupCategoryButton();
	void setupScheduleComboBox();
	void setupDateComboBox();
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
	bool isScheduleLive(const PLSScheComboxItemData &itemData);
	void prepareLiving(std::function<void(bool)> callback);
	void saveLiveInfo();
	void startLiving(std::function<void(bool)> callback);
	bool isProductListChanged(const QList<PLSNaverShoppingLIVEAPI::ProductInfo> &oldProductList);
	void getPrepareInfoFromScheduleInfo(PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &prepareInfo, const PLSNaverShoppingLIVEAPI::ScheduleInfo &scheduleInfo);
	void updateContentMargins(double dpi);
	bool isTitleTooLong(QString &title, int maxWordCount);
	void updateLiveTitleUI();
	void updateLivePhotoUI();
	void updateLiveSummaryUI();
	void updateLiveCategoryUI();
	void updateLiveDateUI();
	QString getFirstCategoryTitle();
	QString getSecondCategoryTitle();
	void switchNewScheduleItem(PLSScheComboxItemType type, QString id);
	void updateScheduleLiveInfoRequest(std::function<void(bool)> callback);
	void createLivingRequest(std::function<void(bool)> callback);
	int apIndexForString(const QString &apString);

protected:
	bool eventFilter(QObject *i_Object, QEvent *i_Event);

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
	vector<PLSScheComboxItemData> m_vecItemDatas;
	QScrollBar *m_verticalScrollBar{nullptr};
	QPointer<QLabel> m_pLabelToast;
	QPushButton *m_pButtonToastClose = nullptr;
	//PLSScheComboxItemData m_tempItemData;
	PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo m_TempPrepareInfo;
	PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo m_normalTempPrepareInfo;
	PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo m_scheduleTempPrepareInfo;
	PLSPLSNaverShoppingLIVEImageScaleThread *imageScaleThread = nullptr;
	bool m_refreshProductList;
	PrepareRequestType m_prepareLivingType;
};

#endif // PLSLIVEINFONAVERSHOPPINGLIVE_H
