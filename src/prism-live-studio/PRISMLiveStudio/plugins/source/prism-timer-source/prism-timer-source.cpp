#include <QCoreApplication>
#include <QString>
#include <QDir>
#include <QUrl>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QColor>
#include <qlogging.h>
#include <QMetaObject>
#include <obs.h>
#include "prism-timer-source.hpp"
#include <QJsonDocument>
#include "frontend-api.h"
#include "liblog.h"
#include "utils-api.h"
#include "pls-common-define.hpp"
#include "pls/pls-source.h"
#include "pls/pls-properties.h"
#include "pls/pls-obs-api.h"
#include <qdebug.h>

static const char *const s_moduleName = "PRISMTimer";
const static QString s_template_start = "<html><head/><body><p style='white-space: pre-wrap;'>%1<span style='color:#c34151;font-weight:normal;'>*</span></p></body></html>";
static const char *const s_need_force_set_data = "need_force_set_data";
static const char *const s_template_list = "template_list";
static const char *const s_tap = "tab";
static const char *const s_timerType = "timer_type";
static const char *const s_line_timerType = "line_timerType";
static const char *const s_line_timerControl = "line_timerControl";
static const char *const s_font_color = "font_color";
static const char *const s_bg_color = "bg_color";
static const char *const s_opacity_bar = "opacity_bar";
static const char *const s_font_family = "font_famliy";
static const char *const s_line_font = "line_font";
static const char *const s_listen_list = "listen_list";
static const char *const s_listen_list_btn = "listen_list_btn";
static const char *const s_listen_list_btn_enabeled = "listen_list_btn_enabeled";
static const char *const s_control_group = "control_group";
static const char *const s_control_tip = "control_tip";
static const char *const s_stopwatch_tip = "stopwatch_tip";
static const char *const s_text_current = "text_current";
static const char *const s_text_stream = "text_stream";
static const char *const s_text_stopwatch = "text_stopwatch";
static const char *const s_text_start = "text_start";
static const char *const s_text_stop = "text_stop";
static const char *const s_checkbox_hms_hour = "checkbox_hms_hour";
static const char *const s_checkbox_hms_min = "checkbox_hms_min";
static const char *const s_checkbox_hms_second = "checkbox_hms_second";
static const char *const s_checkbox_ms_min = "checkbox_ms_min";
static const char *const s_checkbox_ms_second = "checkbox_ms_second";
static const char *const s_checkbox_s_second = "checkbox_s_second";
static const char *const s_timer_all_seconds = "timer_all_seconds";
static const char *const s_list_corlors_23 = "list_corlors";
static const char *const s_list_corlors_45 = "list_corlors_4";
static const char *const s_list_corlors_6 = "list_corlors_6";
static const char *const s_timer_checkbox_group_hms = "timer_checkbox_group_hms";
static const char *const s_timer_checkbox_group_ms = "timer_checkbox_group_ms";
static const char *const s_timer_checkbox_group_s = "timer_checkbox_group_s";
static const char *const s_url_path = "timer_clock/index.html";
static const char *const s_color_white = "#ffffff";
static const char *const s_color_black = "#000000";

static const char *const s_cb_enabled = "is_ui_click_cb";

static const long long s_timer_normal_max_h = 99;
static const long long s_timer_normal_max_m = 59;
static const long long s_timer_normal_max_s = 59;
static const long long s_timer_6_max_s = 999;

static const long long s_timer_default_seconds = 60;

#if defined(Q_OS_WIN)
static const char *const s_default_font = "Segoe UI";
#elif defined(Q_OS_MACOS)
static const char *const s_default_font = "Apple SD Gothic Neo";
#endif

using namespace std;

static void timer_set_some_part_default(obs_data_t *settings);
static void set_hotkey_visible(timer_source *source);

inline bool &isLivingInstance()
{
	static bool the_tool(false);
	return the_tool;
}

static inline uint32_t GetAudioChannels(enum speaker_layout speakers)
{
	switch (speakers) {
	case SPEAKERS_MONO:
		return 1;
	case SPEAKERS_STEREO:
		return 2;
	case SPEAKERS_2POINT1:
		return 3;
	case SPEAKERS_4POINT0:
		return 4;
	case SPEAKERS_4POINT1:
		return 5;
	case SPEAKERS_5POINT1:
		return 6;
	case SPEAKERS_7POINT1:
		return 8;
	default:
		return 0;
	}
}

AudioConvertThread::AudioConvertThread()
{
	moveToThread(this);
}

void AudioConvertThread::run()
{
	this->exec();
}

void AudioConvertThread::startConvertAudio(obs_source_t *source, const obs_source_audio &audio)
{
	uint64_t linesize = static_cast<uint64_t>(audio.frames) * 4 /*AUDIO_FORMAT_FLOAT_PLANAR is 4*/;
	uint32_t channels = GetAudioChannels(audio.speakers);
	if (!linesize || !channels)
		return;

	auto data = (uint8_t *)bmalloc(linesize * channels);
	if (!data)
		return;

	std::shared_ptr<uint8_t> ptr(data, [](uint8_t *p) { bfree(p); });

	obs_source_audio tmp = audio;
	for (size_t i = 0; i < MAX_AV_PLANES; i++)
		tmp.data[i] = nullptr;

	uint8_t *plane = data;
	for (size_t i = 0; i < channels; i++) {
		memcpy(plane, audio.data[i], linesize);
		tmp.data[i] = plane;
		plane += linesize;
	}

	QMetaObject::invokeMethod(
		this,
		[source, tmp, ptr]() {
			if (source) {
				obs_source_output_audio(source, &tmp);
			}
		},
		Qt::QueuedConnection);
}

static void source_sub_web_receive(void *data, calldata_t *calldata)
{
	pls_unused(calldata);
	const char *name = calldata_string(calldata, "msg");
	auto timerClass = static_cast<timer_source *>(data);
	if (timerClass) {

		QByteArray tempMsg = name;
		QMetaObject::invokeMethod(
			timerClass, [timerClass, tempMsg]() { timerClass->webDidReceiveMessage(tempMsg.constData()); }, Qt::QueuedConnection);
	}
}

static void audio_capture(void *param, obs_source_t *src, const struct audio_data *data, bool muted)
{
	pls_unused(src);
	if (!param || !src || !data || muted)
		return;

	auto timerClass = static_cast<timer_source *>(param);
	if (!timerClass || !timerClass->config.source) {
		return;
	}
	struct obs_source_audio source_data = {};

	for (int i = 0; i < MAX_AV_PLANES; i++)
		source_data.data[i] = data->data[i];

	source_data.frames = data->frames;
	source_data.timestamp = data->timestamp;

	const audio_output_info *mainAOI = audio_output_get_info(obs_get_audio());
	source_data.samples_per_sec = mainAOI->samples_per_sec;
	source_data.speakers = mainAOI->speakers;
	source_data.format = AUDIO_FORMAT_FLOAT_PLANAR;

	if (timerClass->audioThread != nullptr) {
		timerClass->audioThread->startConvertAudio(timerClass->config.source, source_data);
	}
	return;
}

static void init_web_source(timer_source *timerSource)
{
	if (timerSource->audioThread == nullptr) {
		timerSource->audioThread = pls_new<AudioConvertThread>();
		timerSource->audioThread->start();
	}

	if (timerSource->config.web) {
		return;
	}

	// init web source
	obs_data_t *web_settings = obs_data_create();
	QString path = pls_to_abs_path(obs_module_file(s_url_path));
	QUrl url = QUrl::fromLocalFile(path);
	obs_data_set_string(web_settings, "url", url.toString(QUrl::FullyEncoded).toUtf8().constData());
	obs_data_set_bool(web_settings, "is_local_file", false);
	obs_data_set_bool(web_settings, "reroute_audio", true);
	obs_data_set_bool(web_settings, "ignore_reload", true);
	obs_data_set_int(web_settings, "width", 720);
	obs_data_set_int(web_settings, "height", 210);
	timerSource->config.web = obs_source_create_private("browser_source", obs_source_get_name(timerSource->config.source), web_settings);
	auto webSingal = obs_source_get_signal_handler(timerSource->config.web);
	signal_handler_add(webSingal, "void sub_web_receive(ptr source, string msg)");
	signal_handler_connect_ref(webSingal, "sub_web_receive", source_sub_web_receive, timerSource);
	obs_source_add_audio_capture_callback(timerSource->config.web, audio_capture, timerSource);

	obs_data_release(web_settings);

	obs_source_inc_active(timerSource->config.web);
	obs_source_inc_showing(timerSource->config.web);
}

static void mediaStateChanged(void *data, calldata_t *cb)
{
	pls_unused(cb);
	auto context = static_cast<timer_source *>(data);
	if (!context || !data)
		return;

	obs_media_state state = obs_source_media_get_state(context->config.sub_music);
	PLS_INFO(s_moduleName, QString("media state changed, currrent is %1").arg(state).toUtf8().constData());

	bool isStoped = false;
	switch (state) {
	case OBS_MEDIA_STATE_STOPPED:
	case OBS_MEDIA_STATE_ERROR:
	case OBS_MEDIA_STATE_ENDED:
	case OBS_MEDIA_STATE_NONE:
	case OBS_MEDIA_STATE_PAUSED:
		isStoped = true;
		break;
	default:
		break;
	}

	bool oldStatus = obs_data_get_bool(context->config.settings, s_listen_list_btn);
	if (oldStatus != !isStoped) {
		PLS_INFO(s_moduleName, "timer source media state changed, try to update properties page");
		obs_data_set_bool(context->config.settings, s_listen_list_btn, !isStoped);
		obs_source_update_properties(context->config.source);
	}
}

void timer_source::init_music_source()
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", true);
	config.sub_music = obs_source_create_private(common::MEDIA_SOURCE_ID, "prism_timer_music_source", settings);
	if (!config.sub_music) {
		obs_data_release(settings);
		return;
	}
	obs_source_set_monitoring_type(config.sub_music, OBS_MONITORING_TYPE_MONITOR_ONLY);
	obs_data_release(settings);
	obs_source_inc_active(config.sub_music);
	signal_handler_connect_ref(obs_source_get_signal_handler(config.sub_music), "media_play", mediaStateChanged, this);
	signal_handler_connect_ref(obs_source_get_signal_handler(config.sub_music), "media_pause", mediaStateChanged, this);
	signal_handler_connect_ref(obs_source_get_signal_handler(config.sub_music), "media_restart", mediaStateChanged, this);
	signal_handler_connect_ref(obs_source_get_signal_handler(config.sub_music), "media_started", mediaStateChanged, this);
	signal_handler_connect_ref(obs_source_get_signal_handler(config.sub_music), "media_ended", mediaStateChanged, this);
	signal_handler_connect_ref(obs_source_get_signal_handler(config.sub_music), "media_stopped", mediaStateChanged, this);
}

