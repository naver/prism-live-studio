#include "PLSSceneTransitionsView.h"
#include "ui_PLSSceneTransitionsView.h"

#include "qt-wrappers.hpp"
#include "PLSNameDialog.hpp"
#include "obs-app.hpp"
#include "PLSBasic.h"
#include "PLSDialogView.h"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "action.h"
//#include "ChannelCommonFunctions.h"
#include "libutils-api.h"
#include "obs.hpp"
using namespace common;
Q_DECLARE_METATYPE(OBSSource);

PLSSceneTransitionsView::PLSSceneTransitionsView(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::PLSSceneTransitionsView>();

#if defined(Q_OS_MACOS)
	setFixedSize(410, 240);
#elif defined(Q_OS_WIN)
	setFixedSize(410, 280);
#endif

	setupUi(ui);
	pls_add_css(this, {"PLSSceneTransitionsView"});
	setResizeEnabled(false);
	initSize(410, 280);
	this->setWindowTitle(QTStr("Basic.Hotkeys.SelectScene"));
	connect(ui->okBtn, &QPushButton::clicked, this, &PLSSceneTransitionsView::close);
}

PLSSceneTransitionsView::~PLSSceneTransitionsView()
{
	pls_delete(ui);
}

void PLSSceneTransitionsView::InitLoadTransition(obs_data_array_t *transitions, obs_source_t *fadeTransition, const int &transitionDuration, const QString &crrentTransition, obs_load_source_cb cb,
						 void *private_data)
{
	if (transitions)
		LoadTransitions(transitions, cb, private_data);

	obs_source_t *curTransition = FindTransition(crrentTransition.toStdString().c_str());
	if (!curTransition)
		curTransition = fadeTransition;

	SetTransitionDurationValue(transitionDuration);
	SetTransition(curTransition);
}

void PLSSceneTransitionsView::InitTransition(const obs_source_t *transition)
{
	auto onTransitionStop = [](void *data, calldata_t *) {
		auto window = (PLSBasic *)data;
		QMetaObject::invokeMethod(window, "TransitionStopped", Qt::QueuedConnection);
	};

	auto onTransitionFullStop = [](void *data, calldata_t *) {
		auto window = (PLSBasic *)data;
		QMetaObject::invokeMethod(window, "TransitionFullyStopped", Qt::QueuedConnection);
	};

	signal_handler_t *handler = obs_source_get_signal_handler(transition);
	main = PLSBasic::instance();

	signal_handler_connect(handler, "transition_video_stop", onTransitionStop, main);
	signal_handler_connect(handler, "transition_stop", onTransitionFullStop, main);
}

obs_source_t *PLSSceneTransitionsView::FindTransition(const char *name)
{
	for (int i = 0; i < ui->transitions->count(); i++) {
		OBSSource tr = ui->transitions->itemData(i).value<OBSSource>();

		const char *trName = obs_source_get_name(tr);
		if (strcmp(trName, name) == 0)
			return tr;
	}
	return nullptr;
}

OBSSource PLSSceneTransitionsView::GetCurrentTransition() const
{
	return ui->transitions->currentData().value<OBSSource>();
}

OBSSource PLSSceneTransitionsView::GetTransitionByIndex(const int &index) const
{
	return ui->transitions->itemData(index).value<OBSSource>();
}

obs_data_array_t *PLSSceneTransitionsView::SaveTransitions()
{
	obs_data_array_t *transitions = obs_data_array_create();

	for (int i = 0; i < ui->transitions->count(); i++) {
		OBSSource tr = ui->transitions->itemData(i).value<OBSSource>();
		if (!obs_source_configurable(tr))
			continue;

		obs_data_t *sourceData = obs_data_create();
		obs_data_t *settings = obs_source_get_settings(tr);

		obs_data_set_string(sourceData, "name", obs_source_get_name(tr));
		obs_data_set_string(sourceData, "id", obs_obj_get_id(tr));
		obs_data_set_obj(sourceData, "settings", settings);

		obs_data_array_push_back(transitions, sourceData);

		obs_data_release(settings);
		obs_data_release(sourceData);
	}

	return transitions;
}

void PLSSceneTransitionsView::LoadTransitions(obs_data_array_t *transitions, obs_load_source_cb cb, void *private_data)
{
	size_t count = obs_data_array_count(transitions);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(transitions, i);
		const char *name = obs_data_get_string(item, "name");
		const char *id = obs_data_get_string(item, "id");
		obs_data_t *settings = obs_data_get_obj(item, "settings");

		obs_source_t *source = obs_source_create_private(id, name, settings);
		if (!obs_obj_invalid(source)) {
			InitTransition(source);
			ui->transitions->addItem(QT_UTF8(name), QVariant::fromValue(OBSSource(source)));
			ui->transitions->setCurrentIndex(ui->transitions->count() - 1);
		}

		obs_data_release(settings);
		obs_data_release(item);
		obs_source_release(source);
		if (cb)
			cb(private_data, source);
	}
}

