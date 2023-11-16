#ifndef GIPHYDEFINE_H
#define GIPHYDEFINE_H

#include <QObject>
#include <QSize>
#include <QVector>
#include <QNetworkReply>

static const int MAX_HISTORY_LIST_COUNT = 7;
static const int MAX_RECENT_STICKER_COUNT = 24;
static const int STICKER_DISPLAY_SIZE = 56;
constexpr auto FILE_THUMBNAIL = "thumbnail.gif";
constexpr auto FILE_ORIGINAL = "original.gif";
constexpr auto DEFAULT_KEY_WORD = "@PRISMLiveStudio";

enum class StickerDownloadType { THUMBNAIL, ORIGINAL };
Q_DECLARE_METATYPE(StickerDownloadType)

using PointerValue = unsigned long long;
Q_DECLARE_METATYPE(PointerValue)

enum class ResultStatus { GIPHY_NO_ERROR, ERROR_OCCUR };

enum class ErrorSubType { Error_Timeout, Error_Other };

Q_DECLARE_METATYPE(ResultStatus)

struct GiphyData {
	QString id;
	QString type;
	QString title;
	QString rating;
	QSize sizePreview;
	QSize sizeOriginal;
	QString previewUrl;
	QString originalUrl;
};

Q_DECLARE_METATYPE(GiphyData)

struct Meta {
	int status{0};
	QString msg;
	QString responId;
};

Q_DECLARE_METATYPE(Meta)

struct Pagination {
	int totalCount{0};
	int count{0};
	int offset{0};
};

Q_DECLARE_METATYPE(Pagination)

// request task struct for requesting a list of sticker.
struct RequestTaskData {
	QString keyword;
	QString url;
	PointerValue randomId{0LL};
};

Q_DECLARE_METATYPE(RequestTaskData)

// downloading task struct for requesting a sticker resource.
struct TaskResponData;
struct DownloadTaskData {
	QString uniqueId;
	QString url;
	QString outputPath;
	QString outputFileName;
	qint64 version;
	StickerDownloadType type = StickerDownloadType::THUMBNAIL;
	QSize SourceSize;
	PointerValue randomId{0LL};
	QVariant extraData = QVariant();
	bool needRetry{true};
	int timeoutMs = 0;
	std::function<void(const TaskResponData &)> callback;
	std::function<void(const QByteArray &, const TaskResponData &)> rawDataCallback;

	bool equals(const DownloadTaskData &other) const { return (this->uniqueId == other.uniqueId && this->url == other.url); }
};

Q_DECLARE_METATYPE(DownloadTaskData)

// respon struct data from a downloading task request.
struct TaskResponData {
	ResultStatus resultType = ResultStatus::GIPHY_NO_ERROR;
	DownloadTaskData taskData;
	QString fileName;
	QString errorString;
	ErrorSubType subType = ErrorSubType::Error_Other;
};

Q_DECLARE_METATYPE(TaskResponData)

// respon struct data from a request task.
struct ResponData {
	RequestTaskData task;
	QVector<GiphyData> giphyData{};
	Meta metaData;
	Pagination pageData;
};

Q_DECLARE_METATYPE(ResponData)

enum class RequestErrorType { RequstTimeOut, OtherNetworkError };
Q_DECLARE_METATYPE(RequestErrorType)

// request error struct
struct RequestErrorInfo {
	RequestErrorType errorType{RequestErrorType::OtherNetworkError};
	QNetworkReply::NetworkError networkError{QNetworkReply::NoError};
	QString errorText;
};
Q_DECLARE_METATYPE(RequestErrorInfo)

#endif // GIPHYDEFINE_H
