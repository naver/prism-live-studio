#ifndef PLSLIVEINFOFACEBOOK_H
#define PLSLIVEINFOFACEBOOK_H

#include "../PLSLiveInfoBase.h"
#include "PLSAPIFacebook.h"
#include "PLSPlatformFacebook.h"

enum class PLSLiveInfoFacebookErrorType {
	PLSLiveInfoFacebookGroupError,
	PLSLiveInfoFacebookPageError,
	PLSLiveInfoFacebookSearchGameError,
};

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSLiveInfoFacebook;
}
QT_END_NAMESPACE

class PLSLiveInfoFacebook : public PLSLiveInfoBase {

	Q_OBJECT

public:
	explicit PLSLiveInfoFacebook(PLSPlatformBase *pPlatformBase, QWidget *parent = nullptr);
	~PLSLiveInfoFacebook() override;
	void handleRequestFunctionType(PLSAPIFacebookType type);

	void handleFacebookIncalidAccessToken();

private slots:
	void on_cancelButton_clicked();
	void on_okButton_clicked();

private:
	void initComboBoxList();
	void initLineEdit();
	void onClickGroupComboBox();
	void getMyGroupListRequestSuccess();
	void onClickPageComboBox();
	void getMyPageListRequestSuccess();
	void searchGameKeyword(const QString keyword);
	void gameTagListRequestSuccess();
	void hideSearchGameList();
	void showSearchGameList();
	void doUpdateOkState();
	void startLiving();
	void facebookLivingAndUpdatingDecline(PLSAPIFacebookType &type);
	void saveLiveInfo(PLSAPIFacebook::FacebookPrepareLiveInfo &oldPrepareInfo);
	bool isModified();
	void initPlaceTextHorderColor(QWidget *widget) const;
	void setupGameListCornerRadius();
	void showNetworkErrorAlert(PLSLiveInfoFacebookErrorType errorType);
	void getLivingTitleDescRequest();
	void getLivingTimelinePrivacy();

protected:
	void showEvent(QShowEvent *event) override;

private:
	Ui::PLSLiveInfoFacebook *ui;
	QFrame *m_frame;
	PLSPlatformFacebook *platform;
	QListWidget *m_gameListWidget;
	QList<QString> m_expiredObjectList;
	bool m_startLivingApi;
	bool m_showTokenAlert{false};
	PLSAPIFacebook::FacebookPrepareLiveInfo m_oldPrepareInfo;
};
#endif // PLSLIVEINFOFACEBOOK_H
