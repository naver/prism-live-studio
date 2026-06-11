//#include <Windows.h>

#include "PLSNaverShoppingLIVEAPI.h"

#include <ctime>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDateTime>
#include <QDomDocument>
#include <QMetaEnum>
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
#include "PLSChannelsVirualAPI.h"
#include "log/log.h"
#include "PLSDateFormate.h"
#include "ui-config.h"
#include "libhttp-client.h"

using namespace common;
constexpr auto CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY = "";

#define UPLOAD_IMAGE_PARAM_IMAGE QStringLiteral("")
#define UPLOAD_IMAGE_PARAM_USERID QStringLiteral("")
#define HEADER_MINE_APPLICATION QStringLiteral("")
#define ApiPropertyShowAlert QStringLiteral("")
#define ApiPropertyHandleTokenExpire QStringLiteral("")

const int AM = 0;
const int PM = 1;
auto constexpr AM_STR = "AM";
auto constexpr PM_STR = "PM";

#ifdef ENABLE_TEST
PLSErrorHandler::RetData testRetData;
#endif

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
	return "";
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
	  expectedStartDate(JSON_getString(object, expectedStartDate)),
	  searchable(JSON_getBool(object, searchable)),
	  sendNotification(JSON_getBool(object, sendNotification))
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
	  hasLiveDiscountRate(JSON_hasPriceOrRateKey(object, liveDiscountRate) || JSON_hasPriceOrRateKey(object, liveDiscountPrice)),
	  activeLiveDiscount(JSON_getBool(object, activeLiveDiscount)),
	  introducing(JSON_getBool(object, introducing)),
	  key(JSON_getString(object, key)),
	  name(JSON_getString(object, productName)),
	  imageUrl(JSON_getString(object, image)),
	  mallName(JSON_getString(object, mallName)),
	  productStatus(JSON_getString(object, productStatus)),
	  linkUrl(JSON_getString(object, productEndUrl)),
	  discountedSalePrice(JSON_getDouble(object, discountedSalePrice)),
	  discountRate(JSON_getDouble(object, discountRate)),
	  specialPrice(JSON_getDouble(object, specialPrice)),
	  liveDiscountPrice(JSON_getDouble(object, liveDiscountPrice)),
	  liveDiscountRate(JSON_getDouble(object, liveDiscountRate)),
	  wholeCategoryId(JSON_getString(object, wholeCategoryId)),
	  wholeCategoryName(JSON_getString(object, wholeCategoryName)),
	  attachmentType(JSON_getString(object, attachmentType)),
	  salePrice(JSON_getDouble(object, salePrice)),
	  attachable(JSON_getBoolEx(object, attachable, true))
{
	channelNo = QString::number(JSON_getInt64(object, channelNo));
	accountNo = QString::number(JSON_getInt64(object, accountNo));

	setProductType();
}

void PLSNaverShoppingLIVEAPI::ProductInfo::setAttachmentType(PLSProductType productType_)
{
	productType = productType_;
	if (productType == PLSProductType::MainProduct) {
		attachmentType = "MAIN";
	} else if (productType == PLSProductType::SubProduct) {
		attachmentType = "SUB";
	}
}

void PLSNaverShoppingLIVEAPI::ProductInfo::setProductType()
{
	if (attachmentType == "MAIN") {
		productType = PLSProductType::MainProduct;
	} else if (attachmentType == "SUB") {
		productType = PLSProductType::SubProduct;
	}
}

bool PLSNaverShoppingLIVEAPI::ProductInfo::discountRateIsValid() const
{
	return (productStatus == PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_SALE) ? hasDiscountRate : false;
}

bool PLSNaverShoppingLIVEAPI::ProductInfo::specialPriceIsValid() const
{
	return (productStatus == PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_SALE) ? hasSpecialPrice : false;
}

bool PLSNaverShoppingLIVEAPI::ProductInfo::liveDiscountRateIsValid() const
{
	return (productStatus == PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_SALE) ? hasLiveDiscountRate : false;
}

