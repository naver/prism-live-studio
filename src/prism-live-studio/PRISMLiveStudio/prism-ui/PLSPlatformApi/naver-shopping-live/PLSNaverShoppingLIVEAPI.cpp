//#include <Windows.h>

#include "PLSNaverShoppingLIVEAPI.h"

#include <ctime>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDateTime>
#include <QDomDocument>
#include <QtNetwork/QHttpPart>
#include <QtNetwork/QHttpMultiPart>

#include "pls-net-url.hpp"
#include "pls-common-define.hpp"
#include "PLSSyncServerManager.hpp"
#include "PLSChannelDataAPI.h"
#include "ChannelCommonFunctions.h"
#include "pls-gpop-data-struct.hpp"
#include "PLSLiveInfoNaverShoppingLIVE.h"
#include "PLSNaverShoppingLIVEDataManager.h"
#include "log/log.h"
#include "PLSDateFormate.h"
#include "ui-config.h"
#include "libhttp-client.h"

using namespace common;
constexpr auto CREATE_NOW_LIVING_LOG = "create navershopping now living";
constexpr auto UPDATE_NOW_LIVING_LOG = "update navershopping living";
constexpr auto CREATE_SCHEDULE_LIVING_LOG = "create navershopping schedule living";
constexpr auto UPDATE_SCHEDULE_LIVING_LOG = "update navershopping living";
constexpr auto CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY = "apigw-routing-key";

#define UPLOAD_IMAGE_PARAM_IMAGE QStringLiteral("image")
#define UPLOAD_IMAGE_PARAM_USERID QStringLiteral("userId")
#define HEADER_MINE_APPLICATION QStringLiteral("application/octet-stream")
#define ApiPropertyShowAlert QStringLiteral("ApiPropertyShowAlert")
#define ApiPropertyHandleTokenExpire QStringLiteral("ApiPropertyHandleTokenExpire")

const int AM = 0;
const int PM = 1;
auto constexpr AM_STR = "AM";
auto constexpr PM_STR = "PM";

#define REQUST_NO_ALERT                                               \
	{                                                             \
		{                                                     \
			PLSAPINaverShoppingType::PLSNaverShoppingAll, \
			{                                             \
				{                                     \
					ApiPropertyShowAlert, false   \
				}                                     \
			}                                             \
		}                                                     \
	}

const QString CHANNEL_CONNECTION_AGREE = "AGREED";
const QString CHANNEL_CONNECTION_NOAGREE = "NOT_AGREED";
const QString CHANNEL_CONNECTION_NONE = "NONE";

static bool hasProductNo(const QJsonObject &object)
{
	auto iter = object.find("productNo");
	if (iter == object.end()) {
		return false;
	}

	if (auto value = iter.value(); value.isDouble() || value.isString()) {
		return true;
	}
	return false;
}

static QString getUserAgent()
{
#define _VERSTR_I(major, minor, patch) #major "." #minor "." #patch
#define _VERSTR(major, minor, patch) _VERSTR_I(major, minor, patch)
	return "Mozilla/5.0 (Windows NT 6.2; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/75.0.3770.100 Safari/537.36 NAVER(pc; prism; prism-pc; " _VERSTR(
		PLS_RELEASE_CANDIDATE_MAJOR, PLS_RELEASE_CANDIDATE_MINOR, PLS_RELEASE_CANDIDATE_PATCH) ";)";
#undef _VERSTR_I
#undef _VERSTR
}

PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo::NaverShoppingUserInfo(const QJsonObject &object)
	: broadcastOwnerId(JSON_getString(object, broadcastOwnerId)),
	  serviceId(JSON_getString(object, serviceId)),
	  broadcasterId(JSON_getString(object, broadcasterId)),
	  profileImageUrl(JSON_getString(object, profileImageUrl)),
	  nickname(JSON_getString(object, nickname)),
	  storeAccountNo(JSON_getString(object, storeAccountNo))
{
}

PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo::NaverShoppingLivingInfo(const QJsonObject &object)
	: title(JSON_getString(object, title)),
	  status(JSON_getString(object, status)),
	  releaseLevel(JSON_getString(object, releaseLevel)),
	  id(QString::number(JSON_getInt64(object, id))),
	  streamSeq(QString::number(JSON_getInt64(object, streamSeq))),
	  publishUrl(JSON_getString(object, publishUrl)),
	  standByImage(JSON_getString(object, standByImage)),
	  broadcastType(JSON_getString(object, broadcastType)),
	  broadcastEndUrl(JSON_getString(object, broadcastEndUrl)),
	  displayType(JSON_getString(object, displayType)),
	  description(JSON_getString(object, description)),
	  externalExposeAgreementStatus(JSON_getString(object, externalExposeAgreementStatus)),
	  searchable(JSON_getBool(object, searchable))
{
	for (auto it : object["shoppingProducts"].toArray()) {
		shoppingProducts.append(ProductInfo(it.toObject()));
	}
	displayCategory = LiveCategory(object["displayCategory"].toObject());
}

PLSNaverShoppingLIVEAPI::GetSelectiveAccountStore::GetSelectiveAccountStore(const QJsonObject &object)
	: channelNo(QString::number(JSON_getInt64(object, channelNo))),
	  channelName(JSON_getString(object, channelName)),
	  windowIconImageUrl(JSON_getString(object, windowIconImageUrl)),
	  representImageUrl(JSON_getString(object, representImageUrl))
{
}

PLSNaverShoppingLIVEAPI::ProductInfo::ProductInfo(const QJsonObject &object) : ProductInfo(JSON_getInt64(object, productNo), object) {}

PLSNaverShoppingLIVEAPI::ProductInfo::ProductInfo(qint64 productNo_, const QJsonObject &object)
	: productNo(productNo_),
	  represent(JSON_getBool(object, represent)),
	  hasDiscountRate(JSON_hasPriceOrRateKey(object, discountRate)),
	  hasSpecialPrice(JSON_hasPriceOrRateKey(object, specialPrice)),
	  isMinorPurchasable(JSON_getBool(object, isMinorPurchasable)),
	  key(JSON_getString(object, key)),
	  name(JSON_getString(object, productName)),
	  imageUrl(JSON_getString(object, image)),
	  mallName(JSON_getString(object, mallName)),
	  productStatus(JSON_getString(object, productStatus)),
	  linkUrl(JSON_getString(object, productEndUrl)),
	  price(JSON_getDouble(object, price)),
	  discountRate(JSON_getDouble(object, discountRate)),
	  specialPrice(JSON_getDouble(object, specialPrice)),
	  wholeCategoryId(JSON_getString(object, wholeCategoryId)),
	  wholeCategoryName(JSON_getString(object, wholeCategoryName))
{
	channelNo = QString::number(JSON_getInt64(object, channelNo));
	accountNo = QString::number(JSON_getInt64(object, accountNo));
}

bool PLSNaverShoppingLIVEAPI::ProductInfo::discountRateIsValid() const
{
	return (productStatus == PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_SALE) ? hasDiscountRate : false;
}

bool PLSNaverShoppingLIVEAPI::ProductInfo::specialPriceIsValid() const
{
	return (productStatus == PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_SALE) ? hasSpecialPrice : false;
}

