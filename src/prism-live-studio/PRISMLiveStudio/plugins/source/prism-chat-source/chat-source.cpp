/******************************************************************************
//PRISM/Zhangdewen/20200901/#for chat source
 ******************************************************************************/

#include "chat-source.hpp"
#include <util/threading.h>
#include <QApplication>
#include <util/dstr.h>
#include <functional>
#include <thread>
#include <mutex>
#include "obs-frontend-api.h"
#include <qjsondocument.h>
#include <QThread>
#include <libutils-api.h>
#include "log.h"
#include <pls/pls-obs-api.h>
#include <pls/pls-properties.h>
#include <pls/pls-source.h>
#include <libui.h>
#include "pls-net-url.hpp"

#ifdef USE_QT_LOOP
#include <QEventLoop>
#include <QThread>
#endif

using namespace std;

#define S_CHAT_SOURCE_TEMPLATE "ChatSource.Template"
#define S_CHAT_SOURCE_TEMPLATE_DESC "ChatSource.Template.Desc"
#define S_CHAT_SOURCE_FONT_SIZE "ChatSource.FontSize"
#define S_URL "url"
#define S_WIDTH "width"
#define S_HEIGHT "height"
#define CHAT_WIDTH 488
#define CHAT_HEIGHT 784

static const char *get_language(char *lang)
{
	const char *locale = obs_get_locale();
	if (!locale || !locale[0]) {
		return strcpy(lang, "en");
	}

	const char *pos = strchr(locale, '-');
	if (pos) {
		return strncpy(lang, locale, pos - locale);
	} else {
		return strcpy(lang, "en");
	}
}

static void process_url_language(QString &url)
{
	char lang[20] = {0};
	url += "?lang=";
	url += get_language(lang);
}

static obs_properties_t *chat_source_get_properties(void *data)
{
	obs_properties_t *properties = obs_properties_create();
	auto chat_template_list = pls_properties_chat_add_template_list(properties, S_CHAT_SOURCE_TEMPLATE, obs_module_text(S_CHAT_SOURCE_TEMPLATE));
	obs_property_set_long_description(chat_template_list, obs_module_text(S_CHAT_SOURCE_TEMPLATE_DESC));

	pls_properties_chat_add_font_size(properties, S_CHAT_SOURCE_FONT_SIZE, obs_module_text(S_CHAT_SOURCE_FONT_SIZE), 12, 60, 1);

	return properties;
}
static void chat_source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, S_CHAT_SOURCE_TEMPLATE, 1); // 1~4
	obs_data_set_default_int(settings, S_CHAT_SOURCE_FONT_SIZE, 20);

	obs_data_set_default_bool(settings, "reroute_audio", false);
}
static void source_notified(void *data, calldata_t *calldata)
{
	pls_used(calldata);
	auto context = static_cast<struct chat_source *>(data);
	auto source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (!source || (source != context->m_browser) || !data)
		return;

	auto type = (int)calldata_int(calldata, "message");
	switch (type) {
	case OBS_SOURCE_BROWSER_LOADED:
		//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
		context->sendNotifyAsync(OBS_SOURCE_CHAT_UPDATE_PARAMS, OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_LOADED);
		break;
	default:
		break;
	}
}
static void init_browser_source(struct chat_source *context)
{
	if (!context || context->m_browser) {
		return;
	}

	// init browser source

	QString url = CHAT_SOURCE_URL;
	process_url_language(url);

	obs_data_t *browser_settings = obs_data_create();
	obs_data_set_string(browser_settings, "url", url.toUtf8().constData());
	obs_data_set_bool(browser_settings, "is_local_file", false);
	obs_data_set_bool(browser_settings, "reroute_audio", true);
	obs_data_set_bool(browser_settings, "ignore_reload", true);
	obs_data_set_int(browser_settings, S_WIDTH, CHAT_WIDTH);
	obs_data_set_int(browser_settings, S_HEIGHT, CHAT_HEIGHT);
	context->m_browser = obs_source_create_private("browser_source", "prism_chat_browser_source", browser_settings);
	obs_data_release(browser_settings);

	signal_handler_connect_ref(obs_get_signal_handler(), "source_notify", source_notified, context);

	obs_source_inc_active(context->m_browser);
	obs_source_inc_showing(context->m_browser);
}
static void source_created(void *data, calldata_t *calldata)
{
	auto context = static_cast<struct chat_source *>(data);
	auto source = (uint64_t)calldata_int(calldata, "source_address");
	if (!source || (source != (uint64_t)context->m_source) || !data)
		return;

	init_browser_source(context);
}
static void *chat_source_create(obs_data_t *settings, obs_source_t *source)
{
	//obs_source_set_capture_valid(source, true, OBS_SOURCE_ERROR_OK);

	auto context = pls_new_nothrow<chat_source>();
	if (!context) {
		PLS_PLUGIN_ERROR("viewer count source create failed, because out of memory.");
		//obs_source_set_capture_valid(source, false, OBS_SOURCE_ERROR_UNKNOWN);
		return nullptr;
	}
	signal_handler_connect_ref(obs_get_signal_handler(), "source_create_finished", source_created, context);
	signal_handler_add(obs_source_get_signal_handler(source), "void source_notify(ptr source, int message, int sub_code)");
	context->m_source = source;
	context->update(settings);
	return context;
}
static void chat_source_destroy(void *data)
{
	auto context = (chat_source *)(data);
	signal_handler_disconnect(obs_get_signal_handler(), "source_create_finished", source_created, context);

	if (context->m_browser) {
		signal_handler_disconnect(obs_get_signal_handler(), "source_notify", source_notified, context);
		obs_source_dec_active(context->m_browser);
		obs_source_release(context->m_browser);
	}

	if (context->m_source_texture) {
		obs_enter_graphics();
		gs_texture_destroy(context->m_source_texture);
		context->m_source_texture = nullptr;
		obs_leave_graphics();
	}

	pls_delete(context);
}
static void chat_source_activate(void *data)
{
	if (auto context = (chat_source *)(data); context->m_browser) {
		obs_source_inc_active(context->m_browser);
	}
}
static void chat_source_deactivate(void *data)
{
	if (auto context = (chat_source *)(data); context->m_browser) {
		obs_source_dec_active(context->m_browser);
	}
}
static void chat_source_clear_texture(gs_texture_t *tex)
{
	if (!tex) {
		return;
	}
	obs_enter_graphics();
	gs_texture_t *pre_rt = gs_get_render_target();
	gs_projection_push();
	gs_set_render_target(tex, nullptr);
	struct vec4 clear_color = {0};
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);
	gs_set_render_target(pre_rt, nullptr);
	gs_projection_pop();
	obs_leave_graphics();
}
static void chat_source_video_render(void *data, gs_effect_t *effect)
{
	auto context = (chat_source *)(data);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), context->m_source_texture);
	gs_draw_sprite(context->m_source_texture, 0, 0, 0);
	pls_used(effect);
}
static void chat_source_render(void *data, obs_source_t *source)
{
	auto vc_source = (chat_source *)(data);
	uint32_t source_width = obs_source_get_width(source);
	uint32_t source_height = obs_source_get_height(source);
	if (source_width <= 0 || source_height <= 0) {
		chat_source_clear_texture(vc_source->m_source_texture);
		return;
	}
	obs_enter_graphics();
	if (vc_source->m_source_texture) {
		uint32_t tex_width = gs_texture_get_width(vc_source->m_source_texture);
		uint32_t tex_height = gs_texture_get_height(vc_source->m_source_texture);
		if (tex_width != source_width || tex_height != source_height) {
			gs_texture_destroy(vc_source->m_source_texture);
			vc_source->m_source_texture = nullptr;
		}
	}

	if (!vc_source->m_source_texture) {
		vc_source->m_source_texture = gs_texture_create(source_width, source_height, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
	}

	gs_texture_t *pre_rt = gs_get_render_target();
	gs_viewport_push();
	gs_projection_push();
	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);

	gs_set_render_target(vc_source->m_source_texture, nullptr);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);
	gs_set_viewport(0, 0, source_width, source_height);

	gs_ortho(0.0f, float(source_width), 0.0f, float(source_height), -100.0f, 100.0f);
	obs_source_video_render(source);

	gs_set_render_target(pre_rt, nullptr);
	gs_viewport_pop();
	gs_projection_pop();

	obs_leave_graphics();
}

