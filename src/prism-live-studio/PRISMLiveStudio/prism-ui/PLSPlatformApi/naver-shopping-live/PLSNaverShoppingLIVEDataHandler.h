#pragma once

#include "../../channels/ChannelsDataApi/PLSChannelDataHandler.h"

class PLSNaverShoppingLIVEDataHandler : public ChannelDataBaseHandler {

public:
	PLSNaverShoppingLIVEDataHandler() = default;
	~PLSNaverShoppingLIVEDataHandler() override = default;

	QString getPlatformName() override;
	bool isMultiChildren() override;
	bool tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &callback) override;
};
