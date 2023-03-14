#ifndef PLSTOASTBUTTON_H
#define PLSTOASTBUTTON_H

#include <QPushButton>
#include <qlabel.h>

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
	void setShowAlert(bool showAlert);
	QPushButton *getButton();
signals:
	void clickButton();

protected:
	void setNumLabelPositon(const double dpi);
	virtual bool eventFilter(QObject *o, QEvent *e) override;

private:
	Ui::PLSToastButton *ui;
	int m_num;
	QLabel *m_numLabel;
};

#endif // PLSTOASTBUTTON_H