static void chat_source_video_tick(void *data, float /*seconds*/)
{
	if (auto context = (chat_source *)(data); context->m_browser) {
		chat_source_render(data, context->m_browser);
	}
}
static void chat_cef_dispatch_js(void *data, const char *event_name, const char *json_data)
{
	if (auto context = (chat_source *)(data); context->m_browser) {
		pls_source_dispatch_cef_js(context->m_browser, event_name, json_data);
	}
}
static uint32_t chat_width(void *data)
{
	return CHAT_WIDTH;
}
static uint32_t chat_height(void *data)
{
	return CHAT_HEIGHT;
}
void register_prism_chat_source()
{
	struct obs_source_info info = {0};
	info.id = "prism_chat_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_properties = chat_source_get_properties;
	info.get_defaults = chat_source_get_defaults;
	info.activate = chat_source_activate;
	info.deactivate = chat_source_deactivate;

	info.get_name = [](void *) { return obs_module_text("ChatSource"); };
	info.create = chat_source_create;
	info.destroy = chat_source_destroy;
	info.update = [](void *data, obs_data_t *settings) { static_cast<chat_source *>(data)->update(settings); };
	info.get_width = chat_width;
	info.get_height = chat_height;
	info.video_tick = chat_source_video_tick;
	info.video_render = chat_source_video_render;
	info.icon_type = static_cast<obs_icon_type>(PLS_ICON_TYPE_CHAT);

	pls_source_info psi = {0};
	psi.properties_edit_start = [](void *data, obs_data_t *settings) { static_cast<chat_source *>(data)->propertiesEditStart(settings); };
	psi.properties_edit_end = [](void *data, obs_data_t *settings, bool) { static_cast<chat_source *>(data)->propertiesEditEnd(settings); };
	//PRISM/Zhangdewen/20211015/#/Chat Source Event
	psi.update_extern_params = [](void *data, const calldata_t *extern_params) {
		//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
		static_cast<chat_source *>(data)->updateExternParamsAsync(extern_params);
	};

	register_pls_source_info(&info, &psi);
	obs_register_source(&info);

	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	if (!chat_source::asyncThread) {
		chat_source::asyncThread = new QThread();
		chat_source::asyncThread->start();
	}
}

