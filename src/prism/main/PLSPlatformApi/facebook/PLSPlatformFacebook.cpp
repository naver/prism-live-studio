#include "PLSPlatformFacebook.h"

PLSPlatformFacebook::PLSPlatformFacebook() {}

PLSServiceType PLSPlatformFacebook::getServiceType() const
{
	return PLSServiceType::ST_FACEBOOK;
}
