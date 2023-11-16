#ifndef PLSSHOPPINGHOSTCHANNEL_H
#define PLSSHOPPINGHOSTCHANNEL_H

#include "PLSDialogView.h"

namespace Ui {
class PLSShoppingHostChannel;
}

class PLSShoppingHostChannel : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSShoppingHostChannel(QWidget *parent = nullptr);
	~PLSShoppingHostChannel() override;

private slots:
	void on_declineButton_clicked();
	void on_agreeButton_clicked();

protected:
	void closeEvent(QCloseEvent *event) override;

private:
	QString setLineHeight(QString sourceText, uint lineHeight) const;

	Ui::PLSShoppingHostChannel *ui;
};

#endif // PLSSHOPPINGHOSTCHANNEL_H
