#ifndef PLSRESENUMS_H
#define PLSRESENUMS_H
#include "PLSResourceHandle_global.h"
#include <qmetatype.h>

enum class PLSResEvents {
	RES_DOWNLOAD_SUCCESS = 1,
	RES_DOWNLOAD_FAILED = 2,
	RES_CACHE_NOT_EXIST = 3,
	RES_NEW_VERSION = 4,
};

Q_DECLARE_METATYPE(PLSResEvents)
enum class PLSResDownloadStatus { None, Downloading = 1, DownloadSuccess, DownloadFailed };

namespace pls_res_const {
constexpr auto resourceTime = "resource_time";
constexpr auto resourceSize = "resource_size";
constexpr auto resourceName = "resource_name";
constexpr auto resourceWebName = "resource_web_name";
constexpr auto resourcePath = "resource_path";
constexpr auto resourceUrl = "resource_url";
constexpr auto resource_sub_files = "resource_sub_files";
constexpr auto resourceType = "type";
constexpr auto itemId = "item_id";
constexpr auto isFromCompressFile = "is_from_compress_file";
constexpr auto resourceCategoryId = "categoryId";
constexpr auto resourceModuleVersion = "version";
constexpr auto resourceModuleUrl = "resourceUrl";
constexpr auto categoryName = "category.json";
constexpr auto moduleDir = "%1/resources/%2";
constexpr auto resourceCaheDataName = "cache_%1.json";
constexpr auto appLocale = "locale";
constexpr auto AppSupportLocale = "localeList";
constexpr auto gcc = "gcc";
constexpr auto logModule = "ResDownload";
constexpr auto resourceDir = "%1/resources";
constexpr auto resTmpDir = "%1/temp";
constexpr auto resourceTmpFilePath = "%1/%2";
constexpr auto resCacheDir = "%1/res_cache";
constexpr auto resTemplateDir = "%1/res_template";
constexpr auto resBannerJson = "banner_resources.json";
}

#endif // PLSRESENUMS_H