pls::http::Request PLSNaverShoppingLIVEAPI::storeChannelProductSearch(PLSPlatformNaverShoppingLIVE *platform, const QString &channelNo, const QString &productName, int page, int pageSize,
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
	auto failCallback = [callback](PLSAPINaverShoppingType, const QByteArray &data) { callback(false, {}, false, 0); };

	if (!productName.isEmpty()) {
		return getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_STORE_CHANNEL_PRODUCT_SEARCH.arg(channelNo))), PLSAPINaverShoppingUrlType::PLSStoreChannelProductSearch, okCallback,
			       failCallback, receiver ? receiver : platform, receiverIsValid, {},
			       {{"productName", QString::fromUtf8(productName.toUtf8().toPercentEncoding())}, {"page", page}, {"pageSize", pageSize}}, true, REQUST_NO_ALERT);
	} else {
		return getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_STORE_CHANNEL_PRODUCT_SEARCH.arg(channelNo))), PLSAPINaverShoppingUrlType::PLSStoreChannelProductSearch, okCallback,
			       failCallback, receiver ? receiver : platform, receiverIsValid, {}, {{"page", page}, {"pageSize", pageSize}}, true, REQUST_NO_ALERT);
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
				   .id(NAVER_SHOPPING_LIVE)
				   .hmacUrl(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_TOKEN), PLS_PC_HMAC_KEY.toUtf8()) //
				   .userAgent(getUserAgent())
				   .jsonContentType()
				   .withLog()
				   .receiver(receiver, receiverIsValid)
				   .objectOkResult(
					   [callback, platform, receiver, receiverIsValid](const pls::http::Reply &reply, const QJsonObject &jsonObject) {
						   PLSNaverShoppingLIVEDataManager::instance()->downloadImage(
							   platform, JSON_getString(jsonObject, profileImageUrl),
							   [callback, accessToken = reply.rawHeader("X-LIVE-COMMERCE-AUTH"), jsonObject, data = reply.data(),
							    statusCode = reply.statusCode()](bool result, const QString &imagePath) {
								   PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE,
									    "Naver Shopping Live refresh token and get user info success and image download succeed:%d", result);
								   NaverShoppingUserInfo info(jsonObject);
								   info.profileImagePath = imagePath;
								   info.accessToken = accessToken;
								   callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, info, data, statusCode, QNetworkReply::NoError);
							   },
							   receiver, receiverIsValid);
					   },
					   [callback, receiver, receiverIsValid](const pls::http::Reply &reply, const QJsonParseError &error) {
						   PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live refresh token and get user info failed, reason: %s",
							     error.errorString().toUtf8().constData());
						   pls_async_call_mt(receiver, receiverIsValid, [callback, data = reply.data(), statusCode = reply.statusCode(), error = reply.error()]() {
							   PLSErrorHandler::ExtraData exData;
							   exData.urlEn = CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_TOKEN;
							   PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ErrCode::CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_UNKNOWN_ERROR, NAVER_SHOPPING_LIVE,
												 "TempErrorTryAgain", exData);
						   });
					   })
				   .failResult([callback, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live refresh token and get user info failed, networkError: %d, statusCode: %d, data: %s",
						     reply.error(), reply.statusCode(), reply.data().constData());

					   pls_async_call_mt(receiver, receiverIsValid, [callback, data = reply.data(), statusCode = reply.statusCode(), error = reply.error()]() {
						   callback(PLSAPINaverShoppingType::PLSNaverShoppingFailed, {}, data, statusCode, error);
					   });
				   }));
}

void PLSNaverShoppingLIVEAPI::uploadImage(PLSPlatformNaverShoppingLIVE *platform, const QString &imagePath, const UploadImageCallback &callback, const QObject *receiver,
					  const ReceiverIsValid &receiverIsValid)
{
	auto sessionKeyOkCallback = [platform, imagePath, callback, receiver, receiverIsValid](const QJsonDocument &json) {
		QJsonObject object = json.object();
		QString sessionKey = JSON_getString(object, sessionKey);
		QString uploaderDomain = "";
		QString deliveryDomain = "";
		if (uploaderDomain.isEmpty() || deliveryDomain.isEmpty() || sessionKey.isEmpty()) {
			callback(PLSAPINaverShoppingType::PLSNaverShoppingFailed, QString(), "");
			return;
		}
		QString uploadURL = "";
		uploadLocalImage(platform, uploadURL, deliveryDomain, imagePath, callback, receiver, receiverIsValid);
	};
	Url url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_GET_SEESION_KEY.arg(platform->getUserInfo().broadcasterId)),
		urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_GET_SEESION_KEY.arg(pls_masking_person_info(platform->getUserInfo().broadcasterId))));
	auto sessionKeyFailCallback = [callback](PLSAPINaverShoppingType apiType, const QByteArray &data) { callback(apiType, QString(), data); };
	getJson(platform, url, PLSAPINaverShoppingUrlType::PLSUpdateUserImage, sessionKeyOkCallback, sessionKeyFailCallback, receiver ? receiver : platform, receiverIsValid, {}, {}, true);
}

void PLSNaverShoppingLIVEAPI::uploadLocalImage(const PLSPlatformNaverShoppingLIVE *platform, const QString &uploadUrl, const QString &deliveryUrl, const QString &imageFilePath,
					       const UploadImageCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping LIVE upload image, uploadUrl: %s,deliveryUrl = %s", uploadUrl.toUtf8().constData(), deliveryUrl.toUtf8().constData());
	pls::http::request(
		pls::http::Request()
			.method(pls::http::Method::Post)
			.url(uploadUrl)
			.userAgent(getUserAgent())
			.id(NAVER_SHOPPING_LIVE)
			.form(UPLOAD_IMAGE_PARAM_IMAGE, imageFilePath, true)
			.form(UPLOAD_IMAGE_PARAM_USERID, platform->getUserInfo().broadcasterId.toUtf8())
			.receiver(receiver, receiverIsValid)
			.okResult([callback, receiver, receiverIsValid, deliveryUrl](const pls::http::Reply &reply) {
				if (QString url = getUploadImageDomain(reply.data()); !url.isEmpty()) {
					QString imageUrl = QString("%1/%2").arg(deliveryUrl).arg(url);
					pls_async_call_mt(receiver, receiverIsValid,
							  [callback, imageUrl, data = reply.data()]() { callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, imageUrl, data); });
				} else {

					pls_async_call_mt(receiver, receiverIsValid, [callback, data = reply.data()]() { callback(PLSAPINaverShoppingType::PLSNaverShoppingFailed, QString(), data); });
				}
			})
			.failResult([callback, receiver, receiverIsValid](const pls::http::Reply &reply) {
				PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping LIVE upload image failed");
				pls_async_call_mt(receiver, receiverIsValid, [callback, data = reply.data()]() { callback(PLSAPINaverShoppingType::PLSNaverShoppingFailed, QString(), data); });
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
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, info, "");
	};
	auto failCallback = [callback](PLSAPINaverShoppingType apiType, const QByteArray &data) { callback(apiType, NaverShoppingLivingInfo(), data); };
	postJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_CREATE_NOW_LIVING)), body, PLSAPINaverShoppingUrlType::PLSCreateNowLiving, okCallback, failCallback, receiver, receiverIsValid,
		 QVariantMap());
}

