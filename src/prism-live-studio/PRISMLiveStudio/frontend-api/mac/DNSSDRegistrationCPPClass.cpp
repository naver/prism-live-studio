//
//  DNSSDRegistrationCPPClass.cpp
//  TestC++2
//
//  Created by Linda on 2023/1/10.
//  Copyright © 2023 Linda. All rights reserved.
//

#if __APPLE__

#include "DNSSDRegistrationCPPClass.hpp"

static std::map<std::string, DNSSDRegistrationCPPClass *> registeredDNSServices;

DNSSDRegistrationCPPClass::DNSSDRegistrationCPPClass() : impl(nullptr) {}

void DNSSDRegistrationCPPClass::init(const std::string &domain, const std::string &type, const std::string &name, const std::map<std::string, std::string> &txtRecords, int port)
{
	impl = new DNSSDRegistrationImpl();
	impl->init(domain, type, name, port, txtRecords);
}

DNSSDRegistrationCPPClass::~DNSSDRegistrationCPPClass()
{
	if (impl) {
		delete impl;
		impl = NULL;
	}
}

void DNSSDRegistrationCPPClass::registerService(CallbackFunction cb)
{
	impl->registerService(cb);
}

void DNSSDRegistrationCPPClass::unregisterService()
{
	impl->unregisterService();
}

#endif
