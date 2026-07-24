#ifndef PLSDIALOGVIEW_H
#define PLSDIALOGVIEW_H

#include <functional>
#include <QDialog>
#include <QFrame>
#include "libui.h"
#include "PLSToplevelView.h"

namespace Ui {
class PLSDialogView;
}

class QToolButton;

struct DialogInfo {
	int defaultWidth{300};
	int defaultHeight{800};
	int defaultOffset{5};
	int configId{-1};
};

class LIBUI_API PLSDialogView : public PLSToplevelView<QDialog> {
	Q_OBJECT
	Q_PROPERTY(int captionBarHeight READ getCaptionBarHeight WRITE setCaptionBarHeight)
	Q_PROPERTY(int captionHeight READ getCaptionHeight WRITE setCaptionHeight)
	Q_PROPERTY(int captionButtonSize READ getCaptionButtonSize WRITE setCaptionButtonSize)
	Q_PROPERTY(int captionButtonMargin READ getCaptionButtonMargin WRITE setCaptionButtonMargin)
	Q_PROPERTY(int textMarginLeft READ getTextMarginLeft WRITE setTextMarginLeft)
	Q_PROPERTY(int textMarginRight READ getTextMarginRight WRITE setTextMarginRight)
	Q_PROPERTY(int closeButtonMarginRight READ getCloseButtonMarginRight WRITE setCloseButtonMarginRight)
	Q_PROPERTY(bool hasCaption READ getHasCaption WRITE setHasCaption)
	Q_PROPERTY(bool hasHLine READ getHasHLine WRITE setHasHLine)
	Q_PROPERTY(bool hasMinButton READ getHasMinButton WRITE setHasMinButton)
	Q_PROPERTY(bool hasCloseButton READ getHasCloseButton WRITE setHasCloseButton)
	Q_PROPERTY(bool hasMaxResButton READ getHasMaxResButton WRITE setHasMaxResButton)
	Q_PROPERTY(bool maxState READ getMaxState)
	Q_PROPERTY(bool fullScreenState READ getFullScreenState)
	Q_PROPERTY(int captionButtonTopMargin READ getCaptionButtonTopMargin WRITE setCaptionButtonTopMargin)
	Q_PROPERTY(bool hasHelpButton READ getHasHelpButton WRITE setHasHelpButton)
	Q_PROPERTY(QColor titleBarBkgColor READ getTitleBarBkgColor WRITE setTitleBarBkgColor)

public:
	explicit PLSDialogView(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags(), CreateWinId createWinId = CreateWinId::DontCreate);
	explicit PLSDialogView(DialogInfo info, QWidget *parent = nullptr, CreateWinId createWinId = CreateWinId::DontCreate);

	~PLSDialogView() override;

	/**
	* @brief      get title bar view
	* @return     title bar view
	*/
	QWidget *titleBar() const;
	QToolButton *closeButton() const;

	/**
	* @brief      get content view
	* @return     content view
	*/
	QWidget *content() const;

	/**
	* @brief      get or set title widget
	*/
	QWidget *titleWidget() const;
	void setTitleWidget(QWidget *widget, std::function<void(QWidget *titleWidget, const QSize &size)> &&resizeCb = nullptr);

	/**
	* @brief      get or set content widget
	*/
	QWidget *widget() const;
	void setWidget(QWidget *widget);

	/**
	* @brief      get or set title bar height
	*/
	int getCaptionBarHeight() const;
	void setCaptionBarHeight(int captionBarHeight);

	/**
	* @brief      get or set title height
	*/
	int getCaptionHeight() const;
	void setCaptionHeight(int captionHeight);

	/**
	* @brief      get or set title bar close/minimized button's width/height
	*/
	int getCaptionButtonSize() const;
	void setCaptionButtonSize(int captionButtonSize);

	/**
	* @brief      get or set title bar close/minimized button's margin
	*/
	int getCaptionButtonMargin() const;
	void setCaptionButtonMargin(int captionButtonMargin);

	/**
	* @brief      get or set title bar text's left margin
	*/
	int getTextMarginLeft() const;
	void setTextMarginLeft(int textMarginLeft);

	/**
	* @brief      get or set title bar text's right margin
	*/
	int getTextMarginRight() const;
	void setTextMarginRight(int textMarginRight);

	/**
	* @brief      get or set title bar close button's right margin
	*/
	int getCloseButtonMarginRight() const;
	void setCloseButtonMarginRight(int closeButtonMarginRight);

