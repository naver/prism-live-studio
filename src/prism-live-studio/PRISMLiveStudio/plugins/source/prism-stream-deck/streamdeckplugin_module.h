#ifndef STREAMDECKPLUGIN_MODULE_H
#define STREAMDECKPLUGIN_MODULE_H

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <frontend-api.h>
#include <QTimer>

extern "C" {
static void OBSEvent(enum obs_frontend_event event, void *switcher);
static void PLSEvent(enum pls_frontend_event event, const QVariantList &params, void *switcher);
static void SaveCallback(obs_data_t *save_data, bool saving, void *);
void FreeStreamDeckPlugin();
void InitStreamDeckPlugin();
}

class ModuleHelper : public QObject {
	Q_OBJECT
public:
	explicit ModuleHelper(QObject *parent = nullptr) : QObject(parent)
	{
		timerUpdate.setSingleShot(true);
		QObject::connect(&timerUpdate, &QTimer::timeout, this, &ModuleHelper::UpdateSourceList);
	};
	~ModuleHelper() final { timerUpdate.stop(); };

public slots:
	void UpdateSourceList() const;
	void DefferUpdate();

private:
	QTimer timerUpdate;
};

#endif // STREAMDECKPLUGIN_MODULE_H
