#ifndef BROWSERVIEW_HPP
#define BROWSERVIEW_HPP

#include <QMap>
#include <QUrl>
#include <browser-panel.hpp>

#include "dialog-view.hpp"
#include "frontend-api.h"

namespace Ui {
class PLSBrowserView;
}

using PLSResultCheckingCallback = pls_result_checking_callback_t;

class PLSBrowserView : public QDialog {
	Q_OBJECT

public:
	explicit PLSBrowserView(const QUrl &url, QWidget *parent = nullptr);
	explicit PLSBrowserView(const QUrl &url, const std::map<std::string, std::string> &headers, QWidget *parent = nullptr);
	explicit PLSBrowserView(QJsonObject &result, const QUrl &url, PLSResultCheckingCallback callback = nullptr, QWidget *parent = nullptr);
	explicit PLSBrowserView(QJsonObject &result, const QUrl &url, const std::map<std::string, std::string> &headers, PLSResultCheckingCallback callback = nullptr, QWidget *parent = nullptr);
	~PLSBrowserView();

signals:
	void doneSignal(int code);

private slots:
	void urlChanged(const QString &url);

private:
	Ui::PLSBrowserView *ui;
	QJsonObject &result;
	std::string uri;
	PLSResultCheckingCallback resultCheckingCallback;
	QCefWidget *cefWidget;
};

#endif // BROWSERVIEW_HPP
