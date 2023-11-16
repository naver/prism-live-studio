#include "PLSSceneListView.h"
#include "ui_PLSSceneListView.h"

#include "PLSSceneDataMgr.h"
#include "PLSSceneTransitionsView.h"
#include "obs-app.hpp"
#include "qt-wrappers.hpp"
#include "window-namedialog.hpp"
#include "window-basic-main.hpp"
#include "pls-common-language.hpp"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "action.h"
#include "libutils-api.h"
#include "PLSBasic.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QMimeData>
#include <QDropEvent>
#include <QPainter>
#include <QDebug>
#include <QThread>
#include <QDirIterator>
#include <QDir>
using namespace common;
constexpr auto SCENE_REFRESH_THUMBANIL_TIME_MS = 5000;

PLSSceneListView::PLSSceneListView(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSSceneListView>();
	ui->setupUi(this);
	pls_add_css(this, {"PLSScene"});

	setAttribute(Qt::WA_NativeWindow);
#ifdef Q_OS_MACOS
	ui->scrollArea->VerticalScrollBar()->setAttribute(Qt::WA_NativeWindow);
#endif // Q_OS_MACOS

	this->setWindowFlags(windowFlags() ^ Qt::FramelessWindowHint);
	ui->scrollArea->VerticalScrollBar()->setMouseTracking(true);
	connect(ui->scrollArea, &PLSFloatScrollBarScrollArea::ScrollBarVisibleChanged, this, &PLSSceneListView::OnScrollBarVisibleChanged);

	connect(ui->scrollAreaWidgetContents, &PLSScrollAreaContent::DragFinished, this, &PLSSceneListView::OnDragFinished);
	auto func = [this](int xPos, int yPos) {
		Q_UNUSED(xPos)
		ui->scrollArea->VerticalScrollBar()->setValue(ui->scrollArea->VerticalScrollBar()->value() + yPos);
	};
	connect(ui->scrollAreaWidgetContents, &PLSScrollAreaContent::DragMoving, this, func);
	connect(ui->scrollAreaWidgetContents, &PLSScrollAreaContent::resizeEventChanged, this, &PLSSceneListView::RefreshScene, Qt::QueuedConnection);

	CreateSceneTransitionsView();

	thumbnailTimer = pls_new<QTimer>(this);
	connect(thumbnailTimer, &QTimer::timeout, this, &PLSSceneListView::RefreshSceneThumbnail);

	OnLiveStatus(false);
	OnRecordStatus(false);
}

PLSSceneListView::~PLSSceneListView()
{
	if (transitionsView) {
		pls_delete(transitionsView);
		transitionsView = nullptr;
	}

	StopRefreshThumbnailTimer();
	if (thumbnailTimer) {
		pls_delete(thumbnailTimer);
		thumbnailTimer = nullptr;
	}

	pls_delete(ui);
}

void PLSSceneListView::SetSceneDisplayMethod(int method)
{
	if (method < 0 || method > static_cast<int>(DisplayMethod::TextView)) {
		return;
	}

	auto curMethod = static_cast<DisplayMethod>(method);
	if (curMethod == displayMethod) {
		return;
	}

	displayMethod = curMethod;
	SceneDisplayVector vec = PLSSceneDataMgr::Instance()->GetDisplayVector();
	if (vec.empty()) {
		return;
	}
	for (const auto &iter : vec) {
		PLSSceneItemView *item = iter.second;
		if (item) {
			item->SetSceneDisplayMethod(displayMethod);
		}
	}

	if (DisplayMethod::ThumbnailView == displayMethod) {
		RefreshSceneThumbnail();
		StartRefreshThumbnailTimer();
	} else {
		StopRefreshThumbnailTimer();
		RefreshSceneThumbnail();
	}

	this->RefreshScene(true);
}

int PLSSceneListView::GetSceneOrder(const char *name) const
{
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	if (data.empty()) {
		return 0;
	}
	for (int i = 0; i < data.size(); i++) {
		PLSSceneItemView *item = data[i].second;
		if (item && (0 == item->GetName().compare(name))) {
			return i;
		}
	}
	return 0;
}

void PLSSceneListView::AddScene(const QString &name, OBSScene scene, const SignalContainer<OBSScene> &handler, bool loadingScene)
{
	PLSSceneItemView *view = PLSSceneDataMgr::Instance()->FindSceneData(name);
	if (!view) {
		view = pls_new<PLSSceneItemView>(name, scene, displayMethod, ui->scrollAreaWidgetContents);
		view->SetSignalHandler(handler);
		view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		connect(view, &PLSSceneItemView::MouseButtonClicked, this, &PLSSceneListView::OnMouseButtonClicked);
		connect(view, &PLSSceneItemView::ModifyButtonClicked, this, &PLSSceneListView::OnModifySceneButtonClicked);
		connect(view, &PLSSceneItemView::DeleteButtonClicked, this, &PLSSceneListView::OnDeleteSceneButtonClicked);
		connect(view, &PLSSceneItemView::FinishingEditName, this, &PLSSceneListView::OnFinishingEditName, Qt::QueuedConnection);
	}

	PLSSceneDataMgr::Instance()->AddSceneData(name, view);
	if (!loadingScene) {
		SetCurrentItem(view);
	}
}

