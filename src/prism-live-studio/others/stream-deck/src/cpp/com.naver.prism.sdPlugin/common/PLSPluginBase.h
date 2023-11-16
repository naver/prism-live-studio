#pragma once
#include "ESDBasePlugin.h"

class PLSPluginBase : public ESDBasePlugin {
public:
	PLSPluginBase() : ESDBasePlugin() {}
	virtual ~PLSPluginBase() {}

	virtual void UpdateSceneCollectionList(const json &payload) = 0;
	virtual void SendToPropertyInspector(const json &payload) = 0;

	virtual void PrismConnected() = 0;
	virtual void PrismDisconnected() = 0;
	virtual void DidReceiveFromPrism(const json &payload) = 0;

	virtual void LogMessage(const std::string &message) = 0;
	virtual void StreamDeckDisconnected() = 0;
	virtual void LogMessage(const char *format, ...) = 0;
};
