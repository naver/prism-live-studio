#include "login-select-platform-view.hpp"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSCommonConst.h"
#include "PLSLoginDataHandler.h"
#include "../../pls-common-language.hpp"
#include "login-common-helper.hpp"
#include "login-common-struct.hpp"
#include "login-custom-button.hpp"

#include <QStackedWidget>
#include <QEventLoop>
#include <qdebug.h>
#include <qmessagebox.h>
#include <qtcpserver.h>
#include <QDesktopServices>
#include <qurlquery.h>
#include <qdir.h>
#include <qjsonobject.h>
#include "libutils-api.h"
#include <QLocalServer>
#include <QLocalSocket>
#include "libhttp-client.h"
#include "login-terms-of-agree-view.hpp"
#include "PLSAlertView.h"
#include "PLSErrorHandler.h"
#include "network-state.h"
#include "PLSLoginMainView.h"
#include "PLSContactView.hpp"
#include "libbrowser.h"
#include "pls-net-url.hpp"
#include "login-with-email-view.hpp"
#include "pls-gpop-data.hpp"
#include "PLSSyncServerManager.hpp"
#include "obs-app.hpp"
#include "PLSApp.h"
#include "PLSBasic.h"
#include "PLSRecentLoginStore.hpp"
#include <QAbstractSlider>
#include <QEvent>
#include <QLayout>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QTimer>
#include <QUuid>
using namespace common;

namespace {

constexpr int plsRecentBadgeRightMargin = 5;
constexpr int plsRecentBadgeOverlapHeight = 13;

QWidget *findChildButtonWithPrismLoginType(QLayout *lay, qint32 kind)
{
	if (!lay) {
		return nullptr;
	}
	for (int i = 0; i < lay->count(); ++i) {
		QLayoutItem *item = lay->itemAt(i);
		if (!item) {
			continue;
		}
		if (QWidget *w = item->widget()) {
			if (w->property("prismLoginType").toInt() == kind) {
				return w;
			}
		}
		if (QLayout *sub = item->layout()) {
			if (QWidget *found = findChildButtonWithPrismLoginType(sub, kind)) {
				return found;
			}
		}
	}
	return nullptr;
}

} // namespace

PLSSelectLoginPlatformView::PLSSelectLoginPlatformView(QWidget *parent) : QWidget(parent)
{
	ui = pls_new<Ui::SelectLoginPlatformView>();
	initUi();
}

PLSSelectLoginPlatformView::~PLSSelectLoginPlatformView()
{
	PLS_INFO(LAUNCHER_INIT, "PLSSelectLoginPlatformView delete");
	pls_delete(ui);
}

void PLSSelectLoginPlatformView::setLoginStackWidget(QStackedWidget *stackWidget)
{
	if (stackWidget) {
		m_loginStackFrame = stackWidget;
	}
}

QString PLSSelectLoginPlatformView::getmoduleName(int index) const
{
	const auto metaEnum = QMetaEnum::fromType<PRISMLOGINTYPE>();
	return QString(metaEnum.valueToKey(index));
}
void PLSSelectLoginPlatformView::selectB2BLogin(int type, const QString &loginName, qint32 recentKind)
{
	QString serviceName = getLoginParam(loginName).first.value("serviceName").toString();

	if (serviceName.isEmpty()) {
		auto w = LoginCommonHelpers::setCurrentStackWidget(m_loginStackFrame, LOGIN_WITH_EMAIL_VIEW);
		if (LoginWithEmailView *widget = nullptr; widget = dynamic_cast<LoginWithEmailView *>(w)) {
			widget->changeToNCB2BUi();
			PLS_UI_ACTION("show b2b login view");
		} else {
			PLS_WARN(PLS_LOGIN_MODULE, "cannot open input ncb2b name view");
		}
	} else {
		PLSLoginDataHandler::instance()->getNCPServiceId(
			serviceName, [this, loginName, type, recentKind](const QString &authUrl) { getAuthUrlCallback(authUrl, loginName, type, recentKind); }, [this](int, int) {}, loginName);
	}
}
void PLSSelectLoginPlatformView::getAuthUrlCallback(const QString &authUrl, const QString &loginName, int type, qint32 recentKind)
{
	m_ncpAuthUrl = authUrl;
	auto isSuccess = loginWithAccount(type, loginName, recentKind, this);

	LoginCommonHelpers::setCurrentStackWidget(m_loginStackFrame, LOGIN_WITH_EMAIL_VIEW);
	auto w = LoginCommonHelpers::setCurrentStackWidget(m_loginStackFrame, LOGIN_WITH_EMAIL_VIEW);
	if (LoginWithEmailView *widget = dynamic_cast<LoginWithEmailView *>(w); widget) {
		widget->setLoginButtonStatus(isSuccess);
	}
	loginResultHandler(m_loginStackFrame, isSuccess);
}

