#ifndef PLSFACEBOOKGAMELINEEDIT_H
#define PLSFACEBOOKGAMELINEEDIT_H

#include "PLSEdit.h"
#include <QListWidget>

class PLSLoadingComboxMenu;
namespace Ui {
class PLSFacebookGameLineEdit;
}

class PLSFacebookGameLineEdit : public PLSLineEdit {
	Q_OBJECT

public:
	explicit PLSFacebookGameLineEdit(QWidget *parent = nullptr);
	~PLSFacebookGameLineEdit() override;

protected:
	void focusInEvent(QFocusEvent *event) override;
	void focusOutEvent(QFocusEvent *event) override;

signals:
	void searchKeyword(const QString);
	void clearText();

private:
	Ui::PLSFacebookGameLineEdit *ui;
};

#endif // PLSFACEBOOKGAMELINEEDIT_H
