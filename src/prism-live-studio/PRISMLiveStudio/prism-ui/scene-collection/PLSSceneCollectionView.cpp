#include "PLSSceneCollectionView.h"
#include "ui_PLSSceneCollectionView.h"
#include "obs-app.hpp"
#include "window-basic-main.hpp"
#include "json-data-handler.hpp"
#include <QListWidgetItem>
#include <QAbstractItemView>
#include <QMimeData>
#include <QDrag>
#include <QPainter>
#include "pls-common-define.hpp"
#include <libui.h>
#include <QToolTip>

using namespace common;

static const QString SCENE_COLLECTION_DRAG_MIME_TYPE = "sceneCollectionItem";
static const int FIX_SCENE_COLLECTION_ITEM_HEIGHT = 40;
static constexpr const char *SCENE_COLLECTION_CONFIG = "sceneCollection.config";

namespace {
struct LocalGlobal {
	static QPointer<PLSSceneCollectionItem> dropLineItem;
};
QPointer<PLSSceneCollectionItem> LocalGlobal::dropLineItem = nullptr;
}

PLSSceneCollectionListView::PLSSceneCollectionListView(QWidget *parent) : QListView(parent)
{
	auto model = pls_new<PLSSceneCollectionModel>(this);
	PLSSceneCollectionListView::setModel(model);

	scrollBar = pls_new<PLSCommonScrollBar>(this);
	setVerticalScrollBar(scrollBar);
	setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	connect(scrollBar, &PLSCommonScrollBar::isShowScrollBar, this, [this](bool show) { emit ScrollBarShow(show); });
}

PLSSceneCollectionView::PLSSceneCollectionView(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::PLSSceneCollectionView>();
	setupUi(ui);
	pls_add_css(this, {"PLSSceneCollectionView"});

#if defined(Q_OS_MACOS)
	initSize(QSize(580, 630));
#elif defined(Q_OS_WIN)
	initSize(QSize(580, 670));
#endif

	this->setWindowTitle(QTStr("Scene.Collection.View.Management"));
	ui->stackedWidget->setCurrentWidget(ui->itemPage);
	ui->searchListView->SetEnableDrops(false);
	ui->listView->SetEnableDrops(true);
	ui->pushButton->hide();
	ui->macHelpBtn->hide();

	ui->newBtn->installEventFilter(this);
	ui->newLabel->installEventFilter(this);
	ui->importBtn->installEventFilter(this);
	ui->importLabel->installEventFilter(this);

#ifdef Q_OS_MACOS
	ui->macHelpBtn->setVisible(true);
	ui->macHelpBtn->installEventFilter(this);
#endif // Q_OS_MACOS

	pls_flush_style(ui->searchLineEdit, "usedFor", "collection");
	setHasHelpButton(true);
	setHelpButtonToolTip(QTStr("Scene.Collection.View.Help.Tooltip"), 16);

	ui->newLabel->setProperty("showHandCursor", true);
	ui->importLabel->setProperty("showHandCursor", true);

	setAttribute(Qt::WA_AlwaysShowToolTips, true);

	connect(ui->importBtn, &QPushButton::clicked, this, &PLSSceneCollectionView::OnImportButtonClicked);
	connect(ui->closeBtn, &QPushButton::clicked, this, [this]() { close(); });
	connect(ui->searchLineEdit, &PLSSearchLineEdit::SearchTrigger, this, &PLSSceneCollectionView::OnSearchTriggerd, Qt::QueuedConnection);
	connect(ui->searchLineEdit, &PLSSearchLineEdit::textChanged, this, &PLSSceneCollectionView::OnSearchTriggerd, Qt::QueuedConnection);
	connect(ui->listView, &PLSSceneCollectionListView::RowChanged, this, &PLSSceneCollectionView::OnSceneCollectionItemRowChanged);
	connect(ui->listView, &PLSSceneCollectionListView::ScrollBarShow, this, &PLSSceneCollectionView::OnScrollBarShow);
	connect(
		this, &PLSSceneCollectionView::newButtonClicked, this,
		[this]() {
			if (auto basic = OBSBasic::Get(); basic)
				basic->on_actionNewSceneCollection_triggered_with_parent(this);
		},
		Qt::QueuedConnection);
	connect(this, &PLSSceneCollectionView::importButtonClicked, this, &PLSSceneCollectionView::OnImportButtonClicked, Qt::QueuedConnection);
}

PLSSceneCollectionView::~PLSSceneCollectionView()
{
	pls_delete(ui);
}

