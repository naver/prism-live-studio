#include "PLSNetworkMonitorTool.hpp"
#include "ui_PLSNetworkMonitorTool.h"

#include <libhttp-client.h>
#include <liblog.h>

#include <qjsondocument.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>
#include <qscrollbar.h>
#include <qmetaobject.h>

static void clearTable(QTableWidget *table)
{
	while (table->rowCount() > 0)
		table->removeRow(0);
}

static QString getStatus(const pls::http::Reply &reply)
{
	if (reply.isOk()) {
		return QStringLiteral("Ok");
	} else if (reply.isFailed()) {
		return QStringLiteral("Failed");
	} else if (reply.isTimeout()) {
		return QStringLiteral("Timeout");
	} else if (reply.isAborted()) {
		return QStringLiteral("Aborted");
	} else if (reply.isRenameFailed()) {
		return QStringLiteral("RenameFailed");
	} else {
		return QStringLiteral("");
	}
}

bool PLSNetworkMonitorTool::Data::bodyIsText() const
{
	if (contentType.contains(QStringLiteral("application/json")) || contentType.contains(QStringLiteral("text/html")) || contentType.contains(QStringLiteral("text/plain")) ||
	    contentType.contains(QStringLiteral("application/javascript"))) {
		return true;
	}
	return false;
}

bool PLSNetworkMonitorTool::Data::bodyIsJson() const
{
	return contentType.contains(QStringLiteral("application/json"));
}

PLSNetworkMonitorTool::PLSNetworkMonitorTool(QWidget *parent) : PLSToolView<PLSNetworkMonitorTool>(nullptr, Qt::Window), ui(new Ui::PLSNetworkMonitorTool)
{
	setStyleSheet("QTableView, QTreeView { background-color: #2C2C2C; color: white; }"
		      "QTableView QTableCornerButton::section, QTreeView  { background-color: #272727; }"
		      "QSplitter::handle { background-color: #1E1E1F; }"
		      "QTextEdit { background-color: #2C2C2C; }");
	setupUi(ui);
	setHasMinButton(true);
	setHasMaxResButton(true);
	initSize(1600, 800);
	ui->startStopButton->setText("Start");
	ui->requestID->addItem(QStringLiteral("All"));
	ui->requestID->addItems(pls::http::requestIds());
	ui->splitter->setSizes(QList<int>() << 1000 << 3000);
	ui->splitter_2->setSizes(QList<int>() << 3000 << 3000);
	ui->requestID->setCurrentText("All");

	auto vsb = ui->requests->verticalScrollBar();
	connect(vsb, &QScrollBar::valueChanged, this, [vsb, this](int value) {
		if (!m_scrollToBottom)
			m_autoScrollToBottom = value >= vsb->maximum();
	});
}

PLSNetworkMonitorTool::~PLSNetworkMonitorTool()
{
	delete ui;
}

void PLSNetworkMonitorTool::start(const QString &id, const QString &filter)
{
	if (!m_monitoring) {
		m_monitoring = true;
		ui->startStopButton->setText("Stop");

		m_id = id;
		m_filter = filter;
		pls::http::monitor(m_id != QStringLiteral("All") ? m_id : QString(), [mt = pls::QObjectPtr<QObject>(this), this](const pls::http::Request &request, const pls::http::Reply &reply) {
			auto req = std::make_shared<Request>();
			req->statusCode = reply.statusCode();
			req->url = request.url();
			req->method = request.method().toUpper();
			req->id = request.id();
			req->host = req->url.host();
			req->path = req->url.path();
			req->startTime = request.startTime();
			req->endTime = request.endTime();
			req->status = getStatus(reply);
			req->qterror = reply.error();
			req->errors = reply.errors();

			req->request.contentType = reply.requestContentType();
			pls_for_each(reply.requestRawHeaders(), [req](const QNetworkReply::RawHeaderPair &header) { req->request.rawHeaders.insert(header.first, header.second); });
			req->request.body = request.body();

			req->reply.contentType = reply.contentType();
			pls_for_each(reply.replyRawHeaders(), [req](const QNetworkReply::RawHeaderPair &header) { req->reply.rawHeaders.insert(header.first, header.second); });
			req->reply.body = reply.data();
			if (request.forDownload() && reply.isDownloadOk()) {
				req->fileSize = reply.downloadTotalBytes();
				req->filePath = reply.downloadFilePath();
			}
			pls_async_call(mt, [req, this]() { addRow(req); });
		});
	}
}

