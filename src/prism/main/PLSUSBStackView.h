#ifndef PLSUSBSTACKVIEW_H
#define PLSUSBSTACKVIEW_H

#include <QFrame>

namespace Ui {
class PLSUSBStackView;
}

class PLSUSBStackView : public QFrame {
	Q_OBJECT

public:
	explicit PLSUSBStackView(QWidget *parent = nullptr);
	~PLSUSBStackView();

private:
	Ui::PLSUSBStackView *ui;
};

#endif // PLSUSBSTACKVIEW_H
