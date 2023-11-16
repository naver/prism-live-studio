#include "libbrowser.h"
#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <qcoreapplication.h>
#include <qdialog.h>
#include <qboxlayout.h>
#include <qjsondocument.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <quuid.h>
#include <qwindow.h>
#include <qevent.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qdebug.h>
#include <qpainter.h>
#include <qtimer.h>

#include <libipc.h>
#include <libhttp-client.h>
#include <liblog.h>

#include <browser-panel.hpp>
#include <util/profiler.h>
#include <obs.h>
#include <pls/pls-obs-api.h>
#include <pls/pls-base.h>

namespace pls {
namespace browser {

constexpr auto LIBBROWSER_MODULE = "libbrowser";

#define QCSTR_name QStringLiteral("name")
#define QCSTR_value QStringLiteral("value")
#define QCSTR_domain QStringLiteral("domain")
#define QCSTR_path QStringLiteral("path")
#define QCSTR_isOnlyHttp QStringLiteral("isOnlyHttp")

#define g_obs_startup Main::s_obs_startup
#define g_cef Main::s_cef
#define g_cookieManagers Main::s_cookieManagers

using CreateQCef = QCef *(*)();

QCefCookieManager *newCookieManager(const QString &cookieStoragePath = QString());
BrowserWidgetImpl *newBrowserWidgetImpl(const Params &params, const BrowserDone &browserDone);

struct Main {
	static bool s_obs_startup;
	static QCef *s_cef;
	static QMap<QString, QCefCookieManager *> s_cookieManagers;
};

class Initializer {
public:
	Initializer()
	{
		pls_qapp_construct_add_cb([this]() { this->init(); });
		pls_qapp_deconstruct_add_cb([this]() { this->cleanup(); });
	}

	static Initializer *initializer() { return &pls::Initializer<Initializer>::s_initializer; }

	void init() const {}
	void cleanup() const
	{
		if (g_obs_startup) {
			g_obs_startup = false;

			pls_delete(g_cef, nullptr);
			for (auto manager : g_cookieManagers) {
				pls_delete(manager);
			}
			PLS_INFO(LIBBROWSER_MODULE, "Start invoking obs_shutdown");
			obs_shutdown();
			PLS_INFO(LIBBROWSER_MODULE, "End invoking obs_shutdown");
		}
	}
};

bool Main::s_obs_startup = false;
QCef *Main::s_cef = nullptr;
QMap<QString, QCefCookieManager *> Main::s_cookieManagers;

struct ParamsImpl {
	QString m_cookieStoragePath;
	QUrl m_url;
	QIcon m_icon;
	QString m_title;
	QColor m_initBkgColor{Qt::white};
	std::optional<QWidget *> m_parent;
	std::optional<QSize> m_size;
	std::optional<QString> m_script;
	std::optional<QString> m_css;
	std::optional<QMap<QString, QString>> m_headers;
	std::optional<bool> m_allowPopups;
	std::optional<bool> m_autoSetTitle;
	std::optional<bool> m_showAtLoadEnded;
	std::optional<UrlChanged> m_urlChanged;
	std::optional<LoadEnded> m_loadEnded;
	std::optional<MsgReceived> m_msgReceived;
	std::optional<BrowserDone> m_browserDone;

	ParamsImplPtr copy() const { return std::make_shared<ParamsImpl>(*this); }

	QString script() const { return m_script.value_or(QString()); }
	QString css() const { return m_css.value_or(QString()); }
};

struct BrowserWidgetImpl : public BrowserWidget {
	Q_OBJECT

	friend struct BrowserDialogImpl;

public:
	ParamsImplPtr m_paramsImpl;
	BrowserDone m_browerDone;
	mutable QCefWidget *m_cefWidget = nullptr;
	mutable QCefCookieManager *m_cefCookieManager = nullptr;
	mutable std::optional<int> m_result;

