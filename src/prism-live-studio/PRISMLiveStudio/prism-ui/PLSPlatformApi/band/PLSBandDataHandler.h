#ifndef PLSBANDDATAHANDLER_H
#define PLSBANDDATAHANDLER_H

#include <qobject.h>

#include "../../channels/ChannelsDataApi/PLSChannelDataHandler.h"
#include "../channels/ChannelsDataApi/PLSChannelDataAPI.h"
#include "../channels/ChannelsDataApi/ChannelCommonFunctions.h"
#include "PLSPlatformBand.h"

class PLSBandDataHandler : public ChannelDataBaseHandler {

	Q_OBJECT
public:
	explicit PLSBandDataHandler() = default;
	~PLSBandDataHandler() override;

	QString getPlatformName() override;

	bool tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) override;
	bool isMultiChildren() override { return true; }

private:
	QList<QVariantMap> m_bandInfos;
	PLSPlatformBand *m_platformBand = nullptr;

	QVariantMap m_bandLoginInfo;
};

#endif // PLSBANDDATAHANDLER_H
