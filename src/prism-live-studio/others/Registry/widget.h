#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void on_refresh_clicked();
    void on_modify_clicked();
    void on_cancel_clicked();
    void on_app_currentIndexChanged(int index);

private:
    Ui::Widget *ui;
};
#endif // WIDGET_H
