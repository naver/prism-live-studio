#ifndef PLSMAINVIEW_H
#define PLSMAINVIEW_H

#include <functional>
#include <QFrame>
#include <QMenu>
#include <QListWidgetItem>
#include "PLSLivingMsgView.hpp"
#include "frontend-api.h"
#include "PLSToplevelView.hpp"

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
	explicit PLSMainView(QWidget *parent = nullptr);
	~PLSMainView();

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
	void toastClear();

	void setUserButtonIcon(const QIcon &icon);

	void initToastMsgView();

	PLSToastButton *getToastButton();

	void showMainFloatView();
	void hideMainFloatView();

	QRect titleBarRect() const;
	bool canMaximized() const;
	bool canFullScreen() const;
	void flushMaxFullScreenStateStyle();
	void onMaxFullScreenStateChanged();
	void onSaveNormalGeometry();

	bool isClosing() const;
	void close();

public slots:
	void onUpdateChatSytle(bool isShow);

private:
	static void resetToastViewPostion(pls_frontend_event event, const QVariantList &params, void *context);

signals:
	void popupSettingView(const QString &tab, const QString &group);
	void studioModeChanged();
	void isshowSignal(bool isShow);
	void beginResizeSignal();
	void endResizeSignal();

private slots:
	void on_menu_clicked();
	void on_user_clicked();
	void on_studioMode_clicked();
	void on_chat_clicked();
	void on_alert_clicked();
	void on_settings_clicked();

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
	PLSToastView *toast;
	bool m_isFirstLogin;
	PLSLivingMsgView m_livingMsgView;
	PLSToastMsgPopup *m_toastMsg;
	bool m_livingToastViewShow;
	bool closing = false;

	friend class PLSBasic;
};

#endif // PLSMAINVIEW_H
