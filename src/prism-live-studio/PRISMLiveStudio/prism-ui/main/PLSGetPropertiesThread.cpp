#include "PLSGetPropertiesThread.h"
#include <liblog.h>
#include "log/module_names.h"

constexpr uint64_t WAIT_TIMEOUT_MS = 15 * 1000;

static void PropertiesGetterThread()
{
	PLS_LOG(PLS_LOG_INFO, MAINFRAME_MODULE, "%s: Thread started.", __FUNCTION__);
}

static void PropertiesGetterThreadEnd()
{
	PLS_LOG(PLS_LOG_INFO, MAINFRAME_MODULE, "%s: Thread end.", "PropertiesGetterThread");
}

PLSGetPropertiesThread *PLSGetPropertiesThread::Instance()
{
	static PLSGetPropertiesThread instance;
	return &instance;
}

PLSGetPropertiesThread::PLSGetPropertiesThread(QObject *parent) : QObject(parent)
{
	this->moveToThread(&thread);
	connect(&thread, &QThread::started, PropertiesGetterThread);
	connect(&thread, &QThread::finished, PropertiesGetterThreadEnd);
	thread.start();
}

PLSGetPropertiesThread::~PLSGetPropertiesThread()
{
	WaitForFinished();
}

void PLSGetPropertiesThread::GetPropertiesBySource(OBSSource source, quint64 requstId)
{
	PLS_LOG(PLS_LOG_INFO, MAINFRAME_MODULE, "Start GetProperties, request id: %llu", requstId);
	QMetaObject::invokeMethod(this, "_GetPropertiesBySource", Qt::QueuedConnection, Q_ARG(OBSSource, source), Q_ARG(quint64, requstId));
}

void PLSGetPropertiesThread::GetPropertiesBySourceId(const char *id, quint64 requstId)
{
	if (!id)
		return;

	PLS_LOG(PLS_LOG_INFO, MAINFRAME_MODULE, "Start GetProperties, request id: %llu", requstId);
	QString strId(id);
	QMetaObject::invokeMethod(this, "_GetPropertiesBySourceId", Qt::QueuedConnection, Q_ARG(QString, strId), Q_ARG(quint64, requstId));
}

void PLSGetPropertiesThread::GetprivatePropertiesBySource(OBSSource source, quint64 requstId)
{
	PLS_LOG(PLS_LOG_INFO, MAINFRAME_MODULE, "Start GetProperties, request id: %llu", requstId);
	QMetaObject::invokeMethod(this, "_GetprivatePropertiesBySource", Qt::QueuedConnection, Q_ARG(OBSSource, source), Q_ARG(quint64, requstId));
}

void PLSGetPropertiesThread::WaitForFinished()
{
	stopped = true;
	thread.quit();
	if (!thread.wait(WAIT_TIMEOUT_MS)) {
		PLS_LOG(PLS_LOG_WARN, MAINFRAME_MODULE, "Failed to wait PropertiesGetterThread end.");
	}
}

void PLSGetPropertiesThread::_GetPropertiesBySource(OBSSource source, quint64 requstId)
{
	if (!stopped) {
		obs_properties_t *result = obs_source_properties(source);
		PropertiesParam_t param;
		param.id = requstId;
		param.properties = result;
		if (!stopped)
			emit OnProperties(param);
	}

	PLS_LOG(PLS_LOG_INFO, MAINFRAME_MODULE, "End GetProperties, request id: %llu", requstId);
}

void PLSGetPropertiesThread::_GetPropertiesBySourceId(QString id, quint64 requstId)
{
	if (!stopped) {
		obs_properties_t *result = obs_get_source_properties(id.toUtf8().constData());
		PropertiesParam_t param;
		param.id = requstId;
		param.properties = result;
		if (!stopped)
			emit OnProperties(param);
	}

	PLS_LOG(PLS_LOG_INFO, MAINFRAME_MODULE, "End GetProperties, request id: %llu", requstId);
}

void PLSGetPropertiesThread::_GetprivatePropertiesBySource(OBSSource source, quint64 requstId)
{
	if (!stopped) {
		/*obs_properties_t *result = obs_source_private_properties(source);
		PropertiesParam_t param;
		param.id = requstId;
		param.properties = result;
		if (!stopped)
			emit OnPrivateProperties(param);*/
	}

	PLS_LOG(PLS_LOG_INFO, MAINFRAME_MODULE, "End GetProperties, request id: %llu", requstId);
}
