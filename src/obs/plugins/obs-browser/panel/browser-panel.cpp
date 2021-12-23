#include "browser-panel-internal.hpp"
#include "browser-panel-client.hpp"
#include "cef-headers.hpp"
#include "browser-app.hpp"

#include <QWindow>
#include <QApplication>
#include <QVBoxLayout>
#include <QThread>

#ifdef USE_QT_LOOP
#include <QEventLoop>
#include <QThread>
#endif

#include <obs-module.h>
#include <util/threading.h>
#include <util/base.h>
#include <thread>
#include <algorithm>
#include <qevent.h>
#include <set>

extern bool QueueCEFTask(std::function<void()> task);
extern "C" void obs_browser_initialize(void);
extern os_event_t *cef_started_event;

std::mutex popup_whitelist_mutex;
std::vector<PopupWhitelistInfo> popup_whitelist;
std::vector<PopupWhitelistInfo> forced_popups;

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
static std::recursive_mutex cef_widgets_mutex;
static std::set<QCefWidgetInner *> cef_widgets;

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
bool cef_widgets_contains(QCefWidgetInner *inner)
{
	if (!inner) {
		return false;
	}

	std::lock_guard<std::recursive_mutex> lock(cef_widgets_mutex);
	if (cef_widgets.find(inner) != cef_widgets.end()) {
		return true;
	}
	return false;
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void cef_widgets_sync_call(QCefWidgetInner *inner,
			   std::function<void(QCefWidgetInner *)> call)
{
	if (!inner) {
		return;
	}

	std::lock_guard<std::recursive_mutex> lock(cef_widgets_mutex);
	if (cef_widgets.find(inner) != cef_widgets.end()) {
		call(inner);
	}
}

//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
static void cef_widgets_add(QCefWidgetInner *inner, bool locked)
{
	if (!locked) {
		std::lock_guard<std::recursive_mutex> lock(cef_widgets_mutex);
		cef_widgets.insert(inner);
	} else {
		cef_widgets.insert(inner);
	}
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
static void cef_widgets_remove(QCefWidgetInner *inner)
{
	std::lock_guard<std::recursive_mutex> lock(cef_widgets_mutex);
	cef_widgets.erase(inner);

	//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
	inner->popups.clear();
	inner->popup = nullptr;
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
template<typename Callback> static void cef_widgets_foreach(Callback callback)
{
	std::lock_guard<std::recursive_mutex> lock(cef_widgets_mutex);
	for (QCefWidgetInner *inner : cef_widgets) {
		callback(inner);
	}
}

/* ------------------------------------------------------------------------- */

#if CHROME_VERSION_BUILD < 3770
CefRefPtr<CefCookieManager> QCefRequestContextHandler::GetCookieManager()
{
	return cm;
}
#endif

class CookieCheck : public CefCookieVisitor {
public:
	QCefCookieManager::cookie_exists_cb callback;
	std::string target;
	bool cookie_found = false;

	inline CookieCheck(QCefCookieManager::cookie_exists_cb callback_,
			   const std::string target_)
		: callback(callback_), target(target_)
	{
	}

	virtual ~CookieCheck() { callback(cookie_found); }

	virtual bool Visit(const CefCookie &cookie, int, int, bool &) override
	{
		CefString cef_name = cookie.name.str;
		std::string name = cef_name;

		if (name == target) {
			cookie_found = true;
			return false;
		}
		return true;
	}

	IMPLEMENT_REFCOUNTING(CookieCheck);
};

class CookieRead : public CefCookieVisitor {
public:
	QCefCookieManager::read_cookie_cb cookie_cb;
	void *context;
	bool has_cookie;

	inline CookieRead(QCefCookieManager::read_cookie_cb cookie_cb_,
			  void *context_)
		: cookie_cb(cookie_cb_), context(context_), has_cookie(false)
	{
	}

	virtual ~CookieRead()
	{
		if (!has_cookie) {
			cookie_cb(nullptr, nullptr, nullptr, nullptr, true,
				  context);
		}
	}

	virtual bool Visit(const CefCookie &cookie, int index, int total,
			   bool &) override
	{
		has_cookie = true;
		cookie_cb(CefString(cookie.name.str).ToString().c_str(),
			  CefString(cookie.value.str).ToString().c_str(),
			  CefString(cookie.domain.str).ToString().c_str(),
			  CefString(cookie.path.str).ToString().c_str(),
			  (index + 1) == total, context);
		return true;
	}

	IMPLEMENT_REFCOUNTING(CookieRead);
};

struct QCefCookieManagerInternal : QCefCookieManager {
	CefRefPtr<CefCookieManager> cm;
#if CHROME_VERSION_BUILD < 3770
	CefRefPtr<CefRequestContextHandler> rch;
#endif
	CefRefPtr<CefRequestContext> rc;

	QCefCookieManagerInternal(const std::string &storage_path,
				  bool persist_session_cookies)
	{
		if (os_event_try(cef_started_event) != 0)
			throw "Browser thread not initialized";

		BPtr<char> rpath = obs_module_config_path(storage_path.c_str());
		BPtr<char> path = os_get_abs_path_ptr(rpath.Get());

#if CHROME_VERSION_BUILD < 3770
		cm = CefCookieManager::CreateManager(
			path.Get(), persist_session_cookies, nullptr);
		if (!cm)
			throw "Failed to create cookie manager";
#endif

#if CHROME_VERSION_BUILD < 3770
		rch = new QCefRequestContextHandler(cm);

		rc = CefRequestContext::CreateContext(
			CefRequestContext::GetGlobalContext(), rch);
#else
		CefRequestContextSettings settings;
		CefString(&settings.cache_path) = path.Get();
		rc = CefRequestContext::CreateContext(
			settings, CefRefPtr<CefRequestContextHandler>());
		if (rc)
			cm = rc->GetCookieManager(nullptr);

		UNUSED_PARAMETER(persist_session_cookies);
#endif
	}

	virtual bool DeleteCookies(const std::string &url,
				   const std::string &name) override
	{
		return !!cm ? cm->DeleteCookies(url, name, nullptr) : false;
	}

	virtual bool SetStoragePath(const std::string &storage_path,
				    bool persist_session_cookies) override
	{
		BPtr<char> rpath = obs_module_config_path(storage_path.c_str());
		BPtr<char> path = os_get_abs_path_ptr(rpath.Get());

#if CHROME_VERSION_BUILD < 3770
		return cm->SetStoragePath(path.Get(), persist_session_cookies,
					  nullptr);
#else
		CefRequestContextSettings settings;
		CefString(&settings.cache_path) = storage_path;
		rc = CefRequestContext::CreateContext(
			settings, CefRefPtr<CefRequestContextHandler>());
		if (rc)
			cm = rc->GetCookieManager(nullptr);

		UNUSED_PARAMETER(persist_session_cookies);
		return true;
#endif
	}

	virtual bool FlushStore() override
	{
		return !!cm ? cm->FlushStore(nullptr) : false;
	}

	virtual void CheckForCookie(const std::string &site,
				    const std::string &cookie,
				    cookie_exists_cb callback) override
	{
		if (!cm)
			return;

		CefRefPtr<CookieCheck> c = new CookieCheck(callback, cookie);
		cm->VisitUrlCookies(site, true, c);
	}

	virtual void ReadCookies(const std::string &site,
				 read_cookie_cb cookie_cb,
				 void *context) override
	{
		if (!cm)
			return;

		CefRefPtr<CookieRead> c = new CookieRead(cookie_cb, context);
		cm->VisitUrlCookies(site, true, c);
	}
	// OBS Modification:
	// Cheng Bing / 20200513 /
	// Reason: naverTv chat need cookie information to login;cef have not save the cookies;
	// Solution: app init Manual set naverTv cookies into cef
	virtual bool SetCookie(const std::string &url, const std::string &name,
			       const std::string &value,
			       const std::string &domain,
			       const std::string &path, bool isOnlyHttp)
	{
		if (!cm)
			return false;

		CefCookie cookie;
		cef_string_utf8_to_utf16(name.c_str(), name.size(),
					 &cookie.name);
		cef_string_utf8_to_utf16(value.c_str(), value.size(),
					 &cookie.value);
		cef_string_utf8_to_utf16(domain.c_str(), domain.size(),
					 &cookie.domain);
		cef_string_utf8_to_utf16(path.c_str(), path.size(),
					 &cookie.path);

		if (isOnlyHttp) {
			cookie.httponly = true;
		} else {
			cookie.secure = true;
		}

		return cm->SetCookie(url, cookie, nullptr);
	}
	// OBS Modification:
	// Cheng Bing / 20200518/
	// Reason: get cef all cookies infomation

	virtual void visitAllCookies(read_cookie_cb cookie_cb, void *context)
	{
		if (!cm)
			return;
		CefRefPtr<CookieRead> c = new CookieRead(cookie_cb, context);
		cm->VisitAllCookies(c);
	}
};

/* ------------------------------------------------------------------------- */

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
QCefWidgetInner::QCefWidgetInner(
	QWidget *parent, const std::string &url_, const std::string &script_,
	Rqc rqc_, const Headers &headers_, bool allowPopups_,
	//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
	QPLSBrowserPopupDialog *popup_)
	: QFrame(parent),
	  browser(),
	  url(url_),
	  script(script_),
	  rqc(rqc_),
	  headers(headers_),
	  allowPopups(allowPopups_),
	  popups(),
	  //PRISM/Zhangdewen/20210330/#/optimization, maybe crash
	  popup(popup_)
{
	setAttribute(Qt::WA_NativeWindow);
	setFocusPolicy(Qt::ClickFocus);
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
QCefWidgetInner::~QCefWidgetInner() {}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetInner::init()
{
	executeOnCef([this, size = this->size(), id = winId()]() {
		if (!cef_widgets_contains(this) || browser) {
			return;
		}

		CefWindowInfo windowInfo;
#ifdef _WIN32
		RECT rc = {0, 0, size.width(), size.height()};
		windowInfo.SetAsChild((HWND)id, rc);
#elif __APPLE__
		windowInfo.SetAsChild((CefWindowHandle)id, 0, 0, size.width(),
				      size.height());
#endif

		CefRefPtr<QCefBrowserClient> browserClient =
			new QCefBrowserClient(this);

		CefBrowserSettings cefBrowserSettings;
		cefBrowserSettings.web_security = STATE_DISABLED;
		browser = CefBrowserHost::CreateBrowserSync(
			windowInfo, browserClient, url, cefBrowserSettings,
#if CHROME_VERSION_BUILD >= 3770
			CefRefPtr<CefDictionaryValue>(),
#endif
			rqc);
#ifdef _WIN32
		resize(true);
		//PRISM/Zhangdewen/20210608/#/auto restart cef, browser don's show sometimes
		resize(false);
#endif
	});
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
//PRISM/Zhangdewen/20210601/#/optimization, exception restart
void QCefWidgetInner::destroy(bool restart)
{
	if (!restart) {
		hide();
		setParent(nullptr);

		CefRefPtr<CefBrowser> browser = this->browser;
		if (browser) {
			this->browser = nullptr;

#ifdef _WIN32
			/* So you're probably wondering what's going on here.  If you
		 * call CefBrowserHost::CloseBrowser, and it fails to unload
		 * the web page *before* WM_NCDESTROY is called on the browser
		 * HWND, it will call an internal CEF function
		 * CefBrowserPlatformDelegateNativeWin::CloseHostWindow, which
		 * will attempt to close the browser's main window itself.
		 * Problem is, this closes the root window containing the
		 * browser's HWND rather than the browser's specific HWND for
		 * whatever mysterious reason.  If the browser is in a dock
		 * widget, then the window it closes is, unfortunately, the
		 * main program's window, causing the entire program to shut
		 * down.
		 *
		 * So, instead, before closing the browser, we need to decouple
		 * the browser from the widget.  To do this, we hide it, then
		 * remove its parent. */
			HWND hwnd = (HWND)browser->GetHost()->GetWindowHandle();
			if (hwnd) {
				ShowWindow(hwnd, SW_HIDE);
				SetParent(hwnd, nullptr);
			}
#endif
		}

		executeOnCef([browser, this]() {
			if (browser) {
				//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
				browser->GetHost()->WasHidden(true);
				browser->GetHost()->CloseBrowser(true);
			}

			executeOnUI([this]() { delete this; });
		});
	} else {
		//PRISM/Zhangdewen/20210601/#/optimization, exception restart
		CefRefPtr<CefBrowser> browser = this->browser;
		if (browser) {
			this->browser = nullptr;

			HWND hwnd = (HWND)browser->GetHost()->GetWindowHandle();
			if (hwnd) {
				ShowWindow(hwnd, SW_HIDE);
				SetParent(hwnd, nullptr);
			}
		}

		executeOnCef([browser, this]() {
			if (browser) {
				//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
				browser->GetHost()->WasHidden(true);
				browser->GetHost()->CloseBrowser(true);
			}

			//PRISM/Zhangdewen/20210802/#9043/cross thread call widget's winId() maybe cause crash
			executeOnUI([this]() { init(); });
		});
	}
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetInner::resizeEvent(QResizeEvent *event)
{
	QFrame::resizeEvent(event);
	resize(false);
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetInner::closeEvent(QCloseEvent *event)
{
	//renjinbo, #5053 because chat widget when start to load cef, this widget will cauch alt+f4 to close this cef widget instead of chat dialog.
	event->ignore();
	this->window()->hide();
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetInner::setURL(const std::string &url_)
{
	url = url_;
	if (browser) {
		browser->GetMainFrame()->LoadURL(url);
	}
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetInner::resize(bool bImmediately)
{
	auto func = [this, size = this->size()]() {
		if (!cef_widgets_contains(this) || !browser)
			return;

		HWND hwnd = browser->GetHost()->GetWindowHandle();
		MoveWindow(hwnd, 0, 0, size.width(), size.height(), TRUE);
	};

	if (bImmediately) {
		func();
	} else {
		executeOnCef(func);
	}
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetInner::executeOnUI(std::function<void()> func)
{
	QMetaObject::invokeMethod(qApp, func, Qt::QueuedConnection);
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetInner::executeOnCef(std::function<void()> func)
{
	QueueCEFTask([=]() { func(); });
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetInner::executeOnCef(
	std::function<void(CefRefPtr<CefBrowser>)> func)
{
	CefRefPtr<CefBrowser> browser = this->browser;
	if (browser) {
		QueueCEFTask([=]() { func(browser); });
	}
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetInner::attachBrowser(CefRefPtr<CefBrowser> browser)
{
	this->browser = browser;
	resize(true);

	//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
	if (popup) {
		QMetaObject::invokeMethod(popup, "open");
	}
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetInner::detachBrowser()
{
	this->browser = nullptr;

	//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
	if (popup) {
		QMetaObject::invokeMethod(popup, "accept");
		popup = nullptr;
	}
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
QCefWidgetImpl::QCefWidgetImpl(QWidget *parent, const std::string &url,
			       const std::string &script, Rqc rqc,
			       const Headers &headers, bool allowPopups,
			       bool callInit, QPLSBrowserPopupDialog *popup,
			       bool locked)
	: QCefWidget(parent), inner(nullptr)
{
	obs_browser_initialize();
	setAttribute(Qt::WA_NativeWindow);
	setFocusPolicy(Qt::ClickFocus);

	inner = new QCefWidgetInner(this, url, script, rqc, headers,
				    allowPopups, popup);
	cef_widgets_add(inner, locked);

	connect(inner, &QCefWidgetInner::titleChanged, this,
		&QCefWidgetImpl::titleChanged);
	connect(inner, &QCefWidgetInner::urlChanged, this,
		&QCefWidgetImpl::urlChanged);

	if (callInit) {
		inner->init();
	}
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
QCefWidgetImpl::QCefWidgetImpl(QPLSBrowserPopupDialog *parent,
			       QCefWidgetInner *checkInner, bool locked)
	: QCefWidgetImpl(parent, checkInner->url, checkInner->script,
			 checkInner->rqc, checkInner->headers,
			 checkInner->allowPopups, false, parent, locked)
{
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
QCefWidgetImpl::~QCefWidgetImpl()
{
	cef_widgets_remove(inner);
	inner->destroy();
	inner = nullptr;
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
bool QCefWidgetImpl::event(QEvent *event)
{
	bool result = QCefWidget::event(event);
	if (event->type() == QEvent::ParentChange) {
		inner->resize(false);
	}
	return result;
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetImpl::resizeEvent(QResizeEvent *event)
{
	QCefWidget::resizeEvent(event);
	inner->setGeometry(0, 0, event->size().width(), event->size().height());
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetImpl::setURL(const std::string &url)
{
	inner->setURL(url);
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetImpl::allowAllPopups(bool allow)
{
	inner->allowPopups = allow;
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QCefWidgetImpl::closeBrowser() {}

//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
QPLSBrowserPopupDialog::QPLSBrowserPopupDialog(QCefWidgetInner *checkInner_,
					       QWidget *parent, bool locked)
	: QDialog(parent), checkInner(checkInner_)
{
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint |
		       Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
	setAttribute(Qt::WA_DeleteOnClose, true);
	setWindowTitle(" ");
	setWindowIcon(QIcon());
	setGeometry(parent->geometry());

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setMargin(0);
	layout->setSpacing(0);

	impl = new QCefWidgetImpl(this, checkInner, locked);
	layout->addWidget(impl);
}

//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
QPLSBrowserPopupDialog::~QPLSBrowserPopupDialog()
{
	checkInner = nullptr;
	delete impl;
	impl = nullptr;
}

//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
QPLSBrowserPopupDialog *
QPLSBrowserPopupDialog::create(HWND &hwnd, QCefWidgetInner *checkInner,
			       QWidget *parent, bool locked)
{
	if (QThread::currentThread() == qApp->thread()) {
		QPLSBrowserPopupDialog *dialog =
			new QPLSBrowserPopupDialog(checkInner, parent, locked);
		hwnd = (HWND)dialog->impl->inner->winId();
		return dialog;
	}

	QPLSBrowserPopupDialog *dialog = nullptr;
	QMetaObject::invokeMethod(
		qApp,
		[&dialog, &hwnd, checkInner, parent, locked]() {
			dialog = create(hwnd, checkInner, parent, locked);
		},
		Qt::BlockingQueuedConnection);

	return dialog;
}

extern void DispatchJSEvent(std::string eventName, std::string jsonString);
//PRISM/Zhangdewen/20200921/#/fix jsonString must be keep problem
extern void DispatchJSEvent(obs_source_t *source, std::string eventName,
			    std::string jsonString);

void DispatchPrismEvent(const char *eventName, const char *jsonString)
{
	std::string event = eventName;
	std::string json = jsonString ? jsonString : "{}";

	auto func = [event, json](CefRefPtr<CefBrowser> browser) {
		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create("DispatchJSEvent");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();

		args->SetString(0, event);
		args->SetString(1, json);
		SendBrowserProcessMessage(browser, PID_RENDERER, msg);
	};

	cef_widgets_foreach([&](auto item) { item->executeOnCef(func); });

	//PRISM/Zhangdewen/20200921/#/fix event not notify web source
	DispatchJSEvent(event, json);
}

//PRISM/Zhangdewen/20200901/#/for chat source
void DispatchPrismEvent(obs_source_t *source, const char *eventName,
			const char *jsonString)
{
	DispatchJSEvent(source, eventName, jsonString);
}

/* ------------------------------------------------------------------------- */

struct QCefInternal : QCef {
	virtual bool init_browser(void) override;
	virtual bool initialized(void) override;
	virtual bool wait_for_browser_init(void) override;

	//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
	virtual QCefWidget *
	create_widget(QWidget *parent, const std::string &url,
		      const std::string &script,
		      QCefCookieManager *cookie_manager,
		      const std::map<std::string, std::string> &headers,
		      bool allowPopups) override;

	virtual QCefCookieManager *
	create_cookie_manager(const std::string &storage_path,
			      bool persist_session_cookies) override;

	virtual BPtr<char>
	get_cookie_path(const std::string &storage_path) override;

	virtual void add_popup_whitelist_url(const std::string &url,
					     QObject *obj) override;
	virtual void add_force_popup_url(const std::string &url,
					 QObject *obj) override;
};

bool QCefInternal::init_browser(void)
{
	if (os_event_try(cef_started_event) == 0)
		return true;

	obs_browser_initialize();
	return false;
}

bool QCefInternal::initialized(void)
{
	return os_event_try(cef_started_event) == 0;
}

bool QCefInternal::wait_for_browser_init(void)
{
	return os_event_wait(cef_started_event) == 0;
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
QCefWidget *QCefInternal::create_widget(
	QWidget *parent, const std::string &url, const std::string &script,
	QCefCookieManager *cookie_manager,
	const std::map<std::string, std::string> &headers, bool allowPopups)
{
	QCefCookieManagerInternal *cmi =
		reinterpret_cast<QCefCookieManagerInternal *>(cookie_manager);

	return new QCefWidgetImpl(parent, url, script, cmi ? cmi->rc : nullptr,
				  headers, allowPopups, true);
}

QCefCookieManager *
QCefInternal::create_cookie_manager(const std::string &storage_path,
				    bool persist_session_cookies)
{
	try {
		return new QCefCookieManagerInternal(storage_path,
						     persist_session_cookies);
	} catch (const char *error) {
		plog(LOG_ERROR, "Failed to create cookie manager: %s", error);
		return nullptr;
	}
}

BPtr<char> QCefInternal::get_cookie_path(const std::string &storage_path)
{
	BPtr<char> rpath = obs_module_config_path(storage_path.c_str());
	return os_get_abs_path_ptr(rpath.Get());
}

void QCefInternal::add_popup_whitelist_url(const std::string &url, QObject *obj)
{
	std::lock_guard<std::mutex> lock(popup_whitelist_mutex);
	popup_whitelist.emplace_back(url, obj);
}

void QCefInternal::add_force_popup_url(const std::string &url, QObject *obj)
{
	std::lock_guard<std::mutex> lock(popup_whitelist_mutex);
	forced_popups.emplace_back(url, obj);
}

extern "C" EXPORT QCef *obs_browser_create_qcef(void)
{
	return new QCefInternal();
}

#define BROWSER_PANEL_VERSION 2

extern "C" EXPORT int obs_browser_qcef_version_export(void)
{
	return BROWSER_PANEL_VERSION;
}
