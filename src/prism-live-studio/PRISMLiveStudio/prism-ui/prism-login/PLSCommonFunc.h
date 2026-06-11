#ifndef PLSCOMMONFUNC_H
#define PLSCOMMONFUNC_H

#include <qstring.h>
#include <qmap.h>
#include <qpair.h>
#include <qapplication.h>
#include "libhttp-client.h"
#include "prism-version.h"

using snsResultCallback = std::function<void(const QJsonDocument &json)>;
using prismPrivacyCallback = std::function<bool(const QString &snsPlatformName, const QMap<QString, QString> &cookies, const QByteArray &data)>;

struct PLSLoginFunc {
	static QString getGpopUrl();
	static QString getGoogleClientId();
	static QString getGoogleClientKey();
	static void showAlertViewAsync(QObject *obj, const QString &text);
	static void sendAction(const QByteArray &body);

	static bool isExistPath(const QString &dirName);

	static QString getUserPath(const QString &dirName, const QString &fileName = QString());

	static bool isKR();
	static QString getCurrentLocaleShort();
	static QLocale getCurrentLocale();
	static QStringList getLocaleKey();

	static QByteArray readFile(const QString &path);
	static bool saveFile(const QString &path, const QByteArray &data);
	static void loadTheme(QWidget *widget, const QStringList &cssFiles);

	static QWidget *getToplevelView(QWidget *widget);
	static QString getPrismVersion();
	static QString getPrismVersionWithBuild();
	static QString makePath(const QString &resDir);
	static void saveUpdateInfo(const QVariantMap &map);
	static QVariantMap getUpdateInfo(const QStringList &keys);

private:
	static QString languageID2Locale(int languageID, const QString &defaultLanguage = "en-US");
	static QMap<int, QPair<QString, QString>> getLocaleName();
};

using PLSLoadingPage = QWidget;
struct PLSUIFunc {
	static PLSLoadingPage *showLoadingView(QWidget *parent = nullptr, const QString &tipStr = QString());
	static void showEnumTimeoutAlertView(const QString &deviceName, QWidget *parent);
};

#endif // PLSCOMMONFUNC_H
