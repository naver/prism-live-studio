#ifndef BROWSERVIEW_HPP
#define BROWSERVIEW_HPP

#include <QEventLoop>
#include <QMap>
#include <QUrl>
#include <browser-panel.hpp>
#include <memory>

#include "frontend-api.h"
#include "libui.h"

namespace Ui {
class PLSBrowserView;
}

using PLSResultCheckingCallback = pls_result_checking_callback_t;

class PLSBrowserView : public QDialog, public pls::ICloseDialog {
	Q_OBJECT

private:
	enum { InitValue, QuitByDoneSignal, QuitByCloseSignal, QuitByTimeoutSignal, QuitByCancel, QuitByVisitAllCookies };

	struct EventLoop {
		std::atomic<int> quitFlag = InitValue;
		QEventLoop *el = nullptr;
		QMap<QString, QString> cookies;
		QTimer *timer = nullptr;

		EventLoop();
		~EventLoop();

		void quit(int quitFlag, bool async);
		int exec(int timeout = 10000);
	};

public:
	explicit PLSBrowserView(bool readCookies, const QUrl &url, QWidget *parent = nullptr);
	explicit PLSBrowserView(bool readCookies, const QUrl &url, const std_map<std::string, std::string> &headers, QWidget *parent = nullptr);
	explicit PLSBrowserView(bool readCookies, QJsonObject *result, const QUrl &url, const PLSResultCheckingCallback &callback = nullptr, QWidget *parent = nullptr);
	explicit PLSBrowserView(bool readCookies, QJsonObject *result, const QUrl &url, const std_map<std::string, std::string> &headers, const QString &pannelCookieName = QString(),
				const PLSResultCheckingCallback &callback = nullptr, QWidget *parent = nullptr);
	explicit PLSBrowserView(bool readCookies, QJsonObject *result, const QUrl &url, const std_map<std::string, std::string> &headers, const QString &pannelCookieName = QString(),
				const std::string &script = std::string(), const PLSResultCheckingCallback &callback = nullptr, QWidget *parent = nullptr);
	~PLSBrowserView() override;

	int exec() override;
	void done(int result) override;
	void closeBrowser();

protected:
	void closeNoButton() override;
	void closeEvent(QCloseEvent *event) override;
	void showEvent(QShowEvent *event) override;

signals:
	void doneSignal(int code);
	void closeSignal();

private slots:
	void urlChanged(const QString &url);

private:
	Ui::PLSBrowserView *ui = nullptr;
	QJsonObject *result = nullptr;
	std::string uri;
	PLSResultCheckingCallback resultCheckingCallback = nullptr;
	QCefWidget *cefWidget = nullptr;
	QCefCookieManager *browser_panel_cookies = nullptr;
	double m_dpi = 0.0;
	bool m_readCookies = true;
	bool m_closeBrowserTag = false;
	std::weak_ptr<EventLoop> m_eventLoop;
};

#endif // BROWSERVIEW_HPP
