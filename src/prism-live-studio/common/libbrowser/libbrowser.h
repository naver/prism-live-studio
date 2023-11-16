#if !defined(_PRISM_COMMON_LIBBROWSER_LIBBROWSER_H)
#define _PRISM_COMMON_LIBBROWSER_LIBBROWSER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <qbytearray.h>
#include <qurl.h>
#include <qmap.h>
#include <qwidget.h>
#include <qdialog.h>
#include <qnetworkcookie.h>

#include "libutils-api.h"
#include "PLSWidgetCloseHook.h"

#if defined(Q_OS_WIN)
#ifdef LIBBROWSER_LIB
#define LIBBROWSER_API __declspec(dllexport)
#else
#define LIBBROWSER_API __declspec(dllimport)
#endif
#elif defined(Q_OS_MACOS)
#define LIBBROWSER_API
#endif

namespace pls {
namespace browser {

struct Browser;
struct Params;
struct Cookie;
class BrowserWidget;

struct ParamsImpl;
struct BrowserWidgetImpl;
struct BrowserDialogImpl;

using ParamsImplPtr = std::shared_ptr<ParamsImpl>;

using UrlChanged = std::function<void(Browser *browser, const QString &url, const QList<Cookie> &cookies)>;
using LoadEnded = std::function<void(Browser *browser)>;
using MsgReceived = std::function<void(Browser *browser, const QString &type, const QJsonObject &msg)>;
using Done = std::function<void(int result)>;
using BrowserDone = std::function<void(Browser *browser, int result)>;

template<typename... Args> using OkResult = std::function<void(Args...)>;
using FailResult = std::function<void()>;

struct Browser {
	virtual ~Browser() = default;

	virtual QString url() const = 0;
	virtual void url(const QString &url) = 0;

	virtual void done(int result) = 0;

	virtual void getAllCookie(QObject *receiver, const OkResult<const QList<Cookie> &> &result) const = 0;
	virtual void getCookie(const QString &url, bool isOnlyHttp, QObject *receiver, const OkResult<const QList<Cookie> &> &result) const = 0;
	virtual void setCookie(const QString &url, const Cookie &cookie) const = 0;
	virtual void deleteCookie(const QString &url, const QString &name) const = 0;
	virtual void flush() const = 0;

	virtual void send(const QString &type, const QJsonObject &msg) const = 0;

	virtual void closeBrowser() = 0;
	virtual void refreshBrowser() = 0;
};

struct LIBBROWSER_API Params {
	mutable ParamsImplPtr m_impl;

	Params();
	Params(const ParamsImplPtr &impl);

	Params copy() const;

	QString cookieStoragePath() const;
	const Params &cookieStoragePath(const QString &cookieStoragePath) const;

	QUrl url() const;
	const Params &url(const QString &url) const;
	const Params &url(const QUrl &url) const;
	const Params &hmacUrl(const QString &url, const QByteArray &hmacKey) const;
	const Params &hmacUrl(const QUrl &url, const QByteArray &hmacKey) const;

	QIcon icon() const;
	const Params &icon(const QIcon &icon) const;

	QString title() const;
	const Params &title(const QString &title) const;

	QWidget *parent() const;
	const Params &parent(QWidget *parent) const;

	QSize size() const;
	const Params &size(const QSize &size) const;

	QString script() const;
	const Params &script(const QString &script) const;

	QString css() const;
	const Params &css(const QString &css) const;

	QMap<QString, QString> headers() const;
	const Params &headers(const QMap<QString, QString> &headers) const;

	bool allowPopups() const;
	const Params &allowPopups(bool allowPopups) const;

	bool autoSetTitle() const;
	const Params &autoSetTitle(bool autoSetTitle) const;

	QColor initBkgColor() const;
	const Params &initBkgColor(const QColor &initBkgColor) const;

	bool showAtLoadEnded() const;
	const Params &showAtLoadEnded(bool showAtLoadEnded) const;

	const Params &urlChanged(const UrlChanged &urlChanged) const;
	const Params &loadEnded(const LoadEnded &loadEnded) const;
	const Params &msgReceived(const MsgReceived &msgReceived) const;
	const Params &browserDone(const BrowserDone &browserDone) const;
};

struct LIBBROWSER_API Cookie {
	QString name;
	QString value;
	QString domain;
	QString path;
	bool isOnlyHttp = false;

	Cookie() = default;
	explicit Cookie(const QJsonObject &jsonObject);

	QJsonObject toJsonObject() const;
};

struct BrowserAlias : public Browser {
	~BrowserAlias() override = default;

	void done(int result) override { doneAlias(result); }

	virtual void doneAlias(int result) = 0;
};

class LIBBROWSER_API BrowserWidget : public QWidget, public BrowserAlias {
	Q_OBJECT

protected:
	explicit BrowserWidget(QWidget *parent = nullptr);

signals:
	void titleChanged(const QString &title);
	void loadEnded();
	void msgRecevied(const QString &type, const QJsonObject &msg);
};

class LIBBROWSER_API BrowserDialog : public PLSWidgetCloseHookQt<QDialog>, public BrowserAlias {
	Q_OBJECT
	Q_DISABLE_COPY(BrowserDialog)

protected:
	explicit BrowserDialog(QWidget *parent = nullptr);

signals:
	void loadEnded();
	void msgRecevied(const QString &type, const QJsonObject &msg);
};

LIBBROWSER_API bool init(const QString &locale);

LIBBROWSER_API QJsonArray toJsonArray(const QList<Cookie> &cookies);
LIBBROWSER_API QList<Cookie> fromJsonArray(const QJsonArray &jsonArray);

LIBBROWSER_API QNetworkCookie toNetworkCookie(const Cookie &cookie);
LIBBROWSER_API QList<QNetworkCookie> toNetworkCookieList(const QList<Cookie> &cookies);

LIBBROWSER_API bool isValidBrowser(const Browser *browser);

LIBBROWSER_API BrowserWidget *newBrowserWidget(const Params &params, const Done &done = nullptr);
LIBBROWSER_API BrowserDialog *newBrowserDialog(const Params &params);

LIBBROWSER_API int openBrowser(const Params &params);

// notification
}
}

#endif // _PRISM_COMMON_LIBBROWSER_LIBBROWSER_H
