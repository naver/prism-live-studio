#include "widget.h"
#include "ui_widget.h"

#include <QMovie>

Widget::Widget(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::Widget)
{
	ui->setupUi(this);

	ui->label_image_apng->setPixmap(QPixmap(QStringLiteral(":/data/sample.apng")));
	ui->label_image_png->setPixmap(QPixmap(QStringLiteral(":/data/sample.png"), "apng"));

	auto m1 = new QMovie(QStringLiteral(":/data/sample.apng"), "apng", this);
	m1->start();
	ui->label_movie_apng->setMovie(m1);
	auto m2 = new QMovie(QStringLiteral(":/data/sample.png"), "apng", this);
	m2->start();
	ui->label_movie_png->setMovie(m2);
}

Widget::~Widget()
{
	delete ui;
}
