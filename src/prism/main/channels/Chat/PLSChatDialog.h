#ifndef PLSCHATDIALOG_H
#define PLSCHATDIALOG_H

#include <dialog-view.hpp>

namespace Ui {
class PLSChatDialog;
}

using namespace std;

class PLSChatDialog : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSChatDialog(QWidget *parent = nullptr);
	~PLSChatDialog();

private:
	Ui::PLSChatDialog *ui;
};

#endif // PLSCHATDIALOG_H
