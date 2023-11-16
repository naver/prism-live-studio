#ifndef PLSTOASTBUTTON_H
#define PLSTOASTBUTTON_H

#include <QPushButton>
#include <qlabel.h>

namespace Ui {
class PLSToastButton;
}

class PLSToastButton : public QWidget {
	Q_OBJECT

public:
	explicit PLSToastButton(QWidget *parent = nullptr);
	~PLSToastButton() override;
	void setNum(const int num);
	int num() const;
	QString getNumText() const;
	void setShowAlert(bool showAlert);
	QPushButton *getButton();
signals:
	void clickButton();

protected:
	void setNumLabelPositon();
	bool eventFilter(QObject *o, QEvent *e) override;

private:
	Ui::PLSToastButton *ui;
	int m_num = 0;
	QLabel *m_numLabel = nullptr;
};

#endif // PLSTOASTBUTTON_H
