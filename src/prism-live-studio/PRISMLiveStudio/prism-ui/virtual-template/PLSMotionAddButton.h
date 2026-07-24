#ifndef PLSMOTIONADDBUTTON_H
#define PLSMOTIONADDBUTTON_H

#include <QPushButton>
#include <QPixmap>

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
	QPixmap m_pixDefault;
	QPixmap m_pixHover;
	QPixmap m_pixClick;
};

#endif // PLSMOTIONADDBUTTON_H
