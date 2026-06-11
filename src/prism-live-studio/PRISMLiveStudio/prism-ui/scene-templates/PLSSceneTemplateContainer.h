#ifndef PLSSCENETEMPLATECONTAINER_H
#define PLSSCENETEMPLATECONTAINER_H

#include "PLSSideBarDialogView.h"
#include "loading-event.hpp"
#include <QPixmap>

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

	QPixmap &getAIBadge();
	QPixmap &getAILongBadge();
	QPixmap &getPlusBadge();
	QPixmap &getPlusDetailBadge();

protected:
	void hideEvent(QHideEvent *event) override;
	void showEvent(QShowEvent *event) override;

private:
	Ui::PLSSceneTemplateContainer *ui;
	QPointer<QObject> m_pWidgetLoadingBGParent = nullptr;
	QPointer<QWidget> m_pWidgetLoadingBG = nullptr;
	PLSLoadingEvent m_loadingEvent;

	QPixmap m_pixmapAIBadge, m_pixmapAILongBadge;
	QPixmap m_pixmapPlusBadge, m_pixmapPlusDetailBadge;
};

#endif // PLSSCENETEMPLATECONTAINER_H
