#ifndef PLSMAINVIEW_H
#define PLSMAINVIEW_H

#include <functional>
#include <QFrame>
#include <QMenu>
#include <QListWidgetItem>
#include <PLSToplevelView.h>
#include <frontend-api.h>
#if defined(_WIN32)
#include "windows/PLSWinrtNotify.h"
#elif defined(Q_OS_MACOS)
#include "mac/PLSMacSleepNotify.h"
#endif

namespace Ui {
class PLSMainView;

}

class QPushButton;
class PLSBasicStatusBar;
class CustomHelpMenuItem;
class QLabel;
class QVBoxLayout;
class QSpacerItem;
class PLSToastMsgPopup;
class PLSToastButton;
class PLSBasic;
class QButtonGroup;
class ResolutionGuidePage;
class ResolutionTipFrame;
class PLSLivingMsgView;
class PLSChatDialog;
class PLSAlertView;
class PLSLoadingView;
class QTimer;
class VScrollArea;

class PLSMainView : public PLSToplevelView<QFrame> {
	Q_OBJECT
	Q_PROPERTY(int captionHeight READ getCaptionHeight WRITE setCaptionHeight)
	Q_PROPERTY(int captionButtonSize READ getCaptionButtonSize WRITE setCaptionButtonSize)
	Q_PROPERTY(int captionButtonMargin READ getCaptionButtonMargin WRITE setCaptionButtonMargin)
	Q_PROPERTY(int logoMarginLeft READ getLogoMarginLeft WRITE setLogoMarginLeft)
	Q_PROPERTY(int menuButtonMarginLeft READ getMenuButtonMarginLeft WRITE setMenuButtonMarginLeft)
	Q_PROPERTY(QSize menuButtonSize READ getMenuButtonSize WRITE setMenuButtonSize)
	Q_PROPERTY(int closeButtonMarginRight READ getCloseButtonMarginRight WRITE setCloseButtonMarginRight)
	Q_PROPERTY(int rightAreaWidth READ getRightAreaWidth WRITE setRightAreaWidth)
	Q_PROPERTY(int bottomAreaHeight READ getBottomAreaHeight WRITE setBottomAreaHeight)
	Q_PROPERTY(int channelsAreaHeight READ getChannelsAreaHeight WRITE setChannelsAreaHeight)
	Q_PROPERTY(bool maxState READ getMaxState)
	Q_PROPERTY(bool fullScreenState READ getFullScreenState)
	Q_PROPERTY(QColor titleBarBkgColor READ getTitleBarBkgColor WRITE setTitleBarBkgColor)

public:
	explicit PLSMainView(QWidget *parent = nullptr);
	~PLSMainView() override;

public:
	static PLSMainView *instance();

	//public:
	struct IconData {
		QString iconOffNormal;
		QString iconOffHover;
		QString iconOffPress;
		QString iconOffDisabled;

		QString iconOnNormal;
		QString iconOnHover;
		QString iconOnPress;
		QString iconOnDisabled;

		QString tooltip;

		int minWidth{26};
		int minHeight{26};
		int maxWidth{26};
		int maxHeight{26};
	};

	//public:
	QWidget *content() const;
	QWidget *channelsArea() const;

	QPushButton *menuButton() const;
	PLSBasicStatusBar *statusBar() const;

	int getCaptionHeight() const;
	void setCaptionHeight(int captionHeight);

	int getCaptionButtonSize() const;
	void setCaptionButtonSize(int captionButtonSize);

	int getCaptionButtonMargin() const;
	void setCaptionButtonMargin(int captionButtonMargin);

	int getLogoMarginLeft() const;
	void setLogoMarginLeft(int logoMarginLeft);

	int getMenuButtonMarginLeft() const;
	void setMenuButtonMarginLeft(int menuButtonMarginLeft);

	QSize getMenuButtonSize() const;
	void setMenuButtonSize(const QSize &menuButtonSize);