void release_prism_chat_source()
{
	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	if (chat_source::asyncThread) {
		QThread *asyncThread = chat_source::asyncThread;
		chat_source::asyncThread = nullptr;

		asyncThread->quit();
		asyncThread->wait();
		delete asyncThread;
	}
}

//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
ChatSourceAsynInvoke::ChatSourceAsynInvoke(chat_source *chatSource_) : chatSource(chatSource_)
{
	moveToThread(chat_source::asyncThread);
}

//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
ChatSourceAsynInvoke::~ChatSourceAsynInvoke() {}

void ChatSourceAsynInvoke::setChatSource(chat_source *chatSource)
{
	QWriteLocker locker(&chatSourceLock);
	this->chatSource = chatSource;
}

//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
void ChatSourceAsynInvoke::sendNotify(int type, int sub_code)
{
	QReadLocker locker(&chatSourceLock);
	if (chatSource) {
		chatSource->sendNotify(type, sub_code);
	}
}

//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
void ChatSourceAsynInvoke::updateExternParams(const QByteArray &cjson, int sub_code)
{
	QReadLocker locker(&chatSourceLock);
	if (chatSource) {
		chatSource->updateExternParams(cjson, sub_code);
	}
}

//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
QThread *chat_source::asyncThread = nullptr;

chat_source::chat_source()
{
	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	asynInvoke = pls_new_nothrow<ChatSourceAsynInvoke>(this);
}

//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
chat_source::~chat_source()
{
	asynInvoke->setChatSource(nullptr);
	asynInvoke->deleteLater();
}

void chat_source::update(obs_data_t *settings)
{
	style = (int)obs_data_get_int(settings, S_CHAT_SOURCE_TEMPLATE);
	fontSize = (int)obs_data_get_int(settings, S_CHAT_SOURCE_FONT_SIZE);

	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	sendNotifyAsync(OBS_SOURCE_CHAT_UPDATE_PARAMS, OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_UPDATE);
}

void chat_source::propertiesEditStart(obs_data_t *settings)
{
	//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
	sendNotifyAsync(OBS_SOURCE_CHAT_UPDATE_PARAMS, OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_EDIT_START);
}

void chat_source::propertiesEditEnd(obs_data_t *settings) {}

void chat_source::dispatchJSEvent(const QByteArray &json)
{
	PLS_PLUGIN_INFO("chatEvent: %s", json.constData());

	//send event to Web
	if (m_browser) {
		pls_source_dispatch_cef_js(m_browser, "chatEvent", json.constData());
	}
}

//PRISM/Zhangdewen/20211015/#/Chat Source Event
QByteArray chat_source::toJson(const char *cjson) const
{
	QJsonParseError error;
	QJsonObject setting = QJsonDocument::fromJson(QByteArray(cjson), &error).object();
	if (error.error != QJsonParseError::NoError) {
		return QByteArray();
	}

	QJsonObject data = setting.value("data").toObject();
	data.insert("fontSize", QString::number(fontSize));
	data.insert("styleVersion", QString::number(style));
	setting.insert("data", data);

	QByteArray json = QJsonDocument(setting).toJson(QJsonDocument::Compact);
	return json;
}

//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
void chat_source::sendNotifyAsync(int type, int subCode)
{
	QMetaObject::invokeMethod(asynInvoke, "sendNotify", Qt::QueuedConnection, Q_ARG(int, type), Q_ARG(int, subCode));
}

//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
void chat_source::updateExternParamsAsync(const calldata_t *extern_params)
{
	const char *cjson = calldata_string(extern_params, "cjson");
	auto sub_code = calldata_int(extern_params, "sub_code");
	QMetaObject::invokeMethod(asynInvoke, "updateExternParams", Qt::QueuedConnection, Q_ARG(QByteArray, QByteArray(cjson)), Q_ARG(int, sub_code));
}

//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
void chat_source::sendNotify(int type, int sub_code)
{
	pls_source_send_notify(m_source, static_cast<obs_source_event_type>(type), sub_code);
}

//PRISM/Zhangdewen/20211028/#10168/Async notify (fix deadlock)
void chat_source::updateExternParams(const QByteArray &cjson, int sub_code)
{
	switch (sub_code) {
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_UPDATE:
		dispatchJSEvent(toJson(cjson));
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_EDIT_START:
		dispatchJSEvent(toJson(cjson));
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_LOADED:
		dispatchJSEvent(toJson(cjson));
		sendNotifyAsync(OBS_SOURCE_CHAT_UPDATE_PARAMS, OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_CHECK_LIVE);
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_CHECK_LIVE:
		dispatchJSEvent(toJson(cjson));
		break;
	}
}
