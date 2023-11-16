//
//  DNSSDRegisterManager.hpp
//  DNSDemo
//
//  Created by Zhong Ling on 2023/2/6.
//

#if __APPLE__

#ifndef DNSSDRegisterManager_hpp
#define DNSSDRegisterManager_hpp

#include <map>
#include <mutex>

#include "../frontend-api.h"

class DNSSDRegistrationCPPClass;

class DNSSDRegisterManager {

public:
	static DNSSDRegisterManager *getInstance();
	DNSSDRegisterManager();
	~DNSSDRegisterManager();

	DNSSDRegisterManager(DNSSDRegisterManager const &) = delete;
	void operator=(DNSSDRegisterManager const &) = delete;

	void registerService(const std::string &uuid, unsigned short wPort, const ServiceRecord &record, void *context, void (*cb)(void *context, bool success));
	void unregisterService(const std::string &uuid);

private:
	static DNSSDRegisterManager *_instance;
	std::map<std::string, DNSSDRegistrationCPPClass *> registerServices;
	std::mutex mutex;
	void *_context;
};

#endif /* DNSSDRegisterManager_hpp */

#endif
