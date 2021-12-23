#ifndef CHANNELLOGINVIEW_HPP
#define CHANNELLOGINVIEW_HPP

#include <QDialog>

namespace Ui {
class PLSChannelLoginView;
}

class PLSChannelLoginView : public QDialog {
	Q_OBJECT

public:
	explicit PLSChannelLoginView(QJsonObject &result, QWidget *parent = nullptr);
	~PLSChannelLoginView();

private:
	Ui::PLSChannelLoginView *ui;
	QJsonObject &result;
};

#endif // CHANNELLOGINVIEW_HPP
