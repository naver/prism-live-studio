#pragma once

#include <functional>

#include <QObject>
#include <QUrl>
#include <QVariant>
#include <QJsonObject>
#include <QJsonValue>

#include "../ChannelsDataApi/ChannelConst.h"
#include "pls-net-url.hpp"
#include <qnetworkreply.h>

class PLSPlatformNaverShoppingLIVE;

#define ApiPropertyMap QMap<PLSAPINaverShoppingType, QVariantMap>

#define JSON_getBool(object, key) PLSNaverShoppingLIVEAPI::json_getBool(object, #key)
#define JSON_getBoolEx(object, key, default) PLSNaverShoppingLIVEAPI::json_getBool(object, #key, default)
#define JSON_getInt(object, key) PLSNaverShoppingLIVEAPI::json_getInt(object, #key)
#define JSON_getIntEx(object, key, default) PLSNaverShoppingLIVEAPI::json_getInt(object, #key, default)
#define JSON_getInt64(object, key) PLSNaverShoppingLIVEAPI::json_getInt64(object, #key)
#define JSON_getInt64Ex(object, key, default) PLSNaverShoppingLIVEAPI::json_getInt64(object, #key, default)
#define JSON_getDouble(object, key) PLSNaverShoppingLIVEAPI::json_getDouble(object, #key)
#define JSON_getDoubleEx(object, key, default) PLSNaverShoppingLIVEAPI::json_getDouble(object, #key, default)
#define JSON_getString(object, key) PLSNaverShoppingLIVEAPI::json_getString(object, #key)
#define JSON_getStringEx(object, key, default) PLSNaverShoppingLIVEAPI::json_getString(object, #key, default)

#define JSON_toBool(value) PLSNaverShoppingLIVEAPI::json_toBool(value)
#define JSON_toBoolEx(value, default) PLSNaverShoppingLIVEAPI::json_toBool(value, default)
#define JSON_toInt(value) PLSNaverShoppingLIVEAPI::json_toInt(value)
#define JSON_toIntEx(value, default) PLSNaverShoppingLIVEAPI::json_toInt(value, default)
#define JSON_toInt64(value) PLSNaverShoppingLIVEAPI::json_toInt64(value)
#define JSON_toInt64Ex(value, default) PLSNaverShoppingLIVEAPI::json_toInt64(value, default)
#define JSON_toDouble(value) PLSNaverShoppingLIVEAPI::json_toDouble(value)
#define JSON_toDoubleEx(value, default) PLSNaverShoppingLIVEAPI::json_toDouble(value, default)
#define JSON_toString(value) PLSNaverShoppingLIVEAPI::json_toString(value)
#define JSON_toStringEx(value, default) PLSNaverShoppingLIVEAPI::json_toString(value, default)

#define JSON_hasKey(object, key) PLSNaverShoppingLIVEAPI::json_hasKey(object, #key)
#define JSON_hasPriceOrRateKey(object, key) PLSNaverShoppingLIVEAPI::json_hasPriceOrRateKey(object, #key)

#define JSON_mkObject(key, value) \
	{                         \
#key, value       \
	}

enum class PLSAPINaverShoppingType {
	PLSNaverShoppingSuccess,
	PLSNaverShoppingFailed,
	PLSNaverShoppingCreateLivingFailed,
	PLSNaverShoppingScheduleListFailed,
	PLSNaverShoppingCategoryListFailed,
	PLSNaverShoppingUpdateFailed,
	PLSNaverShoppingUpdateScheduleInfoFailed,
	PLSNaverShoppingInvalidAccessToken,
	PLSNaverShoppingNoLiveRight,
	PLSNaverShoppingLoginFailed,
	PLSNaverShoppingNetworkError,
	PLSNaverShoppingScheduleTimeNotReached,
	PLSNaverShoppingScheduleIsLived,
	PLSNaverShoppingScheduleDelete,
	PLSNaverShoppingAgeRestrictedProduct,
	PLSNaverShoppingUploadImageFailed,
	PLSNaverShoppingNotFound204,
	PLSNaverShoppingCreateSchduleExternalStream,
	PLSNaverShoppingAll,
};

struct PLSNShoppingLIVEEndTime {
	std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
	bool isMinShown_10{false};
	bool isMinShown_5{false};
	bool isMinShown_1{false};
	bool isMinShown_0{false};
};