void PLSNaverShoppingLIVEAPI::storeChannelProductSearch(PLSPlatformNaverShoppingLIVE *platform, const QString &channelNo, const QString &productName, int page, int pageSize,
							const StoreChannelProductSearchCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback, pageSize](const QJsonDocument &json) {
		QJsonObject object = json.object();
		QList<ProductInfo> products;
		for (auto it : object["list"].toArray())
			products.append(ProductInfo(it.toObject()));
		int l_page = JSON_getInt(object, page);
		int totalCount = JSON_getInt(object, totalCount);
		callback(true, products, ((l_page > 0 ? l_page : 1) * pageSize) < totalCount, l_page);
	};
	auto failCallback = [callback](PLSAPINaverShoppingType) { callback(false, {}, false, 0); };

	if (!productName.isEmpty()) {
		getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_STORE_CHANNEL_PRODUCT_SEARCH.arg(channelNo))), "store channel product search", okCallback, failCallback,
			receiver ? receiver : platform, receiverIsValid, {}, {{"productName", QString::fromUtf8(productName.toUtf8().toPercentEncoding())}, {"page", page}, {"pageSize", pageSize}},
			true, REQUST_NO_ALERT);
	} else {
		getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_STORE_CHANNEL_PRODUCT_SEARCH.arg(channelNo))), "store channel product search", okCallback, failCallback,
			receiver ? receiver : platform, receiverIsValid, {}, {{"page", page}, {"pageSize", pageSize}}, true, REQUST_NO_ALERT);
	}
}

void PLSNaverShoppingLIVEAPI::refreshChannelToken(const PLSPlatformNaverShoppingLIVE *platform, const GetUserInfoCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	pls::http::Request request;
	if (platform->getAccessToken().length() > 0) {
		request.rawHeader("X-LIVE-COMMERCE-AUTH", platform->getAccessToken()) //
			.rawHeader("X-LIVE-COMMERCE-DEVICE-ID", platform->getSoftwareUUid());
		if (platform->getChannelCookie().length() > 0) {
			request.cookie(platform->getChannelCookie());
		}
		PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping use token request v1/brocaster,serviceId: %s", platform->getUserInfo().serviceId.toUtf8().constData());
	} else {
		request.cookie(platform->getChannelCookie());
		PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping use cookie request v1/brocaster,serviceId: %s", platform->getUserInfo().serviceId.toUtf8().constData());
	}
	request.rawHeader(CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY, CHANNEL_NAVER_SHOPPING_LIVE_HEADER);
	pls::http::request(request.method(pls::http::Method::Get)
				   .hmacUrl(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_TOKEN), PLS_PC_HMAC_KEY.toUtf8()) //
				   .userAgent(getUserAgent())
				   .jsonContentType()
				   .withLog()
				   .receiver(receiver, receiverIsValid)
				   .objectOkResult(
					   [callback, platform, receiver, receiverIsValid](const pls::http::Reply &reply, const QJsonObject &jsonObject) {
						   PLSNaverShoppingLIVEDataManager::instance()->downloadImage(
							   platform, JSON_getString(jsonObject, profileImageUrl),
							   [callback, accessToken = reply.rawHeader("X-LIVE-COMMERCE-AUTH"), jsonObject](bool result, const QString &imagePath) {
								   PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE,
									    "Naver Shopping Live refresh token and get user info success and image download succeed:%d", result);
								   NaverShoppingUserInfo info(jsonObject);
								   info.profileImagePath = imagePath;
								   info.accessToken = accessToken;
								   callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, info);
							   },
							   receiver, receiverIsValid);
					   },
					   [callback, receiver, receiverIsValid](const pls::http::Reply &, const QJsonParseError &error) {
						   PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live refresh token and get user info failed, reason: %s",
							     error.errorString().toUtf8().constData());
						   pls_async_call_mt(receiver, receiverIsValid, [callback]() { callback(PLSAPINaverShoppingType::PLSNaverShoppingFailed, {}); });
					   })
				   .failResult([callback, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live refresh token and get user info failed, networkError: %d, statusCode: %d, data: %s",
						     reply.error(), reply.statusCode(), reply.data().constData());
					   if (reply.statusCode() == 0 && reply.error() > QNetworkReply::NoError && reply.error() <= QNetworkReply::UnknownNetworkError) {
						   pls_async_call_mt(receiver, receiverIsValid, [callback]() { callback(PLSAPINaverShoppingType::PLSNaverShoppingNetworkError, {}); });
					   } else if (reply.statusCode() == 401) {
						   pls_async_call_mt(receiver, receiverIsValid, [callback, data = reply.data()]() { callback(getLoginFailType(data), {}); });
					   } else {
						   pls_async_call_mt(receiver, receiverIsValid, [callback]() { callback(PLSAPINaverShoppingType::PLSNaverShoppingFailed, {}); });
					   }
				   }));
}

PLSAPINaverShoppingType PLSNaverShoppingLIVEAPI::getLoginFailType(const QByteArray &data)
{
	QJsonParseError jsonError;
	QJsonDocument jsonDocument = QJsonDocument::fromJson(data, &jsonError);
	QJsonObject object = jsonDocument.object();
	if (jsonError.error == QJsonParseError::NoError) {
		QJsonObject l_object = jsonDocument.object();
		qint64 errorCode = 0;
		if (l_object.contains(name2str(code))) {
			errorCode = JSON_getInt64(l_object, code);
		} else if (l_object.contains(name2str(errorCode))) {
			errorCode = JSON_getInt64(l_object, errorCode);
		}
		if (errorCode == 1003 || errorCode == 1030 || errorCode == 1031 || errorCode == 1035 || errorCode == 1037) {
			return PLSAPINaverShoppingType::PLSNaverShoppingNoLiveRight;
		} else if (errorCode == 1004 || errorCode == 1029) {
			return PLSAPINaverShoppingType::PLSNaverShoppingInvalidAccessToken;
		} else if (errorCode == 1036) {
			return PLSAPINaverShoppingType::PLSNaverShoppingLoginFailed;
		}
	}
	return PLSAPINaverShoppingType::PLSNaverShoppingFailed;
}

void PLSNaverShoppingLIVEAPI::uploadImage(PLSPlatformNaverShoppingLIVE *platform, const QString &imagePath, const UploadImageCallback &callback, const QObject *receiver,
					  const ReceiverIsValid &receiverIsValid)
{
	auto sessionKeyOkCallback = [platform, imagePath, callback, receiver, receiverIsValid](const QJsonDocument &json) {
		QJsonObject object = json.object();
		QString sessionKey = JSON_getString(object, sessionKey);
		QString uploaderDomain = object.value("phinfInfo").toObject().value("uploaderDomain").toString();
		QString deliveryDomain = object.value("phinfInfo").toObject().value("deliveryDomain").toString();
		if (uploaderDomain.isEmpty() || deliveryDomain.isEmpty() || sessionKey.isEmpty()) {
			callback(PLSAPINaverShoppingType::PLSNaverShoppingFailed, QString());
			return;
		}
		QString uploadURL = "";
		uploadLocalImage(platform, uploadURL, deliveryDomain, imagePath, callback, receiver, receiverIsValid);
	};
	Url url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_GET_SEESION_KEY.arg(platform->getUserInfo().broadcasterId)),
		urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_GET_SEESION_KEY.arg(pls_masking_person_info(platform->getUserInfo().broadcasterId))));
	auto sessionKeyFailCallback = [callback](PLSAPINaverShoppingType apiType) { callback(apiType, QString()); };
	getJson(platform, url, "upload userImage", sessionKeyOkCallback, sessionKeyFailCallback, receiver ? receiver : platform, receiverIsValid, {}, {}, true);
}

