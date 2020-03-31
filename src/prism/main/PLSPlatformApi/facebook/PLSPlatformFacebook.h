/*
* @file		PLSPlatformFacebook.h
* @brief	All facebook relevant api is implemented in this file
* @author	wu.longyue@navercorp.com
* @date		2020-01-06
*/

#pragma once

#include "..\PLSPlatformBase.hpp"

using namespace std;

class PLSPlatformFacebook : public PLSPlatformBase {
public:
	PLSPlatformFacebook();

	PLSServiceType getServiceType() const override;

private:
};