void PLSNaverShoppingLIVEAPI::createScheduleLiving(PLSPlatformNaverShoppingLIVE *platform, const QString &scheduleId, const CreateNowLivingCallback &callback, const QObject *receiver,
						   const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &json) {
		QJsonObject object = json.object();
		NaverShoppingLivingInfo info(object);
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, info, "");
	};
	auto failCallback = [callback](PLSAPINaverShoppingType apiType, const QByteArray &data) { callback(apiType, NaverShoppingLivingInfo(), data); };
	customJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_CREATE_SCHEDULE_LIVING.arg(scheduleId))), PLSAPINaverShoppingUrlType::PLSCreateScheduleLiving, okCallback, failCallback,
		   receiver, receiverIsValid);
}

void PLSNaverShoppingLIVEAPI::getLivingInfo(PLSPlatformNaverShoppingLIVE *platform, bool livePolling, const GetLivingInfoCallback &callback, const QObject *receiver,
					    const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &json) {
		QJsonObject object = json.object();
		NaverShoppingLivingInfo info(object);
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, info);
	};
	auto failCallback = [callback](PLSAPINaverShoppingType apiType, const QByteArray &data) { callback(apiType, NaverShoppingLivingInfo()); };
	if (livePolling) {
		QVariantMap propertyMap;
		propertyMap.insert(ApiPropertyShowAlert, false);
		getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_GET_LIVING_INFO.arg(platform->getLivingInfo().id))), PLSAPINaverShoppingUrlType::PLSGetLivingInfo, okCallback,
			failCallback, receiver ? receiver : platform, receiverIsValid, {}, {}, true, {{PLSAPINaverShoppingType::PLSNaverShoppingAll, propertyMap}});
	} else {
		getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_GET_LIVING_INFO.arg(platform->getLivingInfo().id))), PLSAPINaverShoppingUrlType::PLSGetLivingInfo, okCallback,
			failCallback, receiver ? receiver : platform, receiverIsValid, {}, {}, true);
	}
}

void PLSNaverShoppingLIVEAPI::getLivingInfo(PLSPlatformNaverShoppingLIVE *platform, const QString &scheduleId, bool livePolling, const GetLivingInfoCallback &callback, const QObject *receiver,
					    const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &json) {
		QJsonObject object = json.object();
		NaverShoppingLivingInfo info(object);
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, info);
	};
	auto failCallback = [callback](PLSAPINaverShoppingType apiType, const QByteArray &data) { callback(apiType, NaverShoppingLivingInfo()); };
	if (livePolling) {
		QVariantMap propertyMap;
		propertyMap.insert(ApiPropertyShowAlert, false);
		getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_GET_LIVING_INFO.arg(scheduleId))), PLSAPINaverShoppingUrlType::PLSGetLivingInfo, okCallback, failCallback,
			receiver ? receiver : platform, receiverIsValid, {}, {}, true, {{PLSAPINaverShoppingType::PLSNaverShoppingAll, propertyMap}});
	} else {
		getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_GET_LIVING_INFO.arg(scheduleId))), PLSAPINaverShoppingUrlType::PLSGetLivingInfo, okCallback, failCallback,
			receiver ? receiver : platform, receiverIsValid, {}, {}, true);
	}
}

void PLSNaverShoppingLIVEAPI::updateNowLiving(PLSPlatformNaverShoppingLIVE *platform, const QString &id, const QJsonObject &body,
					      const std::function<void(PLSAPINaverShoppingType apiType, const QByteArray &data)> &callback, const QObject *receiver,
					      const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &) { callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, ""); };
	auto failCallback = [callback](PLSAPINaverShoppingType apiType, const QByteArray &data) { callback(apiType, data); };
	putJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVING.arg(id))), body, PLSAPINaverShoppingUrlType::PLSUpdateNowLiving, okCallback, failCallback, receiver, receiverIsValid,
		QVariantMap());
}

void PLSNaverShoppingLIVEAPI::updateScheduleInfo(PLSPlatformNaverShoppingLIVE *platform, const QString &id, const QJsonObject &body, const UpdateScheduleCallback &callback, const QObject *receiver,
						 const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback](const QJsonDocument &json) {
		QJsonObject object = json.object();
		ScheduleInfo info(object);
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, info, "");
	};
	auto failCallback = [callback](PLSAPINaverShoppingType apiType, const QByteArray &data) { callback(apiType, ScheduleInfo(), data); };
	putJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVING.arg(id))), body, PLSAPINaverShoppingUrlType::PLSUpdateScheduleLiving, okCallback, failCallback, receiver,
		receiverIsValid, QVariantMap());
}

