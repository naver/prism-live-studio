#include "PLSMosquitto.h"

#include <thread>
#include <QApplication>
#include "log/log.h"
#include "PLSPlatformPrism.h"
#include "pls-net-url.hpp"
#include "frontend-api.h"

using namespace std;
constexpr auto MODULE_MQTT = "mqtt";

#define PLS_LIVE_INFO_EX(module_name, sequence, format, ...) pls_flow2_log(false, PLS_LOG_INFO, module_name, "LiveStatus", sequence, __FILE__, __LINE__, format, __VA_ARGS__)

PLSMosquitto::PLSMosquitto(DualOutputType outputType) : m_outputType(outputType)
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
}