	BrowserWidgetImpl(const ParamsImplPtr &paramsImpl, const BrowserDone &browerDone) : BrowserWidget(paramsImpl->m_parent.value_or(nullptr)), m_paramsImpl(paramsImpl), m_browerDone(browerDone)
	{
		setAttribute(Qt::WA_NativeWindow, true);
		if (g_cef) {
			m_cefCookieManager = newCookieManager(m_paramsImpl->m_cookieStoragePath);
			pls::map<std::string, std::string> headers;
			if (m_paramsImpl->m_headers.has_value()) {
				pls_for_each(m_paramsImpl->m_headers.value(), [&headers](const QString &key, const QString &value) {
					headers[key.toStdString()] = value.toStdString(); //
				});
			}

			if (m_cefWidget = g_cef->create_widget(this, BrowserWidgetImpl::url().toStdString(), m_paramsImpl->script().toStdString(), m_cefCookieManager, headers,
							       m_paramsImpl->m_allowPopups.value_or(true), m_paramsImpl->m_initBkgColor, m_paramsImpl->css().toStdString(),
							       m_paramsImpl->m_showAtLoadEnded.value_or(false));
			    m_cefWidget) {
				QVBoxLayout *layout = pls_new<QVBoxLayout>(this);
				layout->setContentsMargins(0, 0, 0, 0);
				layout->setSpacing(0);
				layout->addWidget(m_cefWidget, 1);
				connect(m_cefWidget, SIGNAL(titleChanged(QString)), this, SLOT(onCefWidgetTitleChanged(QString)));
				connect(m_cefWidget, SIGNAL(urlChanged(QString)), this, SLOT(onCefWidgetUrlChanged(QString)));
				connect(m_cefWidget, SIGNAL(loadEnded()), this, SLOT(onCefWidgetLoadEnded()));
				connect(m_cefWidget, SIGNAL(msgRecevied(QString, QString)), this, SLOT(onCefWidgetMsgRecevied(QString, QString)));
				connect(m_cefWidget, SIGNAL(browserClosed()), this, SLOT(onCefWidgetClosed()));
			}
		}
	}

	QString url() const override { return m_paramsImpl->m_url.toString(QUrl::FullyEncoded); }
	void url(const QString &url) override
	{
		m_paramsImpl->m_url.setUrl(url);
		if (pls_object_is_valid(m_cefWidget)) {
			m_cefWidget->setURL(url.toStdString());
		}
	}

	void doneAlias(int result) override
	{
		m_result = result;
		closeBrowser();
	}

	void getAllCookie(QObject *receiver, const OkResult<const QList<Cookie> &> &result) const override
	{
		PLS_DEBUG(LIBBROWSER_MODULE, "get all browser cookies");
		m_cefCookieManager->ReadAllCookies([receiver, result](const std::list<QCefCookieManager::Cookie> &cefCookies) {
			PLS_INFO(LIBBROWSER_MODULE, "complete read browser cookies");
			QList<Cookie> cookies;
			std::for_each(cefCookies.begin(), cefCookies.end(), [&cookies](const QCefCookieManager::Cookie &c) {
				Cookie cookie;
				cookie.name = QString::fromStdString(c.name);
				cookie.value = QString::fromStdString(c.value);
				cookie.domain = QString::fromStdString(c.domain);
				cookie.path = QString::fromStdString(c.path);
				cookie.isOnlyHttp = c.isOnlyHttp;
				cookies.append(cookie);
			});

			pls_async_call_mt(receiver, [result, cookies]() {
				pls_invoke_safe(result, cookies); //
			});
		});
	}
	void getCookie(const QString &url, bool isOnlyHttp, QObject *receiver, const OkResult<const QList<Cookie> &> &result) const override
	{
		PLS_DEBUG(LIBBROWSER_MODULE, "get browser cookies");
		m_cefCookieManager->ReadCookies(
			url.toStdString(),
			[receiver, result](const std::list<QCefCookieManager::Cookie> &cefCookies) {
				PLS_INFO(LIBBROWSER_MODULE, "complete read browser cookies");
				QList<Cookie> cookies;
				std::for_each(cefCookies.begin(), cefCookies.end(), [&cookies](const QCefCookieManager::Cookie &c) {
					Cookie cookie;
					cookie.name = QString::fromStdString(c.name);
					cookie.value = QString::fromStdString(c.value);
					cookie.domain = QString::fromStdString(c.domain);
					cookie.path = QString::fromStdString(c.path);
					cookie.isOnlyHttp = c.isOnlyHttp;
					cookies.append(cookie);
				});

				pls_async_call_mt(receiver, [result, cookies]() {
					pls_invoke_safe(result, cookies); //
				});
			},
			false);
	}
	void setCookie(const QString &url, const Cookie &cookie) const override
	{
		m_cefCookieManager->SetCookie(url.toStdString(), cookie.name.toStdString(), cookie.value.toStdString(), cookie.domain.toStdString(), cookie.path.toStdString(), cookie.isOnlyHttp);
	}
	void deleteCookie(const QString &url, const QString &name) const override { m_cefCookieManager->DeleteCookies(url.toStdString(), name.toStdString()); }
	void flush() const override { m_cefCookieManager->FlushStore(); }