void PLSNaverShoppingLIVEAPI::stopLiving(const PLSPlatformNaverShoppingLIVE *platform, bool needVideoSave, const std::function<void(bool ok)> &callback, const QObject *receiver,
					 const ReceiverIsValid &receiverIsValid)
{
	auto url = Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_STOP_LIVING.arg(platform->getLivingInfo().id).arg(needVideoSave)));
	pls::http::request(pls::http::Request()
				   .method("PATCH")
				   .id(NAVER_SHOPPING_LIVE)
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
				   .id(NAVER_SHOPPING_LIVE)
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
	auto failCallback = [callback](PLSAPINaverShoppingType, const QByteArray &) { callback(false, {}); };

	getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_TOKEN)), PLSAPINaverShoppingUrlType::PLSGetSelectiveAccountStores, okCallback, failCallback,
		receiver ? receiver : platform, receiverIsValid, {}, {}, true, REQUST_NO_ALERT);
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
		callback(PLSAPINaverShoppingType::PLSNaverShoppingSuccess, list, page, totalCount, "");
	};
	auto failCallback = [callback](PLSAPINaverShoppingType apiType, const QByteArray &data) { callback(apiType, QList<ScheduleInfo>(), 0, 0, data); };
	if (isNotice) {
		QVariantMap propertyMap;
		propertyMap.insert(ApiPropertyShowAlert, false);
		propertyMap.insert(ApiPropertyHandleTokenExpire, false);
		getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_SCHEDULE_LIST)), PLSAPINaverShoppingUrlType::PLSGetScheduleListFilter, okCallback, failCallback,
			receiver ? receiver : platform, receiverIsValid, {}, {{"pageNum", currentPage}, {"pageSize", SCHEDULE_PER_PAGE_MAX_NUM}}, true,
			{{PLSAPINaverShoppingType::PLSNaverShoppingAll, propertyMap}}, false, true);
	} else {
		getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_SCHEDULE_LIST)), PLSAPINaverShoppingUrlType::PLSGetScheduleList, okCallback, failCallback, receiver ? receiver : platform,
			receiverIsValid, {}, {{"pageNum", currentPage}, {"pageSize", SCHEDULE_PER_PAGE_MAX_NUM}}, true, {}, false, true);
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
	auto failCallback = [callback](PLSAPINaverShoppingType apiType, const QByteArray &) { callback(apiType, QList<LiveCategory>()); };
	getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_CATEGORY_LIST)), PLSAPINaverShoppingUrlType::PLSGetCategoryList, okCallback, failCallback, receiver ? receiver : platform,
		receiverIsValid, {}, {});
}

void PLSNaverShoppingLIVEAPI::sendPushNotification(PLSPlatformNaverShoppingLIVE *platform, const QObject *receiver, const RequestOkCallback &ok, const RequestFailedCallback &fail,
						   const ReceiverIsValid &receiverIsValid)
{
	NaverShoppingLivingInfo liveInfo = platform->getLivingInfo();
	auto apiPropertyMap = ApiPropertyMap();
	getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_PSUH_NOTIFICATION.arg(liveInfo.id))), PLSAPINaverShoppingUrlType::PLSSendPushNotification, ok, fail,
		receiver ? receiver : platform, receiverIsValid, {}, {{"categoryType", "LIVE_ONAIR"}}, true, apiPropertyMap, true);
}

void PLSNaverShoppingLIVEAPI::sendNotice(PLSPlatformNaverShoppingLIVE *platform, const QJsonObject &body, const QObject *receiver, const RequestOkCallback &ok, const RequestFailedCallback &fail,
					 const ReceiverIsValid &receiverIsValid)
{
	QVariantMap propertyMap;
	propertyMap.insert(ApiPropertyShowAlert, false);
	propertyMap.insert(ApiPropertyHandleTokenExpire, false);
	postJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_SEND_NOTICE)), body, PLSAPINaverShoppingUrlType::PLSSendNoticeRequest, ok, fail, receiver ? receiver : platform, receiverIsValid,
		 {}, {{PLSAPINaverShoppingType::PLSNaverShoppingAll, propertyMap}});
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
	auto failCallback = [callback](PLSAPINaverShoppingType, const QByteArray &data) { callback(false, false, {}); };
	getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_URL)), PLSAPINaverShoppingUrlType::PLSProductSearchByUrl, okCallback, failCallback,
		receiver ? receiver : platform, receiverIsValid, {}, {{"url", url}}, true, REQUST_NO_ALERT);
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
	auto failCallback = [callback](PLSAPINaverShoppingType, const QByteArray &data) { callback(false, {}, false, 0); };
	getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_TAG)), PLSAPINaverShoppingUrlType::PLSProductSearchByTag, okCallback, failCallback,
		receiver ? receiver : platform, receiverIsValid, {}, {{"query", QString::fromUtf8(tag.toUtf8().toPercentEncoding())}, {"page", page}, {"pageSize", pageSize}}, true, REQUST_NO_ALERT);
}