void PLSNetworkMonitorTool::stop()
{
	if (m_monitoring) {
		m_monitoring = false;
		ui->startStopButton->setText("Start");
		pls::http::unmonitor(m_id != QStringLiteral("All") ? m_id : QString());
	}
}

void PLSNetworkMonitorTool::addRow(std::shared_ptr<Request> req)
{
	m_reqs.append(req);
	showRow(req);
}

void PLSNetworkMonitorTool::showRow(std::shared_ptr<Request> req)
{
	if (!(m_filter.isEmpty() || req->path.contains(m_filter)))
		return;

	int row = ui->requests->rowCount();
	ui->requests->insertRow(row);

	auto item = pls_new<QTableWidgetItem>(QString::number(req->statusCode));
	item->setData(Qt::UserRole, QVariant::fromValue<void *>(req.get()));
	ui->requests->setItem(row, 0, item);
	item = pls_new<QTableWidgetItem>(req->method);
	ui->requests->setItem(row, 1, item);
	item = pls_new<QTableWidgetItem>(req->id);
	ui->requests->setItem(row, 2, item);
	item = pls_new<QTableWidgetItem>(req->host);
	ui->requests->setItem(row, 3, item);
	item = pls_new<QTableWidgetItem>(req->path);
	ui->requests->setItem(row, 4, item);
	item = pls_new<QTableWidgetItem>(req->startTime.toString(QStringLiteral("hh:mm:ss.zzz")));
	ui->requests->setItem(row, 5, item);
	item = pls_new<QTableWidgetItem>(req->endTime.toString(QStringLiteral("hh:mm:ss.zzz")));
	ui->requests->setItem(row, 6, item);
	item = pls_new<QTableWidgetItem>(QStringLiteral("%1ms").arg(req->startTime.msecsTo(req->endTime)));
	ui->requests->setItem(row, 7, item);
	item = pls_new<QTableWidgetItem>(req->status);
	ui->requests->setItem(row, 8, item);

	if (m_autoScrollToBottom) {
		m_scrollToBottom = true;
		ui->requests->scrollToBottom();
		m_scrollToBottom = false;
	}
}

void PLSNetworkMonitorTool::setHeaders(QTableWidget *table, const QMap<QByteArray, QByteArray> &rawHeaders)
{
	clearTable(table);

	for (auto i = rawHeaders.begin(), end = rawHeaders.end(); i != end; ++i) {
		int row = table->rowCount();
		table->insertRow(row);

		auto item = pls_new<QTableWidgetItem>(QString::fromUtf8(i.key()));
		item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
		table->setItem(row, 0, item);
		item = pls_new<QTableWidgetItem>(QString::fromUtf8(i.value()));
		table->setItem(row, 1, item);
	}

	table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
}

void PLSNetworkMonitorTool::setText(QTextEdit *text, const QByteArray &body)
{
	text->setPlainText(QString::fromUtf8(body));
}
void PLSNetworkMonitorTool::setJSON(QTreeWidget *tree, const QByteArray &body)
{
	tree->clear();

	QJsonParseError error;
	auto doc = QJsonDocument::fromJson(body, &error);
	if (error.error != QJsonParseError::NoError || doc.isEmpty())
		return;

	if (doc.isObject()) {
		auto obj = doc.object();
		auto item = pls_new<QTreeWidgetItem>(QStringList() << QStringLiteral("") << QStringLiteral("Object") << QString());
		tree->addTopLevelItem(item);
		setJSON(tree, item, obj);
	} else {
		auto arr = doc.array();
		auto item = pls_new<QTreeWidgetItem>(QStringList() << QStringLiteral("") << QStringLiteral("Array") << QString());
		tree->addTopLevelItem(item);
		setJSON(tree, item, arr);
	}

	tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
}

