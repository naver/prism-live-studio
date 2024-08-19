#ifndef PLSSCENEINSTALLBUTTON_H
#define PLSSCENEINSTALLBUTTON_H

#include <QPushButton>
#include "loading-event.hpp"

namespace Ui {
class PLSSceneInstallButton;
}

class PLSSceneInstallButton : public QPushButton
{
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
    PLSLoadingEvent m_loadingEvent;
};

#endif // PLSSCENEINSTALLBUTTON_H
