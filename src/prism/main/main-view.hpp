#ifndef PLSMAINVIEW_H
#define PLSMAINVIEW_H

#include <functional>
#include <QFrame>
#include <QMenu>
#include <QListWidgetItem>
#include "PLSLivingMsgView.hpp"
#include "frontend-api.h"
#include "PLSToplevelView.hpp"
#include "PLSUSBWiFiHelpView.h"

namespace Ui {
class PLSMainView;

}

class QPushButton;
class PLSBasicStatusBar;
class PLSToastView;
class CustomHelpMenuItem;
class QLabel;
class PLSToastMsgPopup;
class PLSToastButton;
class PLSBasic;
class QButtonGroup;
class ResolutionGuidePage;

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

public:
	explicit PLSMainView(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSMainView();

public:
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

public:
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

	void setCloseEventCallback(std::function<void(QCloseEvent *)> closeEventCallback);
	void callBaseCloseEvent(QCloseEvent *event);

	void setStudioMode(bool studioMode);

	void toastMessage(pls_toast_info_type type, const QString &message, int autoClose);
	void toastMessage(pls_toast_info_type type, const QString &message, const QString &url, const QString &replaceStr, int autoClose);

	void toastClear();

	void setUserButtonIcon(const QIcon &icon);

	QRect &getGeometryOfNormal() { return geometryOfNormal; }

	void initToastMsgView(bool isInitShow = true);
	void initMobileHelperView(bool isInitShow);

	QRect titleBarRect() const;
	bool canMaximized() const;
	bool canFullScreen() const;
	void flushMaxFullScreenStateStyle();
	void onMaxFullScreenStateChanged();
	void onSaveNormalGeometry();

	bool isClosing() const;
	void close();
	void OnBeautySourceDownloadFinished();
	void registerSideBarButton(ConfigId id, const IconData &data, bool inFixedArea = true);
	void updateSideBarButtonStyle(ConfigId id, bool on);

	QList<SideWindowInfo> getSideWindowInfo() const;
	int getToastMessageCount() const;
	void setSidebarWindowVisible(int windowId, bool visible);

public slots:
	//void onUpdateChatSytle(bool isShow);

private:
	static void resetToastViewPostion(pls_frontend_event event, const QVariantList &params, void *context);
	void helpMenuAboutToHide();
	void initSideBarButtons();
	void addSideBarSeparator(bool inFixedArea = true);
	void addSideBarStretch();
	QString generalStyleSheet(const QString &objectName, IconData data);
	QWidget *getSiderBarButton(const QString &objName);
	void toggleResolutionButton(bool isVisible);
	void AdjustSideBarMenu();

signals:
	void popupSettingView(const QString &tab, const QString &group);
	void studioModeChanged();
	void isshowSignal(bool isShow);
	void beginResizeSignal();
	void endResizeSignal();
	void maxFullScreenStateChanged(bool isMaxState, bool isFullScreenState);
	void BeautySourceDownloadFinished();
	void visibleSignal(bool visible);
	void virtualButtonClicked();
	void sideBarButtonClicked(int id);

	void mainViewUIChanged();

public slots:
	void on_menu_clicked();
	void on_user_clicked();
	void on_studioMode_clicked();
	void on_chat_clicked();
	void on_alert_clicked();

	void showResolutionGuidePage(bool visible = true);

	void showResolutionTips(const QString &platform);

	void on_help_clicked();
	void on_settings_clicked();
	void on_listWidget_itemClicked(QListWidgetItem *item);
	void onSideBarButtonClicked(int buttonId);

	void wifiHelperclicked();
	void on_mobile_lost();
	void on_mobile_disconnect();
	void on_mobile_connected(QString mobileName); //Not use reference param, it may be invoked in different thread.

protected:
	void closeEvent(QCloseEvent *event);
	void showEvent(QShowEvent *event);
	void hideEvent(QHideEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	bool eventFilter(QObject *watcher, QEvent *event);

private:
	Ui::PLSMainView *ui;
	int captionHeight;
	int captionButtonSize;
	int captionButtonMargin;
	int logoMarginLeft;
	int menuButtonMarginLeft;
	QSize menuButtonSize;
	int closeButtonMarginRight;
	int rightAreaWidth;
	int bottomAreaHeight;
	int channelsAreaHeight;
	CustomHelpMenuItem *checkMenuItem;
	QListWidgetItem *checkListWidgetItem;
	std::function<void(QCloseEvent *)> closeEventCallback;
	QMap<qint64, QString> toastMessages;
	QMenu *helpMenu;
	PLSToastView *toast;
	PLSLivingMsgView *m_livingMsgView;
	PLSToastMsgPopup *m_toastMsg;
	bool m_livingToastViewShow;
	bool m_IsChatShowWhenRebackLogin = false;
	bool closing = false;
	QButtonGroup *sideBarBtnGroup = nullptr;
	QList<SideWindowInfo> windowInfo;

	friend class PLSBasic;
	friend class ResolutionGuidePage;
	PLSUSBWiFiHelpView *m_usbWiFiView;
};

#endif // PLSMAINVIEW_H
