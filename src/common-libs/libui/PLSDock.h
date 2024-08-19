#pragma once

#include <QDockWidget>
#include <QMargins>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QAction>
#include <QTimer>

#include "PLSWidgetCloseHook.h"

class QMenu;
class PLSDock;
class PLSIconButton;

class LIBUI_API PLSDockTitle : public QFrame {
	Q_OBJECT
	Q_PROPERTY(int captionHeight READ getCaptionHeight WRITE setCaptionHeight)
	Q_PROPERTY(int marginLeft READ getMarginLeft WRITE setMarginLeft)
	Q_PROPERTY(int marginRight READ getMarginRight WRITE setMarginRight)
	Q_PROPERTY(int contentSpacing READ getContentSpacing WRITE setContentSpacing)

public:
	explicit PLSDockTitle(PLSDock *parent = nullptr);
	~PLSDockTitle() override;

	int getCaptionHeight() const;
	void setCaptionHeight(int captionHeight);

	int getMarginLeft() const;
	void setMarginLeft(int marginLeft);

	int getMarginRight() const;
	void setMarginRight(int marginRight);

	int getContentSpacing() const;
	void setContentSpacing(int contentSpacing);

	QList<QToolButton *> getButtons() const;
	PLSIconButton *getAdvButton() const;

	void setButtonActions(QList<QAction *> actions);

	void setAdvButtonMenu(QMenu *menu);
	void setAdvButtonActions(QList<QAction *> actions);
	void addAdvButtonMenu(QMenu *menu);
	QList<QAction *> GetAdvButtonActions() const;
	void setAdvButtonActionsEnabledByObjName(const QString &objName, bool enable);

	void setWidgets(QList<QWidget *> widgets);
	void setTitleWidgets(QList<QWidget *> widgets);

	void setCloseButtonVisible(bool visible);
	void setHasCloseButton(bool has);

	void updateTitle(const QString &title = QString());

private:
	void setButtonPropertiesFromAction(QToolButton *button, const QAction *action) const;

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void showEvent(QShowEvent *event) override;
	QSize sizeHint() const override { return minimumSizeHint(); }
	QSize minimumSizeHint() const override;

private:
	PLSDock *dock;
	int captionHeight;
	int marginLeft{0};
	int marginRight{0};
	int contentSpacing{10};
	QLabel *titleLabel;
	QList<QToolButton *> buttons;
	QList<QWidget *> titleWidgets;
	QList<QAction *> buttonActions;
	PLSIconButton *advButton;
	QMenu *advButtonMenu{nullptr};
	QHBoxLayout *buttonsLayout;
	QHBoxLayout *titleHLayout{nullptr};
	QToolButton *closeButton{nullptr};
	bool hasCloseButton{false};
};

class LIBUI_API PLSDock : public PLSWidgetCloseHookQt<QDockWidget> {
	Q_OBJECT
	Q_PROPERTY(bool moving READ isMoving WRITE setMoving)
	Q_PROPERTY(bool movingAndFloating READ isMovingAndFloating)
	Q_PROPERTY(int contentMarginLeft READ getContentMarginLeft WRITE setContentMarginLeft)
	Q_PROPERTY(int contentMarginTop READ getContentMarginTop WRITE setContentMarginTop)
	Q_PROPERTY(int contentMarginRight READ getContentMarginRight WRITE setContentMarginRight)
	Q_PROPERTY(int contentMarginBottom READ getContentMarginBottom WRITE setContentMarginBottom)

public:
	explicit PLSDock(QWidget *parent = nullptr);

	bool isMoving() const;
	void setMoving(bool moving);
	void setChangeState(bool toChange = true) { isChangeTopLevel = toChange; }

	bool isMovingAndFloating() const;

	int getContentMarginLeft() const;
	void setContentMarginLeft(int contentMarginLeft);

	int getContentMarginTop() const;
	void setContentMarginTop(int contentMarginTop);

	int getContentMarginRight() const;
	void setContentMarginRight(int contentMarginRight);

	int getContentMarginBottom() const;
	void setContentMarginBottom(int contentMarginBottom);

	PLSDockTitle *titleWidget() const;

	QWidget *widget() const;
	void setWidget(QWidget *widget);

	QWidget *getContent() const;

	void printChatGeometryLog(const QString &preLog);

signals:
	void doubleClicked();

private slots:
	void delayReleaseState();

protected:
	void closeEvent(QCloseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	bool event(QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	Q_DISABLE_COPY(PLSDock)
	PLSDockTitle *dockTitle{nullptr};
	bool moving{false};
	int contentMarginLeft{0};
	int contentMarginTop{0};
	int contentMarginRight{0};
	int contentMarginBottom{0};
	QFrame *content{nullptr};
	QHBoxLayout *contentLayout{nullptr};
	QWidget *owidget{nullptr};
	QRect geometryOfNormal;
	QSharedPointer<QTimer> delayTimer = nullptr;
	bool isChangeTopLevel = false;
	QTimer mouseReleaseChecker;

	friend class PLSDockTitle;
	friend class PLSWidgetResizeHandler;
};
