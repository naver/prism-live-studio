#pragma once

#include <QSpinBox>
#include "libui.h"

class LIBUI_API PLSSpinBox : public QSpinBox {
	Q_OBJECT

public:
	explicit PLSSpinBox(QWidget *parent = nullptr);
	~PLSSpinBox() override = default;
	void makeTextVCenter() const;

public:
	void stepBy(int steps) override;

protected:
	void wheelEvent(QWheelEvent *event) override;
	QValidator::State validate(QString &input, int &pos) const override;
};

class LIBUI_API PLSDoubleSpinBox : public QDoubleSpinBox {
	Q_OBJECT

public:
	explicit PLSDoubleSpinBox(QWidget *parent = nullptr);
	~PLSDoubleSpinBox() override = default;

public:
	void stepBy(int steps) override;

protected:
	void wheelEvent(QWheelEvent *event) override;
};
