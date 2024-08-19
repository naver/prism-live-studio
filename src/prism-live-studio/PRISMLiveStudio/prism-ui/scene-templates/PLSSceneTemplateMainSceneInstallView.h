#ifndef PLSSCENETEMPLATEMAINSCENEINSTALLVIEW_H
#define PLSSCENETEMPLATEMAINSCENEINSTALLVIEW_H

#include <QWidget>
#include "PLSSceneTemplateModel.h"

namespace Ui {
class PLSSceneTemplateMainSceneInstallView;
}

class PLSSceneTemplateMainSceneInstallView : public QWidget
{
    Q_OBJECT

public:
    explicit PLSSceneTemplateMainSceneInstallView(QWidget *parent = nullptr);
    ~PLSSceneTemplateMainSceneInstallView();

public:
    void updateUI(const SceneTemplateItem &model);

private slots:
    void on_installButton_clicked();
	void on_detailButton_clicked();

private:
    Ui::PLSSceneTemplateMainSceneInstallView *ui;
	SceneTemplateItem m_item;
};

#endif // PLSSCENETEMPLATEMAINSCENEINSTALLVIEW_H
