#ifndef PLSSETTINGGENERALVIEW_HPP
#define PLSSETTINGGENERALVIEW_HPP

#include <QWidget>

namespace Ui {
class PLSSettingGeneralView;
}

class PLSSettingGeneralView : public QWidget {
	Q_OBJECT

public:
	explicit PLSSettingGeneralView(QWidget *parent = nullptr);
	~PLSSettingGeneralView() override;
	void initUi();
	void setEnable(bool enable);
	void setNickNameWidth(int width);

private slots:
	void on_pushButton_logout_clicked();
	void on_pushButton_del_account_clicked();
	void on_pushButton_change_pwd_clicked();

private:
	Ui::PLSSettingGeneralView *ui;
};

#endif // PLSSETTINGGENERALVIEW_HPP
