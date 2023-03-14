#ifndef PLSLIVEINFONAVERTV_H
#define PLSLIVEINFONAVERTV_H

#include <vector>

#include "../PLSLiveInfoBase.h"
#include "PLSScheduleCombox.h"
#include "PLSPlatformNaverTV.h"

namespace Ui {
class PLSLiveInfoNaverTV;
}

class PLSLiveInfoNaverTV : public PLSLiveInfoBase {
	Q_OBJECT

public:
	explicit PLSLiveInfoNaverTV(PLSPlatformBase *pPlatformBase, const QVariantMap &info, QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSLiveInfoNaverTV();

public:
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
	virtual void showEvent(QShowEvent *event) override;

private:
	Ui::PLSLiveInfoNaverTV *ui;
	PLSPlatformNaverTV *platform;
	QVariantMap srcInfo;
	vector<PLSScheComboxItemData> scheLiveItems;
	int selectedId;
	QList<PLSPlatformNaverTV::LiveInfo> scheLiveList;
	QString tmpNewLiveInfoTitle;
	QString tmpNewLiveInfoThumbnailPath;
	bool refreshingScheLiveInfo = false;
};

#endif // PLSLIVEINFONAVERTV_H