struct PLSNaverShoppingLIVEAPI {
	struct ProductInfo {
		qint64 productNo = 0;
		bool represent = false;
		bool hasDiscountRate = false;
		bool hasSpecialPrice = false;
		bool isMinorPurchasable = true;
		QString key;
		QString name;
		QString imageUrl;
		QString mallName;
		QString productStatus;
		QString linkUrl;
		double price = 0.0;
		double discountRate = 0.0;
		double specialPrice = 0.0;

		ProductInfo();
		ProductInfo(const QJsonObject &object);
		ProductInfo(qint64 productNo, const QJsonObject &object);

		bool discountRateIsValid() const;
		bool specialPriceIsValid() const;
	};

	struct NaverShoppingUserInfo {
		QString broadcastOwnerId;
		QString serviceId;
		QString broadcasterId;
		QString profileImageUrl;
		QString profileImagePath;
		QString nickname;
		QString accessToken;

		NaverShoppingUserInfo();
		NaverShoppingUserInfo(const QJsonObject &object);
	};

	struct LiveCategory {
		QString id;
		QString displayName;
		QString parentId;
		QList<LiveCategory> children;

		LiveCategory();
		LiveCategory(const QJsonObject &object);
	};

	struct NaverShoppingLivingInfo {
		QString title;
		QString status;
		QString releaseLevel;
		QString id;
		QString streamSeq;
		QString publishUrl;
		QString standByImage;
		QString broadcastType;
		QList<ProductInfo> shoppingProducts;
		QString broadcastEndUrl;
		QString displayType;
		LiveCategory displayCategory;
		QString description;

		NaverShoppingLivingInfo();
		NaverShoppingLivingInfo(const QJsonObject &object);
	};

	struct NaverShoppingPrepareLiveInfo {
		QString title;
		QString standByImagePath;
		QString standByImageURL;
		QList<ProductInfo> shoppingProducts;
		bool isNowLiving = true;
		QString scheduleId;
		QString broadcastEndUrl;
		QString releaseLevel;
		QDate ymdDate;
		QString yearMonthDay;
		QString hour;
		QString minute;
		int ap = -1;
		QString description;
		QString firstCategoryName;
		QString secondCategoryName;
	};

	struct GetSelectiveAccountStore {
		QString channelNo;
		QString channelName;
		QString windowIconImageUrl;
		QString representImageUrl;

		GetSelectiveAccountStore();
		GetSelectiveAccountStore(const QJsonObject &object);
	};

	struct ScheduleInfo {
		QString title;
		QString description;
		QString serviceName;
		QString id;
		QString standByImage;
		QString broadcasterId;
		QString serviceId;
		QString releaseLevel;
		QString expectedStartDate;
		uint timeStamp = 0;
		QString startTimeUTC;
		QString broadcastEndUrl;
		QList<ProductInfo> shoppingProducts;
		QString standByImagePath;
		LiveCategory displayCategory;

		ScheduleInfo();
		ScheduleInfo(const QJsonObject &object);

		bool checkStartTime(int beforeSeconds, int afterSeconds) const;
	};

	struct Url {
		QUrl url;
		QString maskingUrl;

		Url(const QString &url_) : url(url_), maskingUrl() {}
		Url(const QString &url_, const QString &maskingUrl_) : url(url_), maskingUrl(maskingUrl_) {}
		Url(const QUrl &url_) : url(url_), maskingUrl() {}
		Url(const QUrl &url_, const QString &maskingUrl_) : url(url_), maskingUrl(maskingUrl_) {}
		~Url() {}
	};

