#include "PLSPrismShareMemory.h"
#include <QApplication>
#include "liblog.h"
#include "log/module_names.h"
#include "libutils-api.h"
#include "pls-shared-values.h"
#include <QProcess>
#include <QSharedMemory>
#include <QBuffer>
#include <QDataStream>

const static char *const k_shared_memory_key = "PLSLauncherSharedMemory";

//-----PLSLauncherShareMemory---
PLSPrismShareMemory *PLSPrismShareMemory::instance()
{
	static PLSPrismShareMemory _instance;
	return &_instance;
}

PLSPrismShareMemory::~PLSPrismShareMemory()
{
	if (m_sharedMemory != nullptr) {
		if (m_sharedMemory->isAttached()) {
			m_sharedMemory->detach();
		}
		pls_delete(m_sharedMemory);
	}
}

QString PLSPrismShareMemory::tryGetMemory()
{
	if (!m_sharedMemory->isAttached()) {
		if (!m_sharedMemory->attach()) {
			PLS_INFO(LAUNCHER_STARTUP, "tryGetMemory attach err: %s", m_sharedMemory->errorString().toUtf8().constData());
			return {};
		}
		PLS_INFO(LAUNCHER_STARTUP, "tryGetMemory isAttached err: %s", m_sharedMemory->errorString().toUtf8().constData());
	}

	QBuffer buffer;
	QDataStream in(&buffer);
	QString text;
	m_sharedMemory->lock();
	buffer.setData(static_cast<const char *>(m_sharedMemory->constData()), (int)m_sharedMemory->size());
	buffer.open(QBuffer::ReadOnly);
	in >> text;

	m_sharedMemory->unlock();
	return text;
}

void PLSPrismShareMemory::sendFilePathToSharedMemeory(const QString &path)
{
	auto sharedMemory = QSharedMemory(k_shared_memory_key);
	auto isAtt = sharedMemory.isAttached();
	if (!isAtt) {
		if (!sharedMemory.attach()) {
			PLS_INFO(LAUNCHER_STARTUP, "sendFilePathToSharedMemeory err: %s", sharedMemory.errorString().toUtf8().constData());
			return;
		}
	}

	QBuffer buffer;
	buffer.open(QBuffer::ReadWrite);
	QDataStream out(&buffer);
	QString fileName;
	out << path;

	sharedMemory.lock();
	auto to = sharedMemory.data();
	const char *from = buffer.data().data();
	memcpy(to, from, qMin(static_cast<qint64>(sharedMemory.size()), buffer.size()));
	sharedMemory.unlock();
	buffer.close();
}

PLSPrismShareMemory::PLSPrismShareMemory()
{
	m_sharedMemory = pls_new<QSharedMemory>(k_shared_memory_key);
	if (!m_sharedMemory->create(4096)) {
		if (m_sharedMemory->error() == QSharedMemory::AlreadyExists) {
			PLS_INFO(LAUNCHER_SHARE_MEMORY, "setupInitData err::  QSharedMemory::AlreadyExists");
		} else {
			PLS_INFO(LAUNCHER_SHARE_MEMORY, "setupInitData err: %s", m_sharedMemory->errorString().toUtf8().constData());
		}
	}
}
