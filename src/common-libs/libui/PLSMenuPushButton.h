#ifndef PLSMENUPUSHBUTTON_H
#define PLSMENUPUSHBUTTON_H

#include <QPushButton>
#include <QEvent>

#include "libui.h"

class LIBUI_API PLSMenuPushButton : public QPushButton {
	Q_OBJECT
	// no need to use 'hdpi' flag
	Q_PROPERTY(int leftSpacing READ LeftSpacing WRITE SetLeftSpacing)
	Q_PROPERTY(int rightSpacing READ RightSpacing WRITE SetRightSpacing)
	Q_PROPERTY(int spacing READ Spacing WRITE SetSpacing)
public:
	explicit PLSMenuPushButton(QWidget *parent = nullptr, bool needTipsDelegate = false);
	void SetText(const QString &text);
	QString GetText() const;
	QMenu *GetPopupMenu();

	int LeftSpacing() const;
	void SetLeftSpacing(int v);

	int RightSpacing() const;
	void SetRightSpacing(int v);

	int Spacing() const;
	void SetSpacing(int v);

	int ContentWidth() const;

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void leaveEvent(QEvent *event) override;

private:
	void setMouseStatus(const char *status);
	void asyncUpdate();
	void checkNeedUpdate();

signals:
	void PopupClicked();
	void ResizeMenu();
	void ShowMenu();
	void HideMenu();

private:
	QPushButton *popupBtn{nullptr};
	QWidget *tipsDelegate{nullptr};
	QMenu *popupMenu{nullptr};
	QString name;
	QRect contentRect;
	int lastStatus = QEvent::Leave;
	int menuBtnX = 0;
	int leftSpacing = 6;
	int rightSpacing = 6;
	int spacing = 6;
	bool needUpdate = true;
};

#endif