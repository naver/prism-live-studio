#ifndef PLSSCENETRANSITIONSVIEW_H
#define PLSSCENETRANSITIONSVIEW_H

#include <QDialog>
#include <QSpinBox>
#include <QComboBox>
#include <QString>
#include "PLSAlertView.h"
#include "obs.hpp"

namespace Ui {
class PLSSceneTransitionsView;
}

class PLSBasic;

class PLSSceneTransitionsView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSSceneTransitionsView(QWidget *parent = nullptr);
	virtual ~PLSSceneTransitionsView();

	void InitLoadTransition(obs_data_array_t *transitions, obs_source_t *fadeTransition, const int &transitionDuration, const QString &crrentTransition, obs_load_source_cb cb, void *private_data);
	void InitTransition(const obs_source_t *transition);
	obs_source_t *FindTransition(const char *name);
	OBSSource GetCurrentTransition() const;
	OBSSource GetTransitionByIndex(const int &index) const;
	obs_data_array_t *SaveTransitions();
	void LoadTransitions(obs_data_array_t *transitions, obs_load_source_cb cb, void *private_data);

	void AddTransition();
	void RenameTransition();
	void ClearTransition() const;
	void SetTransition(OBSSource transition);
	void AddTransitionsItem(const std::vector<OBSSource> &transitions) const;

	QComboBox *GetTransitionCombobox();
	QSpinBox *GetTransitionDuration();
	int GetTransitionComboBoxCount() const;
	int GetTransitionDurationValue() const;
	void SetTransitionDurationValue(const int &value);

	void EnableTransitionWidgets(bool enable) const;

public slots:
	void on_transitions_currentIndexChanged(int index);
	void on_transitionAdd_clicked();
	void on_transitionRemove_clicked();
	void on_transitionProps_clicked();
	void on_transitionDuration_valueChanged(int value);

protected:
	void showEvent(QShowEvent *event) override;

private:
	Ui::PLSSceneTransitionsView *ui;
	PLSBasic *main{};
	bool initTransitionDuration{false};
};

#endif // PLSSCENETRANSITIONSVIEW_H
