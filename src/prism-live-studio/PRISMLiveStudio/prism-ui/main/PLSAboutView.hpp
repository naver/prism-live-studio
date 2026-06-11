#ifndef PLSABOUTVIEW_HPP
#define PLSABOUTVIEW_HPP

#include "PLSDialogView.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSAboutView;
}
QT_END_NAMESPACE

class PLSAboutView : public PLSDialogView {
	Q_OBJECT

public:
	PLSAboutView(QWidget *parent = nullptr);
	~PLSAboutView() override;

private slots:
	void on_checkUpdateButton_clicked();

private:
	Ui::PLSAboutView *ui;
};
#endif // PLSABOUTVIEW_HPP
