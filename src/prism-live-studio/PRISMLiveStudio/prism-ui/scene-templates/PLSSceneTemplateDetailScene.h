#ifndef PLSSCENETEMPLATEDETAILSCENE_H
#define PLSSCENETEMPLATEDETAILSCENE_H

#include <QWidget>
#include "PLSSceneTemplateModel.h"

class PLSSceneTemplateImageView;
class PLSMediaRender;

namespace Ui {
class PLSSceneTemplateDetailScene;
}

class PLSSceneTemplateDetailScene : public QWidget {
	Q_OBJECT

public:
	explicit PLSSceneTemplateDetailScene(QWidget *parent = nullptr);
	~PLSSceneTemplateDetailScene();

private slots:
	void on_installButton_clicked();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void removeImageAndVideoView();
	void updateMainSceneResourcePath(const QString &path, int);
	void setSelectedViewBorder(const QString &path, int);

public:
	void updateUI(const SceneTemplateItem &model);

private:
	Ui::PLSSceneTemplateDetailScene *ui;
	SceneTemplateItem m_item;
	QStringList m_sceneNames;
	QMap<QString, PLSSceneTemplateImageView *> initImageViewCache;
	QMap<QString, PLSMediaRender *> initVideoViewCache;

	PLSSceneTemplateImageView *m_preClickImageView{nullptr};
	PLSMediaRender *m_preClickVideoView{nullptr};

	qint64 m_dtLastInstall = 0;
};

#endif // PLSSCENETEMPLATEDETAILSCENE_H
