#include "PLSGetPropertiesThread.h"

PLSGetPropertiesThread *PLSGetPropertiesThread::Instance()
{
	static PLSGetPropertiesThread instance;
	return &instance;
}

PLSGetPropertiesThread::PLSGetPropertiesThread(QObject *parent) : QObject(parent)
{
	this->moveToThread(&thread);
	thread.start();
}

PLSGetPropertiesThread::~PLSGetPropertiesThread()
{
	WaitForFinished();
}

void PLSGetPropertiesThread::GetPropertiesBySource(OBSSource source, quint64 requstId)
{
	QMetaObject::invokeMethod(this, "_GetPropertiesBySource", Qt::QueuedConnection, Q_ARG(OBSSource, source), Q_ARG(quint64, requstId));
}

void PLSGetPropertiesThread::GetPropertiesBySourceId(const char *id, quint64 requstId)
{
	if (!id)
		return;
	QString strId(id);
	QMetaObject::invokeMethod(this, "_GetPropertiesBySourceId", Qt::QueuedConnection, Q_ARG(QString, strId), Q_ARG(quint64, requstId));
}

void PLSGetPropertiesThread::GetprivatePropertiesBySource(OBSSource source, quint64 requstId)
{
	QMetaObject::invokeMethod(this, "_GetprivatePropertiesBySource", Qt::QueuedConnection, Q_ARG(OBSSource, source), Q_ARG(quint64, requstId));
}

void PLSGetPropertiesThread::WaitForFinished()
{
	stopped = true;
	thread.quit();
	thread.wait();
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
}

void PLSGetPropertiesThread::_GetprivatePropertiesBySource(OBSSource source, quint64 requstId)
{
	if (!stopped) {
		obs_properties_t *result = obs_source_private_properties(source);
		PropertiesParam_t param;
		param.id = requstId;
		param.properties = result;
		if (!stopped)
			emit OnPrivateProperties(param);
	}
}
