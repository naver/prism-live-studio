#ifndef PLSABOUTVIEW_HPP
#define PLSABOUTVIEW_HPP

#include <dialog-view.hpp>

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSAboutView;
}
QT_END_NAMESPACE

class PLSAboutView : public PLSDialogView {
	Q_OBJECT

public:
	PLSAboutView(QWidget *parent = nullptr);
	~PLSAboutView();

private slots:
	void on_checkUpdateButton_clicked();

private:
	Ui::PLSAboutView *ui;

private:
	void initConnect();
};
#endif // PLSABOUTVIEW_HPP