static void source_create_finished(void *data, calldata_t *calldata)
{
	pls_unused(calldata);
	auto context = static_cast<struct timer_source *>(data);
	auto source = (uint64_t)calldata_int(calldata, "source_address");
	if (!source || (source != (uint64_t)context->config.source) || !data)
		return;

	QMetaObject::invokeMethod(
		context, [context]() { init_web_source(context); }, Qt::QueuedConnection);
}

static void liveStatusChangedToReloadUI(bool isStartLive, void *data)
{
	isLivingInstance() = isStartLive;
	auto sourceClass = static_cast<timer_source *>(data);
	if (!sourceClass) {
		return;
	}
	auto timerType = static_cast<TimerType>(obs_data_get_int(sourceClass->config.settings, s_timerType));
	if (timerType != TimerType::Live) {
		return;
	}
	QMetaObject::invokeMethod(
		sourceClass, [sourceClass]() { sourceClass->dispatahControlJsToWeb(); }, Qt::QueuedConnection);
}

void onLiveStateChanged(enum obs_frontend_event event, void *data)
{
	if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		liveStatusChangedToReloadUI(true, data);
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPING || event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
		liveStatusChangedToReloadUI(false, data);
	}
}

void onPrismRearsalSwitchtoLive(pls_frontend_event event, const QVariantList &params, void *context)
{
	Q_UNUSED(params)
	if (event == pls_frontend_event::PLS_FRONTEND_EVENT_REHEARSAL_SWITCH_TO_LIVE) {
		PLS_INFO(s_moduleName, "rehearsal switch to live to reload live time");
		liveStatusChangedToReloadUI(true, context);
	}
}

timer_source::timer_source(obs_source_t *source, obs_data_t *settings)
{
	config.settings = settings;
	obs_data_addref(config.settings);
	config.source = source;
	isLivingInstance() = pls_is_streaming();
	if (!source) {
		PLS_ERROR(s_moduleName, "timer create failed, because out of memory.");
		return;
	}
	obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_ONLY);

	updateWebAudioShow(false);

	signal_handler_connect_ref(obs_get_signal_handler(), "source_create_finished", source_create_finished, this);
	obs_frontend_add_event_callback(onLiveStateChanged, this);
	pls_frontend_add_event_callback(pls_frontend_event::PLS_FRONTEND_EVENT_REHEARSAL_SWITCH_TO_LIVE, onPrismRearsalSwitchtoLive, this);

	if (obs_data_get_bool(config.settings, s_listen_list_btn)) {
		obs_data_set_bool(config.settings, s_listen_list_btn, false);
	}

	signal_handler_add(obs_source_get_signal_handler(source), "void update_properties_ok_button_enable(ptr source, bool enable)");
}

timer_source::~timer_source()
{
	timer_source_destroy();
}

void timer_source::timer_source_update()
{
	dispatahNoramlJsToWeb();

	auto timerType = static_cast<TimerType>(obs_data_get_int(config.settings, s_timerType));
	if (config.mStaus != MusicStatus::Noraml && timerType == TimerType::Current) {
		updateControlButtons(MusicStatus::Noraml, nullptr, false, false);
		dispatahControlJsToWeb();
	}

	if (config.mStaus == MusicStatus::Noraml && timerType == TimerType::Live && isLivingInstance()) {
		dispatahControlJsToWeb();
	}

	if (timerType == TimerType::Countdown && strcmp(obs_data_get_string(config.settings, s_listen_list), "") != 0) {
		updateWebAudioShow(true);
	} else {
		updateWebAudioShow(false);
	}
}

void timer_source::didClickDefaultsButton()
{
	onlyStopMusicIfNeeded();
	obs_data_set_bool(config.settings, s_need_force_set_data, false);
	obs_data_set_int(config.settings, s_template_list, static_cast<int>(TemplateType::One));
	obs_data_set_int(config.settings, s_tap, 0);
	obs_data_set_int(config.settings, s_timerType, 0);
	updateControlButtons(MusicStatus::Noraml, nullptr, false, false);
	dispatahControlJsToWeb();
	timer_set_some_part_default(config.settings);
	dispatahNoramlJsToWeb();
	set_hotkey_visible(this);
}

void timer_source::resetTextToDefaultWhenEmpty()
{

	auto templateType = static_cast<TemplateType>(obs_data_get_int(config.settings, s_template_list));
	if (templateType != TemplateType::Four) {
		return;
	}
	QString text;
	QString stopText;
	auto timerType = static_cast<TimerType>(obs_data_get_int(config.settings, s_timerType));
	switch (timerType) {
	case TimerType::Current:

		text = obs_data_get_string(config.settings, s_text_current);
		if (text.trimmed().isEmpty()) {
			obs_data_set_string(config.settings, s_text_current, obs_module_text("timer.current.type.text"));
		}
		break;
	case TimerType::Live:

		text = obs_data_get_string(config.settings, s_text_stream);
		if (text.trimmed().isEmpty()) {
			obs_data_set_string(config.settings, s_text_stream, obs_module_text("timer.stream.type.text"));
		}
		break;
	case TimerType::Countdown:
		text = obs_data_get_string(config.settings, s_text_start);
		stopText = obs_data_get_string(config.settings, s_text_stop);
		if (text.trimmed().isEmpty()) {
			obs_data_set_string(config.settings, s_text_start, obs_module_text("timer.timer.type.text.start"));
		}
		if (stopText.trimmed().isEmpty()) {
			obs_data_set_string(config.settings, s_text_stop, obs_module_text("timer.timer.type.text.stop"));
		}
		break;
	case TimerType::Stopwatch:
		text = obs_data_get_string(config.settings, s_text_stopwatch);
		if (QString(text).trimmed().isEmpty()) {
			obs_data_set_string(config.settings, s_text_stopwatch, obs_module_text("timer.stopwatch.type.text"));
		}
		break;
	default:
		break;
	}
	dispatahNoramlJsToWeb();
}

void timer_source::sendAnalogWhenClickSavedBtn()
{

	QString type = "basic";
	QString forColor = "";
	QString bgColor = "";
	bool hasBackground = true;
	getColorData(type, forColor, bgColor, hasBackground);

	QString mold = "clock";
	QString _text = "";
	getTextData(mold, _text);

	obs_data_t *font_obj = obs_data_get_obj(config.settings, s_font_family);
	QString fontFamliy = obs_data_get_string(font_obj, "font-family");
	obs_data_release(font_obj);

	QVariantMap _map;
	_map["templateId"] = type;
	_map["clockType"] = mold;
	_map["fontFamily"] = fontFamliy;
	_map["fontColor"] = forColor;
	_map["bgColor"] = bgColor;
	_map["hasBgColor"] = hasBackground;

	pls_send_analog(AnalogType::ANALOG_CLOCK_WIDGET, _map);
}

void timer_source::dispatahNoramlJsToWeb()
{
	if (!this->config.web) {
		return;
	}
	pls_source_dispatch_cef_js(this->config.web, "timerClockWidget", this->toJsonStr().constData());
}

void timer_source::dispatahControlJsToWeb()
{

	long long _startTime = -1;
	auto timerType = static_cast<TimerType>(obs_data_get_int(config.settings, s_timerType));
	QString actionStr = getControlEvent();
	if (timerType == TimerType::Live) {
		if (isLivingInstance()) {
			_startTime = static_cast<long long>(pls_get_live_start_time());
			_startTime = _startTime == 0 ? 0 : _startTime * 1000;
			actionStr = "start";
		} else {
			actionStr = "stop";
		}
	}

	QJsonObject data;

	data["liveStartTime"] = _startTime;
	data["countTime"] = getCountTime();

	data["action"] = actionStr;

	QJsonObject root;
	root["type"] = "control";
	root["data"] = data;
	auto _bArray = QJsonDocument(root).toJson(QJsonDocument::Compact);
	QString _log = QString("timer Log control:").append(_bArray);
	PLS_INFO(s_moduleName, _log.toUtf8().constData());

	if (this->config.web) {
		pls_source_dispatch_cef_js(this->config.web, "timerClockWidget", _bArray.constData());
	}
}

void timer_source::webDidReceiveMessage(const char *msg)
{
	auto doc = QJsonDocument::fromJson(msg);
	if (!doc.isObject()) {
		return;
	}
	auto root = doc.object();
	auto type = root["module"].toString();

	if ("timerClockWidget" != type) {
		return;
	}
	PLS_INFO(s_moduleName, QString("timer did receive web message:").append(msg).toUtf8().constData());
	QJsonObject jsonData;
	const char *KEY_DATA = "data";
	if (root[KEY_DATA].isObject()) {
		jsonData = root[KEY_DATA].toObject();
	} else if (root[KEY_DATA].isString()) {
		auto strData = root["data"].toString();
		jsonData = QJsonDocument::fromJson(strData.toUtf8()).object();
	}

	auto eveName = jsonData["eventName"].toString();

	if (eveName == "pageLoaded") {
		this->timer_source_update();
		return;
	}

	bool isGotStatus = true;
	MusicStatus status = getStatus(eveName, isGotStatus);
	if (isGotStatus) {
		updateControlButtons(status, nullptr, false, true);
	}
}

MusicStatus timer_source::getStatus(const QString &name, bool &isGot) const
{
	isGot = true;
	if (name == "start") {
		return MusicStatus::Started;
	}
	if (name == "finish" && MusicStatus::Noraml != config.mStaus) {
		return MusicStatus::Stoped;
	}
	if (name == "pause") {
		return MusicStatus::Paused;
	}
	if (name == "stop") {
		return MusicStatus::Started;
	}
	if (name == "restart" || name == "resume") {
		return MusicStatus::Started;
	}
	if (name == "cancel" || name == "countTimeChangeWhenWorking") {
		return MusicStatus::Noraml;
	}
	isGot = false;
	return MusicStatus();
}

QString timer_source::getControlEvent() const
{
	QString eventName;
	switch (config.mStaus) {
	case MusicStatus::Noraml:
		eventName = "cancel";
		break;
	case MusicStatus::Started:
		eventName = "start";
		break;
	case MusicStatus::Paused:
		eventName = "pause";
		break;
	case MusicStatus::Resume:
		eventName = "resume";
		break;
	case MusicStatus::Stoped:
		eventName = "stop";
		break;
	default:
		break;
	}
	return eventName;
}