	int getCloseButtonMarginRight() const;
	void setCloseButtonMarginRight(int closeButtonMarginRight);

	int getRightAreaWidth() const;
	void setRightAreaWidth(int rightAreaWidth);

	int getBottomAreaHeight() const;
	void setBottomAreaHeight(int bottomAreaHeight);

	int getChannelsAreaHeight() const;
	void setChannelsAreaHeight(int channelsAreaHeight);

	QColor getTitleBarBkgColor() const;
	void setTitleBarBkgColor(QColor bkgColor);

	void setCloseEventCallback(const std::function<void(QCloseEvent *)> &closeEventCallback);
	void callBaseCloseEvent(QCloseEvent *event);
	void close();

	bool getMaxState() const;
	bool getFullScreenState() const;

	void onSaveGeometry() const;
	void onRestoreGeometry() override;

	int titleBarHeight() const override;
	void setUpdateTipsStatus(bool isShowTips);

	void registerSideBarButton(ConfigId id, const IconData &data, bool isAddLoading = false);
	void updateSideBarButtonStyle(ConfigId id, bool on);
	void updateSidebarButtonTips(ConfigId id, const QString &tips = QString()) const;
	void updateLoadingState(ConfigId id, bool visible);
	void blockSidebarButton(ConfigId id, bool toBlock = true) const;
	bool isSidebarButtonInScroll(ConfigId id) const;
	QList<SideWindowInfo> getSideWindowInfo() const;
	bool setSidebarWindowVisible(int windowId, bool visible);
	bool setSidebarButtonVisible(int windowId, bool visible);
	bool setSidebarButtonEnabled(int windowId, bool enabled);
	void setStudioModeEnabled(bool enabled);
	void setStudioModeDimmed(bool bValue);

	Q_INVOKABLE void toastMessage(pls_toast_info_type type, const QString &message, int autoClose);
	Q_INVOKABLE void toastMessage(pls_toast_info_type type, const QString &message, const QString &url, const QString &replaceStr, int autoClose);
	int getToastMessageCount() const;
	Q_INVOKABLE void toastClear();

	void setUserButtonIcon(const QIcon &icon);
	void initToastMsgView(bool isInitShow = true);
	void setToastMsgViewVisible(bool isShow);
	void initMobileHelperView(bool isInitShow);

	void setStudioMode(bool studioMode);
	bool isFirstShow() const;
	void setSettingIconCheck(bool bCheck);
	void setResolutionBtnCheck(bool bCheck);

	bool isSettingEnabled() const;
	static bool isMatchOBSSupportPluginVersion(const QString &winObsPath = "");

public slots:
	void onSideBarButtonClicked(int buttonId);
	void updateTipsEnableChanged(bool isEnable);
	void updateAppView();
	void on_alert_clicked();
	bool alert_message_visible() const;
	int alert_message_count() const;
	void on_ResolutionBtn_clicked();
	void on_chat_clicked() const;

	void showChatView(bool isOnlyShow = false, bool isOnlyInit = false) const;