void PLSSceneListView::DeleteScene(const QString &name)
{
	const PLSSceneItemView *view = PLSSceneDataMgr::Instance()->DeleteSceneData(name);

	PLSBasic *main = PLSBasic::instance();
	if (nullptr == main->GetCurrentSceneItemView()) {
		if (view) {
			obs_source_t *source = obs_scene_get_source(view->GetData());
			main->SetCurrentScene(source);
		}
	}

	this->RefreshScene();
	this->setFocus();
	main->SaveProjectDeferred();
}

PLSSceneItemView *PLSSceneListView::GetCurrentItem()
{
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	if (data.empty()) {
		return nullptr;
	}
	for (const auto &iter : data) {
		PLSSceneItemView *item = iter.second;
		if (item && item->GetCurrentFlag()) {
			return item;
		}
	}
	return nullptr;
}

int PLSSceneListView::GetCurrentRow()
{
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	for (int i = 0; i < data.size(); i++) {
		PLSSceneItemView *item = data[i].second;
		if (item && item->GetCurrentFlag()) {
			return i;
		}
	}

	return -1;
}

void PLSSceneListView::SetCurrentItem(const PLSSceneItemView *item) const
{
	if (!item) {
		return;
	}
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	if (data.empty()) {
		return;
	}
	for (const auto &iter : data) {
		if (0 == strcmp(item->GetName().toStdString().c_str(), iter.first.toStdString().c_str())) {
			PLSBasic::instance()->SetScene(item->GetData());
			iter.second->SetCurrentFlag(true);
		} else {
			iter.second->SetCurrentFlag(false);
		}
	}
}

void PLSSceneListView::SetCurrentItem(const QString &name) const
{
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	if (data.empty()) {
		return;
	}
	for (const auto &iter : data) {
		if (0 == strcmp(iter.first.toStdString().c_str(), name.toStdString().c_str())) {
			SetCurrentItem(iter.second);
			break;
		}
	}
}

QList<PLSSceneItemView *> PLSSceneListView::FindItems(const QString &name) const
{
	QList<PLSSceneItemView *> items;
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	if (data.empty()) {
		return items;
	}
	for (const auto &iter : data) {
		if (0 == strcmp(iter.first.toStdString().c_str(), name.toStdString().c_str())) {
			items.push_back(iter.second);
		}
	}
	return items;
}

void PLSSceneListView::SetLoadTransitionsData(obs_data_array_t *transitions, obs_source_t *fadeTransition, const int &transitionDuration_, const char *currentTransition_, obs_load_source_cb cb,
					      void *private_data)
{
	loadTransitions = transitions;
	loadTransitionDuration = transitionDuration_;
	loadCurrentTransition = currentTransition_;

	if (transitionsView) {
		transitionsView->InitLoadTransition(transitions, fadeTransition, transitionDuration_, currentTransition_, cb, private_data);
	}
}

void PLSSceneListView::ClearTransition() const
{
	if (transitionsView) {
		transitionsView->ClearTransition();
	}
}

void PLSSceneListView::InitTransition(const obs_source_t *transition)
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

void PLSSceneListView::AddTransitionsItem(const std::vector<OBSSource> &transitions) const
{
	if (transitionsView) {
		return transitionsView->AddTransitionsItem(transitions);
	}
}

OBSSource PLSSceneListView::GetCurrentTransition() const
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

int PLSSceneListView::GetTransitionDurationValue() const
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

OBSSource PLSSceneListView::GetTransitionComboItem(int idx) const
{
	if (transitionsView) {
		return transitionsView->GetTransitionByIndex(idx);
	} else {
		return OBSSource();
	}
}

int PLSSceneListView::GetTransitionComboBoxCount() const
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

void PLSSceneListView::EnableTransitionWidgets(bool enable) const
{
	if (transitionsView) {
		transitionsView->EnableTransitionWidgets(enable);
	}
}

void PLSSceneListView::OnLiveStatus(bool)
{
	AsyncRefreshSceneBadge();
}

void PLSSceneListView::OnRecordStatus(bool)
{
	AsyncRefreshSceneBadge();
}

void PLSSceneListView::OnStudioModeStatus(bool)
{
	AsyncRefreshSceneBadge();
}

