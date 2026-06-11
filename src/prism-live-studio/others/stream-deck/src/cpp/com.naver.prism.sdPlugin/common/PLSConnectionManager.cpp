#include "PLSConnectionManager.h"
#include "ESDConnectionManager.h"
#include "EPLJSONUtils.h"
#if defined(_WIN32)
#include "../windows/pch.h"
#elif defined(__APPLE__)
#include "../mac/pch.h"
#endif

const std::string uri = "ws://localhost:28120";

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef Server::message_ptr message_ptr;

PLSConnectionManager::PLSConnectionManager(PLSPluginBase *plugin) : mPlugin(plugin)
{
	if (plugin != nullptr)
		plugin->SetPLSConnectionManager(this);
}

PLSConnectionManager::~PLSConnectionManager()
{
	needRetry = false;
	Disconnect();
}

void PLSConnectionManager::Run(websocketpp::lib::asio::io_service *ios)
{
	assert(ios);
	// Set logging to be pretty verbose (everything except message payloads)
	mClient.set_access_channels(websocketpp::log::alevel::all);
	mClient.clear_access_channels(websocketpp::log::alevel::frame_payload);

	// Initialize ASIO
    std::error_code error;
	mClient.init_asio(ios, error);

	// Initialize socket client
	initSocketClient();
}

bool PLSConnectionManager::PrismDidConnect()
{
	return isConnected.load();
}

void PLSConnectionManager::GetSecneCollection()
{
	json payload;
	payload["id"] = RPC_ID_fetchSceneCollectionsSchema;
	websocketpp::lib::error_code ec;
	mClient.send(mConnection, payload.dump(), websocketpp::frame::opcode::text, ec);
}

void PLSConnectionManager::SendToPrism(const json &payload)
{
	websocketpp::lib::error_code ec;
	mClient.send(mConnection, payload.dump(), websocketpp::frame::opcode::text, ec);
}

bool PLSConnectionManager::initSocketClient(bool firstTime)
{
	if (mPlugin)
		mPlugin->LogMessage("%s connecting to prism...", __FUNCTION__);

	try {
		//Register on open handler
		mClient.set_open_handler(websocketpp::lib::bind(&PLSConnectionManager::OnOpen, this, websocketpp::lib::placeholders::_1));

		// Register our message handler
		mClient.set_message_handler(websocketpp::lib::bind(&PLSConnectionManager::OnMessage, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));

		// Register close handler
		mClient.set_close_handler(websocketpp::lib::bind(&PLSConnectionManager::OnClose, this, websocketpp::lib::placeholders::_1));

		// Register fail handler
		mClient.set_fail_handler(websocketpp::lib::bind(&PLSConnectionManager::OnFail, this, websocketpp::lib::placeholders::_1));

		websocketpp::lib::error_code ec;
		mConnection = mClient.get_connection(uri, ec);
		if (ec) {
			DebugPrint("could not create connection because: %s\n", ec.message().c_str());
			if (mPlugin)
				mPlugin->LogMessage("%s could not create connection\n", __FUNCTION__);
			return false;
		}
		mClient.connect(mConnection);
	} catch (websocketpp::exception const &e) {
		DebugPrint("create connection exception: %s\n", e.what());
		return false;
	}
	return true;
}

bool PLSConnectionManager::Disconnect()
{
	try {
		if (mConnection) {
			websocketpp::lib::error_code ec;
			if (mConnection->get_state() == websocketpp::session::state::open) {
				mClient.close(mConnection, websocketpp::close::status::normal, std::string("disconnect"), ec);
				if (ec)
					DebugPrint("> Error on disconnect close: %s\n", ec.message().c_str());
			}
		}
		mClient.set_message_handler([=](...) {});
		mClient.set_close_handler([=](...) {});
		mClient.set_fail_handler([=](...) {});
		isConnected.store(false);
	} catch (const websocketpp::exception &e) {
		DebugPrint("disconnect exception: %s\n", e.what());
		return false;
	}
	return true;
}

void PLSConnectionManager::OnMessage(websocketpp::connection_hdl, WebsocketClient::message_ptr inMsg)
{
	if (inMsg != NULL && inMsg->get_opcode() == websocketpp::frame::opcode::text) {
		std::string message = inMsg->get_payload();
		DebugPrint("OnMessage: %s\n", message.c_str());

		try {
			json payload = json::parse(message);
			if (mPlugin) {
				mPlugin->DidReceiveFromPrism(payload);
			}
		} catch (std::exception e) {
			DebugPrint("%s exception: %s\n", __FUNCTION__, e.what());
			if (mPlugin) {
				mPlugin->LogMessage("%s parse message error", __FUNCTION__);
			}
		}
	}
}

void PLSConnectionManager::OnOpen(websocketpp::connection_hdl)
{
	isConnected.store(true);
	if (mPlugin) {
		mPlugin->LogMessage("%s connect to prism success!", __FUNCTION__);
		mPlugin->PrismConnected();
	}
}

void PLSConnectionManager::OnClose(websocketpp::connection_hdl)
{
	if (mPlugin) {
		mPlugin->LogMessage("%s disconnect from prism", __FUNCTION__);
		mPlugin->PrismDisconnected();
	}
	Disconnect();
	if (needRetry) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		initSocketClient(false);
	}
}

void PLSConnectionManager::OnFail(websocketpp::connection_hdl)
{
	if (mPlugin)
		mPlugin->LogMessage("%s connect to prism faild, try to reconnect", __FUNCTION__);
	Disconnect();

	if (needRetry) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		initSocketClient(false);
	}
}
