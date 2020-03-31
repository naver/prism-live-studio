#include "PLSSceneListView.h"
#include "ui_PLSSceneListView.h"

#include "PLSSceneDataMgr.h"
#include "PLSSceneTransitionsView.h"
#include "pls-app.hpp"
#include "qt-wrappers.hpp"
#include "window-namedialog.hpp"
#include "window-basic-main.hpp"
#include "pls-common-language.hpp"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "action.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QMimeData>
#include <QDropEvent>
#include <QPainter>
#include <QDebug>
#include <QThread>
#include <QDirIterator>
#include <QDir>

void LogoutCallbackFunc(pls_frontend_event event, const QVariantList &params, void *context)
{
	PLSSceneListView *view = static_cast<PLSSceneListView *>(context);
	QMetaObject::invokeMethod(view, "OnLogoutEvent", Qt::AutoConnection);
}

PLSSceneListView::PLSSceneListView(QWidget *parent) : QFrame(parent), ui(new Ui::PLSSceneListView)
{
	ui->setupUi(this);
	this->setWindowFlags(windowFlags() ^ Qt::FramelessWindowHint);
	connect(ui->scrollAreaWidgetContents, &PLSScrollAreaContent::DragFinished, this, &PLSSceneListView::OnDragFinished);
	pls_frontend_add_event_callback(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGOUT, LogoutCallbackFunc, this);

	CreateSceneTransitionsView();
}

PLSSceneListView::~PLSSceneListView()
{
	pls_frontend_remove_event_callback(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGOUT, LogoutCallbackFunc, this);

	if (transitionsView) {
		delete transitionsView;
		transitionsView = nullptr;
	}
	delete ui;
}

void PLSSceneListView::AddScene(const QString &name, OBSScene scene, SignalContainer<OBSScene> handler)
{
	PLSSceneItemView *view = PLSSceneDataMgr::Instance()->FindSceneData(name);
	if (!view) {
		view = new PLSSceneItemView(name, scene, ui->scrollAreaWidgetContents);
		view->SetSignalHandler(handler);
		view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		connect(view, &PLSSceneItemView::MouseButtonClicked, this, &PLSSceneListView::OnMouseButtonClicked);
		connect(view, &PLSSceneItemView::ModifyButtonClicked, this, &PLSSceneListView::OnModifySceneButtonClicked);
		connect(view, &PLSSceneItemView::DeleteButtonClicked, this, &PLSSceneListView::OnDeleteSceneButtonClicked);
		connect(view, &PLSSceneItemView::FinishingEditName, this, &PLSSceneListView::OnFinishingEditName);
	}

	PLSSceneDataMgr::Instance()->AddSceneData(name, view);
	SetCurrentItem(view);
}

void PLSSceneListView::DeleteScene(const QString &name)
{
	PLSSceneItemView *view = PLSSceneDataMgr::Instance()->DeleteSceneData(name);

	PLSBasic *main = PLSBasic::Get();
	if (view && main) {
		obs_source_t *source = obs_scene_get_source(view->GetData());
		main->SetCurrentScene(source);
	}
	this->RefreshScene();
	main->SaveProjectDeferred();
}

void PLSSceneListView::DeleteAllScene()
{
	QString path = pls_get_user_path("PRISMLiveStudio/basic/scenes/");

	QDir dir(path);
	dir.setFilter(QDir::Files);
	QFileInfoList list = dir.entryInfoList();
	for (int i = 0; i < list.size(); i++) {
		QFileInfo fileInfo = list.at(i);
		if (fileInfo.fileName() == "." | fileInfo.fileName() == "..") {
			continue;
		}

		QString name = fileInfo.fileName();
		name.insert(0, path);
		os_unlink(name.toStdString().c_str());
		name += ".bak";
		os_unlink(name.toStdString().c_str());
	}
}

void PLSSceneListView::OnLogoutEvent()
{
	DeleteAllScene();
	emit LogoutEvent();
}

PLSSceneItemView *PLSSceneListView::GetCurrentItem()
{
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	for (auto iter = data.begin(); iter != data.end(); ++iter) {
		PLSSceneItemView *item = iter->second;
		if (item && item->GetCurrentFlag()) {
			return item;
		}
	}
	return nullptr;
}

void PLSSceneListView::SetCurrentItem(PLSSceneItemView *item)
{
	if (!item) {
		return;
	}
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	for (auto iter = data.begin(); iter != data.end(); ++iter) {
		if (0 == strcmp(item->GetName().toStdString().c_str(), iter->first.toStdString().c_str())) {
			iter->second->SetCurrentFlag(true);
		} else {
			iter->second->SetCurrentFlag(false);
		}
	}
}

void PLSSceneListView::SetCurrentItem(const QString &name)
{
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	for (auto iter = data.begin(); iter != data.end(); ++iter) {
		if (0 == strcmp(iter->first.toStdString().c_str(), name.toStdString().c_str())) {
			SetCurrentItem(iter->second);
			break;
		}
	}
}

