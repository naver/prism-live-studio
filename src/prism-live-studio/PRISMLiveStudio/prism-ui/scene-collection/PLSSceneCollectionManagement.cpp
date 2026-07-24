#include "PLSSceneCollectionManagement.h"
#include "ui_PLSSceneCollectionManagement.h"
#include "pls-common-define.hpp"
#include "obs-app.hpp"
#include <QPainter>
#include <QWidgetAction>
#include <QTimer>
#include <QMouseEvent>
#include <QShowEvent>
#include "liblog.h"
#include "log/module_names.h"

using namespace common;

// Scene collection popup menu: item height, total border width, top margin
const int SCENE_COLLECTION_POPUP_ITEM_HEIGHT = 40;
const int SCENE_COLLECTION_POPUP_BORDER_WIDTH = 3;
const int SCENE_COLLECTION_POPUP_TOP_MARGIN = 6;

PLSSceneCollectionManagement::PLSSceneCollectionManagement(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSSceneCollectionManagement>();
	ui->setupUi(this);
	ui->listview->SetEnableDrops(false);
	ui->managementLabel->SetText(QTStr("Scene.Collection.View.Management"));
	pls_uistep_v2_set_title(this, QStringLiteral("Scenes Dock"));
	ui->buttonFrame->installEventFilter(this);
	ui->listview->installEventFilter(this);

	pls_add_css(this, {"PLSSceneCollectionManagement"});
	pls_uistep_v2_set_value(ui->goBtn, QStringLiteral("Go"));
	this->installEventFilter(this);
	connect(ui->goBtn, &QPushButton::clicked, this, [this]() { emit ShowSceneCollectionView(); });
	connect(ui->listview, &PLSSceneCollectionListView::TriggerEventEvent, this, &PLSSceneCollectionManagement::OnTriggerEnterEvent);
}

PLSSceneCollectionManagement::~PLSSceneCollectionManagement()
{
	pls_delete(ui);
}

void PLSSceneCollectionManagement::InitDefaultCollectionText(QVector<PLSSceneCollectionData> datas) const
{
	auto manaDatas = datas;
	for (auto &data : manaDatas) {
		data.textMode = true;
	}
	ui->listview->InitWidgets(manaDatas);
}

void PLSSceneCollectionManagement::SetCurrentText(const QString &name, const QString &path) const
{
	ui->listview->SetCurrentData(name, path);
}

void PLSSceneCollectionManagement::AddSceneCollection(const QString &name, const QString &path) const
{
	PLSSceneCollectionData data;
	data.fileName = name;
	data.filePath = path;
	data.textMode = true;
	ui->listview->Add(data);
}

void PLSSceneCollectionManagement::RemoveSceneCollection(const QString &name, const QString &path) const
{
	PLSSceneCollectionData data;
	data.fileName = name;
	data.filePath = path;
	ui->listview->Remove(data);
}

void PLSSceneCollectionManagement::RenameSceneCollection(const QString &srcName, const QString &srcPath, const QString &destName, const QString &destPath) const
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

	ui->listview->Rename(srcData, destData);
}

void PLSSceneCollectionManagement::Resize(int count)
{
	ui->listview->setProperty("fixed", (count - 1) == 1);
	pls_flush_style(ui->listview);

	this->resize(198, count * SCENE_COLLECTION_POPUP_ITEM_HEIGHT + SCENE_COLLECTION_POPUP_BORDER_WIDTH + SCENE_COLLECTION_POPUP_TOP_MARGIN);
}

void PLSSceneCollectionManagement::showEvent(QShowEvent *event)
{
	QFrame::showEvent(event);
	pls_flush_style(ui->buttonFrame, STATUS, STATUS_NORMAL);
}

void PLSSceneCollectionManagement::OnTriggerEnterEvent(const QString &name, const QString &path)
{
	auto finder = [name, path](const PLSSceneCollectionData &data) { return data.fileName == name && data.filePath == path; };
	QVector<PLSSceneCollectionData> datas = ui->listview->GetDatas();
	auto row = -1;
	auto iter = std::find_if(datas.begin(), datas.end(), finder);
	if (iter != datas.end()) {
		row = (int)(iter - datas.begin());
	}

	for (int i = 0; i < ui->listview->Count(); i++) {
		ui->listview->SetData(i, row == i, SceneCollectionCustomRole::EnterRole);
	}
	ui->listview->UpdateWidgets();
}

bool PLSSceneCollectionManagement::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == ui->buttonFrame) {
		if (event->type() == QEvent::MouseButtonPress) {
			auto mouseEvent = dynamic_cast<QMouseEvent *>(event);
			if (mouseEvent->button() == Qt::LeftButton) {
				pls_flush_style(ui->buttonFrame, STATUS, STATUS_CLICKED);
				pls_uistep_v2(this, "Click", "Button", "Scene Set Management");
				emit ShowSceneCollectionView();
			}
		} else if (event->type() == QEvent::Enter) {
			pls_flush_style(ui->buttonFrame, STATUS, STATUS_HOVER);
		} else if (event->type() == QEvent::Leave) {
			pls_flush_style(ui->buttonFrame, STATUS, STATUS_NORMAL);
		}
	} else if (obj == ui->listview && event->type() == QEvent::Enter) {
		// Mouse entered listview without buttonFrame getting Leave; clear buttonFrame hover.
		pls_flush_style(ui->buttonFrame, STATUS, STATUS_NORMAL);
	}

	return QFrame::eventFilter(obj, event);
}