bool PLSSelectLoginPlatformView::isB2BAccessDenied(const QString &urlStr) const
{
	QUrl url(urlStr);
	QUrlQuery query(url);
	if (bool isHasItem = query.hasQueryItem("error"); isHasItem && query.queryItemValue("error") == "access_denied") {
		return true;
	}
	return false;
}
bool PLSSelectLoginPlatformView::runLocalServer(const QString &callbackUrl, const std::function<void(const QString &code)> &callback)
{
	auto getCode = [](const QString &url) {
		PLS_INFO(LAUNCHER_LOGIN, "sns auth code from url = %s", qPrintable(url));
		QUrlQuery queryCode(QUrl(url).query());
		auto code = queryCode.queryItemValue("code", QUrl::FullyDecoded);
		return code;
	};
#if defined Q_OS_WIN
	auto server = new QLocalServer();
	bool isRemove = server->removeServer("PRISMLiveStudio");
	if (!isRemove) {
		PLS_WARN(LAUNCHER_LOGIN, "remove local server failed");
		callback("");
		return false;
	}
	if (!server->listen("PRISMLiveStudio")) {
		PLS_WARN(LAUNCHER_LOGIN, "listen local server failed");
		callback("");
		return false;
	}
	QObject::connect(server, &QLocalServer::newConnection, [server, this, callbackUrl, callback, getCode] {
		QLocalSocket *client = server->nextPendingConnection();
		if (!client)
			return;
		QObject::connect(client, &QLocalSocket::readyRead, [this, client, server, callbackUrl, callback, getCode] {
			QString msg = QString::fromUtf8(client->readAll());
			if (msg.startsWith(callbackUrl)) {
				auto code = getCode(msg);
				server->disconnect();
				client->disconnect();
				client->flush();
				client->close();
				client->deleteLater();
				server->close();
				server->deleteLater();
				pls_invoke_safe(callback, code);
			}
		});
	});
#elif defined Q_OS_MACOS
	auto callbackHandler = [getCode, callbackUrl, callback](const QString &url) {
		if (!url.startsWith(callbackUrl)) {
			return;
		}
		auto code = getCode(url);
		pls_invoke_safe(callback, code);
	};
	if (callbackUrl == g_plsFacebookCallbackUrl) {
		QObject::connect(PLSApp::plsApp(), &PLSApp::facebookAuthCallbackUrl, this, callbackHandler, Qt::SingleShotConnection);
	} else {
		QObject::connect(PLSApp::plsApp(), &PLSApp::appleIDAuthCallbackUrl, this, callbackHandler, Qt::SingleShotConnection);
	}

#endif

	return true;
}

bool PLSSelectLoginPlatformView::isSpecialLogin(PRISMLOGINTYPE loginType)
{
	return m_specialLoginList.contains(loginType);
}

