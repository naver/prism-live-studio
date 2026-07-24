#ifndef LOGINVIEW_HPP
#define LOGINVIEW_HPP

#include "PLSDialogView.h"
#include "login-background-view.hpp"
#include <qpointer.h>

#include "login-with-email-view.hpp"
#include "signup-with-email-view.hpp"

namespace Ui {
class PLSLoginView;
}

class PLSLoginView : public QWidget {
	Q_OBJECT

public:
	explicit PLSLoginView(QWidget *parent = nullptr);
	~PLSLoginView() override;
	void macQuitWithCommand_Q();

private:
	void initUi();

	Ui::PLSLoginView *ui;
	LoginBackgroundView *m_loginBackgroundView = nullptr;
	LoginWithEmailView *m_loginWithEmailView = nullptr;
	SignupWithEmailView *m_signupWithEmailView = nullptr;
};

#endif // LOGINVIEW_HPP
