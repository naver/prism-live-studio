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
class PLSQCefWidget;

class PLSChatDialog : public QWidget {
	Q_OBJECT

public:
	explicit PLSChatDialog(QWidget *parent = nullptr);
	~PLSChatDialog() override;
	void quitAppToReleaseCefData();

private:
	struct ChatDatas {
		QPointer<QWidget> widget;
		QPointer<QPushButton> button;
		std::string url;
		bool isWebLoaded = false;
	};

	Ui::PLSChatDialog *ui;
	void setupFirstUI();
	void refreshTabButtonCount();
	void hideOrShowTabButton();
	void setupFirstRtmpUI(QWidget *parent);
	int rtmpChannelCount() const;
	PLSQCefWidget *createANewCefWidget(const std::string &url, int index);

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
	void updateTopAddSourceText();

	QCefWidget *getCefWidgetByWidget(const ChatDatas &data);
	void updateTabBtnCss();
	std::wstring getSendWebData(const QJsonObject &data);
signals:
	void chatShowOrHide(bool show);
	void fontBtnDisabled();

private slots:
	void changedSelectIndex(int index, bool isClicked = false);
	void channelRemoveToDeleteCef(int index);
	void facebookPrivateChatChanged(bool oldPrivate, bool newPrivate);
	//PRISM/Zhangdewen/20200921/#/add chat source button
	void chatSourceButtonClicked() const;
	void fontZoomButtonClicked();

	void updateFontBtnStatus();

	void urlDidChanged(const QString &url);
	void titleChanged(const QString &title);

	void refreshUI();
	void youtubePrivateChange();
	void recvLocalChatWebMsg(const QString &type, const QString &msg);

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
	QCefCookieManager *chat_panel_cookies = nullptr;

	QPointer<QLabel> m_rtmpPlaceTextLabel;

	//PRISM/Zhangdewen/20200921/#/add chat source button
	QWidget *m_chatSourceButtonOnePlatform = nullptr;
	QWidget *m_chatSourceButtonNoPlatform = nullptr;
	QPointer<QWidget> m_chatSourceBtn = nullptr;

	QPointer<QPushButton> m_fontChangeBtn = nullptr;

	std::vector<ChatDatas> m_vecChatDatas;
	bool m_isForceRefresh = true;
	int m_selectIndex = ChatPlatformIndex::UnDefine;
	QSpacerItem *m_rightHorizontalSpacer;

	bool m_bShowToastAgain = false;
	bool m_bRefeshYoutubeChat = false;
	bool m_chatActionSelect = false;

	QPointer<QFrame> m_maskWidget = nullptr;
};

#endif // PLSCHATDIALOG_H
