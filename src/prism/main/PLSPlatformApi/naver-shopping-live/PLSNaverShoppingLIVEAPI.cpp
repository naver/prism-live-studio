#include <Windows.h>

#include "PLSNaverShoppingLIVEAPI.h"

static bool hasProductNo(const QJsonObject &object)
{
	auto iter = object.find("productNo");
	if (iter == object.end()) {
		return false;
	}

	auto value = iter.value();
	if (value.isDouble() || value.isString()) {
		return true;
	}
	return false;
}

static QString getUserAgent()
{
	return QString();
}

PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo::NaverShoppingUserInfo() {}

PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo::NaverShoppingUserInfo(const QJsonObject &object)
	: broadcastOwnerId(JSON_getString(object, broadcastOwnerId)),
	  serviceId(JSON_getString(object, serviceId)),
	  broadcasterId(JSON_getString(object, broadcasterId)),
	  profileImageUrl(JSON_getString(object, profileImageUrl)),
	  nickname(JSON_getString(object, nickname))
{
}

PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo::NaverShoppingLivingInfo() {}

PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo::NaverShoppingLivingInfo(const QJsonObject &object)
	: title(JSON_getString(object, title)),
	  status(JSON_getString(object, status)),
	  releaseLevel(JSON_getString(object, releaseLevel)),
	  id(JSON_getString(object, id)),
	  streamSeq(JSON_getString(object, streamSeq)),
	  publishUrl(JSON_getString(object, publishUrl)),
	  standByImage(JSON_getString(object, standByImage)),
	  broadcastType(JSON_getString(object, broadcastType)),
	  broadcastEndUrl(JSON_getString(object, broadcastEndUrl)),
	  displayType(JSON_getString(object, displayType)),
	  description(JSON_getString(object, description))
{
}

PLSNaverShoppingLIVEAPI::GetSelectiveAccountStore::GetSelectiveAccountStore() {}

PLSNaverShoppingLIVEAPI::GetSelectiveAccountStore::GetSelectiveAccountStore(const QJsonObject &object)
	: channelNo(QString::number(JSON_getInt64(object, channelNo))),
	  channelName(JSON_getString(object, channelName)),
	  windowIconImageUrl(JSON_getString(object, windowIconImageUrl)),
	  representImageUrl(JSON_getString(object, representImageUrl))
{
}

PLSNaverShoppingLIVEAPI::ProductInfo::ProductInfo() {}

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
	  specialPrice(JSON_getDouble(object, specialPrice))
{
}

bool PLSNaverShoppingLIVEAPI::ProductInfo::discountRateIsValid() const
{
	return false;
}

bool PLSNaverShoppingLIVEAPI::ProductInfo::specialPriceIsValid() const
{
	return false;
}

void PLSNaverShoppingLIVEAPI::storeChannelProductSearch(PLSPlatformNaverShoppingLIVE *platform, const QString &channelNo, const QString &productName, int page, int pageSize,
							StoreChannelProductSearchCallback &&callback, QObject *receiver, ReceiverIsValid receiverIsValid)
{
}

void PLSNaverShoppingLIVEAPI::refreshChannelToken(PLSPlatformNaverShoppingLIVE *platform, GetUserInfoCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid) {}

PLSAPINaverShoppingType PLSNaverShoppingLIVEAPI::getLoginFailType(const QByteArray &data)
{
	return PLSAPINaverShoppingType::PLSNaverShoppingFailed;
}

void PLSNaverShoppingLIVEAPI::uploadImage(PLSPlatformNaverShoppingLIVE *platform, const QString &imagePath, UploadImageCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid) {}

void PLSNaverShoppingLIVEAPI::uploadLocalImage(PLSPlatformNaverShoppingLIVE *platform, const QString &uploadUrl, const QString &deliveryUrl, const QString &imageFilePath, UploadImageCallback callback,
					       QObject *receiver, ReceiverIsValid receiverIsValid)
{
}

void PLSNaverShoppingLIVEAPI::createNowLiving(PLSPlatformNaverShoppingLIVE *platform, const QJsonObject &body, CreateNowLivingCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid) {}

void PLSNaverShoppingLIVEAPI::createScheduleLiving(PLSPlatformNaverShoppingLIVE *platform, const QString &scheduleId, CreateNowLivingCallback callback, QObject *receiver,
						   ReceiverIsValid receiverIsValid)
{
}

void PLSNaverShoppingLIVEAPI::getLivingInfo(PLSPlatformNaverShoppingLIVE *platform, bool livePolling, GetLivingInfoCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid) {}

