//
//  DNSSDRegisterManager.cpp
//  DNSDemo
//
//  Created by Zhong Ling on 2023/2/6.
//

#if __APPLE__
#include "DNSSDRegisterManager.hpp"

#include "DNSSDRegistrationCPPClass.hpp"

#include "../frontend-api.h"

DNSSDRegisterManager *DNSSDRegisterManager::_instance = nullptr;

DNSSDRegisterManager *DNSSDRegisterManager::getInstance()
{
	if (_instance)
		return _instance;

	_instance = new DNSSDRegisterManager();
	return _instance;
}

DNSSDRegisterManager::DNSSDRegisterManager()
{
	_context = nullptr;
}

DNSSDRegisterManager::~DNSSDRegisterManager()
{
	std::unique_lock<std::mutex> lock(mutex);
	for (auto iter = registerServices.begin(); iter != registerServices.end(); ++iter) {
		iter->second->unregisterService();
		delete iter->second;
	}
	lock.unlock();
}

void DNSSDRegisterManager::registerService(const std::string &uuid, unsigned short wPort, const ServiceRecord &record, void *context, void (*cb)(void *context, bool success))
{
	this->_context = context;

	std::unique_lock<std::mutex> lock(mutex);
	auto it_found = registerServices.find(uuid);
	if (it_found != registerServices.end()) {
		lock.unlock();
		return;
	}

	std::map<std::string, std::string> txtRecords;
	// todo: std::string => pls_utf8_to_unicode, then to_wstring
	txtRecords["id"] = std::string(record.id);
	txtRecords["name"] = std::string(record.name);
	txtRecords["deviceType"] = std::string(record.deviceType);
	txtRecords["version"] = std::to_string(record.version);
    txtRecords["connectType"] = std::string(record.connectType);
	DNSSDRegistrationCPPClass *dnssd = new DNSSDRegistrationCPPClass();

	dnssd->init("local", "_prismconnect._tcp.", uuid, txtRecords, wPort);

	dnssd->registerService([=](bool success) {
		if (!success) {
			std::unique_lock<std::mutex> lock(mutex);
			this->registerServices.erase(uuid);
			free(dnssd);
			lock.unlock();
		}
		if (cb)
			cb(this->_context, success);
	});

	registerServices[uuid] = dnssd;

	lock.unlock();
}

void DNSSDRegisterManager::unregisterService(const std::string &uuid)
{
	std::unique_lock<std::mutex> lock(mutex);

	auto it_found = registerServices.find(uuid);
	if (it_found != registerServices.end()) {
		it_found->second->unregisterService();
		registerServices.erase(it_found);
	}

	lock.unlock();
}
#endif
