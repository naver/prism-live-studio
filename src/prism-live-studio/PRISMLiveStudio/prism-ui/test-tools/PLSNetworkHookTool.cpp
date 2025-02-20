#include "PLSNetworkHookTool.hpp"
#include "ui_PLSNetworkHookTool.h"

#include <libui.h>

#include <qmetaobject.h>
#include <qfiledialog.h>

class ReplyHook : public pls::http::IReplyHook {
	QPointer<PLSNetworkHookTool> m_nrh;

public:
	ReplyHook(const pls::http::Request &request) : IReplyHook(request), m_nrh(PLSNetworkHookTool::instance()) {}

	int triggerDelay() const override { return m_nrh ? m_nrh->m_delay : 0; }

	// QUrl url() const override {}
	int statusCode() const override { return m_nrh ? m_nrh->m_statusCode : 200; }

	pls::http::Status status() const override { return m_nrh ? m_nrh->m_status : pls::http::Status::Ok; }

	QNetworkReply::NetworkError error() const override { return m_nrh ? m_nrh->m_error : QNetworkReply::NoError; }
	// QString errorString() const override { }

	QVariant header(QNetworkRequest::KnownHeaders header) const override { return m_nrh ? m_nrh->getKnownHeader(header) : QVariant(); }
	bool hasRawHeader(const QByteArray &header) const override { return m_nrh ? (m_nrh->m_otherHeaders.find(header) != m_nrh->m_otherHeaders.end()) : false; }
	QByteArray rawHeader(const QByteArray &header) const override { return m_nrh ? pls_get_value(m_nrh->m_otherHeaders, header, QByteArray()) : QByteArray(); }
	QList<QNetworkReply::RawHeaderPair> replyRawHeaders() const override { return m_nrh ? m_nrh->replyRawHeaders() : QList<QNetworkReply::RawHeaderPair>(); }

	std::optional<QByteArray> data() const override { return m_nrh ? m_nrh->m_data : std::nullopt; }
	std::optional<QString> filePath() const override { return m_nrh ? m_nrh->m_filePath : std::nullopt; }
};

PLSNetworkHookTool::PLSNetworkHookTool(QWidget *parent) : PLSToolView<PLSNetworkHookTool>(nullptr, Qt::Window), ui(new Ui::PLSNetworkHookTool)
{
	setupUi(ui);
	setHasMinButton(true);
	setHasMaxResButton(true);
	initSize(1024, 768);
	ui->unhookButton->setEnabled(false);
	ui->requestID->addItem(QStringLiteral("All"));
	ui->requestID->addItems(pls::http::requestIds());
#define AddStatus(name) ui->status->addItem(#name, static_cast<int>(pls::http::Status::name))
	AddStatus(Ok);
	AddStatus(Failed);
	AddStatus(Timeout);
	AddStatus(Aborted);
#undef AddStatus
#define AddQtError(name) ui->qtError->addItem(#name, static_cast<int>(QNetworkReply::name))
	AddQtError(NoError);
	AddQtError(ConnectionRefusedError);
	AddQtError(RemoteHostClosedError);
	AddQtError(HostNotFoundError);
	AddQtError(TimeoutError);
	AddQtError(OperationCanceledError);
	AddQtError(SslHandshakeFailedError);
	AddQtError(TemporaryNetworkFailureError);
	AddQtError(NetworkSessionFailedError);
	AddQtError(BackgroundRequestNotAllowedError);
	AddQtError(TooManyRedirectsError);
	AddQtError(InsecureRedirectError);
	AddQtError(UnknownNetworkError);
	AddQtError(ProxyConnectionRefusedError);
	AddQtError(ProxyConnectionClosedError);
	AddQtError(ProxyNotFoundError);
	AddQtError(ProxyTimeoutError);
	AddQtError(ProxyAuthenticationRequiredError);
	AddQtError(UnknownProxyError);
	AddQtError(ContentAccessDenied);
	AddQtError(ContentOperationNotPermittedError);
	AddQtError(ContentNotFoundError);
	AddQtError(AuthenticationRequiredError);
	AddQtError(ContentReSendError);
	AddQtError(ContentConflictError);
	AddQtError(ContentGoneError);
	AddQtError(UnknownContentError);
	AddQtError(ProtocolUnknownError);
	AddQtError(ProtocolInvalidOperationError);
	AddQtError(ProtocolFailure);
	AddQtError(InternalServerError);
	AddQtError(OperationNotImplementedError);
	AddQtError(ServiceUnavailableError);
	AddQtError(UnknownServerError);
#undef AddQtError
	ui->contentType->addItems(QStringList() << "application/json"
						<< "application/json;charset=UTF-8"
						<< "application/javascript"
						<< "application/zip"
						<< "text/plain"
						<< "text/html"
						<< "image/png"
						<< "image/gif"
						<< "image/jpeg"
						<< "image/bmp"
						<< "image/webp"
						<< "image/tiff"
						<< "audio/mpeg");
	ui->requestID->setCurrentText("All");
}

