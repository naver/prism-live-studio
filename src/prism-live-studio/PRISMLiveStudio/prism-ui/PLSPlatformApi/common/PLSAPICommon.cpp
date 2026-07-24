#include "PLSAPICommon.h"
#include <qfileinfo.h>
#include <quuid.h>
#include <QNetworkCookieJar>
#include <vector>
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "PLSPlatformApi.h"
#include "PLSServerStreamHandler.hpp"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "PLSPlatformBase.hpp"
#include <ctime>
#include <string>
#include <iomanip>
#include <sstream>
#include <time.h>
#include "PLSDialogView.h"

using namespace common;
using namespace std;

static std::string http_gmtime()
{
	//e.g. : Sat, 22 Aug 2015 11 : 48 : 50 GMT
	time_t now = time(nullptr);
	struct tm timeinfo;

#if defined(Q_OS_WIN)
	localtime_s(&timeinfo, &now);
#elif defined(Q_OS_MACOS)
	localtime_r(&now, &timeinfo);
#endif

	const char *fmt = "%a, %d %b %Y %H:%M:%S GMT";
	std::ostringstream os;
	os << std::put_time(&timeinfo, fmt);
	return os.str();
}

void PLSAPICommon::downloadImageAsync(const QObject *receiver, const QString &imageUrl, const imageCallback &callback, bool ignoreCache, const QString &saveFilePath)
{
	if (imageUrl.isEmpty()) {
		PLS_INFO(MODULE_PlatformService, "%s url is empty", __FUNCTION__);
		callback(false, QString());
		return;
	}

	auto _request = pls::http::Request(pls::http::NoDefaultRequestHeaders);
	if (ignoreCache) {
		_request.ignoreCache();
		_request.rawHeader("Cache-Control", QString("no-store, max-age=0"));
		_request.rawHeader("If-Modified-Since", http_gmtime());
	}
	_request.method(pls::http::Method::Get)
		.url(imageUrl)
		.withLog(pls_masking_person_info(imageUrl))
		.receiver(receiver)
		.forDownload(true)
		.timeout(PRISM_NET_DOWNLOAD_TIMEOUT)
		.result([callback, receiver](const pls::http::Reply &reply) {
			if (callback) {
				pls_async_call(receiver, [callback, ok = reply.isDownloadOk(), path = reply.downloadFilePath()]() { callback(ok, path); });
			}
		});
	if (saveFilePath.isEmpty()) {
		_request.saveDir(getTmpCacheDir());
	} else {
		_request.saveFilePath(saveFilePath);
	}
	pls::http::request(_request);
}

void PLSAPICommon::maskingUrlKeys(const pls::http::Request &_request, const QStringList &keys)
{
	_request.withLog([keys](const QString &url) { return PLSAPICommon::maskingUrlKeys(url, keys); });
}

void PLSAPICommon::maskingAfterUrlKeys(const pls::http::Request &_request, const QStringList &keys)
{
	_request.withAfterLog([keys](const QString &url) { return PLSAPICommon::maskingUrlKeys(url, keys); }, pls::http::LogInclude::Fail);
}

QString PLSAPICommon::maskingUrlKeys(const QString &originUrl, const QStringList &keys)
{
	QString _urlString = originUrl;
	if (!keys.isEmpty()) {
		for (auto &key : keys) {
			_urlString.replace(key, pls_masking_person_info(key));
		}
	}
	return _urlString;
}

bool PLSAPICommon::isTokenValid(long timestamp)
{
	const static int expiresDiff = 3 * 60; //3min
	auto differ = PLSDateFormate::getNowTimeStamp();
	return ((timestamp - differ - expiresDiff) > 0);
}

QString PLSAPICommon::getMd5ImagePath(const QString &url)
{
	QString urlHash = QLatin1String(QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Md5).toHex());
	auto path = getTmpCacheDir() + "/" + urlHash;
	return path;
}

QString PLSAPICommon::getPairedString(const PLSAPICommon::privacyVec &pairs, const QString cmpStr, bool isCmpFirst, bool isCaseInsensitive)
{
	auto cmpFunc = [cmpStr, isCmpFirst, isCaseInsensitive](const auto &item) {
		const auto &thisItem = isCmpFirst ? item.first : item.second;
		return thisItem.compare(cmpStr, isCaseInsensitive ? Qt::CaseInsensitive : Qt::CaseSensitive) == 0;
	};
	auto itr = std::find_if(pairs.begin(), pairs.end(), cmpFunc);
	if (itr != pairs.end()) {
		return isCmpFirst ? itr->second : itr->first;
	}
	return QString();
}

