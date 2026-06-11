#ifndef PLSENDTIPVIEW_H
#define PLSENDTIPVIEW_H
#include "PLSAlertView.h"
class QCefWidget;

namespace Ui {
class PLSEndTipView;
}

class PLSEndTipView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSEndTipView(QWidget *parent = nullptr);
	~PLSEndTipView() override;

protected:
	void accept() final;
	void done(int) final;

private slots:
	void onTimeOut();

private:
	QTimer *timer{nullptr};
	QCefWidget *m_cefWidget;
	Ui::PLSEndTipView *ui;
};

#endif // PLSENDTIPVIEW_H
