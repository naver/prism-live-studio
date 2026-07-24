#include "PLSErrorHandler.h"
#include "libutils-api.h"
#include "liblog.h"
#include "frontend-api.h"
#include "PLSAlertView.h"
#include <QMetaEnum>
#include <QDesktopServices>
#include "PLSDialogButtonBox.h"
#include <QDialogButtonBox>
#include <type_traits>
#include <QStringList>
#include <QSet>
#include <QDir>
#include <QFileInfo>
#include "libutils-api.h"
#include "libui.h"

const QString PLSErrPhaseChannel = "channel";
const QString PLSErrPhaseLogin = "login";
const QString PLSErrPhaseDashBoard = "dashBoard";
const QString PLSErrPhaseAll = "all";

const QString PLSErrCustomKey_LoadLiveInfoFailed = "LoadLiveInfoFailed";
const QString PLSErrCustomKey_StartLiveFailed_Single = "StartLiveFailed_Single";
const QString PLSErrCustomKey_StartLiveFailed_Multi = "StartLiveFailed_Multi";
const QString PLSErrCustomKey_UpdateLiveInfoFailed = "UpdateLiveInfoFailed";
const QString PLSErrCustomKey_StartRehearsalFailed = "StartRehearsalFailed";
const QString PLSErrCustomKey_LoadLiveInfoExpired = "LoadLiveInfoExpired";
const QString PLSErrCustomKey_UpdateLiveInfoFailedNoService = "UpdateLiveInfoFailedNoService";
const QString PLSErrCustomKey_UploadImageFailed = "UploadImageFailed";
const QString PLSErrCustomKey_OutputRecordFailed = "OutputRecordFailed";
const QString PLSErrCustomKey_PRISMTempError = "PRISMTempError";

const QString PLSErrCustomKey_SOOPUpdateLiveInfoFailed = "SOOPUpdateLiveInfoFailed";
const QString PLSErrCustomKey_SOOPLoadLiveInfoFailed = "SOOPLoadLiveInfoFailed";

const QString PLSErrApiKey_OutputRecord = "OutputRecord";
const QString PLSErrApiKey_OutputStream = "OutputStream";

constexpr auto PLSErrKeyCommon = "Common";
constexpr auto PLSErrKeyDefault = "default";
const QString PLSErrKeyAllAlert = "Alert";

constexpr auto s_path_api_statusCode = "api.statusCode";
constexpr auto s_api_statusCode = "$statusCode";
constexpr auto s_api_errorCode = "errorCode";

constexpr auto s_path_err_title = "err.title"; //alert title
constexpr auto s_path_err_msgKey = "err.msgKey";
constexpr auto s_path_err_errorType = "err.errorType";
constexpr auto s_path_err_arg = "err.arg";
constexpr auto s_path_err_buttons = "err.buttons"; //json array string.
constexpr auto s_path_err_append = "err.append";
constexpr auto s_path_err_append_join = "err.appendJoin";
constexpr auto s_path_err_default_btn = "err.defaultBtn";
constexpr auto s_path_err_prismCode = "err.prismCode";
constexpr auto s_path_err_prismCodeName = "err.prismName";
constexpr auto s_path_err_alertType = "err.alertType"; //"error" or empty : contact us. "normal" or other : normal
constexpr auto s_path_err_stage = "err.stage";         // example: "goLiveFlow", "refreshChannelFlow"
constexpr auto s_path_extra_path = "extra.path";
constexpr auto s_path_extra_append = "extra.append";
constexpr auto s_path_macro_prefix = "default.extra.macroPrefix";
constexpr auto s_path_extra = "extra";
constexpr auto s_path_phase = "phase";
constexpr auto s_moudule_prefix = "Error handler";
constexpr auto s_default_error_type = PLSErrorHandler::COMMON_UNKNOWN_ERROR;
constexpr auto s_log_moudule = "ErrorHandler";

static QStringList s_specialPath({"Common", PLSErrKeyDefault});
const static QString s_valueRegStr("\\{\\{(.+?)\\}\\}");

static bool s_isPrintDebugLog = false;

static void printJson(const QMap<QString, QString> &map, const QString &log = {})
{
	if (s_isPrintDebugLog) {
		qDebug() << log << " " << map;
	}
}

template<typename U, typename T> static U getValue(const QJsonObject &obj, const T &keyPath, const U &defaultValue = U())
{
	if constexpr (std::is_same_v<T, QString>)
		return getValue(obj, keyPath.split("."), defaultValue);
	else if constexpr (std::is_same_v<T, QStringList>)
		return pls_get_attr<U>(obj, keyPath, defaultValue);
	else if constexpr (std::is_same_v<T, const char *> || std::is_array_v<T>)
		return getValue(obj, QString::fromUtf8(keyPath), defaultValue);
	else
		return pls_get_attr<U>(obj, keyPath, defaultValue);
}

static std::optional<QString> forceJsonValue2String(QJsonValue jsonValue)
{
	QString str;
	if (jsonValue.isString()) {
		str = jsonValue.toString();
	} else if (jsonValue.isDouble()) {
		str = QString::number(jsonValue.toDouble(), 'g', 20);
	} else if (jsonValue.isBool()) {
		str = jsonValue.toBool() == true ? "1" : "0";
	} else {
		return std::nullopt;
	}
	return str;
}

static QDialogButtonBox::StandardButton str2DialogBtn(const QString &str)
{
	if (str.isEmpty()) {
		return QDialogButtonBox::StandardButton::NoButton;
	}
	auto &&metaEnum = QMetaEnum::fromType<QDialogButtonBox::StandardButtons>();
	QDialogButtonBox::StandardButton wantedEnum = static_cast<QDialogButtonBox::StandardButton>(metaEnum.keyToValue(str.toUtf8().constData()));
	return wantedEnum;
}
static PLSErrorHandler::ErrorType str2ErrorType(const QString &str)
{
	if (str.isEmpty()) {
		return PLSErrorHandler::ErrorType::Other;
	}
	auto &&metaEnum = QMetaEnum::fromType<PLSErrorHandler::ErrorType>();
	PLSErrorHandler::ErrorType wantedEnum = static_cast<PLSErrorHandler::ErrorType>(metaEnum.keyToValue(str.toUtf8().constData()));
	return wantedEnum;
}
static PLSErrorHandler::AlertType str2AlertType(const QString &str, PLSErrorHandler::AlertType defaultType)
{
	if (str.isEmpty()) {
		return defaultType;
	}
	auto &&metaEnum = QMetaEnum::fromType<PLSErrorHandler::AlertType>();
	auto value = metaEnum.keyToValue(str.toUtf8().constData());
	if (value == -1) {
		return defaultType;
	}
	PLSErrorHandler::AlertType wantedEnum = static_cast<PLSErrorHandler::AlertType>(value);
	return wantedEnum;
}
static const char *dialogBtn2Str(QDialogButtonBox::StandardButton btn)
{
	auto &&metaEnum = QMetaEnum::fromType<QDialogButtonBox::StandardButtons>();
	return metaEnum.valueToKey(btn);
}

