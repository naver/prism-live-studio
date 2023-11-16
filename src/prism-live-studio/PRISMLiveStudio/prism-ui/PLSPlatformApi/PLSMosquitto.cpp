#include "PLSMosquitto.h"


PLSMosquitto::PLSMosquitto()
{
	
}

void PLSMosquitto::start(int iVideoSeq)
{
	
}

void PLSMosquitto::stop()
{
	
}

void PLSMosquitto::subscribleAll()
{
	
}

void PLSMosquitto::on_connect(int status)
{
	
}

void PLSMosquitto::on_disconnect(int status)
{
	
}

void PLSMosquitto::on_message(const struct mosquitto_message *msg)
{
	QString topic(msg->topic);
	QString content(QByteArray(static_cast<const char *>(msg->payload), msg->payloadlen));

	emit onMessage(topic, content);
}
