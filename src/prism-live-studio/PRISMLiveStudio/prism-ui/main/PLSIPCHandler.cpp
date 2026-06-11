#include "PLSIPCHandler.h"
#include <QtGlobal>
#include <qvariant.h>
#include <qstring.h>
#include <qcoreapplication.h>

template<typename IPCType> void readMsgFunImpl(IPCType, pls::ipc::Event, const QVariantHash &params)
{
	int type = params.value("type").toInt();
	auto msgBody = params.value("msg").toJsonObject();
	qDebug() << " proc "
		 << " params " << type << " body " << msgBody;
	switch (MessageType(type)) {
	case MessageType::ChannelsModuleMsg:
		break;
	case MessageType::MainViewMsg:
		break;

	default:
		break;
	}
}

void readMsgFun(IPCTypeD subproc, pls::ipc::Event event, const QVariantHash &params)
{
	readMsgFunImpl(subproc, event, params);
}

void readMsgFunP(IPCTypeP subproc, pls::ipc::Event event, const QVariantHash &params)
{
	readMsgFunImpl(subproc, event, params);
}
