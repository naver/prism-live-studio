#ifndef PLSALERTVIEW_H
#define PLSALERTVIEW_H

#include <functional>
#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QMap>
#include <QTimer>
#include <optional>

#include "PLSDialogView.h"
#include "PLSCheckBox.h"
#include "libui.h"

namespace Ui {
class PLSAlertView;
}

LIBUI_API extern const QString AlertKeyDisableEsc;
LIBUI_API extern const QString AlertKeyDisableAltF4;

/**
 * @class PLSAlertView
 * @brief common popup alert
 */
class LIBUI_API PLSAlertView : public PLSDialogView {
	Q_OBJECT
public:
	using Button = QDialogButtonBox::StandardButton;
	using Buttons = QDialogButtonBox::StandardButtons;

	enum class Icon { NoIcon, Information, Question, Warning, Critical };

	struct Result {
		Button button;
		bool isChecked;
	};

	/**
	* @brief      open alert
	* @param[in]  parent        : the parent widget
	* @param[in]  icon          : the icon
	* @param[in]  title         : the title
	* @param[in]  message       : the message
	* @param[in]  checkbox      : the checkbox text
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @param[in]  sugsize          : the window sugsize
	*/
	explicit PLSAlertView(QWidget *parent, Icon icon, const QString &title, const pls_text_t &message, const QString &checkbox, const Buttons &buttons, Button defaultButton = Button::NoButton,
			      const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	explicit PLSAlertView(QWidget *parent, Icon icon, const QString &title, const pls_text_t &message, const QString &checkbox, const QMap<Button, pls_text_t> &buttons,
			      Button defaultButton = Button::NoButton, const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	explicit PLSAlertView(Icon icon, const QString &title, const pls_text_t &messageTitle, const pls_text_t &messageContent, QWidget *parent, PLSAlertView::Buttons buttons,
			      Button defaultButton = Button::NoButton, const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	~PLSAlertView() override;

	/**
	* @brief      open alert
	* @param[in]  parent        : the parent widget
	* @param[in]  message       : the message
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @return     the button that user clicked
	*/
	static Button open(QWidget *parent, const pls_text_t &message, const Buttons &buttons, Button defaultButton = Button::NoButton, const std::optional<int> &timeout = std::optional<int>(),
			   const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Button open(QWidget *parent, const pls_text_t &message, const QMap<Button, pls_text_t> &buttons, Button defaultButton = Button::NoButton,
			   const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Button open(const QString &title, const pls_text_t &messageTitle, const pls_text_t &messageContent, QWidget *parent, const QMap<Button, pls_text_t> &buttons,
			   Button defaultButton = Button::NoButton, const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());

	/**
	* @brief      open alert
	* @param[in]  parent     : the parent widget
	* @param[in]  title      : the title
	* @param[in]  message    : the message
	* @param[in]  buttons    : the custom buttons
	* @return     the button that user clicked
	*/
	static Button open(QWidget *parent, const QString &title, const pls_text_t &message, const Buttons &buttons, Button defaultButton = Button::NoButton,
			   const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Button open(QWidget *parent, const QString &title, const pls_text_t &message, const QMap<Button, pls_text_t> &buttons, Button defaultButton = Button::NoButton,
			   const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());

	/**
	* @brief      open alert
	* @param[in]  parent        : the parent widget
	* @param[in]  icon          : the icon
	* @param[in]  title         : the title
	* @param[in]  message       : the message
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @return     the button that user clicked
	*/
	static Button open(QWidget *parent, Icon icon, const QString &title, const pls_text_t &message, const Buttons &buttons, Button defaultButton = Button::NoButton,
			   const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Button open(QWidget *parent, Icon icon, const QString &title, const pls_text_t &message, const QMap<Button, pls_text_t> &buttons, Button defaultButton = Button::NoButton,
			   const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Button open(Icon icon, const QString &title, const pls_text_t &messageTitle, const pls_text_t &messageContent, QWidget *parent, PLSAlertView::Buttons buttons,
			   Button defaultButton = Button::NoButton, const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());

	/**
	* @brief      open alert
	* @param[in]  parent        : the parent widget
	* @param[in]  icon          : the icon
	* @param[in]  title         : the title
	* @param[in]  message       : the message
	* @param[in]  checkbox      : the checkbox text
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @return     the button that user clicked and checkbox state
	*/
	static Result open(QWidget *parent, Icon icon, const QString &title, const pls_text_t &message, const QString &checkbox, const Buttons &buttons, Button defaultButton = Button::NoButton,
			   const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Result open(QWidget *parent, Icon icon, const QString &title, const pls_text_t &message, const QString &checkbox, const QMap<Button, pls_text_t> &buttons,
			   Button defaultButton = Button::NoButton, const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());

	/**
	* @brief      open information alert
	* @param[in]  parent        : the parent widget
	* @param[in]  title         : the title
	* @param[in]  message       : the message
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @return     the button that user clicked
	*/
	static Button information(QWidget *parent, const QString &title, const pls_text_t &message, Buttons buttons = Button::Ok, Button defaultButton = Button::Ok,
				  const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Button information(QWidget *parent, const QString &title, const pls_text_t &message, const QMap<Button, pls_text_t> &buttons, Button defaultButton = Button::NoButton,
				  const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	/**
	* @brief      open information alert
	* @param[in]  parent        : the parent widget
	* @param[in]  title         : the title
	* @param[in]  message       : the message
	* @param[in]  checkbox      : the checkbox text
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @return     the button that user clicked and checkbox state
	*/
	static Result information(QWidget *parent, const QString &title, const pls_text_t &message, const QString &checkbox, Buttons buttons = Button::Ok, Button defaultButton = Button::Ok,
				  const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Result information(QWidget *parent, const QString &title, const pls_text_t &message, const QString &checkbox, const QMap<Button, pls_text_t> &buttons,
				  Button defaultButton = Button::NoButton, const std::optional<int> &timeout = std::optional<int>(),
				  const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());

	/**
	* @brief      open question alert
	* @param[in]  parent        : the parent widget
	* @param[in]  title         : the title
	* @param[in]  message       : the message
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @return     the button that user clicked
	*/
	static Button question(QWidget *parent, const QString &title, const pls_text_t &message, Buttons buttons = Buttons(Button::Yes | Button::No), Button defaultButton = Button::NoButton,
			       const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Button question(QWidget *parent, const QString &title, const pls_text_t &message, const QMap<Button, pls_text_t> &buttons, Button defaultButton = Button::NoButton,
			       const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Button question(const QString &title, const pls_text_t &messageTitle, const pls_text_t &messageContent, QWidget *parent, PLSAlertView::Buttons buttons,
			       Button defaultButton = Button::NoButton, const std::optional<int> &timeout = std::optional<int>(),
			       const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());

	/**
	* @brief      open question alert
	* @param[in]  parent        : the parent widget
	* @param[in]  title         : the title
	* @param[in]  message       : the message
	* @param[in]  checkbox      : the checkbox text
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @return     the button that user clicked and checkbox state
	*/
	static Result question(QWidget *parent, const QString &title, const pls_text_t &message, const QString &checkbox, Buttons buttons = Buttons(Button::Yes | Button::No),
			       Button defaultButton = Button::NoButton, const std::optional<int> &timeout = std::optional<int>(),
			       const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Result question(QWidget *parent, const QString &title, const pls_text_t &message, const QString &checkbox, const QMap<Button, pls_text_t> &buttons,
			       Button defaultButton = Button::NoButton, const std::optional<int> &timeout = std::optional<int>(),
			       const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());

	/**
	* @brief      open warning alert
	* @param[in]  parent        : the parent widget
	* @param[in]  title         : the title
	* @param[in]  message       : the message
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @return     the button that user clicked
	*/
	static Button warning(QWidget *parent, const QString &title, const pls_text_t &message, Buttons buttons = Button::Ok, Button defaultButton = Button::Ok,
			      const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Button warning(QWidget *parent, const QString &title, const pls_text_t &message, const QMap<Button, pls_text_t> &buttons, Button defaultButton = Button::NoButton,
			      const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	/**
	* @brief      open warning alert
	* @param[in]  parent        : the parent widget
	* @param[in]  title         : the title
	* @param[in]  message       : the message
	* @param[in]  checkbox      : the checkbox text
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @return     the button that user clicked and checkbox state
	*/
	static Result warning(QWidget *parent, const QString &title, const pls_text_t &message, const QString &checkbox, Buttons buttons = Button::Ok, Button defaultButton = Button::Ok,
			      const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Result warning(QWidget *parent, const QString &title, const pls_text_t &message, const QString &checkbox, const QMap<Button, pls_text_t> &buttons,
			      Button defaultButton = Button::NoButton, const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());

	/**
	* @brief      open critical alert
	* @param[in]  parent        : the parent widget
	* @param[in]  title         : the title
	* @param[in]  message       : the message
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @return     the button that user clicked
	*/
	static Button critical(QWidget *parent, const QString &title, const pls_text_t &message, Buttons buttons = Button::Ok, Button defaultButton = Button::Ok,
			       const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Button critical(QWidget *parent, const QString &title, const pls_text_t &message, const QMap<Button, pls_text_t> &buttons, Button defaultButton = Button::NoButton,
			       const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	/**
	* @brief      open critical alert
	* @param[in]  parent        : the parent widget
	* @param[in]  title         : the title
	* @param[in]  message       : the message
	* @param[in]  checkbox      : the checkbox text
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @return     the button that user clicked and checkbox state
	*/
	static Result critical(QWidget *parent, const QString &title, const pls_text_t &message, const QString &checkbox, Buttons buttons = Button::Ok, Button defaultButton = Button::Ok,
			       const std::optional<int> &timeout = std::optional<int>(), const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Result critical(QWidget *parent, const QString &title, const pls_text_t &message, const QString &checkbox, const QMap<Button, pls_text_t> &buttons,
			       Button defaultButton = Button::NoButton, const std::optional<int> &timeout = std::optional<int>(),
			       const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());

	/**
	* @brief      open error message alert
	* @param[in]  parent        : the parent widget
	* @param[in]  title         : the title
	* @param[in]  message       : the message
	* @param[in]  errorCode     : the error code
	* @param[in]  userId        : the user id
	* @param[in]  buttons       : the custom buttons
	* @param[in]  defaultButton : the default button
	* @return     the button that user clicked and checkbox state
	*/
	static Button errorMessage(QWidget *parent, const QString &title, const pls_text_t &message, const QString &errorCode, const QString &userId,
				   const std::function<void(const QString &title, const QString &message, const QString &errorCode, const QString &userId, const QString &time)> &contactUsCb,
				   Buttons buttons = Button::Ok, Button defaultButton = Button::Ok, const std::optional<int> &timeout = std::optional<int>(),
				   const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());
	static Button errorMessage(QWidget *parent, const QString &title, const pls_text_t &message, const QString &errorCode, const QString &userId,
				   const std::function<void(const QString &title, const QString &message, const QString &errorCode, const QString &userId, const QString &time)> &contactUsCb,
				   const QMap<Button, pls_text_t> &buttons, Button defaultButton = Button::NoButton, const std::optional<int> &timeout = std::optional<int>(),
				   const QMap<QString, QVariant> &properties = QMap<QString, QVariant>());

	//for countdown
	static Result openWithCountDownView(QWidget *parent, Icon icon, const QString &title, const pls_text_t &message, const QString &checkbox, const QMap<Button, pls_text_t> &buttons,
					    Button defaultButton = Button::NoButton, const quint64 &timeout = 10 * 1000, int buttonBoxWidth = 170);

	static Result questionWithCountdownView(QWidget *parent, const QString &title, const pls_text_t &message, const QString &checkbox, const QMap<Button, pls_text_t> &buttons,
						Button defaultButton = Button::NoButton, const quint64 &timeout = 10 * 1000, int buttonBoxWidth = 170);

	static Button dualOutputApplyResolutionWarn(QWidget *parent, const QString &title, const QString &message, const QMap<Button, pls_text_t> &buttons, const QString &hRadioMsg,
						    const QString &vRadioMsg, bool &selectVRadio, bool bShowCloseBtn = false, Button defaultButton = Button::NoButton);
	bool isChecked() const;

	Qt::TextFormat getTextFormat() const;
	void setTextFormat(Qt::TextFormat format);

	Icon getIcon() const;
	void setIcon(Icon icon);

	void delayAutoClick(const std::optional<int> &timeout /* milliseconds */, Button button);
	void stopDelayAutoClick();

signals:
	void contentlinkActivated(const QString &link);

private slots:
	void onButtonClicked(QAbstractButton *button);

protected:
	void showEvent(QShowEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:
	Ui::PLSAlertView *ui = nullptr;
	Icon m_icon = Icon::NoIcon;
	PLSCheckBox *m_checkBox = nullptr;
	QTimer *m_delayAutoClickTimer = nullptr;
	int m_btnCount = 0;
	QByteArray m_key;
	QString m_stage;
	QMap<QString, QVariant> m_savedProperties{};
};

#endif // PLSALERTVIEW_H
