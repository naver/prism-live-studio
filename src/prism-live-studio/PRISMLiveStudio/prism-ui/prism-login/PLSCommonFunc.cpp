#include "PLSCommonFunc.h"
#include <qsettings.h>
#include <qlayout.h>
#include "PLSCommonConst.h"
#include <qapplication.h>
#include <qstandardpaths.h>
#include <qdir.h>
#include <qapplication.h>
#include <qwidget.h>
#include "PLSDialogView.h"
#include "liblog.h"
#include "pls-common-define.hpp"
#include "prism-version.h"
#include "loading-event.hpp"
#include "window-basic-interaction.hpp"
#include <util/platform.h>
#include "PLSAlertView.h"
#include "frontend-api.h"

#define USER_CACHE_PATH QString("%1/%2").arg(PLSResCommonFuns::getAppLocationPath()).arg("/user/cache")

QString PLSLoginFunc::getGpopUrl()
{
	return QString("%1/%2").arg(pls_http_api_func::getPrismHost()).arg(pls_launcher_const::GPOP_URL);
}

QString PLSLoginFunc::getGoogleClientId()
{
	return pls_prism_is_dev() ? pls_launcher_const::YOUTUBE_CLIENT_ID_DEV : pls_launcher_const::YOUTUBE_CLIENT_ID_;
}
QString PLSLoginFunc::getGoogleClientKey()
{
	return pls_prism_is_dev() ? pls_launcher_const::YOUTUBE_CLIENT_KEY_DEV : pls_launcher_const::YOUTUBE_CLIENT_KEY_;
}

void PLSLoginFunc::showAlertViewAsync(QObject *obj, const QString &text)
{
	pls_unused(text);
	QMetaObject::invokeMethod(
		obj, []() { /*add alert view*/ }, Qt::QueuedConnection);
}

void PLSLoginFunc::sendAction(const QByteArray &body)
{
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post) //
				   .jsonContentType()               //
				   .withLog()                       //
				   .receiver(qApp)                  //
				   .workInMainThread()              //
				   .body(body)                      //
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .hmacUrl(QUrl(QString("%1%2").arg(pls_http_api_func::getPrismLogGateWay()).arg(pls_launcher_const::PLS_ACTION_LOG)), pls_http_api_func::getPrismHamcKey())
				   .failResult([](const pls::http::Reply &reply) { PLS_ERROR(LAUNCHER_STARTUP, "send action log failed error = %d", reply.error()); }));
}

bool PLSLoginFunc::isExistPath(const QString &dirName)
{
	auto resDir = getUserPath(dirName);
	QDir dir(resDir);
	bool isExist = dir.exists();
	return isExist;
}

QString PLSLoginFunc::getUserPath(const QString &dirName, const QString &fileName)
{
	QString path = pls_get_prism_subpath(dirName, true);
	if (!makePath(path).isEmpty() && !fileName.isEmpty()) {
		path += "/" + fileName;
	}
	return path;
}

bool PLSLoginFunc::isKR()
{
	return pls_prism_get_locale().contains("kr", Qt::CaseInsensitive);
}

QString PLSLoginFunc::getCurrentLocaleShort()
{
	return pls_prism_get_locale().section(QRegularExpression("\\W+"), 0, 0);
}

QLocale PLSLoginFunc::getCurrentLocale()
{
	return QLocale(pls_prism_get_locale());
}

QByteArray PLSLoginFunc::readFile(const QString &path)
{
	QByteArray array;
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return array;
	array = file.readAll();
	file.close();
	return array;
}

bool PLSLoginFunc::saveFile(const QString &path, const QByteArray &data)
{
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly))
		return false;
	file.write(data);
	file.close();
	return true;
}

void PLSLoginFunc::loadTheme(QWidget *widget, const QStringList &cssFiles)
{
	if (!widget) {
		return;
	} else {

		QByteArray cssData;
		for (const auto &cssFile : cssFiles) {
			QString path(QString(pls_launcher_const::PLS_THEMES_PATH).arg(cssFile));
			cssData.append(readFile(path));
		}
		widget->setStyleSheet(cssData);
	}
}

QWidget *PLSLoginFunc::getToplevelView(QWidget *widget)
{
	if (!widget) {
		return nullptr;
	}

	for (widget = widget->parentWidget(); widget; widget = widget->parentWidget()) {
		if (dynamic_cast<PLSDialogView *>(widget)) {
			return widget;
		}
	}
	return nullptr;
}

QString PLSLoginFunc::getPrismVersion()
{
	return QString("%1.%2.%3").arg(PLS_VERSION_MAJOR).arg(PLS_VERSION_MINOR).arg(PLS_VERSION_PATCH);
}

QString PLSLoginFunc::getPrismVersionWithBuild()
{
	return QString("%1.%2.%3.%4").arg(PLS_VERSION_MAJOR).arg(PLS_VERSION_MINOR).arg(PLS_VERSION_PATCH).arg(PLS_VERSION_BUILD);
}

QString PLSLoginFunc::makePath(const QString &resDir)
{
	QDir dir(resDir);
	bool isExist = dir.exists();
	if (!isExist) {
		dir.mkpath(resDir);
	}
	return dir.path();
}

