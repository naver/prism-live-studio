#ifndef PLSCHECKBOX_H
#define PLSCHECKBOX_H

#include <qwidget.h>

#include "libui-globals.h"

class QLabel;
class QHBoxLayout;
constexpr int SPACING = 10;
class LIBUI_API PLSCheckBox : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QString text READ text WRITE setText)
	Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable)
	Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled USER true)

public:
	explicit PLSCheckBox(QWidget *parent = nullptr);
	explicit PLSCheckBox(const QString &text, QWidget *parent = nullptr);
	explicit PLSCheckBox(const QPixmap &pixmap, const QString &text, bool textFirst, const QString &tooltip = QString(), QWidget *parent = nullptr);
	~PLSCheckBox() override = default;

	bool isCheckable() const { return true; }
	void setCheckable(bool) {}

	QSize iconSize() const;

	bool isChecked() const;

	void setText(const QString &text);
	QString text() const;

	Qt::CheckState checkState() const;
	void setCheckState(Qt::CheckState state);

	int getSpac() const;
	void setSpac(int spac);

public slots:
	void setIconSize(const QSize &size);
	void animateClick();
	void click();
	void toggle();
	void setChecked(bool);

signals:
	void pressed();
	void released();
	void clicked(bool checked = false);
	void toggled(bool checked);
	void stateChanged(int);

private:
	void setState(const char *name, bool &state, bool value);
	const QPixmap &getIcon() const;

protected:
	bool event(QEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	QPixmap m_defIcon;
	QHBoxLayout *m_layout = nullptr;
	QLabel *m_text = nullptr;
	bool m_checked = false;
	bool m_hovered = false;
	bool m_pressed = false;
	int m_spac = SPACING;
};

class LIBUI_API PLSElideCheckBox : public PLSCheckBox {
	Q_OBJECT

public:
	explicit PLSElideCheckBox(QWidget *parent = nullptr);
	explicit PLSElideCheckBox(const QString &text, QWidget *parent = nullptr);
	~PLSElideCheckBox() override = default;

	void setText(const QString &text);
	QString text() const;

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

private:
	void updateText();

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	QString originText;
};

#endif // PLSCHECKBOX_H