	/**
	* @brief      get or set title bar show or not
	*/
	bool getHasCaption() const;
	void setHasCaption(bool hasCaption);

	/**
	* @brief      get or set vline show or not
	*/
	bool getHasHLine() const;
	void setHasHLine(bool hasHLine);

	/**
	* @brief      get or set minimized button show or not
	*/
	bool getHasMinButton() const;
	void setHasMinButton(bool hasMinButton);

	/**
	* @brief      get or set maxres button show or not
	*/
	bool getHasMaxResButton() const;
	void setHasMaxResButton(bool hasMaxResButton);

	/**
	* @brief      get or set close button show or not
	*/
	bool getHasCloseButton() const;
	void setHasCloseButton(bool hasCloseButton);

	/**
	* @brief      get or set help button show or not
	*/
	bool getHasHelpButton() const;
	void setHasHelpButton(bool hasHelpButton);

	/**
    * @brief      get or set title bar background color
    */
	QColor getTitleBarBkgColor() const;
	void setTitleBarBkgColor(QColor bkgColor);

	/**
	* @brief      set help button tooltip
	*/
	void setHelpButtonToolTip(QString tooltip, int y);
	/**
	* @brief      get or set close event callback
	*/
	void setCloseEventCallback(const std::function<bool(QCloseEvent *)> &closeEventCallback);
	void callBaseCloseEvent(QCloseEvent *event);

	/**
	* @brief      change dialog size to content
	* @param[inopt] parent      : minimized size
	*/
	void sizeToContent(const QSize &size = QSize());
	void setHeightForFixedWidth();

	bool getEscapeCloseEnabled() const;
	void setEscapeCloseEnabled(bool enabled);

	int getCaptionButtonTopMargin() const;
	void setCaptionButtonTopMargin(int captionButtonTopMargin);

	QWidget *titleLabel() const;
	void done(int) override;

	int titleBarHeight() const override;

	bool canMaximized() const;
	bool canFullScreen() const;
	bool getMaxState() const;
	bool getFullScreenState() const;
	void closeNoButton();
	void setHiddenWidget(QWidget *widget);
	void setNotRetainSizeWhenHidden(QWidget *widget);
	void addMacTopMargin(int defaultHeight = 20);
	/**
	* @brief when run exec(),  YES->use original parent. NO:use new parent if nullptr
	*/
	void setisUseOriginalParent(bool isUseOriginalParent) { m_isUseOriginalParent = isUseOriginalParent; }
	DialogInfo &getDialogInfo() { return defaultInfo; }

	void updateTitleBarLayout(const QSize &titleBarSize);

public slots:
	int exec() override;

signals:
	void shown();
	void resizing(const QSize &size);

private:
	void flushMaxFullScreenStateStyle();
	int getVisibleButtonWidth(QWidget *widget);

protected:
	void closeEvent(QCloseEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	bool eventFilter(QObject *watcher, QEvent *event) override;
	void windowStateChanged(QWindowStateChangeEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

	virtual bool closeButtonHook() { return true; }
	virtual int getTitleWidth(int titleWidth) const { return titleWidth; }

	template<typename Ui> void setupUi(Ui &ui)
	{
		ui->setupUi(content());
		QMetaObject::connectSlotsByName(this);
	}

private:
	Ui::PLSDialogView *ui = nullptr;
	QWidget *owidget = nullptr;
	QWidget *otitleWidget = nullptr;
	int captionBarHeight = 0;
	int captionHeight = 0;
	int captionButtonSize = 0;
	int captionButtonMargin = 0;
	int textMarginLeft = 8;
	int textMarginRight = 5;
	int closeButtonMarginRight = 0;
	int captionButtonTopMargin = 0;
	bool hasCaption = true;
	bool hasHLine = true;
	bool hasMinButton = false;
	bool hasMaxResButton = false;
	bool hasCloseButton = true;
	bool isEscapeCloseEnabled = false;
	bool hasHelpButton = false;
	std::function<bool(QCloseEvent *)> closeEventCallback;
	DialogInfo defaultInfo;
	bool m_isUseOriginalParent = false;
	QString helpTooltip;
	int helpTooltipX = 0;
	int helpTooltipY = 0;
	QColor m_bkgColor;
	std::function<void(QWidget *titleWidget, const QSize &size)> titleWidgetResizeCb = nullptr;
};

#endif // PLSDIALOGVIEW_H
