#pragma once

#include "../../channels/ChannelsDataApi/PLSChannelDataHandler.h"

class PLSNCB2BDataHandler : public ChannelDataBaseHandler {

public:
	PLSNCB2BDataHandler() = default;
	~PLSNCB2BDataHandler() override = default;

	QString getPlatformName() override;
	bool isMultiChildren() override;

	bool tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) override;

	bool hasCountsForLiveEnd() override { return true; };
	QList<QPair<QString, QPixmap>> getEndLiveViewList(const QVariantMap &sourceData) override;
};
