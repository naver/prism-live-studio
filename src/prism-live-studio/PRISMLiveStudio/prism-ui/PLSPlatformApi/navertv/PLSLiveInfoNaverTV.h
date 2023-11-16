#ifndef PLSLIVEINFONAVERTV_H
#define PLSLIVEINFONAVERTV_H

#include <vector>

#include "PLSLiveInfoBase.h"
#include "PLSScheduleCombox.h"
#include "PLSPlatformNaverTV.h"

namespace Ui {
class PLSLiveInfoNaverTV;
}

class PLSLiveInfoNaverTV : public PLSLiveInfoBase {
	Q_OBJECT

public:
	explicit PLSLiveInfoNaverTV(PLSPlatformBase *pPlatformBase, const QVariantMap &info, QWidget *parent = nullptr);
	~PLSLiveInfoNaverTV() override;

	void onOk(bool isRehearsal);

private slots:
	void okButtonClicked();
	void cancelButtonClicked();
	void rehearsalButtonClicked();

	void scheduleButtonClicked();
	void scheduleItemClick(const QString &selectID);

	void titleEdited();

	void doUpdateOkState();

	void onImageSelected(const QString &imageFilePath);

	void onApiRequestFailed(bool tokenExpired);

private:
	const PLSPlatformNaverTV::LiveInfo *getScheLiveInfo(int scheLiveId) const;

protected:
	void showEvent(QShowEvent *event) override;

private:
	void processGetScheLives(bool ok, int code, const QList<PLSPlatformNaverTV::LiveInfo> &scheLiveInfos, PLSPlatformNaverTV::LiveInfo *liveInfo);
	void processRefreshScheLives(bool ok, const QList<PLSPlatformNaverTV::LiveInfo> &scheliveInfos);

	Ui::PLSLiveInfoNaverTV *ui = nullptr;
	PLSPlatformNaverTV *platform = nullptr;
	QVariantMap srcInfo;
	std::vector<PLSScheComboxItemData> scheLiveItems;
	int selectedId = -1;
	QList<PLSPlatformNaverTV::LiveInfo> scheLiveList;
	QString tmpNewLiveInfoTitle;
	QString tmpNewLiveInfoThumbnailPath;
	bool refreshingScheLiveInfo = false;
};

#endif // PLSLIVEINFONAVERTV_H
