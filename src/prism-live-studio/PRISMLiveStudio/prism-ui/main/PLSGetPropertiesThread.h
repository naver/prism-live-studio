#ifndef PLSGETPROPERTIESTHREAD_H
#define PLSGETPROPERTIESTHREAD_H

#include <QObject>
#include <QThread>
#include <obs.hpp>

struct PropertiesParam_t {
	obs_properties_t *properties;
	quint64 id;
};

Q_DECLARE_METATYPE(PropertiesParam_t)

class PLSGetPropertiesThread : public QObject {
	Q_OBJECT
public:
	static PLSGetPropertiesThread *Instance();

	explicit PLSGetPropertiesThread(QObject *parent = nullptr);
	~PLSGetPropertiesThread() final;

	void GetPropertiesBySource(OBSSource source, quint64 requstId);

	void GetPropertiesBySourceId(const char *id, quint64 requstId);

	void GetprivatePropertiesBySource(OBSSource source, quint64 requstId);

	void WaitForFinished();

private slots:
	void _GetPropertiesBySource(OBSSource source, quint64 requstId);

	void _GetPropertiesBySourceId(QString id, quint64 requstId);

	void _GetprivatePropertiesBySource(OBSSource source, quint64 requstId);

signals:
	void OnProperties(PropertiesParam_t param);
	void OnPrivateProperties(PropertiesParam_t param);

private:
	QThread thread;
	std::atomic_bool stopped = false;
};

#endif // PLSGETPROPERTIESTHREAD_H
