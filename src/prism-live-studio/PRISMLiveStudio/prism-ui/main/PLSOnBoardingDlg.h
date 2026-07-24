#ifndef PLSONBOARDINGDLG_H
#define PLSONBOARDINGDLG_H

#include "PLSDialogView.h"

namespace Ui {
class PLSOnBoardingDlg;
}

class PLSOnBoardingDlg : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSOnBoardingDlg(QWidget *parent = nullptr);
	~PLSOnBoardingDlg();

private slots:
	void on_pushButton_link_clicked();
	void on_pushButton_start_clicked();

private:
	Ui::PLSOnBoardingDlg *ui;
};

namespace pls::lens {
QString getLensVersion();
bool isLensVersionLessThanStartCloseVersion();

bool isNeedShowQuitAlert();
void showLensQuitAlert(QWidget *parent = nullptr);

bool isNeedStartLensWhenStartPrism();
void startLensIfNeed();
void startLens();

bool isNeedShowOnBoardingDialog();
void showOnBoardingDialogIfNeed(QWidget *parent);

void closeLensIfNeed();
void printBaseLog();

bool getIsLensRunning();

bool getThisShowOnBoardingDialog();

}

#endif // PLSONBOARDINGDLG_H