bool PLSSelectLoginPlatformView::loginWithAccount(int loginType, const QString &loginName, qint32 recentKind, const QWidget *parent)
{
	pls_unused(parent);
	bool isDeleteCookie = true;
	auto loginEnum = static_cast<PRISMLOGINTYPE>(loginType);
	auto callbackUrl = PLSLoginDataHandler::instance()->getSnsCallbackUrl(getmoduleName(loginType));
	pls::http::Method method = pls::http::Method::Get;
	auto url = QString("%1%2").arg(pls_http_api_func::getPrismAuthGateWay());
	QUrl authUrl;
	QString accessTokenUrl;
	switch (loginEnum) {
	case PLSSelectLoginPlatformView::PRISMLOGINTYPE::Google:
		break;
	case PLSSelectLoginPlatformView::PRISMLOGINTYPE::Twitch:
		method = pls::http::Method::Post;
		isDeleteCookie = false;
		callbackUrl = TWITCH_REDIRECT_URI;
		authUrl = PLSLOGINDATAHANDLER->getSnsAuthUrl(CHANNEL_TWITCH_LOGIN_URL, TWITCH_CLIENT_ID, TWITCH_CLIENT_SECRET, callbackUrl, "channel_read+chat_login+channel_editor", "", loginName);
		accessTokenUrl = g_plsTwitchAuthTokenUrl;
		break;
	case PLSSelectLoginPlatformView::PRISMLOGINTYPE::NAVER:
		url = url.arg(pls_launcher_const::NAVER_LOGIN_URL);
		authUrl = pls::http::buildHmacUrl(url, pls_http_api_func::getPrismHamcKey()).toString();
		break;
	case PLSSelectLoginPlatformView::PRISMLOGINTYPE::LINE:
		url = url.arg(pls_launcher_const::LINE_LOGIN_URL);
		authUrl = pls::http::buildHmacUrl(url, pls_http_api_func::getPrismHamcKey()).toString();
		break;
	case PLSSelectLoginPlatformView::PRISMLOGINTYPE::NAVER_Cloud_B2B: {
		authUrl = m_ncpAuthUrl;
		QUrlQuery query(authUrl.query());
		callbackUrl = query.queryItemValue("redirect_uri", QUrl::FullyDecoded);
		method = pls::http::Method::Post;
	} break;
	default:
		PLS_ERROR(LAUNCHER_LOGIN, "invaild login button");
		return false;
	}
	QString prismCallbackUrl;
	QList<QNetworkCookie> prismLoginCookies;

	PLS_INFO(LAUNCHER_LOGIN, "start request sns login url = %s", authUrl.toString().toUtf8().constData());

	auto browserDialog =
		pls::browser::newBrowserDialog(pls::browser::Params()
						       .cookieStoragePath(PLSBasic::cookiePath(loginName))
						       .url(authUrl)
						       .size(QSize(800, 600))
						       .autoSetTitle(false)
						       .title(" ")
						       .parent(pls_get_toplevel_view(this))
						       .showAtLoadEnded(true)
						       .urlChanged([loginEnum, loginType, this, &prismCallbackUrl, &prismLoginCookies,
								    callbackUrl](pls::browser::Browser *browser, const QString &urlStr, const QList<pls::browser::Cookie> &cookies) {
							       if (callbackUrl.isEmpty()) {
								       PLS_ERROR(LAUNCHER_LOGIN, "login name %s,callback url is empty!", getmoduleName(loginType).toUtf8().constData());
								       browser->done(QDialog::Rejected);
							       }
							       if (urlStr.startsWith(callbackUrl) && !callbackUrl.isEmpty()) {
								       prismLoginCookies = pls::browser::toNetworkCookieList(cookies);
								       prismCallbackUrl = urlStr;
								       PLS_INFO(LAUNCHER_LOGIN, "get sns login callback url= %s", prismCallbackUrl.toUtf8().constData());
								       browser->done(QDialog::Accepted);
							       }
						       })
						       .browserDone([loginEnum, &prismCallbackUrl, &prismLoginCookies, method, this, isDeleteCookie](const pls::browser::Browser *browser, int result) {
							       pls_unused(result);
							       if (isDeleteCookie)
								       browser->deleteCookie({}, {});
						       }));

	if (!browserDialog) {
		PLS_ERROR(LAUNCHER_LOGIN, "init login browser error");
		return false;
	}

	browserDialog->setAttribute(Qt::WA_DeleteOnClose);
	browserDialog->setObjectName(loginName);
	browserDialog->addCanCloseChecker([browserDialog, isDeleteCookie]() -> bool {
		PLS_INFO(LAUNCHER_LOGIN, "close browser dialog");
		if (isDeleteCookie) {
			browserDialog->deleteCookie({}, {});
		}
		browserDialog->closeBrowser();
		return true;
	});
	if (QDialog::Accepted != browserDialog->exec()) {
		return false;
	}

	if (loginEnum == PRISMLOGINTYPE::NAVER_Cloud_B2B) {
		if (isB2BAccessDenied(prismCallbackUrl)) {
			PLS_ERROR(LAUNCHER_LOGIN, "B2B access denied, prism callbackUrl = %s", prismCallbackUrl.toUtf8().constData());
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LOGIN_B2B_ACCESS_DENIED, PLSErrKeyAllAlert, {},
							      PLSErrorHandler::ExtraData("PLSSelectLoginPlatformView::loginWithAccount"), const_cast<QWidget *>(parent));
			return false;
		}
		if (!PLSLoginDataHandler::instance()->getNCPAccessToken(prismCallbackUrl, loginName)) {
			PLS_ERROR(LAUNCHER_LOGIN, "B2B get accesstoken failed");
			return false;
		}
		prismCallbackUrl = PRISM_NCP_AUTH_JOIN_API.arg(PRISM_SSL);
		prismLoginCookies = {};

	} else if (loginEnum == PRISMLOGINTYPE::Twitch) {
		QUrlQuery query(authUrl.query());
		QUrlQuery queryCode(QUrl(prismCallbackUrl).query());
		auto code = queryCode.queryItemValue("code", QUrl::FullyDecoded);
		bool isGetTokenSucces = PLSLOGINDATAHANDLER->getSNSAccessToken(accessTokenUrl, query.queryItemValue(HTTP_CLIENT_ID), query.queryItemValue(HTTP_CLIENT_SECRET),
									       query.queryItemValue(HTTP_REDIRECT_URI), code, loginName);
		if (!isGetTokenSucces) {
			PLS_ERROR(LAUNCHER_LOGIN, "%s get accesstoken failed", loginName.toUtf8().constData());
			return false;
		}
		prismCallbackUrl = PLS_GOOGLE_LOGIN_URL_TOKEN.arg(PRISM_SSL);
		prismLoginCookies = {};
	}
	return PLSLoginDataHandler::instance()->getPrismUserInfoFromRemote(prismLoginCookies, prismCallbackUrl, method, loginName, recentKind);
}

