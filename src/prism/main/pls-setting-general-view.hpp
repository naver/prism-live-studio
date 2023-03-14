#ifndef PLSSETTINGGENERALVIEW_HPP
#define PLSSETTINGGENERALVIEW_HPP

#include <QWidget>

#include <PLSDpiHelper.h>

namespace Ui {
class PLSSettingGeneralView;
}

class PLSSettingGeneralView : public QWidget {
	Q_OBJECT

public:
	explicit PLSSettingGeneralView(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSSettingGeneralView();
	void initUi(double dpi);
	void setEnable(bool enable);

protected:
	void changeEvent(QEvent *e);

private slots:
	void on_pushButton_logout_clicked();
	void on_pushButton_del_account_clicked();
	void on_pushButton_change_pwd_clicked();

private:
	Ui::PLSSettingGeneralView *ui;
};

#endif // PLSSETTINGGENERALVIEW_HPP