void PLSSceneListView::OnPreviewSceneChanged()
{
	AsyncRefreshSceneBadge();
}

void PLSSceneListView::RefreshScene(bool scrollToCurrent)
{
	int y = ui->scrollAreaWidgetContents->Refresh(displayMethod, ui->scrollArea->VerticalScrollBar()->isVisible());
	if (!scrollToCurrent) {
		return;
	}

	if (SCENE_ITEM_DO_NOT_NEED_AUTO_SCROLL != y) {
		ui->scrollArea->VerticalScrollBar()->setValue(y);
	}
}

void PLSSceneListView::MoveSceneToUp()
{
	const PLSSceneItemView *item = this->GetCurrentItem();
	if (item) {
		PLSSceneDataMgr::Instance()->SwapToUp(item->GetName());
		RefreshScene();
	}
}

void PLSSceneListView::MoveSceneToDown()
{
	const PLSSceneItemView *item = this->GetCurrentItem();
	if (item) {
		PLSSceneDataMgr::Instance()->SwapToDown(item->GetName());
		RefreshScene();
	}
}

void PLSSceneListView::MoveSceneToTop()
{
	const PLSSceneItemView *item = this->GetCurrentItem();
	if (item) {
		PLSSceneDataMgr::Instance()->SwapToTop(item->GetName());
		RefreshScene();
	}
}

void PLSSceneListView::MoveSceneToBottom()
{
	const PLSSceneItemView *item = this->GetCurrentItem();
	if (item) {
		PLSSceneDataMgr::Instance()->SwapToBottom(item->GetName());
		RefreshScene();
	}
}

void PLSSceneListView::SetRenderCallback() const
{
	SceneDisplayVector vec = PLSSceneDataMgr::Instance()->GetDisplayVector();
	if (vec.empty()) {
		return;
	}
	for (const auto &[key, item] : vec) {
		if (item) {
			item->CustomCreateDisplay();
		}
	}
}

void PLSSceneListView::OnSceneSwitchEffectBtnClicked()
{
	if (!transitionsView) {
		CreateSceneTransitionsView();
	}

	transitionsView->show();
	transitionsView->raise();
}

void PLSSceneListView::OnDragFinished()
{
	this->RefreshScene();

	OBSBasic *main = OBSBasic::Get();
	if (main) {
		main->SaveProjectDeferred();
	}
	OBSProjector::UpdateMultiviewProjectors();
}

void PLSSceneListView::showEvent(QShowEvent *event)
{
	QFrame::showEvent(event);
	ui->scrollAreaWidgetContents->resize(ui->scrollArea->width(), ui->scrollArea->height());
}

void PLSSceneListView::OnMouseButtonClicked(const PLSSceneItemView *item) const
{
	PLSBasic *main = PLSBasic::instance();
	if (main && item) {
		OBSScene scene = item->GetData();
		if (scene == main->GetCurrentScene()) {
			return;
		}
		obs_source_t *source = obs_scene_get_source(scene);
		main->SetCurrentScene(source);
		main->OnScenesCurrentItemChanged();
	}
}

struct DeleteSceneSourceHelper {
	const char *delete_scene_name;        // input param
	std::vector<obs_sceneitem_t *> items; // output list
};

static bool EnumItemForDelScene(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	auto helper = (DeleteSceneSourceHelper *)ptr;

	if (obs_sceneitem_is_group(item)) {
		obs_sceneitem_group_enum_items(item, EnumItemForDelScene, ptr);
		return true;
	}

	const obs_source_t *source = obs_sceneitem_get_source(item);
	if (!source)
		return true;

	const char *id = obs_source_get_id(source);
	if (id && 0 == strcmp(id, SCENE_SOURCE_ID)) {
		const char *name = obs_source_get_name(source);
		if (name && 0 == strcmp(name, helper->delete_scene_name))
			helper->items.push_back(item);
	}

	return true;
}

void DeleteRelatedSceneSource(const char *scene_name)
{
	if (!scene_name) {
		return;
	}

	auto cb = [](void *helper, obs_source_t *src) {
		obs_scene_t *scene = obs_scene_from_source(src);
		if (scene) {
			obs_scene_enum_items(scene, EnumItemForDelScene, helper);
		}
		return true;
	};

	DeleteSceneSourceHelper helper;
	helper.delete_scene_name = scene_name;
	obs_enum_scenes(cb, &helper);

	size_t count = helper.items.size();
	for (size_t i = 0; i < count; i++) {
		obs_sceneitem_remove(helper.items[i]);
	}
}

static bool EnumItemForInteraction(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	if (obs_sceneitem_is_group(item)) {
		obs_sceneitem_group_enum_items(item, EnumItemForInteraction, ptr);
	} else {
		auto main = PLSBasic::instance();
		bool successed = main->CheckHideInteraction(OBSSceneItem(item));
		if (successed) {
			return false;
		}
	}

	return true;
}