QUrl PLSSelectLoginPlatformView::buildFacebookLoginAuthUrl()
{
	QUrl url(CHANNEL_FACEBOOK_LOGIN_URL);
	QUrlQuery query;
	query.addQueryItem(HTTP_CLIENT_ID, CHANNEL_FACEBOOK_CLIENT_ID);
	query.addQueryItem(HTTP_REDIRECT_URI, FACEBOOK_LOGIN_REDIRECT_URI);
	query.addQueryItem("response_type", "code");
	query.addQueryItem("auth_type", "rerequest");
	query.addQueryItem("state", QStringLiteral("login"));
	url.setQuery(query);
	return url;
}

void PLSSelectLoginPlatformView::loginWithAccountAsync(const std::function<void(bool ok, const QJsonObject &)> &callback, int loginType, const QString &loginName, qint32 recentKind,
						       QPushButton *button, const QWidget *parent)
{
	pls_unused(parent);

	const auto loginEnum = static_cast<PRISMLOGINTYPE>(loginType);
	if (loginEnum == PRISMLOGINTYPE::Apple || loginEnum == PRISMLOGINTYPE::Facebook) {
		const bool isFacebookLogin = loginEnum == PRISMLOGINTYPE::Facebook;
		const QString callbackUrl = isFacebookLogin ? g_plsFacebookCallbackUrl : g_plsAppleIDCallbackUrl;
		QPointer<QPushButton> loginButton(button);
		runLocalServer(callbackUrl, [callback, loginButton, loginName, recentKind, isFacebookLogin](const QString &code) {
			if (!code.isEmpty()) {
				if (!loginButton) {
					callback(false, QJsonObject());
					return;
				}
				loginButton->blockSignals(true);
				PLSLOGINDATAHANDLER->setSnsCode(code);
				auto prismCallbackUrl = isFacebookLogin ? PLS_GOOGLE_LOGIN_URL_TOKEN.arg(PRISM_SSL) : PLS_SNS_LOGIN_SIGNUP_URL.arg(PRISM_SSL);
				if (isFacebookLogin) {
					bool isGetTokenSucces = PLSLOGINDATAHANDLER->getSNSAccessToken(FACEBOOK_AUTH_TOKEN_URL, CHANNEL_FACEBOOK_CLIENT_ID, CHANNEL_FACEBOOK_SECRET,
												       FACEBOOK_LOGIN_REDIRECT_URI, code, loginName);
					if (!isGetTokenSucces) {
						PLS_ERROR(LAUNCHER_LOGIN, "%s get accesstoken failed", loginName.toUtf8().constData());
						callback(false, QJsonObject());
						return;
					}
				}
				bool isSuccess = PLSLoginDataHandler::instance()->getPrismUserInfoFromRemote({}, prismCallbackUrl, pls::http::Method::Post, loginName, recentKind);
				callback(isSuccess, QJsonObject());
			} else {
				callback(false, QJsonObject());
			}
		});
		auto authUrl = isFacebookLogin ? buildFacebookLoginAuthUrl()
					       : PLSLOGINDATAHANDLER->getSnsAuthUrl(g_plsAppleIDAuthUrl, APPLE_ID_CLIENT_ID, "", APPLE_ID_REDIRECT_URI, "email name", "", loginName);
		pls_async_invoke([authUrl]() { QDesktopServices ::openUrl(authUrl); });
		PLS_UI_ACTION("%s browser view", loginName.toUtf8().constData());
		return;
	}
	if (m_httpServerAddress.isEmpty()) {
		if (pls_run_http_server("/oauth2/callback/google", m_httpServerAddress, [this, callback, button, loginName, recentKind](const auto &url, const QJsonObject &json) {
			    this->m_httpServerAddress.clear();

			    if (QString code = json.value("code").toString(); !code.isEmpty()) {
				    button->blockSignals(true);
				    PLSLoginDataHandler::instance()->pls_google_user_info(callback, QString("http://%1/oauth2/callback/google").arg(url), code, loginName, recentKind);
			    } else {
				    PLS_WARN(FRONTEND_PLUGINS_GOOGLE_LOGIN, json.value("message").toString().toUtf8().data());
				    callback(false, QJsonObject());
			    }
		    })) {
		} else {
			PLS_WARN(tr("Alert.Title").toUtf8().constData(), tr("host.browser.error").toUtf8().constData());
		}
	}

	if (!m_httpServerAddress.isEmpty()) {
		QString url("https://accounts.google.com/o/oauth2/v2/auth?client_id=%1&redirect_uri=http://%2/oauth2/callback/google&scope=profile&email&response_type=code");
		url = url.arg(pls_launcher_const::YOUTUBE_CLIENT_ID_, m_httpServerAddress);
		pls_async_invoke([url]() { QDesktopServices ::openUrl(url); });
		PLS_UI_ACTION("%s browser view", loginName.toUtf8().constData());
	}
}
template<class Callback> bool PLSSelectLoginPlatformView::pls_run_http_server(const char *path, QString &addr, Callback callback)
{
	auto tcpServer = pls_new<QTcpServer>(this);
	if (!tcpServer->listen(QHostAddress::LocalHost)) {
		return false;
	}
	addr = QString("%1:%2").arg(tcpServer->serverAddress().toString()).arg(tcpServer->serverPort());

	QObject::connect(tcpServer, &QTcpServer::acceptError, &QObject::deleteLater);
	QObject::connect(tcpServer, &QTcpServer::newConnection, [this, tcpServer, path = QString(path), callback, addr] {
		auto tcpClient = tcpServer->nextPendingConnection();
		if (nullptr == tcpClient)
			return;

		QObject::connect(tcpClient, &QTcpSocket::readyRead, tcpServer, [this, tcpClient, tcpServer, path, callback, addr] {
			PLS_INFO(LAUNCHER_INIT, "PLSSelectLoginPlatformView readyRead enter");
			if (nullptr == tcpServer || nullptr == tcpClient) {
				return;
			}

			readDataFromRemote(tcpClient, path, callback, addr, tcpServer);
		});
	});
	return true;
}
template<class Callback> void PLSSelectLoginPlatformView::readDataFromRemote(QTcpSocket *tcpClient, const QString &path, Callback callback, const QString &addr, QTcpServer *tcpServer) const
{
	for (;;) {
		auto data = tcpClient->readLine();
		if (data.isEmpty()) {
			break;
		}

		auto line = QString(data.trimmed());
		if (line.isEmpty() || !(line.startsWith("POST") || line.startsWith("GET"))) {
			PLS_INFO(LAUNCHER_LOGIN, "line invalid");
			continue;
		}

		auto urls = line.split(" ");
		if (urls.length() != 3) {
			break;
		}

		if (!urls[1].startsWith(path)) {
			PLS_INFO(LAUNCHER_LOGIN, "urls invalid");
			break;
		}
		onWriteClient(addr, callback, tcpClient, QUrl(urls[1]), path);

		tcpClient->flush();
		tcpClient->close();
		tcpClient->deleteLater();

		tcpServer->close();
		tcpServer->deleteLater();
		break;
	}
}

