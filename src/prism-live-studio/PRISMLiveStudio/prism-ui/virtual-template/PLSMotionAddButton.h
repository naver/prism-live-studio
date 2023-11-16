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
	~PLSMotionAddButton() override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	Ui::PLSMotionAddButton *ui;
};

#endif // PLSMOTIONADDBUTTON_H
