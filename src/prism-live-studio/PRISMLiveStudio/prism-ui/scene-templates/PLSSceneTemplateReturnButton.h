#ifndef PLSSCENETEMPLATERETURNBUTTON_H
#define PLSSCENETEMPLATERETURNBUTTON_H

#include <QFrame>

namespace Ui {
class PLSSceneTemplateReturnButton;
}

class PLSSceneTemplateReturnButton : public QFrame {
	Q_OBJECT

public:
	explicit PLSSceneTemplateReturnButton(QWidget *parent = nullptr);
	~PLSSceneTemplateReturnButton();

protected:
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

signals:
	void clicked();

private:
	Ui::PLSSceneTemplateReturnButton *ui;
};

#endif // PLSSCENETEMPLATERETURNBUTTON_H
