#pragma once

#include <QSpinBox>
#include "libui.h"

class LIBUI_API PLSSpinBox : public QSpinBox {
	Q_OBJECT

public:
	explicit PLSSpinBox(QWidget *parent = nullptr);
	~PLSSpinBox() override = default;
	void makeTextVCenter() const;

protected:
	void wheelEvent(QWheelEvent *event) override;
};

class LIBUI_API PLSDoubleSpinBox : public QDoubleSpinBox {
	Q_OBJECT

public:
	explicit PLSDoubleSpinBox(QWidget *parent = nullptr);
	~PLSDoubleSpinBox() override = default;

protected:
	void wheelEvent(QWheelEvent *event) override;
};
