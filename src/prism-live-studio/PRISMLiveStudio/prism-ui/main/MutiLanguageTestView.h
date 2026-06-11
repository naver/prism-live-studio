
#ifndef MUTILANGUAGETESTVIEW_H
#define MUTILANGUAGETESTVIEW_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QMetaEnum>
#include "PLSDialogView.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MutiLanguageTestView;
}
QT_END_NAMESPACE

class MutiLanguageTestView : public PLSDialogView

{
	Q_OBJECT
public:
	enum class ButtonRole {
		// keep this in sync with QMessageBox::StandardButton and QPlatformDialogHelper::StandardButton
		NoButton = 0x00000000,
		Ok = 0x00000400,
		Save = 0x00000800,
		SaveAll = 0x00001000,
		Open = 0x00002000,
		Yes = 0x00004000,
		YesToAll = 0x00008000,
		No = 0x00010000,
		NoToAll = 0x00020000,
		Abort = 0x00040000,
		Retry = 0x00080000,
		Ignore = 0x00100000,
		Close = 0x00200000,
		Cancel = 0x00400000,
		Discard = 0x00800000,
		Help = 0x01000000,
		Apply = 0x02000000,
		Reset = 0x04000000,
		RestoreDefaults = 0x08000000,
	};
	Q_ENUM(ButtonRole)

	MutiLanguageTestView(QWidget *parent = nullptr);
	~MutiLanguageTestView();

private slots:
	void on_button_show_message_clicked();

private:
	void initUi();

private:
	Ui::MutiLanguageTestView *ui;
};

#endif // MUTILANGUAGETESTVIEW_H