void timer_source::updateControlButtons(MusicStatus status, obs_properties_t *props, bool isForce, bool isNeedUpdate)
{
	MusicStatus oldStatus = config.mStaus;
	config.mStaus = status;

	QString startText;
	QString stopText;
	bool startEnable = false;
	bool stopEnable = false;
	getControlButtonTextAndEnable(0, startText, startEnable);
	getControlButtonTextAndEnable(1, stopText, stopEnable);

	obs_hotkey_set_description(config.start_hotkey, startText.toUtf8().constData());
	obs_hotkey_set_description(config.cancel_hotkey, stopText.toUtf8().constData());

	PLS_INFO(s_moduleName, QString("timer current status:%1").arg((int)status).toUtf8().constData());
	pls_source_send_notify(config.source, OBS_SOURCE_TIMER_BUTTON_STATE_CHANGED, 0);

	if (status == oldStatus && !isForce) {
		return;
	}

	if (!isPropertiesOpened) {
		return;
	}

	obs_properties_t *_props = props ? props : obs_source_properties(config.source);
	if (!_props) {
		return;
	}
	obs_property_t *p = obs_properties_get(_props, s_control_group);
	if (!p && !props) {
		obs_properties_destroy(_props);
		return;
	}

	pls_property_button_group_set_item_desc(p, 0, startText.toUtf8().constData());
	pls_property_button_group_set_item_highlight(p, 0, getStartButtonHightlight());
	pls_property_button_group_set_item_desc(p, 1, stopText.toUtf8().constData());
	pls_property_button_group_set_item_enabled(p, 0, startEnable);
	pls_property_button_group_set_item_enabled(p, 1, stopEnable);

	if (!props) {
		obs_properties_destroy(_props);
	}

	if (isNeedUpdate) {
		obs_source_update_properties(config.source);
	}
}
void timer_source::getControlButtonTextAndEnable(int idx, QString &text, bool &enable)
{
	if (idx == 0) {
		enable = true;
		auto timerType = static_cast<TimerType>(obs_data_get_int(config.settings, s_timerType));

		if (timerType == TimerType::Current || timerType == TimerType::Live) {
			enable = false;
		} else if (timerType == TimerType::Countdown && getCountTime() == 0) {
			enable = false;
		}
		switch (config.mStaus) {
		case MusicStatus::Noraml:
			text = obs_module_text("timer.btn.start");
			break;
		case MusicStatus::Started:
			text = obs_module_text("timer.btn.pause");
			break;
		case MusicStatus::Paused:
			text = obs_module_text("timer.btn.restart");
			break;
		case MusicStatus::Resume:
			text = obs_module_text("timer.btn.pause");
			break;
		case MusicStatus::Stoped:
			text = obs_module_text("timer.btn.stop");
			break;
		default:
			break;
		}
		return;
	}
	auto timerType = static_cast<TimerType>(obs_data_get_int(config.settings, s_timerType));

	text = timerType == TimerType::Stopwatch ? obs_module_text("timer.btn.reset") : obs_module_text("timer.btn.cancel");

	if (timerType == TimerType::Current || timerType == TimerType::Live) {
		enable = false;
		return;
	}
	switch (config.mStaus) {
	case MusicStatus::Noraml:
		enable = false;
		break;
	case MusicStatus::Started:
		enable = true;
		break;
	case MusicStatus::Paused:
		enable = true;
		break;
	case MusicStatus::Resume:
		enable = true;
		break;
	case MusicStatus::Stoped:
		enable = timerType == TimerType::Stopwatch ? true : false;
		break;
	default:
		break;
	}
}

void timer_source::timer_source_destroy()
{
	if (isPropertiesOpened == true) {
		propertiesEditEnd(false);
	}

	obs_data_release(config.settings);
	obs_frontend_remove_event_callback(onLiveStateChanged, this);
	pls_frontend_remove_event_callback(pls_frontend_event::PLS_FRONTEND_EVENT_REHEARSAL_SWITCH_TO_LIVE, onPrismRearsalSwitchtoLive, this);

	signal_handler_disconnect(obs_get_signal_handler(), "source_create_finished", source_create_finished, this);

	if (config.start_hotkey) {
		obs_hotkey_unregister(config.start_hotkey);
	}
	if (config.cancel_hotkey) {
		obs_hotkey_unregister(config.cancel_hotkey);
	}
	if (config.web) {
		signal_handler_disconnect(obs_source_get_signal_handler(config.web), "sub_web_receive", source_sub_web_receive, this);
		obs_source_remove_audio_capture_callback(config.web, audio_capture, this);

		obs_source_dec_active(config.web);
		obs_source_release(config.web);
	}
	if (audioThread) {
		audioThread->quit();
		audioThread->wait();
		audioThread->deleteLater();
	}
	obs_enter_graphics();
	if (config.web_source_tex) {
		gs_texture_destroy(config.web_source_tex);
		config.web_source_tex = nullptr;
	}
	obs_leave_graphics();
}

static inline QString color_string_from_int(long long val)
{
	auto _color = QColor(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff, (val >> 24) & 0xff);
	QString colorStr = _color.name(QColor::HexRgb);
	return colorStr;
}

static qint64 int_from_hour_min_second(qint64 hour, qint64 minute, qint64 second)
{
	return hour * 60 * 60 + minute * 60 + second;
}

static void int_to_hour_min_second(qint64 allSecond, qint64 &hour, qint64 &minute, qint64 &second)
{
	hour = allSecond / 3600;
	minute = (allSecond - hour * 3600) / 60;
	second = (allSecond - hour * 3600 - minute * 60);
}

void timer_source::getColorData(QString &type, QString &forColor, QString &bgColor, bool &hasBackground)
{
	auto templateType = static_cast<TemplateType>(obs_data_get_int(config.settings, s_template_list));

	auto f_colorIndex_23 = obs_data_get_int(config.settings, s_list_corlors_23);

	if (templateType == TemplateType::One) {
		type = "basic";
		forColor = color_string_from_int(obs_data_get_int(config.settings, s_font_color));

		obs_data_t *color_obj = obs_data_get_obj(config.settings, s_bg_color);
		hasBackground = obs_data_get_bool(color_obj, "is_enable");
		auto color_val = static_cast<int>(obs_data_get_int(color_obj, "color_val"));
		bgColor = color_string_from_int(color_val);
		obs_data_release(color_obj);

	} else if (templateType == TemplateType::Two) {

		type = "round";
		bgColor = PLSColorData::instance()->m_vecColors_t23[f_colorIndex_23].colorValue;
		forColor = s_color_white;
		if (bgColor.toLower() == s_color_white) {
			forColor = s_color_black;
		}
	} else if (templateType == TemplateType::Three) {
		type = "flip";
		forColor = PLSColorData::instance()->m_vecColors_t23[f_colorIndex_23].colorValue;
		bgColor = s_color_white;
		if (forColor.toLower() == s_color_white) {
			bgColor = s_color_black;
		}
	} else if (templateType == TemplateType::Four) {
		type = "message";
		auto f_colorIndex_4 = obs_data_get_int(config.settings, s_list_corlors_45);
		bgColor = PLSColorData::instance()->m_vecColors_t45[f_colorIndex_4].colorValue;
		forColor = s_color_white;
		if (bgColor.toLower() == s_color_white) {
			forColor = s_color_black;
		}
	} else if (templateType == TemplateType::Five) {
		type = "tech";
		auto f_colorIndex_45 = obs_data_get_int(config.settings, s_list_corlors_45);
		forColor = PLSColorData::instance()->m_vecColors_t45[f_colorIndex_45].colorValue;
		bgColor = s_color_white;
		if (forColor.toLower() == s_color_white) {
			bgColor = s_color_black;
		}
	} else if (templateType == TemplateType::Six) {
		type = "second";
		auto f_colorIndex_6 = obs_data_get_int(config.settings, s_list_corlors_6);
		hasBackground = false;
		forColor = PLSColorData::instance()->m_vecColors_t6[f_colorIndex_6].colorValue;
		bgColor = s_color_white;
	}
}

void timer_source::getTextData(QString &mold, QString &_text)
{

	auto timerType = static_cast<TimerType>(obs_data_get_int(config.settings, s_timerType));
	switch (timerType) {
	case TimerType::Current:
		mold = "clock";
		_text = obs_data_get_string(config.settings, s_text_current);
		break;
	case TimerType::Live:
		mold = "liveTimer";
		_text = obs_data_get_string(config.settings, s_text_stream);
		break;
	case TimerType::Countdown:
		mold = "countDown";
		break;
	case TimerType::Stopwatch:
		mold = "countUp";
		_text = obs_data_get_string(config.settings, s_text_stopwatch);
		break;
	default:
		break;
	}
}

QByteArray timer_source::toJsonStr()
{
	auto opacityValue = obs_data_get_int(config.settings, s_opacity_bar);

	obs_data_t *font_obj = obs_data_get_obj(config.settings, s_font_family);
	QString fontFamliy = obs_data_get_string(font_obj, "font-family");
	QString fontWidget = obs_data_get_string(font_obj, "font-weight");
	obs_data_release(font_obj);

	auto musicPath = QString(obs_data_get_string(config.settings, s_listen_list));

	QString type = "basic";
	QString forColor = "";
	QString bgColor = "";
	bool hasBackground = true;
	getColorData(type, forColor, bgColor, hasBackground);

	QString mold = "clock";
	QString _text = "";
	getTextData(mold, _text);

	QJsonObject data;
	data["type"] = type;
	data["mold"] = mold;
	data["color"] = forColor;
	data["hasBackground"] = hasBackground;
	data["background"] = bgColor;
	data["opacity"] = QString::number(static_cast<double>(opacityValue) / 100.0);
	data["countTime"] = getCountTime();
	data["fontName"] = fontFamliy;
	data["fontWeight"] = fontWidget;

	data["endMusicName"] = musicPath;
	data["text"] = _text;
	data["startText"] = obs_data_get_string(config.settings, s_text_start);
	data["endText"] = obs_data_get_string(config.settings, s_text_stop);

	QJsonObject root;
	root["type"] = "setting";
	root["data"] = data;
	auto _bArray = QJsonDocument(root).toJson(QJsonDocument::Compact);
	QString _log = QString("timer log normal:").append(_bArray);
	PLS_INFO(s_moduleName, _log.toUtf8().constData());
	return _bArray;
}

