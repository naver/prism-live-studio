#pragma once

#include "../../channels/ChannelsDataApi/PLSChannelDataHandler.h"

class PLSNaverShoppingLIVEDataHandler : public ChannelDataBaseHandler {

public:
	PLSNaverShoppingLIVEDataHandler();
	virtual ~PLSNaverShoppingLIVEDataHandler();

	virtual QString getPlatformName();

	virtual bool isMultiChildren();

	virtual bool tryToUpdate(const QVariantMap &srcInfo, UpdateCallback callback);
};