void PLSSceneCollectionView::AddSceneCollectionItem(const QString &name, const QString &path, QString userLocalPath) const
{
	PLSSceneCollectionData data;
	data.fileName = name;
	data.filePath = path;
	data.userLocalPath = userLocalPath;
	ui->listView->Add(data);

	if (ui->stackedWidget->currentWidget() == ui->searchPage && name.contains(ui->searchLineEdit->text(), Qt::CaseInsensitive)) {
		ui->searchListView->Add(data);
	}

	if (ui->stackedWidget->currentWidget() == ui->noDataPage && name.contains(ui->searchLineEdit->text(), Qt::CaseInsensitive)) {
		OnSearchTriggerd(name);
	}

	UpdateDeleteButtonState();
	WriteSceneCollectionConfig();
}

void PLSSceneCollectionView::InitDefaultCollectionItem(QVector<PLSSceneCollectionData> &datas) const
{
	InitSceneCollectionConfig(datas);
	ui->listView->InitWidgets(datas);

	UpdateDeleteButtonState();
	WriteSceneCollectionConfig();
}

QVector<PLSSceneCollectionData> PLSSceneCollectionView::GetDatas() const
{
	return ui->listView->GetDatas();
}

void PLSSceneCollectionView::UpdateDeleteButtonState() const
{
	UpdateListViewDelBtnStatus(ui->listView, ui->listView->GetDatas());

	if (ui->stackedWidget->currentWidget() == ui->searchPage) {
		UpdateListViewDelBtnStatus(ui->searchListView, ui->listView->GetDatas());
	}
}

void PLSSceneCollectionView::UpdateTimeStampLabel() const
{
	ui->listView->UpdateCurrentTimeStampLabel();

	if (ui->stackedWidget->currentWidget() == ui->searchPage) {
		ui->searchListView->UpdateCurrentTimeStampLabel();
	}
}

void PLSSceneCollectionView::SetCurrentItem(const QString &name, const QString &path)
{
	SetCurrentText(name, path);

	emit currentSceneCollectionChanged(name, path);
}

void PLSSceneCollectionView::SetCurrentText(const QString &name, const QString &path)
{
	ui->listView->SetCurrentData(name, path);

	if (ui->stackedWidget->currentWidget() == ui->searchPage) {
		ui->searchListView->SetCurrentData(name, path);
	}
	repaint();
}

void PLSSceneCollectionView::AddCollectionUserLocalPath(const QString &name, const QString &userLocalPath) const
{
	QVector<PLSSceneCollectionData> datas = GetDatas();
	for (int i = 0; i < datas.count(); i++) {
		PLSSceneCollectionData data = datas.at(i);
		if (0 == data.fileName.compare(name)) {
			ui->listView->SetData(i, userLocalPath, SceneCollectionCustomRole::UserLocalPathRole);
			break;
		}
	}
	WriteSceneCollectionConfig();
}

void PLSSceneCollectionView::ClearCollectionUserLocalPath(const QString &name) const
{
	QVector<PLSSceneCollectionData> datas = GetDatas();
	for (int i = 0; i < datas.count(); i++) {
		PLSSceneCollectionData data = datas.at(i);
		if (0 == data.fileName.compare(name)) {
			ui->listView->SetData(i, "", SceneCollectionCustomRole::UserLocalPathRole);
			break;
		}
	}
	WriteSceneCollectionConfig();
}

void PLSSceneCollectionView::showEvent(QShowEvent *event)
{
	ui->stackedWidget->setCurrentWidget(ui->itemPage);
	ui->searchLineEdit->clear();
	PLSDialogView::showEvent(event);
}

void PLSSceneCollectionView::closeEvent(QCloseEvent *event)
{
	hide();
	event->ignore();
}

bool PLSSceneCollectionView::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::Enter) {
		HandleEnterEvent(obj, event);
	}
	if (event->type() == QEvent::Leave) {
		HandleLeaveEvent(obj, event);
	} else if (event->type() == QEvent::MouseButtonPress) {
		HandlePressEvent(obj, event);
	} else if (event->type() == QEvent::MouseButtonRelease) {
		HandleReleaseEvent(obj, event);
	}

	return PLSDialogView::eventFilter(obj, event);
}

void PLSSceneCollectionView::OnSceneCollectionItemRowChanged(int srcIndex, int destIndex) const
{
	pls_unused(srcIndex);
	pls_unused(destIndex);
	if (auto basic = OBSBasic::Get(); basic) {
		basic->ReorderSceneCollectionManageView();
	}
	WriteSceneCollectionConfig();
}

