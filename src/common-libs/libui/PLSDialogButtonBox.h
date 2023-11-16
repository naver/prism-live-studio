#ifndef _PRISM_COMMON_LIBHDPI_DIALOGBUTTONBOX_H
#define _PRISM_COMMON_LIBHDPI_DIALOGBUTTONBOX_H

#include <QDialogButtonBox>

#include "libui.h"

class LIBUI_API PLSDialogButtonBox : public QDialogButtonBox {
	Q_OBJECT
	Q_PROPERTY(QString extraLog READ extraLog WRITE setExtraLog)

public:
	struct Btn {
		QPushButton *m_btn = nullptr;
		StandardButton m_button = StandardButton::NoButton;
		ButtonRole m_role = ButtonRole::NoRole;

		Btn() {}
		Btn(QPushButton *btn, StandardButton button, ButtonRole role) : m_btn(btn), m_button(button), m_role(role) {}
	};

public:
	explicit PLSDialogButtonBox(QWidget *parent = nullptr);

	using ButtonCb = std::function<void(QPushButton *btn, StandardButton button, ButtonRole role)>;
	static void updateStandardButtonsStyle(QDialogButtonBox *buttonBox, const ButtonCb &buttonCb = nullptr);

	void addButton(QAbstractButton *button, ButtonRole role) = delete;
	QPushButton *addButton(const QString &text, ButtonRole role) = delete;
	QPushButton *addButton(StandardButton button) = delete;
	void removeButton(QAbstractButton *button) = delete;
	void clear();

	QList<QAbstractButton *> buttons() const;
	ButtonRole buttonRole(QAbstractButton *button) const;

	void setStandardButtons(StandardButtons buttons);
	StandardButtons standardButtons() const;
	StandardButton standardButton(QAbstractButton *button) const;
	QPushButton *button(StandardButton which) const;

	QString extraLog() const;
	void setExtraLog(const QString &extraLog);

protected:
	bool event(QEvent *event) override;

private:
	QList<Btn> m_btns;
	QString m_extraLog;
};

#endif // _PRISM_COMMON_LIBHDPI_DIALOGBUTTONBOX_H
