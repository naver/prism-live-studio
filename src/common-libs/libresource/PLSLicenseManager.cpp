#include "PLSLicenseManager.h"
#include "PLSResourceManager.h"
#include "libhttp-client.h"
#include "liblog.h"
#include "PLSCommonConst.h"
#include "PLSResCommonFuns.h"
#include <QJsonObject>

#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

#define ITEMS QStringLiteral("items")
#define ITEMID QStringLiteral("itemId")
#define RESOURCEURL QStringLiteral("resourceUrl")
#define SENSETIME_ZIP_NAME QStringLiteral("library_SenseTime_PC.zip")

constexpr auto SENSETIME_NEW_VERSION_FILE_PATH = "/beauty/library_SenseTime_PC/2.5.0/";
constexpr auto SENSETIME_NEW_VERSION_FILE_NAME = "sense_license_encode.lic";
constexpr auto SENSETIME_FILE_PATH = "/beauty/library_SenseTime_PC/";
constexpr auto BEAUTY_PATH = "/beauty/";
constexpr auto LICENSE_MGR_MODULE_NAME = "license-manager";

void PLSLicenseManager::getNewestLicense()
{
	PLS_INFO(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] start request newest categories.json files from server.");

	auto url = QString("%1%2").arg(pls_http_api_func::getPrismSynGateWay()).arg(pls_resource_const::PLS_CATEGORY_URL);
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)                     //
				   .hmacUrl(url, pls_http_api_func::getPrismHamcKey()) //
				   .forDownload(false)                                 //
				   .withLog()                                          //
				   .okResult([](const pls::http::Reply &reply) {
					   PLS_INFO(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getNewestLicense: Get categories.json successfully");
					   QJsonArray items = QJsonDocument::fromJson(reply.data()).array();
					   for (auto item : items) {
						   auto id = item.toObject().value("categoryId").toString();
						   if (0 == id.compare("library")) {
							   auto resource = item.toObject().value("resourceUrl").toString();
							   getLibrary(resource);
							   return;
						   }
					   }
					   PLS_WARN(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getNewestLicense: Failed to parse categories.json, download library.json abort");
				   })
				   .failResult([](const pls::http::Reply &reply) {
					   PLS_WARN(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getNewestLicense: Failed to get categories.json, erroCode: %d", static_cast<int>(reply.error()));
				   }));
}

void PLSLicenseManager::getLibrary(const QString &url)
{
	if (url.isEmpty()) {
		PLS_WARN(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getLibrary: library url is empty, download license abort.");
		return;
	}

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get) //
				   .hmacUrl(url, "")               //
				   .forDownload(false)             //
				   .withLog()                      //
				   .okResult([](const pls::http::Reply &reply) {
					   PLS_INFO(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getLibrary: Get library.json successfully");
					   auto obj = pls_to_json_object(reply.data());
					   if (!obj.empty()) {
						   auto array = obj.value(ITEMS).toArray();
						   for (const auto &map : array) {
							   if (map.toObject().value(ITEMID).toString() == pls_http_api_func::getSenseTimeId()) {
								   auto licenseUrl = map.toObject().value(RESOURCEURL).toString();
								   getLicense(licenseUrl);
								   return;
							   }
						   }
					   }
					   PLS_WARN(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getLibrary: Failed to parse library.json, download license abort");
				   })
				   .failResult([](const pls::http::Reply &reply) {
					   PLS_WARN(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getLibrary: Failed to get library.json, errorCode: %d", static_cast<int>(reply.error()));
				   }));
}

static bool pls_copy_file(const QString &fileName, const QString &newName, bool overwrite = true, bool sendSre = false)
{
	if (fileName.isEmpty() || newName.isEmpty())
		return false;
#if defined(Q_OS_WIN)
	auto ret = CopyFile(fileName.toStdWString().c_str(), newName.toStdWString().c_str(), !overwrite);
	if (!ret) {
		auto errorCode = GetLastError();
#elif defined(Q_OS_MACOS)
	int errorCode = 0;
	auto ret = pls_copy_file_with_error_code(fileName, newName, overwrite, errorCode);
	if (!ret) {
#endif
		auto name = QFileInfo(fileName).fileName();
		if (sendSre) {
			auto errorCodeStr = std::to_string(errorCode);
			PLS_LOGEX(PLS_LOG_ERROR, pls_resource_const::MAIN_FRONTEND_API, {{"CopyErrorCode", errorCodeStr.c_str()}, {"FileName", qUtf8Printable(name)}},
				  "Failed to copy file '%s'. ErrorCode: %lu", qUtf8Printable(name), errorCode);
		} else {
			PLS_WARN(pls_resource_const::MAIN_FRONTEND_API, "Failed to copy file '%s'. ErrorCode: %lu", qUtf8Printable(name), errorCode);
		}
		return false;
	}
	return true;
}

static void onGetLicenseSuccessed(const QByteArray &data)
{
	PLS_INFO(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getLicense: Get license file successfully");
	auto savePath = PLSResCommonFuns::getAppLocationPath().append(BEAUTY_PATH) + SENSETIME_ZIP_NAME;
	bool result = pls_save_file(savePath, data);
	if (!result) {
		PLS_ERROR(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getLicense: Failed to save license.");
		return;
	}

	PLSLicenseManager::HandleLicenseZipFile();
}

void PLSLicenseManager::getLicense(const QString &url)
{
	if (url.isEmpty()) {
		PLS_WARN(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getLicense: license url is empty, download license abort.");
		return;
	}

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get) //
				   .hmacUrl(url, "")               //
				   .withLog()                      //
				   .okResult([](const pls::http::Reply &reply) { onGetLicenseSuccessed(reply.data()); })
				   .failResult([](const pls::http::Reply &reply) {
					   PLS_WARN(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getLibrary: Failed to get library.json, errorCode: %d", static_cast<int>(reply.error()));
				   }));
}

bool PLSLicenseManager::HandleLicenseZipFile()
{
	auto beautyPath = PLSResCommonFuns::getAppLocationPath() + BEAUTY_PATH;
	auto savePath = beautyPath + SENSETIME_ZIP_NAME;
	bool result = PLSResCommonFuns::unZip(beautyPath, savePath, SENSETIME_ZIP_NAME, true);
	if (!result) {
		PLS_ERROR(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getLicense: Failed to unzip license file.");
		return false;
	}
	auto fileSrc = PLSResCommonFuns::getAppLocationPath() + SENSETIME_NEW_VERSION_FILE_PATH + SENSETIME_NEW_VERSION_FILE_NAME;
	auto desDir = beautyPath + SENSETIME_NEW_VERSION_FILE_NAME;

	result = pls_copy_file(fileSrc, desDir, true, true);
	if (!result) {
		PLS_ERROR(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getLicense: Failed to copy license file.");
		return false;
	}

	// copy successfully, remove the temporary folder.
	QDir dir(PLSResCommonFuns::getAppLocationPath() + SENSETIME_FILE_PATH);
	result = dir.removeRecursively();
	if (!result) {
		PLS_WARN(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getLibrary: Failed to remove folder: library_SenseTime_PC.");
		return false;
	}

	PLS_INFO(LICENSE_MGR_MODULE_NAME, "[TRACE-LICENSE] getLicense: Download and copy license successfully.");
	return true;
}

bool PLSLicenseManager::CopyLicenseZip(const QString &from)
{
	return pls_copy_file(from, PLSResCommonFuns::getAppLocationPath() + BEAUTY_PATH + SENSETIME_ZIP_NAME);
}