void PLSSceneListView::OnDeleteSceneButtonClicked(const PLSSceneItemView *item) const
{
	if (!item) {
		return;
	}
	OBSScene scene = item->GetData();
	OBSBasic *main = OBSBasic::Get();
	if (main) {
		main->DeleteSelectedScene(scene);
	}
}

void PLSSceneListView::OnModifySceneButtonClicked(const PLSSceneItemView *item) const
{
	Q_UNUSED(item)
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
	PLSBasic *main = PLSBasic::instance();
	if (main) {
		const PLSSceneItemView *item = main->GetCurrentSceneItemView();
		if (item) {
			main->OnScenesCustomContextMenuRequested(item);
		}
	}
	QFrame::contextMenuEvent(event);
}

void PLSSceneListView::RefreshSceneThumbnail() const
{
	SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
	if (data.empty()) {
		return;
	}
	int i = 0;
	for (auto iter = data.begin(); iter != data.end(); ++iter, i++) {
		const PLSSceneItemView *item = iter->second;
		if (!item) {
			continue;
		}

		item->RefreshSceneThumbnail();
	}
}

void PLSSceneListView::OnScrollBarVisibleChanged(bool visible)
{
	Q_UNUSED(visible)
	if (displayMethod == DisplayMethod::TextView) {
		QTimer::singleShot(0, this, [this]() { RefreshScene(false); });
	}
}

void PLSSceneListView::AsyncRefreshSceneBadge()
{
	QMetaObject::invokeMethod(this, "RefreshSceneBadge", Qt::QueuedConnection);
}

void PLSSceneListView::RefreshSceneBadge()
{
	pls_check_app_exiting();
	SceneDisplayVector vec = PLSSceneDataMgr::Instance()->GetDisplayVector();
	if (vec.empty()) {
		return;
	}
	for (auto iter = vec.begin(); iter != vec.end(); ++iter) {
		PLSSceneItemView *item = iter->second;
		if (item) {
			item->SetStatusBadge();
		}
	}
}

void PLSSceneListView::RenameSceneItem(PLSSceneItemView *item, obs_source_t *source, const QString &name)
{
	const char *prevName = obs_source_get_name(source);
	if (name == prevName || !item)
		return;

	QString trimmedText = QT_TO_UTF8(name.simplified());

	obs_source_t *foundSource = obs_get_source_by_name(QT_TO_UTF8(trimmedText));
	if (foundSource || trimmedText.isEmpty()) {
		item->SetName(prevName);
		OBSBasic *main = OBSBasic::Get();
		if (foundSource) {
			OBSMessageBox::warning(main, QTStr("Alert.Title"), QTStr("NameExists.Text"));
		} else if (trimmedText.isEmpty()) {
			OBSMessageBox::warning(main, QTStr("Alert.Title"), QTStr("NoNameEntered.Text"));
		}

		obs_source_release(foundSource);
	} else {
		auto undo = [prev = std::string(prevName)](const std::string &data) {
			OBSSourceAutoRelease source = obs_get_source_by_name(data.c_str());
			obs_source_set_name(source, prev.c_str());
		};

		auto redo = [name](const std::string &data) {
			OBSSourceAutoRelease source = obs_get_source_by_name(data.c_str());
			obs_source_set_name(source, name.toStdString().c_str());
		};

		std::string undo_data(name.toStdString());
		std::string redo_data(prevName);
		OBSBasic::Get()->undo_s.add_action(QTStr("Undo.Rename").arg(name.toStdString().c_str()), undo, redo, undo_data, redo_data);

		item->SetName(trimmedText);
		obs_source_set_name(source, trimmedText.toStdString().c_str());
		PLSSceneDataMgr::Instance()->RenameSceneData(prevName, trimmedText);
		emit SceneRenameFinished();
	}
}

void PLSSceneListView::CreateSceneTransitionsView()
{
	OBSBasic *main = OBSBasic::Get();
	transitionsView = pls_new<PLSSceneTransitionsView>(main);
	transitionsView->hide();
}

void PLSSceneListView::StartRefreshThumbnailTimer()
{
	if (!thumbnailTimer) {
		return;
	}

	if (thumbnailTimer->isActive()) {
		return;
	}

	if (displayMethod != DisplayMethod::ThumbnailView) {
		return;
	}

	thumbnailTimer->start(SCENE_REFRESH_THUMBANIL_TIME_MS);
}

void PLSSceneListView::StopRefreshThumbnailTimer()
{
	if (!thumbnailTimer) {
		return;
	}

	if (!thumbnailTimer->isActive()) {
		return;
	}

	thumbnailTimer->stop();
}