pls::http::Request PLSNaverShoppingLIVEAPI::productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, int currentPage, int pageSize, PLSProductType productType,
								      const QList<qint64> &fixedProductNos, const QList<qint64> &unfixedProductNos, const QList<qint64> &introducingProductNos,
								      const ProductSearchByProductNosSplitCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	QString searchKey;
	QStringList strProductNos;
	for (auto productNo : fixedProductNos) {
		strProductNos.append(QString::number(productNo));
		searchKey.append(QString::number(productNo)).append("-");
	}
	for (auto productNo : unfixedProductNos) {
		strProductNos.append(QString::number(productNo));
		searchKey.append(QString::number(productNo)).append("-");
	}

	bool hasNext;
	QStringList onePageProductNos = getOnePageProductNos(hasNext, strProductNos, currentPage, pageSize);
	auto requestCount = onePageProductNos.count();
	if (0 == requestCount) {
		return pls::http::Request();
	}
	return productSearchByProductNos(
		platform, productType, onePageProductNos,
		[callback, fixedProductNos, unfixedProductNos, introducingProductNos, requestCount, hasNext, searchKey](bool ok, PLSProductType productType, const QList<ProductInfo> &products) {
			if (ok) {
				QList<ProductInfo> fixedProducts;
				QList<ProductInfo> unfixedProducts;
				for (const auto &product : products) {
					auto pro = product;
					if (introducingProductNos.contains(product.productNo)) {
						pro.introducing = true;
					}
					if (fixedProductNos.contains(product.productNo)) {
						pro.represent = true;
						pro.setAttachmentType(productType);
						fixedProducts.append(pro);
					} else {
						pro.represent = false;
						pro.setAttachmentType(productType);
						unfixedProducts.append(pro);
					}
				}
				if (unfixedProducts.count() + fixedProducts.count() == requestCount) {
					callback(true, hasNext, productType, fixedProducts, unfixedProducts, searchKey);
				} else {
					callback(false, false, productType, {}, {}, searchKey);
				}
			} else {
				callback(false, false, productType, {}, {}, searchKey);
			}
		},
		receiver, receiverIsValid);
}

pls::http::Request PLSNaverShoppingLIVEAPI::productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, PLSProductType productType, const QList<qint64> &productNos,
								      const ProductSearchByProductNosAllCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	QStringList strProductNos;
	for (auto productNo : productNos)
		strProductNos.append(QString::number(productNo));
	return productSearchByProductNos(platform, productType, strProductNos, callback, receiver, receiverIsValid);
}

pls::http::Request PLSNaverShoppingLIVEAPI::productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, PLSProductType productType, const QStringList &strProductNos,
								      const ProductSearchByProductNosAllCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid)
{
	auto okCallback = [callback, productType, strProductNos](const QJsonDocument &json) {
		QJsonObject object = json.object();
		QList<ProductInfo> products;
		for (const QString &productNo : strProductNos) {
			QJsonValue value = object[productNo];
			if (value.isObject()) {
				products.append(ProductInfo(productNo.toLongLong(), value.toObject()));
			}
		}

		callback(true, productType, products);
	};
	auto failCallback = [callback, productType](PLSAPINaverShoppingType, const QByteArray &data) { callback(false, productType, {}); };

	QString productNos;
	for (int i = 0; i < strProductNos.size(); i++) {
		if (i == 0)
			productNos = pls_masking_person_info(strProductNos.at(i));
		else
			productNos.append(QString(",%1").arg(pls_masking_person_info(strProductNos.at(i))));
	}
	QString encUrl = QString("%1?productNos=%2").arg(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_PRODUCTNOS)).arg(productNos);
	return getJson(platform, Url(urlForPath(CHANNEL_NAVER_SHOPPING_LIVE_PRODUCT_SEARCH_BY_PRODUCTNOS), encUrl), PLSAPINaverShoppingUrlType::PLSProductSearchByProNos, okCallback, failCallback,
		       receiver ? receiver : platform, receiverIsValid, {}, {{"productNos", strProductNos.join(',')}}, true, REQUST_NO_ALERT);
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
				   .id(NAVER_SHOPPING_LIVE)
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

void PLSNaverShoppingLIVEAPI::processRequestOkCallback(const PLSPlatformNaverShoppingLIVE *, const QByteArray &data, PLSAPINaverShoppingUrlType urlType, const QObject *receiver,
						       const ReceiverIsValid &receiverIsValid, const RequestOkCallback &ok, const RequestFailedCallback &fail, int statusCode, bool isIgnoreEmptyJson)
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
		PLS_INFO(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live %s success", PLSNaverShoppingLIVEAPI::getStrValueByEnum(urlType));
		pls_async_call_mt(receiver, receiverIsValid, [ok, respjson]() { pls_invoke_safe(ok, respjson); });
	} else {
		PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live %s failed, reason: %s", PLSNaverShoppingLIVEAPI::getStrValueByEnum(urlType),
			  jsonError.errorString().toUtf8().constData());
		pls_async_call_mt(receiver, receiverIsValid, [fail, statusCode, data]() {
			pls_invoke_safe(fail, statusCode == 204 ? PLSAPINaverShoppingType::PLSNaverShoppingNotFound204 : PLSAPINaverShoppingType::PLSNaverShoppingFailed, data);
		});
	}
}

