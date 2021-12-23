#pragma once

#include <QTimer>
#include <QPointer>
#include <QDialog>
#include <QFrame>
#include "browser-panel.hpp"
#include "cef-headers.hpp"

#include <vector>
#include <mutex>

struct PopupWhitelistInfo {
	std::string url;
	QPointer<QObject> obj;

	inline PopupWhitelistInfo(const std::string &url_, QObject *obj_)
		: url(url_), obj(obj_)
	{
	}
};

extern std::mutex popup_whitelist_mutex;
extern std::vector<PopupWhitelistInfo> popup_whitelist;
extern std::vector<PopupWhitelistInfo> forced_popups;

/* ------------------------------------------------------------------------- */

#if CHROME_VERSION_BUILD < 3770
class QCefRequestContextHandler : public CefRequestContextHandler {
	CefRefPtr<CefCookieManager> cm;

public:
	inline QCefRequestContextHandler(CefRefPtr<CefCookieManager> cm_)
		: cm(cm_)
	{
	}

	virtual CefRefPtr<CefCookieManager> GetCookieManager() override;

	IMPLEMENT_REFCOUNTING(QCefRequestContextHandler);
};
#endif

/* ------------------------------------------------------------------------- */

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
class QCefBrowserClient;
class QPLSBrowserPopupClient;
class QCefWidgetInner;

//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
class QPLSBrowserPopupDialog;

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
bool cef_widgets_contains(QCefWidgetInner *inner);
void cef_widgets_sync_call(QCefWidgetInner *inner,
			   std::function<void(QCefWidgetInner *)> call);

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
class QCefWidgetInner : public QFrame {
	Q_OBJECT

public:
	using Rqc = CefRefPtr<CefRequestContext>;
	using Headers = std::map<std::string, std::string>;

public:
	QCefWidgetInner(QWidget *parent, const std::string &url,
			const std::string &script, Rqc rqc,
			const Headers &headers, bool allowPopups,
			//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
			QPLSBrowserPopupDialog *popup);
	~QCefWidgetInner();

	CefRefPtr<CefBrowser> browser;

	std::string url;
	std::string script;
	CefRefPtr<CefRequestContext> rqc;
	Headers headers;

	bool allowPopups = false;
	QList<CefRefPtr<QPLSBrowserPopupClient>> popups;
	//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
	QPLSBrowserPopupDialog *popup = nullptr;

	void init();
	//PRISM/Zhangdewen/20210601/#/optimization, exception restart
	void destroy(bool restart = false);

	virtual void resizeEvent(QResizeEvent *event) override;
	virtual void closeEvent(QCloseEvent *event) override;

	void setURL(const std::string &url);
	void resize(bool bImmediately);

	void executeOnUI(std::function<void()> func);
	void executeOnCef(std::function<void()> func);
	void executeOnCef(std::function<void(CefRefPtr<CefBrowser>)> func);

	void attachBrowser(CefRefPtr<CefBrowser> browser);
	void detachBrowser();

signals:
	void titleChanged(const QString &title);
	void urlChanged(const QString &url);
};

//PRISM/Zhangdewen/20210311/#6991/nelo crash, browser refactoring
class QCefWidgetImpl : public QCefWidget {
	Q_OBJECT

public:
	using Rqc = QCefWidgetInner::Rqc;
	using Headers = QCefWidgetInner::Headers;

public:
	QCefWidgetImpl(QWidget *parent, const std::string &url,
		       const std::string &script, Rqc rqc,
		       const Headers &headers, bool allowPopups, bool callInit,
		       //PRISM/Zhangdewen/20210330/#/optimization, maybe crash
		       QPLSBrowserPopupDialog *popup = nullptr,
		       bool locked = false);
	QCefWidgetImpl(QPLSBrowserPopupDialog *parent,
		       QCefWidgetInner *checkInner, bool locked = false);
	~QCefWidgetImpl();

	bool event(QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void setURL(const std::string &url) override;
	void allowAllPopups(bool allow) override;
	void closeBrowser() override;

public:
	QCefWidgetInner *inner;
};

//PRISM/Zhangdewen/20210330/#/optimization, maybe crash
class QPLSBrowserPopupDialog : public QDialog {
	Q_OBJECT

private:
	QPLSBrowserPopupDialog(QCefWidgetInner *checkInner,
			       QWidget *parent = nullptr, bool locked = false);
	~QPLSBrowserPopupDialog();

public:
	static QPLSBrowserPopupDialog *create(HWND &hwnd,
					      QCefWidgetInner *checkInner,
					      QWidget *parent,
					      bool locked = false);

public:
	QCefWidgetInner *getCheckInner() { return checkInner; }
	QCefWidgetInner *getBrowserInner() { return impl->inner; }

private:
	QCefWidgetInner *checkInner = nullptr;
	QCefWidgetImpl *impl = nullptr;
};
