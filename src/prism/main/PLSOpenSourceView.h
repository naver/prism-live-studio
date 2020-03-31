#ifndef PLSOPENSOURCEVIEW_H
#define PLSOPENSOURCEVIEW_H

#include "dialog-view.hpp"

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSOpenSourceView;
}
QT_END_NAMESPACE

class PLSOpenSourceView : public PLSDialogView {
	Q_OBJECT

public:
	PLSOpenSourceView(QWidget *parent = nullptr);
	~PLSOpenSourceView();

private slots:
	void on_confirmButton_clicked();

private:
	void setupTextEdit();

private:
	Ui::PLSOpenSourceView *ui;
};
#endif // PLSOPENSOURCEVIEW_H
