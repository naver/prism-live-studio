#pragma once

#include "../../channels/ChannelsDataApi/PLSChannelDataHandler.h"

class PLSNaverTVDataHandler : public ChannelDataBaseHandler {

public:
	PLSNaverTVDataHandler();
	virtual ~PLSNaverTVDataHandler();

	virtual QString getPlatformName();

	virtual bool isMultiChildren();

	virtual bool tryToUpdate(const QVariantMap &srcInfo, UpdateCallback callback);
};