template<class Callback> void PLSSelectLoginPlatformView::onWriteClient(const QString &addr, Callback &&callback, QTcpSocket *tcpClient, const QUrl &url, const QString &path) const
{
	tcpClient->write("HTTP/1.1 200 OK\n");
	tcpClient->write("Content-Type: text/html; charset=UTF-8\n");
	tcpClient->write("\n");

	QJsonObject root;
	QUrlQuery query(url.query());
#if defined(Q_OS_WIN)
	QString htmlPath = pls_get_app_dir() + QString("/../../data/prism-studio/webpage/%1");
#elif defined(Q_OS_MACOS)
	QString htmlPath = pls_get_app_resource_dir() + "/data/prism-studio/webpage/%1";
#endif
	if (auto code = query.queryItemValue("code", QUrl::FullyDecoded); code.isEmpty()) {
		root.insert("message", "code is empty");

		static QByteArray bodyFailed;

		if (bodyFailed.isEmpty()) {
			QFile fileFailed(htmlPath.arg("loginFail.html"));
			if (fileFailed.open(QIODevice::ReadOnly)) {
				bodyFailed = fileFailed.readAll();
				fileFailed.close();
			}
		}
		tcpClient->write(bodyFailed.replace("$(lang)", PLSLoginFunc::getCurrentLocaleShort().toUtf8()));
	} else {
		root.insert("code", code);
		root.insert("path", path);
		root.insert("scope", query.queryItemValue("scope", QUrl::FullyDecoded));

		static QByteArray bodyOk;
		if (bodyOk.isEmpty()) {
			QFile fileOk(htmlPath.arg("login.html"));
			if (fileOk.open(QIODevice::ReadOnly)) {
				bodyOk = fileOk.readAll();
				fileOk.close();
			}
		}

		tcpClient->write(bodyOk.replace("$(lang)", PLSLoginFunc::getCurrentLocaleShort().toUtf8()));
	}

	callback(addr, root);
}