	void send(const QString &type, const QJsonObject &msg) const override
	{
		if (pls_object_is_valid(m_cefWidget)) {
			m_cefWidget->sendMsg(type.toStdWString(), pls_to_string(msg).toStdWString());
		}
	}

	void closeBrowser() override
	{
		if (pls_object_is_valid(m_cefWidget)) {
			m_cefWidget->closeBrowser();
		} else if (m_result.has_value()) {
			pls_invoke_safe(m_browerDone, this, m_result.value());
		}
	}
	void refreshBrowser() override
	{
		if (pls_object_is_valid(m_cefWidget)) {
			m_cefWidget->reloadPage();
		}
	}

private slots:
	void onCefWidgetTitleChanged(const QString &title)
	{
		titleChanged(title);
		if (m_paramsImpl->m_autoSetTitle.value_or(true)) {
			setWindowTitle(title);
		}
	}
	void onCefWidgetUrlChanged(const QString &url)
	{
		PLS_INFO(LIBBROWSER_MODULE, "begin read browser cookies");
		m_cefCookieManager->ReadAllCookies([url, this](const std::list<QCefCookieManager::Cookie> &cefCookies) {
			PLS_INFO(LIBBROWSER_MODULE, "complete read browser cookies");
			QList<Cookie> cookies;
			std::for_each(cefCookies.begin(), cefCookies.end(), [&cookies](const QCefCookieManager::Cookie &c) {
				Cookie cookie;
				cookie.name = QString::fromStdString(c.name);
				cookie.value = QString::fromStdString(c.value);
				cookie.domain = QString::fromStdString(c.domain);
				cookie.path = QString::fromStdString(c.path);
				cookie.isOnlyHttp = c.isOnlyHttp;
				cookies.append(cookie);
			});

			pls_async_call_mt(this, [this, url, cookies]() {
				pls_invoke_safe(m_paramsImpl->m_urlChanged, this, url, cookies); //
			});
		});
	}
	void onCefWidgetLoadEnded()
	{
		emit loadEnded();
		pls_invoke_safe(m_paramsImpl->m_loadEnded, this);
	}
	void onCefWidgetMsgRecevied(const QString &type, const QString &msg)
	{
		QJsonObject obj = pls_to_json_object(msg);

		emit msgRecevied(type, obj);
		pls_invoke_safe(m_paramsImpl->m_msgReceived, this, type, obj);
	}
	void onCefWidgetClosed()
	{
		if (m_result.has_value())
			pls_invoke_safe(m_browerDone, this, m_result.value());
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.fillRect(rect(), m_paramsImpl->m_initBkgColor);
	}
};

struct BrowserDialogImpl : public BrowserDialog {
	Q_OBJECT
	Q_DISABLE_COPY(BrowserDialogImpl)

public:
	explicit BrowserDialogImpl(const Params &params) : BrowserDialog(params.parent())
	{
		if (!params.m_impl->m_allowPopups.has_value()) {
			params.allowPopups(true);
		}

		setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
		setWindowIcon(params.icon());
		setWindowTitle(params.title());

		QVBoxLayout *layout = pls_new<QVBoxLayout>(this);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);