void PLSSceneTransitionsView::AddTransition()
{
	auto action = dynamic_cast<QAction *>(sender());
	QString idStr = action->property("id").toString();

	std::string name;
	QString placeHolderText = QT_UTF8(obs_source_get_display_name(QT_TO_UTF8(idStr)));
	QString format = placeHolderText + " (%1)";
	obs_source_t *source = nullptr;
	int i = 1;

	source = FindTransition(QT_TO_UTF8(placeHolderText));
	while (source) {
		++i;
		placeHolderText = format.arg(i);
		source = FindTransition(QT_TO_UTF8(placeHolderText));
	}

	bool accepted = PLSNameDialog::AskForName(this, QTStr("TransitionNameDlg.Title"), QTStr("TransitionNameDlg.Text"), name, placeHolderText);
	name = QString(name.c_str()).simplified().toStdString();
	if (accepted) {
		if (name.empty()) {
			OBSMessageBox::warning(this, QTStr("Alert.Title"), QTStr("NoNameEntered.Text"));
			AddTransition();
			return;
		}

		source = FindTransition(name.c_str());
		if (source) {
			OBSMessageBox::warning(this, QTStr("Alert.Title"), QTStr("NameExists.Text"));

			AddTransition();
			return;
		}

		source = obs_source_create_private(QT_TO_UTF8(idStr), name.c_str(), nullptr);
		InitTransition(source);
		ui->transitions->addItem(QT_UTF8(name.c_str()), QVariant::fromValue(OBSSource(source)));
		ui->transitions->setCurrentIndex(ui->transitions->count() - 1);
		main = PLSBasic::instance();
		if (main) {
			main->CreatePropertiesWindow(source, OPERATION_NONE);
			main->OnTransitionAdded();
		}
		obs_source_release(source);
	}
}

void PLSSceneTransitionsView::RenameTransition()
{
	auto action = dynamic_cast<QAction *>(sender());
	QVariant variant = action->property("transition");
	obs_source_t *transition = variant.value<OBSSource>();

	std::string name;
	QString placeHolderText = QT_UTF8(obs_source_get_name(transition));

	bool accepted = PLSNameDialog::AskForName(this, QTStr("TransitionNameDlg.Title"), QTStr("TransitionNameDlg.Text"), name, placeHolderText);
	name = QString(name.c_str()).simplified().toStdString();

	if (accepted) {
		if (name.empty()) {
			OBSMessageBox::warning(this, QTStr("Alert.Title"), QTStr("NoNameEntered.Text"));
			RenameTransition();
			return;
		}

		if (nullptr != FindTransition(name.c_str())) {
			OBSMessageBox::warning(this, QTStr("Alert.Title"), QTStr("NameExists.Text"));

			RenameTransition();
			return;
		}

		obs_source_set_name(transition, name.c_str());
		int idx = ui->transitions->findData(variant);
		if (idx != -1) {
			ui->transitions->setItemText(idx, QT_UTF8(name.c_str()));
			main = PLSBasic::instance();
			if (main) {
				main->OnTransitionRenamed();
			}
		}
	}
}

void PLSSceneTransitionsView::ClearTransition() const
{
	ui->transitions->clear();
}

static inline void SetComboTransition(QComboBox *combo, obs_source_t *tr)
{
	int idx = combo->findData(QVariant::fromValue<OBSSource>(tr));
	if (idx != -1) {
		combo->blockSignals(true);
		combo->setCurrentIndex(idx);
		combo->blockSignals(false);
	}
}

void PLSSceneTransitionsView::SetTransition(OBSSource transition)
{
	obs_source_t *oldTransition = obs_get_output_source(0);

	if (oldTransition && transition) {
		obs_transition_swap_begin(transition, oldTransition);
		if (transition != GetCurrentTransition())
			SetComboTransition(ui->transitions, transition);
		obs_set_output_source(0, transition);
		obs_transition_swap_end(transition, oldTransition);
	} else {
		obs_set_output_source(0, transition);
	}

	if (oldTransition)
		obs_source_release(oldTransition);

	bool fixed = transition ? obs_transition_fixed(transition) : false;
	ui->transitionDurationLabel->setVisible(!fixed);
	ui->transitionDuration->setVisible(!fixed);

	bool configurable = transition ? obs_source_configurable(transition) : false;
	ui->transitionRemove->setEnabled(configurable);
	ui->transitionProps->setEnabled(configurable);

	main = PLSBasic::instance();
	if (main) {
		main->OnTransitionSet();
	}
}

