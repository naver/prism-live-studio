#ifndef PLSCOLORFILTERWEBHANDLER_H
#define PLSCOLORFILTERWEBHANDLER_H

#include "network-access-manager.hpp"

#include <QByteArray>
#include <QObject>
#include <QThread>

class PLSColorFilterWebHandler : public QObject {
	Q_OBJECT
public:
	explicit PLSColorFilterWebHandler(QObject *parent = nullptr);
	~PLSColorFilterWebHandler();

	void GetSyncResourcesRequest();

private:
	bool MoveDirectoryToDest(const QString &srcDir, const QString &destDir, bool isRemove = false);
};

#endif // PLSCOLORFILTERWEBHANDLER_H