void PLSNaverShoppingLIVEAPI::uploadLocalImage(const PLSPlatformNaverShoppingLIVE *platform, const QString &uploadUrl, const QString &deliveryUrl, const QString &imageFilePath,
					       const UploadImageCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping LIVE upload image, uploadUrl: %s,deliveryUrl = %s", uploadUrl.toUtf8().constData(), deliveryUrl.toUtf8().constData());
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post)
				   .url(uploadUrl)
				   .userAgent(getUserAgent())
				   .form(UPLOAD_IMAGE_PARAM_IMAGE, imageFilePath, true)
				   .form(UPLOAD_IMAGE_PARAM_USERID, platform->getUserInfo().broadcasterId.toUtf8())
				   .receiver(receiver, receiverIsValid)
				   .okResult([callback, receiver, receiverIsValid, deliveryUrl](const pls::http::Reply &reply) {
					   if (QString url = getUploadImageDomain(reply.data()); !url.isEmpty()) {
						   QString imageUrl = QString("%1/%2").arg(deliveryUrl).arg(url);
						   pls_async_call_mt(receiver, receiverIsValid, [callback, imageUrl]() { callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, imageUrl); });
					   } else {
						   pls_async_call_mt(receiver, receiverIsValid, [callback]() { callback(PLSAPINaverShoppingType::PLSNaverShoppingFailed, QString()); });
					   }
				   })
				   .failResult([callback, receiver, receiverIsValid](const pls::http::Reply &) {
					   PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping LIVE upload image failed");
					   pls_async_call_mt(receiver, receiverIsValid, [callback]() { callback(PLSAPINaverShoppingType::PLSNaverShoppingFailed, QString()); });
				   }));
}

QString PLSNaverShoppingLIVEAPI::getUploadImageDomain(const QByteArray &data)
{
	QDomDocument doc;
	doc.setContent(data);
	QDomElement docElem = doc.documentElement();
	QString url;
	QDomElement element = docElem.firstChildElement();
	while (!element.isNull()) {
		QString tag_name = element.tagName();
		QString tag_value = element.text();
		if (tag_name == "url") {
			url = tag_value;
			break;
		}
		element = element.nextSiblingElement();
	}
	return url;
}

void PLSNaverShoppingLIVEAPI::createNowLiving(PLSPlatformNaverShoppingLIVE *platform, const QJsonObject &body, const CreateNowLivingCallback &callback, const QObject *receiver,
					      const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &json) {
		QJsonObject object = json.object();
		NaverShoppingLivingInfo info(object);
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, info);
	};
	auto failCallback = [callback](PLSAPINaverShoppingType apiType) { callback(apiType, NaverShoppingLivingInfo()); };
	postJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_CREATE_NOW_LIVING)), body, CREATE_NOW_LIVING_LOG, okCallback, failCallback, receiver, receiverIsValid, QVariantMap());
}

void PLSNaverShoppingLIVEAPI::createScheduleLiving(PLSPlatformNaverShoppingLIVE *platform, const QString &scheduleId, const CreateNowLivingCallback &callback, const QObject *receiver,
						   const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &json) {
		QJsonObject object = json.object();
		NaverShoppingLivingInfo info(object);
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, info);
	};
	auto failCallback = [callback](PLSAPINaverShoppingType apiType) { callback(apiType, NaverShoppingLivingInfo()); };
	customJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_CREATE_SCHEDULE_LIVING.arg(scheduleId))), CREATE_SCHEDULE_LIVING_LOG, okCallback, failCallback, receiver, receiverIsValid);
}

void PLSNaverShoppingLIVEAPI::getLivingInfo(PLSPlatformNaverShoppingLIVE *platform, bool livePolling, const GetLivingInfoCallback &callback, const QObject *receiver,
					    const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &json) {
		QJsonObject object = json.object();
		NaverShoppingLivingInfo info(object);
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, info);
	};
	auto failCallback = [callback](PLSAPINaverShoppingType apiType) { callback(apiType, NaverShoppingLivingInfo()); };
	if (livePolling) {
		QVariantMap propertyMap;
		propertyMap.insert(ApiPropertyShowAlert, false);
		getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_GET_LIVING_INFO.arg(platform->getLivingInfo().id))), "get living info", okCallback, failCallback,
			receiver ? receiver : platform, receiverIsValid, {}, {}, true, {{PLSAPINaverShoppingType::PLSNaverShoppingAll, propertyMap}});
	} else {
		getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_GET_LIVING_INFO.arg(platform->getLivingInfo().id))), "get living info", okCallback, failCallback,
			receiver ? receiver : platform, receiverIsValid, {}, {}, true);
	}
}

void PLSNaverShoppingLIVEAPI::updateNowLiving(PLSPlatformNaverShoppingLIVE *platform, const QString &id, const QJsonObject &body, const std::function<void(PLSAPINaverShoppingType apiType)> &callback,
					      const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &) { callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess); };
	auto failCallback = [callback](PLSAPINaverShoppingType apiType) { callback(apiType); };
	putJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVING.arg(id))), body, UPDATE_NOW_LIVING_LOG, okCallback, failCallback, receiver, receiverIsValid, QVariantMap());
}

void PLSNaverShoppingLIVEAPI::updateScheduleInfo(PLSPlatformNaverShoppingLIVE *platform, const QString &id, const QJsonObject &body, const UpdateScheduleCallback &callback, const QObject *receiver,
						 const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &json) {
		QJsonObject object = json.object();
		ScheduleInfo info(object);
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, info);
	};
	auto failCallback = [callback](PLSAPINaverShoppingType apiType) { callback(apiType, ScheduleInfo()); };
	putJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVING.arg(id))), body, UPDATE_SCHEDULE_LIVING_LOG, okCallback, failCallback, receiver, receiverIsValid, QVariantMap());
}

void PLSNaverShoppingLIVEAPI::stopLiving(const PLSPlatformNaverShoppingLIVE *platform, bool needVideoSave, const std::function<void(bool ok)> &callback, const QObject *receiver,
					 const ReceiverIsValid &receiverIsValid)
{
	auto url = Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_STOP_LIVING.arg(platform->getLivingInfo().id).arg(needVideoSave)));
	pls::http::request(pls::http::Request()
				   .method("PATCH")
				   .hmacUrl(url.url, PLS_PC_HMAC_KEY.toUtf8())
				   .rawHeader("X-LIVE-COMMERCE-AUTH", platform->getAccessToken())
				   .rawHeader("X-LIVE-COMMERCE-DEVICE-ID", platform->getSoftwareUUid())
				   .rawHeader(CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY, CHANNEL_NAVER_SHOPPING_LIVE_HEADER)
				   .userAgent(getUserAgent())
				   .workInMainThread()
				   .withLog(url.maskingUrl)
				   .receiver(receiver, receiverIsValid)
				   .okResult([callback](const pls::http::Reply &) {
					   PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live stop navershopping living success");
					   callback(true);
				   })
				   .failResult([callback](const pls::http::Reply &) {
					   PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live stop navershopping living failed");
					   callback(false);
				   }));
}

