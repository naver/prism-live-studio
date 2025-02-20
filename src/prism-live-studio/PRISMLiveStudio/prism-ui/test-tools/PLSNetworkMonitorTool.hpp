#ifndef PLSNETWORKMONITORTOOL_HPP
#define PLSNETWORKMONITORTOOL_HPP

#include "PLSToolView.hpp"

#include <qnetworkrequest.h>
#include <qnetworkreply.h>

namespace Ui {
class PLSNetworkMonitorTool;
}

class QJsonArray;
class QJsonObject;
class QTableWidget;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

class PLSNetworkMonitorTool : public PLSToolView<PLSNetworkMonitorTool> {
	Q_OBJECT

	struct Data {
		QString contentType;
		QMap<QByteArray, QByteArray> rawHeaders;
		QMap<QNetworkRequest::Attribute, QVariant> attributes;
		QByteArray body;

		bool bodyIsText() const;
		bool bodyIsJson() const;
	};
	struct Request {
		QUrl url;
		QString method;
		QString id;
		QString host;
		QString path;
		QString status;
		QDateTime startTime;
		QDateTime endTime;
		int statusCode = 0;
		QNetworkReply::NetworkError qterror = QNetworkReply::NoError;
		qint64 fileSize = 0;
		QString errors;
		QString filePath;
		Data request;
		Data reply;
	};

public:
	explicit PLSNetworkMonitorTool(QWidget *parent = nullptr);
	~PLSNetworkMonitorTool();

private:
	void start(const QString &id, const QString &filter);
	void stop();
	void addRow(std::shared_ptr<Request> request);
	void showRow(std::shared_ptr<Request> request);
	void setHeaders(QTableWidget *table, const QMap<QByteArray, QByteArray> &rawHeaders);
	void setText(QTextEdit *text, const QByteArray &body);
	void setJSON(QTreeWidget *tree, const QByteArray &body);
	void setJSON(QTreeWidget *tree, QTreeWidgetItem *parent, const QString &name, const QJsonValue &val);
	void setJSON(QTreeWidget *tree, QTreeWidgetItem *parent, const QJsonObject &obj);
	void setJSON(QTreeWidget *tree, QTreeWidgetItem *parent, const QJsonArray &arr);

private slots:
	void on_clearButton_clicked();
	void on_startStopButton_clicked();
	void on_requests_currentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn);
	void on_filter_textChanged(const QString &text);
	void on_requestID_currentTextChanged(const QString &text);

private:
	Ui::PLSNetworkMonitorTool *ui;
	bool m_scrollToBottom = true;
	bool m_autoScrollToBottom = true;
	bool m_monitoring = false;
	QString m_id;
	QString m_filter;
	QList<std::shared_ptr<Request>> m_reqs;
};

#endif // PLSNETWORKMONITORTOOL_HPP
