
#include "PLSSyncServerManager.hpp"
#include <QVariant>

PLSSyncServerManager *PLSSyncServerManager::instance()
{
	static PLSSyncServerManager syncServerManager;
	return &syncServerManager;
}

PLSSyncServerManager::PLSSyncServerManager(QObject *parent) : QObject(parent) {}

bool PLSSyncServerManager::initOutroPolicy(const QString &path)
{
	return true;
}

bool PLSSyncServerManager::initWatermark(const QString &path)
{
	return true;
}

const QMap<QString, QVariantList> &PLSSyncServerManager::getStickerReaction()
{
	return QMap<QString, QVariantList>();
}

bool PLSSyncServerManager::initSupportedResolutionFPS()
{
	return true;
}

const QMap<QString, QVariantMap> &PLSSyncServerManager::outroMap()
{
	return m_outroMap;
}

const QMap<QString, QVariantMap> &PLSSyncServerManager::watermarkMap()
{
	return m_watermarkMap;
}

const QVariantMap &PLSSyncServerManager::platformFPSMap()
{
	return m_platformFPSMap;
}

const QVariantList &PLSSyncServerManager::getResolutionsList()
{
	return m_resolutionsInfos;
}

QString PLSSyncServerManager::getPolicyFileNameByURL(const QString &url)
{
	return QString();
}

QString PLSSyncServerManager::getPolicyFilePathByURL(const QString &url)
{
	return QString();
}

bool PLSSyncServerManager::isExistPolicyFileByURL(const QString &url)
{
	return true;
}

void PLSSyncServerManager::appendMap(QVariantMap &destMap, const QVariantMap &srcMap) {}
