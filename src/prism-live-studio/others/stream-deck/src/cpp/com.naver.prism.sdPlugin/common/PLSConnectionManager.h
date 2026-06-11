#pragma once

#include "PLSPluginBase.h"
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::config::asio_client::message_type::ptr message_ptr;
typedef websocketpp::server<websocketpp::config::asio> Server;
typedef websocketpp::client<websocketpp::config::asio_client> WebsocketClient;

class PLSConnectionManager {

public:
	PLSConnectionManager(PLSPluginBase *mPlugin);
	virtual ~PLSConnectionManager();

public:
	void Run(websocketpp::lib::asio::io_service *ios);
	bool PrismDidConnect();
	void GetSecneCollection();
	void SendToPrism(const json &payload);
	bool Disconnect();

private:
	bool initSocketClient(bool firstTime = true);
	void OnMessage(websocketpp::connection_hdl, WebsocketClient::message_ptr inMsg);
	void OnOpen(websocketpp::connection_hdl);
	void OnClose(websocketpp::connection_hdl);
	void OnFail(websocketpp::connection_hdl);

private:
	WebsocketClient mClient;
	WebsocketClient::connection_ptr mConnection;
	PLSPluginBase *mPlugin = nullptr;
	std::atomic<bool> isConnected;
	bool needRetry = true;
};
