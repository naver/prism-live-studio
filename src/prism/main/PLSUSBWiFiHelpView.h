#ifndef PLSUSBWIFIHELPVIEW_H
#define PLSUSBWIFIHELPVIEW_H

#include "dialog-view.hpp"
#include "PLSDpiHelper.h"

class QButtonGroup;
class QAbstractButton;

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSUSBWiFiHelpView;
}
QT_END_NAMESPACE

class PLSUSBWiFiHelpView : public PLSDialogView {
	Q_OBJECT

public:
	PLSUSBWiFiHelpView(DialogInfo info, QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSUSBWiFiHelpView();

private slots:
	void buttonGroupSlot(int buttonId);

protected:
	void showEvent(QShowEvent *event);
	void hideEvent(QHideEvent *event);
	void closeEvent(QCloseEvent *) override;

private:
	void updateShowPage();

private:
	Ui::PLSUSBWiFiHelpView *ui;
	QButtonGroup *m_buttonGroup;
	int m_buttonId;
};
#endif // PLSUSBWIFIHELPVIEW_H
