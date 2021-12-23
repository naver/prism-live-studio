#ifndef PLSBEAUTYRESHANDLER_H
#define PLSBEAUTYRESHANDLER_H

#include "PLSResourceHandler.h"

class PLSBeautyResHandler : public PLSResourceHandler {
public:
	PLSBeautyResHandler(const QStringList &resUrls, QObject *parent = nullptr);
	~PLSBeautyResHandler();
	virtual void doWorks(const QByteArray &data, DownLoadResourceMode downMode = DownLoadResourceMode::All, const QString &resPath = QString()) override;
	static void HandleBeautyPresetData(const QString &zipName, const QString &desPath, const QString &id, int itemIndex);
	static bool CopyBeautyRequiredFile();

private:
	void onGetBeautyResourcesSuccess(const QByteArray &array);
	bool CopyBeautyConfigFile();

	bool SaveBeautyJsonFile(const QByteArray &data);

private:
	QVariantMap m_beautyPersetUrls;
	QVariantMap m_beautyImage;
	QStringList m_unzipList;
	int beautyPresetCount = 0;
};

#endif // PLSBEAUTYRESHANDLER_H
