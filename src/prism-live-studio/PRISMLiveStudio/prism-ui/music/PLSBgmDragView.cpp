#include "PLSBgmDragView.h"
#include "PLSBgmItemView.h"
#include "liblog.h"
#include "log/module_names.h"
#include "action.h"
#include "PLSBgmDataManager.h"
#include "PLSBgmItemDelegate.h"
#include "utils-api.h"

#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QPainter>
#include <QPen>
#include <QListWidget>
#include <QFileInfo>

static const QString BGM_DRAG_MIME_TYPE = "bgmItem";
static const int FIX_BGM_ITEM_HEIGHT = 70;
PLSBgmDragView::PLSBgmDragView(QWidget *parent) : QListView(parent)
{
	auto stm = pls_new<PLSBgmItemViewModel>(this);
	PLSBgmDragView::setModel(stm);
	setMouseTracking(true);
	this->setAcceptDrops(true);
	setProperty("showHandCursor", true);
	CreateItemDelegate();
}

PLSBgmDragView::~PLSBgmDragView() = default;

void PLSBgmDragView::SetDpi(double dpi) const
{
	PLSBgmItemDelegate::setDpi((float)dpi);
}

void PLSBgmDragView::SetMediaStatus(const int &index, const MediaStatus &status)
{
	PLSBgmItemViewModel *stm = GetStm();
	QModelIndex modelIndex = stm->createIndex(index, 0, nullptr);
	stm->setData(modelIndex, QVariant::fromValue(status), (int)CustomDataRole::MediaStatusRole);
	update(modelIndex);
}

int PLSBgmDragView::GetCurrentRow() const
{
	QVector<PLSBgmItemData> datas = GetStm()->GetData();

	auto it = std::find_if(datas.begin(), datas.end(), [](const PLSBgmItemData &data_) { return data_.isCurrent == true; });
	if (it != datas.end()) {
		return (int)(it - datas.begin());
	}
	return -1;
}

int PLSBgmDragView::GetRow(const PLSBgmItemData &data) const
{
	QVector<PLSBgmItemData> datas = GetStm()->GetData();
	auto it = std::find_if(datas.begin(), datas.end(), [data](const PLSBgmItemData &data_) { return (data_.GetUrl(data_.id) == data.GetUrl(data.id) && data_.id == data.id); });
	if (it != datas.end()) {
		return (int)(it - datas.begin());
	}

	return -1;
}

PLSBgmItemData PLSBgmDragView::GetData(const QModelIndex &idx) const
{
	PLSBgmItemViewModel *stm = GetStm();
	return stm->Get(idx);
}

void PLSBgmDragView::UpdateWidget()
{
	CreateItemDelegate();
	PLSBgmItemViewModel *stm = GetStm();
	for (int i = 0; i < stm->datas.count(); i++) {
		QModelIndex index = stm->createIndex(i, 0, nullptr);
		UpdateWidget(index, stm->datas[i]);
	}
}

void PLSBgmDragView::UpdateWidget(const QVector<PLSBgmItemData> &datas)
{
	PLSBgmItemViewModel *stm = GetStm();
	stm->SetData(datas);

	UpdateWidget();
}

void PLSBgmDragView::InsertWidget(const BgmItemCacheType &datas) const
{
	PLSBgmItemViewModel *stm = GetStm();
	stm->InsertData(datas);
}

void PLSBgmDragView::UpdateWidget(const QModelIndex &idx, const PLSBgmItemData &data) const
{
	GetStm()->setData(idx, QVariant::fromValue(data), (int)CustomDataRole::DataRole);
}

void PLSBgmDragView::SetCurrentRow(const PLSBgmItemData &data)
{
	QVector<PLSBgmItemData> datas = GetStm()->GetData();
	auto count = datas.size();
	for (auto i = 0; i < count; ++i) {
		PLSBgmItemData &data_ = datas[i];
		if (data_.GetUrl(data_.id) == data.GetUrl(data.id) && data_.id == data.id) {
			data_ = data;
			data_.isCurrent = true;
			if (this->isVisible()) {
				this->scrollTo(GetModelIndex(i));
			}
		} else {
			data_.isCurrent = false;
		}
		UpdataData(i, data_);
	}
}

void PLSBgmDragView::UpdataData(const int &index, const PLSBgmItemData &data) const
{
	QModelIndex modelIndex = GetStm()->createIndex(index, 0, nullptr);
	GetStm()->setData(modelIndex, QVariant::fromValue(data), (int)CustomDataRole::DataRole);
}

void PLSBgmDragView::UpdataData(const int &index, QVariant value, CustomDataRole role) const
{
	QModelIndex modelIndex = GetStm()->createIndex(index, 0, nullptr);
	GetStm()->setData(modelIndex, value, (int)role);
}

QVector<PLSBgmItemData> PLSBgmDragView::GetData() const
{
	return GetStm()->GetData();
}

