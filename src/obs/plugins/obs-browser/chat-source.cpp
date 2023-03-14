/******************************************************************************
//PRISM/Zhangdewen/20200901/#for chat source
 ******************************************************************************/

#include "chat-source.hpp"
#include "browser-client.hpp"
#include "browser-scheme.hpp"
#include "wide-string.hpp"
#include <util/threading.h>
#include <QApplication>
#include <util/dstr.h>
#include <functional>
#include <thread>
#include <mutex>
#include "obs-frontend-api.h"
#include <qjsondocument.h>
#include <QTimer>

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

//PRISM/Zhangdewen/20201104/#5586/for chat source
void (*pls_get_chat_source_url)(QString *) = nullptr;

extern bool QueueCEFTask(std::function<void()> task);
extern "C" void obs_browser_initialize(void);
extern void DispatchJSEvent(BrowserSource *source, std::string eventName,
			    std::string jsonString);

void browser_source_get_defaults(obs_data_t *settings);
obs_properties_t *browser_source_get_properties(void *data);

void SendBrowserVisibility(CefRefPtr<CefBrowser> browser, bool isVisible);
void ExecuteOnAllBrowsers(BrowserFunc func);
void DispatchJSEvent(std::string eventName, std::string jsonString);

static QString get_chat_source_url()
{
	QString CHAT_SOURCE_URL;
	if (pls_get_chat_source_url) {
		pls_get_chat_source_url(&CHAT_SOURCE_URL);
	}
	return CHAT_SOURCE_URL;
}

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
	auto chat_template_list = obs_properties_add_chat_template_list(
		properties, S_CHAT_SOURCE_TEMPLATE,
		obs_module_text(S_CHAT_SOURCE_TEMPLATE));
	obs_property_set_long_description(
		chat_template_list,
		obs_module_text(S_CHAT_SOURCE_TEMPLATE_DESC));

	obs_properties_add_chat_font_size(
		properties, S_CHAT_SOURCE_FONT_SIZE,
		obs_module_text(S_CHAT_SOURCE_FONT_SIZE), 12, 60, 1);

	return properties;
}
static void chat_source_get_defaults(obs_data_t *settings)
{
	browser_source_get_defaults(settings);

	QString url = get_chat_source_url();
	process_url_language(url);
	obs_data_set_default_string(settings, S_URL, url.toUtf8().constData());

	obs_data_set_default_int(settings, S_WIDTH, CHAT_WIDTH);
	obs_data_set_default_int(settings, S_HEIGHT, CHAT_HEIGHT);

	obs_data_set_default_int(settings, S_CHAT_SOURCE_TEMPLATE, 1); // 1~4
	obs_data_set_default_int(settings, S_CHAT_SOURCE_FONT_SIZE, 20);

	obs_data_set_default_bool(settings, "reroute_audio", false);
}

void register_prism_chat_source()
{
	struct obs_source_info info = {};
	info.id = "prism_chat_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			    OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_properties = chat_source_get_properties;
	info.get_defaults = chat_source_get_defaults;

	info.get_name = [](void *) { return obs_module_text("ChatSource"); };
	info.create = [](obs_data_t *settings, obs_source_t *source) -> void * {
		obs_browser_initialize();
		return new ChatSource(settings, source);
	};
	info.destroy = [](void *data) {
		delete static_cast<ChatSource *>(data);
	};
	info.update = [](void *data, obs_data_t *settings) {
		static_cast<ChatSource *>(data)->Update(settings);
	};
	info.get_width = [](void *data) {
		return (uint32_t) static_cast<ChatSource *>(data)->width;
	};
	info.get_height = [](void *data) {
		return (uint32_t) static_cast<ChatSource *>(data)->height;
	};
	info.video_tick = [](void *data, float) {
		static_cast<ChatSource *>(data)->Tick();
	};
	info.video_render = [](void *data, gs_effect_t *) {
		static_cast<ChatSource *>(data)->Render();
	};
#if CHROME_VERSION_BUILD >= 3683
	info.audio_mix = [](void *data, uint64_t *ts_out,
			    struct audio_output_data *audio_output,
			    size_t channels, size_t sample_rate) {
		return static_cast<ChatSource *>(data)->AudioMix(
			ts_out, audio_output, channels, sample_rate);
	};
	info.enum_active_sources = [](void *data, obs_source_enum_proc_t cb,
				      void *param) {
		static_cast<ChatSource *>(data)->EnumAudioStreams(cb, param);
	};
#endif
	/*
	info.mouse_click = [](void *data, const struct obs_mouse_event *event,
			      int32_t type, bool mouse_up,
			      uint32_t click_count) {
		static_cast<ChatSource *>(data)->SendMouseClick(
			event, type, mouse_up, click_count);
	};
	info.mouse_move = [](void *data, const struct obs_mouse_event *event,
			     bool mouse_leave) {
		static_cast<ChatSource *>(data)->SendMouseMove(event,
							       mouse_leave);
	};
	info.mouse_wheel = [](void *data, const struct obs_mouse_event *event,
			      int x_delta, int y_delta) {
		static_cast<ChatSource *>(data)->SendMouseWheel(event, x_delta,
								y_delta);
	};
	info.focus = [](void *data, bool focus) {
		static_cast<ChatSource *>(data)->SendFocus(focus);
	};
	info.key_click = [](void *data, const struct obs_key_event *event,
			    bool key_up) {
		static_cast<ChatSource *>(data)->SendKeyClick(event, key_up);
	};*/

	info.set_private_data = BrowserSource::SetBrowserData;
	info.get_private_data = BrowserSource::GetBrowserData;

	info.show = [](void *data) {
		static_cast<ChatSource *>(data)->SetShowing(true);
	};
	info.hide = [](void *data) {
		static_cast<ChatSource *>(data)->SetShowing(false);
	};
	info.activate = [](void *data) {
		ChatSource *cs = static_cast<ChatSource *>(data);
		if (cs->restart)
			cs->Refresh();
		cs->SetActive(true);
	};
	info.deactivate = [](void *data) {
		static_cast<ChatSource *>(data)->SetActive(false);
	};
	info.properties_edit_start = [](void *data, obs_data_t *settings) {
		static_cast<ChatSource *>(data)->propertiesEditStart(settings);
	};
	info.properties_edit_end = [](void *data, obs_data_t *settings, bool) {
		static_cast<ChatSource *>(data)->propertiesEditEnd(settings);
	};
	//PRISM/Zhangdewen/20211015/#/Chat Source Event
	info.update_extern_params = [](void *data,
				       const calldata_t *extern_params) {
		static_cast<ChatSource *>(data)->updateExternParams(
			extern_params);
	};

	info.icon_type = OBS_ICON_TYPE_CHAT;

	obs_register_source(&info);
}

