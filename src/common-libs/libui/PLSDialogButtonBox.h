#ifndef _PRISM_COMMON_LIBHDPI_DIALOGBUTTONBOX_H
#define _PRISM_COMMON_LIBHDPI_DIALOGBUTTONBOX_H

#include <QDialogButtonBox>
#include <QHBoxLayout>

#include "libui.h"
using StandardButton = QDialogButtonBox::StandardButton;
using ButtonRole = QDialogButtonBox::ButtonRole;
using StandardButtons = QDialogButtonBox::StandardButtons;

class LIBUI_API PLSDialogButtonBox : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QString extraLog READ extraLog WRITE setExtraLog)
	Q_PROPERTY(bool centerButtons READ centerButtons WRITE setCenterButtons)

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
	~PLSDialogButtonBox() = default;

	using ButtonCb = std::function<void(QPushButton *btn, StandardButton button, ButtonRole role)>;
	static void updateStandardButtonsStyle(QDialogButtonBox *buttonBox, const ButtonCb &buttonCb = nullptr);

	void clear();

	QList<QAbstractButton *> buttons() const;
	ButtonRole buttonRole(QAbstractButton *button) const;

	void setStandardButtons(StandardButtons buttons);
	StandardButtons standardButtons() const;
	StandardButton standardButton(QAbstractButton *button) const;
	QPushButton *button(StandardButton which) const;
	QString getText(StandardButton button);
	QString extraLog() const;
	void setExtraLog(const QString &extraLog);
	void setOrientation(Qt::Orientation orientation);
	void setCenterButtons(bool center);
	bool centerButtons() const;

protected:
	bool event(QEvent *event) override;
signals:
	void accepted();
	void clicked(QAbstractButton *button);
	void helpRequested();
	void rejected();

private:
	QPushButton *createButton(StandardButton sbutton, bool doLayout);
	void createStandardButtons(StandardButtons buttons);
	ButtonRole buttonRole(QDialogButtonBox::StandardButton sbutton);

	void layoutButtons();
	void initLayout();
	void addButtonsToLayout(const QList<QAbstractButton *> &buttonList, bool reverse);
	void ensureFirstAcceptIsDefault();

private slots:
	void handleButtonClicked();

private:
	QList<Btn> m_btns;
	QString m_extraLog;
	QHBoxLayout *m_buttonLayout = nullptr;
	QDialogButtonBox::ButtonLayout m_layoutPolicy = QDialogButtonBox::WinLayout;
	QList<QAbstractButton *> buttonLists[QDialogButtonBox::NRoles];
	bool m_center = false;
};

#endif // _PRISM_COMMON_LIBHDPI_DIALOGBUTTONBOX_H