		m_browserWidget = newBrowserWidgetImpl(params.parent(this), [this, paramsImpl = params.m_impl](Browser *browser, int result) {
			m_done = true;
			pls_invoke_safe(paramsImpl->m_browserDone, browser, result);

			if (!m_closing) {
				QDialog::done(result);
			}
		});
		layout->addWidget(m_browserWidget, 1);
		connect(m_browserWidget, &BrowserWidget::titleChanged, this, [this, params](const QString &title) {
			if (params.autoSetTitle()) {
				setWindowTitle(title);
			}
		});
		connect(m_browserWidget, &BrowserWidget::loadEnded, this, [this]() {
			emit loadEnded(); //
		});
		connect(m_browserWidget, &BrowserWidget::msgRecevied, this, [this](const QString &type, const QJsonObject &msg) {
			emit msgRecevied(type, msg); //
		});
		resize(params.size());
	}

	QString url() const override { return m_browserWidget->url(); }
	void url(const QString &url) override { m_browserWidget->url(url); }

	void doneAlias(int result) override { m_browserWidget->done(result); }

	void getAllCookie(QObject *receiver, const OkResult<const QList<Cookie> &> &result) const override { m_browserWidget->getAllCookie(receiver, result); }
	void getCookie(const QString &url, bool isOnlyHttp, QObject *receiver, const OkResult<const QList<Cookie> &> &result) const override
	{
		m_browserWidget->getCookie(url, isOnlyHttp, receiver, result);
	}
	void setCookie(const QString &url, const Cookie &cookie) const override { m_browserWidget->setCookie(url, cookie); }
	void deleteCookie(const QString &url, const QString &name) const override { m_browserWidget->deleteCookie(url, name); }
	void flush() const override { m_browserWidget->flush(); }

	void send(const QString &type, const QJsonObject &msg) const override { m_browserWidget->send(type, msg); }

