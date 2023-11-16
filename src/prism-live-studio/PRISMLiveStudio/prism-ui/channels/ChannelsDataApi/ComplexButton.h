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
	~ComplexButton() override;

	void setAliginment(Qt::Alignment ali, int space = 5);
	void setText(const QString &txt);

signals:
	void clicked();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void changeEvent(QEvent *e) override;
	void setState(const QString &state);
	void setWidgetState(const QString &state, QWidget *widget) const;

private:
	Ui::ComplexButton *ui;

	// QObject interface
};

#endif // COMPLEXBUTTON_H