void PLSNaverShoppingLIVEAPI::processRequestFailCallback(PLSPlatformNaverShoppingLIVE *platform, int statusCode, const QByteArray &data, PLSAPINaverShoppingUrlType urlType,
							 QNetworkReply::NetworkError networkError, const QString &urlPath, const QObject *receiver, const ReceiverIsValid &receiverIsValid,
							 const RequestFailedCallback &fail, const ApiPropertyMap &apiPropertyMap)
{
	if (pls_get_app_exiting()) {
		return;
	}

	PLSAPINaverShoppingType apiType = PLSAPINaverShoppingType::PLSNaverShoppingFailed;

	if (networkError > QNetworkReply::NoError && networkError <= QNetworkReply::UnknownNetworkError) {
		apiType = PLSAPINaverShoppingType::PLSNaverShoppingNetworkError;
		PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live %s network error, statuscode: %d", PLSNaverShoppingLIVEAPI::getStrValueByEnum(urlType), statusCode);
	} else if (statusCode == 401) {
		auto root = QJsonDocument::fromJson(data).object();
		qint64 errorCode = JSON_getInt64(root, errorCode);
		if (errorCode == 1031) {
			PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live %s no live right", PLSNaverShoppingLIVEAPI::getStrValueByEnum(urlType));
		} else {
			apiType = PLSAPINaverShoppingType::PLSNaverShoppingInvalidAccessToken;
			PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live %s invalid access token", PLSNaverShoppingLIVEAPI::getStrValueByEnum(urlType));
		}
	}
	PLSErrorHandler::ExtraData extraData;
	extraData.pathValueMap["logContent"] = QMetaEnum::fromType<PLSAPINaverShoppingUrlType>().valueToKey(static_cast<int>(urlType));
	extraData.urlEn = urlPath;

	PLSErrorHandler::RetData retData = PLSErrorHandler::getAlertString({statusCode, networkError, data}, NAVER_SHOPPING_LIVE, "", extraData);
	if (retData.isExactMatch) {
		pls_async_call(platform, receiver, receiverIsValid,
			       [platform, apiType, apiPropertyMap, retData, urlType]() { platform->handleCommonApiType(retData, apiType, urlType, apiPropertyMap); });
		apiType = PLSAPINaverShoppingType::PLSNaverShoppingErrorMatched;
	}
#ifdef ENABLE_TEST
	testRetData = retData;
#endif
	// handle failed type by custom
	pls_async_call_mt(receiver, receiverIsValid, [fail, apiType, data]() { pls_invoke_safe(fail, apiType, data); });
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

void PLSNaverShoppingLIVEAPI::getStoreLoginUrl(const QWidget *widget, const std::function<void(const QString &storeLoginUrl)> &ok, const std::function<void(const QByteArray &object)> &fail)
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
			.id(NAVER_SHOPPING_LIVE)
			.objectOkResult([widget, ok, fail](const pls::http::Reply &reply, const QJsonObject &object) {
				if (QString loginUrl = JSON_getString(object, smartStoreLoginUrl); !loginUrl.isEmpty() && (loginUrl.startsWith("http://") || loginUrl.startsWith("https://"))) {
					pls_async_call_mt(widget, [ok, loginUrl]() { pls_invoke_safe(ok, loginUrl); });
				} else {
					PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live get store login url failed, invalid login url: %s", loginUrl.toUtf8().constData());
					pls_async_call_mt(widget, [fail, data = reply.data()]() { pls_invoke_safe(fail, data); });
				}
			})
			.failResult([widget, fail](const pls::http::Reply &reply) {
				PLS_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live get store login url failed");
				pls_async_call_mt(widget, [fail, data = reply.data()]() { pls_invoke_safe(fail, data); });
			}));
}

QStringList PLSNaverShoppingLIVEAPI::getOnePageProductNos(bool &hasNextPage, const QStringList &strProductNos, int currentPage, int pageSize)
{
	hasNextPage = ((currentPage + 1) * pageSize) < strProductNos.count();
	QStringList onePageProductNos;
	for (int i = currentPage * pageSize, count = qMin(strProductNos.count(), (currentPage + 1) * pageSize); i < count; ++i) {
		onePageProductNos.append(strProductNos[i]);
	}
	return onePageProductNos;
}

void PLSNaverShoppingLIVEAPI::getErrorCodeOrErrorMessage(const QByteArray &array, QString &errorCode, QString &errorMsg)
{
	auto doc = QJsonDocument::fromJson(array);
	auto root = doc.object();
	if (root.contains(name2str(errorCode))) {
		errorCode = JSON_getString(root, errorCode);
	} else if (root.contains(name2str(code))) {
		errorCode = JSON_getString(root, code);
	} else if (root.contains(name2str(rtn_cd))) {
		errorCode = JSON_getString(root, rtn_cd);
	}

	if (root.contains(name2str(errorMessage))) {
		errorMsg = JSON_getString(root, errorMessage);
	} else if (root.contains(name2str(message))) {
		errorMsg = JSON_getString(root, message);
	} else if (root.contains(name2str(rtn_msg))) {
		errorMsg = JSON_getString(root, rtn_msg);
	}
}

QString PLSNaverShoppingLIVEAPI::generateMsgWithErrorCodeOrErrorMessage(const QString &content, const QString &errorCode, const QString &errorMsg)
{
	QString errorContent = content;
	if (!errorCode.isEmpty()) {
		errorContent = errorContent + "\n" + QTStr("NaverShoppingLive.Alert.Error.Code") + errorCode;
	}
	if (!errorMsg.isEmpty()) {
		errorContent = errorContent + "\n" + QTStr("NaverShoppingLive.Alert.Error.Message") + errorMsg;
	}
	return errorContent;
}