static QString &transTrString(QString &trString, bool trAll /*= false*/, bool isEnglish)
{
	QRegularExpression regExp("tr\\(\"(.*?)\"\\)");
	QRegularExpressionMatchIterator iterator = regExp.globalMatch(trString);
	while (iterator.hasNext()) {
		QRegularExpressionMatch match = iterator.next();
		QString innerContent = match.captured(1); //"ABC"
		QString fullContent = match.captured(0);  //"tr(\"ABC\")"
		QString trStr;
		if (!isEnglish) {
			trStr = QObject::tr(innerContent.toUtf8().constData());
		} else {
			trStr = pls_uistep_v2_get_english(innerContent.toUtf8());
		}
		trString.replace(fullContent, trStr);
	}
	if (trAll) {
		if (!isEnglish) {
			trString = QObject::tr(trString.toUtf8().constData());
		} else {
			trString = pls_uistep_v2_get_english(trString.toUtf8());
		}
	}
	return trString;
}
static QString &replaceValString(QString &dealStr, const QMap<QString, QString> &searchMap, bool isDefaultEmpty = false)
{
	QRegularExpression regExp(s_valueRegStr);
	QRegularExpressionMatchIterator iterator = regExp.globalMatch(dealStr);
	while (iterator.hasNext()) {
		QRegularExpressionMatch match = iterator.next();
		QString innerContent = match.captured(1);
		QString fullContent = match.captured(0);

		//only path obj contain this key, then will replace this parameter
		if (searchMap.contains(innerContent)) {
			dealStr.replace(fullContent, searchMap.value(innerContent));
		} else if (isDefaultEmpty) {
			dealStr.replace(fullContent, "");
		}
	}
	return dealStr;
}

static QStringList getArgsList(const QString &excelArg, const QStringList &codeArgs)
{
	if (excelArg.isEmpty()) {
		return codeArgs;
	}
	if (excelArg.startsWith("[") && excelArg.endsWith("]")) {
		QString tempExcelArg = excelArg.mid(1, excelArg.size() - 2);
		QStringList tempList = tempExcelArg.split(",");
		QStringList retList;
		for (auto item : tempList) {
			retList.append(item.trimmed());
		}
		return retList;
	}
	return QStringList(excelArg);
}

static QString &replaceQtStringFormat(QString &trString, const QJsonObject &errorObj, const PLSErrorHandler::ExtraData &extraData)
{
	if (!trString.contains("%1")) {
		return trString;
	}
	auto arg = getValue<QString>(errorObj, s_path_err_arg);
	auto argList = getArgsList(arg, extraData.defaultArg);
	for (int i = 0; i < argList.size(); i++) {
		QString index = QString::number(i + 1);
		if (trString.contains("%" + index)) {
			trString = trString.arg(argList[i]);
		}
	}
	return trString;
}

static QStringList getAppendList(const QJsonObject &errorObj)
{
	auto appendStr = getValue<QString>(errorObj, s_path_err_append);
	if (!appendStr.isEmpty()) {
		auto doc = QJsonDocument::fromJson(appendStr.toUtf8());
		if (doc.isArray()) {
			QStringList list;
			auto arr = doc.array();
			for (const auto &item : arr) {
				auto val = forceJsonValue2String(item);
				if (val.has_value()) {
					list.append(val.value());
				}
			}
			if (!list.isEmpty()) {
				return list;
			}
		}
	}
	return getValue<QStringList>(errorObj, s_path_err_append);
}