void PLSNaverShoppingLIVEAPI::logoutNaverShopping(const PLSPlatformNaverShoppingLIVE *platform, const QString &accessToken, const QObject *)
{
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Delete)
				   .hmacUrl(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_DELETE_TOKEN), PLS_PC_HMAC_KEY.toUtf8())
				   .rawHeader("X-LIVE-COMMERCE-AUTH", accessToken)
				   .rawHeader("X-LIVE-COMMERCE-DEVICE-ID", platform->getSoftwareUUid())
				   .rawHeader(CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY, CHANNEL_NAVER_SHOPPING_LIVE_HEADER)
				   .userAgent(getUserAgent())
				   .okResult([](const pls::http::Reply &) { PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live delete token success"); })
				   .failResult([](const pls::http::Reply &) { PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live delete token failed"); }));
}

void PLSNaverShoppingLIVEAPI::getSelectiveAccountStores(PLSPlatformNaverShoppingLIVE *platform, const GetSelectiveAccountStoresCallabck &callback, const QObject *receiver,
							const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &json) {
		QJsonObject object = json.object();
		QList<GetSelectiveAccountStore> stores;
		for (auto it : object["smartStoreInfo"].toArray())
			stores.append(GetSelectiveAccountStore(it.toObject()));
		callback(true, stores);
	};
	auto failCallback = [callback](PLSAPINaverShoppingType) { callback(false, {}); };

	getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_TOKEN)), "get selective account stores", okCallback, failCallback, receiver ? receiver : platform, receiverIsValid, {}, {},
		true, REQUST_NO_ALERT);
}

void PLSNaverShoppingLIVEAPI::getScheduleList(PLSPlatformNaverShoppingLIVE *platform, int currentPage, bool isNotice, const GetScheduleListCallback &callback, const QObject *receiver,
					      const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &json) {
		QJsonObject rootJsonObject = json.object();
		QJsonArray jsonArray = rootJsonObject.value(name2str(list)).toArray();
		int page = rootJsonObject.value(name2str(page)).toInt();
		int totalCount = rootJsonObject.value(name2str(totalCount)).toInt();
		QList<ScheduleInfo> list;
		for (auto it : jsonArray) {
			QJsonObject jsonObject = it.toObject();
			bool presetting = JSON_getBool(jsonObject, presetting);
			if (presetting) {
				list.append(ScheduleInfo(jsonObject));
			}
		}
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, list, page, totalCount);
	};
	auto failCallback = [callback](PLSAPINaverShoppingType apiType) { callback(apiType, QList<ScheduleInfo>(), 0, 0); };
	if (isNotice) {
		QVariantMap propertyMap;
		propertyMap.insert(ApiPropertyShowAlert, false);
		propertyMap.insert(ApiPropertyHandleTokenExpire, false);
		getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_SCHEDULE_LIST)), "get schedule list filter beforeSeconds and afterSeconds", okCallback, failCallback,
			receiver ? receiver : platform, receiverIsValid, {}, {{"pageNum", currentPage}, {"pageSize", SCHEDULE_PER_PAGE_MAX_NUM}}, true,
			{{PLSAPINaverShoppingType::PLSNaverShoppingAll, propertyMap}}, false, true);
	} else {
		getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_SCHEDULE_LIST)), "get schedule list", okCallback, failCallback, receiver ? receiver : platform, receiverIsValid, {},
			{{"pageNum", currentPage}, {"pageSize", SCHEDULE_PER_PAGE_MAX_NUM}}, true, {}, false, true);
	}
}

void PLSNaverShoppingLIVEAPI::getCategoryList(PLSPlatformNaverShoppingLIVE *platform, const GetCategoryListCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &json) {
		QJsonArray jsonArray = json.array();
		QList<LiveCategory> list;
		for (auto it : jsonArray) {
			LiveCategory category(it.toObject());
			list.append(category);
		}
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, list);
	};
	auto failCallback = [callback](PLSAPINaverShoppingType apiType) { callback(apiType, QList<LiveCategory>()); };
	getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_CATEGORY_LIST)), "get category list", okCallback, failCallback, receiver ? receiver : platform, receiverIsValid, {}, {});
}

void PLSNaverShoppingLIVEAPI::sendPushNotification(PLSPlatformNaverShoppingLIVE *platform, const QObject *receiver, const RequestOkCallback &ok, const RequestFailedCallback &fail,
						   const ReceiverIsValid &receiverIsValid)
{
	NaverShoppingLivingInfo liveInfo = platform->getLivingInfo();
	auto apiPropertyMap = ApiPropertyMap();
	getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_PSUH_NOTIFICATION.arg(liveInfo.id))), "send push notification", ok, fail, receiver ? receiver : platform, receiverIsValid, {},
		{{"categoryType", "LIVE_ONAIR"}}, true, apiPropertyMap, true);
}

void PLSNaverShoppingLIVEAPI::sendNotice(PLSPlatformNaverShoppingLIVE *platform, const QJsonObject &body, const QObject *receiver, const RequestOkCallback &ok, const RequestFailedCallback &fail,
					 const ReceiverIsValid &receiverIsValid)
{
	QVariantMap propertyMap;
	propertyMap.insert(ApiPropertyShowAlert, false);
	propertyMap.insert(ApiPropertyHandleTokenExpire, false);
	postJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_SEND_NOTICE)), body, "send navershopping notice request", ok, fail, receiver ? receiver : platform, receiverIsValid, {},
		 {{PLSAPINaverShoppingType::PLSNaverShoppingAll, propertyMap}});
}

void PLSNaverShoppingLIVEAPI::productSearchByUrl(PLSPlatformNaverShoppingLIVE *platform, const QString &url, const ProductSearchByUrlCallback &callback, const QObject *receiver,
						 const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &json) {
		if (QJsonObject po = json.object(); hasProductNo(po)) {
			callback(true, true, ProductInfo(po));
		} else {
			callback(true, false, ProductInfo());
		}
	};
	auto failCallback = [callback](PLSAPINaverShoppingType) { callback(false, false, {}); };
	getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_URL)), "product search by url", okCallback, failCallback, receiver ? receiver : platform, receiverIsValid, {},
		{{"url", url}}, true, REQUST_NO_ALERT);
}

void PLSNaverShoppingLIVEAPI::productSearchByTag(PLSPlatformNaverShoppingLIVE *platform, const QString &tag, int page, int pageSize, const ProductSearchByTagCallback &callback,
						 const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback, pageSize](const QJsonDocument &json) {
		QJsonObject object = json.object();
		QList<ProductInfo> products;
		for (auto it : object["list"].toArray()) {
			products.append(ProductInfo(it.toObject()));
		}
		int l_page = JSON_getInt(object, page);
		int totalCount = JSON_getInt(object, totalCount);
		callback(true, products, ((l_page > 0 ? l_page : 1) * pageSize) < totalCount, l_page);
	};
	auto failCallback = [callback](PLSAPINaverShoppingType) { callback(false, {}, false, 0); };
	getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_TAG)), "product search by tag", okCallback, failCallback, receiver ? receiver : platform, receiverIsValid, {},
		{{"query", QString::fromUtf8(tag.toUtf8().toPercentEncoding())}, {"page", page}, {"pageSize", pageSize}}, true, REQUST_NO_ALERT);
}

void PLSNaverShoppingLIVEAPI::productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, const QList<qint64> &fixedProductNos, const QList<qint64> &unfixedProductNos,
							const ProductSearchByProductNosSplitCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	QStringList strProductNos;
	for (auto productNo : fixedProductNos)
		strProductNos.append(QString::number(productNo));
	for (auto productNo : unfixedProductNos)
		strProductNos.append(QString::number(productNo));
	productSearchByProductNos(
		platform, strProductNos,
		[callback, fixedProductNos, unfixedProductNos](bool ok, const QList<ProductInfo> &products) {
			if (ok) {
				QList<ProductInfo> fixedProducts;
				QList<ProductInfo> unfixedProducts;
				for (const auto &product : products) {
					if (fixedProductNos.contains(product.productNo)) {
						fixedProducts.append(product);
					} else {
						unfixedProducts.append(product);
					}
				}

				if ((fixedProductNos.count() == fixedProducts.count()) && (unfixedProductNos.count() == unfixedProducts.count())) {
					callback(true, fixedProducts, unfixedProducts);
				} else {
					callback(false, {}, {});
				}
			} else {
				callback(false, {}, {});
			}
		},
		receiver, receiverIsValid);
}

