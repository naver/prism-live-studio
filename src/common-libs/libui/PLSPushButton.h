#ifndef PLSPUSHBUTTON_H
#define PLSPUSHBUTTON_H

#include <QPushButton>
#include <QTimer>
#include <qevent.h>
#include "libui.h"
#include "PLSUIApp.h"

class LIBUI_API PLSCategoryButton : public QPushButton {
	Q_OBJECT
public:
	explicit PLSCategoryButton(QWidget *parent = nullptr) : QPushButton(parent) { setCursor(Qt::PointingHandCursor); }
	~PLSCategoryButton() override = default;

	void SetDisplayText(const QString &text)
	{
		QPushButton::setText(text);
		QTimer::singleShot(0, this, [this]() { UpdateSize(); });
	}

private:
	void UpdateSize()
	{
		int textWidth = this->fontMetrics().boundingRect(text()).width();
		this->setFixedWidth(30 + textWidth);
	}
};

class LIBUI_API PLSPushButton : public QPushButton {
	Q_OBJECT
public:
	using QPushButton::QPushButton;
	explicit PLSPushButton(const QString &text_, QWidget *parent = nullptr) : QPushButton(parent), text(text_) {}

	void setText(const QString &text);

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	QString GetNameElideString() const;
	QString text;
};

class LIBUI_API PLSIconButton : public QPushButton {
	Q_OBJECT
	Q_PROPERTY(QString iconOffNormal READ iconOffNormal WRITE setIconOffNormal)
	Q_PROPERTY(QString iconOffHover READ iconOffHover WRITE setIconOffHover)
	Q_PROPERTY(QString iconOffPressed READ iconOffPressed WRITE setIconOffPressed)
	Q_PROPERTY(QString iconOffDisabled READ iconOffDisabled WRITE setIconOffDisabled)

	Q_PROPERTY(QString iconOnNormal READ iconOnNormal WRITE setIconOnNormal)
	Q_PROPERTY(QString iconOnHover READ iconOnHover WRITE setIconOnHover)
	Q_PROPERTY(QString iconOnPressed READ iconOnPressed WRITE setIconOnPressed)
	Q_PROPERTY(QString iconOnDisabled READ iconOnDisabled WRITE setIconOnDisabled)

	Q_PROPERTY(int marginLeft READ marginLeft WRITE setMarginLeft)
	Q_PROPERTY(int marginRight READ marginRight WRITE setMarginRight)

	Q_PROPERTY(int iconWidth READ iconWidth WRITE setIconWidth)
	Q_PROPERTY(int iconHeight READ iconHeight WRITE setIconHeight)
public:
	explicit PLSIconButton(QWidget *parent = nullptr);

	QString iconOffNormal() const;
	void setIconOffNormal(const QString &icon);

	QString iconOffHover() const;
	void setIconOffHover(const QString &icon);

	QString iconOffPressed() const;
	void setIconOffPressed(const QString &icon);

	QString iconOffDisabled() const;
	void setIconOffDisabled(const QString &icon);

	QString iconOnNormal() const;
	void setIconOnNormal(const QString &icon);

	QString iconOnHover() const;
	void setIconOnHover(const QString &icon);

	QString iconOnPressed() const;
	void setIconOnPressed(const QString &icon);

	QString iconOnDisabled() const;
	void setIconOnDisabled(const QString &icon);

	int iconWidth() const;
	void setIconWidth(int width);

	int iconHeight() const;
	void setIconHeight(int height);

	int marginLeft() const;
	void setMarginLeft(int margin);

	int marginRight() const;
	void setMarginRight(int margin);

protected:
	bool event(QEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

protected:
	void setIconFile(const QString &icon, PLSUiApp::Icon type);
	int m_iconWidth = 22;
	int m_iconHeight = 22;
	int m_state = PLSUiApp::UncheckedNormal;
	bool m_hoverd = false;
	int m_marginLeft = 1;
	int m_marginRight = 1;
	QPair<QString, QPixmap> m_icons[PLSUiApp::IconMax];
};

class LIBUI_API PLSSwitchButton : public PLSIconButton {
	Q_OBJECT
public:
	explicit PLSSwitchButton(QWidget *parent = nullptr);
	~PLSSwitchButton();

	void setChecked(bool checked);
	bool isChecked() const;

signals:
	void stateChanged(int);

protected:
	void paintEvent(QPaintEvent *event) override;
	bool event(QEvent *event) override;

private:
	bool m_check = false;
	int m_checkState = Qt::Unchecked;
};

class LIBUI_API PLSDelayResponseButton : public QPushButton {
	Q_OBJECT
public:
	explicit PLSDelayResponseButton(QWidget *parent = nullptr);
	~PLSDelayResponseButton();

	void setDelayRespInterval(int intervalMs);

private:
	void onButtonClicked();
	void startTimer();
	void stopTimer();
	void timerCallback();
signals:
	void buttonClicked();

private:
	int intervalMs = 200;
	QTimer timer;
	QTime clickBtnTime;
};

#endif //PLSPUSHBUTTON_H