static QJsonObject loadJsonFromSplitFiles()
{
	const QString downloadDirPath = pls_get_app_user_data_dir_path_pn(QStringLiteral("resources/library/Library_Policy_PC/errorCode"), false);
	const QString appDirPath = pls_get_app_data_path_pn(QStringLiteral("resources/errorCode"));
	auto getFolderVersion = [](const QString &folderPath) -> qint64 {
		QJsonObject versionObj;
		const QString versionPath = QDir::cleanPath(folderPath + QStringLiteral("/version.json"));
		if (!pls_read_json(versionObj, versionPath)) {
			return 0;
		}
		return getValue<qint64>(versionObj, QStringLiteral("version"));
	};

	const auto downloadDirVersion = getFolderVersion(downloadDirPath);
	const auto appDirVersion = getFolderVersion(appDirPath);
	const bool isUseDownloadDir = downloadDirVersion > appDirVersion;
	const qint64 maxVersion = qMax(downloadDirVersion, appDirVersion);
	const QString targetDir = isUseDownloadDir ? downloadDirPath : appDirPath;
	PLS_INFO(s_log_moudule, "%s splitVersion downloadVersion:%lld appVersion:%lld jsonUse:%s", s_moudule_prefix, static_cast<long long>(downloadDirVersion), static_cast<long long>(appDirVersion),
		 isUseDownloadDir ? "download" : "app");

	QJsonObject versionObj;
	if (maxVersion <= 0) {
		PLS_WARN(s_log_moudule, "%s load split json failed, fallback single file.", s_moudule_prefix);
		return {};
	}

	QJsonObject mergedObj;
	mergedObj.insert(QStringLiteral("version"), maxVersion);
	int jsonCount = 0;
	const auto files = pls_enum_files(
		targetDir,
		[](const QString &, const QString &name) {
			if (!name.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
				return true;
			if (name.compare(QStringLiteral("version.json"), Qt::CaseInsensitive) == 0)
				return true;
			return false;
		},
		false, false);
	for (const auto &filePath : files) {
		QJsonObject sectionObj;
		if (!pls_read_json(sectionObj, filePath)) {
			PLS_ERROR(s_log_moudule, "%s splitVersion load json item failed:%s", s_moudule_prefix, qUtf8Printable(QFileInfo(filePath).fileName()));
			break;
		}
		PLS_INFO(s_log_moudule, "%s splitVersion load json item succeed:%s", s_moudule_prefix, qUtf8Printable(QFileInfo(filePath).fileName()));
		const QFileInfo fi(filePath);
		mergedObj.insert(fi.completeBaseName(), sectionObj);
		++jsonCount;
	}
	PLS_INFO(s_log_moudule, "%s splitVersion load json count: %d", s_moudule_prefix, jsonCount);
	return mergedObj;
}

static QJsonObject loadJsonFromSingleFile()
{
	QString downloadPath = pls_get_app_user_data_dir_path_pn(QStringLiteral("resources/library/Library_Policy_PC/errorCode.json"), false);
	QString qrcPath = pls_get_app_data_path_pn(QStringLiteral("/resources/errorCode.json"));
	QJsonObject downloadObj;
	QJsonObject qrcObj;
	bool b_download = pls_read_json(downloadObj, downloadPath);
	bool b_qrc = pls_read_json(qrcObj, qrcPath);
	if (!b_download && !b_qrc) {
		PLS_ERROR(s_log_moudule, "%s serialization of json failed", s_moudule_prefix);
		return {};
	}
	auto downloadVersion = getValue<qint64>(downloadObj, QStringLiteral("version"));
	auto qrcVersion = getValue<qint64>(qrcObj, QStringLiteral("version"));
	bool isUseDownload = downloadVersion > qrcVersion;
	PLS_INFO(s_log_moudule, "%s downloadVersion:%lld appVersion:%lld jsonUse:%s", s_moudule_prefix, static_cast<long long>(downloadVersion), static_cast<long long>(qrcVersion),
		 isUseDownload ? "download" : "app");

	return isUseDownload ? downloadObj : qrcObj;
}

PLSErrorHandler *PLSErrorHandler::instance()
{
	static PLSErrorHandler _instance;
	return &_instance;
}

PLSErrorHandler::PLSErrorHandler()
{
	qRegisterMetaType<PLSErrorHandler::RetData>("PLSErrorHandler::RetData");

	loadJson();
}
QJsonObject PLSErrorHandler::loadJson()
{
	auto tempRootJson = loadJsonFromSplitFiles();
	if (tempRootJson.isEmpty())
		tempRootJson = loadJsonFromSingleFile();
	if (tempRootJson.isEmpty()) {
		PLS_ERROR(s_log_moudule, "%s load all json failed", s_moudule_prefix);
		return {};
	}
	auto iniArray = getValue<QJsonArray>(tempRootJson, "IniValue.data");
	QMap<QString, QString> iniMap;
	QMap<QString, QString> enIniMap;
	for (auto &&v : iniArray) {
		auto item = v.toObject();
		auto key = item.value("key").toString().toUtf8();
		if (key.isEmpty()) {
			continue;
		}
		auto curStr = item.value(pls_get_current_language()).toString();
		auto enStr = item.value("en-US").toString();
		if (curStr.isEmpty()) {
			curStr = enStr;
		}
		if (!curStr.isEmpty()) {
			iniMap[key] = curStr;
		}
		if (!enStr.isEmpty()) {
			enIniMap[key] = enStr;
		}
	}

	std::unique_lock<std::shared_mutex> lock(m_mutex);
	m_rootJson = tempRootJson;
	m_iniMap = iniMap;
	m_enIniMap = enIniMap;

	return m_rootJson;
}

QJsonObject PLSErrorHandler::getRootObj() const
{
	std::shared_lock<std::shared_mutex> lock(m_mutex);
	return m_rootJson;
}

bool PLSErrorHandler::getTranslateString(const QString &key, QString &transVal, bool isSearchEnglish) const
{
	std::shared_lock<std::shared_mutex> lock(m_mutex);
	if (key.isEmpty()) {
		return false;
	}
	auto val = isSearchEnglish ? m_enIniMap.value(key) : m_iniMap.value(key);
	if (val.isEmpty()) {
		return false;
	}
	transVal = val;
	return true;
}
bool PLSErrorHandler::getEnglishStrByOtherLang(const QString &otherLang, QString &engString) const
{
	std::shared_lock<std::shared_mutex> lock(m_mutex);

	QString enKey;
	QMap<QString, QString>::const_iterator i;
	for (i = m_iniMap.constBegin(); i != m_iniMap.constEnd(); ++i) {
		if (i.value() == otherLang) {
			enKey = i.key();
			break;
		}
	}

	return getTranslateString(enKey, engString, true);
}

PLSErrorHandler::RetData PLSErrorHandler::getDataWithInheritList(const QJsonObject &rootObj, const QString &platformName, const NetworkData &netData, const QString &customErrName,
								 ExtraData &extraData, SearchType searchType)
{
	if (rootObj.isEmpty()) {
		return {};
	}
	PLSErrorHandler::RetData retData;
	QStringList inheritList = getInheritList(rootObj, platformName);

	QJsonObject apiJson;
	auto doc = QJsonDocument::fromJson(netData.errData);
	if (doc.isObject()) {
		apiJson = doc.object();
	} else if (!netData.errData.isEmpty()) {
		PLS_WARN(s_log_moudule, "%s %s Failed to parse net body json data!", qUtf8Printable(platformName), s_moudule_prefix);
	}

	for (const auto &superItem : inheritList) {
		auto _pathJson = getPathValue(rootObj, superItem, apiJson);
		for (auto it = _pathJson.constBegin(); it != _pathJson.constEnd(); it++) {
			auto strOpt = forceJsonValue2String(it.value());
			if (strOpt.has_value()) {
				extraData.pathValueMap.insert(it.key(), strOpt.value());
			}
		}
		printJson(extraData.pathValueMap, superItem + " jimbo pathObj: ");
		auto jsonArray = getValue<QJsonArray>(rootObj, QStringList({superItem, "data"}));
		if (jsonArray.isEmpty()) {
			QString log = QString("%1 %2 config failed, not found this sheet key!").arg(s_moudule_prefix).arg(superItem);
			PLS_WARN(s_log_moudule, qUtf8Printable(log));
			assert(false && "not found this sheet key");
		}
		auto matchedObj = getErrObj(jsonArray, superItem, customErrName, extraData);
		if (matchedObj.isEmpty()) {
			continue;
		}
		retData = fillRetDataWithMatchObj(matchedObj, superItem, extraData, searchType);
		break;
	}
	return retData;
}

PLSErrorHandler::RetData PLSErrorHandler::getAlertString(const NetworkData &netData, const QString &platformName, const QString &customErrName, const ExtraData &extraData)
{
	auto rootObj = PLSErrorHandler::instance()->getRootObj();
	ExtraData tempOtherData = generateNewOtherData(extraData, platformName);
	tempOtherData.pathValueMap[s_api_statusCode] = QString::number(netData.statusCode);
	PLSErrorHandler::RetData retData;
	if (QNetworkReply::ConnectionRefusedError <= netData.netError && netData.netError <= QNetworkReply::UnknownNetworkError) {
		retData = PLSErrorHandler::getAlertStringByPrismCode(COMMON_NETWORK_ERROR, platformName, customErrName, tempOtherData);
	} else {
		retData = getDataWithInheritList(rootObj, platformName, netData, customErrName, tempOtherData, SearchType::apiError);
	}
	fillUnknownDataIfEmpty(retData, s_default_error_type, platformName, customErrName, tempOtherData, SearchType::apiError);
	return retData;
}

PLSErrorHandler::RetData PLSErrorHandler::showAlert(const NetworkData &netData, const QString &platformName, const QString &customErrName, const ExtraData &extraData, QWidget *showParent)
{
	PLSErrorHandler::RetData data = getAlertString(netData, platformName, customErrName, extraData);
	PLSErrorHandler::directShowAlert(data, showParent);
	return data;
}

PLSErrorHandler::RetData PLSErrorHandler::getAlertStringByCustomErrName(const QString &customErrName, const QString &platformName, const ExtraData &extraData)
{
	auto rootObj = PLSErrorHandler::instance()->getRootObj();
	ExtraData tempOtherData = generateNewOtherData(extraData, platformName);
	PLSErrorHandler::RetData retData = getDataWithInheritList(rootObj, PLSErrKeyDefault, NetworkData(), customErrName, tempOtherData, SearchType::customError);
	fillUnknownDataIfEmpty(retData, s_default_error_type, "", customErrName, tempOtherData, SearchType::customError);
	return retData;
}
PLSErrorHandler::RetData PLSErrorHandler::showAlertByCustomErrName(const QString &customErrName, const QString &platformName, const ExtraData &extraData, QWidget *showParent)
{
	PLSErrorHandler::RetData data = getAlertStringByCustomErrName(customErrName, platformName, extraData);
	PLSErrorHandler::directShowAlert(data, showParent);
	return data;
}
PLSErrorHandler::RetData PLSErrorHandler::getAlertStringByErrCode(const QString &errorCode, const QString &platformName, const QString &customErrName, const ExtraData &extraData)
{
	auto rootObj = PLSErrorHandler::instance()->getRootObj();
	ExtraData tempOtherData = generateNewOtherData(extraData, platformName);
	tempOtherData.pathValueMap[s_api_errorCode] = errorCode;

	PLSErrorHandler::RetData retData = getDataWithInheritList(rootObj, platformName, {}, customErrName, tempOtherData, SearchType::apiError);
	fillUnknownDataIfEmpty(retData, s_default_error_type, platformName, customErrName, tempOtherData, SearchType::apiError);
	return retData;
}
PLSErrorHandler::RetData PLSErrorHandler::showAlertByErrCode(const QString &errorCode, const QString &platformName, const QString &customErrName, const ExtraData &extraData, QWidget *showParent)
{
	PLSErrorHandler::RetData data = getAlertStringByErrCode(errorCode, platformName, customErrName, extraData);
	PLSErrorHandler::directShowAlert(data, showParent);
	return data;
}

PLSErrorHandler::RetData PLSErrorHandler::getAlertStringByPrismCode(ErrCode prismCode, const QString &platformName, const QString &customErrName, const ExtraData &extraData)
{
	ExtraData tempOtherData = generateNewOtherData(extraData, platformName);
	PLSErrorHandler::RetData retData;
	fillUnknownDataIfEmpty(retData, prismCode, platformName, customErrName, tempOtherData, SearchType::prismCode);
	return retData;
}

void PLSErrorHandler::fillUnknownDataIfEmpty(RetData &retData, ErrCode prismCode, const QString &platformName, const QString &customErrName, ExtraData &extraData, SearchType searchType)
{
	if (retData.isMatched || retData.isNotError) {
		return;
	}
	if (!extraData.isShowUnknownError && prismCode == s_default_error_type) {
		return;
	}
	auto rootObj = PLSErrorHandler::instance()->getRootObj();
	QStringList inheritList = getInheritList(rootObj, platformName);
	for (auto superItem : inheritList) {
		auto jsonArray = getValue<QJsonArray>(rootObj, QStringList({superItem, "data"}));
		auto matchedObj = getErrObjByPrismCode(prismCode, jsonArray, superItem, customErrName, extraData);
		if (matchedObj.isEmpty()) {
			continue;
		}
		retData = fillRetDataWithMatchObj(matchedObj, superItem, extraData, searchType);
		break;
	}
	if (retData.isMatched || retData.isNotError) {
		return;
	}
	if (prismCode == s_default_error_type) {
		retData.isMatched = true;
		retData.alertMsgKey = "Common.Unknown.Error";
		retData.alertMsg = QObject::tr(qUtf8Printable(retData.alertMsgKey));
		retData.alertMsgEnglish = pls_uistep_v2_get_english(retData.alertMsgKey.toUtf8());
		retData.prismCode = COMMON_UNKNOWN_ERROR;
		retData.prismName = QVariant::fromValue(retData.prismCode).toString();
		retData.isExactMatch = false;
		retData.prismCodePrefix = "COMMON_";
		retData.extraData = extraData;
		printLog(retData, false);
	} else {
		fillUnknownDataIfEmpty(retData, s_default_error_type, platformName, customErrName, extraData, searchType);
	}
}

PLSErrorHandler::RetData PLSErrorHandler::showAlertByPrismCode(ErrCode prismCode, const QString &platformName, const QString &customErrName, const ExtraData &extraData, QWidget *showParent)
{
	PLSErrorHandler::RetData data = getAlertStringByPrismCode(prismCode, platformName, customErrName, extraData);
	PLSErrorHandler::directShowAlert(data, showParent);
	return data;
}

static void _getInheritList(QStringList &pathList, const QJsonObject &obj, const QString &platformName)
{
	auto jsonArray = getValue<QJsonArray>(obj, QStringList({platformName, s_path_extra, "inherit"}));
	for (auto i = 0; i < jsonArray.size(); i++) {
		auto superName = jsonArray[i].toString();
		if (superName.isEmpty()) {
			continue;
		}
		if (pathList.contains(superName)) {
			continue;
		}
		pathList.append(superName);
		_getInheritList(pathList, obj, superName);
	}
}

QStringList PLSErrorHandler::getInheritList(const QJsonObject &obj, const QString &platformName)
{
	if (obj.isEmpty()) {
		return {};
	}
	QStringList pathList;
	pathList.append(platformName);
	_getInheritList(pathList, obj, platformName);

	for (auto single : s_specialPath) {
		if (pathList.contains(single)) {
			pathList.removeAll(single);
		}
		//move to end
		pathList.append(single);
	}
	return pathList;
}

QJsonObject PLSErrorHandler::getErrObj(const QJsonArray &jsonArray, const QString &platformName, const QString &customErrName, const ExtraData &extraData)
{
	auto compareString = [](const QString &apiV, const QString &pathV) -> bool {
		if (apiV.startsWith("/") && apiV.endsWith("/")) {
			QString tempApiReg = apiV.mid(1, apiV.size() - 2);
			QRegularExpression re(tempApiReg);
			QRegularExpressionMatch match = re.match(pathV);
			return match.hasMatch();
		}
		return apiV == pathV;
	};

	bool isDefaultPath = platformName == PLSErrKeyDefault;

	std::pair<int /*match index*/, int /*match count*/> matchPair({-1, -1});

	for (auto i = 0; i < jsonArray.size(); i++) {
		auto subData = jsonArray[i].toObject();

		if (!isValidErrPhase(getValue<QString>(subData, s_path_phase), extraData.errPhase)) {
			continue;
		}

		auto apiData = subData["api"].toObject();

		if (isDefaultPath) {
			//only found one is succeed
			//search last default path
			bool isMatched = isDefaultKeyMatch(apiData, customErrName);
			if (isMatched) {
				matchPair = {i, 1};
				break;
			}
		} else {
			//all matched ,so succeed
			auto keys = apiData.keys();
			bool isMatched = keys.empty() ? false : true;
			for (auto subKey : keys) {
				auto excelValue = getValue<QString>(apiData, subKey);
				transTrString(excelValue, false, false);
				bool containValue = extraData.pathValueMap.contains(subKey);

				QString pathValue = containValue ? extraData.pathValueMap.value(subKey) : "nullErr";
				if (s_isPrintDebugLog) {
					qDebug() << s_moudule_prefix << platformName << "." << subKey << "compare excel apiValue:" << excelValue << " get pathValue:" << pathValue;
				}
				if (!containValue || !compareString(excelValue, pathValue)) {
					isMatched = false;
					break;
				}
			}
			if (isMatched) {
				if (keys.size() > matchPair.second) {
					matchPair = {i, keys.size()};
				}
			}
		}
	}
	if (matchPair.first > -1) {
		return jsonArray[matchPair.first].toObject();
	}
	return {};
}

QJsonObject PLSErrorHandler::getErrObjByPrismCode(ErrCode prismCode, const QJsonArray &jsonArray, const QString &platformName, const QString &customErrName, const ExtraData &extraData)
{
	bool isDefaultPath = platformName == PLSErrKeyDefault;
	for (auto i = 0; i < jsonArray.size(); i++) {
		auto subData = jsonArray[i].toObject();
		if (!isValidErrPhase(getValue<QString>(subData, s_path_phase), extraData.errPhase)) {
			continue;
		}
		bool isMatched = false;
		if (isDefaultPath) {
			auto apiData = subData["api"].toObject();
			isMatched = isDefaultKeyMatch(apiData, customErrName);
		} else {
			auto errData = subData["err"].toObject();
			isMatched = getValue<int>(subData, s_path_err_prismCode, ErrCode::INVALID) == (int)prismCode;
		}
		if (isMatched) {
			return subData;
		}
	}
	return {};
}

bool PLSErrorHandler::isDefaultKeyMatch(const QJsonObject &apiData, const QString &customErrName)
{
	auto _matchNext = [](const QString &regKeys, const QString &compareStr) {
		auto regList = regKeys.split(",");
		for (const auto &reg : regList) {
			if (reg.trimmed() == compareStr) {
				return true;
			}
		}
		return false;
	};

	auto regKeys = getValue<QString>(apiData, QStringLiteral("defaultKey"));
	return _matchNext(regKeys, customErrName);
}

QJsonObject PLSErrorHandler::getPathValue(const QJsonObject &obj, const QString &platformName, const QJsonObject &apiJson)
{

	QJsonObject valuesObj;

	auto pathObj = getValue<QJsonObject>(obj, QStringList({platformName, s_path_extra, "path"}));
	auto keys = pathObj.keys();
	for (auto i = 0; i < keys.size(); i++) {
		auto key = keys[i];
		auto valueList = getValue<QStringList>(pathObj, key); //YouTube.path.reason[]
		//get value in network json
		for (const auto &item : valueList) {
			auto pathList = pls_to_attr_names(item.split("."));
			std::optional<QJsonValue> jsonValue = pls_get_attr(apiJson, pathList);
			if (!jsonValue.has_value()) {
				continue;
			}
			auto jsonString = forceJsonValue2String(jsonValue.value());
			if (jsonString.has_value()) {
				valuesObj[key] = jsonString.value(); //{reason: channelNotFound}
				continue;
			}
		}
	}
	return valuesObj;
}
PLSErrorHandler::RetData PLSErrorHandler::fillRetDataWithMatchObj(const QJsonObject &matchedObj, const QString &platformName, ExtraData &extraData, SearchType searchType)
{
	if (matchedObj.isEmpty()) {
		return {};
	}
	RetData data;
	data.isMatched = true;
	data.prismCode = (ErrCode)getValue<int>(matchedObj, s_path_err_prismCode);
	data.prismName = getValue<QString>(matchedObj, s_path_err_prismCodeName);
	extraData.pathValueMap["prismCode"] = QString::number(data.prismCode);
	extraData.pathValueMap["prismName"] = data.prismName;

	auto errAppendList = getAppendList(matchedObj);
	if (!errAppendList.isEmpty()) {
		extraData.appendList = errAppendList;
	}
	auto errAppendJoin = getValue<QString>(matchedObj, s_path_err_append_join);
	if (!errAppendJoin.isEmpty()) {
		extraData.appendJoin = errAppendJoin;
	}
	extraData.appendList.removeDuplicates();

	auto msgPair = getOriginalAlertString(matchedObj, s_path_err_msgKey, "err.msgText");
	data.alertMsg = dealOriginalAlertString(msgPair.first, matchedObj, extraData, false);
	data.alertMsgEnglish = dealOriginalAlertString(pls_uistep_v2_get_english(msgPair.second.toUtf8()), matchedObj, extraData, true);
	data.alertMsgKey = msgPair.second.replace("%", "%%");
	data.matchedObj = matchedObj;
	data.alertType = str2AlertType(getValue<QString>(matchedObj, s_path_err_alertType), PLSErrorHandler::AlertType::Error);
	data.errorType = str2ErrorType(getValue<QString>(matchedObj, s_path_err_errorType));
	data.alertStage = getValue<QString>(matchedObj, s_path_err_stage);
	data.extraData = extraData;

	auto macroPrefixes = getValue<QStringList>(PLSErrorHandler::instance()->getRootObj(), s_path_macro_prefix);
	for (const auto &prefix : macroPrefixes) {
		if (data.prismName.startsWith(prefix)) {
			data.prismCodePrefix = prefix;
			break;
		}
	}
	if (data.prismCodePrefix.isEmpty()) {
		data.prismCodePrefix = data.prismName;
	}

	switch (searchType) {
	case SearchType::apiError:
	case SearchType::prismCode:
	case SearchType::errorCode:
		if (platformName == PLSErrKeyDefault) {
			data.isExactMatch = false;
		}
		break;
	case SearchType::customError:
	default:
		break;
	}
	if (data.prismCode == PLSErrorHandler::COMMON_UNKNOWN_ERROR) {
		data.isExactMatch = false;
	}
	QStringList appendList;
	if (data.prismCode == PLSErrorHandler::COMMON_NETWORK_ERROR) {
		appendList.append("network error");
	} else {
		for (QString append : extraData.logAppend) {
			appendList.append(replaceValString(append, extraData.pathValueMap, true));
		}
	}
	if (!appendList.isEmpty()) {
		data.failedLogString = appendList.join("\n");
	}
	printLog(data, false);
	return data;
}
std::pair<QString /*trans*/, QString /*ini key*/> PLSErrorHandler::getOriginalAlertString(const QJsonObject &errorObj, const QString &msgKeyPath, const QString &msgTextPath)
{
	auto msgKey = getValue<QString>(errorObj, msgKeyPath);
	QString trString = QObject::tr(msgKey.toUtf8().constData());
	if (trString != msgKey) {
		return {trString, msgKey};
	}
	auto msgText = getValue<QString>(errorObj, msgTextPath);
	if (!msgText.isEmpty()) {
		return {msgText, msgText};
	}
	return {msgKey, msgKey};
}

QString PLSErrorHandler::dealOriginalAlertString(const QString &originalStr, const QJsonObject &errorObj, const ExtraData &extraData, bool isEnglish)
{
	const static QString getValueRegStr("\\{\\{(.+?)\\}\\}");
	auto getStrIfFound = [](const QString &dealStr, const QMap<QString, QString> &searchObj) -> std::optional<QString> {
		QString tmp(dealStr);
		QRegularExpression regExp(s_valueRegStr);
		QRegularExpressionMatchIterator iterator = regExp.globalMatch(dealStr);
		bool isFound = !iterator.hasNext();
		while (iterator.hasNext()) {
			QRegularExpressionMatch match = iterator.next();
			QString innerContent = match.captured(1);
			//only path obj contain this key, then will replace this parameter
			if (searchObj.contains(innerContent)) {
				isFound = true;
				QString fullContent = match.captured(0);
				tmp.replace(fullContent, searchObj.value(innerContent));
			}
		}
		return isFound ? std::optional<QString>(tmp) : std::nullopt;
	};

	if (originalStr.isEmpty()) {
		return originalStr;
	}
	QString dealStr = originalStr;
	QStringList translatedList;
	for (const auto &item : extraData.appendList) {
		auto tmpItem = item;
		transTrString(tmpItem, false, isEnglish);
		auto appendStrOpt = getStrIfFound(tmpItem, extraData.pathValueMap);
		if (appendStrOpt.has_value()) {
			translatedList.append(appendStrOpt.value());
		}
	}

	if (!translatedList.empty()) {
		QString joinStr = extraData.appendJoin;
		dealStr.append("\n").append(translatedList.join(joinStr));
	}

	return transAndDealValStr(dealStr, errorObj, extraData, isEnglish);
}

QJsonArray PLSErrorHandler::getButtonsObj(const QJsonObject &errorObj)
{

	auto buttonsStr = getValue<QString>(errorObj, s_path_err_buttons);
	if (buttonsStr.isEmpty()) {
		return {};
	}
	auto doc = QJsonDocument::fromJson(buttonsStr.toUtf8());
	if (!doc.isArray()) {
		return {};
	}

	auto root = doc.array();
	return root;
}

QString PLSErrorHandler::getButtonOpenUrl(const QString &str, const QMap<QString, QString> &pathValueMap)
{
	const static QString action_open = QStringLiteral("open.");
	if (str.isEmpty()) {
		return {};
	}
	if (str.startsWith(action_open)) {
		auto temp = str.mid(action_open.size(), str.size() - action_open.size());
		if (temp.isEmpty()) {
			return {};
		}
		return tr(replaceValString(temp, pathValueMap).toUtf8().constData());
	}
	return {};
}

bool PLSErrorHandler::dealAlertAction(const QString &str, const QMap<QString, QString> &pathValueMap)
{
	auto openUrl = getButtonOpenUrl(str, pathValueMap);
	if (openUrl.isEmpty()) {
		return false;
	}
	PLS_INFO(s_log_moudule, "%s buttons open url:%s", s_moudule_prefix, qUtf8Printable(openUrl));
	QDesktopServices::openUrl(QUrl(openUrl));
	return true;
}

QDialogButtonBox::StandardButton PLSErrorHandler::directShowAlert(RetData &data, QWidget *showParent)
{
	if (pls_current_is_main_thread()) {
		showAlertInMain(data, showParent);
		return data.clickedBtn;
	}
	QEventLoop loop;
	pls_async_call_mt(PLSErrorHandler::instance(), [&data, &loop, showParent]() {
		showAlertInMain(data, showParent);
		loop.quit();
	});
	loop.exec();
	return data.clickedBtn;
}

static QPair<QString /*current*/, QString /*english*/> getInheritValueString(const QString &alertKey, const QJsonObject &rootObj, const QStringList &inheritList, const PLSErrorHandler::RetData &data)
{

	if (rootObj.isEmpty()) {
		return {};
	}

	QString valueStr;
	QJsonValue firstVal = getValue<QJsonValue>(data.matchedObj, alertKey);
	if (firstVal.isString()) {
		valueStr = firstVal.toString();
	}
	if (valueStr.isEmpty()) {
		for (const auto &superItem : inheritList) {
			QString keyPath = QString("%1.%2.%3").arg(superItem).arg("extra").arg(alertKey);
			QJsonValue val = getValue<QJsonValue>(rootObj, keyPath);
			if (val.isString()) {
				valueStr = val.toString();
				break;
			}
		}
	}
	if (valueStr.isEmpty()) {
		QString log = QString("%1 %2 found alert special text failed errorCode:%3, key path: %4")
				      .arg(s_moudule_prefix)
				      .arg(inheritList.first())
				      .arg(getValue<int>(data.matchedObj, "err.prismCode"))
				      .arg(alertKey);
		PLS_WARN(s_log_moudule, qUtf8Printable(log));
		assert(false && "not found");
		return {};
	}
	auto currentStr = replaceValString(valueStr, data.extraData.pathValueMap);
	auto englishStr = currentStr;
	transTrString(currentStr, true, false);
	transTrString(englishStr, true, true);
	return {currentStr, englishStr};
}

PLSErrorHandler::AlertType PLSErrorHandler::getButtonAndMsgDatas(QMap<PLSAlertView::Button, QString> &actionMap, QMap<PLSAlertView::Button, pls_text_t> &buttonsMap, RetData &data)
{
	AlertType finalType = AlertType::Normal;

	if (data.alertType == AlertType::Normal || data.alertType == AlertType::Error) {
		finalType = data.alertType;
		auto buttonsArray = getButtonsObj(data.matchedObj);
		if (!buttonsArray.isEmpty()) {
			for (auto i = 0; i < buttonsArray.size(); i++) {
				auto bObj = buttonsArray[i].toObject();
				auto qBtn = str2DialogBtn(getValue<QString>(bObj, QString("button")));
				if (qBtn == QDialogButtonBox::NoButton) {
					PLS_ERROR(s_log_moudule, "%s buttons btn is undefined, errorCode: %i", s_moudule_prefix, data.prismCode);
					buttonsMap.clear();
					break;
				}
				auto btnStr = QObject::tr(getValue<QString>(bObj, "tr").toUtf8().constData());
				if (btnStr.isEmpty()) {
					btnStr = QObject::tr(dialogBtn2Str(qBtn));
				}
				buttonsMap.insert(qBtn, btnStr);
				actionMap.insert(qBtn, getValue<QString>(bObj, QStringLiteral("action")));
			}
		}
	} else {
		QPair<QString /*current*/, QString /*english*/> btnText;
		QPair<QString /*current*/, QString /*english*/> btnLink;
		auto rootObj = PLSErrorHandler::instance()->getRootObj();
		QStringList inheritList = getInheritList(rootObj, data.extraData.platformName);

		if (data.alertType == AlertType::Normal_Code || data.alertType == AlertType::Normal_Code_Blog || data.alertType == AlertType::Normal_Code_Link) {
			finalType = AlertType::Normal;

			auto codeText = getInheritValueString("alert.normal.codeText", rootObj, inheritList, data);
			if (data.alertType == AlertType::Normal_Code_Blog) {
				btnText = getInheritValueString("alert.normal.blogText", rootObj, inheritList, data);
				btnLink = getInheritValueString("alert.normal.blogLink", rootObj, inheritList, data);
			} else if (data.alertType == AlertType::Normal_Code_Link) {
				btnText = getInheritValueString("alert.normal.thirdLinkText", rootObj, inheritList, data);
				btnLink = getInheritValueString("alert.normal.thirdLink", rootObj, inheritList, data);
			}
			data.alertMsg.append(codeText.first);
			data.alertMsgEnglish.append(codeText.second);
		} else if (data.alertType == AlertType::Error_Blog || data.alertType == AlertType::Error_Link || data.alertType == AlertType::Error_Blog_Link) {
			finalType = AlertType::Error;

			if (data.alertType == AlertType::Error_Blog || data.alertType == AlertType::Error_Blog_Link) {
				btnText = getInheritValueString("alert.error.blogText", rootObj, inheritList, data);
				btnLink = getInheritValueString("alert.error.blogLink", rootObj, inheritList, data);
			}

			if (data.alertType == AlertType::Error_Link || data.alertType == AlertType::Error_Blog_Link) {
				auto codeText = getInheritValueString("alert.error.thirdLinkText", rootObj, inheritList, data);
				auto codeTextLink = getInheritValueString("alert.error.thirdLink", rootObj, inheritList, data);
				if (!codeText.first.isEmpty() && !codeTextLink.first.isEmpty()) {
					{
						QString links =
							QString("<br><br><a href='%1' style='color:#effc35; text-decoration:none'>").arg(codeTextLink.first).append(codeText.first).append("</a>");
						data.alertMsg.append(links);
						if (!Qt::mightBeRichText(data.alertMsg)) {
							data.alertMsg = QString("<qt>%1</qt>").arg(data.alertMsg);
						}
					}
					{
						QString linksEnglish =
							QString("<br><br><a href='%1' style='color:#effc35; text-decoration:none'>").arg(codeTextLink.second).append(codeText.second).append("</a>");
						data.alertMsgEnglish.append(linksEnglish);
						if (!Qt::mightBeRichText(data.alertMsgEnglish)) {
							data.alertMsgEnglish = QString("<qt>%1</qt>").arg(data.alertMsgEnglish);
						}
					}
				}
			}
		}
		transAndDealValStr(data.alertMsg, data.matchedObj, data.extraData, false);
		transAndDealValStr(data.alertMsgEnglish, data.matchedObj, data.extraData, true);
		buttonsMap.insert(QDialogButtonBox::Ok, QObject::tr(dialogBtn2Str(QDialogButtonBox::Ok)));
		if (!btnText.first.isEmpty()) {
			auto qHelpBtn = QDialogButtonBox::Help;
			buttonsMap.insert(qHelpBtn, btnText.first);
			actionMap.insert(qHelpBtn, QString("open.").append(btnLink.first));
		}
	}
	if (buttonsMap.isEmpty()) {
		auto okString = QObject::tr(dialogBtn2Str(QDialogButtonBox::Ok));
		buttonsMap.insert(QDialogButtonBox::Ok, okString);
	}
	return finalType;
}

QString &PLSErrorHandler::transAndDealValStr(QString &dealStr, const QJsonObject &errorObj, const ExtraData &extraData, bool isEnglish)
{
	replaceQtStringFormat(dealStr, errorObj, extraData);
	transTrString(replaceValString(dealStr, extraData.pathValueMap), false, isEnglish);
	if (Qt::mightBeRichText(dealStr)) {
		dealStr.replace("\n", "<br>");
	}
	return dealStr;
}

static PLSAlertView::Button getDefaultBtn(PLSErrorHandler::RetData &data)
{
	PLSAlertView::Button defaultButton = PLSAlertView::Button::Ok;
	const QString defaultBtnStr = getValue<QString>(data.matchedObj, s_path_err_default_btn);
	if (!defaultBtnStr.isEmpty()) {
		auto defaultBtn = str2DialogBtn(defaultBtnStr);
		if (defaultBtn != QDialogButtonBox::NoButton) {
			defaultButton = static_cast<PLSAlertView::Button>(defaultBtn);
		}
	}
	return defaultButton;
}

void PLSErrorHandler::showAlertInMain(RetData &data, QWidget *showParent)
{
	if (data.isNotError)
		return;

	if (!data.isMatched) {
		return;
	}

	QMap<PLSAlertView::Button, QString> actionMap;
	QMap<PLSAlertView::Button, pls_text_t> buttonsMap;
	AlertType finalType = getButtonAndMsgDatas(actionMap, buttonsMap, data);
	QString title = QObject::tr(getValue<QString>(data.matchedObj, s_path_err_title, "Alert.Title").toUtf8().constData());

	PLSAlertView::Button defaultButton = getDefaultBtn(data);

	const QString checkBox = QObject::tr(getValue<QString>(data.matchedObj, "err.checkbox").toUtf8().constData());

	if (checkBox.isEmpty()) {
		PLSAlertView::Button ret = PLSAlertView::Button::Ok;
		if (finalType == AlertType::Error) {
			data.clickedBtn = pls_alert_error_message(showParent, title, pls_text_t(data.alertMsg, data.alertMsgEnglish, data.alertStage), QString::number(data.prismCode), buttonsMap,
								  defaultButton, -1, data.extraData.propertiesMap);
		} else {
			data.clickedBtn =
				PLSAlertView::warning(showParent, title, pls_text_t(data.alertMsg, data.alertMsgEnglish, data.alertStage), buttonsMap, defaultButton, -1, data.extraData.propertiesMap);
		}
	} else {
		PLSAlertView::Result ret;
		if (finalType == AlertType::Error) {
			data.clickedBtn = pls_alert_error_message(showParent, title, pls_text_t(data.alertMsg, data.alertMsgEnglish, data.alertStage), QString::number(data.prismCode), buttonsMap,
								  defaultButton, -1, data.extraData.propertiesMap);
		} else {
			ret = PLSAlertView::warning(showParent, title, pls_text_t(data.alertMsg, data.alertMsgEnglish, data.alertStage), checkBox, buttonsMap, defaultButton, -1,
						    data.extraData.propertiesMap);
			data.clickedBtn = ret.button;
			data.isCheckBoxClick = ret.isChecked;
		}
	}

	PLSErrorHandler::dealAlertAction(actionMap.value(data.clickedBtn), data.extraData.pathValueMap);
}

bool PLSErrorHandler::isValidErrPhase(const QString &jsonPhase, const QString &userPhase)
{
	auto _temp = jsonPhase.trimmed().toLower();
	auto lowerUser = userPhase.toLower();
	if (_temp.isEmpty() || _temp == PLSErrPhaseAll) {
		return true;
	}
	if (lowerUser.isEmpty() || lowerUser == PLSErrPhaseAll) {
		return true;
	}
	if (_temp.contains(lowerUser)) {
		return true;
	}
	return false;
}
PLSErrorHandler::ExtraData PLSErrorHandler::generateNewOtherData(const ExtraData &extraData, const QString &platformName)
{
	auto rootObj = PLSErrorHandler::instance()->getRootObj();
	ExtraData retOtherData = extraData;
	retOtherData.defaultArg = retOtherData.defaultArg.isEmpty() ? QStringList(platformName) : retOtherData.defaultArg;
	if (rootObj.isEmpty()) {
		return retOtherData;
	}
	auto firstAppendList = getValue<QStringList>(rootObj, QStringList({platformName, s_path_extra, "append"}));
	retOtherData.appendList += firstAppendList;
	retOtherData.appendList.removeDuplicates();
	auto firstAppendJoinList = getValue<QStringList>(rootObj, QStringList({platformName, s_path_extra, "appendJoin"}));
	if (!firstAppendJoinList.isEmpty()) {
		retOtherData.appendJoin = firstAppendJoinList.first();
	}

	auto logAppends = getValue<QStringList>(rootObj, QStringList({platformName, s_path_extra, "logAppend"}));
	retOtherData.logAppend += logAppends;
	retOtherData.logAppend.removeDuplicates();

	retOtherData.platformName = platformName;
	retOtherData.errPhase = extraData.errPhase.trimmed();

	if (!retOtherData.pathValueMap.contains("platformName")) {
		retOtherData.pathValueMap["platformName"] = platformName;
	}
	if (!retOtherData.pathValueMap.contains("logPlatformName")) {
		retOtherData.pathValueMap["logPlatformName"] = retOtherData.pathValueMap["platformName"];
	}
	if (!retOtherData.urlEn.isEmpty()) {
		retOtherData.urlEn = retOtherData.urlEn.replace("%", "%%");
	}
	if (!retOtherData.urlKr.isEmpty()) {
		retOtherData.urlKr = retOtherData.urlKr.replace("%", "%%");
	}

	return retOtherData;
}
void PLSErrorHandler::printLog(const RetData &retData, bool forcePrint)
{
	const ExtraData &extraData = retData.extraData;
	if (!forcePrint) {
		if (!PLSErrorHandler::instance()->isPrintLog())
			return;
		if (!extraData.printLog)
			return;
	}
	auto logPlatformName = extraData.pathValueMap["logPlatformName"];
	QString logPre = QString("%1 matched the error case. platformName: %2").arg(s_moudule_prefix).arg(logPlatformName);

	QString logKr = QString("\nerrorUrl: ").append(extraData.urlKr);
	QString logEn = QString("\nerrorUrl: ").append(extraData.urlEn);

	QString logSuffix = QString("\nprismErrorCode: ").append(QString::number(retData.prismCode));
	logSuffix = logSuffix.append("\nprismErrorName: ").append(retData.prismName);
	logSuffix = logSuffix.append("\nalertKey: ").append(retData.alertMsgKey);
	logSuffix = logSuffix.append("\nalertMsg: ").append(retData.alertMsg);
	logSuffix = logSuffix.append("\nisExactMatch: ").append(pls_bool_2_string(retData.isExactMatch));

	if (!retData.failedLogString.isEmpty()) {
		logSuffix = logSuffix.append("\n").append(retData.failedLogString);
	}

	logKr = logPre + logKr + logSuffix;
	logEn = logPre + logEn + logSuffix;

	QVariantMap anaMap;
	anaMap["dev"] = pls_bool_2_string(pls_is_dev());
	anaMap["type"] = "APIError";
	anaMap["errcode"] = QString::number(retData.prismCode);
	anaMap["errname"] = retData.prismName;
	anaMap["msgkey"] = retData.alertMsgKey;
	anaMap["platformName"] = logPlatformName;
	anaMap["platformPhase"] = retData.prismCodePrefix;
	anaMap["isExactMatch"] = pls_bool_2_string(retData.isExactMatch);
	anaMap["url"] = extraData.urlEn;

	std::vector<std::pair<QByteArray, QByteArray>> fields_ByteArray;
	for (auto iter = anaMap.begin(); iter != anaMap.end(); iter++) {
		fields_ByteArray.push_back({iter.key().toUtf8(), iter.value().toString().toUtf8()});
	}
	std::vector<std::pair<const char *, const char *>> fileds = pls_to_fields(fields_ByteArray);

	for (auto &item : fileds) {
		if (pls_is_empty(item.second)) {
			item.second = "empty";
			QString log = QString("%1 fields value is empty, which key is: %2").arg(s_moudule_prefix).arg(item.first);
			PLS_WARN(s_log_moudule, qUtf8Printable(log));
			assert(false && "value is empty");
		}
	}

	PLS_LOGEX(PLS_LOG_INFO, s_log_moudule, fileds, qUtf8Printable(logEn));
	if (!extraData.urlKr.isEmpty() && extraData.urlKr != extraData.urlEn) {
		auto logFromKr_utf8 = extraData.urlKr.toUtf8();
		for (auto &item : fileds) {
			if (pls_is_equal(item.first, "url")) {
				item.second = logFromKr_utf8.constData();
				break;
			}
		}
		PLS_LOGEX_KR(PLS_LOG_INFO, s_log_moudule, fileds, qUtf8Printable(logKr));
	}
}