QModelIndex PLSBgmDragView::GetModelIndex(const int &row) const
{
	return GetStm()->index(row);
}

void PLSBgmDragView::Remove(const PLSBgmItemData &data) const
{
	GetStm()->Remove(data.GetUrl(data.id), data.id);
	RemoveFile(data);
}

bool PLSBgmDragView::Existed(const QString &url) const
{
	QVector<PLSBgmItemData> datas = GetStm()->GetData();
	return std::any_of(datas.cbegin(), datas.cend(), [url](const PLSBgmItemData &data_) { return data_.GetUrl(data_.id) == url; });
}

void PLSBgmDragView::Clear() const
{
	QVector<PLSBgmItemData> datas = GetStm()->GetData();
	std::for_each(datas.begin(), datas.end(), [this](const PLSBgmItemData &data_) { RemoveFile(data_); });
	GetStm()->Clear();
}

int PLSBgmDragView::Count() const
{
	return GetStm()->Count();
}

int PLSBgmDragView::GetId(const QString url) const
{
	int id = 0;
	while (true) {
		if (ExistedId(url, id)) {
			id++;
			continue;
		}
		break;
	}
	return id;
}

PLSBgmItemData PLSBgmDragView::Get(int idx) const
{
	return GetStm()->Get(idx);
}

PLSBgmItemData PLSBgmDragView::Get(const QString &url, const int &id) const
{
	QVector<PLSBgmItemData> datas = GetStm()->GetData();
	for (const auto &data_ : datas) {
		if (data_.GetUrl(data_.id) == url && data_.id == id) {
			return data_;
		}
	}
	return PLSBgmItemData();
}

PLSBgmItemData PLSBgmDragView::GetCurrent() const
{
	QVector<PLSBgmItemData> datas = GetStm()->GetData();
	for (const auto &data_ : datas) {
		if (data_.isCurrent) {
			return data_;
		}
	}
	return PLSBgmItemData();
}

void PLSBgmDragView::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		startDragPoint = event->pos();
		startDragModelIdx = this->indexAt(event->pos());
	}

	QListView::mousePressEvent(event);
}

