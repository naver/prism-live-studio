#include "PLSMosquitto.h"

#include <thread>
#include <QApplication>

#include "prism/PLSPlatformPrism.h"
#include "log.h"
#include "log/log.h"
#include "pls-net-url.hpp"
#include "frontend-api.h"

using namespace std;

#define MODULE_MQTT "mqtt"

MQTTContainer PLSMosquitto::mMqtts = MQTTContainer();
QReadWriteLock PLSMosquitto::mMqttLock = QReadWriteLock(QReadWriteLock::Recursive);

PLSMosquitto *PLSMosquitto::instance()
{
	static PLSMosquitto *_instance = nullptr;

	if (nullptr == _instance) {
		mosqpp::lib_init();
		_instance = new PLSMosquitto();
		QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] { delete _instance; });
	}

	return _instance;
}

PLSMosquitto *PLSMosquitto::createInstance()
{
	QWriteLocker locker(&mMqttLock);
	MQTTPtr _instance = new PLSMosquitto();
	mMqtts.append(_instance);
	return _instance;
}

void PLSMosquitto::startIntance(PLSMosquitto *ins, int iVideoSeq, bool bStatistic)
{
	QReadLocker locker(&mMqttLock);
	if (ins && mMqtts.contains(ins)) {
		ins->start(iVideoSeq, bStatistic);
	}
}

void PLSMosquitto::stopInstance(PLSMosquitto *ins)
{
	QReadLocker locker(&mMqttLock);
	if (ins && mMqtts.contains(ins)) {
		ins->stop();
	}
}

void PLSMosquitto::deleteInstance(PLSMosquitto *ins)
{
	QWriteLocker locker(&mMqttLock);
	auto isOK = mMqtts.removeOne(ins);
	if (isOK && ins) {
		ins->deleteLater();
	}
}

bool PLSMosquitto::isInstanceAlive(PLSMosquitto *ins)
{
	QReadLocker locker(&mMqttLock);
	return ins && mMqtts.contains(ins);
}

PLSMosquitto::PLSMosquitto()
{
	username_pw_set("client", MQTT_SERVER_PW.toStdString().c_str());
	threaded_set();
}

void PLSMosquitto::start(int iVideoSeq, bool bStatistic)
{
	PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt start main thread call connnect,Thread=%d ", this_thread::get_id());

	m_iVideoSeq = iVideoSeq;
	m_bStatistic = bStatistic;

	std::thread([this] {
		PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt start child thread call connnect,Thread=%d ", this_thread::get_id());

		m_mqttUrl = MQTT_SERVER;
		if (!pls_is_dev_server()) {
			/*QString ssl = PRISM_SSL;
			if (ssl.startsWith("https://de.") || ssl.startsWith("http://de.")) {
				m_mqttUrl = "msg-de.prismlive.com";
			} else if (ssl.startsWith("https://sg.") || ssl.startsWith("http://sg.")) {
				m_mqttUrl = "msg-sg.prismlive.com";
			} else if (ssl.startsWith("https://us.") || ssl.startsWith("http://us.")) {
				m_mqttUrl = "msg-us.prismlive.com";
			} else if (ssl.startsWith("https://usw.") || ssl.startsWith("http://usw.")) {
				m_mqttUrl = "msg-usw.prismlive.com";
			}*/
			m_mqttUrl = "msg-global.prismlive.com";
		}
		auto retCode = mosqpp::mosquittopp::connect(m_mqttUrl.toStdString().c_str(), 80, 10);
		if (retCode != MOSQ_ERR_SUCCESS) {
			PLS_LIVE_INFO(MODULE_PlatformService, "MQTT status: start connected mqtt error");
		}
		PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt start child thread loop.start,Thread=%d connect retCode: %d,mqttUrl=%s", this_thread::get_id(), retCode, m_mqttUrl.toStdString().c_str());
		loop_forever();
		PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt start child thread loop.end,Thread=%d", this_thread::get_id());
		PLSMosquitto::deleteInstance(this);
	}).detach();
}

void PLSMosquitto::disconnectInstance(PLSMosquitto *ins)
{
	QReadLocker locker(&mMqttLock);
	if (ins && mMqtts.contains(ins)) {
		ins->mosqpp::mosquittopp::disconnect();
	}
}

void PLSMosquitto::stop()
{
	PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt start stop,Thread=%d", this_thread::get_id());
	if (m_status > 0) {
		PLS_LIVE_INFO(MODULE_PlatformService, "MQTT status: living connecting mqtt error, status(%d)", m_status);
	}
	std::thread([this] { PLSMosquitto::disconnectInstance(this); }).detach();

	PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt end stop,Thread=%d", this_thread::get_id());
}

void PLSMosquitto::subscribleAll()
{
	PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt subscribe all ,Thread=%d,mqttUrl=%s", this_thread::get_id(), m_mqttUrl.toStdString().c_str());
	if (m_bStatistic) {
		auto stat = QString("prism/live/%1/pub/stat").arg(m_iVideoSeq);
		subscribe(nullptr, stat.toStdString().c_str());
	}

	auto status = QString("prism/live/%1/priv/status").arg(m_iVideoSeq);
	subscribe(nullptr, status.toStdString().c_str());

	auto chat = QString("prism/live/%1/pub/chat").arg(m_iVideoSeq);
	subscribe(nullptr, chat.toStdString().c_str());
}

void PLSMosquitto::on_connect(int status)
{
	PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt on_connect,Thread=%d,mqttUrl=%s,status=%d", this_thread::get_id(), m_mqttUrl.toStdString().c_str(), status);
	m_status = status;
	subscribleAll();
}

void PLSMosquitto::on_disconnect(int status)
{
	PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt on_disconnect,Thread=%d,mqttUrl=%s,status=%d", this_thread::get_id(), m_mqttUrl.toStdString().c_str(), status);
	m_status = status;
}

void PLSMosquitto::on_message(const struct mosquitto_message *msg)
{
	QString topic(msg->topic);
	QString content(QByteArray(static_cast<const char *>(msg->payload), msg->payloadlen));

	emit onMessage(topic, content);
}

void PLSMosquitto::on_log(int /*level*/, const char *)
{
	//PLS_INFO(MODULE_MQTT, __FUNCTION__ ": Thread=%d %s", this_thread::get_id(), str);
}