	void closeEvent(QCloseEvent *event) override
	{
		m_closing = true;

		if (!m_done)
			doneAlias(QDialog::Rejected);
		BrowserDialog::closeEvent(event);
	}
	void closeBrowser() override
	{
		if (pls_object_is_valid(m_browserWidget)) {
			m_browserWidget->closeBrowser();
		}
	}
	void refreshBrowser() override
	{
		if (pls_object_is_valid(m_browserWidget)) {
			m_browserWidget->refreshBrowser();
		}
	}
	BrowserWidgetImpl *m_browserWidget = nullptr;
	bool m_done = false;
	bool m_closing = false;
};

QCefCookieManager *newCookieManager(const QString &cookieStoragePath)
{
	QString storagePath;
	if (cookieStoragePath.isEmpty()) {
		storagePath = QStringLiteral("libbrowser/defaults");
	} else if (pls_is_abs_path(cookieStoragePath)) {
		storagePath = cookieStoragePath;
	} else {
		storagePath = QStringLiteral("libbrowser/../") + cookieStoragePath;
	}

	if (auto iter = g_cookieManagers.find(storagePath); iter != g_cookieManagers.end()) {
		return iter.value();
	}

	if (!g_cef) {
		return nullptr;
	}

	QCefCookieManager *cookieManager = g_cef->create_cookie_manager(storagePath.toStdString());
	g_cookieManagers.insert(storagePath, cookieManager);
	return cookieManager;
}
BrowserWidgetImpl *newBrowserWidgetImpl(const Params &params, const BrowserDone &browserDone)
{
	return pls_new<BrowserWidgetImpl>(params.m_impl, browserDone);
}

Params::Params()
{
	m_impl = std::make_shared<ParamsImpl>();
	this->size({800, 600});
}

Params::Params(const ParamsImplPtr &impl) : m_impl(impl) {}

Params Params::copy() const
{
	return m_impl->copy();
}

QString Params::cookieStoragePath() const
{
	return m_impl->m_cookieStoragePath;
}
const Params &Params::cookieStoragePath(const QString &cookieStoragePath) const
{
	m_impl->m_cookieStoragePath = cookieStoragePath;
	return *this;
}

QUrl Params::url() const
{
	return m_impl->m_url;
}
const Params &Params::url(const QString &url) const
{
	m_impl->m_url.setUrl(url);
	return *this;
}
const Params &Params::url(const QUrl &url) const
{
	m_impl->m_url = url;
	return *this;
}
const Params &Params::hmacUrl(const QString &url, const QByteArray &hmacKey) const
{
	m_impl->m_url = http::buildHmacUrl(url, hmacKey);
	return *this;
}
const Params &Params::hmacUrl(const QUrl &url, const QByteArray &hmacKey) const
{
	m_impl->m_url = http::buildHmacUrl(url, hmacKey);
	return *this;
}

QIcon Params::icon() const
{
	return m_impl->m_icon;
}
const Params &Params::icon(const QIcon &icon) const
{
	m_impl->m_icon = icon;
	return *this;
}
QString Params::title() const
{
	return m_impl->m_title;
}
const Params &Params::title(const QString &title) const
{
	m_impl->m_title = title;
	return *this;
}

QWidget *Params::parent() const
{
	return m_impl->m_parent.value_or(nullptr);
}
const Params &Params::parent(QWidget *parent) const
{
	if (parent) {
		m_impl->m_parent = parent;
	} else {
		m_impl->m_parent = std::nullopt;
	}
	return *this;
}

QSize Params::size() const
{
	return m_impl->m_size.value();
}
const Params &Params::size(const QSize &size) const
{
	m_impl->m_size = size;
	return *this;
}

QString Params::script() const
{
	return m_impl->script();
}
const Params &Params::script(const QString &script) const
{
	if (!script.isEmpty()) {
		m_impl->m_script = script;
	} else {
		m_impl->m_script = std::nullopt;
	}
	return *this;
}

QString Params::css() const
{
	return m_impl->css();
}
const Params &Params::css(const QString &css) const
{
	if (!css.isEmpty()) {
		m_impl->m_css = css;
	} else {
		m_impl->m_css = std::nullopt;
	}
	return *this;
}

QMap<QString, QString> Params::headers() const
{
	if (m_impl->m_headers.has_value()) {
		return m_impl->m_headers.value();
	}
	return {};
}
const Params &Params::headers(const QMap<QString, QString> &headers) const
{
	if (!headers.isEmpty()) {
		m_impl->m_headers = headers;
	} else {
		m_impl->m_headers = std::nullopt;
	}
	return *this;
}

bool Params::allowPopups() const
{
	return m_impl->m_allowPopups.value_or(true);
}
const Params &Params::allowPopups(bool allowPopups) const
{
	m_impl->m_allowPopups = allowPopups;
	return *this;
}

bool Params::autoSetTitle() const
{
	return m_impl->m_autoSetTitle.value_or(true);
}
const Params &Params::autoSetTitle(bool autoSetTitle) const
{
	m_impl->m_autoSetTitle = autoSetTitle;
	return *this;
}

QColor Params::initBkgColor() const
{
	return m_impl->m_initBkgColor;
}
const Params &Params::initBkgColor(const QColor &initBkgColor) const
{
	m_impl->m_initBkgColor = initBkgColor;
	return *this;
}

bool Params::showAtLoadEnded() const
{
	return m_impl->m_showAtLoadEnded.value_or(false);
}
const Params &Params::showAtLoadEnded(bool showAtLoadEnded) const
{
	if (showAtLoadEnded) {
		m_impl->m_showAtLoadEnded = showAtLoadEnded;
	} else {
		m_impl->m_showAtLoadEnded = std::nullopt;
	}
	return *this;
}

const Params &Params::urlChanged(const UrlChanged &urlChanged) const
{
	if (urlChanged) {
		m_impl->m_urlChanged = urlChanged;
	} else {
		m_impl->m_urlChanged = std::nullopt;
	}
	return *this;
}

const Params &Params::loadEnded(const LoadEnded &loadEnded) const
{
	if (loadEnded) {
		m_impl->m_loadEnded = loadEnded;
	} else {
		m_impl->m_loadEnded = std::nullopt;
	}
	return *this;
}

const Params &Params::msgReceived(const MsgReceived &msgReceived) const
{
	if (msgReceived) {
		m_impl->m_msgReceived = msgReceived;
	} else {
		m_impl->m_msgReceived = std::nullopt;
	}
	return *this;
}

const Params &Params::browserDone(const BrowserDone &browserDone) const
{
	if (browserDone) {
		m_impl->m_browserDone = browserDone;
	} else {
		m_impl->m_browserDone = std::nullopt;
	}
	return *this;
}

Cookie::Cookie(const QJsonObject &jsonObject)
	: name(jsonObject[QCSTR_name].toString()),
	  value(jsonObject[QCSTR_value].toString()),
	  domain(jsonObject[QCSTR_domain].toString()),
	  path(jsonObject[QCSTR_path].toString()),
	  isOnlyHttp(jsonObject[QCSTR_isOnlyHttp].toBool())
{
}

QJsonObject Cookie::toJsonObject() const
{
	return {{QCSTR_name, name}, {QCSTR_value, value}, {QCSTR_domain, domain}, {QCSTR_path, path}, {QCSTR_isOnlyHttp, isOnlyHttp}};
}

BrowserWidget::BrowserWidget(QWidget *parent) : QWidget(parent)
{
	setAttribute(Qt::WA_NativeWindow, true);
}

BrowserDialog::BrowserDialog(QWidget *parent) : PLSWidgetCloseHookQt<QDialog>(parent)
{
	setAttribute(Qt::WA_NativeWindow, true);
}

static void defObsLogHandler(bool kr, int log_level, const char *format, va_list args, const char *fields[][2], int field_count, void *)
{
	std::vector<std::pair<const char *, const char *>> tmp_fields;
	for (int i = 0; i < field_count; ++i) {
		tmp_fields.push_back({fields[i][0], fields[i][1]});
	}

	int arg_count = -1;
	switch (log_level) {
	case LOG_ERROR:
		pls_logvaex(kr, PLS_LOG_ERROR, "obs", nullptr, 0, tmp_fields, arg_count, format, args);
		break;
	case LOG_WARNING:
		pls_logvaex(kr, PLS_LOG_WARN, "obs", nullptr, 0, tmp_fields, arg_count, format, args);
		break;
	case LOG_INFO:
		pls_logvaex(kr, PLS_LOG_INFO, "obs", nullptr, 0, tmp_fields, arg_count, format, args);
		break;
	case LOG_DEBUG:
		pls_logvaex(kr, PLS_LOG_DEBUG, "obs", nullptr, 0, tmp_fields, arg_count, format, args);
		break;
	default:
		break;
	}
}
static void defObsLogHandler(int log_level, const char *format, va_list args, void *param)
{
	defObsLogHandler(false, log_level, format, args, nullptr, 0, param);
}
static void obsEnumModuleCallback(void *param, obs_module_t *mod)
{
	if (auto fn = obs_get_module_file_name(mod); fn && strstr(fn, "obs-browser")) {
		*(obs_module_t **)param = mod;
	}
}
static bool findObsBrowser(CreateQCef &createCef)
{
	obs_module_t *mod = nullptr;
	obs_enum_modules(obsEnumModuleCallback, &mod);
	if (mod) {
		createCef = (CreateQCef)os_dlsym(obs_get_module_lib(mod), "obs_browser_create_qcef");
		return true;
	}
	return false;
}
static bool loadObsBrwoser(CreateQCef &create_qcef, const QString &locale)
{
#if defined(Q_OS_WIN)
	QString binDir = pls_get_dll_dir("libbrowser");
	qputenv("PATH", qEnvironmentVariable("PATH").toLocal8Bit() + ";" + binDir.toLocal8Bit() + ";" + binDir.toLocal8Bit() + "\\..\\..\\obs-plugins");
#endif

	base_set_log_handler(defObsLogHandler, nullptr);
	base_set_log_handler_ex(defObsLogHandler, nullptr);

	QString pluginConfigDir = pls_get_app_data_dir("PRISMLiveStudio/plugin_config");
	if (!obs_startup(locale.toUtf8().constData(), pluginConfigDir.toUtf8().constData(), nullptr)) {
		return false;
	}

	g_obs_startup = true;

#if defined(Q_OS_WIN)
	obs_add_module_path("../../prism-plugins/", "../../data/prism-plugins/%module%");
#elif defined(Q_OS_MACOS)
	QString pluginDir = pls_get_app_plugin_dir();
	obs_add_module_path((pluginDir + "/%module%.plugin/Contents/MacOS").toUtf8().constData(), (pluginDir + "/%module%.plugin/Contents/Resources").toUtf8().constData());
#endif
	pls_load_all_modules([](const char *binPath) {
#if defined(Q_OS_WIN)
		return _stricmp(binPath, "../../obs-plugins/64bit/obs-browser.dll") == 0;
#elif defined(Q_OS_MACOS)
		return strstr(binPath, "obs-browser.plugin") != nullptr;
#endif
	});

	if (!findObsBrowser(create_qcef)) {
		PLS_ERROR(LIBBROWSER_MODULE, "load obs-browser library failed");
		return false;
	}
	return true;
}

LIBBROWSER_API bool init(const QString &locale)
{
	CreateQCef create_qcef = nullptr;
	if (!findObsBrowser(create_qcef) && !loadObsBrwoser(create_qcef, locale)) {
		return false;
	}

	if (!create_qcef) {
		PLS_ERROR(LIBBROWSER_MODULE, "init obs-browser failed, error: obs_browser_create_qcef not found");
		return false;
	}

	g_cef = create_qcef();
	if (!g_cef) {
		PLS_ERROR(LIBBROWSER_MODULE, "init obs-browser failed, error: create QCef failed");
		return false;
	}

	g_cef->init_browser();
	g_cef->wait_for_browser_init();
	if (auto cookieManager = newCookieManager(); !cookieManager) {
		return false;
	}
	return true;
}

LIBBROWSER_API QJsonArray toJsonArray(const QList<Cookie> &cookies)
{
	QJsonArray jsonArray;
	for (const auto &cookie : cookies) {
		jsonArray.append(cookie.toJsonObject());
	}
	return jsonArray;
}
LIBBROWSER_API QList<Cookie> fromJsonArray(const QJsonArray &jsonArray)
{
	QList<Cookie> cookies;
	for (QJsonValue val : jsonArray) {
		cookies.append(Cookie(val.toObject()));
	}
	return cookies;
}

LIBBROWSER_API QNetworkCookie toNetworkCookie(const Cookie &cookie)
{
	QNetworkCookie ncookie;
	ncookie.setName(cookie.name.toUtf8());
	ncookie.setValue(cookie.value.toUtf8());
	ncookie.setDomain(cookie.domain.toUtf8());
	ncookie.setPath(cookie.path.toUtf8());
	if (cookie.isOnlyHttp)
		ncookie.setHttpOnly(true);
	else
		ncookie.setSecure(true);
	return ncookie;
}
LIBBROWSER_API QList<QNetworkCookie> toNetworkCookieList(const QList<Cookie> &cookies)
{
	QList<QNetworkCookie> ncookies;
	for (const auto &cookie : cookies)
		ncookies.append(toNetworkCookie(cookie));
	return ncookies;
}

LIBBROWSER_API bool isValidBrowser(const Browser *browser)
{
	if (pls_object_is_valid(static_cast<const BrowserWidgetImpl *>(browser)) //
	    || pls_object_is_valid(static_cast<const BrowserDialogImpl *>(browser))) {
		return true;
	}
	return false;
}

LIBBROWSER_API BrowserWidget *newBrowserWidget(const Params &params, const Done &done)
{
	if (g_cef) {
		return newBrowserWidgetImpl(params.m_impl, [paramsImpl = params.m_impl, done](Browser *browser, int result) {
			pls_invoke_safe(paramsImpl->m_browserDone, browser, result);
			pls_invoke_safe(done, result);
		});
	}
	return nullptr;
}
LIBBROWSER_API BrowserDialog *newBrowserDialog(const Params &params)
{
	if (g_cef) {
		return pls_new<BrowserDialogImpl>(params);
	}
	return nullptr;
}
LIBBROWSER_API int openBrowser(const Params &params)
{
	if (auto *dlg = newBrowserDialog(params); dlg) {
		dlg->setAttribute(Qt::WA_DeleteOnClose, true);
		return dlg->exec();
	}
	return -1;
}

}
}

#include "libbrowser.moc"
