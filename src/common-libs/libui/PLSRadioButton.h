#ifndef PLSRADIOBUTTON_H
#define PLSRADIOBUTTON_H

#include <qwidget.h>
#include <qpointer.h>

#include "libui-globals.h"

class QLabel;
class QHBoxLayout;

class PLSRadioButton;

class LIBUI_API PLSRadioButtonGroup : public QObject {
	Q_OBJECT

public:
	PLSRadioButtonGroup(QObject *parent = nullptr);
	~PLSRadioButtonGroup() = default;

public:
	PLSRadioButton *button(int id) const;
	QList<PLSRadioButton *> buttons() const;

	PLSRadioButton *checkedButton() const;
	int checkedId() const;

	int id(PLSRadioButton *button) const;
	void setId(PLSRadioButton *button, int id = -1);

	void addButton(PLSRadioButton *button, int id = -1);
	void removeButton(PLSRadioButton *button);

private:
	void uncheckAllBut(PLSRadioButton *button);

signals:
	void buttonClicked(PLSRadioButton *button);
	void buttonPressed(PLSRadioButton *button);
	void buttonReleased(PLSRadioButton *button);
	void buttonToggled(PLSRadioButton *button, bool checked);
	void idClicked(int id);
	void idPressed(int id);
	void idReleased(int id);
	void idToggled(int id, bool checked);

private:
	QList<PLSRadioButton *> m_btns;
};

class LIBUI_API PLSRadioButton : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QString text READ text WRITE setText)
	Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable)
	Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled USER true)

public:
	explicit PLSRadioButton(QWidget *parent = nullptr);
	explicit PLSRadioButton(const QString &text, QWidget *parent = nullptr);
	~PLSRadioButton() override = default;

	bool isCheckable() const { return true; }
	void setCheckable(bool) {}

	QSize iconSize() const;

	bool isChecked() const;

	void setText(const QString &text);
	QString text() const;

	int id() const;
	void setId(int id);

	PLSRadioButtonGroup *group() const;

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

private:
	void initGroup(QWidget *parent);
	void setState(const char *name, bool &state, bool value);
	const QPixmap &getIcon() const;

protected:
	bool event(QEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	QPointer<PLSRadioButtonGroup> m_group = nullptr;
	QPixmap m_defIcon;
	QHBoxLayout *m_layout = nullptr;
	QLabel *m_text = nullptr;
	int m_id = -1;
	bool m_checked = false;
	bool m_hovered = false;
	bool m_pressed = false;
};

class LIBUI_API PLSElideRadioButton : public PLSRadioButton {
	Q_OBJECT

public:
	explicit PLSElideRadioButton(QWidget *parent = nullptr);
	explicit PLSElideRadioButton(const QString &text, QWidget *parent = nullptr);
	~PLSElideRadioButton() override = default;

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

#endif // PLSRADIOBUTTON_H
