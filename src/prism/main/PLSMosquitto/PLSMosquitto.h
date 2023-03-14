/*
* @file		PLSMosquitto.h
* @brief	A mqtt client
* @author	wu.longyue@navercorp.com
* @date		2020-03-14
*/

#pragma once

#include "mosquittopp.h"

#include <thread>
#include <QObject>
#include <QList>
#include <QPointer>
#include <qreadwritelock.h>

class PLSMosquitto;
using MQTTPtr = QPointer<PLSMosquitto>;
using MQTTContainer = QList<MQTTPtr>;

class PLSMosquitto : public QObject, private mosqpp::mosquittopp {
	Q_OBJECT
public:
	static PLSMosquitto *instance();

	static PLSMosquitto *createInstance();
	static void deleteInstance(PLSMosquitto *ins);
	static bool isInstanceAlive(PLSMosquitto *ins);

	static void startIntance(PLSMosquitto *ins, int iVideoSeq, bool bStatistic);
	static void stopInstance(PLSMosquitto *ins);
	static void disconnectInstance(PLSMosquitto *ins);

private:
	PLSMosquitto();
	~PLSMosquitto() = default;

private:
	void start(int iVideoSeq, bool bStatistic);
	void stop();
signals:
	//IMPORTANT: Different thread, Must pass by value
	void onMessage(QString, QString);

private:
	void subscribleAll();

	//IMPORTANT: These events are called from mqtt thread, NOT main thread.
	void on_connect(int status) override;
	void on_disconnect(int) override;
	void on_message(const struct mosquitto_message *) override;
	void on_log(int /*level*/, const char * /*str*/) override;

private:
	int m_iVideoSeq = 0;
	bool m_bStatistic = true;
	static MQTTContainer mMqtts;
	static QReadWriteLock mMqttLock;
	int m_status = 0;
	QString m_mqttUrl;
};
