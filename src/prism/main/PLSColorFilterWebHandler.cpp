#include "PLSColorFilterWebHandler.h"

#include <QDir>
#include "log/module_names.h"
#include "log.h"

#include "pls-common-define.hpp"
#include "frontend-api.h"

PLSColorFilterWebHandler::PLSColorFilterWebHandler(QObject *parent /*= nullptr*/) : QObject(parent) {}

PLSColorFilterWebHandler::~PLSColorFilterWebHandler() {}

void PLSColorFilterWebHandler::GetSyncResourcesRequest()
{
	// copy color_filter file to user path
	MoveDirectoryToDest(CONFIG_COLOR_FILTER_PATH, pls_get_color_filter_dir_path(), false);
}

bool PLSColorFilterWebHandler::MoveDirectoryToDest(const QString &srcDir, const QString &destDir, bool isRemove)
{
	QDir sourceDir(srcDir);
	QDir targetDir(destDir);

	if (!targetDir.exists()) {
		bool ok = targetDir.mkpath(destDir);
		if (!ok) {
			PLS_ERROR(MAINFILTER_MODULE, "mkdir %s failed.", destDir.toStdString().c_str());
			return false;
		}
	}

	QFileInfoList fileInfoList = sourceDir.entryInfoList();
	foreach(QFileInfo fileInfo, fileInfoList)
	{
		if (fileInfo.fileName() == "." || fileInfo.fileName() == ".." || fileInfo.isDir())
			continue;

		if (targetDir.exists(fileInfo.fileName())) {
			if (isRemove) {
				targetDir.remove(fileInfo.fileName());
			}
		}

		if (!QFile::copy(fileInfo.filePath(), targetDir.filePath(fileInfo.fileName()))) {
			return false;
		}
	}
	sourceDir.removeRecursively();
	return true;
}