void PLSSceneCollectionView::OnImportButtonClicked()
{
	QMenu popup(this);
	popup.setWindowFlags(popup.windowFlags() | Qt::NoDropShadowWindowHint);
	popup.setObjectName("importerButton");

	auto importAction = pls_new<QAction>(QTStr("Scene.Collection.Import.From.Local"), &popup);
	auto importFromOtherAction = pls_new<QAction>(QTStr("Scene.Collection.Import.From.Other"), &popup);
	connect(importAction, &QAction::triggered, this, &PLSSceneCollectionView::OnImportFromLocalButtonClicked);
	connect(importFromOtherAction, &QAction::triggered, this, &PLSSceneCollectionView::OnImportFromOtherButtonClicked);

	popup.addAction(importAction);
	popup.addAction(importFromOtherAction);

	QPoint pos = mapToGlobal(this->rect().bottomLeft());
	popup.exec(QPoint(pos.x() + 148, pos.y() - 57 - 96));
}

void PLSSceneCollectionView::OnImportFromLocalButtonClicked()
{
	if (auto basic = OBSBasic::Get(); basic)
		basic->on_actionImportSceneCollection_triggered_with_parent(this);
}

void PLSSceneCollectionView::OnImportFromOtherButtonClicked() const
{
	if (auto basic = OBSBasic::Get(); basic)
		basic->on_actionImportFromOtherSceneCollection_triggered();
}

void PLSSceneCollectionView::OnCloseButtonClicked()
{
	close();
}

void PLSSceneCollectionView::OnScrollBarShow(bool show)
{
	if (show) {
		ui->horizontalLayout_3->setContentsMargins(12, 0, 2, 0);
		ui->horizontalLayout_4->setContentsMargins(12, 0, 2, 0);
	} else {
		ui->horizontalLayout_3->setContentsMargins(12, 0, 12, 0);
		ui->horizontalLayout_4->setContentsMargins(12, 0, 12, 0);
	}
}

void PLSSceneCollectionView::HandleEnterEvent(const QObject *obj, const QEvent *) const
{
	if (obj == ui->newLabel || obj == ui->newBtn) {
		SetMouseStatus(ui->newLabel, STATUS_ENTER);
		SetMouseStatus(ui->newBtn, STATUS_ENTER);
	} else if (obj == ui->importLabel || obj == ui->importBtn) {
		SetMouseStatus(ui->importLabel, STATUS_ENTER);
		SetMouseStatus(ui->importBtn, STATUS_ENTER);
	} else if (obj == ui->macHelpBtn) {
		QPoint pos = this->rect().bottomLeft();
		QPoint global = mapToGlobal(pos);
		QPoint x = mapToParent(ui->macHelpBtn->pos());
		QToolTip::showText(QPoint(x.x() - 1, global.y() - 46), QTStr("Scene.Collection.View.Help.Tooltip"), this->widget());
	}
}

void PLSSceneCollectionView::HandleLeaveEvent(const QObject *obj, const QEvent *) const
{
	if (obj == ui->newLabel || obj == ui->newBtn) {
		SetMouseStatus(ui->newLabel, STATUS_NORMAL);
		SetMouseStatus(ui->newBtn, STATUS_NORMAL);
	} else if (obj == ui->importLabel || obj == ui->importBtn) {
		SetMouseStatus(ui->importLabel, STATUS_NORMAL);
		SetMouseStatus(ui->importBtn, STATUS_NORMAL);
	}
}

void PLSSceneCollectionView::HandlePressEvent(const QObject *obj, QEvent *event) const
{
	auto mouseEvent = static_cast<QMouseEvent *>(event);
	if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {
		if (obj == ui->newLabel || obj == ui->newBtn) {
			SetMouseStatus(ui->newLabel, STATUS_CLICKED);
			SetMouseStatus(ui->newBtn, STATUS_CLICKED);
		} else if (obj == ui->importLabel || obj == ui->importBtn) {
			SetMouseStatus(ui->importLabel, STATUS_CLICKED);
			SetMouseStatus(ui->importBtn, STATUS_CLICKED);
		}
	}
}

void PLSSceneCollectionView::HandleReleaseEvent(const QObject *obj, QEvent *event)
{
	auto mouseEvent = static_cast<QMouseEvent *>(event);
	if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {
		if (obj == ui->newLabel || obj == ui->newBtn) {
			emit newButtonClicked();
		} else if (obj == ui->importLabel || obj == ui->importBtn) {
			emit importButtonClicked();
		}
	}
}

void PLSSceneCollectionListView::OnApplyBtnClicked(const QString &name, const QString &path, bool textMode) const
{
	if (auto basic = OBSBasic::Get(); basic) {
		basic->on_actionChangeSceneCollection_triggered(name, path, textMode);
	}

	RepaintWidgets();
}

void PLSSceneCollectionListView::OnExportBtnClicked(const QString &name, const QString &path) const
{
	if (auto basic = OBSBasic::Get(); basic) {
		basic->on_actionExportSceneCollection_triggered_with_path(name, basic->ExtractFileName(path.toStdString()).c_str(), this->parentWidget());
	}
}