void PLSNaverShoppingLIVEAPI::productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, const QList<qint64> &productNos, const ProductSearchByProductNosAllCallback &callback,
							const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	QStringList strProductNos;
	for (auto productNo : productNos)
		strProductNos.append(QString::number(productNo));
	productSearchByProductNos(platform, strProductNos, callback, receiver, receiverIsValid);
}

void PLSNaverShoppingLIVEAPI::productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, const QStringList &strProductNos, const ProductSearchByProductNosAllCallback &callback,
							const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback, strProductNos](const QJsonDocument &json) {
		QJsonObject object = json.object();
		QList<ProductInfo> products;
		for (const QString &productNo : strProductNos) {
			QJsonValue value = object[productNo];
			if (value.isObject()) {
				products.append(ProductInfo(productNo.toLongLong(), value.toObject()));
			}
		}

		callback(true, products);
	};
	auto failCallback = [callback](PLSAPINaverShoppingType) { callback(false, {}); };

	QString productNos;
	for (int i = 0; i < strProductNos.size(); i++) {
		if (i == 0)
			productNos = pls_masking_person_info(strProductNos.at(i));
		else
			productNos.append(QString(",%1").arg(pls_masking_person_info(strProductNos.at(i))));
	}
	QString encUrl = QString("%1?productNos=%2").arg(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_PRODUCTNOS)).arg(productNos);
	getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_PRODUCTNOS), encUrl), "product search by product nos", okCallback, failCallback, receiver ? receiver : platform,
		receiverIsValid, {}, {{"productNos", strProductNos.join(',')}}, true, REQUST_NO_ALERT);
}

QString PLSNaverShoppingLIVEAPI::urlForPath(const QString &path)
{
	return QString("%1%2").arg(CHANNEL_NAVER_SHOPPING_HOST).arg(PRISM_SSL).arg(path);
}

void PLSNaverShoppingLIVEAPI::downloadImage(const PLSPlatformNaverShoppingLIVE *, const QString &url, const std::function<void(bool ok, const QString &imagePath)> &callback, const QObject *receiver,
					    const ReceiverIsValid &receiverIsValid, int timeout)
{
	pls::http::request(pls::http::Request()
				   .url(url)
				   .forDownload(true)
				   .userAgent(getUserAgent())
				   .withLog(pls_masking_person_info(url))
				   .saveFileNamePrefix(QStringLiteral("NaverShoppingLIVE-"))
				   .saveDir(PLSNaverShoppingLIVEDataManager::getCacheDirPath())
				   .receiver(receiver, receiverIsValid)
				   .timeout(timeout)
				   .okResult([receiver, receiverIsValid, callback](const pls::http::Reply &reply) {
					   pls_async_call_mt(receiver, receiverIsValid, [callback, imagePath = reply.downloadFilePath()]() { pls_invoke_safe(callback, true, imagePath); });
				   })
				   .failResult([receiver, receiverIsValid, callback](const pls::http::Reply &) {
					   pls_async_call_mt(receiver, receiverIsValid, [callback]() { pls_invoke_safe(callback, false, QString()); }); //
				   }));
}

void PLSNaverShoppingLIVEAPI::downloadImages(const PLSPlatformNaverShoppingLIVE *, const QStringList &urls, const std::function<void(const QList<QPair<bool, QString>> &imagePath)> &callback,
					     const QObject *receiver, const ReceiverIsValid &receiverIsValid, int timeout)
{
	pls::http::Requests requests;
	for (const QString &url : urls) {
		requests.add(pls::http::Request()
				     .url(url)
				     .forDownload(true)
				     .userAgent(getUserAgent())
				     .withLog(pls_masking_person_info(url))
				     .saveFileNamePrefix(QStringLiteral("NaverShoppingLIVE-"))
				     .saveDir(PLSNaverShoppingLIVEDataManager::getCacheDirPath())
				     .receiver(receiver, receiverIsValid)
				     .timeout(timeout));
	}
	pls::http::requests(requests                                     //
				    .receiver(receiver, receiverIsValid) //
				    .results([callback, receiver, receiverIsValid](const pls::http::Replies &replies) {
					    QList<QPair<bool, QString>> imagePaths;
					    replies.replies([&imagePaths](const pls::http::Reply &reply) { imagePaths.append(QPair<bool, QString>(reply.isOk(), reply.downloadFilePath())); });
					    pls_async_call_mt(receiver, receiverIsValid, [imagePaths, callback]() { pls_invoke_safe(callback, imagePaths); });
				    }));
}

bool PLSNaverShoppingLIVEAPI::json_hasKey(const QJsonObject &object, const QString &key)
{
	auto iter = object.find(key);
	if (iter == object.end()) {
		return false;
	}

	if (auto value = iter.value(); value.isNull() || value.isUndefined()) {
		return false;
	}
	return true;
}

bool PLSNaverShoppingLIVEAPI::json_hasPriceOrRateKey(const QJsonObject &object, const QString &key)
{
	auto iter = object.find(key);
	if (iter == object.end()) {
		return false;
	}

	double price = 0.0;

	if (auto value = iter.value(); value.isDouble()) {
		price = value.toDouble();
	} else if (value.isString()) {
		price = value.toString().toDouble();
	} else {
		return false;
	}

	if (PLSNaverShoppingLIVEDataManager::priceOrRateToInteger(price) > 0) {
		return true;
	}
	return false;
}

bool PLSNaverShoppingLIVEAPI::json_getBool(const QJsonObject &object, const QString &key, bool defaultValue)
{
	return json_toBool(object[key], defaultValue);
}

int PLSNaverShoppingLIVEAPI::json_getInt(const QJsonObject &object, const QString &key, int defaultValue)
{
	return json_toInt(object[key], defaultValue);
}

qint64 PLSNaverShoppingLIVEAPI::json_getInt64(const QJsonObject &object, const QString &key, qint64 defaultValue)
{
	return json_toInt64(object[key], defaultValue);
}

double PLSNaverShoppingLIVEAPI::json_getDouble(const QJsonObject &object, const QString &key, double defaultValue)
{
	return json_toDouble(object[key], defaultValue);
}

QString PLSNaverShoppingLIVEAPI::json_getString(const QJsonObject &object, const QString &key, const QString &defaultValue)
{
	return json_toString(object[key], defaultValue);
}

bool PLSNaverShoppingLIVEAPI::json_toBool(const QJsonValue &value, bool defaultValue)
{
	if (value.isBool())
		return value.toBool();
	else if (value.isDouble())
		return static_cast<int>(value.toDouble()) ? true : false;
	else if (value.isString())
		return value.toString().toInt() ? true : false;
	else
		return defaultValue;
}

int PLSNaverShoppingLIVEAPI::json_toInt(const QJsonValue &value, int defaultValue)
{
	if (value.isDouble())
		return static_cast<int>(value.toDouble());
	else if (value.isString())
		return value.toString().toInt();
	else if (value.isBool())
		return static_cast<int>(value.toBool());
	else
		return defaultValue;
}

qint64 PLSNaverShoppingLIVEAPI::json_toInt64(const QJsonValue &value, qint64 defaultValue)
{
	if (value.isDouble())
		return static_cast<qint64>(value.toDouble());
	else if (value.isString())
		return value.toString().toLongLong();
	else if (value.isBool())
		return static_cast<qint64>(value.toBool());
	else
		return defaultValue;
}