	using ReceiverIsValid = std::function<bool(QObject *)>;
	using StoreChannelProductSearchCallback = std::function<void(bool ok, const QList<ProductInfo> &products, bool next, int page)>;
	using ProductSearchByUrlCallback = std::function<void(bool ok, bool hasProduct, const ProductInfo &product)>;
	using ProductSearchByTagCallback = std::function<void(bool ok, const QList<ProductInfo> &products, bool next, int page)>;
	using ProductSearchByProductNoCallback = std::function<void(bool ok, const ProductInfo &product)>;
	using ProductSearchByProductNosSplitCallback = std::function<void(bool ok, const QList<ProductInfo> &fixedProducts, const QList<ProductInfo> &unfixedProducts)>;
	using ProductSearchByProductNosAllCallback = std::function<void(bool ok, const QList<ProductInfo> &products)>;
	using RefreshStoreInfoCallabck = std::function<void(bool ok, const QString &storeId, const QString &storeName, const QString &accessToken)>;
	using GetSelectiveAccountStoresCallabck = std::function<void(bool ok, const QList<GetSelectiveAccountStore> &stores)>;
	using DownloadImageCallback = std::function<void(bool ok, const QString &imagePath)>;
	using GetUserInfoCallback = std::function<void(PLSAPINaverShoppingType apiType, const NaverShoppingUserInfo &userInfo)>;
	using UploadImageCallback = std::function<void(PLSAPINaverShoppingType apiType, const QString &imageURL)>;
	using CreateNowLivingCallback = std::function<void(PLSAPINaverShoppingType apiType, const NaverShoppingLivingInfo &livingInfo)>;
	using GetLivingInfoCallback = std::function<void(PLSAPINaverShoppingType apiType, const NaverShoppingLivingInfo &livingInfo)>;
	using GetCategoryListCallback = std::function<void(PLSAPINaverShoppingType apiType, const QList<LiveCategory> &categoryList)>;
	using GetScheduleListCallback = std::function<void(PLSAPINaverShoppingType apiType, const QList<ScheduleInfo> &scheduleList)>;
	using RequestOkCallback = std::function<void(const QJsonDocument &)>;
	using RequestFailedCallback = std::function<void(PLSAPINaverShoppingType apiType)>;

