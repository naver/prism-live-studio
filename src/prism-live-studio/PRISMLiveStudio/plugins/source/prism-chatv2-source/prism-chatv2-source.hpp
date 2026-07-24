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
#define CT_BROWSER_WIDTH 330
#define CT_BROWSER_HEIGHT 610
#define CT_CHAT_WIDTH 360
#define CT_CHAT_HEIGHT 650
#define CT_X_MARGIN 15
#define CT_Y_MARGIN 20

#define CT_BROWSER_WIDTH_ID5 770
#define CT_BROWSER_HEIGHT_ID5 400

struct chat_template_source;

//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
class ChatSourceAsynInvoke : public QObject {
	Q_OBJECT

public:
	ChatSourceAsynInvoke(chat_template_source *chatSource);
	~ChatSourceAsynInvoke();

public:
	void setChatSource(chat_template_source *chatSource);

private slots:
	void sendNotify(int type, int sub_code);
	void updateExternParams(const QByteArray &cjson, int sub_code);

public:
	QReadWriteLock chatSourceLock{QReadWriteLock::Recursive};
	chat_template_source *chatSource = nullptr;
};

struct chat_template_source {
	int style = 1;
	int fontSize = 20;
	int currentTemplateId = -1;
	int currentBKTemplateId = -1;

	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	ChatSourceAsynInvoke *asynInvoke = nullptr;

	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	static QThread *asyncThread;
	obs_source_t *m_source = nullptr;
	obs_source_t *m_browser = nullptr;
	gs_texture_t *m_source_texture = nullptr;
	QMetaObject::Connection m_netConnection;
	int m_browserWidth = CT_BROWSER_WIDTH;
	int m_browserHeight = CT_BROWSER_HEIGHT;
	int m_chatWidth = CT_CHAT_WIDTH;
	int m_chatHeight = CT_CHAT_HEIGHT;
	QStringList m_allPaltformName;
	std::atomic<bool> template5Changed = false;
	std::atomic<int> renderCount = 0;
	QByteArray cacheData;

	chat_template_source();
	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	~chat_template_source();

	void update(obs_data_t *settings);
	void propertiesEditStart(obs_data_t *settings);
	void propertiesEditEnd(obs_data_t *settings);
	void dispatchJSEvent(const QByteArray &json);
	//PRISM/Zhangdewen/20211015/#/Chat Source Event
	QByteArray toJson(const char *cjson, bool isForce);

	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	void sendNotifyAsync(int type, int sub_code);
	void updateExternParamsAsync(const calldata_t *extern_params);
	void sendNotify(int type, int sub_code);
	void updateExternParams(const QByteArray &cjson, int sub_code);

	void networkStateCallbackFunc(bool accessible);
	QJsonObject getData() const;
	QString modifyNCB2BService2Ncp(QString &selectPlatform) const;
};
