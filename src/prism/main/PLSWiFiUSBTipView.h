#ifndef PLSWIFIUSBTIPVIEW_H
#define PLSWIFIUSBTIPVIEW_H

#include <QFrame>
class PLSWiFiStackView;

namespace Ui {
class PLSWiFiUSBTipView;

}

class PLSWiFiUSBTipView : public QFrame {
	Q_OBJECT

public:
	explicit PLSWiFiUSBTipView(QWidget *parent = nullptr);
	~PLSWiFiUSBTipView();
	void updateContent();
Q_SIGNALS:
	void closed();
private slots:
	void on_closeBtn_clicked();

private:
	Ui::PLSWiFiUSBTipView *ui;
};

#endif // PLSWIFIUSBTIPVIEW_H
