#pragma once
#include <QObject>

const char *const k_task_res_search = "PRISMResCheck";

class QTimer;
class QSharedMemory;

class PLSPrismShareMemory : public QObject {
	Q_OBJECT
public:
	static PLSPrismShareMemory *instance();
	~PLSPrismShareMemory() final;

	QString tryGetMemory();
	static void sendFilePathToSharedMemeory(const QString &path);

private:
	PLSPrismShareMemory();

	QSharedMemory *m_sharedMemory;
};
#define PLS_PRSIM_SHARE_MEMORY PLSPrismShareMemory::instance()