long long timer_source::getCountTime()
{
	long long _downTime = -1;
	auto timerType = static_cast<TimerType>(obs_data_get_int(config.settings, s_timerType));
	if (timerType != TimerType::Countdown) {
		return _downTime;
	}

	auto templateType = static_cast<TemplateType>(obs_data_get_int(config.settings, s_template_list));
	_downTime = getCountTime(templateType);

	return _downTime;
}
long long timer_source::getCountTime(TemplateType templateType)
{
	long long h = 0;
	long long m = 0;
	long long s = 0;

	switch (templateType) {
	case TemplateType::One:
	case TemplateType::Two:
	case TemplateType::Three:
	case TemplateType::Four:
		h = obs_data_get_int(config.settings, s_checkbox_hms_hour);
		m = obs_data_get_int(config.settings, s_checkbox_hms_min);
		s = obs_data_get_int(config.settings, s_checkbox_hms_second);
		break;
	case TemplateType::Five:
		m = obs_data_get_int(config.settings, s_checkbox_ms_min);
		s = obs_data_get_int(config.settings, s_checkbox_ms_second);
		break;
	case TemplateType::Six:
		s = obs_data_get_int(config.settings, s_checkbox_s_second);
		break;
	default:
		break;
	}
	return int_from_hour_min_second(h, m, s);
}
void timer_source::onlyStopMusicIfNeeded()
{
	if (!config.sub_music) {
		return;
	}
	obs_data_set_bool(config.settings, s_listen_list_btn, false);

	obs_data_t *sub_settings = obs_source_get_settings(config.sub_music);
	obs_media_state state = obs_source_media_get_state(config.sub_music);
	if (state == OBS_MEDIA_STATE_PLAYING || state == OBS_MEDIA_STATE_OPENING) {
		obs_source_media_stop(config.sub_music);
	}
	obs_data_release(sub_settings);
}

static uint32_t timer_source_getwidth(void *data)
{
	const auto *sourceClass = static_cast<timer_source *>(data);
	auto web = sourceClass->config.web;
	return obs_source_get_width(web);
}

static uint32_t timer_source_getheight(void *data)
{
	const auto *sourceClass = static_cast<timer_source *>(data);
	auto web = sourceClass->config.web;
	return obs_source_get_height(web);
}

static void timer_source_render(void *data, gs_effect_t *gs)
{
	pls_unused(gs);
	auto context = static_cast<timer_source *>(data);

	gs_blend_state_push();
	gs_reset_blend_state();

	const gs_effect_t *def_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_technique_t *tech = gs_effect_get_technique(def_effect, "Draw");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_effect_set_texture(gs_effect_get_param_by_name(def_effect, "image"), context->config.web_source_tex);
	gs_draw_sprite(context->config.web_source_tex, 0, 0, 0);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_blend_state_pop();
}

static void timer_source_tick(void *data, float)
{
	auto *sourceClass = static_cast<timer_source *>(data);
	auto web = sourceClass->config.web;
	if (!sourceClass || !web) {
		return;
	}

	uint32_t source_width = obs_source_get_width(web);
	uint32_t source_height = obs_source_get_height(web);
	if (source_width <= 0 || source_height <= 0) {
		if (sourceClass->config.web_source_tex) {
			obs_enter_graphics();
			gs_texture_t *pre_rt = gs_get_render_target();
			gs_projection_push();
			gs_set_render_target(sourceClass->config.web_source_tex, nullptr);
			struct vec4 clear_color = {0};
			vec4_zero(&clear_color);
			gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);
			gs_set_render_target(pre_rt, nullptr);
			gs_projection_pop();
			obs_leave_graphics();
		}

		return;
	}
	obs_enter_graphics();
	if (sourceClass->config.web_source_tex) {
		uint32_t tex_width = gs_texture_get_width(sourceClass->config.web_source_tex);
		uint32_t tex_height = gs_texture_get_height(sourceClass->config.web_source_tex);
		if (tex_width != source_width || tex_height != source_height) {
			gs_texture_destroy(sourceClass->config.web_source_tex);
			sourceClass->config.web_source_tex = nullptr;
		}
	}

	if (!sourceClass->config.web_source_tex) {
		sourceClass->config.web_source_tex = gs_texture_create(source_width, source_height, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
	}

	gs_texture_t *pre_rt = gs_get_render_target();
	gs_viewport_push();
	gs_projection_push();
	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);

	gs_set_render_target(sourceClass->config.web_source_tex, nullptr);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);
	gs_set_viewport(0, 0, source_width, source_height);

	gs_ortho(0.0f, float(source_width), 0.0f, float(source_height), -100.0f, 100.0f);
	obs_source_video_render(sourceClass->config.web);

	gs_set_render_target(pre_rt, nullptr);
	gs_viewport_pop();
	gs_projection_pop();

	obs_leave_graphics();
}

static bool ui_page_check_visible(void *data, obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	pls_unused(data);
	pls_unused(p);

	bool isDetailTab = obs_data_get_int(settings, s_tap) == 0 ? false : true;
	auto templateType = static_cast<TemplateType>(obs_data_get_int(settings, s_template_list));
	auto timerType = static_cast<TimerType>(obs_data_get_int(settings, s_timerType));
	pls_unused(timerType);
	auto *template_list = obs_properties_get(props, s_template_list);
	auto *timer_type = obs_properties_get(props, s_timerType);
	auto *line_timerType = obs_properties_get(props, s_line_timerType);

	auto *timer_checkbox_group_hms = obs_properties_get(props, s_timer_checkbox_group_hms);
	auto *timer_checkbox_group_ms = obs_properties_get(props, s_timer_checkbox_group_ms);
	auto *timer_checkbox_group_s = obs_properties_get(props, s_timer_checkbox_group_s);

	auto *listen_list = obs_properties_get(props, s_listen_list);
	auto *control_group = obs_properties_get(props, s_control_group);
	auto *line_timerControl = obs_properties_get(props, s_line_timerControl);

	auto *font_family = obs_properties_get(props, s_font_family);
	auto *font_color = obs_properties_get(props, s_font_color);
	auto *bg_color = obs_properties_get(props, s_bg_color);
	auto *list_corlors23 = obs_properties_get(props, s_list_corlors_23);
	auto *list_corlors_45 = obs_properties_get(props, s_list_corlors_45);
	auto *list_corlors_6 = obs_properties_get(props, s_list_corlors_6);
	auto *opacity_bar = obs_properties_get(props, s_opacity_bar);
	auto *line_font = obs_properties_get(props, s_line_font);

	//CheckBoxs and dropdown and Btns
	bool isControlGroupShow = false;

	bool isTemplateFour = false;
	bool isStartStopEditShow = false;
	bool isControlBtnShow = false;
	bool isFontGrounpShow = false;

	//color list shown
	bool isListColor23Show = false;
	bool isListColor45Show = false;
	bool isListColor6Show = false;

	// hour min second group shown
	bool isIntGroupShow_hms = false;
	bool isIntGroupShow_ms = false;
	bool isIntGroupShow_s = false;

	auto controlBtnTitle = QString(obs_module_text("timer.clock.control.title"));
	if (timerType == TimerType::Stopwatch) {
		controlBtnTitle = s_template_start.arg(obs_module_text("timer.stopwatch.control.title"));
	}

	switch (templateType) {
	case TemplateType::One:
		isFontGrounpShow = isDetailTab;
		isControlGroupShow = isDetailTab && timerType == TimerType::Countdown;
		isControlBtnShow = isDetailTab && timerType == TimerType::Stopwatch;
		isIntGroupShow_hms = isControlGroupShow;
		break;
	case TemplateType::Two:
	case TemplateType::Three:
		isControlGroupShow = isDetailTab && timerType == TimerType::Countdown;
		isControlBtnShow = isDetailTab && timerType == TimerType::Stopwatch;
		isListColor23Show = isDetailTab;
		isIntGroupShow_hms = isControlGroupShow;
		break;
	case TemplateType::Four:
		isStartStopEditShow = isDetailTab && timerType == TimerType::Countdown;
		isTemplateFour = isDetailTab;
		isControlGroupShow = isDetailTab && timerType == TimerType::Countdown;
		isControlBtnShow = isDetailTab && timerType == TimerType::Stopwatch;
		isListColor45Show = isDetailTab;
		isIntGroupShow_hms = isControlGroupShow;
		break;
	case TemplateType::Five:
		isControlGroupShow = isDetailTab && timerType == TimerType::Countdown;
		isControlBtnShow = isDetailTab && timerType == TimerType::Stopwatch;
		isListColor45Show = isDetailTab;
		isIntGroupShow_ms = isDetailTab && timerType == TimerType::Countdown;
		break;
	case TemplateType::Six:
		isControlGroupShow = isDetailTab && timerType == TimerType::Countdown;
		isControlBtnShow = isDetailTab && timerType == TimerType::Stopwatch;
		isListColor6Show = isDetailTab;
		isIntGroupShow_s = isDetailTab && timerType == TimerType::Countdown;
		break;
	default:
		break;
	}

	bool isOnlyCountdown = (templateType == TemplateType::Five || templateType == TemplateType::Six);

	pls_property_bool_group_set_item_enabled(timer_type, static_cast<int>(TimerType::Current), !isOnlyCountdown);
	pls_property_bool_group_set_item_enabled(timer_type, static_cast<int>(TimerType::Live), !isOnlyCountdown);
	pls_property_bool_group_set_item_enabled(timer_type, static_cast<int>(TimerType::Countdown), true);
	pls_property_bool_group_set_item_enabled(timer_type, static_cast<int>(TimerType::Stopwatch), !isOnlyCountdown);

	obs_property_set_description(control_group, controlBtnTitle.toUtf8().constData());
	obs_property_set_visible(template_list, !isDetailTab);

	obs_property_set_visible(timer_type, isDetailTab);
	obs_property_set_visible(line_timerType, isDetailTab);

	obs_property_set_visible(timer_checkbox_group_hms, isIntGroupShow_hms);
	obs_property_set_visible(timer_checkbox_group_ms, isIntGroupShow_ms);
	obs_property_set_visible(timer_checkbox_group_s, isIntGroupShow_s);

	obs_property_set_visible(listen_list, isControlGroupShow);
	obs_property_set_visible(control_group, isControlBtnShow || isControlGroupShow);
	obs_property_set_visible(obs_properties_get(props, s_control_tip), isDetailTab && timerType == TimerType::Countdown);
	obs_property_set_visible(obs_properties_get(props, s_stopwatch_tip), isDetailTab && timerType == TimerType::Stopwatch);
	obs_property_set_visible(line_timerControl, isControlGroupShow || isControlBtnShow);

	obs_property_set_visible(obs_properties_get(props, s_text_current), isTemplateFour && timerType == TimerType::Current);
	obs_property_set_visible(obs_properties_get(props, s_text_stream), isTemplateFour && timerType == TimerType::Live);
	obs_property_set_visible(obs_properties_get(props, s_text_start), isStartStopEditShow);
	obs_property_set_visible(obs_properties_get(props, s_text_stop), isStartStopEditShow);
	obs_property_set_visible(obs_properties_get(props, s_text_stopwatch), isTemplateFour && timerType == TimerType::Stopwatch);

	obs_property_set_visible(font_family, isFontGrounpShow);
	obs_property_set_visible(font_color, isFontGrounpShow);
	obs_property_set_visible(bg_color, isFontGrounpShow);

	obs_property_set_visible(list_corlors23, isListColor23Show);
	obs_property_set_visible(list_corlors_45, isListColor45Show);
	obs_property_set_visible(list_corlors_6, isListColor6Show);

	bool isShowOpacityBar = isFontGrounpShow || isListColor23Show || isListColor45Show || isListColor6Show;
	obs_property_set_visible(opacity_bar, isShowOpacityBar);
	obs_property_set_visible(line_font, isShowOpacityBar);
	return true;
}