void PLSLoginFunc::saveUpdateInfo(const QVariantMap &map)
{
	QDir updatesDir(PLSLoginFunc::getUserPath("updates"));
	QString currentLanguagePath = updatesDir.absoluteFilePath(QString("update.ini"));
	QSettings langSetting(currentLanguagePath, QSettings::IniFormat);
	langSetting.beginGroup(common::UPDATE_MESSAGE_INFO);
	for (QString key : map.keys()) {
		langSetting.setValue(key, map.value(key));
	}
	langSetting.endGroup();
	langSetting.sync();
}

QVariantMap PLSLoginFunc::getUpdateInfo(const QStringList &keys)
{
	QDir updatesDir(PLSLoginFunc::getUserPath("updates"));
	QString currentLanguagePath = updatesDir.absoluteFilePath(QString("update.ini"));
	QSettings langSetting(currentLanguagePath, QSettings::IniFormat);
	langSetting.beginGroup(common::UPDATE_MESSAGE_INFO);
	QVariantMap infoMap;
	for (const QString &key : keys) {
		infoMap.insert(key, langSetting.value(key));
	}
	langSetting.endGroup();
	return infoMap;
}

QString PLSLoginFunc::languageID2Locale(int languageID, const QString &defaultLanguage)
{
	const auto &locales = getLocaleName();
	for (const auto &key : locales.keys()) {
		if (key == languageID) {
			return locales.value(key).first;
		}
	}
	return defaultLanguage;
}

QMap<int, QPair<QString, QString>> PLSLoginFunc::getLocaleName()
{
	QMap<int, QPair<QString, QString>> _locale; //key LID ; first en-US second: english
	QDir appDir(QApplication::applicationDirPath());
	QString localePath = appDir.absoluteFilePath("data/prism-studio/locale.ini");
	QSettings s(localePath, QSettings::IniFormat);
	for (auto groupName : s.childGroups()) {
		QPair<QString, QString> pair;
		s.beginGroup(groupName);
		_locale.insert(s.value("LID").toInt(), QPair<QString, QString>(groupName, s.value("Name").toString()));
		s.endGroup();
	}
	return _locale;
}
QStringList PLSLoginFunc::getLocaleKey()
{
	auto locales = getLocaleName();
	QStringList localeKeys;
	for (const auto &key : locales.keys()) {
		localeKeys.append(locales.value(key).first);
	}
	return localeKeys;
}

PLSLoadingPage *PLSUIFunc::showLoadingView(QWidget *parent, const QString &tipStr)
{
	if (!parent)
		parent = pls_get_main_view();

	if (!parent)
		return nullptr;

	QPointer<QWidget> pWidgetLoadingBG = pls_new<QWidget>(parent);
	pWidgetLoadingBG->setObjectName("loadingBG");
	pWidgetLoadingBG->setGeometry(parent->geometry());
	pWidgetLoadingBG->show();

	PLSLoadingEvent *pLoadingEvent = pls_new<PLSLoadingEvent>();
	pLoadingEvent->setParent(pWidgetLoadingBG);

	auto layout = pls_new<QVBoxLayout>(pWidgetLoadingBG);
	auto loadingBtn = pls_new<QPushButton>(pWidgetLoadingBG);
	auto tips = pls_new<QLabel>(tipStr);
	tips->setWordWrap(true);
	tips->setAlignment(Qt::AlignCenter);
	tips->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	tips->setObjectName("loadingTipsLabel");
	layout->addStretch();
	layout->addWidget(loadingBtn, 0, Qt::AlignHCenter);
	layout->addWidget(tips);
	layout->addStretch();
	loadingBtn->setObjectName("loadingBtn");
	loadingBtn->show();

	pLoadingEvent->startLoadingTimer(loadingBtn);

	pWidgetLoadingBG->setStyleSheet("#loadingBG{background-color: rgba(39, 39, 39, 0.8);}"
					"#loadingTipsLabel{font-size: 14px; font-weight: normal; background-color: transparent; padding-left: 20px; padding-right: 20px;}"
					"#loadingBtn{background-color: transparent;}");

	parent->installEventFilter(new OBSEventFilter(parent, [parent, pWidgetLoadingBG](QObject *obj, QEvent *event) {
		if (obj == parent && event->type() == QEvent::Resize) {
			if (pWidgetLoadingBG)
				pWidgetLoadingBG->setGeometry(parent->geometry());
		}
		return false;
	}));

	return pWidgetLoadingBG;
}

void PLSUIFunc::showEnumTimeoutAlertView(const QString &deviceName)
{
	PLS_LOGEX(PLS_LOG_ERROR, MAINFRAME_MODULE, {{"enumTimeOut", deviceName.toUtf8().data()}}, "Enumerate device '%s' timeout.", qUtf8Printable(deviceName));
	PLSAlertView::warning(pls_get_main_view(), pls_translate_qstr("Alert.Title"), pls_translate_qstr("main.property.prism.enume.device.timeout").arg(deviceName));
}