void PLSSceneCollectionListView::OnRenameBtnClicked(const QString &name, const QString &path) const
{
	if (auto basic = OBSBasic::Get(); basic) {
		basic->on_actionRenameSceneCollection_triggered(name, path);
	}
}

void PLSSceneCollectionListView::OnDuplicateBtnClicked(const QString &name, const QString &path) const
{
	if (auto basic = OBSBasic::Get(); basic) {
		basic->on_actionDupSceneCollection_triggered(name, path);
	}
}

void PLSSceneCollectionListView::OnDeleteBtnClicked(const QString &name, const QString &path) const
{
	if (auto basic = OBSBasic::Get(); basic) {
		basic->on_actionRemoveSceneCollection_triggered(name, path);
	}
}

DropLine getDropLineType(const QPoint &pos, const PLSSceneCollectionItem *item)
{
	if (!item)
		return DropLine::DropLineNone;

	auto middleY = item->mapToParent(QPoint(0, item->height() / 2)).y();
	if (pos.y() > middleY) {
		return DropLine::DropLineBottom;
	} else {
		return DropLine::DropLineTop;
	}
}

void PLSSceneCollectionListView::SetPaintLinePos(int startPosX, int startPosY, int, int)
{
	auto pos = QPoint(startPosX, startPosY);
	auto index = indexAt(pos);
	auto item = GetItemWidget(index.row());
	if (LocalGlobal::dropLineItem) {
		LocalGlobal::dropLineItem->ClearDropLine();
	}
	if (item) {
		item->DrawDropLine(getDropLineType(pos, item));
		LocalGlobal::dropLineItem = item;
	}
}

PLSSceneCollectionItem *PLSSceneCollectionListView::CreateItem(const PLSSceneCollectionData &data)
{
	auto itemView = pls_new<PLSSceneCollectionItem>(data.fileName, data.filePath, data.current, data.textMode, this);
	connect(itemView, &PLSSceneCollectionItem::applyClicked, this, &PLSSceneCollectionListView::OnApplyBtnClicked);
	connect(itemView, &PLSSceneCollectionItem::exportClicked, this, &PLSSceneCollectionListView::OnExportBtnClicked);
	connect(itemView, &PLSSceneCollectionItem::renameClicked, this, &PLSSceneCollectionListView::OnRenameBtnClicked);
	connect(itemView, &PLSSceneCollectionItem::duplicateClicked, this, &PLSSceneCollectionListView::OnDuplicateBtnClicked, Qt::QueuedConnection);
	connect(itemView, &PLSSceneCollectionItem::deleteClicked, this, &PLSSceneCollectionListView::OnDeleteBtnClicked);
	return itemView;
}

void PLSSceneCollectionListView::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		startDragPoint = event->pos();
		startDragModelIdx = this->indexAt(event->pos());
	}
	QListView::mousePressEvent(event);
}

void PLSSceneCollectionListView::mouseMoveEvent(QMouseEvent *event)
{
	if (event->buttons() & Qt::LeftButton) {
		QPoint distance = event->pos() - startDragPoint;
		if (distance.manhattanLength() > QApplication::startDragDistance() && enableDrops) {

			auto drag = pls_new<QDrag>(this);
			auto mimeData = pls_new<QMimeData>();
			startDragIndex = startDragModelIdx.row();
			mimeData->setData(SCENE_COLLECTION_DRAG_MIME_TYPE, QByteArray(QString::number(startDragIndex).toStdString().c_str()));

			QRect currentRect = visualRect(this->indexAt(event->pos()));
			QPixmap pixmap = viewport()->grab(currentRect);
			drag->setHotSpot(QPoint(startDragPoint.x(), pixmap.height() / 2));
			drag->setPixmap(pixmap);

			drag->setMimeData(mimeData);
			drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
		}
	}
	QListView::mouseMoveEvent(event);
}

void PLSSceneCollectionListView::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat(SCENE_COLLECTION_DRAG_MIME_TYPE) && enableDrops) {
		event->setDropAction(Qt::MoveAction);
		event->accept();
		isDraging = true;
	} else {
		event->ignore();
	}
}

