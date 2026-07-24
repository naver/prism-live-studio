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
#include "PLSErrorHandler.h"
#include "frontend-api.h"
#include "obs.hpp"
#include "PLSSceneTransitionBuiltin.h"
#include <pls/pls-source.h>
using namespace common;
Q_DECLARE_METATYPE(OBSSource);

// PRISM_PC-5670: ReplaceTransitionAtIndex / merge append use obs_source_get_name for combo text; scene JSON stores prism_builtin_default (see pls::kPrismBuiltinDefaultTransitionKey).

static int FindConfigurableBuiltinSlotIndex(QComboBox *combo, const char *id)
{
	if (!combo || !id || !*id)
		return -1;
	for (int j = 0; j < combo->count(); j++) {
		OBSSource tr = combo->itemData(j).value<OBSSource>();
		if (!tr)
			continue;
		const char *trId = obs_obj_get_id(tr);
		if (!trId || strcmp(trId, id) != 0)
			continue;
		if (pls::IsPrismBuiltinDefaultMarkedInPrivateSettings(tr))
			return j;
	}
	return -1;
}

static int FindCutFadeDefaultRowIndex(QComboBox *combo, const char *id)
{
	if (!combo || !id)
		return -1;
	if (strcmp(id, "cut_transition") != 0 && strcmp(id, "fade_transition") != 0)
		return -1;
	for (int j = 0; j < combo->count(); j++) {
		OBSSource tr = combo->itemData(j).value<OBSSource>();
		if (!tr)
			continue;
		if (strcmp(obs_obj_get_id(tr), id) != 0)
			continue;
		if (!obs_source_configurable(tr))
			return j;
	}
	return -1;
}

/** Cut/Fade row or marked configurable built-in row for this id (from InitDefaultTransitions). */
static int FindDefaultSlotRowForId(QComboBox *combo, const char *id)
{
	if (!id)
		return -1;
	if (!strcmp(id, "cut_transition") || !strcmp(id, "fade_transition"))
		return FindCutFadeDefaultRowIndex(combo, id);
	if (pls::IsPrismFiveMarkedBuiltinTransitionId(id))
		return FindConfigurableBuiltinSlotIndex(combo, id);
	return -1;
}

PLSSceneTransitionsView::PLSSceneTransitionsView(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::PLSSceneTransitionsView>();

#if defined(Q_OS_MACOS)
	setFixedSize(410, 280 - PLS_TITLE_BAR_HEIGHT);
#elif defined(Q_OS_WIN)
	setFixedSize(410, 280);
#endif
	setupUi(ui);
	pls_add_css(this, {"PLSSceneTransitionsView"});
	setResizeEnabled(false);
	initSize(410, 280);
	pls_uistep_v2_set_name(ui->transitions, "transitions");
	pls_uistep_v2_set_name(ui->transitionDuration, "transition duration");
	this->setWindowTitle(QTStr("Basic.Hotkeys.SelectScene"));
	connect(ui->okBtn, &QPushButton::clicked, this, &PLSSceneTransitionsView::close);

	pls_uistep_v2_set_value(ui->transitionAdd, QStringLiteral("Add"));
	pls_uistep_v2_set_value(ui->transitionProps, QStringLiteral("More"));
	pls_uistep_v2_set_value(ui->transitionRemove, QStringLiteral("Delete"));
}

PLSSceneTransitionsView::~PLSSceneTransitionsView()
{
	pls_delete(ui);
}

void PLSSceneTransitionsView::InitLoadTransition(obs_data_array_t *transitions, obs_source_t *fadeTransition, const int &transitionDuration, const QString &currentTransition, obs_load_source_cb cb,
						 void *private_data)
{
	// InitDefaultTransitions already created all types; merge saved rows per PRISM_PC-5670 rules.
	if (transitions && obs_data_array_count(transitions) > 0)
		MergeSavedTransitions(transitions, cb, private_data);

	obs_source_t *curTransition = FindTransition(currentTransition.toUtf8().constData());
	if (!curTransition)
		curTransition = fadeTransition;

	SetTransitionDurationValue(transitionDuration);
	SetTransition(curTransition);
}