PLSNetworkHookTool::~PLSNetworkHookTool()
{
	delete ui;
}

QVariant PLSNetworkHookTool::getKnownHeader(QNetworkRequest::KnownHeaders header) const
{
	switch (header) {
	case QNetworkRequest::ContentTypeHeader:
		return m_contentType;
	case QNetworkRequest::CookieHeader:
		break;
	default:
		break;
	}
	return QVariant();
}

QList<QNetworkReply::RawHeaderPair> PLSNetworkHookTool::replyRawHeaders() const
{
	QList<QNetworkReply::RawHeaderPair> headers;
	pls_for_each(m_otherHeaders, [&headers](const QByteArray &key, const QByteArray &value) { headers.append({key, value}); });
	headers.append({"Content-Type", m_contentType.toUtf8()});
	return headers;
}

void PLSNetworkHookTool::on_browseButton_clicked()
{
	QString dir;
	if (m_filePath)
		dir = QFileInfo(m_filePath.value()).filePath();

	auto filePath = QFileDialog::getOpenFileName(this, "Select File", dir);
	ui->filePath->setText(filePath);
}

void PLSNetworkHookTool::on_hookButton_clicked()
{
	on_unhookButton_clicked();

	ui->hookButton->setEnabled(false);
	ui->unhookButton->setEnabled(true);

	m_id = ui->requestID->lineEdit()->text();
	m_path = ui->path->text();
	m_delay = ui->delay->value();
	m_statusCode = ui->statusCode->value();
	m_status = static_cast<pls::http::Status>(ui->status->currentData().toInt());
	m_error = static_cast<QNetworkReply::NetworkError>(ui->qtError->currentData().toInt());
	m_contentType = ui->contentType->lineEdit()->text();
	m_otherHeaders.insert({"Content-Type", m_contentType.toUtf8()});
	if (auto otherHeaders = ui->otherHeaders->toPlainText(); !otherHeaders.isEmpty())
		if (QJsonObject obj; pls_parse_json(obj, otherHeaders.toUtf8()))
			for (auto i = obj.begin(), end = obj.end(); i != end; ++i)
				m_otherHeaders.insert({i.key().toUtf8(), i.value().toString().toUtf8()});
	if (auto data = ui->text->toPlainText(); !data.isEmpty())
		m_data = data.toUtf8();
	else
		m_data = std::nullopt;
	if (auto filePath = ui->filePath->text(); !filePath.isEmpty())
		m_filePath = filePath;
	else
		m_filePath = std::nullopt;

	pls::http::hook(m_id != QStringLiteral("All") ? m_id : QString(), [](const pls::http::Request &request) -> pls::http::IReplyHookPtr {
		if (auto tool = instance(); tool && (tool->m_path.isEmpty() || request.originalUrl().path().contains(tool->m_path)))
			return std::make_shared<ReplyHook>(request);
		return nullptr;
	});
}
void PLSNetworkHookTool::on_unhookButton_clicked()
{
	ui->hookButton->setEnabled(true);
	ui->unhookButton->setEnabled(false);

	if (!m_id.isEmpty()) {
		pls::http::unhook(m_id != QStringLiteral("All") ? m_id : QString());
		m_id.clear();
	}
}

void PLSNetworkHookTool::on_closeButton_clicked()
{
	hide();
}