static bool template_changed(void *data, obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	pls_unused(data);
	pls_unused(p);
	pls_unused(settings);
	if (!settings || !obs_data_get_bool(settings, s_cb_enabled)) {
		return false;
	}
	obs_data_set_bool(settings, s_cb_enabled, false);
	auto source = static_cast<timer_source *>(data);

	source->resetCountdownIfNeed(settings);
	source->updateOKButtonEnable();
	timer_set_some_part_default(source->config.settings);
	obs_property_set_description(obs_properties_get(props, s_list_corlors_23), source->getColorTitle());
	obs_property_set_description(obs_properties_get(props, s_list_corlors_45), source->getColorTitle());
	obs_property_set_description(obs_properties_get(props, s_list_corlors_6), source->getColorTitle());
	set_hotkey_visible(source);
	source->updateControlButtons(source->config.mStaus, props, true, false);
	return false;
}

static bool top_buttons_changed(void *data, obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	//this method not check ignore condition, s_cb_enabled
	return ui_page_check_visible(data, props, p, settings);
}

static void start_timer_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
static void cancel_timer_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

static void set_hotkey_visible(timer_source *source)
{
	if (!source) {
		return;
	}

	OBSDataAutoRelease privateSettings = obs_source_get_private_settings(source->config.source);

	auto timerType = static_cast<TimerType>(obs_data_get_int(source->config.settings, s_timerType));
	if (timerType != TimerType::Countdown && timerType != TimerType::Stopwatch) {
		obs_data_set_bool(privateSettings, "hotkey_invisible", true);
	} else {
		obs_data_set_bool(privateSettings, "hotkey_invisible", false);
	}
	OBSDataArrayAutoRelease dataArray = obs_data_array_create();
	OBSDataAutoRelease hotkeyData1 = obs_data_create();
	obs_data_set_string(hotkeyData1, "hotkey_name", "TimerSource.Start");
	OBSDataAutoRelease hotkeyData2 = obs_data_create();
	obs_data_set_string(hotkeyData2, "hotkey_name", "TimerSource.Cancel");
	obs_data_array_push_back(dataArray, hotkeyData1);
	obs_data_array_push_back(dataArray, hotkeyData2);
	obs_data_set_array(privateSettings, "invisible_lists", dataArray);
}

static bool top_type_radius_changed(void *data, obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	pls_unused(p);
	if (!settings || !obs_data_get_bool(settings, s_cb_enabled)) {
		return false;
	}
	obs_data_set_bool(settings, s_cb_enabled, false);
	auto source = static_cast<timer_source *>(data);
	source->isChangeControlByHand = true;
	source->onlyStopMusicIfNeeded();
	source->updateControlButtons(MusicStatus::Noraml, props, true, false);
	source->updateOKButtonEnable();
	set_hotkey_visible(source);
	return ui_page_check_visible(data, props, nullptr, settings);
}

static bool startButtonClicked(obs_properties_t *props, obs_property_t *p, void *data)
{
	pls_unused(props);
	pls_unused(p);
	auto source = static_cast<timer_source *>(data);
	source->isChangeControlByHand = true;
	MusicStatus _status = MusicStatus::Noraml;

	switch (source->config.mStaus) {
	case MusicStatus::Noraml:
		_status = MusicStatus::Started;
		break;
	case MusicStatus::Started:
		_status = MusicStatus::Paused;
		break;
	case MusicStatus::Paused:
		_status = MusicStatus::Resume;
		break;
	case MusicStatus::Resume:
		_status = MusicStatus::Started;
		break;
	case MusicStatus::Stoped:
		_status = MusicStatus::Noraml;
		break;
	default:
		break;
	}
	source->updateControlButtons(_status, props);
	source->dispatahControlJsToWeb();
	return true;
}

static bool stopButtonClicked(obs_properties_t *props, obs_property_t *p, void *data)
{
	pls_unused(props);
	pls_unused(p);
	auto source = static_cast<timer_source *>(data);
	source->isChangeControlByHand = true;
	source->updateControlButtons(MusicStatus::Noraml, props);
	source->dispatahControlJsToWeb();

	return true;
}

#define BOOL2STR(x) (x) ? "true" : "false"

static bool timeListenCallback(void *data, obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	pls_unused(props);
	pls_unused(p);

	if (!settings || !obs_data_get_bool(settings, s_cb_enabled)) {
		return false;
	}
	obs_data_set_bool(settings, s_cb_enabled, false);
	auto source = static_cast<timer_source *>(data);
	auto pathName = obs_data_get_string(settings, s_listen_list);
	auto isCheck = obs_data_get_bool(settings, s_listen_list_btn);

	obs_data_set_bool(settings, s_listen_list_btn_enabeled, source->getListenButtonEnable());

	if (!isCheck && !source->config.sub_music) {
		return true;
	}

	if (!source->config.sub_music) {
		source->init_music_source();
	}

	auto listenUrl = QString();
	if (QString::compare("", pathName) == 0) {
		obs_data_set_bool(settings, s_listen_list_btn, false);
	} else {
#if defined(Q_OS_WIN)
		QDir appDir(QCoreApplication::applicationDirPath());
		const static QString s_music_path = "../../data/prism-plugins/prism-timer-source/timer_clock/media/";
		listenUrl = QString(QUrl::fromLocalFile(appDir.absoluteFilePath(s_music_path)).toLocalFile()).append(pathName);
#elif defined(Q_OS_MACOS)
		QString timerResPath = pls_get_dll_dir("prism-timer-source");
		const static QString s_music_path = "/timer_clock/media/";
		listenUrl = QString(QUrl::fromLocalFile(QString("%1%2").arg(timerResPath).arg(s_music_path)).toLocalFile()).append(pathName);
#endif
	}

	obs_data_t *sub_settings = obs_source_get_settings(source->config.sub_music);
	QString currentUrl = QString::fromUtf8(obs_data_get_string(sub_settings, "local_file"));
	obs_media_state state = obs_source_media_get_state(source->config.sub_music);

	if (!isCheck) {
		if (state == OBS_MEDIA_STATE_PLAYING || state == OBS_MEDIA_STATE_OPENING) {
			obs_source_media_stop(source->config.sub_music);
		}
	} else if (listenUrl.size() > 0 && state != OBS_MEDIA_STATE_PLAYING) {
		if (currentUrl == listenUrl) {
			obs_source_media_restart(source->config.sub_music);
		} else {
			obs_data_set_string(sub_settings, "local_file", listenUrl.toUtf8().constData());
			obs_source_update(source->config.sub_music, sub_settings);
		}
	}

	obs_data_release(sub_settings);
	return true;
}

static bool timeIntGrounpCallback(void *data, obs_properties_t *props, obs_property_t *_p, obs_data_t *settings)
{
	pls_unused(props);
	pls_unused(_p);

	if (!settings || !obs_data_get_bool(settings, s_cb_enabled)) {
		return false;
	}
	obs_data_set_bool(settings, s_cb_enabled, false);
	auto source = static_cast<timer_source *>(data);
	auto *p = obs_properties_get(props, s_control_group);
	if (!p) {
		return false;
	}

	source->isChangeControlByHand = true;
	auto timerCount = source->getCountTime(static_cast<TemplateType>(obs_data_get_int(settings, s_template_list)));
	obs_data_set_int(settings, s_timer_all_seconds, timerCount);

	if (source->config.mStaus != MusicStatus::Noraml) {
		source->updateControlButtons(MusicStatus::Noraml, props, false, false);
		source->dispatahControlJsToWeb();
		return true;
	} else {
		bool startEnable = pls_property_button_group_item_enabled(p, 0);
		auto _count = source->getCountTime();
		if ((_count == 0 && startEnable) || (_count > 0 && !startEnable)) {
			QMetaObject::invokeMethod(
				source, [source]() { source->updateControlButtons(MusicStatus::Noraml, nullptr, true); }, Qt::QueuedConnection);
		}
		return false;
	}
}

static bool textDidChangedCallback(void *data, obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	pls_unused(props);
	pls_unused(p);

	if (!settings || !obs_data_get_bool(settings, s_cb_enabled)) {
		return false;
	}
	obs_data_set_bool(settings, s_cb_enabled, false);
	auto source = static_cast<timer_source *>(data);
	source->updateOKButtonEnable();
	return false;
}

