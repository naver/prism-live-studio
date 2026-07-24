/*
 * @fine      PrismLiveStudio
 * @brief     third party login button
 *             the custom button include icon and text
 * @date      2019-09-26
 * @author    Bing Cheng
 * @attention

 * @version   v1.0
 * @modify
 */

#ifndef LOGIN_LOGINCUSTOMBUTTON_H
#define LOGIN_LOGINCUSTOMBUTTON_H

#include "ui_PLSLoginCustomButton.h"
#include <QPushButton>

namespace Ui {
class PLSLoginCustomButton;
}
class LoginCustomButton : public QPushButton {
	Q_OBJECT

public:
	explicit LoginCustomButton(QWidget *parent = nullptr);
	~LoginCustomButton() override;
	/**
     * @brief setButtonPicture set button icon
     * @param picPath
     */
	void setButtonPicture(const QString &picPath);

private:
	Ui::PLSLoginCustomButton *ui;
};

#endif // LOGINCUSTOMBUTTON_H
