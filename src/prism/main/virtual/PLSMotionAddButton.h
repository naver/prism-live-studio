#ifndef PLSMOTIONADDBUTTON_H
#define PLSMOTIONADDBUTTON_H

#include <QPushButton>

namespace Ui {
class PLSMotionAddButton;
}

class PLSMotionAddButton : public QPushButton {
	Q_OBJECT

public:
	explicit PLSMotionAddButton(QWidget *parent = nullptr);
	~PLSMotionAddButton();
	bool eventFilter(QObject *watched, QEvent *event);

private:
	Ui::PLSMotionAddButton *ui;
};

#endif // PLSMOTIONADDBUTTON_H
