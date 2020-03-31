#ifndef PLSTOASTBUTTON_H
#define PLSTOASTBUTTON_H

#include <QPushButton>

namespace Ui {
class PLSToastButton;
}

class PLSToastButton : public QPushButton {
	Q_OBJECT

public:
	explicit PLSToastButton(QWidget *parent = nullptr);
	~PLSToastButton();
	void setNum(const int num);
	int num() const;
	QString getNumText() const;
	void updateIconStyle(bool num);

protected:
	void paintEvent(QPaintEvent *event);

private:
private:
	Ui::PLSToastButton *ui;
	int m_num;
};

#endif // PLSTOASTBUTTON_H