void PLSSceneCollectionListView::dragMoveEvent(QDragMoveEvent *event)
{
	if (event->mimeData()->hasFormat(SCENE_COLLECTION_DRAG_MIME_TYPE) && enableDrops) {
		QRect currentRect;
		int count = this->Count();
		int rowCount = this->indexAt(event->position().toPoint()).row();
		if (-1 == rowCount || rowCount > count) {
			rowCount = count - 1;
		}
		currentRect = visualRect(GetModel()->index(rowCount));
		if (QPoint topleft = currentRect.topLeft(); event->position().toPoint().y() - topleft.y() > FIX_SCENE_COLLECTION_ITEM_HEIGHT / 2) {
			SetPaintLinePos(currentRect.bottomLeft().x(), currentRect.bottomLeft().y(), currentRect.bottomRight().x(), currentRect.bottomRight().y());
		} else {
			SetPaintLinePos(currentRect.topLeft().x(), currentRect.topLeft().y(), currentRect.topRight().x(), currentRect.topRight().y());
		}

		this->viewport()->update();
		event->setDropAction(Qt::MoveAction);
		QListView::dragMoveEvent(event);
		event->accept();
	} else {
		QListView::dragMoveEvent(event);
		event->ignore();
	}
}

void PLSSceneCollectionListView::dropEvent(QDropEvent *event)
{
	if (event->mimeData()->hasFormat(SCENE_COLLECTION_DRAG_MIME_TYPE) && enableDrops) {
		event->setDropAction(Qt::MoveAction);
		event->accept();
		isDraging = false;
		int count = this->Count();
		int currentIndex = this->indexAt(event->position().toPoint()).row();
		if (-1 == currentIndex || currentIndex > count) {
			currentIndex = count - 1;
		}
		QRect currentRect = visualRect(GetModel()->index(currentIndex));
		if (QPoint topleft = currentRect.topLeft(); event->position().toPoint().y() - topleft.y() < FIX_SCENE_COLLECTION_ITEM_HEIGHT / 2) {
			if (startDragIndex < currentIndex) {
				currentIndex <= 0 ? 0 : currentIndex -= 1;
			}
		} else {
			if (startDragIndex > currentIndex) {
				currentIndex >= count - 1 ? count - 1 : currentIndex += 1;
			}
		}

		QListView::dropEvent(event);
		GetModel()->RowChanged(startDragIndex, currentIndex);
		if (LocalGlobal::dropLineItem) {
			LocalGlobal::dropLineItem->ClearDropLine();
		}
		emit RowChanged(startDragIndex, currentIndex);
		return;
	}
	QListView::dropEvent(event);
	event->ignore();
}

void PLSSceneCollectionListView::dragLeaveEvent(QDragLeaveEvent *event)
{
	isDraging = false;
	QListView::dragLeaveEvent(event);
}

void PLSSceneCollectionListView::showEvent(QShowEvent *event)
{
	int row = GetModel()->GetCurrentRow();
	QModelIndex index = GetModel()->createIndex(row, 0, nullptr);
	scrollTo(index);

	QListView::showEvent(event);
}

void PLSSceneCollectionView::OnSearchTriggerd(const QString &text) const
{
	QSignalBlocker blocker(ui->searchLineEdit);
	if (text.isEmpty()) {
		ui->searchLineEdit->SetDeleteBtnVisible(false);
		ui->stackedWidget->setCurrentWidget(ui->itemPage);
		return;
	}

	ui->searchLineEdit->SetDeleteBtnVisible(true);

	QVector<PLSSceneCollectionData> datas = ui->listView->GetDatas();
	QVector<PLSSceneCollectionData> searchItems;
	for (PLSSceneCollectionData data : datas) {
		QString name = data.fileName;
		if (name.contains(text, Qt::CaseInsensitive)) {
			searchItems.push_back(data);
		}
	}

	if (searchItems.isEmpty()) {
		ui->stackedWidget->setCurrentWidget(ui->noDataPage);
	} else {
		ui->searchListView->InitWidgets(searchItems);
		ui->stackedWidget->setCurrentWidget(ui->searchPage);
		UpdateDeleteButtonState();
	}
}

int PLSSceneCollectionView::FindExistedCollectionData(const QVector<PLSSceneCollectionData> &datas, const QString &name, const QString &path) const
{
	if (datas.isEmpty()) {
		return -1;
	}

	int index = 0;
	for (PLSSceneCollectionData data : datas) {
		if (0 == name.compare(data.fileName) && 0 == path.compare(data.filePath)) {
			return index;
		}
		index = index + 1;
	}

	return false;
}

int PLSSceneCollectionView::GetCollectionItemRow(const QString &name, const QString &path) const
{
	QVector<PLSSceneCollectionData> datas = ui->listView->GetDatas();

	return FindExistedCollectionData(datas, name, path);
}

