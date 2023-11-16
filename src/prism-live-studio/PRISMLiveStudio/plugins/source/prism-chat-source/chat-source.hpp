/******************************************************************************
//PRISM/Zhangdewen/20200901/#for chat source
 ******************************************************************************/

#pragma once

#include <obs-module.h>
#include <obs.hpp>

#include <unordered_map>
#include <functional>
#include <vector>
#include <string>
#include <mutex>
#include <qstring.h>
#include <qjsonobject.h>
#include <qobject.h>
#include <qreadwritelock.h>

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
extern bool hwaccel;
#endif

struct chat_source;

//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
class ChatSourceAsynInvoke : public QObject {
	Q_OBJECT

public:
	ChatSourceAsynInvoke(chat_source *chatSource);
	~ChatSourceAsynInvoke();

public:
	void setChatSource(chat_source *chatSource);

private slots:
	void sendNotify(int type, int sub_code);
	void updateExternParams(const QByteArray &cjson, int sub_code);

public:
	QReadWriteLock chatSourceLock{QReadWriteLock::Recursive};
	chat_source *chatSource = nullptr;
};

struct chat_source {
	int style = 1;
	int fontSize = 20;

	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	ChatSourceAsynInvoke *asynInvoke = nullptr;

	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	static QThread *asyncThread;
	obs_source_t *m_source = nullptr;
	obs_source_t *m_browser = nullptr;
	gs_texture_t *m_source_texture = nullptr;

	chat_source();
	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	~chat_source();

	void update(obs_data_t *settings);
	void propertiesEditStart(obs_data_t *settings);
	void propertiesEditEnd(obs_data_t *settings);
	void dispatchJSEvent(const QByteArray &json);
	//PRISM/Zhangdewen/20211015/#/Chat Source Event
	QByteArray toJson(const char *cjson) const;

	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	void sendNotifyAsync(int type, int sub_code);
	void updateExternParamsAsync(const calldata_t *extern_params);
	void sendNotify(int type, int sub_code);
	void updateExternParams(const QByteArray &cjson, int sub_code);
};
