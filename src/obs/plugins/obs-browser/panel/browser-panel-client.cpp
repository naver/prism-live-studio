#include "browser-panel-client.hpp"
#include "obs-frontend-api.h"
#include <util/dstr.h>

#include <QUrl>
#include <QDesktopServices>
#include <QApplication>
#include <QSettings>

//PRISM/Zhangdewen/20210120/#/for naver shopping
#include <QDialog>
#include <QThread>
#include <QVBoxLayout>
#include <QIcon>

#ifdef _WIN32
#include <windows.h>
#endif

//PRISM/Zhangdewen/20210113/#/for naver shopping
extern QWidget *(*pls_get_toplevel_view)(QWidget *widget);

//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
static inline QWidget *_pls_get_toplevel_view(QWidget *widget)
{
	return pls_get_toplevel_view ? pls_get_toplevel_view(widget) : widget;
}

//PRISM/Zhangdewen/20210113/#/for naver shopping
static QWidget *browser_popup_dialog_toplevel_view(QWidget *widget)
{
	for (; widget; widget = widget->parentWidget()) {
		if (dynamic_cast<QPLSBrowserPopupDialog *>(widget)) {
			return widget;
		}
	}
	return nullptr;
}

//PRISM/Zhangdewen/20210113/#/for naver shopping
static inline QWidget *get_toplevel_view(QWidget *widget)
{
	QWidget *toplevel_view = browser_popup_dialog_toplevel_view(widget);
	if (!toplevel_view) {
		return _pls_get_toplevel_view(widget);
	}
	return toplevel_view;
}

/* CefClient */
CefRefPtr<CefLoadHandler> QCefBrowserClient::GetLoadHandler()
{
	return this;
}

CefRefPtr<CefDisplayHandler> QCefBrowserClient::GetDisplayHandler()
{
	return this;
}

CefRefPtr<CefRequestHandler> QCefBrowserClient::GetRequestHandler()
{
	return this;
}

CefRefPtr<CefLifeSpanHandler> QCefBrowserClient::GetLifeSpanHandler()
{
	return this;
}

CefRefPtr<CefKeyboardHandler> QCefBrowserClient::GetKeyboardHandler()
{
	return this;
}

/* CefDisplayHandler */
void QCefBrowserClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
				      const CefString &title)
{
	//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
	bool processed = false;
	cef_widgets_sync_call(browserInner, [this, &processed, &browser,
					     &title](QCefWidgetInner *inner) {
		//PRISM/Zhangdewen/20210608/#/fix crash caused by strong killing cef process
		if (inner->browser && inner->browser->IsSame(browser)) {
			std::string str_title = title;
			QString qt_title = QString::fromUtf8(str_title.c_str());
			QMetaObject::invokeMethod(inner, "titleChanged",
						  Q_ARG(QString, qt_title));
			processed = true;
		}
	});
	if (!processed) {
#ifdef _WIN32
		std::wstring str_title = title;
		HWND hwnd = browser->GetHost()->GetWindowHandle();
		SetWindowTextW(hwnd, str_title.c_str());
#endif
	}
}

/* CefRequestHandler */
bool QCefBrowserClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
				       CefRefPtr<CefFrame>,
				       CefRefPtr<CefRequest> request, bool,
				       bool)
{
	std::string str_url = request->GetURL();

	//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
	QCefWidgetInner::Headers headers;
	cef_widgets_sync_call(browserInner, [&headers](QCefWidgetInner *inner) {
		headers = inner->headers;
	});
	for (const auto &header : headers) {
		request->SetHeaderByName(header.first, header.second, true);
	}

	//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
	cef_widgets_sync_call(checkInner, [str_url](QCefWidgetInner *inner) {
		QString qt_url = QString::fromUtf8(str_url.c_str());
		QMetaObject::invokeMethod(inner, "urlChanged",
					  Q_ARG(QString, qt_url));
	});
	return false;
}

