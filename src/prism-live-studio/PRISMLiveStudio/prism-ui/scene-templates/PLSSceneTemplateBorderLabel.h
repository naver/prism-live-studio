#ifndef PLSSCENETEMPLATEBORDERLABEL_H
#define PLSSCENETEMPLATEBORDERLABEL_H

#include <QLabel>

namespace Ui {
class PLSSceneTemplateBorderLabel;
}

class PLSSceneTemplateBorderLabel : public QLabel
{
    Q_OBJECT

public:
    explicit PLSSceneTemplateBorderLabel(QWidget *parent = nullptr);
    ~PLSSceneTemplateBorderLabel();
    void setHasBorder(bool hasBorder);
    void setSceneNameLabel(const QString& sceneName);

private:
    Ui::PLSSceneTemplateBorderLabel *ui;
};

#endif // PLSSCENETEMPLATEBORDERLABEL_H