void PLSSceneCollectionView::InitSceneCollectionConfig(QVector<PLSSceneCollectionData> &datas) const
{
	auto collectionConfigFile = pls_get_user_path("PRISMLiveStudio/basic/scenes/") + SCENE_COLLECTION_CONFIG;
	if (!QFile::exists(collectionConfigFile))
		return;

	QByteArray array;
	if (!PLSJsonDataHandler::getJsonArrayFromFile(array, collectionConfigFile)) {
		return;
	}

	QVector<PLSSceneCollectionData> cacheDatas;
	QJsonObject objectJson = QJsonDocument::fromJson(array).object();
	const QJsonArray &jsonArray = objectJson.value("order").toArray();
	for (auto json : jsonArray) {
		QJsonObject object = json.toObject();
		PLSSceneCollectionData data;
		data.fileName = object["name"].toString();
		data.filePath = object["path"].toString();
		data.userLocalPath = object["userLocalPath"].toString();
		auto findIndex = FindExistedCollectionData(datas, data.fileName, data.filePath);
		if (-1 != findIndex) {
			PLSSceneCollectionData findData = datas[findIndex];
			findData.userLocalPath = data.userLocalPath;
			cacheDatas.push_back(findData);
			datas.erase(datas.begin() + findIndex);
		}
	}

	for (PLSSceneCollectionData data : datas) {
		QFileInfo info(data.filePath);
		qint64 timestamp = info.birthTime().toMSecsSinceEpoch();
		for (int index = 0; index < cacheDatas.count();) {
			QFileInfo cacheInfo(cacheDatas[index].filePath);
			qint64 cacheTimestamp = cacheInfo.birthTime().toMSecsSinceEpoch();
			bool lastIndex = index == cacheDatas.count() - 1;
			if (timestamp > cacheTimestamp || lastIndex) {
				if (lastIndex) {
					cacheDatas.push_back(data);
				} else {
					cacheDatas.insert(cacheDatas.begin() + index, data);
				}
				break;
			} else {
				index++;
			}
		}
	}

	if (!cacheDatas.isEmpty())
		datas.swap(cacheDatas);
}

void PLSSceneCollectionView::WriteSceneCollectionConfig() const
{
	QVector<PLSSceneCollectionData> datas = ui->listView->GetDatas();
	if (datas.isEmpty()) {
		return;
	}

	QJsonArray array;
	for (PLSSceneCollectionData data : datas) {
		QJsonObject object;
		object["name"] = data.fileName;
		object["path"] = data.filePath;
		object["userLocalPath"] = data.userLocalPath;
		array.push_back(object);
	}
	QJsonObject rootObject;
	rootObject["order"] = array;
	PLSJsonDataHandler::saveJsonFile(QJsonDocument(rootObject).toJson(), pls_get_user_path("PRISMLiveStudio/basic/scenes/").append(SCENE_COLLECTION_CONFIG));
}

void PLSSceneCollectionView::UpdateListViewDelBtnStatus(PLSSceneCollectionListView *view, const QVector<PLSSceneCollectionData> &datas) const
{
	if (!view || datas.empty()) {
		return;
	}

	if (1 == datas.count()) {
		view->SetData(0, QVariant::fromValue(true), SceneCollectionCustomRole::DelButtonDisableRole);
	} else {
		view->SetDatas(QVariant::fromValue(false), SceneCollectionCustomRole::DelButtonDisableRole);
	}
	QTimer::singleShot(0, this, [view]() { view->UpdateWidgets(); });
}

void PLSSceneCollectionView::SetMouseStatus(QWidget *widget, QString status) const
{
	pls_flush_style(widget, STATUS, status);
}

void PLSSceneCollectionView::RemoveCollectionItem(const QString &name, const QString &path) const
{
	PLSSceneCollectionData data;
	data.fileName = name;
	data.filePath = path;
	ui->listView->Remove(data);

	if (ui->stackedWidget->currentWidget() == ui->searchPage) {
		ui->searchListView->Remove(data);
		if (ui->searchListView->GetDatas().empty()) {
			ui->stackedWidget->setCurrentWidget(ui->noDataPage);
		}
	}

	UpdateDeleteButtonState();
	WriteSceneCollectionConfig();
}

void PLSSceneCollectionView::RenameCollectionItem(const QString &srcName, const QString &srcPath, const QString &destName, const QString &destPath) const
{
	if (destName.isEmpty() || destPath.isEmpty()) {
		return;
	}

	PLSSceneCollectionData srcData;
	srcData.fileName = srcName;
	srcData.filePath = srcPath;
	PLSSceneCollectionData destData;
	destData.fileName = destName;
	destData.filePath = destPath;

	ui->listView->Rename(srcData, destData);
	if (ui->stackedWidget->currentWidget() == ui->searchPage) {
		ui->searchListView->Rename(srcData, destData);
		OnSearchTriggerd(ui->searchLineEdit->text());
	}
	WriteSceneCollectionConfig();
}

void PLSSceneCollectionModel::InitDatas(const QVector<PLSSceneCollectionData> &datas)
{
	beginResetModel();
	itemDatas.clear();
	itemDatas = datas;
	endResetModel();

	listView->ResetWidgets();
}