static obs_properties_t *timer_source_getproperties(void *data)
{

	auto s = static_cast<timer_source *>(data);

	obs_properties_t *props = pls_properties_create();
	obs_property_t *p;
	QString messageIcon;

	if (IS_KR()) {
		messageIcon = ":/timer-source/resource/images/Message_Ko.png";
	} else if (pls_is_match_current_language("ja-JP")) {
		messageIcon = ":/timer-source/resource/images/Message_Ja.png";
	} else {
		messageIcon = ":/timer-source/resource/images/Message_En.png";
	}

	p = pls_properties_tm_add_tab(props, s_tap);
	obs_property_set_modified_callback2(p, top_buttons_changed, s);

	/*template setting*/
	p = pls_properties_add_image_group(props, s_template_list, "", 1, 4, PLS_IMAGE_STYLE_APNG_BUTTON);
	pls_property_set_flags(p, PROPERTY_FLAG_NO_LABEL_SINGLE);
	pls_property_image_group_add_item(p, "template_btn_one", ":/timer-source/resource/images/Basic.png", 0, nullptr);
	pls_property_image_group_add_item(p, "template_btn_two", ":/timer-source/resource/images/Round.png", 1, nullptr);
	pls_property_image_group_add_item(p, "template_btn_three", ":/timer-source/resource/images/Flip.png", 2, nullptr);
	pls_property_image_group_add_item(p, "template_btn_four", messageIcon.toUtf8().constData(), 3, nullptr);
	pls_property_image_group_add_item(p, "template_btn_five", ":/timer-source/resource/images/timer_digital_thumb.png", 0, nullptr);
	pls_property_image_group_add_item(p, "template_btn_six", ":/timer-source/resource/images/countdown_thumb.png", 0, nullptr);

	obs_property_set_modified_callback2(p, template_changed, s);

	/* top radios */
	p = pls_properties_add_bool_group(props, s_timerType, s_template_start.arg(obs_module_text("timer.clock.type")).toUtf8().constData());
	pls_property_bool_group_add_item(p, "timer_type 0", obs_module_text("timer.type.current.time"), nullptr, nullptr);
	pls_property_bool_group_add_item(p, "timer_type 1", obs_module_text("timer.type.live.time"), nullptr, nullptr);
	pls_property_bool_group_add_item(p, "timer_type 2", obs_module_text("timer.type.timer.time"), nullptr, nullptr);
	pls_property_bool_group_add_item(p, "timer_type 3", obs_module_text("timer.type.stopwatch.time"), nullptr, nullptr);
	obs_property_set_modified_callback2(p, top_type_radius_changed, s);

	pls_properties_add_line(props, s_line_timerType, "");

	p = pls_properties_add_custom_group(props, s_timer_checkbox_group_hms, s_template_start.arg(obs_module_text("timer.timer.time")).toUtf8().constData());
	pls_property_custom_group_add_int(p, s_checkbox_hms_hour, obs_module_text("timer.h"), "", true, true, 0, s_timer_normal_max_h, 1, "", OBS_NUMBER_SCROLLER);
	pls_property_custom_group_add_int(p, s_checkbox_hms_min, obs_module_text("timer.m"), "", true, true, 0, s_timer_normal_max_m, 1, "", OBS_NUMBER_SCROLLER);
	pls_property_custom_group_add_int(p, s_checkbox_hms_second, obs_module_text("timer.s"), "", true, true, 0, s_timer_normal_max_s, 1, "", OBS_NUMBER_SCROLLER);
	obs_property_set_modified_callback2(p, timeIntGrounpCallback, s);

	p = pls_properties_add_custom_group(props, s_timer_checkbox_group_ms, s_template_start.arg(obs_module_text("timer.timer.time")).toUtf8().constData());
	pls_property_custom_group_add_int(p, s_checkbox_ms_min, obs_module_text("timer.m"), "", true, true, 0, s_timer_normal_max_m, 1, "", OBS_NUMBER_SCROLLER);
	pls_property_custom_group_add_int(p, s_checkbox_ms_second, obs_module_text("timer.s"), "", true, true, 0, s_timer_normal_max_m, 1, "", OBS_NUMBER_SCROLLER);
	obs_property_set_modified_callback2(p, timeIntGrounpCallback, s);

	p = pls_properties_add_custom_group(props, s_timer_checkbox_group_s, s_template_start.arg(obs_module_text("timer.timer.time")).toUtf8().constData());
	pls_property_custom_group_add_int(p, s_checkbox_s_second, obs_module_text("timer.s"), "", true, true, 0, s_timer_6_max_s, 1, "", OBS_NUMBER_SCROLLER);
	obs_property_set_modified_callback2(p, timeIntGrounpCallback, s);

	p = obs_properties_add_list(props, s_listen_list, s_template_start.arg(obs_module_text("timer.when.timer.ends")).toUtf8().constData(), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	pls_property_set_flags(p, PROPERTY_FLAG_LIST_TIMER_LISTEN);
	for (const auto &item : PLSColorData::instance()->m_vecMusicPath) {
		obs_property_list_add_string(p, item.first, item.second);
	}
	obs_property_set_modified_callback2(p, timeListenCallback, s);

	p = pls_properties_add_button_group(props, s_control_group, obs_module_text("timer.clock.control.title"));
	pls_property_set_flags(p, PROPERTY_FLAG_BUTTON_WIDTH_FIXED);
	QString startText;
	QString stopText;
	bool startEnable = false;
	bool stopEnable = false;
	s->getControlButtonTextAndEnable(0, startText, startEnable);
	s->getControlButtonTextAndEnable(1, stopText, stopEnable);
	pls_property_button_group_add_item(p, "timer_start", startText.toUtf8().constData(), startEnable, startButtonClicked);
	pls_property_button_group_add_item(p, "timer_stop", stopText.toUtf8().constData(), stopEnable, stopButtonClicked);
	pls_property_button_group_set_item_highlight(p, 0, s->getStartButtonHightlight());
	pls_properties_add_tips(props, s_control_tip, obs_module_text("timer.timer.control.tip"));
	pls_properties_add_tips(props, s_stopwatch_tip, obs_module_text("timer.timer.stopwatch.tip"));
	pls_properties_add_line(props, s_line_timerControl, "");

	/* font color */
	const static int s_max_length_15 = 15;
	const static int s_max_length_11 = 11;
	p = obs_properties_add_text(props, s_text_current, s_template_start.arg(obs_module_text("timer.text")).toUtf8().constData(), OBS_TEXT_DEFAULT);
	obs_property_set_modified_callback2(p, textDidChangedCallback, s);
	pls_property_set_length_limit(p, s_max_length_15);
	pls_property_set_placeholder(p, QString(obs_module_text("timer.placeholder")).arg(s_max_length_15).toUtf8().constData());

	p = obs_properties_add_text(props, s_text_stream, s_template_start.arg(obs_module_text("timer.text")).toUtf8().constData(), OBS_TEXT_DEFAULT);
	obs_property_set_modified_callback2(p, textDidChangedCallback, s);
	pls_property_set_length_limit(p, s_max_length_11);
	pls_property_set_placeholder(p, QString(obs_module_text("timer.placeholder")).arg(s_max_length_11).toUtf8().constData());

	p = obs_properties_add_text(props, s_text_start, s_template_start.arg(obs_module_text("timer.starting.text")).toUtf8().constData(), OBS_TEXT_DEFAULT);
	obs_property_set_modified_callback2(p, textDidChangedCallback, s);
	pls_property_set_length_limit(p, s_max_length_11);
	pls_property_set_placeholder(p, QString(obs_module_text("timer.placeholder")).arg(s_max_length_11).toUtf8().constData());

	p = obs_properties_add_text(props, s_text_stop, s_template_start.arg(obs_module_text("timer.ending.text")).toUtf8().constData(), OBS_TEXT_DEFAULT);
	obs_property_set_modified_callback2(p, textDidChangedCallback, s);
	pls_property_set_length_limit(p, s_max_length_11);
	pls_property_set_placeholder(p, QString(obs_module_text("timer.placeholder")).arg(s_max_length_11).toUtf8().constData());

	p = obs_properties_add_text(props, s_text_stopwatch, s_template_start.arg(obs_module_text("timer.text")).toUtf8().constData(), OBS_TEXT_DEFAULT);
	obs_property_set_modified_callback2(p, textDidChangedCallback, s);
	pls_property_set_length_limit(p, s_max_length_11);
	pls_property_set_placeholder(p, QString(obs_module_text("timer.placeholder")).arg(s_max_length_11).toUtf8().constData());

	pls_properties_add_font_simple(props, s_font_family, obs_module_text("timer.font"));
	obs_properties_add_color(props, s_font_color, obs_module_text("timer.text.color"));
	pls_properties_add_color_checkbox(props, s_bg_color, obs_module_text("timer.background.color.title"));

	/* timer control */
	/* text */
	p = pls_properties_add_image_group(props, s_list_corlors_23, s->getColorTitle(), 1, 13, PLS_IMAGE_STYLE_BORDER_BUTTON);
	for (int i = 0; i < PLSColorData::instance()->m_vecColors_t23.size(); i++) {
		const auto &_data = PLSColorData::instance()->m_vecColors_t23[i];
		pls_property_image_group_add_item(p, _data.name, _data.path, i, nullptr);
	}

	p = pls_properties_add_image_group(props, s_list_corlors_45, s->getColorTitle(), 1, 13, PLS_IMAGE_STYLE_BORDER_BUTTON);
	for (int i = 0; i < PLSColorData::instance()->m_vecColors_t45.size(); i++) {
		const auto &_data = PLSColorData::instance()->m_vecColors_t45[i];
		pls_property_image_group_add_item(p, _data.name, _data.path, i, nullptr);
	}

	p = pls_properties_add_image_group(props, s_list_corlors_6, s->getColorTitle(), 1, 13, PLS_IMAGE_STYLE_BORDER_BUTTON);
	for (int i = 0; i < PLSColorData::instance()->m_vecColors_t6.size(); i++) {
		const auto &_data = PLSColorData::instance()->m_vecColors_t6[i];
		pls_property_image_group_add_item(p, _data.name, _data.path, i, nullptr);
	}

	obs_properties_add_int_slider(props, s_opacity_bar, obs_module_text("timer.transparency"), 0, 100, 1);

	pls_properties_add_line(props, s_line_font, "");

	return props;
}
static void timer_set_some_part_default(obs_data_t *settings)
{
	auto templateType = static_cast<TemplateType>(obs_data_get_int(settings, s_template_list));

	obs_data_set_string(settings, s_text_current, obs_module_text("timer.current.type.text"));
	obs_data_set_string(settings, s_text_stream, obs_module_text("timer.stream.type.text"));
	obs_data_set_string(settings, s_text_start, obs_module_text("timer.timer.type.text.start"));
	obs_data_set_string(settings, s_text_stop, obs_module_text("timer.timer.type.text.stop"));
	obs_data_set_string(settings, s_text_stopwatch, obs_module_text("timer.stopwatch.type.text"));

	obs_data_t *font_obj = obs_data_create();
	obs_data_set_string(font_obj, "font-family", s_default_font);
	obs_data_set_string(font_obj, "font-weight", "Bold");
	obs_data_set_obj(settings, s_font_family, font_obj);
	obs_data_release(font_obj);

	//normal color and bar
	obs_data_set_int(settings, s_font_color, 0xFFFFFF);

	obs_data_t *bg_color_obj = obs_data_create();
	obs_data_set_int(bg_color_obj, "color_val", 0x000000);
	obs_data_set_bool(bg_color_obj, "is_enable", false);
	obs_data_set_obj(settings, s_bg_color, bg_color_obj);
	obs_data_release(bg_color_obj);

	obs_data_set_int(settings, s_list_corlors_23, templateType == TemplateType::Two ? 2 : 4);
	obs_data_set_int(settings, s_list_corlors_45, templateType == TemplateType::Four ? 1 : 2);
	obs_data_set_int(settings, s_list_corlors_6, 5);
	obs_data_set_int(settings, s_opacity_bar, 100);

	if (templateType == TemplateType::Five || templateType == TemplateType::Six) {
		obs_data_set_int(settings, s_timerType, static_cast<int>(TimerType::Countdown));
	}
}

static void timer_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, s_template_list, static_cast<int>(TemplateType::One));
	obs_data_set_default_int(settings, s_tap, 0);
	obs_data_set_default_int(settings, s_timerType, static_cast<int>(TimerType::Current));

	obs_data_set_default_string(settings, s_text_current, obs_module_text("timer.current.type.text"));
	obs_data_set_default_string(settings, s_text_stream, obs_module_text("timer.stream.type.text"));
	obs_data_set_default_string(settings, s_text_start, obs_module_text("timer.timer.type.text.start"));
	obs_data_set_default_string(settings, s_text_stop, obs_module_text("timer.timer.type.text.stop"));
	obs_data_set_default_string(settings, s_text_stopwatch, obs_module_text("timer.stopwatch.type.text"));
	obs_data_set_default_string(settings, s_listen_list, PLSColorData::instance()->m_vecMusicPath[0].second);

	obs_data_set_default_int(settings, s_checkbox_hms_hour, 0);
	obs_data_set_default_int(settings, s_checkbox_hms_min, 1);
	obs_data_set_default_int(settings, s_checkbox_hms_second, 0);

	obs_data_set_default_int(settings, s_checkbox_ms_min, 1);
	obs_data_set_default_int(settings, s_checkbox_ms_second, 0);

	obs_data_set_default_int(settings, s_checkbox_s_second, s_timer_default_seconds);

	obs_data_set_default_bool(settings, s_listen_list_btn, false);

	obs_data_set_default_bool(settings, s_listen_list_btn_enabeled, false);

	obs_data_set_default_int(settings, s_timer_all_seconds, s_timer_default_seconds);

	//font
	obs_data_t *font_obj = obs_data_create();
	obs_data_set_default_string(font_obj, "font-family", s_default_font);
	obs_data_set_default_string(font_obj, "font-weight", "Bold");
	obs_data_set_default_obj(settings, s_font_family, font_obj);
	obs_data_release(font_obj);

	//normal color and bar
	obs_data_set_default_int(settings, s_font_color, 0xFFFFFFFF);
	obs_data_t *bg_color_obj = obs_data_create();
	obs_data_set_default_int(bg_color_obj, "color_val", 0x000000);
	obs_data_set_default_bool(bg_color_obj, "is_enable", false);
	obs_data_set_default_obj(settings, s_bg_color, bg_color_obj);
	obs_data_release(bg_color_obj);
	obs_data_set_default_int(settings, s_list_corlors_23, 2);
	obs_data_set_default_int(settings, s_list_corlors_45, 1);
	obs_data_set_default_int(settings, s_opacity_bar, 100);

	obs_data_set_default_bool(settings, s_need_force_set_data, true);

	obs_data_set_default_bool(settings, s_cb_enabled, false);
}

