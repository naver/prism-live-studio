#include "browser-view.hpp"
#include "ui_PLSBrowserView.h"
#include "pls-common-define.hpp"
#include <QHBoxLayout>

#include "window-basic-main.hpp"

extern QCef *cef;
extern QCefCookieManager *panel_cookies;

static QJsonObject nullJsonObject;

PLSBrowserView::PLSBrowserView(const QUrl &url, QWidget *parent) : PLSBrowserView(nullJsonObject, url, nullptr, parent) {}

PLSBrowserView::PLSBrowserView(const QUrl &url, const std::map<std::string, std::string> &headers, QWidget *parent) : PLSBrowserView(nullJsonObject, url, headers, nullptr, parent) {}

PLSBrowserView::PLSBrowserView(QJsonObject &res, const QUrl &url, PLSResultCheckingCallback callback, QWidget *parent)
	: PLSBrowserView(nullJsonObject, url, std::map<std::string, std::string>(), nullptr, parent)
{
}

PLSBrowserView::PLSBrowserView(QJsonObject &res, const QUrl &url, const std::map<std::string, std::string> &headers, PLSResultCheckingCallback callback, QWidget *parent)
	: QDialog(parent), ui(new Ui::PLSBrowserView), result(res), uri(url.toString().toStdString()), resultCheckingCallback(callback), cefWidget(nullptr)
{
	ui->setupUi(this);
	QObject::connect(this, &PLSBrowserView::doneSignal, this, &PLSBrowserView::done, Qt::QueuedConnection);

	if (!cef) {
		emit doneSignal(QDialog::Rejected);
		return;
	}
	PLSBasic::InitBrowserPanelSafeBlock();

	cefWidget = cef->create_widget(this, uri, panel_cookies, headers);
	if (!cefWidget) {
		emit doneSignal(QDialog::Rejected);
		return;
	} else {
		cefWidget->setStartupScript(pls_get_offline_javaScript());
	}

	// QObject::connect(cefWidget, SIGNAL(titleChanged(const QString &)), this, SLOT(setWindowTitle(const QString &)));
	QObject::connect(cefWidget, SIGNAL(urlChanged(const QString &)), this, SLOT(urlChanged(const QString &)));

	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(0);
	layout->setSpacing(0);
	layout->addWidget(cefWidget);
	setWindowIcon(QIcon(""));
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
	setWindowTitle(ONE_SPACE);
	resize(800, 600);
}

PLSBrowserView::~PLSBrowserView()
{
	delete ui;
}

void PLSBrowserView::urlChanged(const QString &url)
{
	if (!resultCheckingCallback) {
		return;
	}

	auto cookie_cb = [this, url](const char *name, const char *value, const char *domain, const char *path, bool complete, void *context) {
		QMap<QString, QString> *cookies = (QMap<QString, QString> *)context;
		cookies->insert(QString::fromUtf8(name), QString::fromUtf8(value));
		if (complete) {
			switch (resultCheckingCallback(result, url, *cookies)) {
			case PLSResultCheckingResult::Ok:
				emit doneSignal(QDialog::Accepted);
				delete cookies;
				break;
			case PLSResultCheckingResult::Close:
				emit doneSignal(QDialog::Rejected);
				delete cookies;
				break;
			default:
				delete cookies;
				break;
			}
		}
	};

	QMap<QString, QString> *cookies = new QMap<QString, QString>();
	panel_cookies->ReadCookies(uri, cookie_cb, cookies);
}
