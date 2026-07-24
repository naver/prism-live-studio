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
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void checkMouseEnterEvent();
	void checkMouseLeaveEvent();
	void showHoverUI();       // immediate: show install/video view, hide intro/image
	void startHoverVideo();   // delayed: start video playback (called by timer)
	void performMouseEnterEvent(); // full enter: showHoverUI + startHoverVideo
	void performMouseLeaveEvent();

private:
	Ui::PLSSceneTemplateMainSceneItem *ui;
	SceneTemplateItem m_item;
	bool m_hoverEnter{false};
	QTimer m_checkMouseLeaveTimer;
	QTimer m_performMouseEnterTimer;
};

#endif // PLSSCENETEMPLATEMAINSCENEITEM_H
