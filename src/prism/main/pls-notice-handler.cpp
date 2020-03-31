#include "pls-notice-handler.hpp"
#include "pls-net-url.hpp"
#include "pls-app.hpp"
#include "network-access-manager.hpp"
#include "pls-common-define.hpp"
#include "json-data-handler.hpp"
#include <qfile.h>
#include <qdatetime.h>

PLSNoticeHandler *PLSNoticeHandler::getInstance()
{
	static PLSNoticeHandler noticeHandler;
	return &noticeHandler;
}

PLSNoticeHandler::PLSNoticeHandler(QObject *parent) : QObject(parent) {}

PLSNoticeHandler::~PLSNoticeHandler() {}

QString PLSNoticeHandler::getLocale()
{
	const char *lang = App()->GetLocale();
	QString currentLangure(lang);
	return currentLangure.split('-').last();
}

bool PLSNoticeHandler::requestNoticeInfo()
{
	return false;
}

void PLSNoticeHandler::saveNoticeInfo(const QByteArray &noticeData)
{
	return;
}

void PLSNoticeHandler::getUsedNoticeSeqs()
{
	return;
}

QVariantMap PLSNoticeHandler::getNewNotice()
{
	return QVariantMap();
}
