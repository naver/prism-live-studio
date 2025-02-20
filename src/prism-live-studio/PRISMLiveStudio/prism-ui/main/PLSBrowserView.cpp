#include "PLSBrowserView.hpp"
#include "ui_PLSBrowserView.h"
#include "pls-common-define.hpp"
#include <QHBoxLayout>
#include <QEventLoop>
#include "pls-shared-functions.h"

#include <liblog.h>

//#include "pls-global-vars.h"
#include "PLSBasic.h"
#include "PLSBrowserPanel.h"

constexpr auto PLS_BROWSER_VIEW_MODULE = "PLSBrowserView";
using namespace common;

extern PLSQCef *plsCef;

PLSBrowserView::PLSBrowserView(bool readCookies, const QUrl &url, QWidget *parent) : PLSBrowserView(readCookies, nullptr, url, nullptr, parent) {}

PLSBrowserView::PLSBrowserView(bool readCookies, const QUrl &url, const std_map<std::string, std::string> &headers, QWidget *parent)
	: PLSBrowserView(readCookies, nullptr, url, headers, QString(), nullptr, parent)
{
}

PLSBrowserView::PLSBrowserView(bool readCookies, QVariantHash *res, const QUrl &url, const PLSResultCheckingCallback &callback, QWidget *parent)
	: PLSBrowserView(readCookies, res, url, std_map<std::string, std::string>(), QString(), callback, parent)
{
}

PLSBrowserView::PLSBrowserView(bool readCookies, QVariantHash *res, const QUrl &url, const std_map<std::string, std::string> &headers, const QString &pannelCookieName,
			       const PLSResultCheckingCallback &callback, QWidget *parent)
	: PLSBrowserView(readCookies, res, url, headers, pannelCookieName, std::string(), callback, parent)
{
}

