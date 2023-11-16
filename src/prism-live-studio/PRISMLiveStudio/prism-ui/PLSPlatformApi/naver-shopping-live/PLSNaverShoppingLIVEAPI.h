#pragma once

#include <functional>

#include <QObject>
#include <QUrl>
#include <QVariant>
#include <QJsonObject>
#include <QJsonValue>

#include "pls-channel-const.h"
#include "pls-net-url.hpp"
#include <qnetworkreply.h>

class PLSPlatformNaverShoppingLIVE;

enum class PrepareInfoType { NonePrepareInfoType, GoLivePrepareInfo, RehearsalPrepareInfo };

//#define ApiPropertyMap QMap<PLSAPINaverShoppingType, QVariantMap>

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

const int SCHEDULE_FIRST_PAGE_NUM = 1;
const int SCHEDULE_PER_PAGE_MAX_NUM = 20;

extern const QString CHANNEL_CONNECTION_AGREE;
extern const QString CHANNEL_CONNECTION_NOAGREE;
extern const QString CHANNEL_CONNECTION_NONE;

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
	PLSNaverShoppingCannotAddOtherShopProduct,
	PLSNaverShoppingAttachProductsToOwnMall,
	PLSNaverShoppingAll,
};

using ApiPropertyMap = QMap<PLSAPINaverShoppingType, QVariantMap>;

struct PLSNaverShoppingLIVEAPI {
	struct ProductInfo {
		qint64 productNo = 0;
		bool represent = false;
		bool hasDiscountRate = false;
		bool hasSpecialPrice = false;
		bool isMinorPurchasable = true;
		QString accountNo;
		QString channelNo;
		QString key;
		QString name;
		QString imageUrl;
		QString mallName;
		QString productStatus;
		QString linkUrl;
		double price = 0.0;
		double discountRate = 0.0;
		double specialPrice = 0.0;
		QString wholeCategoryId;
		QString wholeCategoryName;

		ProductInfo() = default;
		explicit ProductInfo(const QJsonObject &object);
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
		QString storeAccountNo;

		NaverShoppingUserInfo() = default;
		explicit NaverShoppingUserInfo(const QJsonObject &object);
	};

	struct LiveCategory {
		QString id;
		QString displayName;
		QString parentId;
		QList<LiveCategory> children;

		LiveCategory() = default;
		explicit LiveCategory(const QJsonObject &object);
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
		QString externalExposeAgreementStatus = CHANNEL_CONNECTION_NONE;
		bool searchable;
		NaverShoppingLivingInfo() = default;
		explicit NaverShoppingLivingInfo(const QJsonObject &object);
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
		bool isPlanLiving = false;
		bool allowSearch = true;
		QDate ymdDate;
		QString yearMonthDay;
		QString hour;
		QString minute;
		int ap = -1;
		QString description;
		QString externalExposeAgreementStatus = CHANNEL_CONNECTION_NONE;
		PrepareInfoType infoType = PrepareInfoType::NonePrepareInfoType;
	};

	struct GetSelectiveAccountStore {
		QString channelNo;
		QString channelName;
		QString windowIconImageUrl;
		QString representImageUrl;

		GetSelectiveAccountStore() = default;
		explicit GetSelectiveAccountStore(const QJsonObject &object);
	};

	struct ScheduleInfo {
		QString title;
		QString description;
		QString serviceName;
		QString id;
		QString standByImage;
		QString broadcasterId;
		QString broadcastType;
		QString serviceId;
		QString releaseLevel;
		QString expectedStartDate;
		uint timeStamp = 0;
		QString startTimeUTC;
		QString broadcastEndUrl;
		QList<ProductInfo> shoppingProducts;
		QString standByImagePath;
		LiveCategory displayCategory;
		QString externalExposeAgreementStatus = CHANNEL_CONNECTION_NONE;
		bool searchable{false};
		bool highQualityAvailable = false;
		ScheduleInfo() = default;
		explicit ScheduleInfo(const QJsonObject &object);

		bool checkStartTime(int beforeSeconds, int afterSeconds) const;
	};

	struct Url {
		QUrl url;
		QString maskingUrl;

		explicit Url(const QString &url_) : url(url_) {}
		Url(const QString &url_, const QString &maskingUrl_) : url(url_), maskingUrl(maskingUrl_) {}
		explicit Url(const QUrl &url_) : url(url_) {}
		Url(const QUrl &url_, const QString &maskingUrl_) : url(url_), maskingUrl(maskingUrl_) {}
		~Url() = default;
	};