void PLSBgmDragView::mouseMoveEvent(QMouseEvent *event)
{
	if (event->buttons() & Qt::LeftButton) {
		QPoint distance = event->pos() - startDragPoint;
		if (distance.manhattanLength() > QApplication::startDragDistance()) {

			auto drag = pls_new<QDrag>(this);
			auto mimeData = pls_new<QMimeData>();
			startDragIndex = startDragModelIdx.row();
			mimeData->setData(BGM_DRAG_MIME_TYPE, QByteArray(QString::number(startDragIndex).toStdString().c_str()));

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

void PLSBgmDragView::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls()) {
		if (PLSBgmDataViewManager::Instance()->IsSupportFormat(event->mimeData()->urls())) {
			event->acceptProposedAction();
			return;
		}
		event->ignore();
		return;
	}

	if (event->mimeData()->hasFormat(BGM_DRAG_MIME_TYPE)) {
		event->setDropAction(Qt::MoveAction);
		event->accept();
		isDraging = true;
	} else {
		event->ignore();
	}
}

void PLSBgmDragView::dragMoveEvent(QDragMoveEvent *event)
{
	if (event->mimeData()->hasFormat(BGM_DRAG_MIME_TYPE)) {
		isDraging = true;

		GetStm()->setData(dragOverModelIndex, QVariant::fromValue(DropIndicator::None), (int)CustomDataRole::DropIndicatorRole);
		update(dragOverModelIndex);

		QModelIndex dragModelIdx = this->indexAt(event->position().toPoint());
		int rowCount = dragModelIdx.row();
		if (-1 == rowCount || rowCount > this->Count()) {
			rowCount = this->Count() - 1;
			dragModelIdx = model()->index(rowCount, 0);
		}
		dragOverModelIndex = dragModelIdx;
		QRect currentRect = visualRect(GetModelIndex(rowCount));
		QPoint topleft = currentRect.topLeft();
		if (event->position().toPoint().y() - topleft.y() > FIX_BGM_ITEM_HEIGHT / 2) {
			GetStm()->setData(dragOverModelIndex, QVariant::fromValue(DropIndicator::Bottom), (int)CustomDataRole::DropIndicatorRole);
			update(dragOverModelIndex);
		} else {
			GetStm()->setData(dragOverModelIndex, QVariant::fromValue(DropIndicator::Top), (int)CustomDataRole::DropIndicatorRole);
			update(dragOverModelIndex);
		}

		this->viewport()->update();
		event->setDropAction(Qt::MoveAction);
		QListView::dragMoveEvent(event);
		event->accept();
	} else if (event->mimeData()->hasUrls()) {
		event->setDropAction(Qt::MoveAction);
		QListView::dragMoveEvent(event);
		event->accept();
	} else {
		QListView::dragMoveEvent(event);
		event->ignore();
	}
}

static QStringList getDropFilePaths(const QDropEvent *event)
{
	QStringList paths;
	QList<QUrl> urls = event->mimeData()->urls();
	for (auto item : urls) {
		QString file = item.toLocalFile();
		QFileInfo fileInfo(file);
		if (!fileInfo.exists())
			continue;
		if (!PLSBgmDataViewManager::Instance()->IsSupportFormat(file)) {
			continue;
		}

		paths << file;
	}
	return paths;
}

void PLSBgmDragView::dropEvent(QDropEvent *event)
{
	QListView::dropEvent(event);

	if (event->mimeData()->hasUrls()) {
		QStringList paths = getDropFilePaths(event);
		emit AudioFileDraggedIn(paths);
		return;
	}

	if (event->mimeData()->hasFormat(BGM_DRAG_MIME_TYPE)) {
		event->setDropAction(Qt::MoveAction);
		event->accept();
		isDraging = false;
		QModelIndex dropDragModelIdx = this->indexAt(event->position().toPoint());
		int currentIndex = dropDragModelIdx.row();
		if (-1 == currentIndex || currentIndex > this->Count()) {
			currentIndex = this->Count() - 1;
		}
		QRect currentRect = visualRect(GetModelIndex(currentIndex));
		QPoint topleft = currentRect.topLeft();
		if (event->position().toPoint().y() - topleft.y() < FIX_BGM_ITEM_HEIGHT / 2) {
			if (startDragIndex < currentIndex) {
				currentIndex <= 0 ? 0 : currentIndex -= 1;
			}
		} else {
			if (startDragIndex > currentIndex) {
				currentIndex >= Count() - 1 ? Count() - 1 : currentIndex += 1;
			}
		}

		QListView::dropEvent(event);
		emit RowChanged(startDragIndex, currentIndex);
		GetStm()->setData(dragOverModelIndex, QVariant::fromValue(DropIndicator::None), (int)CustomDataRole::DropIndicatorRole);
		update(dragOverModelIndex);
		return;
	}
	QListView::dropEvent(event);
	event->ignore();
}

void PLSBgmDragView::dragLeaveEvent(QDragLeaveEvent *event)
{
	isDraging = false;
	update();
	GetStm()->setData(dragOverModelIndex, QVariant::fromValue(DropIndicator::None), (int)CustomDataRole::DropIndicatorRole);
	update(dragOverModelIndex);
	QListView::dragLeaveEvent(event);
}

void PLSBgmDragView::paintEvent(QPaintEvent *event)
{
	QPainter painter(this->viewport());
	if (isDraging) {
		painter.setPen(QPen(QColor("#effc35"), 1));
	} else {
		painter.setPen(QPen(QColor("#1e1e1f"), 1));
	}

	QLine l;
	l.setP1(lineStart);
	l.setP2(lineEnd);
	painter.drawLine(l);

	QListView::paintEvent(event);
}

void PLSBgmDragView::showEvent(QShowEvent *event)
{
	QVector<PLSBgmItemData> datas = GetStm()->GetData();
	auto count = datas.size();
	for (auto i = 0; i < count; i++) {
		PLSBgmItemData data_ = datas[i];
		if (data_.isCurrent) {
			pls_async_call(this, [this, i]() { scrollTo(GetModelIndex(i)); });
			break;
		}
	}
	QListView::showEvent(event);
}

void PLSBgmDragView::SetPaintLinePos(const int &startPosX, const int &startPosY, const int &endPosX, const int &endPosY)
{
	lineStart.setX(startPosX);
	lineStart.setY(startPosY);
	lineEnd.setX(endPosX);
	lineEnd.setY(endPosY);
}

PLSBgmItemViewModel *PLSBgmDragView::GetStm() const
{
	return static_cast<PLSBgmItemViewModel *>(model());
}

bool PLSBgmDragView::ExistedId(const QString url, const int &id) const
{
	QVector<PLSBgmItemData> datas = GetStm()->GetData();
	return std::any_of(datas.cbegin(), datas.cend(), [url, id](const PLSBgmItemData &data_) { return (data_.GetUrl(data_.id) == url && id == data_.id); });
}

void PLSBgmDragView::RemoveFile(const PLSBgmItemData &data) const
{
	if (!data.coverPath.isEmpty() && data.isLocalFile) {
		QFile::remove(data.coverPath);
	}
}

void PLSBgmDragView::CreateItemDelegate()
{
	auto delegate = pls_new<PLSBgmItemDelegate>(this, font(), this);
	connect(delegate, &PLSBgmItemDelegate::delBtnClicked, this, [this](const QModelIndex &index) { emit DelButtonClickedSignal(GetData(index), false); });
	connect(delegate, &PLSBgmItemDelegate::doubleClicked, this, [this](const QModelIndex &index) { emit MouseDoublePressedSignal(index); });

	setItemDelegate(delegate);
}