void PLSNaverShoppingLIVEAPI::updateNowLiving(PLSPlatformNaverShoppingLIVE *platform, const QString &id, const QJsonObject &body, std::function<void(PLSAPINaverShoppingType apiType)> callback,
					      QObject *receiver, ReceiverIsValid receiverIsValid)
{
}

void PLSNaverShoppingLIVEAPI::stopLiving(PLSPlatformNaverShoppingLIVE *platform, bool needVideoSave, std::function<void(bool ok)> callback, QObject *receiver, ReceiverIsValid receiverIsValid) {}

void PLSNaverShoppingLIVEAPI::logoutNaverShopping(PLSPlatformNaverShoppingLIVE *platform, const QString &accessToken, QObject *receiver) {}

void PLSNaverShoppingLIVEAPI::getSelectiveAccountStores(PLSPlatformNaverShoppingLIVE *platform, GetSelectiveAccountStoresCallabck callback, QObject *receiver, ReceiverIsValid receiverIsValid) {}

void PLSNaverShoppingLIVEAPI::getScheduleList(PLSPlatformNaverShoppingLIVE *platform, GetScheduleListCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid) {}

void PLSNaverShoppingLIVEAPI::getScheduleList(PLSPlatformNaverShoppingLIVE *platform, int beforeSeconds, int afterSeconds, GetScheduleListCallback callback, QObject *receiver,
					      ReceiverIsValid receiverIsValid)
{
}

void PLSNaverShoppingLIVEAPI::getCategoryList(PLSPlatformNaverShoppingLIVE *platform, GetCategoryListCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid) {}

void PLSNaverShoppingLIVEAPI::sendPushNotification(PLSPlatformNaverShoppingLIVE *platform, QObject *receiver, RequestOkCallback ok, RequestFailedCallback fail, ReceiverIsValid receiverIsValid) {}

void PLSNaverShoppingLIVEAPI::productSearchByUrl(PLSPlatformNaverShoppingLIVE *platform, const QString &url, ProductSearchByUrlCallback &&callback, QObject *receiver, ReceiverIsValid receiverIsValid)
{
}

void PLSNaverShoppingLIVEAPI::productSearchByTag(PLSPlatformNaverShoppingLIVE *platform, const QString &tag, int page, int pageSize, ProductSearchByTagCallback &&callback, QObject *receiver,
						 ReceiverIsValid receiverIsValid)
{
}

void PLSNaverShoppingLIVEAPI::productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, const QList<qint64> &fixedProductNos, const QList<qint64> &unfixedProductNos,
							ProductSearchByProductNosSplitCallback &&callback, QObject *receiver, ReceiverIsValid receiverIsValid)
{
}

void PLSNaverShoppingLIVEAPI::productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, const QList<qint64> &productNos, ProductSearchByProductNosAllCallback &&callback, QObject *receiver,
							ReceiverIsValid receiverIsValid)
{
}

void PLSNaverShoppingLIVEAPI::productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, const QStringList &strProductNos, ProductSearchByProductNosAllCallback &&callback, QObject *receiver,
							ReceiverIsValid receiverIsValid)
{
}

QString PLSNaverShoppingLIVEAPI::urlForPath(const QString &path)
{
	return QString();
}

void PLSNaverShoppingLIVEAPI::downloadImage(PLSPlatformNaverShoppingLIVE *, const QString &url, std::function<void(bool ok, const QString &imagePath)> callback, QObject *receiver,
					    ReceiverIsValid receiverIsValid, int timeout)
{
}

void PLSNaverShoppingLIVEAPI::downloadImages(PLSPlatformNaverShoppingLIVE *, const QStringList &urls, std::function<void(const QList<QPair<bool, QString>> &imagePath)> callback, QObject *receiver,
					     ReceiverIsValid receiverIsValid, int timeout)
{
}

bool PLSNaverShoppingLIVEAPI::json_hasKey(const QJsonObject &object, const QString &key)
{
	auto iter = object.find(key);
	if (iter == object.end()) {
		return false;
	}

	auto value = iter.value();
	if (value.isNull() || value.isUndefined()) {
		return false;
	}
	return true;
}

