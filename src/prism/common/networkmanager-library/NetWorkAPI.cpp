#include "NetWorkAPI.h"
#include "NetworkAccessManager.h"

void deleteAPI(NetWorkAPI *API)
{
	if (API != nullptr) {
		auto tmpointer = dynamic_cast<NetworkAccessManager *>(API);
		//to be solved on delete
		tmpointer->deleteLater();
	}
}

NetWorkAPI *createNetWorkAPI()
{
	return dynamic_cast<NetWorkAPI *>(new NetworkAccessManager());
}
