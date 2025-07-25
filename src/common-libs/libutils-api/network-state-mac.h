
#pragma once

#include <QObject>
#include "libutils-api.h"

namespace pls {

class NetworkState : public QObject {
	Q_OBJECT
	//Q_DECLARE_PRIVATE(NetworkState)
	PLS_DISABLE_COPY_AND_MOVE(NetworkState)

private:
	explicit NetworkState(QObject *parent = nullptr);
	~NetworkState() override = default;

public:
	static NetworkState *instance();
	bool isAvailable() const;

signals:
	void stateChanged(bool available);
};
}