	using ReceiverIsValid = std::function<bool(const QObject *)>;
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
	using GetScheduleListCallback = std::function<void(PLSAPINaverShoppingType apiType, const QList<ScheduleInfo> &scheduleList, int currentPage, int totalCount)>;
	using UpdateScheduleCallback = std::function<void(PLSAPINaverShoppingType apiType, const ScheduleInfo &scheduleInfo)>;
	using RequestOkCallback = std::function<void(const QJsonDocument &)>;
	using RequestFailedCallback = std::function<void(PLSAPINaverShoppingType apiType)>;

	static void storeChannelProductSearch(PLSPlatformNaverShoppingLIVE *platform, const QString &channelNo, const QString &productName, int page, int pageSize,
					      const StoreChannelProductSearchCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid = nullptr);
	static void productSearchByUrl(PLSPlatformNaverShoppingLIVE *platform, const QString &url, const ProductSearchByUrlCallback &callback, const QObject *receiver,
				       const ReceiverIsValid &receiverIsValid = nullptr);
	static void productSearchByTag(PLSPlatformNaverShoppingLIVE *platform, const QString &tag, int page, int pageSize, const ProductSearchByTagCallback &callback, const QObject *receiver,
				       const ReceiverIsValid &receiverIsValid = nullptr);
	static void productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, const QList<qint64> &fixedProductNos, const QList<qint64> &unfixedProductNos,
					      const ProductSearchByProductNosSplitCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid = nullptr);
	static void productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, const QList<qint64> &productNos, const ProductSearchByProductNosAllCallback &callback, const QObject *receiver,
					      const ReceiverIsValid &receiverIsValid = nullptr);
	static void productSearchByProductNos(PLSPlatformNaverShoppingLIVE *platform, const QStringList &strProductNos, const ProductSearchByProductNosAllCallback &callback, const QObject *receiver,
					      const ReceiverIsValid &receiverIsValid = nullptr);
	static QString urlForPath(const QString &path);
	static void refreshChannelToken(const PLSPlatformNaverShoppingLIVE *platform, const GetUserInfoCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid = nullptr);
	static PLSAPINaverShoppingType getLoginFailType(const QByteArray &data);
	static void uploadImage(PLSPlatformNaverShoppingLIVE *platform, const QString &imagePath, const UploadImageCallback &callback, const QObject *receiver,
				const ReceiverIsValid &receiverIsValid = nullptr);
	static void uploadLocalImage(const PLSPlatformNaverShoppingLIVE *platform, const QString &uploadUrl, const QString &deliveryUrl, const QString &imageFilePath,
				     const UploadImageCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid = nullptr);
	static QString getUploadImageDomain(const QByteArray &data);
	static void createNowLiving(PLSPlatformNaverShoppingLIVE *platform, const QJsonObject &body, const CreateNowLivingCallback &callback, const QObject *receiver,
				    const ReceiverIsValid &receiverIsValid = nullptr);
	static void createScheduleLiving(PLSPlatformNaverShoppingLIVE *platform, const QString &scheduleId, const CreateNowLivingCallback &callback, const QObject *receiver,
					 const ReceiverIsValid &receiverIsValid = nullptr);
	static void getLivingInfo(PLSPlatformNaverShoppingLIVE *platform, bool livePolling, const GetLivingInfoCallback &callback, const QObject *receiver,
				  const ReceiverIsValid &receiverIsValid = nullptr);
	static void updateNowLiving(PLSPlatformNaverShoppingLIVE *platform, const QString &id, const QJsonObject &body, const std::function<void(PLSAPINaverShoppingType apiType)> &callback,
				    const QObject *receiver, const ReceiverIsValid &receiverIsValid = nullptr);
	static void updateScheduleInfo(PLSPlatformNaverShoppingLIVE *platform, const QString &id, const QJsonObject &body, const UpdateScheduleCallback &callback, const QObject *receiver,
				       const ReceiverIsValid &receiverIsValid = nullptr);
	static void stopLiving(const PLSPlatformNaverShoppingLIVE *platform, bool needVideoSave, const std::function<void(bool ok)> &callback, const QObject *receiver,
			       const ReceiverIsValid &receiverIsValid = nullptr);
	static void logoutNaverShopping(const PLSPlatformNaverShoppingLIVE *platform, const QString &accessToken, const QObject *receiver);
	static void getSelectiveAccountStores(PLSPlatformNaverShoppingLIVE *platform, const GetSelectiveAccountStoresCallabck &callback, const QObject *receiver,
					      const ReceiverIsValid &receiverIsValid = nullptr);
	static void getScheduleList(PLSPlatformNaverShoppingLIVE *platform, int currentPage, bool isNotice, const GetScheduleListCallback &callback, const QObject *receiver,
				    const ReceiverIsValid &receiverIsValid = nullptr);
	static void getCategoryList(PLSPlatformNaverShoppingLIVE *platform, const GetCategoryListCallback &callback, const QObject *receiver, const ReceiverIsValid &receiverIsValid = nullptr);

	static void sendPushNotification(PLSPlatformNaverShoppingLIVE *platform, const QObject *receiver, const RequestOkCallback &ok, const RequestFailedCallback &fail,
					 const ReceiverIsValid &receiverIsValid = nullptr);
	static void sendNotice(PLSPlatformNaverShoppingLIVE *platform, const QJsonObject &body, const QObject *receiver, const RequestOkCallback &ok, const RequestFailedCallback &fail,
			       const ReceiverIsValid &receiverIsValid = nullptr);
	static void downloadImage(const PLSPlatformNaverShoppingLIVE *platform, const QString &url, const std::function<void(bool ok, const QString &imagePath)> &callback,
				  const QObject *receiver = nullptr, const ReceiverIsValid &receiverIsValid = nullptr, int timeout = PRISM_NET_REQUEST_TIMEOUT);
	static void downloadImages(const PLSPlatformNaverShoppingLIVE *platform, const QStringList &urls, const std::function<void(const QList<QPair<bool, QString>> &imagePath)> &callback,
				   const QObject *receiver = nullptr, const ReceiverIsValid &receiverIsValid = nullptr, int timeout = PRISM_NET_REQUEST_TIMEOUT);

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
	static void processRequestOkCallback(const PLSPlatformNaverShoppingLIVE *platform, const QByteArray &data, const char *log, const QObject *receiver, const ReceiverIsValid &receiverIsValid,
					     const RequestOkCallback &ok, const RequestFailedCallback &fail, int statusCode = 200, bool isIgnoreEmptyJson = false);
	static void processRequestFailCallback(PLSPlatformNaverShoppingLIVE *platform, int statusCode, const QByteArray &data, const char *log, QNetworkReply::NetworkError networkError,
					       const QObject *receiver, const ReceiverIsValid &receiverIsValid, const RequestFailedCallback &fail,
					       const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	static bool isShowApiAlert(PLSAPINaverShoppingType apiType, const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	static bool isHandleTokenExpired(PLSAPINaverShoppingType apiType, const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	static void getNaverShoppingDateFormat(uint timeStamp, QDate &date, QString &yearMonthDay, QString &hour, QString &minute, QString &ap);
	static void getNaverShoppingDateFormat(qint64 timeStamp, QString &yearMonthDay);
	static qint64 getLocalTimeStamp(const QDate &date, const QString &hourString, const QString &minuteString, int AmOrPm);
	static bool isRhythmicityProduct(const QString &matchCategoryId, const QString &matchCategoryName);
	static void getStoreLoginUrl(const QWidget *widget, const std::function<void(const QString &storeLoginUrl)> &ok, const std::function<void()> &fail);

private:
	static void getJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const char *log, const RequestOkCallback &ok, const RequestFailedCallback &fail, const QObject *receiver,
			    const ReceiverIsValid &receiverIsValid, const QVariantMap &headers, const QVariantMap &params, bool useAccessToken = true,
			    const ApiPropertyMap &apiPropertyMap = ApiPropertyMap(), bool isIgnoreEmptyJson = false, bool workInMainThread = false);
	static void postJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const QJsonObject &json, const char *log, const std::function<void(const QJsonDocument &)> &ok,
			     const std::function<void(PLSAPINaverShoppingType apiType)> &fail, const QObject *receiver = nullptr, const ReceiverIsValid &receiverIsValid = nullptr,
			     const QVariantMap &headers = QVariantMap(), const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	static void putJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const QJsonObject &json, const char *log, const std::function<void(const QJsonDocument &)> &ok,
			    const std::function<void(PLSAPINaverShoppingType apiType)> &fail, const QObject *receiver = nullptr, const ReceiverIsValid &receiverIsValid = nullptr,
			    const QVariantMap &headers = QVariantMap(), const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	static void deleteJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const char *log, const std::function<void(const QJsonDocument &)> &ok,
			       const std::function<void(PLSAPINaverShoppingType apiType)> &fail, const QObject *receiver = nullptr, const ReceiverIsValid &receiverIsValid = nullptr,
			       const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	static void customJson(PLSPlatformNaverShoppingLIVE *platform, const Url &url, const char *log, const std::function<void(const QJsonDocument &)> &ok,
			       const std::function<void(PLSAPINaverShoppingType apiType)> &fail, const QObject *receiver = nullptr, const ReceiverIsValid &receiverIsValid = nullptr,
			       const ApiPropertyMap &apiPropertyMap = ApiPropertyMap(), bool isIgnoreEmptyJson = false);
};