void PLSAPICommon::downloadChannelImageAsync(const QString &platformName)
{
	auto infos = PLSCHANNELS_API->getChanelInfosByPlatformName(platformName, ChannelData::ChannelType);
	for (const auto &item : infos) {
		auto url = item.value(ChannelData::g_userProfileImg).toString();
		if (url.isEmpty())
			continue;

		QString localPath = item.value(ChannelData::g_userIconCachePath).toString();
		if (!localPath.isEmpty() && QFile(localPath).exists()) {
			continue;
		}
		QString uuid = item.value(ChannelData::g_channelUUID).toString();
		auto _callBack = [uuid](bool ok, const QString &imagePath) {
			if (ok) {
				if (PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_userIconCachePath, imagePath)) {
					PLSCHANNELS_API->channelModified(uuid);
				}
			} else {
				PLS_INFO(MODULE_PlatformService, "download user icon failed");
			}
		};

		PLSAPICommon::downloadImageAsync(pls_get_main_view(), url, _callBack, false);
	}
}

static int findLabelPosition(QLabel *targetLabel, QFormLayout *layout)
{
	for (int i = 0; i < layout->rowCount(); ++i) {
		QLayoutItem *labelItem = layout->itemAt(i, QFormLayout::LabelRole);
		if (labelItem && labelItem->widget() == targetLabel) {
			return i;
		}
	}
	return -1;
}

static int calculateWrappedLabelWidth(QLabel *label)
{
	QFont font = label->font();
	QFontMetrics fontMetrics(font);
	QString text = label->text();
	int maxWidth = label->width();

	QTextLayout textLayout(text, font);
	textLayout.beginLayout();
	qreal totalWidth = 0;
	while (true) {
		QTextLine line = textLayout.createLine();
		if (!line.isValid())
			break;

		line.setLineWidth(maxWidth);
		line.setPosition(QPointF(0, 0));

		qreal lineWidth = line.naturalTextWidth();
		if (lineWidth > totalWidth) {
			totalWidth = lineWidth;
		}
	}
	textLayout.endLayout();
	return static_cast<int>(totalWidth);
}

void PLSAPICommon::createHelpIconWidget(QLabel *originalLabel, const QString &tooltip, QFormLayout *formLayout, PLSDialogView *sender)
{
	auto idx = findLabelPosition(originalLabel, formLayout);
	if (idx == -1) {
		return;
	}
	auto oldParent = originalLabel->parentWidget();
	originalLabel->setParent(nullptr);

	QWidget *formLeftWidget = new QWidget(oldParent);
	formLeftWidget->setObjectName("formLeftWidget");
	QHBoxLayout *horizontalLayout = new QHBoxLayout(formLeftWidget);
	horizontalLayout->setSpacing(0);
	horizontalLayout->setContentsMargins(0, 0, 23, 0);

	originalLabel->setIndent(-1);
	originalLabel->setWordWrap(true);
	originalLabel->setObjectName("formLeftWidget_title");

	QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	sizePolicy.setHorizontalStretch(0);
	sizePolicy.setVerticalStretch(0);
	sizePolicy.setHeightForWidth(originalLabel->sizePolicy().hasHeightForWidth());
	originalLabel->setSizePolicy(sizePolicy);

	horizontalLayout->addWidget(originalLabel);

	PLSHelpIcon *help = new PLSHelpIcon(formLeftWidget);
	help->setReleateText(originalLabel->text());

	help->setObjectName("formLeftWidget_icon");
	help->setToolTip(tooltip);

	formLayout->setWidget(idx, QFormLayout::LabelRole, formLeftWidget);

	auto positionFunction = [left = QPointer<QLabel>(originalLabel), rigth = QPointer<QLabel>(help)]() {
		if (!left || !rigth) {
			return;
		}
		int actualWidth = calculateWrappedLabelWidth(left);
		auto x = qMin(actualWidth + 5, left->parentWidget()->width() + left->parentWidget()->x() - rigth->width());
		rigth->move(x, (left->height() - rigth->height()) * 0.5);
	};
	if (sender->isVisible()) {
		pls_async_call_mt(positionFunction);
	} else {
		QObject::connect(sender, &PLSDialogView::shown, sender, positionFunction, Qt::SingleShotConnection);
	}
}
