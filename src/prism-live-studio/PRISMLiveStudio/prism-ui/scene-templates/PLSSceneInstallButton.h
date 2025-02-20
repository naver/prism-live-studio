#ifndef PLSSCENEINSTALLBUTTON_H
#define PLSSCENEINSTALLBUTTON_H

#include <QPushButton>
#include "PLSLoadingView.h"

namespace Ui {
class PLSSceneInstallButton;
}

class PLSSceneInstallButton : public QPushButton {
	Q_OBJECT

public:
	explicit PLSSceneInstallButton(QWidget *parent = nullptr);
	~PLSSceneInstallButton();
	void startInstall();
	void endInstall();

private:
	void showLoading();
	void hideLoading();

private:
	Ui::PLSSceneInstallButton *ui;
	bool m_installing{false};
	PLSLoadingView *m_pLoadingView = nullptr;
};

#endif // PLSSCENEINSTALLBUTTON_H