PLSErrorHandler::RetData PLSNaverShoppingLIVEAPI::showAlertByPrismCodeWithErrorMsg(const QByteArray &array, PLSErrorHandler::ErrCode prismCode, const QString &platformName,
										   const QString &customErrName, const PLSErrorHandler::ExtraData &extraData, QWidget *showParent)
{

	QString errorCode, errorMsg;
	getErrorCodeOrErrorMessage(array, errorCode, errorMsg);
	PLSErrorHandler::ExtraData newData = extraData;
	if (!errorCode.isEmpty()) {
		newData.pathValueMap["errorCode"] = errorCode;
	}
	if (!errorMsg.isEmpty()) {
		newData.pathValueMap["errorMessage"] = errorMsg;
	}

	return PLSErrorHandler::showAlertByPrismCode(prismCode, NAVER_SHOPPING_LIVE, customErrName, newData, showParent);
}

const char *PLSNaverShoppingLIVEAPI::getStrValueByEnum(PLSAPINaverShoppingUrlType urlType)
{
	switch (urlType) {
	case PLSAPINaverShoppingUrlType::PLSNone:
		break;
	case PLSAPINaverShoppingUrlType::PLSRefreshToken:
		return "get navershopping user info";
	case PLSAPINaverShoppingUrlType::PLSCreateScheduleLiving:
		return "create navershopping schedule living";
	case PLSAPINaverShoppingUrlType::PLSCreateNowLiving:
		return "create navershopping now living";
	case PLSAPINaverShoppingUrlType::PLSUpdateScheduleLiving:
		return "update navershopping schedule living";
	case PLSAPINaverShoppingUrlType::PLSUpdateNowLiving:
		return "update navershopping now living";
	case PLSAPINaverShoppingUrlType::PLSGetScheduleList:
		return "get schedule list";
	case PLSAPINaverShoppingUrlType::PLSGetScheduleListFilter:
		return "get schedule list filter beforeSeconds and afterSeconds";
	case PLSAPINaverShoppingUrlType::PLSGetLivingInfo:
		return "get living info";
	case PLSAPINaverShoppingUrlType::PLSGetCategoryList:
		return "get category list";
	case PLSAPINaverShoppingUrlType::PLSStoreChannelProductSearch:
		return "store channel product search";
	case PLSAPINaverShoppingUrlType::PLSUpdateUserImage:
		return "update user image";
	case PLSAPINaverShoppingUrlType::PLSGetSelectiveAccountStores:
		return "get selective account stores";
	case PLSAPINaverShoppingUrlType::PLSSendPushNotification:
		return "send push notification";
	case PLSAPINaverShoppingUrlType::PLSSendNoticeRequest:
		return "send navershopping notice request";
	case PLSAPINaverShoppingUrlType::PLSProductSearchByUrl:
		return "product search by url";
	default:
		break;
	}
	return "";
}

pls::http::Request PLSNaverShoppingLIVEAPI::getJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, PLSAPINaverShoppingUrlType urlType, const RequestOkCallback &ok,
						    const RequestFailedCallback &fail, const QObject *receiver, const ReceiverIsValid &receiverIsValid, const QVariantMap &headers,
						    const QVariantMap &params, bool useAccessToken, const ApiPropertyMap &apiPropertyMap, bool isIgnoreEmptyJson, bool workInMainThread)
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
				   .id(NAVER_SHOPPING_LIVE)
				   .rawHeaders(headers)
				   .jsonContentType()
				   .userAgent(getUserAgent())
				   .urlParams(params)
				   .withLog(url.maskingUrl)
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .receiver(receiver, receiverIsValid)
				   .okResult([platform, urlType, ok, fail, isIgnoreEmptyJson, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   if (reply.hasRawHeader("X-LIVE-COMMERCE-AUTH")) {
						   QByteArray rawHeader = reply.rawHeader("X-LIVE-COMMERCE-AUTH");
						   pls_async_call_mt(platform, [platform, rawHeader]() { platform->setAccessToken(rawHeader); });
					   }
					   processRequestOkCallback(platform, reply.data(), urlType, receiver, receiverIsValid, ok, fail, reply.statusCode(), isIgnoreEmptyJson);
				   })
				   .failResult([platform, urlType, fail, apiPropertyMap, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestFailCallback(platform, reply.statusCode(), reply.data(), urlType, reply.error(), reply.request().originalUrl().path(), receiver,
								      receiverIsValid, fail, apiPropertyMap);
				   }));
	return request;
}

void PLSNaverShoppingLIVEAPI::postJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const QJsonObject &json, PLSAPINaverShoppingUrlType urlType,
				       const std::function<void(const QJsonDocument &)> &ok, const std::function<void(PLSAPINaverShoppingType apiType, const QByteArray &)> &fail,
				       const QObject *receiver, const ReceiverIsValid &receiverIsValid, const QVariantMap &headers, const ApiPropertyMap &apiPropertyMap)
{
	pls_unused(platform);

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post)
				   .id(NAVER_SHOPPING_LIVE)
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
				   .okResult([platform, urlType, ok, fail, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestOkCallback(platform, reply.data(), urlType, receiver, receiverIsValid, ok, fail, reply.statusCode());
				   })
				   .failResult([platform, urlType, fail, apiPropertyMap, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestFailCallback(platform, reply.statusCode(), reply.data(), urlType, reply.error(), reply.request().originalUrl().path(), receiver,
								      receiverIsValid, fail, apiPropertyMap);
				   }));
}

