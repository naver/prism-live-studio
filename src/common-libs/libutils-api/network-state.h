#pragma once

#include "libutils-api.h"

#include <memory>
#include <QObject>

namespace pls {
class NetworkEvent;
class NetworkStatePrivate;

class LIBUTILSAPI_API NetworkState : public QObject {
	Q_OBJECT
	Q_DECLARE_PRIVATE(NetworkState)
	PLS_DISABLE_COPY_AND_MOVE(NetworkState)

	friend class NetworkEvent;

private:
	NetworkState();
	~NetworkState() override = default;

public:
	static NetworkState *instance();

	bool isAvailable() const;

signals:
	void stateChanged(bool available);
};
}
