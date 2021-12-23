#pragma once
#include <QPushButton>
#include <qwidget.h>
#include <QLabel>

class PLSImageTextButton : public QPushButton {
	Q_OBJECT

public:
	explicit PLSImageTextButton(QWidget *parent = nullptr);
	~PLSImageTextButton(){};

	void setLabelText(const QString &str);
	void setFileButtonEnabled(bool enabled);

protected:
	void enterEvent(QEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	QLabel *m_labelRight;
};

class PLSBorderButton : public QPushButton {
	Q_OBJECT

public:
	explicit PLSBorderButton(QWidget *parent = nullptr);
	~PLSBorderButton(){};

	QLabel *m_boderLabel;
};