bool PLSNaverShoppingLIVEAPI::json_hasPriceOrRateKey(const QJsonObject &object, const QString &key)
{
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

void PLSNaverShoppingLIVEAPI::processRequestOkCallback(PLSPlatformNaverShoppingLIVE *platform, QByteArray data, const char *log, QObject *receiver, ReceiverIsValid receiverIsValid,
						       RequestOkCallback ok, RequestFailedCallback fail, int statusCode, bool isIgnoreEmptyJson)
{
	fail(PLSAPINaverShoppingType::PLSNaverShoppingFailed);
}

void PLSNaverShoppingLIVEAPI::processRequestFailCallback(PLSPlatformNaverShoppingLIVE *platform, int statusCode, QByteArray data, const char *log, QNetworkReply::NetworkError networkError,
							 QObject *receiver, ReceiverIsValid receiverIsValid, RequestFailedCallback fail, const ApiPropertyMap &apiPropertyMap)
{
	if (fail) {
		fail(PLSAPINaverShoppingType::PLSNaverShoppingFailed);
	}
}

bool PLSNaverShoppingLIVEAPI::isShowApiAlert(PLSAPINaverShoppingType apiType, const ApiPropertyMap &apiPropertyMap)
{
	return true;
}

void PLSNaverShoppingLIVEAPI::getNaverShoppingDateFormat(uint timeStamp, QDate &date, QString &yearMonthDay, QString &hour, QString &minute, QString &ap) {}

void PLSNaverShoppingLIVEAPI::getNaverShoppingDateFormat(uint timeStamp, QString &yearMonthDay) {}

uint PLSNaverShoppingLIVEAPI::getNaverShoppingLocalTimeStamp(const QDate &date, const QString &hourString, const QString &minuteString, int ap)
{
	int hour = (ap == 1) ? (hourString.toInt() + 12) : hourString.toInt();
	QTime time(hour, minuteString.toInt());
	QDateTime dateTime(date, time);
	return dateTime.toTime_t();
}

void PLSNaverShoppingLIVEAPI::getJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const char *log, RequestOkCallback ok, RequestFailedCallback fail, QObject *receiver,
				      ReceiverIsValid receiverIsValid, const QVariantMap &headers, const QVariantMap &params, bool useAccessToken, const ApiPropertyMap &apiPropertyMap,
				      bool isIgnoreEmptyJson)
{
}

void PLSNaverShoppingLIVEAPI::postJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const QJsonObject &json, const char *log, std::function<void(const QJsonDocument &)> ok,
				       std::function<void(PLSAPINaverShoppingType apiType)> fail, QObject *receiver, ReceiverIsValid receiverIsValid, const QVariantMap &headers,
				       const ApiPropertyMap &apiPropertyMap)
{
}

void PLSNaverShoppingLIVEAPI::putJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const QJsonObject &json, const char *log, std::function<void(const QJsonDocument &)> ok,
				      std::function<void(PLSAPINaverShoppingType apiType)> fail, QObject *receiver, ReceiverIsValid receiverIsValid, const QVariantMap &headers,
				      const ApiPropertyMap &apiPropertyMap)
{
}

void PLSNaverShoppingLIVEAPI::deleteJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const char *log, std::function<void(const QJsonDocument &)> ok,
					 std::function<void(PLSAPINaverShoppingType apiType)> fail, QObject *receiver, ReceiverIsValid receiverIsValid, const ApiPropertyMap &apiPropertyMap)
{
}

void PLSNaverShoppingLIVEAPI::customJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const char *log, std::function<void(const QJsonDocument &)> ok,
					 std::function<void(PLSAPINaverShoppingType apiType)> fail, QObject *receiver, ReceiverIsValid receiverIsValid, const ApiPropertyMap &apiPropertyMap)
{
}

PLSNaverShoppingLIVEAPI::ScheduleInfo::ScheduleInfo() {}

PLSNaverShoppingLIVEAPI::ScheduleInfo::ScheduleInfo(const QJsonObject &object)
	: title(JSON_getString(object, title)),
	  serviceName(JSON_getString(object, serviceName)),
	  id(JSON_getString(object, id)),
	  standByImage(JSON_getString(object, standByImage)),
	  broadcasterId(JSON_getString(object, broadcasterId)),
	  serviceId(JSON_getString(object, serviceId)),
	  releaseLevel(JSON_getString(object, releaseLevel)),
	  expectedStartDate(JSON_getString(object, expectedStartDate)),
	  broadcastEndUrl(JSON_getString(object, broadcastEndUrl)),
	  description(JSON_getString(object, description))
{
}

bool PLSNaverShoppingLIVEAPI::ScheduleInfo::checkStartTime(int beforeSeconds, int afterSeconds) const
{
	return false;
}

PLSNaverShoppingLIVEAPI::LiveCategory::LiveCategory() {}

PLSNaverShoppingLIVEAPI::LiveCategory::LiveCategory(const QJsonObject &object)
	: id(JSON_getString(object, id)), displayName(JSON_getString(object, displayName)), parentId(JSON_getString(object, parentId))
{
}