	void on_settings_clicked();
	void on_help_clicked();
	void on_listWidget_itemClicked(const QListWidgetItem *item);
	void helpMenuAboutToHide();
	void on_user_clicked();
	void showResolutionTips(const QString &platform);
	void showVirtualCameraTips(const QString &tips = QString());
	void showStudioModeTips(const QString &tips = QString());
	void setStudioModeChecked(bool);
	void closeMobileDialog() const;
	void setNoticeTips(bool hasNoticeOrUpdateMsg);
	void consumeNoticeMenuBadge();
	void armNoticeTips(bool hasNoticeOrUpdateMsg);

protected:
	ResolutionTipFrame *createSidebarTipFrame(const QString &txt, QWidget *aliginWidget, bool isAutoColose, const QString &objectName = "ResolutionTipsLabel");
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	void windowStateChanged(QWindowStateChangeEvent *event) override;
	bool eventFilter(QObject *watcher, QEvent *event) override;
	bool event(QEvent *event) override;
	bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
	void winIdChanged(WId winId) override;

signals:
	void popupSettingView(const QString &tab, const QString &group);
	void sideBarButtonClicked(int id);
	void studioModeChanged();
	void isshowSignal(bool);
	void mainViewUIChanged();
	void onGolivePending(bool bPending);

private:
	void initSideBarButtons();
	/** Build right sidebar: scroll (#side_bar_menus: profile, studio, feature buttons, …), bottom strip. */
	void setupRightSidebarWidgets();
	void createSidebarUtilityButtons();
	void initHelpMenu();
	/** Min width from stylesheet (212); widen to fit longest help row (locale, NEW badge). */
	void updateHelpMenuGeometry();
	QString generalStyleSheet(const QString &objectName, IconData data) const;
	QWidget *getSiderBarButton(const QString &objName);
	void addSideBarSeparator();
	void addSideBarStretch();
	void AdjustSideBarMenu();
	void hiddenWidget(QWidget *widget);
	void setUiStepLogParams();
	void setHelpMenuNoticeBadgeVisible(bool visible);
	void applyNoticeBadgeState();

#ifdef _WIN32
	std::shared_ptr<int> freeHandle = nullptr;
	void readDetectResult();
	void runNewDetect();
#endif

private:
	Ui::PLSMainView *ui = nullptr;
	int captionHeight = 40;
	int captionButtonSize = 0;
	int captionButtonMargin = 0;
	int logoMarginLeft = 8;
	int menuButtonMarginLeft = 16;
	QSize menuButtonSize{34, 26};
	int closeButtonMarginRight = 8;
	int rightAreaWidth = 30;
	int bottomAreaHeight = 30;
	int channelsAreaHeight = 70;
	std::function<void(QCloseEvent *)> closeEventCallback;
	QButtonGroup *sideBarBtnGroup = nullptr;
	QMap<ConfigId, PLSLoadingView *> m_LoadingFrameMap;
	QList<SideWindowInfo> windowInfo;

	QPointer<PLSLivingMsgView> m_livingMsgView;
	PLSToastMsgPopup *m_toastMsg;
	QMap<qint64, QString> toastMessages;
	QMenu *helpMenu;
	QListWidget *m_helpListWidget{nullptr};

	int m_showTimes = 0;
	bool m_requestUpdate = false;

	QColor m_bkgColor;

	// mobile source
	QPointer<PLSAlertView> m_pAlertViewMobileLost;
	QPointer<PLSAlertView> m_pAlertViewMobileDisconnect;
	bool m_isFirstShow = true;
	friend class PLSBasic;
	friend class ResolutionGuidePage;

	/* Right sidebar chrome (code-built; object names match PLSMainView.css / autoConnect) */
	QPushButton *m_userBtn{nullptr};
	QPushButton *m_studioModeBtn{nullptr};
	VScrollArea *m_scrollArea{nullptr};
	QWidget *m_sideBarMenus{nullptr};
	QVBoxLayout *m_sideBarLayout{nullptr};
	QLabel *m_separatorBottom{nullptr};
	QWidget *m_bottomStack{nullptr};
	QVBoxLayout *m_bottomLayout{nullptr};

	/** Top spacer above sidebar scroll (#rightArea) */
	QSpacerItem *m_raSpacerTop{nullptr};

	/* Sidebar: created in code (not in .ui); object names match PLSMainView.css */
	/** Dynamic replacements for former ui->ResolutionBtn / settings / help; objectName unchanged for CSS. */
	QPushButton *m_resolutionBtn = nullptr;
	QPushButton *m_settingsBtn = nullptr;
	QPushButton *m_helpBtn = nullptr;
#if defined(_WIN32)
	QPointer<PLSWinRTNotify> winrt_notify;
#elif defined(Q_OS_MACOS)
	QPointer<PLSMacSleepNotify> mac_sleep_notify;
#endif
	QPointer<QLabel> m_noticeTipsIcon;
};

#endif // PLSMAINVIEW_H
