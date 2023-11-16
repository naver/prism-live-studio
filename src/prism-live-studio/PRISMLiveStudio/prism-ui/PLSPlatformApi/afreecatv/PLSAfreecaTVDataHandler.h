#pragma once

#include "../../channels/ChannelsDataApi/PLSChannelDataHandler.h"

class PLSAfreecaTVDataHandler : public ChannelDataBaseHandler {

public:
	PLSAfreecaTVDataHandler() = default;
	~PLSAfreecaTVDataHandler() override = default;

	QString getPlatformName() override;

	bool tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) override;
};