static void on_start_cancel_clicked(timer_source *source, const char *function_name)
{
	if (!source || !function_name) {
		return;
	}
	auto timerType = static_cast<TimerType>(obs_data_get_int(source->config.settings, s_timerType));
	if (timerType == TimerType::Current || timerType == TimerType::Live) {
		PLS_INFO(s_moduleName, "timer type not support timer control, ignore this action");
		return;
	}

	// do not call function when button was disable when use hotkey

	bool startEnable = false;
	bool stopEnable = false;
	QString text;
	source->getControlButtonTextAndEnable(0, text, startEnable);
	source->getControlButtonTextAndEnable(1, text, stopEnable);
	if (0 == strcmp(function_name, "timer_start") && startEnable) {
		startButtonClicked(nullptr, nullptr, source);
	} else if (0 == strcmp(function_name, "timer_stop") && stopEnable) {
		stopButtonClicked(nullptr, nullptr, source);
	}
}

static void timer_set_private_data(void *data, obs_data_t *private_data)
{
	if (!data || !private_data) {
		return;
	}
	auto source = static_cast<timer_source *>(data);
	string method = obs_data_get_string(private_data, "method");
	if (method == "start_clicked") {
		on_start_cancel_clicked(source, "timer_start");
	} else if (method == "cancel_clicked") {
		on_start_cancel_clicked(source, "timer_stop");
	} else if (method == "default_properties") {
		source->didClickDefaultsButton();
	}
	return;
}

static void timer_get_private_data(void *data, obs_data_t *private_data)
{
	if (!data || !private_data) {
		return;
	}
	auto source = static_cast<timer_source *>(data);
	const char *method = obs_data_get_string(private_data, "method");
	if (!method)
		return;

	if (0 == strcmp(method, "get_timer_state")) {
		QString startText;
		QString stopText;
		bool startEnable = false;
		bool stopEnable = false;
		source->getControlButtonTextAndEnable(0, startText, startEnable);
		source->getControlButtonTextAndEnable(1, stopText, stopEnable);

		obs_data_set_string(private_data, "start_text", startText.toUtf8().constData());
		obs_data_set_string(private_data, "cancel_text", stopText.toUtf8().constData());
		obs_data_set_bool(private_data, "start_highlight", source->getStartButtonHightlight());
		obs_data_set_bool(private_data, "cancel_highlight", false);
		obs_data_set_bool(private_data, "start_state", startEnable);
		obs_data_set_bool(private_data, "cancel_state", stopEnable);
	} else if (0 == strcmp(method, "get_timer_type")) {
		auto timerType = static_cast<TimerType>(obs_data_get_int(source->config.settings, s_timerType));
		obs_data_set_bool(private_data, "is_timer", timerType == TimerType::Countdown);
		obs_data_set_bool(private_data, "is_stopwatch", timerType == TimerType::Stopwatch);
	}
}

static void start_timer_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	pls_unused(hotkey);
	pls_unused(id);

	if (!data || !pressed)
		return;

	auto source = static_cast<timer_source *>(data);
	on_start_cancel_clicked(source, "timer_start");
}

static void cancel_timer_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	pls_unused(hotkey);
	pls_unused(id);

	if (!data || !pressed)
		return;

	auto source = static_cast<timer_source *>(data);
	on_start_cancel_clicked(source, "timer_stop");
}

void RegisterPRISMSTimerSource()
{
	obs_source_info info = {};
	info.id = "prism_timer_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.icon_type = static_cast<obs_icon_type>(PLS_ICON_TYPE_PRISM_TIMER);
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_CUSTOM_DRAW;
	info.get_name = [](void *data) {
		pls_unused(data);
		return obs_module_text("timer.source.name");
	};
	info.create = [](obs_data_t *settings, obs_source_t *source) {
		auto s = pls_new<timer_source>(source, settings);
		if (obs_data_get_bool(settings, s_need_force_set_data)) {
			obs_data_set_bool(settings, s_need_force_set_data, false);
			timer_set_some_part_default(settings);
		}
		s->config.start_hotkey = obs_hotkey_register_source(source, "TimerSource.Start", obs_module_text("timer.btn.start"), start_timer_hotkey, s);
		s->config.cancel_hotkey = obs_hotkey_register_source(source, "TimerSource.Cancel", obs_module_text("timer.btn.cancel"), cancel_timer_hotkey, s);
		set_hotkey_visible(s);
		return static_cast<void *>(s);
	};
	info.destroy = [](void *data) {
		auto source = static_cast<timer_source *>(data);
		pls_delete(source, nullptr);
	};
	info.update = [](void *data, obs_data_t *settings) {
		pls_unused(settings);
		static_cast<timer_source *>(data)->timer_source_update();
	};
	info.get_width = timer_source_getwidth;
	info.get_height = timer_source_getheight;
	info.video_render = timer_source_render;
	info.video_tick = timer_source_tick;
	info.get_properties = timer_source_getproperties;
	info.get_defaults = timer_source_defaults;

	pls_source_info prism_info = {};
	prism_info.properties_edit_start = [](void *data, obs_data_t *settings) {
		pls_unused(settings);
		static_cast<timer_source *>(data)->propertiesEditStart();
	};
	prism_info.properties_edit_end = [](void *data, obs_data_t *settings, bool is_save_click) {
		pls_unused(settings);
		static_cast<timer_source *>(data)->propertiesEditEnd(is_save_click);
	};
	prism_info.set_private_data = timer_set_private_data;
	prism_info.get_private_data = timer_get_private_data;
	register_pls_source_info(&info, &prism_info);

	obs_register_source(&info);
}

void timer_source::propertiesEditStart()
{
	isPropertiesOpened = true;
	isChangeControlByHand = false;
	obs_data_set_bool(config.settings, s_cb_enabled, false);

	auto templateType = static_cast<TemplateType>(obs_data_get_int(config.settings, s_template_list));
	auto allSeconds = getCountTime(templateType);
	obs_data_set_int(config.settings, s_timer_all_seconds, allSeconds);

	QMetaObject::invokeMethod(
		this, [this]() { updateControlButtons(config.mStaus, nullptr, true); }, Qt::QueuedConnection);
}

void timer_source::propertiesEditEnd(bool isSaveClick)
{
	if (!isPropertiesOpened) {
		return;
	}
	isPropertiesOpened = false;
	obs_data_set_bool(config.settings, s_cb_enabled, false);
	resetTextToDefaultWhenEmpty();
	if (isChangeControlByHand && !isSaveClick) {
		if (MusicStatus::Noraml != config.mStaus) {
			updateControlButtons(MusicStatus::Noraml, nullptr, false, false);
			dispatahControlJsToWeb();
		}
	}
	if (isSaveClick) {
		sendAnalogWhenClickSavedBtn();
	}
	if (!config.sub_music) {
		return;
	}

	bool isChecked = obs_data_get_bool(config.settings, s_listen_list_btn);
	if (isChecked) {
		obs_data_set_bool(config.settings, s_listen_list_btn, !isChecked);
	}
	if (config.sub_music) {
		signal_handler_disconnect(obs_source_get_signal_handler(config.sub_music), "media_play", mediaStateChanged, this);
		signal_handler_disconnect(obs_source_get_signal_handler(config.sub_music), "media_pause", mediaStateChanged, this);
		signal_handler_disconnect(obs_source_get_signal_handler(config.sub_music), "media_restart", mediaStateChanged, this);
		signal_handler_disconnect(obs_source_get_signal_handler(config.sub_music), "media_started", mediaStateChanged, this);
		signal_handler_disconnect(obs_source_get_signal_handler(config.sub_music), "media_ended", mediaStateChanged, this);
		signal_handler_disconnect(obs_source_get_signal_handler(config.sub_music), "media_stopped", mediaStateChanged, this);
		obs_source_media_stop(config.sub_music);
		obs_source_dec_active(config.sub_music);
		obs_source_release(config.sub_music);
		config.sub_music = nullptr;
	}
}

