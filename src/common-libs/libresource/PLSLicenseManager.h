#pragma once
#include "PLSResourceHandle_global.h"
#include <QString>

struct PLSRESOURCEHANDLE_EXPORT PLSLicenseManager {

	/**
	 * downlaod newest sensetime resource, do not judge version, just request newest license file on server.
	 * defualt to download synchronous.
	 */
	static void getNewestLicense();

	static void getLibrary(const QString &libraryUrl);
	static void getLicense(const QString &licensUrl);
	static bool HandleLicenseZipFile();
	static bool CopyLicenseZip(const QString &from);
};
