#ifndef PLSSCENETEMPLATEBORDERLABEL_H
#define PLSSCENETEMPLATEBORDERLABEL_H

#include <QLabel>
#include <QPixmap>

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

    void showAIBadge(const QPixmap &pixmap, bool bLongAIBadge);

private:
    Ui::PLSSceneTemplateBorderLabel *ui;
};

#endif // PLSSCENETEMPLATEBORDERLABEL_H
