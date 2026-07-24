#ifndef PLSSCENETEMPLATEMAINSCENE_H
#define PLSSCENETEMPLATEMAINSCENE_H

#include <QWidget>
#include <QFrame>
#include "flowlayout.h"
#include "loading-event.hpp"
#include "PLSLoadingView.h"

namespace Ui {
class PLSSceneTemplateMainScene;
}

namespace Ui {
class PLSSceneTemplateToast;
}

class PLSSceneTemplateMainSceneItem;

class PLSSceneTemplateToast : public QFrame {
	Q_OBJECT
public:
	explicit PLSSceneTemplateToast(QWidget *parent = nullptr);
	~PLSSceneTemplateToast();

	void customResize();

private:
	Ui::PLSSceneTemplateToast *ui;
	PLSLoadingView *m_loadingView = nullptr;
};

class PLSSceneTemplateMainScene : public QWidget {
	Q_OBJECT

public:
	explicit PLSSceneTemplateMainScene(QWidget *parent = nullptr);
	~PLSSceneTemplateMainScene();
	void showMainSceneView();

private:
	void initFlowLayout();
	void showLoading(QWidget *parent, const char *loadingText = "SceneTemplate.Label.Loading");
	void hideLoading();
	void updateComboBoxList();
	void showRetry();
	void hideRetry();
	void showToast();
	int getMarginTopAndBottom();

public slots:
	void updateSceneList();
	void refreshItems(const QString &groupId);

protected:
	bool eventFilter(QObject *watcher, QEvent *event) override;

private:
	Ui::PLSSceneTemplateMainScene *ui;
	FlowLayout *m_FlowLayout{nullptr};
	QPointer<QObject> m_pWidgetLoadingBGParent = nullptr;
	QPointer<QWidget> m_pWidgetLoadingBG = nullptr;
	PLSLoadingEvent m_loadingEvent;
	QWidget *m_pWidgetRetryContainer = nullptr;
	PLSSceneTemplateToast *m_toast = nullptr;
	bool m_bRefreshing = false;
	bool m_refreshPending = false;
	QString m_pendingGroupId;

	QMap<QString, PLSSceneTemplateMainSceneItem *> m_mapItems;
};

#endif // PLSSCENETEMPLATEMAINSCENE_H
