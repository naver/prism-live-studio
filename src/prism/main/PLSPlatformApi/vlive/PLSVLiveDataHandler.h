#pragma once

#include "../../channels/ChannelsDataApi/PLSChannelDataHandler.h"
#include <QObject>

class PLSVLiveDataHandler : public ChannelDataBaseHandler /*, public QObject*/ {
	//Q_OBJECT
public:
	PLSVLiveDataHandler();
	virtual ~PLSVLiveDataHandler();

	virtual QString getPlatformName();

	virtual bool isMultiChildren();

	virtual bool tryToUpdate(const QVariantMap &srcInfo, UpdateCallback finishedCall);

	virtual void updateDisplayInfo(InfosList &srcList);
};