void PLSSelectLoginPlatformView::initUi()
{
	ui->setupUi(this);
	ui->snsLoginButtonLayout->setAlignment(Qt::AlignTop);
	if (pls_get_gcc() != common::HTTP_GCC_KR) {
		auto index = m_specialLoginList.indexOf(PRISMLOGINTYPE::LINE);
		m_specialLoginList.replace(index, PRISMLOGINTYPE::NAVER);
	}
	m_snsLoginInfo = getLoginParamInfo();
	for (auto loginParam : m_snsLoginInfo) {
		bool isSpecial = isSpecialLogin(static_cast<PRISMLOGINTYPE>(loginParam.second.second));
		if (!isSpecial) {
			QPushButton *button = creatSnsLoginBtn(loginParam.first, loginParam.second.first, false);
			ui->snsLoginButtonLayout->addWidget(button);
			snsLoginHandler(loginParam.second.second, button);
		}
	}
	for (auto specialLogin : m_specialLoginList) {
		auto loginName = getmoduleName(static_cast<int>(specialLogin));
		QPushButton *button = creatSnsLoginBtn(loginName, {}, true);
		button->setToolTip(loginName);
		pls_uistep_v2_set_custom_enter_leave_name(button, loginName.toUtf8());
		ui->specialLoginLayout->addWidget(button);
		snsLoginHandler(static_cast<int>(specialLogin), button);
	}
	pls_async_call_mt(this, [this]() {
		auto w = LoginCommonHelpers::getWidgetFromStackWidget(m_loginStackFrame, LOGIN_WITH_EMAIL_VIEW);
		if (LoginWithEmailView *widget = nullptr; widget = dynamic_cast<LoginWithEmailView *>(w)) {
			connect(widget, &LoginWithEmailView::getNCB2BAuthUrl, this, [this](const QString &authUrl) {
				getAuthUrlCallback(authUrl, NCB2B, static_cast<int>(PRISMLOGINTYPE::NAVER_Cloud_B2B), static_cast<qint32>(PRISMLOGINTYPE::NAVER_Cloud_B2B));
			});
		}
	});

	if (QScrollBar *vs = ui->platformButtonsScrollArea->verticalScrollBar()) {
		connect(vs, &QAbstractSlider::valueChanged, this, [this]() { updateRecentBadge(); });
	}
	ensureRecentBadge();

	ui->loginContactUsBtn->setText(QTStr("login.footer.contact.us"));
	ui->horizontalLayout_6->setAlignment(Qt::AlignVCenter);
}

QPushButton *PLSSelectLoginPlatformView::creatSnsLoginBtn(const QString &prismLoginName, const QJsonObject &loginAttri, bool isSignUp)
{

	QPushButton *button = nullptr;
	auto objName = prismLoginName.toLower();
	if (isSignUp) {
		button = pls_new<QPushButton>();
		pls_uistep_v2_set_value(button, "*", prismLoginName);
		button->setObjectName("signBtn");
		button->setIcon(QIcon(getLoginIcon(objName, loginAttri.value("signupButtonIcon").toString())));
		auto size = 30;
		if (loginAttri.contains(LOGIN_DEFAULT_ICON_SIZE)) {
			size = loginAttri.value(LOGIN_DEFAULT_ICON_SIZE).toInt();
		}
		button->setIconSize({size, size});
	} else {
		button = pls_new<LoginCustomButton>();
		static_cast<LoginCustomButton *>(button)->setButtonPicture(getLoginIcon(objName, loginAttri.value("loginButtonIcon").toString()));
		button->setText(prismLoginName);
	}
	button->setProperty("name", prismLoginName);
	return button;
}

