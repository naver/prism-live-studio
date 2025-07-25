/******************************************************************************
    Copyright (C) 2019-2020 by Dillon Pentz <dillon@vodbox.io>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "PLSImporter.h"

#include "obs-app.hpp"

#include <QPushButton>
#include <QLineEdit>
#include <QToolButton>
#include <QMimeData>
#include <QStyledItemDelegate>
#include <QDirIterator>
#include <QDropEvent>
#include <QToolTip>

#include "qt-wrappers.hpp"
#include "importers/importers.hpp"
#include "PLSImporterItem.h"
#include "PLSAlertView.h"
#include "liblog.h"

/**
	Window
**/

PLSImporter::PLSImporter(QWidget *parent) : PLSDialogView(parent)
{
	ui.reset(pls_new<Ui::PLSImporter>());
	setupUi(ui);
	setAcceptDrops(true);
	setAttribute(Qt::WA_AlwaysShowToolTips, true);

	pls_add_css(this, {"PLSImporter"});
	ui->buttonBox->setFixedWidth(266);

#if defined(Q_OS_MACOS)
	initSize(QSize(710, 530));
	ui->buttonBox->setContentsMargins(0, 0, 0, 0);
#elif defined(Q_OS_WIN)
	initSize(QSize(710, 570));
#endif

	connect(ui->listView, &PLSImporterListView::DataChanged, this, &PLSImporter::checkSelectedState);
	connect(ui->listView, &PLSImporterListView::ScrollBarShow, this, &PLSImporter::OnScrollBarShow);

	ui->buttonBox->button(QDialogButtonBox::Cancel)->setObjectName("Cancel");
	ui->buttonBox->button(QDialogButtonBox::Ok)->setObjectName("Ok");

	connect(ui->buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, [this]() { importCollections(); });
	connect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, [this]() { close(); });

	searchCollections();
	checkSelectedState();
}

QVector<PLSSceneCollectionData> PLSImporter::GetImportFiles() const
{
	return importFiles;
}

void PLSImporter::ClearImportFiles()
{
	importFiles.clear();
}

void PLSImporter::showEvent(QShowEvent *event)
{
	searchCollections();
	PLSDialogView::showEvent(event);
}

bool checkExisted(const QVector<PLSSceneCollectionData> &importFiles, const std::string &name)
{
	QVector<QString> collections = pls_get_scene_collections();
	for (const auto &collection : collections) {
		if (0 == collection.compare(name.c_str())) {
			return true;
		}
	}
	for (const auto &file : importFiles) {
		if (0 == file.fileName.compare(name.c_str())) {
			return true;
		}
	}
	return false;
}

bool GetUnusedName(QVector<PLSSceneCollectionData> importFiles, std::string &name)
{
	if (!checkExisted(importFiles, name))
		return false;

	std::string newName;
	int inc = 2;
	do {
		newName = name;
		newName += " (";
		newName += std::to_string(inc);
		newName += ")";
		inc++;
	} while (checkExisted(importFiles, newName));

	name = newName;
	return true;
}

void PLSImporter::importCollections()
{
	pls::chars<512> dst;
	GetAppConfigPath(dst, 512, "PRISMLiveStudio/basic/scenes/");

	bool importError = false;
	bool importNotExisted = false;
	QList<ImporterEntry> datas = ui->listView->GetDatas();
	for (ImporterEntry data : datas) {
		if (!data.selected) {
			continue;
		}

		std::string pathStr = data.path.toStdString();
		std::string nameStr = data.name.toStdString();

		json11::Json res;
		int errorCode = ImportSC(pathStr, nameStr, res);
		if (errorCode == IMPORTER_FILE_NOT_FOUND) {
			importNotExisted = true;
			continue;
		}
		if (res == json11::Json()) {
			importError = true;
			continue;
		}

		QVector<QString> collectionNames = pls_get_scene_collections();

		if (res != json11::Json()) {
			json11::Json::object out = res.object_items();
			std::string name = res["name"].string_value();
			std::string file;

			if (GetUnusedName(importFiles, name)) {
				json11::Json::object newOut = out;
				newOut["name"] = name;
				out = newOut;
			}

			GetUnusedSceneCollectionFile(name, file);

			std::string save = dst.data();
			save += "/";
			save += file;
			save += ".json";

			std::string out_str = json11::Json(out).dump();

			bool success = os_quick_write_utf8_file(save.c_str(), out_str.c_str(), out_str.size(), false);
			if (success) {
				PLSSceneCollectionData importData;
				importData.fileName = name.c_str();
				importData.filePath = save.c_str();
				importFiles.push_back(importData);
			}
			PLS_INFO("PLSImporter", "Import Scene Collection: %s (%s) - %s", name.c_str(), file.c_str(), success ? "SUCCESS" : "FAILURE");
		}
	}

	if (importNotExisted) {
		PLSAlertView::warning(nullptr, QTStr("Alert.title"), QTStr("Scene.Collection.Import.NotFound"));
	}

	if (importError) {
		PLSAlertView::warning(nullptr, QTStr("Alert.title"), QTStr("Scene.Collection.Import.Error"));
	}

	accept();
}

void PLSImporter::searchCollections() const
{
	OBSImporterFiles f;
	f = ImportersFindFiles();

	QList<ImporterEntry> datas;

	for (std::string file : f) {
		QString path = file.c_str();
		path.replace("\\", "/");
		ImporterEntry entry;
		entry.program = DetectProgram(path.toStdString()).c_str();
		entry.name = GetSCName(path.toStdString(), entry.program.toStdString()).c_str();
		entry.path = path;
		datas.push_back(entry);
	}
	ui->listView->InitWidgets(datas);

	if (datas.isEmpty()) {
		ui->stackedWidget->setCurrentWidget(ui->noDataPage);
	} else {
		ui->stackedWidget->setCurrentWidget(ui->listPage);
	}

	f.clear();
	checkSelectedState();
}

void PLSImporter::OnScrollBarShow(bool show)
{
	if (show) {
		ui->horizontalLayout_4->setContentsMargins(12, 0, 2, 0);
	} else {
		ui->horizontalLayout_4->setContentsMargins(12, 0, 12, 0);
	}
}

void PLSImporter::checkSelectedState() const
{
	int selected = ui->listView->GetSelectedCount();

	if (selected > 0) {
		ui->nameSelectLabel->setText(QTStr("Scene.Collection.Importer.Scenes.Selected").arg(selected));
	}
	ui->nameSelectLabel->setVisible(selected > 0);
	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(selected > 0);
}
