//
//  DNSSDRegistrationObject-C-Interface.hpp
//  TestC++2
//
//  Created by Linda on 2023/1/10.
//  Copyright © 2023 Linda. All rights reserved.
//

#pragma once

#if __APPLE__

#include <string>
#include <map>

typedef std::function<void(bool)> CallbackFunction;

class DNSSDRegistrationImpl {
public:
	DNSSDRegistrationImpl();
	~DNSSDRegistrationImpl();

	void init(const std::string &domain, const std::string &type, const std::string &name, int port, const std::map<std::string, std::string> &txtRecords);
	void registerService(CallbackFunction cb);
	void unregisterService();

private:
	void *self;
	CallbackFunction registerCB;
};

#endif
