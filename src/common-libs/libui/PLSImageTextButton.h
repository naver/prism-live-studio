#pragma once
#include <QPushButton>
#include <qwidget.h>
#include <QLabel>
#include <QFont>

#include "libui-globals.h"

class QSpacerItem;

class LIBUI_API PLSImageTextButton : public QPushButton {
	Q_OBJECT

public:
	explicit PLSImageTextButton(QWidget *parent = nullptr);

	void setLabelText(const QString &str);
	void setFileButtonEnabled(bool enabled);

	const QFont &getRightFont() const;

	void setWordWrap(bool trap);
	void seIsLeftAlign(bool isLeft);

protected:
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	QLabel *m_labelRight;
	QSpacerItem *m_leftSpacer;
};

class PLSBorderButton : public QPushButton {
	Q_OBJECT

public:
	explicit PLSBorderButton(QWidget *parent = nullptr);
	QLabel *m_boderLabel;
};
