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

	void setLabelText(const QString &str, bool isElidedText = false);
	void setFileButtonEnabled(bool enabled);

	const QFont &getRightFont() const;

	void setWordWrap(bool trap);
	void seIsLeftAlign(bool isLeft);

	void onlyHideContent(bool hide);

protected:
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void resizeEvent(QResizeEvent *) override;

private:
	QLabel *m_labelLeft;
	QLabel *m_labelRight;
	QSpacerItem *m_leftSpacer;

	QString m_oriRightText{};
	bool m_isElidedText{false};

	void elidedLabelText();
};

class PLSBorderButton : public QPushButton {
	Q_OBJECT

public:
	explicit PLSBorderButton(QWidget *parent = nullptr);
	QLabel *m_boderLabel;
};