PLSBrowserView::PLSBrowserView(bool readCookies, QVariantHash *res, const QUrl &url, const std_map<std::string, std::string> &headers, const QString &pannelCookieName, const std::string &script,
			       const PLSResultCheckingCallback &callback, QWidget *parent)
	: QDialog(parent), result(res), uri(url.toString().toStdString()), resultCheckingCallback(callback), m_readCookies(readCookies)
{
	ui = pls_new<Ui::PLSBrowserView>();
	ui->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
	if (!plsCef) {
		emit doneSignal(QDialog::Rejected);
		return;
	}
	browser_panel_cookies = PLSBasic::getBrowserPannelCookieMgr(pannelCookieName);

	cefWidget = plsCef->create_widget(this, uri, script, browser_panel_cookies, headers, true, Qt::white, {}, true);
	if (!cefWidget) {
		emit doneSignal(QDialog::Rejected);
		return;
	}

#if 0
	QObject::connect(cefWidget, SIGNAL(titleChanged(const QString &)), this, SLOT(setWindowTitle(const QString &)));
#endif

	auto layout = pls_new<QHBoxLayout>(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(cefWidget);
	setWindowIcon(QIcon(""));
	setWindowTitle(ONE_SPACE);

	auto palette = this->palette();
	palette.setColor(QPalette::ColorRole::Window, Qt::white);
	this->setPalette(palette);
	setAutoFillBackground(true);

	resize({800, 600});
	QObject::connect(this, &PLSBrowserView::doneSignal, this, &PLSBrowserView::done, Qt::QueuedConnection);
	QObject::connect(cefWidget, SIGNAL(urlChanged(const QString &)), this, SLOT(urlChanged(const QString &)));
}

PLSBrowserView::~PLSBrowserView()
{
	pls_delete(ui, nullptr);
}

int PLSBrowserView::exec()
{
	pls_push_modal_view(this);
	int retval = QDialog::exec();
	pls_pop_modal_view(this);
	return retval;
}

void PLSBrowserView::done(int result)
{
	closeBrowser();
	QDialog::done(result);
}

void PLSBrowserView::closeBrowser()
{
	if (m_closeBrowserTag) {
		return;
	}

	m_closeBrowserTag = true;

	static int panel_version = -1;
	if (panel_version == -1) {
		panel_version = obs_browser_qcef_version();
	}

	if (panel_version >= 2 && cefWidget) {
		cefWidget->closeBrowser();
	}
}

void PLSBrowserView::closeNoButton()
{
	done(QDialog::Rejected);
}

void PLSBrowserView::showEvent(QShowEvent *event)
{
	if (parentWidget()) {
		pls_aligin_to_widget_center(this, parentWidget());
	} else {
		pls_aligin_to_widget_center(this, pls_get_main_view());
	}
	QDialog::showEvent(event);
}

void PLSBrowserView::closeEvent(QCloseEvent *event)
{
	PLS_INFO(PLS_BROWSER_VIEW_MODULE, "close browser view");
	closeBrowser();
	emit closeSignal();
	QDialog::closeEvent(event);
}

void PLSBrowserView::urlChanged(const QString &url)
{
	if (!result || !resultCheckingCallback) {
		return;
	}

	if (!m_readCookies) {
		switch (resultCheckingCallback(*result, url, QMap<QString, QString>())) {
		case PLSResultCheckingResult::Ok:
			emit doneSignal(QDialog::Accepted);
			break;
		case PLSResultCheckingResult::Close:
			emit doneSignal(QDialog::Rejected);
			break;
		default:
			break;
		}
		return;
	}

	if (std::shared_ptr<EventLoop> eventLoop = m_eventLoop.lock(); eventLoop && (eventLoop->quitFlag == InitValue)) {
		PLS_INFO(PLS_BROWSER_VIEW_MODULE, "cancel read browser cookies");
		eventLoop->quit(QuitByCancel, false);

		QMetaObject::invokeMethod(this, "urlChanged", Qt::QueuedConnection, Q_ARG(QString, url));
		return;
	}

	auto eventLoop = std::make_shared<EventLoop>();
	m_eventLoop = eventLoop;

	connect(this, &PLSBrowserView::doneSignal, eventLoop->el, [eventLoop]() {
		if (eventLoop->quitFlag == InitValue) {
			PLS_INFO(PLS_BROWSER_VIEW_MODULE, "cancel read browser cookies by done signal");
			eventLoop->quit(QuitByDoneSignal, false);
		}
	});
	connect(this, &PLSBrowserView::closeSignal, eventLoop->el, [eventLoop]() {
		if (eventLoop->quitFlag == InitValue) {
			PLS_INFO(PLS_BROWSER_VIEW_MODULE, "cancel read browser cookies by close signal");
			eventLoop->quit(QuitByCloseSignal, false);
		}
	});
	connect(eventLoop->timer, &QTimer::timeout, eventLoop->el, [eventLoop]() {
		if (eventLoop->quitFlag == InitValue) {
			PLS_INFO(PLS_BROWSER_VIEW_MODULE, "cancel read browser cookies by timeout signal");
			eventLoop->quit(QuitByTimeoutSignal, false);
		}
	});

	PLS_INFO(PLS_BROWSER_VIEW_MODULE, "begin read browser cookies");
	auto cookieManager = dynamic_cast<PLSQCefCookieManager *>(browser_panel_cookies);
	if (cookieManager) {
		cookieManager->ReadAllCookies([eventLoop](const std::list<PLSQCefCookieManager::Cookie> &cookies) {
			for (const auto &cookie : cookies) {
				eventLoop->cookies.insert(QString::fromStdString(cookie.name), QString::fromStdString(cookie.value));
			}

			if (eventLoop->quitFlag == InitValue) {
				PLS_INFO(PLS_BROWSER_VIEW_MODULE, "complete read browser cookies");
				eventLoop->quit(QuitByVisitAllCookies, true);
			}
		});
		eventLoop->exec();
	}
	pls_modal_check_app_exiting();

	if (eventLoop->quitFlag != QuitByVisitAllCookies) {
		return;
	}

	switch (resultCheckingCallback(*result, url, eventLoop->cookies)) {
	case PLSResultCheckingResult::Ok:
		emit doneSignal(QDialog::Accepted);
		break;
	case PLSResultCheckingResult::Close:
		emit doneSignal(QDialog::Rejected);
		break;
	default:
		break;
	}
}

PLSBrowserView::EventLoop::EventLoop()
{
	el = pls_new<QEventLoop>();
	timer = pls_new<QTimer>();
	timer->setSingleShot(true);
}

PLSBrowserView::EventLoop::~EventLoop()
{
	el->deleteLater();
	timer->deleteLater();
}

void PLSBrowserView::EventLoop::quit(int quitFlag_, bool async)
{
	this->quitFlag = quitFlag_;

	if (async) {
		QMetaObject::invokeMethod(this->timer, "stop");
		QMetaObject::invokeMethod(this->el, "quit");
	} else {
		this->timer->stop();
		this->el->quit();
	}
}

int PLSBrowserView::EventLoop::exec(int timeout)
{
	timer->start(timeout);

	return this->el->exec();
}