void PLSSelectLoginPlatformView::snsLoginHandler(int type, QPushButton *button)
{
	button->setProperty("prismLoginType", type);
	connect(button, &LoginCustomButton::clicked, this, [this, type, buttonPointer = QPointer<QPushButton>(button)]() {
		PLS_UI_STEP(LAUNCHER_LOGIN, (buttonPointer->text() + " Login Button").toUtf8().constData(), ACTION_CLICK);
		auto name = getmoduleName(type);
		PLS_UI_STEP(LAUNCHER_LOGIN, name.toUtf8().constData(), ACTION_CLICK);

		bool isSuccess = false;
		if (!pls::NetworkState::instance()->isAvailable()) {
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LOGIN_CHECK_NOTE_NETWORK, PLSErrKeyAllAlert, {},
							      PLSErrorHandler::ExtraData("PLSSelectLoginPlatformView::snsLoginHandler"));
			return;
		}
		const QString loginName = buttonPointer->property("name").toString();
		const qint32 recentKind = type;
		if (type == static_cast<int>(PRISMLOGINTYPE::Google) || type == static_cast<int>(PRISMLOGINTYPE::Apple) || type == static_cast<int>(PRISMLOGINTYPE::Facebook)) {
			loginWithAccountAsync(
				[this, buttonPointer](bool ok, const auto &) {
					if (buttonPointer) {
						buttonPointer->blockSignals(false);
						loginResultHandler(m_loginStackFrame, ok);
					}
				},
				type, loginName, recentKind, buttonPointer, this);
		} else if (type == static_cast<int>(PRISMLOGINTYPE::NAVER_Cloud_B2B)) {
			selectB2BLogin(type, loginName, recentKind);
		} else {
			buttonPointer->blockSignals(true);
			isSuccess = loginWithAccount(type, loginName, recentKind, this);
			if (buttonPointer) {
				if (!isSuccess) {
					buttonPointer->blockSignals(false);
					del_pannel_cookies(name);
				}
				loginResultHandler(m_loginStackFrame, isSuccess);
			} else {
				del_pannel_cookies(name);
			}
		}
	});
}

void PLSSelectLoginPlatformView::on_defaultLoginWithEmailBtn_clicked()
{
	PLS_UI_STEP(LAUNCHER_LOGIN, " Login with email Button", ACTION_CLICK);
	if (pls::NetworkState::instance()->isAvailable()) {
		LoginCommonHelpers::setCurrentStackWidget(m_loginStackFrame, LOGIN_WITH_EMAIL_VIEW);
		auto w = LoginCommonHelpers::setCurrentStackWidget(m_loginStackFrame, LOGIN_WITH_EMAIL_VIEW);
		if (LoginWithEmailView *widget = nullptr; widget = dynamic_cast<LoginWithEmailView *>(w)) {
			widget->initUi();
		}
		PLSLoginMainView::instance()->setWindowTitle(tr("login.login.with.email"));
	} else {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LOGIN_CHECK_NOTE_NETWORK, PLSErrKeyAllAlert, {},
						      PLSErrorHandler::ExtraData("PLSSelectLoginPlatformView::on_defaultLoginWithEmailBtn_clicked"));
	}
	PLS_UI_ACTION("show login with email view");
}

void PLSSelectLoginPlatformView::on_defaultSignupBtn_clicked()
{
	PLS_UI_STEP(LAUNCHER_LOGIN, " Signup email Button", ACTION_CLICK);

	if (!pls::NetworkState::instance()->isAvailable()) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LOGIN_CHECK_NOTE_NETWORK, PLSErrKeyAllAlert, {},
						      PLSErrorHandler::ExtraData("PLSSelectLoginPlatformView::on_defaultSignupBtn_clicked"));
		return;
	}

	if (!PLSTermsOfAgreeView::showTermsDialog()) {
		return;
	}

	LoginCommonHelpers::setCurrentStackWidget(m_loginStackFrame, SIGNUP_WITH_EMAIL_VIEW);
	PLSLoginMainView::instance()->setWindowTitle(tr("login.sign.up.with.email"));
	PLS_UI_ACTION("show sign up with email view");
}

void PLSSelectLoginPlatformView::on_loginContactUsBtn_clicked()
{
	PLSContactView contactView(QString(), QString(), QString(), PLSLoginMainView::instance());
	contactView.exec();
}

void PLSSelectLoginPlatformView::loginResultHandler(const QStackedWidget *stackWidget, bool isSuccess)
{
	if (!isSuccess || !stackWidget) {
		return;
	}

	QDialog *dialog = nullptr;
	pls_used(dialog);
	for (QWidget *parent = stackWidget->parentWidget(); parent && !dialog;) {
		dialog = dynamic_cast<QDialog *>(parent);
		parent = parent->parentWidget();
	}
	if (dialog) {
		dialog->done(QDialog::Accepted);
	}
}

