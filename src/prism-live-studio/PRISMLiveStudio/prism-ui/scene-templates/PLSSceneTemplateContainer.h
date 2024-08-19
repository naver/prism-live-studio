#ifndef PLSSCENETEMPLATECONTAINER_H
#define PLSSCENETEMPLATECONTAINER_H

#include "PLSSideBarDialogView.h"
#include "loading-event.hpp"

namespace Ui {
class PLSSceneTemplateContainer;
}

class PLSSceneTemplateContainer : public PLSSideBarDialogView {
	Q_OBJECT

public:
	explicit PLSSceneTemplateContainer(DialogInfo info, QWidget *parent = nullptr);
	~PLSSceneTemplateContainer();
	void showMainSceneTemplatePage();
	void showDetailSceneTemplatePage(const SceneTemplateItem &mode);
	void showLoading(QWidget *parent);
	void hideLoading();

protected:
	void hideEvent(QHideEvent *event) override;
	void showEvent(QShowEvent *event) override;

private:
	Ui::PLSSceneTemplateContainer *ui;
	QPointer<QObject> m_pWidgetLoadingBGParent = nullptr;
	QPointer<QWidget> m_pWidgetLoadingBG = nullptr;
	PLSLoadingEvent m_loadingEvent;
};

#endif // PLSSCENETEMPLATECONTAINER_H
