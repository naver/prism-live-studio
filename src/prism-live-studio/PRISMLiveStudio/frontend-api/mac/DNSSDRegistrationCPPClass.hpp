//
//  DNSSDRegistrationCPPClass.hpp
//  TestC++2
//
//  Created by Linda on 2023/1/10.
//  Copyright © 2023 Linda. All rights reserved.
//

#pragma once

#if __APPLE__

#include <string>
#include <map>

#include "DNSSDRegistrationObject-C-Interface.hpp"

class DNSSDRegistrationImpl;

class DNSSDRegistrationCPPClass {
public:
	DNSSDRegistrationCPPClass();
	~DNSSDRegistrationCPPClass();

	void init(const std::string &domain, const std::string &type, const std::string &name, const std::map<std::string, std::string> &txtRecords, int port);
	void registerService(CallbackFunction cb);
	void unregisterService();

private:
	DNSSDRegistrationImpl *impl;
};

#endif
