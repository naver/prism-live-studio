#ifndef PLSCHZZKDATAHANDLER_H
#define PLSCHZZKDATAHANDLER_H

#include <qobject.h>

#include "../../channels/ChannelsDataApi/PLSChannelDataHandler.h"
#include "../channels/ChannelsDataApi/PLSChannelDataAPI.h"
#include "../channels/ChannelsDataApi/ChannelCommonFunctions.h"
#include "PLSPlatformChzzk.h"

class PLSChzzkDataHandler : public ChannelDataBaseHandler {

	Q_OBJECT
public:
	explicit PLSChzzkDataHandler() = default;
	~PLSChzzkDataHandler() override;

	QString getPlatformName() override;

	bool tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) override;
	bool isMultiChildren() override { return true; }

	bool hasCountsForLiveEnd() override { return true; };
	QList<QPair<QString, QPixmap>> getEndLiveViewList(const QVariantMap &sourceData) override;

private:
	PLSPlatformChzzk *m_platformChzzk = nullptr;

	QVariantMap m_chzzkLoginInfo;
};

#endif // PLSCHZZKDATAHANDLER_H
