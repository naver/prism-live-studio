#ifndef PLSFILEBUTTON_H
#define PLSFILEBUTTON_H

#include <QPushButton>

namespace Ui {
class PLSFileButton;
}

class PLSFileButton : public QPushButton {
	Q_OBJECT
public:
	explicit PLSFileButton(QWidget *parent = nullptr);
	~PLSFileButton() override;
	void setFileButtonEnabled(bool enabled);

protected:
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	Ui::PLSFileButton *ui;
	bool m_enabled = true;
};

#endif // PLSFILEBUTTON_H
