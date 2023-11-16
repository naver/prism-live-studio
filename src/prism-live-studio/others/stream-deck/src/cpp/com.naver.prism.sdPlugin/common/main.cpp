//==============================================================================
/**
@file       main.cpp

@brief		Parse arguments and start the plugin

@copyright  (c) 2018, Corsair Memory, Inc.
			This source code is licensed under the MIT-style license found in the LICENSE file.

**/
//==============================================================================

#if defined(_WIN32)
#include "../windows/pch.h"
#elif defined(__APPLE__)
#include "../mac/pch.h"
#endif
#include "ESDConnectionManager.h"
#include "PLSStreamDeckPlugin.h"
#include "ESDLocalizer.h"
#include "EPLJSONUtils.h"
#include "PLSConnectionManager.h"

#define LOCAL_DEBUG 0

using Websocket = websocketpp::lib::asio::io_service;
Websocket *app = nullptr;

int main(int argc, const char *const argv[])
{
    int result = 0;
    do{
    #if !LOCAL_DEBUG
        if (argc != 9) {
            DebugPrint("Invalid number of parameters %d instead of 9\n", argc);
            result = 1;
            break;
        }
    #endif

        int port = 0;
        std::string pluginUUID;
        std::string registerEvent;
        std::string info;

#if !LOCAL_DEBUG
        for (int argumentIndex = 0; argumentIndex < 4; argumentIndex++) {
            std::string parameter(argv[1 + 2 * argumentIndex]);
            std::string value(argv[1 + 2 * argumentIndex + 1]);

            if (parameter == kESDSDKPortParameter) {
                port = std::atoi(value.c_str());
            } else if (parameter == kESDSDKPluginUUIDParameter) {
                pluginUUID = value;
            } else if (parameter == kESDSDKRegisterEventParameter) {
                registerEvent = value;
            } else if (parameter == kESDSDKInfoParameter) {
                info = value;
            }
        }
#endif

#if LOCAL_DEBUG
        port = 28196;
        pluginUUID = "677915CA352C7D936A2DC16EFEAA3EB5";
        registerEvent = "registerPlugin";
        info = "{\"application\":{\"font\":\".AppleSystemUIFont\",\"language\":\"en\",\"platform\":\"mac\",\"platformVersion\":\"13.3.1\",\"version\":\"6.2.0.18816\"},\"colors\":{\"buttonPressedBackgroundColor\":\"#303030FF\",\"buttonPressedBorderColor\":\"#646464FF\",\"buttonPressedTextColor\":\"#969696FF\",\"disabledColor\":\"#007AFF59\",\"highlightColor\":\"#007AFFFF\",\"mouseDownColor\":\"#005CD7FF\"},\"devicePixelRatio\":2,\"devices\":[{\"id\":\"FA9FE462E13B78351D822198B9ED6F41\",\"name\":\"Stream Deck XL\",\"size\":{\"columns\":8,\"rows\":4},\"type\":2}],\"plugin\":{\"uuid\":\"com.elgato.cpu\",\"version\":\"1.3\"}}";
#endif
        if (port == 0) {
            DebugPrint("Invalid port number\n");
            result = 1;
            break;
        }
        
        if (pluginUUID.empty()) {
            DebugPrint("Invalid plugin UUID\n");
            result = 1;
            break;
        }
        
        if (registerEvent.empty()) {
            DebugPrint("Invalid registerEvent\n");
            result = 1;
            break;
        }
        
        if (info.empty()) {
            DebugPrint("Invalid info\n");
            result = 1;
            break;
        }

        app = new Websocket();
        
        // Create the plugin
        PLSStreamDeckPlugin *plugin = new PLSStreamDeckPlugin();

        // Initialize localization helper
        std::string language = "en";

        try {
            json infoJson = json::parse(info);
            json applicationInfo;
            if (EPLJSONUtils::GetObjectByName(infoJson, kESDSDKApplicationInfo, applicationInfo)) {
                language = EPLJSONUtils::GetStringByName(applicationInfo, kESDSDKApplicationInfoLanguage, language);
            }
        } catch (...) {
        }
        
        ESDLocalizer::Initialize(language);

        // Crteate connetcion to PRISM
        PLSConnectionManager *plsConnectionManager = new PLSConnectionManager(plugin);
        plsConnectionManager->Run(app);
        
        // Create the connection manager
        ESDConnectionManager *connectionManager = new ESDConnectionManager(port, pluginUUID, registerEvent, info, plugin);
        connectionManager->Run(app);

        try{
            app->run();
        }catch(...){
            DebugPrint("asio run failed.\n");
        }

        if (plugin) {
            delete plugin;
        }

        if (plsConnectionManager) {
            delete plsConnectionManager;
        }
        
        if (connectionManager) {
            delete connectionManager;
        }
        
        delete app;
        
    }while(false);
    
	return result;
}
