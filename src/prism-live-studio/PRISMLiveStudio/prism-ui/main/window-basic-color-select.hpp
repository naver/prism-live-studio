#pragma once

#include <QPushButton>
#include <QFrame>
#include "obs.hpp"

const int SOURCE_ITEM_MAX_COLOR_COUNT = 8;

namespace Ui {
class ColorSelectNew;
}

class ColorButton : public QPushButton {
	Q_OBJECT
public:
	explicit ColorButton(QWidget *parent = nullptr) : QPushButton(parent)
	{
		setObjectName("ColorButton");
		QSizePolicy sizePolicy = this->sizePolicy();
		sizePolicy.setRetainSizeWhenHidden(true);
		this->setSizePolicy(sizePolicy);
	}
	void SetColor(QString fillColor);
	void SetSelect(bool select);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QString color;
	bool selected;
};

class ColorSelectNew : public QFrame {

	Q_OBJECT
public:
	explicit ColorSelectNew(QWidget *parent = nullptr);
	~ColorSelectNew() override;
	void UpdateRecentColorOrder(obs_data_t *private_settings);

protected:
	// Here we just ignore mouse click messages to avoid hiding menu
	void mousePressEvent(QMouseEvent *event) override { Q_UNUSED(event) }

private:
	void AppendRecentColorButton(const char *color, int index);
	Ui::ColorSelectNew *ui;
};
