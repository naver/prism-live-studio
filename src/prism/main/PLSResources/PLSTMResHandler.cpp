#include "PLSTMResHandler.h"
#include "pls-app.hpp"

PLSTMResHandler::PLSTMResHandler(const QStringList &resUrls, QObject *parent) : PLSResourceHandler(resUrls, parent) {}

PLSTMResHandler::~PLSTMResHandler() {}

void PLSTMResHandler::doWorks(const QByteArray &data, DownLoadResourceMode downMode, const QString &resPath)
{
	switch (downMode) {
	case DownLoadResourceMode::All:
		onGetTMResourcesProcess(data);
		break;
	case DownLoadResourceMode::Part:
		saveResource(data, resPath);
		break;
	default:
		break;
	}
}

void PLSTMResHandler::onGetTMResourcesProcess(const QByteArray &data)
{
	if (!PLSJsonDataHandler::saveJsonFile(data, m_relativeResPath + "textMotionTemplate.json")) {
		PLS_ERROR(MAINFILTER_MODULE, "save to json %s failed", "textMotionTemplate.json");
	}

	QString lang = QString(App()->GetLocale()).split('-')[0];

	QStringList _langList = PLSResourceHandler::getLanguageKey();

	QVariantList list;
	PLSJsonDataHandler::getValuesFromByteArray(data, "group", list);
	for (auto map : list) {
		QString name = map.toMap().value(name2str(groupId)).toString().toLower();
		QJsonArray templateList = map.toMap().value(name2str(items)).toJsonArray();
		for (auto jsonValue : templateList) {
			QVariantMap templateMap = jsonValue.toObject().toVariantMap();
			auto name = templateMap.value(name2str(title)).toString();

			//get lang
			const auto &adjustAttributes = templateMap.value(name2str(properties)).toMap().value("template").toMap().value("baseInfoPC").toMap().value("adjustableAttribute").toMap();
			if (adjustAttributes.find(lang) == adjustAttributes.end()) {
				lang = "en";
			}
			//get gif url
			for (auto lang : _langList) {
				if (adjustAttributes.find(lang) != adjustAttributes.end()) {
					QString gifUrl = adjustAttributes.value(lang).toMap().value(name2str(thumbnailUrl)).toString();
					downLoadGif(gifUrl, lang, name);
				}
			}
		}
	}
}
void PLSTMResHandler::downLoadGif(const QString &urlStr, const QString &language, const QString &id)
{
	PLSNetworkReplyBuilder builder(urlStr);
	auto imageCallback = [](bool, const QString &, void *) {};
	auto reply = builder.get(m_manager);
	appendReply(reply);
	PLSHttpHelper::downloadImageAsync(reply, this, m_relativeResPath, imageCallback, language, id, QString(), nullptr, PRISM_NET_DOWNLOAD_TIMEOUT);
}