//PRISM/review-8564: itemData holds OBSSource (OBSSafeRef); removeItem destroys the QVariant and releases the old source ref — do not obs_source_release the prior item separately.
static void ReplaceTransitionAtIndex(QComboBox *combo, int idx, obs_source_t *source, obs_load_source_cb cb, void *private_data)
{
	combo->removeItem(idx);
	combo->insertItem(idx, QT_UTF8(obs_source_get_name(source)), QVariant::fromValue(OBSSource(source)));
	if (cb)
		cb(private_data, source);
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
		if (pls::IsPrismBuiltinDefaultMarkedInPrivateSettings(tr))
			obs_data_set_bool(sourceData, pls::kPrismBuiltinDefaultTransitionKey, true);

		obs_data_array_push_back(transitions, sourceData);

		obs_data_release(settings);
		obs_data_release(sourceData);
	}

	return transitions;
}

void PLSSceneTransitionsView::mergeOneSavedTransitionItem(obs_data_t *item, obs_load_source_cb cb, void *private_data)
{
	QComboBox *combo = ui->transitions;
	const char *name = obs_data_get_string(item, "name");
	const char *id = obs_data_get_string(item, "id");
	obs_data_t *settings = obs_data_get_obj(item, "settings");
	const bool savedMarker = obs_data_get_bool(item, pls::kPrismBuiltinDefaultTransitionKey);

	if (!id || !*id) {
		obs_data_release(settings);
		return;
	}

	// A: Saved id+name matches a default slot row (same obs_source name as InitDefaultTransitions) — replace and mark the five configurable types when applicable.
	{
		const int defRow = FindDefaultSlotRowForId(combo, id);
		if (defRow >= 0 && name && *name) {
			OBSSource existing = combo->itemData(defRow).value<OBSSource>();
			const char *existingName = existing ? obs_source_get_name(existing) : nullptr;
			if (existingName && strcmp(name, existingName) == 0) {
				obs_source_t *source = obs_source_create_private(id, name, settings);
				if (obs_obj_invalid(source)) {
					obs_source_release(source);
					obs_data_release(settings);
					return;
				}
				InitTransition(source);
				if (pls::IsPrismFiveMarkedBuiltinTransitionId(id))
					pls::MarkPrismBuiltinDefaultTransition(source);
				ReplaceTransitionAtIndex(combo, defRow, source, cb, private_data);
				obs_source_release(source);
				obs_data_release(settings);
				return;
			}
		}
	}

	// B: Marked configurable default (prism_builtin_default in scene JSON). Replace the marked combo row when
	// the saved name differs from that row (renamed default). When names match, B is skipped on purpose: A handles
	// the same-name default slot; otherwise C runs and reapplies savedMarker via MarkPrismBuiltinDefaultTransition
	// below — no missing marker.
	if (pls::IsPrismFiveMarkedBuiltinTransitionId(id) && name && savedMarker) {
		const int markedRow = FindConfigurableBuiltinSlotIndex(combo, id);
		if (markedRow >= 0) {
			OBSSource existing = combo->itemData(markedRow).value<OBSSource>();
			const char *existingName = existing ? obs_source_get_name(existing) : nullptr;
			if (existingName && strcmp(name, existingName) != 0) {
				obs_source_t *source = obs_source_create_private(id, name, settings);
				if (obs_obj_invalid(source)) {
					obs_source_release(source);
					obs_data_release(settings);
					return;
				}
				InitTransition(source);
				pls::MarkPrismBuiltinDefaultTransition(source);
				ReplaceTransitionAtIndex(combo, markedRow, source, cb, private_data);
				obs_source_release(source);
				obs_data_release(settings);
				return;
			}
		}
	}

	// C: Otherwise replace an existing row with same id+name, or append.
	obs_source_t *source = obs_source_create_private(id, name, settings);
	// obs_obj_invalid is true if source is null or plugin create left context.data null; release the shell in both cases.
	if (obs_obj_invalid(source)) {
		obs_source_release(source);
		obs_data_release(settings);
		return;
	}

	InitTransition(source);
	if (savedMarker && pls::IsPrismFiveMarkedBuiltinTransitionId(id))
		pls::MarkPrismBuiltinDefaultTransition(source);

	int replaceIdx = -1;
	for (int j = 0; j < combo->count(); j++) {
		OBSSource tr = combo->itemData(j).value<OBSSource>();
		const char *trId = obs_obj_get_id(tr);
		if (tr && trId && strcmp(trId, id) == 0 && name && strcmp(obs_source_get_name(tr), name) == 0) {
			replaceIdx = j;
			break;
		}
	}
	if (replaceIdx >= 0) {
		ReplaceTransitionAtIndex(combo, replaceIdx, source, cb, private_data);
	} else {
		combo->addItem(QT_UTF8(obs_source_get_name(source)), QVariant::fromValue(OBSSource(source)));
		if (cb)
			cb(private_data, source);
	}

	obs_data_release(settings);
	obs_source_release(source);
}

