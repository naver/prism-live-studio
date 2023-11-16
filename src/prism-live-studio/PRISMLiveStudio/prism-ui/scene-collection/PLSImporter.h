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

#pragma once

#include "obs-app.hpp"
#include "window-basic-main.hpp"
#include <QPointer>
#include <QStyledItemDelegate>
#include <QFileInfo>
#include "ui_PLSImporter.h"
#include "PLSDialogView.h"
#include "PLSSceneCollectionItem.h"

class PLSImporterModel;

class PLSImporter : public PLSDialogView {
	Q_OBJECT

	QScopedPointer<Ui::PLSImporter> ui;
	QVector<PLSSceneCollectionData> importFiles;

public:
	explicit PLSImporter(QWidget *parent = nullptr);

	QVector<PLSSceneCollectionData> GetImportFiles() const;
	void ClearImportFiles();

protected:
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
	void importCollections();
	void searchCollections() const;
	void OnScrollBarShow(bool show);

private:
	void checkSelectedState() const;
};