void timer_source::updateOKButtonEnable()
{
	auto templateType = static_cast<TemplateType>(obs_data_get_int(config.settings, s_template_list));
	auto timerType = static_cast<TimerType>(obs_data_get_int(config.settings, s_timerType));
	bool isOkEnable = true;
	if (templateType == TemplateType::Four) {

		if (timerType == TimerType::Current) {

			const char *text = obs_data_get_string(config.settings, s_text_current);
			if (QString(text).trimmed().isEmpty()) {
				isOkEnable = false;
			}
		} else if (timerType == TimerType::Live) {

			const char *text = obs_data_get_string(config.settings, s_text_stream);
			if (QString(text).trimmed().isEmpty()) {
				isOkEnable = false;
			}
		} else if (timerType == TimerType::Countdown) {
			const char *startText = obs_data_get_string(config.settings, s_text_start);
			const char *stopText = obs_data_get_string(config.settings, s_text_stop);
			if (QString(startText).trimmed().isEmpty() || QString(stopText).trimmed().isEmpty()) {
				isOkEnable = false;
			}
		} else if (timerType == TimerType::Stopwatch) {

			const char *text = obs_data_get_string(config.settings, s_text_stopwatch);
			if (QString(text).trimmed().isEmpty()) {
				isOkEnable = false;
			}
		}
	}
	pls_source_properties_view_ok_button_enable(config.source, isOkEnable);
}

void timer_source::resetCountdownIfNeed(obs_data_t *settings)
{
	auto templateType = static_cast<TemplateType>(obs_data_get_int(config.settings, s_template_list));

	auto timerSeconds = obs_data_get_int(settings, s_timer_all_seconds);
	long long h = 0;
	long long m = 0;
	long long s = 0;

	switch (templateType) {
	case TemplateType::One:
	case TemplateType::Two:
	case TemplateType::Three:
	case TemplateType::Four:
		int_to_hour_min_second(min(int_from_hour_min_second(s_timer_normal_max_h, s_timer_normal_max_m, s_timer_normal_max_s), timerSeconds), h, m, s);
		obs_data_set_int(settings, s_checkbox_hms_hour, h);
		obs_data_set_int(settings, s_checkbox_hms_min, m);
		obs_data_set_int(settings, s_checkbox_hms_second, s);
		break;
	case TemplateType::Five:
		if (int_from_hour_min_second(0, s_timer_normal_max_m, s_timer_normal_max_s) < timerSeconds) {
			m = 1;
		} else {
			int_to_hour_min_second(timerSeconds, h, m, s);
		}
		obs_data_set_int(settings, s_checkbox_ms_min, m);
		obs_data_set_int(settings, s_checkbox_ms_second, s);

		break;
	case TemplateType::Six:
		if (s_timer_6_max_s < timerSeconds) {
			s = s_timer_default_seconds;
		} else {
			s = timerSeconds;
		}
		obs_data_set_int(settings, s_checkbox_s_second, s);
		break;
	default:
		break;
	}

	obs_data_set_int(settings, s_timer_all_seconds, int_from_hour_min_second(h, m, s));
}

void timer_source::updateWebAudioShow(bool isShow)
{
	if (obs_source_audio_active(config.source) == isShow) {
		return;
	}
	obs_source_set_audio_active(config.source, isShow);
}

bool timer_source::getStartButtonHightlight() const
{
	return config.mStaus != MusicStatus::Noraml;
}

bool timer_source::getListenButtonEnable()
{
	auto pathName = obs_data_get_string(config.settings, s_listen_list);
	if (QString::compare("", pathName) == 0) {
		return false;
	}
	return true;
}

const char *timer_source::getColorTitle()
{
	auto templateType = static_cast<TemplateType>(obs_data_get_int(config.settings, s_template_list));
	const char *color_title = nullptr;
	switch (templateType) {
	case TemplateType::One:
		color_title = obs_module_text("timer.text.color");
		break;
	case TemplateType::Two:
		color_title = obs_module_text("timer.background.color.title");
		break;
	case TemplateType::Three:
		color_title = obs_module_text("timer.text.color");
		break;
	case TemplateType::Four:
		color_title = obs_module_text("timer.background.color.title");
		break;
	case TemplateType::Five:
		color_title = obs_module_text("timer.text.color");
		break;
	case TemplateType::Six:
		color_title = obs_module_text("timer.text.color");
		break;
	default:
		break;
	}
	return color_title;
}

PLSColorData *PLSColorData::instance()
{
	static PLSColorData _instance;
	if (_instance.m_vecColors_t23.empty()) {
		_instance.initColorDatas();
	}
	return &_instance;
}

void PLSColorData::initColorDatas()
{
	m_vecColors_t23 = {{"color_0", ":/timer-source/resource/images/img-audiov-color-03.svg", "#06f7d8"},  {"color_1", ":/timer-source/resource/images/img-audiov-color-04.svg", "#01a5f7"},
			   {"color_2", ":/timer-source/resource/images/img-audiov-color-05.svg", "#3c5fff"},  {"color_3", ":/timer-source/resource/images/img-widget-color-01.svg", "#6fd96f"},
			   {"color_4", ":/timer-source/resource/images/img-widget-color-04.svg", "#ff4d4d"},  {"color_5", ":/timer-source/resource/images/img-audiov-color-11.svg", "#f61365"},
			   {"color_6", ":/timer-source/resource/images/img-audiov-color-12.svg", "#fc40ac"},  {"color_7", ":/timer-source/resource/images/img-audiov-color-13.svg", "#db06f6"},
			   {"color_8", ":/timer-source/resource/images/img-audiov-color-06.svg", "#6940fc"},  {"color_10", ":/timer-source/resource/images/img-widget-color-03.svg", "#293f70"},
			   {"color_10", ":/timer-source/resource/images/img-widget-color-02.svg", "#009991"}, {"color_11", ":/timer-source/resource/images/img-audiov-color-02.svg", "#000000"},
			   {"color_12", ":/timer-source/resource/images/img-audiov-color-01.svg", "#FFFFFF"}};

	m_vecColors_t45 = {{"color_0", ":/timer-source/resource/images/img-widget-color-05.svg", "#F662A5,#7026FD"},
			   {"color_1", ":/timer-source/resource/images/img-widget-color-06.svg", "#DF83CF,#B385E0,#A084EB,#9691E5,#80C5EE"},
			   {"color_2", ":/timer-source/resource/images/img-widget-color-07.svg", "#4E7CD4,#5677D5,#5578D5,#4891CD,#33AFBA,#2FBFBA"},
			   {"color_3", ":/timer-source/resource/images/img-widget-color-08.svg", "#7AFCA5,#ADDE5E,#F7CB55"},
			   {"color_4", ":/timer-source/resource/images/img-audiov-color-03.svg", "#06f7d8"},
			   {"color_5", ":/timer-source/resource/images/img-audiov-color-04.svg", "#01a5f7"},
			   {"color_6", ":/timer-source/resource/images/img-audiov-color-05.svg", "#3c5fff"},
			   {"color_7", ":/timer-source/resource/images/img-audiov-color-11.svg", "#f61365"},
			   {"color_8", ":/timer-source/resource/images/img-audiov-color-12.svg", "#fc40ac"},
			   {"color_10", ":/timer-source/resource/images/img-audiov-color-13.svg", "#db06f6"},
			   {"color_10", ":/timer-source/resource/images/img-audiov-color-06.svg", "#6940fc"},
			   {"color_11", ":/timer-source/resource/images/img-audiov-color-02.svg", "#000000"},
			   {"color_12", ":/timer-source/resource/images/img-audiov-color-01.svg", "#FFFFFF"}};

	m_vecColors_t6 = {{"color_0", ":/timer-source/resource/images/img-audiov-color-03.svg", "#06f7d8"},    {"color_1", ":/timer-source/resource/images/img-audiov-color-04.svg", "#01a5f7"},
			  {"color_2", ":/timer-source/resource/images/img-audiov-color-05.svg", "#3c5fff"},    {"color_3", ":/timer-source/resource/images/img-widget-color-01.svg", "#6fd96f"},
			  {"color_4", ":/timer-source/resource/images/img-countdown-color-01.svg", "#fef13d"}, {"color_5", ":/timer-source/resource/images/img-countdown-color-02.svg", "#eda936"},
			  {"color_6", ":/timer-source/resource/images/img-widget-color-04.svg", "#ff4d4d"},    {"color_8", ":/timer-source/resource/images/img-audiov-color-11.svg", "#f61365"},
			  {"color_8", ":/timer-source/resource/images/img-audiov-color-12.svg", "#fc40ac"},    {"color_9", ":/timer-source/resource/images/img-audiov-color-13.svg", "#db06f6"},
			  {"color_10", ":/timer-source/resource/images/img-audiov-color-06.svg", "#6940fc"},   {"color_11", ":/timer-source/resource/images/img-audiov-color-02.svg", "#000000"},
			  {"color_12", ":/timer-source/resource/images/img-audiov-color-01.svg", "#FFFFFF"}};

	m_vecMusicPath = {{obs_module_text("timer.no.sound.effect"), ""},
			  {obs_module_text("timer.classic"), "1_Classic.wav"},
			  {obs_module_text("timer.digital"), "2_Digital.wav"},
			  {obs_module_text("timer.buzzer"), "3_Buzzer.wav"},
			  {obs_module_text("timer.button"), "4_Button.wav"},
			  {obs_module_text("timer.clapping"), "5_Clapping.wav"},
			  {obs_module_text("timer.dog"), "6_Dog.wav"},
			  {obs_module_text("timer.doorbell"), "7_Doorbell.wav"},
			  {obs_module_text("timer.organ"), "8_Organ.wav"},
			  {obs_module_text("timer.magic.wand"), "9_Magic Wand.wav"},
			  {obs_module_text("timer.music.box"), "10_Music Box.wav"},
			  {obs_module_text("timer.piano"), "11_Piano.wav"},
			  {obs_module_text("timer.ta-da"), "12_Ta-da.wav"},
			  {obs_module_text("timer.wave"), "13_Wave.wav"}};
}
