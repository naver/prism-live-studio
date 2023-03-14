#ifndef COMPLEXBUTTON_H
#define COMPLEXBUTTON_H

#include <QFrame>

namespace Ui {
class ComplexButton;
}

class ComplexButton : public QFrame {
	Q_OBJECT

public:
	explicit ComplexButton(QWidget *parent = nullptr);
	~ComplexButton();

	void setAliginment(Qt::Alignment ali, int space = 5);
	void setText(const QString &txt);

signals:
	void clicked();

protected:
	virtual bool eventFilter(QObject *watched, QEvent *event) override;
	void changeEvent(QEvent *e);
	void setState(const QString &state);
	void setWidgetState(const QString &state, QWidget *widget);

private:
	Ui::ComplexButton *ui;

	// QObject interface
};

#endif // COMPLEXBUTTON_H
