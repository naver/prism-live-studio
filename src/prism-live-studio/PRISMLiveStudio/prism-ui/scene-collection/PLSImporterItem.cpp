#include "PLSImporterItem.h"
#include "ui_PLSImporterItem.h"
#include "libutils-api.h"
#include "importers/importers.hpp"
#include "obs-app.hpp"
#include "pls-common-define.hpp"
using namespace common;

PLSImporterItem::PLSImporterItem(QString name, QString filepath, QString program, bool selected, QWidget *parent) : QFrame(parent), path(filepath)
{
	ui = pls_new<Ui::PLSImporterItem>();
	ui->setupUi(this);

	ui->nameLabel->SetText(name);
	ui->programLabel->SetText(program);
	ui->selectCheckBox->setChecked(selected);
	connect(ui->selectCheckBox, &QCheckBox::stateChanged, this, [this](int state) { emit CheckedState(Qt::Checked == state); });
}

PLSImporterItem::~PLSImporterItem()
{
	pls_delete(ui);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void PLSImporterItem::enterEvent(QEnterEvent *event)
#else
void PLSImporterItem::enterEvent(QEvent *event)
#endif
{
	SetMouseStatus(PROPERTY_VALUE_MOUSE_STATUS_HOVER);
	QFrame::enterEvent(event);
}

void PLSImporterItem::leaveEvent(QEvent *event)
{
	SetMouseStatus(PROPERTY_VALUE_MOUSE_STATUS_NORMAL);
	QFrame::leaveEvent(event);
}

void PLSImporterItem::SetMouseStatus(const char *status)
{
	pls_flush_style(ui->nameLabel, PROPERTY_NAME_MOUSE_STATUS, status);
	pls_flush_style(ui->programLabel, PROPERTY_NAME_MOUSE_STATUS, status);
	pls_flush_style(this, PROPERTY_NAME_MOUSE_STATUS, status);
}

/**
	Model
**/

void PLSImporterModel::InitDatas(QList<ImporterEntry> &datas)
{
	beginResetModel();
	options.clear();
	options.swap(datas);
	endResetModel();

	listView->UpdateWidgets();
}

int PLSImporterModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : (int)options.count();
}

QVariant PLSImporterModel::data(const QModelIndex &index, int role) const
{
	int row = index.row();
	if (row < 0 || row >= options.count()) {
		return QVariant();
	}

	auto result = QVariant();
	if (ImporterCustomRole::nameRole == (ImporterCustomRole)role) {
		return QVariant::fromValue(options[row].name);
	} else if (ImporterCustomRole::pathRole == (ImporterCustomRole)role) {
		return QVariant::fromValue(options[row].path);
	} else if (ImporterCustomRole::programRole == (ImporterCustomRole)role) {
		return QVariant::fromValue(options[row].program);
	} else if (ImporterCustomRole::selectedRole == (ImporterCustomRole)role) {
		return QVariant::fromValue(options[row].selected);
	}

	return result;
}

Qt::ItemFlags PLSImporterModel::flags(const QModelIndex &index) const
{
	Q_UNUSED(index)
	return Qt::ItemFlags();
}

bool PLSImporterModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	int row = index.row();
	if (row < 0 || row >= options.count()) {
		return false;
	}

	if (ImporterCustomRole::nameRole == (ImporterCustomRole)role) {
		options[row].name = value.value<QString>();
	} else if (ImporterCustomRole::pathRole == (ImporterCustomRole)role) {
		options[row].path = value.value<QString>();
	} else if (ImporterCustomRole::programRole == (ImporterCustomRole)role) {
		options[row].program = value.value<QString>();
	} else if (ImporterCustomRole::selectedRole == (ImporterCustomRole)role) {
		options[row].selected = value.value<bool>();
	}
	return true;
}
ImporterEntry PLSImporterModel::GetData(int row) const
{
	if (row < 0 || row >= options.count()) {
		return ImporterEntry();
	}
	return options[row];
}

QList<ImporterEntry> PLSImporterModel::GetDatas() const
{
	return options;
}

PLSImporterListView::PLSImporterListView(QWidget *parent) : QListView(parent)
{
	auto model = pls_new<PLSImporterModel>(this);
	PLSImporterListView::setModel(model);

	scrollBar = pls_new<PLSCommonScrollBar>(this);
	setVerticalScrollBar(scrollBar);
	setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	connect(scrollBar, &PLSCommonScrollBar::isShowScrollBar, this, [this](bool show) {
		pls_check_app_exiting();
		emit ScrollBarShow(show);
	});
}

PLSImporterModel *PLSImporterListView::GetModel() const
{
	return (PLSImporterModel *)(model());
}

int PLSImporterListView::GetSelectedCount() const
{
	int count = 0;
	QList<ImporterEntry> datas = GetModel()->GetDatas();
	for (ImporterEntry data : datas) {
		if (data.selected) {
			count += 1;
		}
	}
	return count;
}

void PLSImporterListView::InitWidgets(QList<ImporterEntry> datas) const
{
	GetModel()->InitDatas(datas);
}

void PLSImporterListView::UpdateWidgets()
{
	auto model = GetModel();
	int count = (int)model->options.count();
	for (int i = 0; i < count; i++) {
		UpdateWidget(i, model->GetData(i));
	}
}

void PLSImporterListView::UpdateWidget(int row, const ImporterEntry &data)
{
	auto model = GetModel();
	QModelIndex index = model->createIndex(row, 0, nullptr);
	auto item = CreateItem(row, data);
	setIndexWidget(index, item);
}

void PLSImporterListView::SetData(int row, QVariant variant, ImporterCustomRole role) const
{
	auto model = GetModel();
	QModelIndex modelIndex = model->createIndex(row, 0, nullptr);
	model->setData(modelIndex, variant, (int)role);
}

PLSImporterItem *PLSImporterListView::CreateItem(int row, ImporterEntry data)
{
	auto item = pls_new<PLSImporterItem>(data.name, data.path, data.program, data.selected, this);
	connect(item, &PLSImporterItem::CheckedState, this, [this, row](bool checked) {
		SetData(row, checked, ImporterCustomRole::selectedRole);
		emit DataChanged();
	});
	return item;
}

QList<ImporterEntry> PLSImporterListView::GetDatas() const
{
	return GetModel()->GetDatas();
}
