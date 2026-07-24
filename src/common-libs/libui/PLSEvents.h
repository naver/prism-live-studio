#ifndef PLSEVENTS_H
#define PLSEVENTS_H

#include "libui-globals.h"
#include "pls-shared-values.h"
#include <QPixmap>
#include <qobject.h>

class LIBUI_API PLSEvents : public QObject {
	Q_OBJECT

public:
	static PLSEvents *instance();

signals:
	void updateUserIcon(const QPixmap &pximap);
	void restartApp(RestartAppType value);
	void appleIDAuthCallbackUrl(const QString &url);
	void facebookAuthCallbackUrl(const QString &url);
	void delAuthCookies(const QString &loginName);
#if defined(Q_OS_MACOS)
	void processAppExit(int /*pls_product_type_t*/ type);
#endif

private:
	PLSEvents() = default;
	~PLSEvents() override = default;
};

#define PLS_EVENTS PLSEvents::instance()

#endif // PLSEVENTS_H