void release_prism_chat_source() {}

ChatSource::ChatSource(obs_data_t *settings, obs_source_t *source)
	: BrowserSource(settings, source)
{
	width = CHAT_WIDTH;
	height = CHAT_HEIGHT;
}

void ChatSource::Update(obs_data_t *settings)
{
	BrowserSource::Update(settings);

	style = (int)obs_data_get_int(settings, S_CHAT_SOURCE_TEMPLATE);
	fontSize = (int)obs_data_get_int(settings, S_CHAT_SOURCE_FONT_SIZE);

	//send event to Web
	//PRISM/Zhangdewen/20211015/#/Chat Source Event
	obs_source_send_notify(source, OBS_SOURCE_CHAT_UPDATE_PARAMS,
			       OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_UPDATE);
}

void ChatSource::propertiesEditStart(obs_data_t *settings)
{
	//send event to Web
	//PRISM/Zhangdewen/20211015/#/Chat Source Event
	obs_source_send_notify(
		source, OBS_SOURCE_CHAT_UPDATE_PARAMS,
		OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_EDIT_START);
}

void ChatSource::propertiesEditEnd(obs_data_t *settings) {}

void ChatSource::onBrowserLoadEnd()
{
	//send event to Web
	//PRISM/Zhangdewen/20211015/#/Chat Source Event
	obs_source_send_notify(source, OBS_SOURCE_CHAT_UPDATE_PARAMS,
			       OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_LOADED);
}

void ChatSource::dispatchJSEvent(const QByteArray &json)
{
	plog(LOG_DEBUG, "chatEvent: %s", json.constData());

	//send event to Web
	DispatchJSEvent(this, "chatEvent", json.constData());
}

//PRISM/Zhangdewen/20211015/#/Chat Source Event
void ChatSource::updateExternParams(const calldata_t *extern_params)
{
	const char *cjson = calldata_string(extern_params, "cjson");
	int sub_code = calldata_int(extern_params, "sub_code");

	switch (sub_code) {
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_UPDATE:
		dispatchJSEvent(toJson(cjson));
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_EDIT_START:
		dispatchJSEvent(toJson(cjson));
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_LOADED:
		dispatchJSEvent(toJson(cjson));
		obs_source_send_notify(
			source, OBS_SOURCE_CHAT_UPDATE_PARAMS,
			OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_CHECK_LIVE);
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_CHECK_LIVE:
		dispatchJSEvent(toJson(cjson));
		break;
	}
}

//PRISM/Zhangdewen/20211015/#/Chat Source Event
QByteArray ChatSource::toJson(const char *cjson) const
{
	QJsonParseError error;
	QJsonObject setting =
		QJsonDocument::fromJson(QByteArray(cjson), &error).object();
	if (error.error != QJsonParseError::NoError) {
		plog(LOG_ERROR, "");
		return QByteArray();
	}

	QJsonObject data = setting.value("data").toObject();
	data.insert("fontSize", QString::number(fontSize));
	data.insert("styleVersion", QString::number(style));
	setting.insert("data", data);

	QByteArray json = QJsonDocument(setting).toJson(QJsonDocument::Compact);
	return json;
}
