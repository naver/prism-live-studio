#pragma once

#include "../../channels/ChannelsDataApi/PLSChannelDataHandler.h"

class PLSNaverTVDataHandler : public ChannelDataBaseHandler {

public:
	PLSNaverTVDataHandler() = default;
	~PLSNaverTVDataHandler() override = default;

	QString getPlatformName() override;

	bool isMultiChildren() override;

	bool tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &callback) override;
};
