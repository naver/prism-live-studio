#ifndef PLSMOTIONERRORLAYER_H
#define PLSMOTIONERRORLAYER_H

#include <QFrame>
#include <QTimer>

namespace Ui {
class PLSMotionErrorLayer;
}

class PLSMotionErrorLayer : public QFrame {
	Q_OBJECT

public:
	explicit PLSMotionErrorLayer(QWidget *parent = nullptr, int timerInterval = 5000);
	~PLSMotionErrorLayer() override;
	void showView();
	void hiddenView();
	void setToastText(const QString &text);
	void setToastTextAlignment(Qt::Alignment alignment);
	void setToastWordWrap(bool wrap);

private slots:
	void on_pushButton_clicked() const;

private:
	Ui::PLSMotionErrorLayer *ui;
	QTimer m_timerToastHide;
	int m_timerInterval = 5000;
};

#endif // PLSMOTIONERRORLAYER_H
