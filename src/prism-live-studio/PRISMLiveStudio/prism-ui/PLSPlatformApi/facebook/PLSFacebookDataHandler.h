#ifndef PLSFACEBOOKDATAHANDLER_H
#define PLSFACEBOOKDATAHANDLER_H

#include "../../channels/ChannelsDataApi/PLSChannelDataHandler.h"

class PLSFacebookDataHandler : public ChannelDataBaseHandler {
public:
	PLSFacebookDataHandler() = default;
	~PLSFacebookDataHandler() override = default;
	QString getPlatformName() override;
	bool tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) override;
};

#endif // PLSFACEBOOKDATAHANDLER_H
