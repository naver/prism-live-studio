/*
* @file		PLSMosquitto.h
* @brief	A mqtt client
* @author	wu.longyue@navercorp.com
* @date		2020-03-14
*/

#pragma once

#include "mosquittopp.h"

#include <thread>
#include <atomic>
#include <QObject>
#include <QList>
#include <QPointer>
#include <QThread>
#include <qreadwritelock.h>
#include <qsemaphore.h>
#include "PLSDualOutputConst.h"

class PLSMosquitto : public QObject, private mosqpp::mosquittopp {
	Q_OBJECT
public:
	PLSMosquitto(DualOutputType outputType);
	~PLSMosquitto() override = default;

	void start(int iVideoSeq);
	void stop();
signals:
	void onMessage(const QString &, const QString &, DualOutputType outputType);

private:
	void subscribleAll();

	//IMPORTANT: These events are called from mqtt thread, NOT main thread.
	void on_connect(int status) override;
	void on_disconnect(int) override;
	void on_message(const struct mosquitto_message *) override;

	int m_iVideoSeq = 0;
	QString m_mqttUrl;
	QThread *m_thread{nullptr};
	QSemaphore m_connected;

	DualOutputType m_outputType = DualOutputType::All;
	std::string m_strVideoSequence;
};