void PLSSceneTransitionsView::AddTransitionsItem(const std::vector<OBSSource> &transitions) const
{
	for (const OBSSource &tr : transitions) {
		ui->transitions->addItem(QT_UTF8(obs_source_get_name(tr)), QVariant::fromValue(OBSSource(tr)));
	}
	ui->transitions->setCurrentIndex(0);
}

QComboBox *PLSSceneTransitionsView::GetTransitionCombobox()
{
	return ui->transitions;
}

QSpinBox *PLSSceneTransitionsView::GetTransitionDuration()
{
	return ui->transitionDuration;
}

int PLSSceneTransitionsView::GetTransitionComboBoxCount() const
{
	return ui->transitions->count();
}

int PLSSceneTransitionsView::GetTransitionDurationValue() const
{
	return ui->transitionDuration->value();
}

void PLSSceneTransitionsView::SetTransitionDurationValue(const int &value)
{
	initTransitionDuration = true;
	ui->transitionDuration->setValue(value);
	initTransitionDuration = false;
}

void PLSSceneTransitionsView::EnableTransitionWidgets(bool enable) const
{
	ui->transitions->setEnabled(enable);
	ui->transitionAdd->setEnabled(enable);
	ui->transitionDuration->setEnabled(enable);

	if (!enable) {
		ui->transitionProps->setEnabled(false);
		ui->transitionRemove->setEnabled(false);
	} else {
		bool configurable = obs_source_configurable(GetCurrentTransition());
		ui->transitionProps->setEnabled(configurable);
		ui->transitionRemove->setEnabled(configurable);
	}

	return;
}

void PLSSceneTransitionsView::on_transitions_currentIndexChanged(int index)
{
	Q_UNUSED(index)
	OBSSource transition = GetCurrentTransition();
	SetTransition(transition);
}

void PLSSceneTransitionsView::on_transitionAdd_clicked()
{
	bool foundConfigurableTransitions = false;
	QMenu menu(this);

	size_t idx = 0;
	const char *id;

	while (obs_enum_transition_types(idx, &id)) {
		if (obs_is_source_configurable(id)) {
			const char *name = obs_source_get_display_name(id);
			auto action = pls_new<QAction>(name, this);
			action->setProperty(PROPERTY_NAME_ID, id);

			connect(action, &QAction::triggered, this, &PLSSceneTransitionsView::AddTransition);

			menu.addAction(action);
			foundConfigurableTransitions = true;
		}
		idx++;
	}

	if (foundConfigurableTransitions)
		menu.exec(QCursor::pos());
}

void PLSSceneTransitionsView::on_transitionRemove_clicked()
{
	OBSSource tr = GetCurrentTransition();

	main = PLSBasic::instance();
	if (!tr || !obs_source_configurable(tr) || !main || !main->QueryRemoveSource(tr, this))
		return;

	int idx = ui->transitions->findData(QVariant::fromValue<OBSSource>(tr));
	if (idx == -1)
		return;

	ui->transitions->removeItem(idx);
	main->onTransitionRemoved(tr);
}

void PLSSceneTransitionsView::on_transitionProps_clicked()
{
	OBSSource source = GetCurrentTransition();

	if (!obs_source_configurable(source))
		return;

	auto properties = [this, source]() {
		main = PLSBasic::instance();
		main->CreatePropertiesWindow(source, OPERATION_NONE /*, this*/);
	};

	QMenu menu(this);
	auto action = pls_new<QAction>(QTStr("Rename"), &menu);
	connect(action, &QAction::triggered, this, &PLSSceneTransitionsView::RenameTransition);
	action->setProperty(PROPERTY_NAME_TRANSITION, QVariant::fromValue(source));
	menu.addAction(action);

	action = pls_new<QAction>(QTStr("Transition.Properties"), &menu);
	connect(action, &QAction::triggered, properties);
	menu.addAction(action);

	menu.exec(QCursor::pos());
}

void PLSSceneTransitionsView::on_transitionDuration_valueChanged(int value)
{
	main = PLSBasic::instance();
	if (main) {
		main->OnTransitionDurationValueChanged(value);
	}
}

void PLSSceneTransitionsView::showEvent(QShowEvent *event)
{
	PLSDialogView::showEvent(event);
}
