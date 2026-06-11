#ifndef PLSNETWORKHOOKTOOL_HPP
#define PLSNETWORKHOOKTOOL_HPP

#include "PLSToolView.hpp"

#include <libhttp-client.h>

namespace Ui {
class PLSNetworkHookTool;
}

class ReplyHook;

class PLSNetworkHookTool : public PLSToolView<PLSNetworkHookTool> {
	Q_OBJECT

public:
	explicit PLSNetworkHookTool(QWidget *parent = nullptr);
	~PLSNetworkHookTool();

public:
	QVariant getKnownHeader(QNetworkRequest::KnownHeaders header) const;
	QList<QNetworkReply::RawHeaderPair> replyRawHeaders() const;

private slots:
	void on_browseButton_clicked();
	void on_hookButton_clicked();
	void on_unhookButton_clicked();
	void on_closeButton_clicked();

private:
	Ui::PLSNetworkHookTool *ui;
	QString m_id;
	QString m_path;
	int m_delay = 0;
	int m_statusCode = 200;
	pls::http::Status m_status = pls::http::Status::Ok;
	QNetworkReply::NetworkError m_error = QNetworkReply::NoError;
	QString m_contentType;
	std::map<QByteArray, QByteArray> m_otherHeaders;
	std::optional<QByteArray> m_data;
	std::optional<QString> m_filePath;

	friend class ReplyHook;
};

#endif // PLSNETWORKHOOKTOOL_HPP