void PLSSceneCollectionModel::Add(const PLSSceneCollectionData &data)
{
	beginInsertRows(QModelIndex(), 0, 0);
	itemDatas.insert(0, data);
	endInsertRows();

	listView->UpdateWidget(0, data);
}

void PLSSceneCollectionModel::Remove(const PLSSceneCollectionData &data)
{
	int index = -1;
	for (int i = 0; i < itemDatas.count(); i++) {
		if (0 == data.fileName.compare(itemDatas[i].fileName) && 0 == data.filePath.compare(itemDatas[i].filePath)) {
			index = i;
			break;
		}
	}
	if (index == -1) {
		return;
	}

	int startIdx = index;
	int endIdx = index;
	beginRemoveRows(QModelIndex(), startIdx, endIdx);
	itemDatas.remove(index, endIdx - startIdx + 1);
	endRemoveRows();
}

void PLSSceneCollectionModel::Rename(const PLSSceneCollectionData &srcData, const PLSSceneCollectionData &destData)
{
	for (int i = 0; i < itemDatas.count(); i++) {
		auto &data = itemDatas[i];
		if (0 == srcData.fileName.compare(data.fileName) && 0 == srcData.filePath.compare(data.filePath)) {
			data.fileName = destData.fileName;
			data.filePath = destData.filePath;
			listView->UpdateWidget(i, destData, true);
			break;
		}
	}
}

void PLSSceneCollectionModel::Clear()
{
	beginResetModel();
	itemDatas.clear();
	endResetModel();
}

void PLSSceneCollectionModel::SetCurrentData(const QString &name, const QString &path)
{
	Q_UNUSED(path)
	for (int i = 0; i < itemDatas.count(); i++) {
		if (0 == name.compare(itemDatas[i].fileName)) {
			setData(createIndex(i, 0, nullptr), true, (int)SceneCollectionCustomRole::CurrentRole);
		} else {
			setData(createIndex(i, 0, nullptr), false, (int)SceneCollectionCustomRole::CurrentRole);
		}
	}
	listView->UpdateWidgets();
}

int PLSSceneCollectionModel::GetCurrentRow()
{
	for (int i = 0; i < itemDatas.count(); i++) {
		auto data = itemDatas[i];
		if (data.current) {
			return i;
		}
	}
	return -1;
}

void PLSSceneCollectionModel::RowChanged(int srcIndex, int destIndex)
{
	if (srcIndex == destIndex) {
		return;
	}

	if (auto count = itemDatas.count(); srcIndex < 0 || destIndex < 0 || srcIndex >= count || destIndex >= count) {
		return;
	}

	beginResetModel();
	auto iter = itemDatas.begin();
	iter += srcIndex;
	PLSSceneCollectionData data = *iter;
	itemDatas.erase(iter);

	auto iter_new = itemDatas.begin();
	itemDatas.insert(iter_new + destIndex, data);
	endResetModel();
	listView->ResetWidgets();
}

QVector<PLSSceneCollectionData> PLSSceneCollectionModel::GetDatas() const
{
	return itemDatas;
}

PLSSceneCollectionData PLSSceneCollectionModel::GetData(int row) const
{
	if (row < 0 || row >= itemDatas.count()) {
		return PLSSceneCollectionData();
	}
	return itemDatas[row];
}

PLSSceneCollectionModel::PLSSceneCollectionModel(PLSSceneCollectionListView *view) : QAbstractListModel(view), listView(view) {}

int PLSSceneCollectionModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : (int)itemDatas.count();
}

QVariant PLSSceneCollectionModel::data(const QModelIndex &index, int role) const
{
	int row = index.row();
	if (row < 0 || row >= itemDatas.count()) {
		return QVariant();
	}

	if (SceneCollectionCustomRole::DelButtonDisableRole == (SceneCollectionCustomRole)role) {
		return QVariant::fromValue(itemDatas[row].delButtonDisable);
	} else if (SceneCollectionCustomRole::DataRole == (SceneCollectionCustomRole)role) {
		return QVariant::fromValue(itemDatas[row]);
	} else if (SceneCollectionCustomRole::CurrentRole == (SceneCollectionCustomRole)role) {
		return QVariant::fromValue(itemDatas[row].current);
	} else if (SceneCollectionCustomRole::UserLocalPathRole == (SceneCollectionCustomRole)role) {
		return QVariant::fromValue(itemDatas[row].userLocalPath);
	}

	return QVariant();
}

