#ifndef PLSSCENETEMPLATEMAINSCENEITEM_H
#define PLSSCENETEMPLATEMAINSCENEITEM_H

#include <QWidget>
#include "PLSSceneTemplateModel.h"
#include <QTimer>

namespace Ui {
class PLSSceneTemplateMainSceneItem;
}

class PLSSceneTemplateMainSceneItem : public QWidget {
	Q_OBJECT

public:
	explicit PLSSceneTemplateMainSceneItem(QWidget *parent = nullptr);
	~PLSSceneTemplateMainSceneItem();

public:
	void updateUI(const SceneTemplateItem &model);
	const SceneTemplateItem &getItem() const { return m_item; }

protected:
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;

private:
	void checkMouseEnterEvent();
	void checkMouseLeaveEvent();
	void performMouseEnterEvent();
	void performMouseLeaveEvent();

private:
	Ui::PLSSceneTemplateMainSceneItem *ui;
	SceneTemplateItem m_item;
	bool m_hoverEnter{false};
	QTimer m_checkMouseLeaveTimer;
	QTimer m_performMouseEnterTimer;
};

#endif // PLSSCENETEMPLATEMAINSCENEITEM_H
