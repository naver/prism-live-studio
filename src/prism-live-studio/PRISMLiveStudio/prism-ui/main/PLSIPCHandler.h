#pragma once

#include "qjsonobject.h"
#include <functional>
#include <qhash.h>
#include "libipc.h"
#include <qobject.h>
#include <qpointer.h>

enum class MessageType { NoneMsg, ChannelsModuleMsg, MainViewMsg, GetScheduleList, SelectChanel, AppState, ErrorInfomation, MainViewShowned, MainViewReload, StartupProgress };

template<typename IPCType, typename ParaType, typename ListenType> class PLSIPCHandler : public QObject {

	using CreateFun = IPCType (*)(const ParaType &);

public:
	IPCType initIPC(const CreateFun &fun, const ParaType &params)
	{
		auto callback = [this](IPCType subproc, pls::ipc::Event event, const QVariantHash &subParam) { this->onEventHappened(subproc, event, subParam); };
		params.listen(this, callback);
		return fun(params);
	}

	static PLSIPCHandler *instance()
	{
		static PLSIPCHandler *ins = nullptr;
		if (ins == nullptr) {
			ins = new PLSIPCHandler();
		}
		return ins;
	}
	using EventHandler = ListenType;
	using EventHashMap = QMultiHash<int, EventHandler>;

	void subscribe(int event, const EventHandler &fun) { mEventMap.insert(event, fun); };

	void onEventHappened(IPCType subproc, pls::ipc::Event event, const QVariantHash &params)
	{
		auto funcs = mEventMap.values(int(event));
		for (auto &fun : funcs) {
			fun(subproc, event, params);
		}
	}

	IPCType *getMyIPC() { return mIPC; };
	void setMyIPC(IPCType *IPC) { mIPC = IPC; };

private:
	EventHashMap mEventMap;
	IPCType *mIPC = nullptr;
};

using IPCTypeD = pls::ipc::subproc::Subproc;
using IPCParaTypeD = pls::ipc::subproc::Params;
using IPCListenTypeD = pls::ipc::subproc::Listener;
void readMsgFun(IPCTypeD subproc, pls::ipc::Event event, const QVariantHash &params);
using ClientIPCHandler = PLSIPCHandler<IPCTypeD, IPCParaTypeD, IPCListenTypeD>;
#define PLS_IPC_HANDLER ClientIPCHandler::instance()

using IPCTypeP = pls::ipc::mainproc::Subproc;
using IPCParaTypeP = pls::ipc::mainproc::Params;
using IPCListenTypeP = pls::ipc::mainproc::Listener;
void readMsgFunP(IPCTypeP subproc, pls::ipc::Event event, const QVariantHash &params);
using MainIPCHandler = PLSIPCHandler<IPCTypeP, IPCParaTypeP, IPCListenTypeP>;
#define PLS_LAUNCHEER_IPC_HANDLER MainIPCHandler::instance()
