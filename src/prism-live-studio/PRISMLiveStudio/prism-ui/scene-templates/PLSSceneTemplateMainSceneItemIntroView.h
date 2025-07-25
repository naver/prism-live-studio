#ifndef PLSSCENETEMPLATEMAINSCENEITEMINTROVIEW_H
#define PLSSCENETEMPLATEMAINSCENEITEMINTROVIEW_H

#include <QWidget>
#include "PLSSceneTemplateModel.h"

namespace Ui {
class PLSSceneTemplateMainSceneItemIntroView;
}

class PLSSceneTemplateMainSceneItemIntroView : public QWidget {
	Q_OBJECT

public:
	explicit PLSSceneTemplateMainSceneItemIntroView(QWidget *parent = nullptr);
	~PLSSceneTemplateMainSceneItemIntroView();

public:
	void updateUI(const SceneTemplateItem &model);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	Ui::PLSSceneTemplateMainSceneItemIntroView *ui;
	SceneTemplateItem m_item;
};

#endif // PLSSCENETEMPLATEMAINSCENEITEMINTROVIEW_H