bool PLSSceneCollectionModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	int row = index.row();
	if (row < 0 || row >= itemDatas.count()) {
		return false;
	}

	if (SceneCollectionCustomRole::DelButtonDisableRole == (SceneCollectionCustomRole)role) {
		itemDatas[row].delButtonDisable = value.value<bool>();
	} else if (SceneCollectionCustomRole::DataRole == (SceneCollectionCustomRole)role) {
		itemDatas[row] = value.value<PLSSceneCollectionData>();
	} else if (SceneCollectionCustomRole::CurrentRole == (SceneCollectionCustomRole)role) {
		itemDatas[row].current = value.value<bool>();
	} else if (SceneCollectionCustomRole::UserLocalPathRole == (SceneCollectionCustomRole)role) {
		itemDatas[row].userLocalPath = value.value<QString>();
	}
	return true;
}

Qt::ItemFlags PLSSceneCollectionModel::flags(const QModelIndex &index) const
{
	Q_UNUSED(index)
	return Qt::ItemFlags();
}

Qt::DropActions PLSSceneCollectionModel::supportedDropActions() const
{
	return QAbstractItemModel::supportedDropActions() | Qt::MoveAction;
}

PLSSceneCollectionItem *PLSSceneCollectionListView::GetItemWidget(int index)
{
	QWidget *widget = indexWidget(GetModel()->createIndex(index, 0));
	return static_cast<PLSSceneCollectionItem *>(widget);
}

void PLSSceneCollectionListView::InitWidgets(const QVector<PLSSceneCollectionData> &datas) const
{
	GetModel()->InitDatas(datas);
}

void PLSSceneCollectionListView::Rename(const PLSSceneCollectionData &srcData, const PLSSceneCollectionData &destData) const
{
	auto model = GetModel();
	model->Rename(srcData, destData);
}

void PLSSceneCollectionListView::ResetWidgets()
{
	auto model = GetModel();
	auto count = model->itemDatas.count();
	bool delButtonDisable = (1 == count);
	for (int i = 0; i < count; i++) {
		auto data = model->itemDatas[i];
		QModelIndex index = model->createIndex(i, 0, nullptr);
		model->setData(index, QVariant::fromValue(delButtonDisable), (int)SceneCollectionCustomRole::DelButtonDisableRole);
		auto item = CreateItem(data);
		setIndexWidget(index, item);
	}
}

void PLSSceneCollectionListView::RepaintWidgets() const
{
	auto model = GetModel();
	auto count = model->itemDatas.count();
	for (int i = 0; i < count; i++) {
		QModelIndex index = model->createIndex(i, 0, nullptr);
		auto existedItem = static_cast<PLSSceneCollectionItem *>(indexWidget(index));
		if (!existedItem) {
			continue;
		}
		existedItem->repaint();
	}
}

void PLSSceneCollectionListView::UpdateWidgets()
{
	auto model = GetModel();
	auto count = model->itemDatas.count();
	for (int i = 0; i < count; i++) {
		UpdateWidget(i, model->GetData(i), true);
	}
}

void PLSSceneCollectionListView::UpdateWidget(int row, const PLSSceneCollectionData &data, bool update)
{
	auto model = GetModel();
	QModelIndex index = model->createIndex(row, 0, nullptr);

	if (!update) {
		auto item = CreateItem(data);
		setIndexWidget(index, item);
	} else {
		auto existedItem = static_cast<PLSSceneCollectionItem *>(indexWidget(index));
		existedItem->Update(model->GetData(row));
	}
}

void PLSSceneCollectionListView::SetData(int row, QVariant variant, SceneCollectionCustomRole role) const
{
	auto model = GetModel();
	QModelIndex modelIndex = model->createIndex(row, 0, nullptr);
	model->setData(modelIndex, variant, (int)role);
}

void PLSSceneCollectionListView::SetDatas(QVariant variant, SceneCollectionCustomRole role) const
{
	auto model = GetModel();
	for (int i = 0; i < model->itemDatas.count(); i++) {
		SetData(i, variant, role);
	}
}

void PLSSceneCollectionListView::UpdateCurrentTimeStampLabel() const
{
	auto model = GetModel();
	int count = model->Count();
	for (int i = 0; i < count; i++) {
		QModelIndex index = model->createIndex(i, 0, nullptr);
		auto existedItem = static_cast<PLSSceneCollectionItem *>(indexWidget(index));
		if (existedItem) {
			existedItem->UpdateModifiedTimeStamp();
		}
	}
}

void PLSSceneCollectionListView::SetEnableDrops(bool enable)
{
	enableDrops = enable;
	setAcceptDrops(enable);
}

PLSSceneCollectionModel *PLSSceneCollectionListView::GetModel() const
{
	return (PLSSceneCollectionModel *)(model());
}

QVector<PLSSceneCollectionData> PLSSceneCollectionListView::GetDatas() const
{
	return GetModel()->GetDatas();
}