bool QCefBrowserClient::OnOpenURLFromTab(
	CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString &target_url,
	CefRequestHandler::WindowOpenDisposition, bool)
{
	std::string str_url = target_url;

	/* Open tab popup URLs in user's actual browser */
	QUrl url = QUrl(str_url.c_str(), QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
	return true;
}

/* CefLifeSpanHandler */
bool QCefBrowserClient::OnBeforePopup(
	CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString &target_url,
	const CefString &, CefLifeSpanHandler::WindowOpenDisposition, bool,
	const CefPopupFeatures &, CefWindowInfo &windowInfo,
	CefRefPtr<CefClient> &browserClient, CefBrowserSettings &,
#if CHROME_VERSION_BUILD >= 3770
	CefRefPtr<CefDictionaryValue> &,
#endif
	bool *)
{
	//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
	std::string str_url = target_url;

	//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
	bool processed = false;
	cef_widgets_sync_call(browserInner, [&](QCefWidgetInner *inner) {
		if (!inner->allowPopups) {
			return;
		}

		QWidget *parent = get_toplevel_view(inner);

		HWND hwnd = nullptr;
		QPLSBrowserPopupDialog *dialog = QPLSBrowserPopupDialog::create(
			hwnd, checkInner, parent, true);
		CefRefPtr<QPLSBrowserPopupClient> browserPopupClient =
			new QPLSBrowserPopupClient(dialog);
		browserClient = browserPopupClient;
		inner->popups.append(browserPopupClient);

		RECT rc = {0, 0, dialog->width(), dialog->height()};
		windowInfo.SetAsChild(hwnd, rc);

		processed = true;
	});
	if (processed) {
		return false;
	}

	/* Open popup URLs in user's actual browser */
	QUrl url = QUrl(str_url.c_str(), QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
	return true;
}

void QCefBrowserClient::OnLoadEnd(CefRefPtr<CefBrowser>,
				  CefRefPtr<CefFrame> frame, int)
{
	//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
	if (frame->IsMain()) {
		std::string script;
		cef_widgets_sync_call(browserInner,
				      [&script](QCefWidgetInner *inner) {
					      script = inner->script;
				      });

		if (!script.empty()) {
			frame->ExecuteJavaScript(script, CefString(), 0);
		}
	}
}

bool QCefBrowserClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
				      const CefKeyEvent &event, CefEventHandle,
				      bool *)
{
#ifdef _WIN32
	if (event.type != KEYEVENT_RAWKEYDOWN)
		return false;

		//RENJINBO ignore refresh key, because chat local html can't reload.
		/*if (event.windows_key_code == 'R' &&
	    (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0) {
		browser->ReloadIgnoreCache();
		return true;
	}*/
#endif
	return false;
}

bool QCefBrowserClient::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId,
	CefRefPtr<CefProcessMessage> message)
{
	if (message->GetArgumentList()->GetSize() == 0) {
		return false;
	}

	std::string result("{}");
	std::string name = message->GetName();
	std::string param = message->GetArgumentList()->GetString(0);

	if ("sendToPrism" == name) {
		if (nullptr != prism_frontend_web_invoked) {
			result = prism_frontend_web_invoked(param.c_str());
		}
	} else {
		return false;
	}

	if (message->GetArgumentList()->GetSize() == 2) {
		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create("executeCallback");

		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetInt(0, message->GetArgumentList()->GetInt(1));
		args->SetString(1, result);

		SendBrowserProcessMessage(browser, PID_RENDERER, msg);
	}

	return true;
}

/*CefContextMenuHandler*/
CefRefPtr<CefContextMenuHandler> QCefBrowserClient::GetContextMenuHandler()
{
	return this;
}

enum UserDefineEnum {
	MENU_ID_USER_SHOWDEVTOOLS = MENU_ID_USER_FIRST + 300,
};

void QCefBrowserClient::OnBeforeContextMenu(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
	CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model)
{
	model->Clear();

	QSettings setting("NAVER Corporation", "Prism Live Studio");
	bool devServer = setting.value("DevServer", false).toBool();
	if (!devServer) {
		return;
	}

	model->AddItem(MENU_ID_BACK, "Back");
	model->SetEnabled(MENU_ID_BACK, browser->CanGoBack());

	model->AddItem(MENU_ID_FORWARD, "Forward");
	model->SetEnabled(MENU_ID_FORWARD, browser->CanGoForward());

	model->AddItem(MENU_ID_RELOAD_NOCACHE, "Reload");
	model->SetEnabled(MENU_ID_RELOAD_NOCACHE, true);

	if ((params->GetTypeFlags() & (CM_TYPEFLAG_PAGE | CM_TYPEFLAG_FRAME)) !=
	    0) {
		model->AddItem(MENU_ID_USER_SHOWDEVTOOLS, "Show DevTools");
	}
}

bool QCefBrowserClient::OnContextMenuCommand(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
	CefRefPtr<CefContextMenuParams> params, int command_id, EventFlags)
{
	switch (command_id) {
	case MENU_ID_USER_SHOWDEVTOOLS: {
		ShowDevelopTools(browser);
		return true;
	}
	default:
		break;
	}
	return false;
}

//PRISM/Zhangdewen/20210601/#/optimization, exception restart
void QCefBrowserClient::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
						  TerminationStatus status)
{
	cef_widgets_sync_call(browserInner, [this, browser](QCefWidgetInner *) {
		browserInner->destroy(true);
	});
}

void QCefBrowserClient::ShowDevelopTools(CefRefPtr<CefBrowser> browser)
{
	CefWindowInfo windowInfo;
	CefBrowserSettings settings;

#if defined(OS_WIN)
	windowInfo.SetAsPopup(NULL, "DevTools");
#endif
	browser->GetHost()->ShowDevTools(windowInfo, this, settings,
					 CefPoint());
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
void QPLSBrowserPopupClient::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
	cef_widgets_sync_call(browserInner, [this, browser](QCefWidgetInner *) {
		browserInner->attachBrowser(browser);
	});

	CefLifeSpanHandler::OnAfterCreated(browser);
}

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
bool QPLSBrowserPopupClient::DoClose(CefRefPtr<CefBrowser> browser)
{
	cef_widgets_sync_call(browserInner, [this](QCefWidgetInner *) {
		browserInner->detachBrowser();
		browserInner = nullptr;
	});

	return CefLifeSpanHandler::DoClose(browser);
}