void PLSSceneTransitionsView::LoadTransitions(obs_data_array_t *transitions, obs_load_source_cb cb, void *private_data)
{
	MergeSavedTransitions(transitions, cb, private_data);
}

void PLSSceneTransitionsView::MergeSavedTransitions(obs_data_array_t *saved, obs_load_source_cb cb, void *private_data)
{
	size_t count = obs_data_array_count(saved);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(saved, i);
		mergeOneSavedTransitionItem(item, cb, private_data);
		obs_data_release(item);
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
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_NONAMEENTERED_TEXT, PLSErrKeyAllAlert, QString(),
							      PLSErrorHandler::ExtraData("PLSSceneTransitionsView::AddTransition.empty"), this);
			AddTransition();
			return;
		}

		source = FindTransition(name.c_str());
		if (source) {
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_NAMEEXISTS_TEXT, PLSErrKeyAllAlert, QString(),
							      PLSErrorHandler::ExtraData("PLSSceneTransitionsView::AddTransition.exists"), this);

			AddTransition();
			return;
		}

		source = obs_source_create_private(QT_TO_UTF8(idStr), name.c_str(), nullptr);
		if (!pls_source_context_data_valid(source)) {
			obs_source_release(source);
			return;
		}
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
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_NONAMEENTERED_TEXT, PLSErrKeyAllAlert, QString(),
							      PLSErrorHandler::ExtraData("PLSSceneTransitionsView::RenameTransition.empty"), this);
			RenameTransition();
			return;
		}

		if (nullptr != FindTransition(name.c_str())) {
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_NAMEEXISTS_TEXT, PLSErrKeyAllAlert, QString(),
							      PLSErrorHandler::ExtraData("PLSSceneTransitionsView::RenameTransition.exists"), this);

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

/** Built-in / non-removable: Cut, Fade (non-configurable), or configurable defaults marked in private settings. */
static bool IsDefaultTransition(obs_source_t *source)
{
	if (!source)
		return false;
	if (!obs_source_configurable(source))
		return true;
	return pls::IsPrismBuiltinDefaultMarkedInPrivateSettings(source);
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
	ui->transitionRemove->setEnabled(transition && !IsDefaultTransition(transition));
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
		OBSSource current = GetCurrentTransition();
		bool configurable = obs_source_configurable(current);
		ui->transitionProps->setEnabled(configurable);
		ui->transitionRemove->setEnabled(current && !IsDefaultTransition(current));
	}
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
	pls_uistep_v2_set_custom_show_hide_name(&menu, "Add transition menu");

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
	if (!tr || IsDefaultTransition(tr) || !main || !main->QueryRemoveSource(tr, this))
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
	pls_uistep_v2_set_custom_show_hide_name(&menu, "transition props");
	auto action = pls_new<QAction>(QTStr("Rename"), &menu);
	pls_uistep_v2_set_value(action, QStringLiteral("Rename"));
	connect(action, &QAction::triggered, this, &PLSSceneTransitionsView::RenameTransition);
	action->setProperty(PROPERTY_NAME_TRANSITION, QVariant::fromValue(source));
	action->setEnabled(!IsDefaultTransition(source));
	menu.addAction(action);

	action = pls_new<QAction>(QTStr("Transition.Properties"), &menu);
	pls_uistep_v2_set_value(action, QStringLiteral("Transition properties"));
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
