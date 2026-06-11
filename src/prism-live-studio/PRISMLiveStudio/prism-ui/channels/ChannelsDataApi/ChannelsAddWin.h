#ifndef CHANNELSADDWIN_H
#define CHANNELSADDWIN_H

#include <QFrame>
#include "PLSDialogView.h"

class QListWidgetItem;
class QListWidget;

namespace Ui {
class ChannelsAddWin;
}

class ChannelsAddWin : public PLSDialogView {
	Q_OBJECT

public:
	explicit ChannelsAddWin(QWidget *parent = nullptr);
	~ChannelsAddWin() override;
	void updateUi() const;

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void appendItem(const QString &text);
	void appendRTMPItem(const QString &platformName);
private slots:
	void runBtnCMD();
	void on_ClosePtn_clicked();

private:
	void initDefault();
	void updateItem(int index) const;

	//private:
	Ui::ChannelsAddWin *ui = nullptr;
};

#endif // CHANNELSADDWIN_H
