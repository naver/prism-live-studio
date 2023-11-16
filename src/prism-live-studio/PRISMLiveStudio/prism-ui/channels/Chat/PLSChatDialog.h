#ifndef PLSCHATDIALOG_H
#define PLSCHATDIALOG_H

#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSpacerItem>
#include <QUrl>
#include <browser-panel.hpp>
#include <vector>
#include "PLSChatHelper.h"
#include "PLSSideBarDialogView.h"

namespace Ui {
class PLSChatDialog;
}

class PLSChatDialog : public QWidget {
	Q_OBJECT
	//Q_PROPERTY(bool btnSelected READ getHasCloseButton WRITE setHasCloseButton)

public:
	explicit PLSChatDialog(QWidget *parent = nullptr);
	~PLSChatDialog() override;
	void quitAppToReleaseCefData();

private:
	struct ChatDatas {
		QPointer<QWidget> widget;
		QPushButton *button;
		std::string url;
		bool isWebLoaded = false;
	};

	Ui::PLSChatDialog *ui;
	void setupFirstUI();
	void refreshTabButtonCount();
	void hideOrShowTabButton();
	void setupFirstRtmpUI(QWidget *parent);
	void refreshUI();
	int rtmpChannelCount() const;
	QCefWidget *createANewCefWidget(const std::string &url, int index);

	void setupNewUrl(int index, const std::string &url, bool forceSet = false);
	int foundFirstShowedButton();

	void updateRtmpPlaceText();
	void updateNewUrlByIndex(int index, const QVariantMap &info, bool forceSet = false);

	void createToasWidget();
	void showToastIfNeeded();
	void switchStackWidget(int index);

	void updateYoutubeUrlIfNeeded(bool forceSet = false);

	void setSelectIndex(int index);

	//PRISM/Zhangdewen/20200921/#/add chat source button
	QWidget *createChatSourceButton(QWidget *parent, bool noPlatform);
	void forceResizeDialog();

	int getShownBtnCount();
	void updateTabPolicy();

signals:
	void chatShowOrHide(bool show);
	void fontBtnDisabled();

private slots:
	void changedSelectIndex(int index);
	void channelRemoveToDeleteCef(int index);
	void facebookPrivateChatChanged(bool oldPrivate, bool newPrivate);
	//PRISM/Zhangdewen/20200921/#/add chat source button
	void chatSourceButtonClicked() const;
	void fontZoomButtonClicked();

	void updateFontBtnStatus();

	void urlDidChanged(const QString &url);
	void titleChanged(const QString &title);

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

	bool eventFilter(QObject *, QEvent *) override;
	void wheelEvent(QWheelEvent *event) override;
	void resizeEvent(QResizeEvent *) override;

	void adjustToastSize(int dialogWidth = 0);

private:
	QPointer<QLabel> m_pLabelToast;
	QPointer<QFrame> m_pTestFrame = nullptr;
	QPushButton *m_pButtonToastClose = nullptr;
	QCefCookieManager *chat_panel_cookies = nullptr;

	QLabel *m_rtmpPlaceTextLabel;

	//PRISM/Zhangdewen/20200921/#/add chat source button
	QWidget *m_chatSourceButtonOnePlatform = nullptr;
	QWidget *m_chatSourceButtonNoPlatform = nullptr;

	QPushButton *m_fontChangeBtn = nullptr;

	std::vector<ChatDatas> m_vecChatDatas;
	bool m_isForceRefresh = true;
	int m_selectIndex = ChatPlatformIndex::UnDefine;
	QSpacerItem *m_rightHorizontalSpacer;

	bool m_bShowToastAgain = false;
	bool m_bRefeshYoutubeChat = false;
	QMetaObject::Connection facebookPrivateConn;
	QMetaObject::Connection youtubeIDConn;
	QMetaObject::Connection youtubePrivateConn;
};

#endif // PLSCHATDIALOG_H