double PLSNaverShoppingLIVEAPI::json_toDouble(const QJsonValue &value, double defaultValue)
{
	if (value.isDouble())
		return value.toDouble();
	else if (value.isString())
		return value.toString().toDouble();
	else if (value.isBool())
		return static_cast<int>(value.toBool());
	else
		return defaultValue;
}

QString PLSNaverShoppingLIVEAPI::json_toString(const QJsonValue &value, const QString &defaultValue)
{
	if (value.isString())
		return value.toString();
	else if (value.isDouble())
		return QString::number(value.toDouble());
	else if (value.isBool())
		return value.toBool() ? "true" : "false";
	else
		return defaultValue;
}

void PLSNaverShoppingLIVEAPI::processRequestOkCallback(const PLSPlatformNaverShoppingLIVE *, const QByteArray &data, const char *log, const QObject *receiver, const ReceiverIsValid &receiverIsValid,
						       const RequestOkCallback &ok, const RequestFailedCallback &fail, int statusCode, bool isIgnoreEmptyJson)
{
	if (pls_get_app_exiting()) {
		return;
	}
	QJsonParseError jsonError;
	QJsonDocument respjson = QJsonDocument::fromJson(data, &jsonError);
	bool isValidJson = jsonError.error == QJsonParseError::NoError;
	if (isIgnoreEmptyJson && !isValidJson) {
		respjson = QJsonDocument();
	}

	if (isValidJson || isIgnoreEmptyJson) {
		PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live %s success", log);
		pls_async_call_mt(receiver, receiverIsValid, [ok, respjson]() { pls_invoke_safe(ok, respjson); });
	} else {
		PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live %s failed, reason: %s", log, jsonError.errorString().toUtf8().constData());
		pls_async_call_mt(receiver, receiverIsValid, [fail, statusCode]() {
			pls_invoke_safe(fail, statusCode == 204 ? PLSAPINaverShoppingType::PLSNaverShoppingNotFound204 : PLSAPINaverShoppingType::PLSNaverShoppingFailed);
		});
	}
}

void PLSNaverShoppingLIVEAPI::processRequestFailCallback(PLSPlatformNaverShoppingLIVE *platform, int statusCode, const QByteArray &data, const char *log, QNetworkReply::NetworkError networkError,
							 const QObject *receiver, const ReceiverIsValid &receiverIsValid, const RequestFailedCallback &fail, const ApiPropertyMap &apiPropertyMap)
{
	if (pls_get_app_exiting()) {
		return;
	}
	static const QString cheduleNotReached = QObject::tr("navershopping.api.request.reservation.not.reached");
	static const QString cheduleIsLived = QObject::tr("navershopping.api.request.reservation.living");
	static const QString cheduleIsLiving = QObject::tr("navershopping.api.request.reservation.isLving");
	static const QString cheduleDeleted = QObject::tr("navershopping.api.request.reservation.delete");
	static const QString ageRestrictedProduct = QObject::tr("navershopping.api.request.minor.purchase");
	static const QString attachProductSendOwnMall = QObject::tr("navershopping.api.request.attach.product.send.own.mall");
	PLSAPINaverShoppingType apiType = PLSAPINaverShoppingType::PLSNaverShoppingFailed;
	if (networkError > QNetworkReply::NoError && networkError <= QNetworkReply::UnknownNetworkError) {
		apiType = PLSAPINaverShoppingType::PLSNaverShoppingNetworkError;
		PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live %s network error, statuscode: %d", log, statusCode);
	} else if (statusCode == 401) {
		apiType = PLSAPINaverShoppingType::PLSNaverShoppingInvalidAccessToken;
		PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live %s invalid access token", log);
	} else {
		auto doc = QJsonDocument::fromJson(data);
		auto root = doc.object();
		const static auto messageKey = "message";
		qint64 errorCode = 0;
		if (root.contains(name2str(code))) {
			errorCode = JSON_getInt64(root, code);
		} else if (root.contains(name2str(errorCode))) {
			errorCode = JSON_getInt64(root, errorCode);
		}
		QString exectText = root[messageKey].toVariant().toString();
		if (exectText.contains(cheduleNotReached)) {
			apiType = PLSAPINaverShoppingType::PLSNaverShoppingScheduleTimeNotReached;
		} else if (exectText.contains(cheduleIsLived) || (exectText.contains(cheduleIsLiving) && errorCode == 1002)) {
			apiType = PLSAPINaverShoppingType::PLSNaverShoppingScheduleIsLived;
		} else if (exectText.contains(cheduleDeleted)) {
			apiType = PLSAPINaverShoppingType::PLSNaverShoppingScheduleDelete;
		} else if (exectText.contains(ageRestrictedProduct)) {
			apiType = PLSAPINaverShoppingType::PLSNaverShoppingAgeRestrictedProduct;
		} else if (exectText.contains(attachProductSendOwnMall)) {
			apiType = PLSAPINaverShoppingType::PLSNaverShoppingAttachProductsToOwnMall;
		} else if (strcmp(log, CREATE_SCHEDULE_LIVING_LOG) == 0) {
			if (errorCode == 1067) {
				apiType = PLSAPINaverShoppingType::PLSNaverShoppingCreateSchduleExternalStream;
			} else if (errorCode == 1002) {
				apiType = PLSAPINaverShoppingType::PLSNaverShoppingCannotAddOtherShopProduct;
			}
		} else if (strcmp(log, CREATE_NOW_LIVING_LOG) == 0 || strcmp(log, UPDATE_NOW_LIVING_LOG) == 0 || strcmp(log, UPDATE_SCHEDULE_LIVING_LOG) == 0) {
			if (errorCode == 1002) {
				apiType = PLSAPINaverShoppingType::PLSNaverShoppingCannotAddOtherShopProduct;
			}
		}
		PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live %s other network error reason: %d", log, apiType);
	}

	pls_async_call(platform, receiver, receiverIsValid, [platform, apiType, apiPropertyMap]() { platform->handleCommonApiType(apiType, apiPropertyMap); });
	pls_async_call_mt(receiver, receiverIsValid, [fail, apiType]() { pls_invoke_safe(fail, apiType); });
}

bool PLSNaverShoppingLIVEAPI::isShowApiAlert(PLSAPINaverShoppingType apiType, const ApiPropertyMap &apiPropertyMap)
{
	if (apiPropertyMap.contains(PLSAPINaverShoppingType::PLSNaverShoppingAll)) {
		QVariantMap propertyMap = apiPropertyMap.value(PLSAPINaverShoppingType::PLSNaverShoppingAll);
		bool show = propertyMap.value(ApiPropertyShowAlert).toBool();
		return show;
	} else if (apiPropertyMap.contains(apiType)) {
		QVariantMap propertyMap = apiPropertyMap.value(apiType);
		bool show = propertyMap.value(ApiPropertyShowAlert).toBool();
		return show;
	}
	return true;
}

