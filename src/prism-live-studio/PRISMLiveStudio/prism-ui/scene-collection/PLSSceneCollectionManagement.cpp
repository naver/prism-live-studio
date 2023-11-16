#include "PLSSceneCollectionManagement.h"
#include "ui_PLSSceneCollectionManagement.h"
#include "pls-common-define.hpp"
#include "obs-app.hpp"
#include <QPainter>
#include <QWidgetAction>
#include <QTimer>
#include <QMouseEvent>
using namespace common;

PLSSceneCollectionManagement::PLSSceneCollectionManagement(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSSceneCollectionManagement>();
	ui->setupUi(this);
	ui->listview->SetEnableDrops(false);
	ui->managementLabel->SetText(QTStr("Scene.Collection.View.Management"));

	ui->buttonFrame->installEventFilter(this);

	pls_add_css(this, {"PLSSceneCollectionManagement"});
	this->installEventFilter(this);
	connect(ui->goBtn, &QPushButton::clicked, this, [this]() { emit ShowSceneCollectionView(); });
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

	this->resize(198, count * 40 + 3);
}

bool PLSSceneCollectionManagement::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == ui->buttonFrame) {
		if (event->type() == QEvent::MouseButtonPress) {
			auto mouseEvent = dynamic_cast<QMouseEvent *>(event);
			if (mouseEvent->button() == Qt::LeftButton) {
				pls_flush_style(ui->buttonFrame, STATUS, STATUS_CLICKED);
				emit ShowSceneCollectionView();
			}
		} else if (event->type() == QEvent::Enter) {
			pls_flush_style(ui->buttonFrame, STATUS, STATUS_HOVER);
		} else if (event->type() == QEvent::Leave) {
			pls_flush_style(ui->buttonFrame, STATUS, STATUS_NORMAL);
		}
	}

	return QFrame::eventFilter(obj, event);
}