QList<PLSSceneItemView *> PLSSceneListView::FindItems(const QString &name)
{
	QList<PLSSceneItemView *> items;
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	for (auto iter = data.begin(); iter != data.end(); ++iter) {
		if (0 == strcmp(iter->first.toStdString().c_str(), name.toStdString().c_str())) {
			items.push_back(iter->second);
		}
	}
	return items;
}

void PLSSceneListView::SetLoadTransitionsData(obs_data_array_t *transitions, obs_source_t *fadeTransition, const int &transitionDuration_, const char *currentTransition_)
{
	loadTransitions = transitions;
	loadTransitionDuration = transitionDuration_;
	loadCurrentTransition = currentTransition_;

	if (transitionsView) {
		transitionsView->InitLoadTransition(transitions, fadeTransition, transitionDuration_, currentTransition_);
	}
}

void PLSSceneListView::ClearTransition()
{
	if (transitionsView) {
		transitionsView->ClearTransition();
	}
}

void PLSSceneListView::InitTransition(obs_source_t *transition)
{
	if (transitionsView) {
		transitionsView->InitTransition(transition);
	}
}

void PLSSceneListView::SetTransition(obs_source_t *transition)
{
	if (transitionsView) {
		return transitionsView->SetTransition(transition);
	}
}

void PLSSceneListView::AddTransitionsItem(std::vector<OBSSource> &transitions)
{
	if (transitionsView) {
		return transitionsView->AddTransitionsItem(transitions);
	}
}

OBSSource PLSSceneListView::GetCurrentTransition()
{
	if (transitionsView) {
		return transitionsView->GetCurrentTransition();
	}
	return nullptr;
}

obs_source_t *PLSSceneListView::FindTransition(const char *name)
{
	if (transitionsView) {
		return transitionsView->FindTransition(name);
	}
	return nullptr;
}

int PLSSceneListView::GetTransitionDurationValue()
{
	if (transitionsView) {
		return transitionsView->GetTransitionDurationValue();
	}
	return SCENE_TRANSITION_DEFAULT_DURATION_VALUE;
}

void PLSSceneListView::SetTransitionDurationValue(const int &value)
{
	if (transitionsView) {
		return transitionsView->SetTransitionDurationValue(value);
	}
}

OBSSource PLSSceneListView::GetTransitionComboItem(int idx)
{
	if (transitionsView) {
		return transitionsView->GetTransitionByIndex(idx);
	} else {
		return OBSSource();
	}
}

int PLSSceneListView::GetTransitionComboBoxCount()
{
	if (transitionsView) {
		return transitionsView->GetTransitionComboBoxCount();
	} else {
		return 0;
	}
}

obs_data_array_t *PLSSceneListView::SaveTransitions()
{
	if (transitionsView) {
		return transitionsView->SaveTransitions();
	}
	return nullptr;
}

QSpinBox *PLSSceneListView::GetTransitionDurationSpinBox()
{
	if (transitionsView) {
		return transitionsView->GetTransitionDuration();
	}
	return nullptr;
}

QComboBox *PLSSceneListView::GetTransitionCombobox()
{
	if (transitionsView) {
		return transitionsView->GetTransitionCombobox();
	}
	return nullptr;
}

void PLSSceneListView::RefreshScene()
{
	ui->scrollAreaWidgetContents->Refresh();
}

void PLSSceneListView::MoveSceneToUp()
{
	PLSSceneItemView *item = this->GetCurrentItem();
	if (item) {
		PLSSceneDataMgr::Instance()->SwapToUp(item->GetName());
		RefreshScene();
	}
}

void PLSSceneListView::MoveSceneToDown()
{
	PLSSceneItemView *item = this->GetCurrentItem();
	if (item) {
		PLSSceneDataMgr::Instance()->SwapToDown(item->GetName());
		RefreshScene();
	}
}

void PLSSceneListView::MoveSceneToTop()
{
	PLSSceneItemView *item = this->GetCurrentItem();
	if (item) {
		PLSSceneDataMgr::Instance()->SwapToTop(item->GetName());
		RefreshScene();
	}
}

void PLSSceneListView::MoveSceneToBottom()
{
	PLSSceneItemView *item = this->GetCurrentItem();
	if (item) {
		PLSSceneDataMgr::Instance()->SwapToBottom(item->GetName());
		RefreshScene();
	}
}

void PLSSceneListView::OnAddSceneButtonClicked()
{
	std::string name;
	QString format{QTStr("Basic.Main.DefaultSceneName.Text")};
	PLS_UI_STEP(MAINSCENE_MODULE, "Add Scene", ACTION_CLICK);

	int i = 2;
	QString placeHolderText = format.arg(i);
	obs_source_t *source = nullptr;
	while ((source = obs_get_source_by_name(QT_TO_UTF8(placeHolderText)))) {
		obs_source_release(source);
		placeHolderText = format.arg(++i);
	}

	bool accepted = NameDialog::AskForName(this, QTStr("Basic.Main.AddSceneDlg.Title"), QTStr("Basic.Main.AddSceneDlg.Text"), name, placeHolderText);
	if (accepted) {
		if (name.empty()) {
			PLSMessageBox::warning(this, QTStr("NoNameEntered.Title"), QTStr("NoNameEntered.Text"));
			OnAddSceneButtonClicked();
			return;
		}

		obs_source_t *source = obs_get_source_by_name(name.c_str());
		if (source) {
			PLSMessageBox::warning(this, QTStr("NameExists.Title"), QTStr("NameExists.Text"));

			obs_source_release(source);
			OnAddSceneButtonClicked();
			return;
		}

		obs_scene_t *scene = obs_scene_create(name.c_str());
		source = obs_scene_get_source(scene);

		PLSBasic *main = PLSBasic::Get();
		if (main) {
			main->SetCurrentScene(source);
		}
		obs_scene_release(scene);
	}
}