QString PLSSelectLoginPlatformView::getLoginIcon(const QString &logingName, const QString &iconPath)
{
	auto iconDefaultPath = QString(":/resource/images/prism-login/login-default.svg");
	auto loginIconPath = pls_get_user_path(CONFIGS_LIBRARY_POLICY_PATH + iconPath);
	if (QFile::exists(loginIconPath) && !iconPath.isEmpty()) {
		return QDir::fromNativeSeparators(loginIconPath);
	}
	auto loginResIconPath = QString(":/resource/images/prism-login/login-%1.svg").arg(logingName);
	if (QFile::exists(loginResIconPath)) {
		return loginResIconPath;
	}
	return iconDefaultPath;
}

QList<QPair<QString, QPair<QJsonObject, int>>> PLSSelectLoginPlatformView::getLoginParamInfo()
{
	static QList<QPair<QString, QPair<QJsonObject, int>>> loginParams{};
	if (!loginParams.isEmpty()) {
		return loginParams;
	}
	QMetaEnum metaEnum = QMetaEnum::fromType<PRISMLOGINTYPE>();
	auto loginList = PLSGpopData::instance()->getLoginList();
	QJsonObject loginConfigObj = PLSSyncServerManager::instance()->getLoginObject();

	for (auto loginName : loginList) {
		QPair<QJsonObject, int> loginAttr;
		auto tmpLoginName = loginName;
		loginAttr.second = metaEnum.keyToValue(tmpLoginName.replace(' ', '_').toUtf8().constData());

		if (loginConfigObj.find(loginName) != loginConfigObj.end()) {
			loginAttr.first = loginConfigObj.value(loginName).toObject();
			auto platformName = loginAttr.first.value("platform").toString().trimmed();
			if (platformName.isEmpty()) {
				platformName = loginName;
			}
			auto index = metaEnum.keyToValue(platformName.replace(' ', '_').toUtf8().constData());
			loginAttr.second = index;
		}
		loginParams.append({loginName, loginAttr});
	}
	return loginParams;
}

QPair<QJsonObject, int> PLSSelectLoginPlatformView::getLoginParam(const QString &loginName)
{
	for (auto param : m_snsLoginInfo) {
		if (0 == loginName.compare(param.first, Qt::CaseInsensitive)) {
			return param.second;
		}
	}
	return {};
}

void PLSSelectLoginPlatformView::ensureRecentBadge()
{
	if (m_recentBadge) {
		return;
	}
	m_recentBadge = new PLSRecentLoginBadge(this);
	m_recentBadge->setObjectName(QStringLiteral("plsRecentLoginBadge"));
	m_recentBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
	m_recentBadge->hide();
	m_recentBadge->setBadgeText(tr("login.recent.badge"));
}

QWidget *PLSSelectLoginPlatformView::anchorWidgetForRecentKind(qint32 kind) const
{
	if (kind == PLSRecentLoginStore::KindEmail) {
		return ui->defaultLoginWithEmailBtn;
	}
	if (QWidget *w = findChildButtonWithPrismLoginType(ui->snsLoginButtonLayout, kind)) {
		return w;
	}
	return findChildButtonWithPrismLoginType(ui->specialLoginLayout, kind);
}

void PLSSelectLoginPlatformView::updateRecentBadge()
{
	ensureRecentBadge();
	qint32 kind = 0;
	qint64 epoch = 0;
	PLSRecentLoginStore::read(kind, epoch);
	if (PLSRecentLoginStore::isStaleOrInvalid(kind, epoch)) {
		m_recentBadge->hide();
		return;
	}
	QWidget *anchor = anchorWidgetForRecentKind(kind);
	if (!anchor) {
		m_recentBadge->hide();
		return;
	}
	const int badgeWidth = m_recentBadge->width();
	const int badgeHeight = m_recentBadge->height();
	const QPoint topRight = anchor->mapTo(this, anchor->rect().topRight());
	// UI spec: bubble right margin to target button = 5px; overlap with button top area = 13px.
	const int x = topRight.x() - badgeWidth + plsRecentBadgeRightMargin;
	const int y = topRight.y() - badgeHeight + plsRecentBadgeOverlapHeight;
	m_recentBadge->move(x, y);
	m_recentBadge->raise();
	m_recentBadge->show();
}

void PLSSelectLoginPlatformView::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	QTimer::singleShot(0, this, [this]() { updateRecentBadge(); });
}

void PLSSelectLoginPlatformView::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	updateRecentBadge();
}