bool PLSNaverShoppingLIVEAPI::isHandleTokenExpired(PLSAPINaverShoppingType apiType, const ApiPropertyMap &apiPropertyMap)
{
	if (apiPropertyMap.contains(PLSAPINaverShoppingType::PLSNaverShoppingAll)) {
		if (QVariantMap propertyMap = apiPropertyMap.value(PLSAPINaverShoppingType::PLSNaverShoppingAll); propertyMap.contains(ApiPropertyHandleTokenExpire)) {
			bool handleTokenExpired = propertyMap.value(ApiPropertyHandleTokenExpire).toBool();
			return handleTokenExpired;
		}
		return true;
	} else if (apiPropertyMap.contains(apiType)) {
		if (QVariantMap propertyMap = apiPropertyMap.value(apiType); propertyMap.contains(ApiPropertyHandleTokenExpire)) {
			bool handleTokenExpired = propertyMap.value(ApiPropertyHandleTokenExpire).toBool();
			return handleTokenExpired;
		}
		return true;
	}
	return true;
}

void PLSNaverShoppingLIVEAPI::getNaverShoppingDateFormat(uint timeStamp, QDate &date, QString &yearMonthDay, QString &hour, QString &minute, QString &ap)
{
	QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timeStamp);
	dateTime.setTimeSpec(Qt::UTC);
	QLocale local = QLocale::English;
	QString formateStr = "ddd, MMM dd, yyyy@hh@mm@AP";
	if (IS_KR()) {
		local = QLocale::Korean;
		formateStr = "yyyy. MM. dd. ddd@hh@mm@AP";
	}
	QString enTimeStr = local.toString(dateTime, formateStr);
	QStringList list = enTimeStr.split("@");
	if (!list.isEmpty()) {
		yearMonthDay = list.at(0);
	}
	if (list.count() > 1) {
		hour = list.at(1);
	}
	if (list.count() > 2) {
		minute = list.at(2);
	}
	if (list.count() > 3) {
		ap = list.at(3);
	}
	date = dateTime.date();
}

void PLSNaverShoppingLIVEAPI::getNaverShoppingDateFormat(qint64 timeStamp, QString &yearMonthDay)
{
	QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timeStamp);
	dateTime.setTimeSpec(Qt::UTC);
	QLocale local = QLocale::English;
	QString formateStr = "ddd, MMM dd, yyyy";
	if (IS_KR()) {
		local = QLocale::Korean;
		formateStr = "yyyy. MM. dd. ddd";
	}
	yearMonthDay = local.toString(dateTime, formateStr);
}

qint64 PLSNaverShoppingLIVEAPI::getLocalTimeStamp(const QDate &date, const QString &hourString, const QString &minuteString, int AmOrPm)
{
	//12 hour   to  24 hour
	//12:00 AM  to  00:00
	//12:00 PM  to  12:00
	QString apString = AmOrPm == AM ? AM_STR : PM_STR;
	QString timeString = QString("%1:%2 %3").arg(hourString).arg(minuteString).arg(apString);
	QTime time = QLocale::c().toTime(timeString, "hh:mm A");
	QDateTime dateTime(date, time);
	return dateTime.toSecsSinceEpoch();
}

bool PLSNaverShoppingLIVEAPI::isRhythmicityProduct(const QString &matchCategoryId, const QString &matchCategoryName)
{
	QJsonArray list = PLSSyncServerManager::instance()->getProductCategoryJsonArray();
	QString matchProcutNo = matchCategoryId.split(">").last();
	for (auto variant : list) {
		QJsonObject object = variant.toObject();
		QString categoryId = object.value(name2str(categoryId)).toString();
		QString categoryName = object.value(name2str(categoryName)).toString();
		if (categoryId == matchProcutNo && categoryName == matchCategoryName) {
			return true;
		}
	}
	return false;
}

void PLSNaverShoppingLIVEAPI::getStoreLoginUrl(const QWidget *widget, const std::function<void(const QString &storeLoginUrl)> &ok, const std::function<void()> &fail)
{
	pls::http::request(
		pls::http::Request()
			.method(pls::http::Method::Get)
			.hmacUrl(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_GET_STORE_LOGIN), PLS_PC_HMAC_KEY.toUtf8())
			.rawHeader("X-LIVE-COMMERCE-DEVICE-ID", pls_get_navershopping_deviceId())
			.rawHeader(CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY, CHANNEL_NAVER_SHOPPING_LIVE_HEADER)
			.userAgent(getUserAgent())
			.jsonContentType()
			.receiver(widget)
			.withLog()
			.objectOkResult([widget, ok, fail](const pls::http::Reply &, const QJsonObject &object) {
				if (QString loginUrl = JSON_getString(object, smartStoreLoginUrl); !loginUrl.isEmpty() && (loginUrl.startsWith("http://") || loginUrl.startsWith("https://"))) {
					pls_async_call_mt(widget, [ok, loginUrl]() { pls_invoke_safe(ok, loginUrl); });
				} else {
					PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live get store login url failed, invalid login url: %s", loginUrl.toUtf8().constData());
					pls_async_call_mt(widget, [fail]() { pls_invoke_safe(fail); });
				}
			})
			.failResult([widget, fail](const pls::http::Reply &) {
				PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live get store login url failed");
				pls_async_call_mt(widget, [fail]() { pls_invoke_safe(fail); });
			}));
}

void PLSNaverShoppingLIVEAPI::getJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const char *log, const RequestOkCallback &ok, const RequestFailedCallback &fail, const QObject *receiver,
				      const ReceiverIsValid &receiverIsValid, const QVariantMap &headers, const QVariantMap &params, bool useAccessToken, const ApiPropertyMap &apiPropertyMap,
				      bool isIgnoreEmptyJson, bool workInMainThread)
{
	pls_unused(platform);
	pls::http::Request request;
	if (useAccessToken) {
		request.rawHeader("X-LIVE-COMMERCE-AUTH", platform->getAccessToken());
		request.rawHeader("X-LIVE-COMMERCE-DEVICE-ID", platform->getSoftwareUUid());
		if (platform->getChannelCookie().length() > 0 && url.url.toString() == urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_TOKEN)) {
			request.cookie(platform->getChannelCookie());
		}
	} else {
		request.cookie(platform->getChannelCookie());
	}
	if (workInMainThread) {
		request.workInMainThread();
	}
	request.rawHeader(CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY, CHANNEL_NAVER_SHOPPING_LIVE_HEADER);
	pls::http::request(request.hmacUrl(url.url, PLS_PC_HMAC_KEY.toUtf8())
				   .rawHeaders(headers)
				   .jsonContentType()
				   .userAgent(getUserAgent())
				   .urlParams(params)
				   .withLog(url.maskingUrl)
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .receiver(receiver, receiverIsValid)
				   .okResult([platform, log, ok, fail, isIgnoreEmptyJson, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   if (reply.reply()->hasRawHeader("X-LIVE-COMMERCE-AUTH")) {
						   platform->setAccessToken(reply.rawHeader("X-LIVE-COMMERCE-AUTH"));
					   }
					   processRequestOkCallback(platform, reply.data(), log, receiver, receiverIsValid, ok, fail, reply.statusCode(), isIgnoreEmptyJson);
				   })
				   .failResult([platform, log, fail, apiPropertyMap, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestFailCallback(platform, reply.statusCode(), reply.data(), log, reply.error(), receiver, receiverIsValid, fail, apiPropertyMap);
				   }));
}

