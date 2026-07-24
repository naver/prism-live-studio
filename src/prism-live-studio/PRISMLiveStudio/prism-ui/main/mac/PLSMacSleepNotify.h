#pragma once

#include <QObject>

class PLSMacSleepNotify : public QObject {
	Q_OBJECT
public:
	PLSMacSleepNotify(QObject *parent = nullptr);
	~PLSMacSleepNotify();

signals:
	void systemWillSleep();
	void systemDidWake();

private:
	void *m_impl = nullptr; // Pointer to Objective-C object
};