void PLSSceneListView::OnSceneSwitchEffectBtnClicked()
{
	PLS_UI_STEP(MAINSCENE_MODULE, "scene transition", ACTION_CLICK);
	if (!transitionsView) {
		CreateSceneTransitionsView();
	}
	transitionsView->show();
	transitionsView->raise();
}

void PLSSceneListView::OnDragFinished()
{
	this->RefreshScene();

	PLSBasic *main = PLSBasic::Get();
	if (main) {
		main->SaveProjectDeferred();
	}
	PLSProjector::UpdateMultiviewProjectors();
}

void PLSSceneListView::RefreshMultiviewLayout(int layout)
{
	MultiviewLayout mLayout = static_cast<MultiviewLayout>(layout);
	int renderNumber = PLSSceneDataMgr::Instance()->ConvertMultiviewLayoutToInt(mLayout);
	if (ui->scrollAreaWidgetContents->GetRenderNum() != renderNumber) {
		ui->scrollAreaWidgetContents->SetRenderNum(renderNumber);
		this->RefreshScene();
	}
}

void PLSSceneListView::resizeEvent(QResizeEvent *event)
{
	QFrame::resizeEvent(event);
}

void PLSSceneListView::showEvent(QShowEvent *event)
{
	QFrame::showEvent(event);
	ui->scrollAreaWidgetContents->resize(ui->scrollArea->width(), ui->scrollArea->height());
}

void PLSSceneListView::OnMouseButtonClicked(PLSSceneItemView *item)
{
	PLSBasic *main = PLSBasic::Get();
	if (main && item) {
		OBSScene scene = item->GetData();
		obs_source_t *source = obs_scene_get_source(scene);
		main->SetCurrentScene(source);
		main->OnScenesCurrentItemChanged();
	}
}

void PLSSceneListView::OnDeleteSceneButtonClicked(PLSSceneItemView *item)
{
	if (!item) {
		return;
	}
	OBSScene scene = item->GetData();
	obs_source_t *source = obs_scene_get_source(scene);

	PLSBasic *main = PLSBasic::Get();
	if (source && main && main->QueryRemoveSource(source))
		obs_source_remove(source);
}

void PLSSceneListView::OnModifySceneButtonClicked(PLSSceneItemView *item)
{
	Q_UNUSED(item);
}

void PLSSceneListView::OnFinishingEditName(const QString &text, PLSSceneItemView *item)
{
	if (!item) {
		return;
	}
	OBSScene scene = item->GetData();
	obs_source_t *source = obs_scene_get_source(scene);
	RenameSceneItem(item, source, text);
}

void PLSSceneListView::contextMenuEvent(QContextMenuEvent *event)
{
	PLSBasic *main = PLSBasic::Get();
	if (main) {
		PLSSceneItemView *item = main->GetCurrentSceneItem();
		if (item) {
			obs_source_t *source = obs_scene_get_source(item->GetData());
			main->SetCurrentScene(source);
			main->OnScenesCustomContextMenuRequested(item);
		}
	}
	QFrame::contextMenuEvent(event);
}

void PLSSceneListView::RenameSceneItem(PLSSceneItemView *item, obs_source_t *source, const QString &name)
{
	const char *prevName = obs_source_get_name(source);
	if (name == prevName || !item)
		return;

	obs_source_t *foundSource = obs_get_source_by_name(QT_TO_UTF8(name));
	if (foundSource || name.isEmpty()) {
		item->SetName(prevName);
		PLSBasic *main = PLSBasic::Get();
		if (foundSource) {
			PLSMessageBox::warning(main, QTStr("NameExists.Title"), QTStr("NameExists.Text"));
		} else if (name.isEmpty()) {
			PLSMessageBox::warning(main, QTStr("NoNameEntered.Title"), QTStr("NoNameEntered.Text"));
		}

		obs_source_release(foundSource);
	} else {
		item->SetName(name);
		obs_source_set_name(source, name.toStdString().c_str());
		PLSSceneDataMgr::Instance()->RenameSceneData(prevName, name);
		emit SceneRenameFinished();
	}
}

void PLSSceneListView::CreateSceneTransitionsView()
{
	PLSBasic *main = PLSBasic::Get();
	transitionsView = new PLSSceneTransitionsView(main);
	transitionsView->hide();
}
