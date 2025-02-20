#include "PLSMosquitto.h"

#include <thread>
#include <QApplication>
#include "log/log.h"
#include "PLSPlatformPrism.h"
#include "pls-net-url.hpp"
#include "frontend-api.h"

using namespace std;
constexpr auto MODULE_MQTT = "mqtt";

PLSMosquitto::PLSMosquitto(DualOutputType outputType) : m_outputType(outputType)
{
	username_pw_set("client", MQTT_SERVER_PW.toStdString().c_str());
	threaded_set();
}

void PLSMosquitto::start(int iVideoSeq)
{
	PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt start main thread call connnect,Thread=%d ", this_thread::get_id());

	m_iVideoSeq = iVideoSeq;

	m_thread = QThread::create([this] {
		PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt start child thread call connnect,Thread=%d ", this_thread::get_id());

		m_mqttUrl = MQTT_SERVER;
		if (!pls_is_dev_server()) {
			m_mqttUrl = "msg-global.prismlive.com";
		}

		auto retCode = mosqpp::mosquittopp::connect(m_mqttUrl.toStdString().c_str(), 80, 10);
		pls_check_app_exiting();
		if (retCode != MOSQ_ERR_SUCCESS) {
			PLS_LIVE_INFO(MODULE_PlatformService, "MQTT status: start connected mqtt error");
		}

		PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt start child thread loop.start,Thread=%d connect retCode: %d,mqttUrl=%s", this_thread::get_id(), retCode, m_mqttUrl.toStdString().c_str());
		m_connected.release();
		loop_forever();
		pls_check_app_exiting();
		PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt start child thread loop.end,Thread=%d", this_thread::get_id());
	});
	m_thread->start();
}

void PLSMosquitto::stop()
{
	PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt start stop,Thread=%d", this_thread::get_id());
	std::thread([this] {
		m_connected.acquire();
		mosqpp::mosquittopp::disconnect();
		pls_check_app_exiting();
		m_thread->wait();
		pls_check_app_exiting();
		m_thread->deleteLater();
		deleteLater();
		pls_check_app_exiting();
		PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt end stop,Thread=%d", this_thread::get_id());
	}).detach();
}

void PLSMosquitto::subscribleAll()
{
	PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt subscribe all ,Thread=%d,mqttUrl=%s", this_thread::get_id(), m_mqttUrl.toStdString().c_str());
	auto stat = QString("prism/live/%1/pub/stat").arg(m_iVideoSeq);
	subscribe(nullptr, stat.toStdString().c_str());

	auto status = QString("prism/live/%1/priv/status").arg(m_iVideoSeq);
	subscribe(nullptr, status.toStdString().c_str());

	auto chat = QString("prism/live/%1/pub/chat").arg(m_iVideoSeq);
	subscribe(nullptr, chat.toStdString().c_str());
}

void PLSMosquitto::on_connect(int status)
{
	PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt on_connect,Thread=%d,mqttUrl=%s,status=%d", this_thread::get_id(), m_mqttUrl.toStdString().c_str(), status);
	subscribleAll();
}

void PLSMosquitto::on_disconnect(int status)
{
	PLS_LIVE_INFO(MODULE_MQTT, "MQTT status: mqtt on_disconnect,Thread=%d,mqttUrl=%s,status=%d", this_thread::get_id(), m_mqttUrl.toStdString().c_str(), status);
}

void PLSMosquitto::on_message(const struct mosquitto_message *msg)
{
	QString topic(msg->topic);
	QString content(QByteArray(static_cast<const char *>(msg->payload), msg->payloadlen));

	emit onMessage(topic, content, m_outputType);
}