void PLSNaverShoppingLIVEAPI::postJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const QJsonObject &json, const char *log, const std::function<void(const QJsonDocument &)> &ok,
				       const std::function<void(PLSAPINaverShoppingType apiType)> &fail, const QObject *receiver, const ReceiverIsValid &receiverIsValid, const QVariantMap &headers,
				       const ApiPropertyMap &apiPropertyMap)
{
	pls_unused(platform);

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post)
				   .hmacUrl(url.url, PLS_PC_HMAC_KEY.toUtf8())
				   .rawHeaders(headers)
				   .rawHeader("X-LIVE-COMMERCE-AUTH", platform->getAccessToken())
				   .rawHeader("X-LIVE-COMMERCE-DEVICE-ID", platform->getSoftwareUUid())
				   .rawHeader(CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY, CHANNEL_NAVER_SHOPPING_LIVE_HEADER)
				   .userAgent(getUserAgent())
				   .jsonContentType()
				   .body(json)
				   .withLog(url.maskingUrl)
				   .receiver(receiver, receiverIsValid)
				   .okResult([platform, log, ok, fail, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestOkCallback(platform, reply.data(), log, receiver, receiverIsValid, ok, fail, reply.statusCode());
				   })
				   .failResult([platform, log, fail, apiPropertyMap, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestFailCallback(platform, reply.statusCode(), reply.data(), log, reply.error(), receiver, receiverIsValid, fail, apiPropertyMap);
				   }));
}

void PLSNaverShoppingLIVEAPI::putJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const QJsonObject &json, const char *log, const std::function<void(const QJsonDocument &)> &ok,
				      const std::function<void(PLSAPINaverShoppingType apiType)> &fail, const QObject *receiver, const ReceiverIsValid &receiverIsValid, const QVariantMap &headers,
				      const ApiPropertyMap &apiPropertyMap)
{
	pls_unused(platform);

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Put)
				   .hmacUrl(url.url, PLS_PC_HMAC_KEY.toUtf8())
				   .rawHeaders(headers)
				   .rawHeader("X-LIVE-COMMERCE-AUTH", platform->getAccessToken())
				   .rawHeader("X-LIVE-COMMERCE-DEVICE-ID", platform->getSoftwareUUid())
				   .rawHeader(CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY, CHANNEL_NAVER_SHOPPING_LIVE_HEADER)
				   .userAgent(getUserAgent())
				   .jsonContentType()
				   .body(json)
				   .withLog(url.maskingUrl)
				   .receiver(receiver, receiverIsValid)
				   .okResult([platform, log, ok, fail, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestOkCallback(platform, reply.data(), log, receiver, receiverIsValid, ok, fail, reply.statusCode());
				   })
				   .failResult([platform, log, fail, apiPropertyMap, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestFailCallback(platform, reply.statusCode(), reply.data(), log, reply.error(), receiver, receiverIsValid, fail, apiPropertyMap);
				   }));
}

void PLSNaverShoppingLIVEAPI::deleteJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const char *log, const std::function<void(const QJsonDocument &)> &ok,
					 const std::function<void(PLSAPINaverShoppingType apiType)> &fail, const QObject *receiver, const ReceiverIsValid &receiverIsValid,
					 const ApiPropertyMap &apiPropertyMap)
{
	pls_unused(platform);

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Delete)
				   .hmacUrl(url.url, PLS_PC_HMAC_KEY.toUtf8())
				   .rawHeader("X-LIVE-COMMERCE-AUTH", platform->getAccessToken())
				   .rawHeader("X-LIVE-COMMERCE-DEVICE-ID", platform->getSoftwareUUid())
				   .rawHeader(CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY, CHANNEL_NAVER_SHOPPING_LIVE_HEADER)
				   .userAgent(getUserAgent())
				   .withLog(url.maskingUrl)
				   .receiver(receiver, receiverIsValid)
				   .okResult([platform, log, ok, fail, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestOkCallback(platform, reply.data(), log, receiver, receiverIsValid, ok, fail, reply.statusCode());
				   })
				   .failResult([platform, log, fail, apiPropertyMap, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestFailCallback(platform, reply.statusCode(), reply.data(), log, reply.error(), receiver, receiverIsValid, fail, apiPropertyMap);
				   }));
}

void PLSNaverShoppingLIVEAPI::customJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const char *log, const std::function<void(const QJsonDocument &)> &ok,
					 const std::function<void(PLSAPINaverShoppingType apiType)> &fail, const QObject *receiver, const ReceiverIsValid &receiverIsValid,
					 const ApiPropertyMap &apiPropertyMap, bool isIgnoreEmptyJson)
{
	pls_unused(platform);

	pls::http::request(pls::http::Request()
				   .method("PATCH")
				   .hmacUrl(url.url, PLS_PC_HMAC_KEY.toUtf8())
				   .rawHeader("X-LIVE-COMMERCE-AUTH", platform->getAccessToken())
				   .rawHeader("X-LIVE-COMMERCE-DEVICE-ID", platform->getSoftwareUUid())
				   .rawHeader(CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY, CHANNEL_NAVER_SHOPPING_LIVE_HEADER)
				   .userAgent(getUserAgent())
				   .withLog(url.maskingUrl)
				   .receiver(receiver, receiverIsValid)
				   .okResult([platform, log, ok, fail, receiver, receiverIsValid, isIgnoreEmptyJson](const pls::http::Reply &reply) {
					   processRequestOkCallback(platform, reply.data(), log, receiver, receiverIsValid, ok, fail, reply.statusCode(), isIgnoreEmptyJson);
				   })
				   .failResult([platform, log, fail, apiPropertyMap, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestFailCallback(platform, reply.statusCode(), reply.data(), log, reply.error(), receiver, receiverIsValid, fail, apiPropertyMap);
				   }));
}

PLSNaverShoppingLIVEAPI::ScheduleInfo::ScheduleInfo(const QJsonObject &object)
	: title(JSON_getString(object, title)),
	  description(JSON_getString(object, description)),
	  serviceName(JSON_getString(object, serviceName)),
	  id(QString::number(JSON_getInt64(object, id))),
	  standByImage(JSON_getString(object, standByImage)),
	  broadcasterId(JSON_getString(object, broadcasterId)),
	  broadcastType(JSON_getString(object, broadcastType)),
	  serviceId(JSON_getString(object, serviceId)),
	  releaseLevel(JSON_getString(object, releaseLevel)),
	  expectedStartDate(JSON_getString(object, expectedStartDate)),
	  broadcastEndUrl(JSON_getString(object, broadcastEndUrl)),
	  externalExposeAgreementStatus(JSON_getString(object, externalExposeAgreementStatus)),
	  searchable(JSON_getBool(object, searchable)),
	  highQualityAvailable(JSON_getBool(object, highQualityAvailable))
{
	for (auto it : object["shoppingProducts"].toArray()) {
		shoppingProducts.append(ProductInfo(it.toObject()));
	}
	QString tempTime = expectedStartDate;
	timeStamp = PLSDateFormate::koreanTimeStampToLocalTimeStamp(tempTime);
	startTimeUTC = PLSDateFormate::timeStampToUTCString(timeStamp);
	displayCategory = LiveCategory(object["displayCategory"].toObject());
}

bool PLSNaverShoppingLIVEAPI::ScheduleInfo::checkStartTime(int beforeSeconds, int afterSeconds) const
{
	if ((timeStamp <= 0) || expectedStartDate.isEmpty()) {
		return false;
	}
	if (qint64 current = QDateTime::currentMSecsSinceEpoch() / 1000; (qint64(current - beforeSeconds) <= timeStamp) && (qint64(current + afterSeconds) >= timeStamp)) {
		return true;
	}
	return false;
}

PLSNaverShoppingLIVEAPI::LiveCategory::LiveCategory(const QJsonObject &object)
	: id(JSON_getString(object, id)), displayName(JSON_getString(object, displayName)), parentId(JSON_getString(object, parentId))
{
	for (auto it : object["children"].toArray()) {
		children.append(LiveCategory(it.toObject()));
	}
}
