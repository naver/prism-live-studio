#ifndef PLSSCENELISTVIEW_H
#define PLSSCENELISTVIEW_H

#include "PLSSceneItemView.h"
#include "PLSSceneTransitionsView.h"
#include <QFrame>

namespace Ui {
class PLSSceneListView;
}

class PLSSceneListView : public QFrame {
	Q_OBJECT

public:
	explicit PLSSceneListView(QWidget *parent = nullptr);
	~PLSSceneListView() override;
	PLSSceneListView(const PLSSceneListView &) = delete;
	PLSSceneListView &operator=(const PLSSceneListView &) = delete;

	void SetSceneDisplayMethod(int method);
	int GetSceneOrder(const char *name) const;

	void AddScene(const QString &name, OBSScene scene, const SignalContainer<OBSScene> &handler, bool loadingScene = false);
	void DeleteScene(const QString &name);
	void RefreshScene(bool scrollToCurrent = true);
	void MoveSceneToUp();
	void MoveSceneToDown();
	void MoveSceneToTop();
	void MoveSceneToBottom();
	void SetRenderCallback() const;

	QList<PLSSceneItemView *> FindItems(const QString &name) const;

	void SetLoadTransitionsData(obs_data_array_t *transitions, obs_source_t *fadeTransition, const int &transitionDuration, const char *currentTransition, obs_load_source_cb cb,
				    void *private_data);
	void ClearTransition() const;
	void InitTransition(const obs_source_t *transition);
	void SetTransition(obs_source_t *transition);
	void AddTransitionsItem(const std::vector<OBSSource> &transitions) const;
	OBSSource GetCurrentTransition() const;
	obs_source_t *FindTransition(const char *name);
	int GetTransitionDurationValue() const;
	void SetTransitionDurationValue(const int &value);
	OBSSource GetTransitionComboItem(int idx) const;
	int GetTransitionComboBoxCount() const;
	obs_data_array_t *SaveTransitions();
	QSpinBox *GetTransitionDurationSpinBox();
	QComboBox *GetTransitionCombobox();
	void EnableTransitionWidgets(bool enable) const;

	// notify live/record/studio mode status change event
	void OnLiveStatus(bool on);
	void OnRecordStatus(bool on);
	void OnStudioModeStatus(bool on);
	void OnPreviewSceneChanged();

public slots:
	PLSSceneItemView *GetCurrentItem();
	int GetCurrentRow();

	void SetCurrentItem(const PLSSceneItemView *item) const;
	void SetCurrentItem(const QString &name) const;
	void OnSceneSwitchEffectBtnClicked();
	void OnDragFinished();
	void OnDeleteSceneButtonClicked(const PLSSceneItemView *item) const;
	void StartRefreshThumbnailTimer();
	void StopRefreshThumbnailTimer();
	void RefreshSceneThumbnail() const;
	void OnScrollBarVisibleChanged(bool);

protected:
	void showEvent(QShowEvent *event) override;

private slots:
	void OnMouseButtonClicked(const PLSSceneItemView *item) const;
	void OnModifySceneButtonClicked(const PLSSceneItemView *item) const;
	void OnFinishingEditName(const QString &text, PLSSceneItemView *item);
	void contextMenuEvent(QContextMenuEvent *event) override;
	void AsyncRefreshSceneBadge();
	void RefreshSceneBadge();

private:
	void RenameSceneItem(PLSSceneItemView *item, obs_source_t *source, const QString &name);
	void CreateSceneTransitionsView();
signals:
	void SceneRenameFinished();

private:
	Ui::PLSSceneListView *ui;
	PLSSceneTransitionsView *transitionsView{};
	obs_data_array_t *loadTransitions{};
	int loadTransitionDuration;
	QString loadCurrentTransition;

	DisplayMethod displayMethod{DisplayMethod::TextView};
	QTimer *thumbnailTimer;
};

#endif // PLSSCENELISTVIEW_H