void PLSNaverShoppingLIVEAPI::putJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const QJsonObject &json, PLSAPINaverShoppingUrlType urlType,
				      const std::function<void(const QJsonDocument &)> &ok, const std::function<void(PLSAPINaverShoppingType apiType, const QByteArray &)> &fail,
				      const QObject *receiver, const ReceiverIsValid &receiverIsValid, const QVariantMap &headers, const ApiPropertyMap &apiPropertyMap)
{
	pls_unused(platform);

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Put)
				   .id(NAVER_SHOPPING_LIVE)
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
				   .okResult([platform, urlType, ok, fail, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestOkCallback(platform, reply.data(), urlType, receiver, receiverIsValid, ok, fail, reply.statusCode());
				   })
				   .failResult([platform, urlType, fail, apiPropertyMap, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestFailCallback(platform, reply.statusCode(), reply.data(), urlType, reply.error(), reply.request().originalUrl().path(), receiver,
								      receiverIsValid, fail, apiPropertyMap);
				   }));
}

void PLSNaverShoppingLIVEAPI::deleteJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, PLSAPINaverShoppingUrlType urlType, const std::function<void(const QJsonDocument &)> &ok,
					 const std::function<void(PLSAPINaverShoppingType apiType, const QByteArray &)> &fail, const QObject *receiver, const ReceiverIsValid &receiverIsValid,
					 const ApiPropertyMap &apiPropertyMap)
{
	pls_unused(platform);

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Delete)
				   .id(NAVER_SHOPPING_LIVE)
				   .hmacUrl(url.url, PLS_PC_HMAC_KEY.toUtf8())
				   .rawHeader("X-LIVE-COMMERCE-AUTH", platform->getAccessToken())
				   .rawHeader("X-LIVE-COMMERCE-DEVICE-ID", platform->getSoftwareUUid())
				   .rawHeader(CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY, CHANNEL_NAVER_SHOPPING_LIVE_HEADER)
				   .userAgent(getUserAgent())
				   .withLog(url.maskingUrl)
				   .receiver(receiver, receiverIsValid)
				   .okResult([platform, urlType, ok, fail, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestOkCallback(platform, reply.data(), urlType, receiver, receiverIsValid, ok, fail, reply.statusCode());
				   })
				   .failResult([platform, urlType, fail, apiPropertyMap, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestFailCallback(platform, reply.statusCode(), reply.data(), urlType, reply.error(), reply.request().originalUrl().path(), receiver,
								      receiverIsValid, fail, apiPropertyMap);
				   }));
}

void PLSNaverShoppingLIVEAPI::customJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, PLSAPINaverShoppingUrlType urlType, const std::function<void(const QJsonDocument &)> &ok,
					 const std::function<void(PLSAPINaverShoppingType apiType, const QByteArray &)> &fail, const QObject *receiver, const ReceiverIsValid &receiverIsValid,
					 const ApiPropertyMap &apiPropertyMap, bool isIgnoreEmptyJson)
{
	pls_unused(platform);

	pls::http::request(pls::http::Request()
				   .method("PATCH")
				   .id(NAVER_SHOPPING_LIVE)
				   .hmacUrl(url.url, PLS_PC_HMAC_KEY.toUtf8())
				   .rawHeader("X-LIVE-COMMERCE-AUTH", platform->getAccessToken())
				   .rawHeader("X-LIVE-COMMERCE-DEVICE-ID", platform->getSoftwareUUid())
				   .rawHeader(CHANNEL_NAVER_SHOPPING_LIVE_HEADER_KEY, CHANNEL_NAVER_SHOPPING_LIVE_HEADER)
				   .userAgent(getUserAgent())
				   .withLog(url.maskingUrl)
				   .receiver(receiver, receiverIsValid)
				   .okResult([platform, urlType, ok, fail, receiver, receiverIsValid, isIgnoreEmptyJson](const pls::http::Reply &reply) {
					   processRequestOkCallback(platform, reply.data(), urlType, receiver, receiverIsValid, ok, fail, reply.statusCode(), isIgnoreEmptyJson);
				   })
				   .failResult([platform, urlType, fail, apiPropertyMap, receiver, receiverIsValid](const pls::http::Reply &reply) {
					   processRequestFailCallback(platform, reply.statusCode(), reply.data(), urlType, reply.error(), reply.request().originalUrl().path(), receiver,
								      receiverIsValid, fail, apiPropertyMap);
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
	  highQualityAvailable(JSON_getBool(object, highQualityAvailable)),
	  sendNotification(JSON_getBool(object, sendNotification)),
	  attachable(JSON_getBoolEx(object, attachable, true))
{
	for (auto it : object["shoppingProducts"].toArray()) {
		shoppingProducts.append(ProductInfo(it.toObject()));
	}
	setExpectedStartDate(expectedStartDate);
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

void PLSNaverShoppingLIVEAPI::ScheduleInfo::setExpectedStartDate(const QString &expectedStartDate)
{
	timeStamp = PLSDateFormate::koreanTimeStampToLocalTimeStamp(expectedStartDate);
	startTimeUTC = PLSDateFormate::timeStampToUTCString(timeStamp);
}

PLSNaverShoppingLIVEAPI::LiveCategory::LiveCategory(const QJsonObject &object)
	: id(JSON_getString(object, id)), displayName(JSON_getString(object, displayName)), parentId(JSON_getString(object, parentId))
{
	for (auto it : object["children"].toArray()) {
		children.append(LiveCategory(it.toObject()));
	}
}