	static void storeChannelProductSearch(PLSPlatformNaverShoppingLIVE *platform, const QString &channelNo, const QString &productName, int page, int pageSize,
					      StoreChannelProductSearchCallback &&callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static void productSearchByUrl(PLSPlatformNaverShoppingLIVE *platform, const QString &url, ProductSearchByUrlCallback &&callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static void productSearchByTag(PLSPlatformNaverShoppingLIVE *platform, const QString &tag, int page, int pageSize, ProductSearchByTagCallback &&callback, QObject *receiver,
				       ReceiverIsValid receiverIsValid = nullptr);
	static void productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, const QList<qint64> &fixedProductNos, const QList<qint64> &unfixedProductNos,
					      ProductSearchByProductNosSplitCallback &&callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static void productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, const QList<qint64> &productNos, ProductSearchByProductNosAllCallback &&callback, QObject *receiver,
					      ReceiverIsValid receiverIsValid = nullptr);
	static void productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, const QStringList &strProductNos, ProductSearchByProductNosAllCallback &&callback, QObject *receiver,
					      ReceiverIsValid receiverIsValid = nullptr);
	static QString urlForPath(const QString &path);
	static void refreshChannelToken(PLSPlatformNaverShoppingLIVE *platform, GetUserInfoCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static PLSAPINaverShoppingType getLoginFailType(const QByteArray &data);
	static void uploadImage(PLSPlatformNaverShoppingLIVE *platform, const QString &imagePath, UploadImageCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static void uploadLocalImage(PLSPlatformNaverShoppingLIVE *platform, const QString &uploadUrl, const QString &deliveryUrl, const QString &imageFilePath, UploadImageCallback callback,
				     QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static void createNowLiving(PLSPlatformNaverShoppingLIVE *platform, const QJsonObject &body, CreateNowLivingCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static void createScheduleLiving(PLSPlatformNaverShoppingLIVE *platform, const QString &scheduleId, CreateNowLivingCallback callback, QObject *receiver,
					 ReceiverIsValid receiverIsValid = nullptr);
	static void getLivingInfo(PLSPlatformNaverShoppingLIVE *platform, bool livePolling, GetLivingInfoCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static void updateNowLiving(PLSPlatformNaverShoppingLIVE *platform, const QString &id, const QJsonObject &body, std::function<void(PLSAPINaverShoppingType apiType)> callback,
				    QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static void stopLiving(PLSPlatformNaverShoppingLIVE *platform, bool needVideoSave, std::function<void(bool ok)> callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static void logoutNaverShopping(PLSPlatformNaverShoppingLIVE *platform, const QString &accessToken, QObject *receiver);
	static void getSelectiveAccountStores(PLSPlatformNaverShoppingLIVE *platform, GetSelectiveAccountStoresCallabck callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static void getScheduleList(PLSPlatformNaverShoppingLIVE *platform, GetScheduleListCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static void getScheduleList(PLSPlatformNaverShoppingLIVE *platform, int beforeSeconds, int afterSeconds, GetScheduleListCallback callback, QObject *receiver,
				    ReceiverIsValid receiverIsValid = nullptr);
	static void getCategoryList(PLSPlatformNaverShoppingLIVE *platform, GetCategoryListCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr);
	static void sendPushNotification(PLSPlatformNaverShoppingLIVE *platform, QObject *receiver, RequestOkCallback ok, RequestFailedCallback fail, ReceiverIsValid receiverIsValid = nullptr);

	static void downloadImage(PLSPlatformNaverShoppingLIVE *platform, const QString &url, std::function<void(bool ok, const QString &imagePath)> callback, QObject *receiver,
				  ReceiverIsValid receiverIsValid = nullptr, int timeout = PRISM_NET_REQUEST_TIMEOUT);
	static void downloadImages(PLSPlatformNaverShoppingLIVE *platform, const QStringList &urls, std::function<void(const QList<QPair<bool, QString>> &imagePath)> callback, QObject *receiver,
				   ReceiverIsValid receiverIsValid = nullptr, int timeout = PRISM_NET_REQUEST_TIMEOUT);

	static bool json_hasKey(const QJsonObject &object, const QString &key);
	static bool json_hasPriceOrRateKey(const QJsonObject &object, const QString &key);

	static bool json_getBool(const QJsonObject &object, const QString &key, bool defaultValue = false);
	static int json_getInt(const QJsonObject &object, const QString &key, int defaultValue = 0);
	static qint64 json_getInt64(const QJsonObject &object, const QString &key, qint64 defaultValue = 0);
	static double json_getDouble(const QJsonObject &object, const QString &key, double defaultValue = 0.0);
	static QString json_getString(const QJsonObject &object, const QString &key, const QString &defaultValue = QString());

	static bool json_toBool(const QJsonValue &value, bool defaultValue = false);
	static int json_toInt(const QJsonValue &value, int defaultValue = 0);
	static qint64 json_toInt64(const QJsonValue &value, qint64 defaultValue = 0);
	static double json_toDouble(const QJsonValue &value, double defaultValue = 0.0);
	static QString json_toString(const QJsonValue &value, const QString &defaultValue = QString());
	static void processRequestOkCallback(PLSPlatformNaverShoppingLIVE *platform, QByteArray data, const char *log, QObject *receiver, ReceiverIsValid receiverIsValid, RequestOkCallback ok,
					     RequestFailedCallback fail, int statusCode = 200, bool isIgnoreEmptyJson = false);
	static void processRequestFailCallback(PLSPlatformNaverShoppingLIVE *platform, int statusCode, QByteArray data, const char *log, QNetworkReply::NetworkError networkError, QObject *receiver,
					       ReceiverIsValid receiverIsValid, RequestFailedCallback fail, const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	static bool isShowApiAlert(PLSAPINaverShoppingType apiType, const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	static void getNaverShoppingDateFormat(uint timeStamp, QDate &date, QString &yearMonthDay, QString &hour, QString &minute, QString &ap);
	static void getNaverShoppingDateFormat(uint timeStamp, QString &yearMonthDay);
	static uint getNaverShoppingLocalTimeStamp(const QDate &date, const QString &hourString, const QString &minuteString, int ap);

private:
	static void getJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const char *log, RequestOkCallback ok, RequestFailedCallback fail, QObject *receiver,
			    ReceiverIsValid receiverIsValid, const QVariantMap &headers, const QVariantMap &params, bool useAccessToken = true, const ApiPropertyMap &apiPropertyMap = ApiPropertyMap(),
			    bool isIgnoreEmptyJson = false);
	static void postJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const QJsonObject &json, const char *log, std::function<void(const QJsonDocument &)> ok,
			     std::function<void(PLSAPINaverShoppingType apiType)> fail, QObject *receiver = nullptr, ReceiverIsValid receiverIsValid = nullptr,
			     const QVariantMap &headers = QVariantMap(), const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	static void putJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const QJsonObject &json, const char *log, std::function<void(const QJsonDocument &)> ok,
			    std::function<void(PLSAPINaverShoppingType apiType)> fail, QObject *receiver = nullptr, ReceiverIsValid receiverIsValid = nullptr,
			    const QVariantMap &headers = QVariantMap(), const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	static void deleteJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const char *log, std::function<void(const QJsonDocument &)> ok,
			       std::function<void(PLSAPINaverShoppingType apiType)> fail, QObject *receiver = nullptr, ReceiverIsValid receiverIsValid = nullptr,
			       const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	static void customJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const char *log, std::function<void(const QJsonDocument &)> ok,
			       std::function<void(PLSAPINaverShoppingType apiType)> fail, QObject *receiver = nullptr, ReceiverIsValid receiverIsValid = nullptr,
			       const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
};
