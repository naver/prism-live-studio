#ifndef PLSUSBWIFISTACKVIEW_H
#define PLSUSBWIFISTACKVIEW_H

#include <QFrame>
#include <QToolButton>
#include <PLSWiFiUSBTipView.h>

class QButtonGroup;

namespace Ui {
class PLSWiFiStackView;
}

class PLSWiFiStackView : public QFrame {
	Q_OBJECT

public:
	explicit PLSWiFiStackView(QWidget *parent = nullptr);
	~PLSWiFiStackView();

Q_SIGNALS:
	void sigUpdateTipView();

protected:
	bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
	void buttonGroupSlot(int buttonId);
	void on_leftPageBtn_clicked();
	void on_rightPageBtn_clicked();
	void on_tipsBtn_clicked();

private:
	void updateTipView();
	void updateArrowButtonState();
	QString getTextForTag(int tag);

private:
	Ui::PLSWiFiStackView *ui;
	QButtonGroup *m_buttonGroup;
	int m_id;
	PLSWiFiUSBTipView *m_tipView;
};

#endif // PLSUSBWIFISTACKVIEW_H
