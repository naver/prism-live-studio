#include "PLSLabDownloadFile.h"
#include <QThread>
#include "utils-api.h"
#include <QEventLoop>
#include <QNetworkReply>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include "PLSLaboratoryManage.h"
#include <PLSResCommonFuns.h>

PLSLabDownloadFile::PLSLabDownloadFile(const QString &labId, QObject *parent) : m_labId(labId)
{
	pls_unused(parent);
	PLSLaboratoryManage *manage = PLSLaboratoryManage::instance();
	m_zipFilePath = manage->getLabZipFilePath(m_labId);
	m_unzipFolderPath = manage->getLabZipUnzipFolderPath(m_labId);
	m_title = manage->getStringInfo(m_labId, laboratory_data::g_laboratoryTitle);
	m_version = manage->getStringInfo(m_labId, laboratory_data::g_laboratoryVersion);
	m_type = manage->getStringInfo(m_labId, laboratory_data::g_laboratoryType);
}

PLSLabDownloadFile::~PLSLabDownloadFile()
{
	LAB_LOG(QString("the unzip file task class released, lab id is %1").arg(m_labId));
	emit taskRelease();
}

void PLSLabDownloadFile::run()
{
	LAB_LOG(QString("start run the unzip file task, lab id is %1").arg(m_labId));
	if (!onUncompress(m_zipFilePath)) {
		LAB_LOG(QString("unzip zip file error, lab id is %1").arg(m_labId));
		emit taskFailed();
		return;
	}
	if (!writeDownloadCache()) {
		LAB_LOG(QString("write download map cache error , lab id is %1").arg(m_labId));
		emit taskFailed();
		return;
	}
	LAB_LOG(QString("end run the unzip file task, lab id is %1").arg(m_labId));
	emit taskSucceeded(m_downloadMap);
}

bool PLSLabDownloadFile::onUncompress(const QString &path) const
{
	QFileInfo fi(path);
	QDir dstDir = fi.dir();
	return PLSResCommonFuns::unZip(dstDir.absolutePath(), path, fi.fileName(), false);
}

bool PLSLabDownloadFile::writeDownloadCache()
{
	m_downloadMap.clear();

	//Downloaded plugin id
	if (m_labId.isEmpty()) {
		LAB_LOG("download lab id is empty");
		return false;
	}
	m_downloadMap.insert(g_labDownloadId, m_labId);

	//Downloaded plugin name
	if (LabManage->isDllType(m_labId)) {
		m_title = LabManage->getDllNameWithoutSuffix(m_labId);
	}
	if (m_title.isEmpty()) {
		LAB_LOG("download lab title is empty");
		return false;
	}
	m_downloadMap.insert(g_labDownloadName, m_title);

	//Downloaded plugin ZIP file path
	if (m_zipFilePath.isEmpty()) {
		LAB_LOG("download lab zip path is empty");
		return false;
	}
	m_downloadMap.insert(g_labDownloadZipFilePath, m_zipFilePath);

	//The folder path of the downloaded plugin ZIP after decompression
	if (m_unzipFolderPath.isEmpty()) {
		LAB_LOG("download lab unzip folder path is empty");
		return false;
	}
	m_downloadMap.insert(g_labDownloadUnzipFolderPath, m_unzipFolderPath);

	//Downloaded plugin Zip version
	if (m_version.isEmpty()) {
		LAB_LOG("download lab version is empty");
		return false;
	}
	m_downloadMap.insert(g_labDownloadVersion, m_version);

	//Downloaded plugin type
	if (m_type.isEmpty()) {
		LAB_LOG("download lab type is empty");
		return false;
	}
	m_downloadMap.insert(g_labDownloadType, m_type);

	//Downloaded plugin file name list
	QStringList dllFileNameList;
	LabManage->getDllNameList(m_unzipFolderPath, dllFileNameList);
	m_downloadMap.insert(g_labDownloadDllFileNameList, dllFileNameList);

	//List of all downloaded plugin files
	QStringList filePathList;
	LabManage->getFilePathListRecursive(m_unzipFolderPath, filePathList);
	if (filePathList.isEmpty()) {
		LAB_LOG("download lab all file list is empty");
		return false;
	}
	m_downloadMap.insert(g_labDownloadFileListPath, filePathList);
	m_downloadMap.insert(g_labDownloadUseState, LabManage->getLaboratoryUseState(m_labId));

	return true;
}