void PLSNetworkMonitorTool::setJSON(QTreeWidget *tree, QTreeWidgetItem *parent, const QString &name, const QJsonValue &val)
{
	if (val.isNull()) {
		auto item = pls_new<QTreeWidgetItem>(QStringList() << name << QStringLiteral("Null") << QStringLiteral("Null"));
		parent->addChild(item);
	} else if (val.isBool()) {
		auto item = pls_new<QTreeWidgetItem>(QStringList() << name << QStringLiteral("Bool") << (val.toBool() ? QStringLiteral("true") : QStringLiteral("false")));
		parent->addChild(item);
	} else if (val.isDouble()) {
		auto item = pls_new<QTreeWidgetItem>(QStringList() << name << QStringLiteral("Double") << QString::number(val.toDouble(), 'g', 30));
		parent->addChild(item);
	} else if (val.isString()) {
		auto item = pls_new<QTreeWidgetItem>(QStringList() << name << QStringLiteral("String") << val.toString());
		parent->addChild(item);
	} else if (val.isArray()) {
		auto item = pls_new<QTreeWidgetItem>(QStringList() << name << QStringLiteral("Array") << QString());
		parent->addChild(item);
		setJSON(tree, item, val.toArray());
	} else if (val.isObject()) {
		auto item = pls_new<QTreeWidgetItem>(QStringList() << name << QStringLiteral("Object") << QString());
		parent->addChild(item);
		setJSON(tree, item, val.toObject());
	} else if (val.isUndefined()) {
		auto item = pls_new<QTreeWidgetItem>(QStringList() << name << QStringLiteral("Undefined") << QStringLiteral("Undefined"));
		parent->addChild(item);
	}
}

void PLSNetworkMonitorTool::setJSON(QTreeWidget *tree, QTreeWidgetItem *parent, const QJsonObject &obj)
{
	for (auto i = obj.begin(), end = obj.end(); i != end; ++i) {
		setJSON(tree, parent, i.key(), i.value());
	}
	parent->setExpanded(true);
}

void PLSNetworkMonitorTool::setJSON(QTreeWidget *tree, QTreeWidgetItem *parent, const QJsonArray &arr)
{
	for (qsizetype i = 0, size = arr.size(); i < size; ++i) {
		setJSON(tree, parent, QString(), arr[i]);
	}
	parent->setExpanded(true);
}

void PLSNetworkMonitorTool::on_clearButton_clicked()
{
	clearTable(ui->requests);

	ui->overview->clear();

	clearTable(ui->requestHeaders);
	ui->requestBodyText->clear();
	ui->requestBodyJson->clear();

	clearTable(ui->replyHeaders);
	ui->replyBodyText->clear();
	ui->replyBodyJson->clear();

	m_reqs.clear();
}

void PLSNetworkMonitorTool::on_startStopButton_clicked()
{
	if (!m_monitoring) {
		start(ui->requestID->lineEdit()->text(), ui->filter->text());
	} else {
		stop();
	}
}

void PLSNetworkMonitorTool::on_requests_currentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn)
{
	if (currentRow < 0)
		return;

	auto item = ui->requests->item(currentRow, 0);
	auto req = (Request *)item->data(Qt::UserRole).value<void *>();

	QString overview;
	overview.append(QStringLiteral("Method: %1\n").arg(req->method));
	overview.append(QStringLiteral("URL: %1\n").arg(req->url.toString(QUrl::FullyEncoded)));
	overview.append(QStringLiteral("Status Code: %1\n").arg(req->statusCode));
	overview.append(QStringLiteral("Status: %1").arg(req->status));
	if (req->qterror != QNetworkReply::NoError)
		overview.append(QStringLiteral("Qt Error: %1").arg(QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(req->qterror)));
	if (!req->errors.isEmpty())
		overview.append(QStringLiteral("Qt Errors: %1").arg(req->errors));
	if (!req->filePath.isEmpty()) {
		overview.append(QStringLiteral("File Size: %1").arg(req->fileSize));
		overview.append(QStringLiteral("File Path: %1").arg(req->filePath));
	}
	ui->overview->setPlainText(overview);

	setHeaders(ui->requestHeaders, req->request.rawHeaders);
	if (req->request.bodyIsText()) {
		setText(ui->requestBodyText, req->request.body);
	}
	if (req->request.bodyIsJson()) {
		setJSON(ui->requestBodyJson, req->request.body);
	}

	setHeaders(ui->replyHeaders, req->reply.rawHeaders);
	if (req->reply.bodyIsText()) {
		setText(ui->replyBodyText, req->reply.body);
	}
	if (req->reply.bodyIsJson()) {
		setJSON(ui->replyBodyJson, req->reply.body);
	}
}

void PLSNetworkMonitorTool::on_filter_textChanged(const QString &text)
{
	m_filter = text;

	clearTable(ui->requests);

	ui->overview->clear();

	clearTable(ui->requestHeaders);
	ui->requestBodyText->clear();
	ui->requestBodyJson->clear();

	clearTable(ui->replyHeaders);
	ui->replyBodyText->clear();
	ui->replyBodyJson->clear();

	for (auto req : m_reqs)
		showRow(req);
}

void PLSNetworkMonitorTool::on_requestID_currentTextChanged(const QString &text)
{
	if (m_monitoring) {
		stop();
		start(text, ui->filter->text());
	}
}
