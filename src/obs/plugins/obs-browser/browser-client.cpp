/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>
 Copyright (C) 2018 by Hugh Bailey ("Jim") <jim@obsproject.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include "browser-client.hpp"
#include "obs-browser-source.hpp"
#include "base64/base64.hpp"
#include "json11/json11.hpp"
#include <obs-frontend-api.h>
#include <obs.hpp>
#include <util/platform.h>
#include <map>

using namespace json11;

//PRISM/Wangshaohui/20200917/#3714/for check client
std::mutex lock_objects;
std::map<void *, bool> valid_objects;

//PRISM/Wangshaohui/20200917/#3714/for check client
void SetObjectValid(void *pointer, bool valid)
{
	std::lock_guard<std::mutex> auto_lock(lock_objects);
	valid_objects[pointer] = valid;
}

//PRISM/Wangshaohui/20200917/#3714/for check client
bool IsObjectValid(void *pointer)
{
	bool valid = false;

	{
		std::lock_guard<std::mutex> auto_lock(lock_objects);
		valid = valid_objects[pointer];
	}

	if (!valid) {
		plog(LOG_WARNING, "Invalid object for BrowserSource:%p",
		     pointer);
	}

	return valid;
}

BrowserClient::~BrowserClient()
{
	//PRISM/Wangshaohui/20200813/#3784/for cef interaction
	plog(LOG_INFO, "Destructure function of BrowserClient:%p", this);

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED && USE_TEXTURE_COPY
	if (sharing_available) {
		obs_enter_graphics();
		gs_texture_destroy(texture);
		obs_leave_graphics();
	}
#endif
}

//PRISM/Wangshaohui/20200811/#3784/for cef interaction
CefRefPtr<CefRequestHandler> BrowserClient::GetRequestHandler()
{
	return this;
}

CefRefPtr<CefLoadHandler> BrowserClient::GetLoadHandler()
{
	return this;
}

CefRefPtr<CefRenderHandler> BrowserClient::GetRenderHandler()
{
	return this;
}

CefRefPtr<CefDisplayHandler> BrowserClient::GetDisplayHandler()
{
	return this;
}

CefRefPtr<CefLifeSpanHandler> BrowserClient::GetLifeSpanHandler()
{
	return this;
}

CefRefPtr<CefContextMenuHandler> BrowserClient::GetContextMenuHandler()
{
	return this;
}

#if CHROME_VERSION_BUILD >= 3683
CefRefPtr<CefAudioHandler> BrowserClient::GetAudioHandler()
{
	return reroute_audio ? this : nullptr;
}
#endif

//PRISM/Wangshaohui/20200811/#3784/for cef interaction
//#define ENABLE_URL_JUMP
bool BrowserClient::OnOpenURLFromTab(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
	const CefString &target_url,
	CefRequestHandler::WindowOpenDisposition target_disposition,
	bool user_gesture)
{
#ifdef ENABLE_URL_JUMP
	frame->LoadURL(target_url);
#endif
	return true;
}

bool BrowserClient::OnBeforePopup(CefRefPtr<CefBrowser>,
				  CefRefPtr<CefFrame> frame,
				  const CefString &target_url,
				  const CefString &,
				  CefLifeSpanHandler::WindowOpenDisposition,
				  bool, const CefPopupFeatures &,
				  CefWindowInfo &, CefRefPtr<CefClient> &,
				  CefBrowserSettings &,
#if CHROME_VERSION_BUILD >= 3770
				  CefRefPtr<CefDictionaryValue> &,
#endif
				  bool *)
{
	//PRISM/Wangshaohui/20200811/#3784/for cef interaction
#ifdef ENABLE_URL_JUMP
	frame->LoadURL(target_url);
#endif
	return true;
}

void BrowserClient::OnBeforeContextMenu(CefRefPtr<CefBrowser>,
					CefRefPtr<CefFrame>,
					CefRefPtr<CefContextMenuParams>,
					CefRefPtr<CefMenuModel> model)
{
	/* remove all context menu contributions */
	model->Clear();
}

bool BrowserClient::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
#if CHROME_VERSION_BUILD >= 3770
	CefRefPtr<CefFrame>,
#endif
	CefProcessId, CefRefPtr<CefProcessMessage> message)
{
	const std::string &name = message->GetName();
	Json json;

	//PRISM/Wangshaohui/20200917/#3714/for check client
	if (!bs || !IsObjectValid(bs)) {
		return false;
	}

	if (name == "getCurrentScene") {
		OBSSource current_scene = obs_frontend_get_current_scene();
		obs_source_release(current_scene);

		if (!current_scene)
			return false;

		const char *name = obs_source_get_name(current_scene);
		if (!name)
			return false;

		json = Json::object{
			{"name", name},
			{"width", (int)obs_source_get_width(current_scene)},
			{"height", (int)obs_source_get_height(current_scene)}};

	} else if (name == "getStatus") {
		json = Json::object{
			{"recording", obs_frontend_recording_active()},
			{"streaming", obs_frontend_streaming_active()},
			{"replaybuffer", obs_frontend_replay_buffer_active()}};

	} //PRISM/ChengBing/20201215/add web log info
	else if (name == "sendToPrism") {
		std::string result("{}");
		const std::string &param =
			message->GetArgumentList()->GetString(0);
		if (nullptr != prism_frontend_web_invoked) {
			result = prism_frontend_web_invoked(param.c_str());
		}
		bs->receiveWebMessage(param.c_str());

	} else {
		return false;
	}

	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create("executeCallback");

	CefRefPtr<CefListValue> args = msg->GetArgumentList();
	args->SetInt(0, message->GetArgumentList()->GetInt(0));
	args->SetString(1, json.dump());

	SendBrowserProcessMessage(browser, PID_RENDERER, msg);

	return true;
}
#if CHROME_VERSION_BUILD >= 3578
void BrowserClient::GetViewRect(
#else
bool BrowserClient::GetViewRect(
#endif
	CefRefPtr<CefBrowser>, CefRect &rect)
{
	//PRISM/Wangshaohui/20200917/#3714/for check client
	if (!bs || !IsObjectValid(bs)) {
#if CHROME_VERSION_BUILD >= 3578
		rect.Set(0, 0, 16, 16);
		return;
#else
		return false;
#endif
	}

	rect.Set(0, 0, bs->width < 1 ? 1 : bs->width,
		 bs->height < 1 ? 1 : bs->height);
#if CHROME_VERSION_BUILD >= 3578
	return;
#else
	return true;
#endif
}

//PRISM/Wangshaohui/20200811/#3784/for cef interaction
bool BrowserClient::GetScreenPoint(CefRefPtr<CefBrowser> browser, int viewX,
				   int viewY, int &screenX, int &screenY)
{
	auto interaction = interaction_weak.lock();
	if (interaction != NULL) {
		HWND hWnd = interaction->GetInteractionView();
		if (::IsWindow(hWnd)) {
			int source_cx_;
			int source_cy_;
			CefRefPtr<CefBrowser> cefBrowser;
			interaction->GetInteractionInfo(source_cx_, source_cy_,
							cefBrowser);

			RECT rc;
			GetClientRect(hWnd, &rc);

			int left, top;
			float scale;
			GetScaleAndCenterPos(source_cx_, source_cy_,
					     RectWidth(rc), RectHeight(rc),
					     left, top, scale);

			float clientX = left + float(viewX) * scale;
			float clientY = top + float(viewY) * scale;

			POINT screen_pt = {clientX, clientY};
			::ClientToScreen(hWnd, &screen_pt);

			screenX = screen_pt.x;
			screenY = screen_pt.y;

			return true;
		}
	}

	return false;
}

void BrowserClient::OnPaint(CefRefPtr<CefBrowser>, PaintElementType type,
			    const RectList &, const void *buffer, int width,
			    int height)
{
	if (type != PET_VIEW) {
		return;
	}

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
	if (sharing_available) {
		return;
	}
#endif

	//PRISM/Wangshaohui/20200917/#3714/for check client
	if (!bs || !IsObjectValid(bs)) {
		return;
	}

	if (bs->width != width || bs->height != height) {
		obs_enter_graphics();
		bs->DestroyTextures();
		obs_leave_graphics();
	}

	if (!bs->texture && width && height) {
		obs_enter_graphics();
		bs->texture = gs_texture_create(width, height, GS_BGRA, 1,
						(const uint8_t **)&buffer,
						GS_DYNAMIC);
		bs->width = width;
		bs->height = height;
		obs_leave_graphics();
	} else {
		obs_enter_graphics();
		gs_texture_set_image(bs->texture, (const uint8_t *)buffer,
				     width * 4, false);
		obs_leave_graphics();
	}
}

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
void BrowserClient::OnAcceleratedPaint(CefRefPtr<CefBrowser>, PaintElementType,
				       const RectList &, void *shared_handle)
{
	//PRISM/Wangshaohui/20200917/#3714/for check client
	if (!bs || !IsObjectValid(bs)) {
		return;
	}

	if (shared_handle != last_handle) {
		obs_enter_graphics();
#if USE_TEXTURE_COPY
		gs_texture_destroy(texture);
		texture = nullptr;
#endif
		gs_texture_destroy(bs->texture);
		bs->texture = nullptr;

#if USE_TEXTURE_COPY
		texture = gs_texture_open_shared(
			(uint32_t)(uintptr_t)shared_handle);

		uint32_t cx = gs_texture_get_width(texture);
		uint32_t cy = gs_texture_get_height(texture);
		gs_color_format format = gs_texture_get_color_format(texture);

		bs->texture = gs_texture_create(cx, cy, format, 1, nullptr, 0);
#else
		bs->texture = gs_texture_open_shared(
			(uint32_t)(uintptr_t)shared_handle);
#endif
		obs_leave_graphics();

		last_handle = shared_handle;
	}

#if USE_TEXTURE_COPY
	if (texture && bs->texture) {
		obs_enter_graphics();
		gs_copy_texture(bs->texture, texture);
		obs_leave_graphics();
	}
#endif
}
#endif

//PRISM/Wangshaohui/20200811/#3784/for cef interaction
void BrowserClient::OnCursorChange(CefRefPtr<CefBrowser> browser,
				   CefCursorHandle cursor, CursorType type,
				   const CefCursorInfo &custom_cursor_info)
{
	DCHECK(CefCurrentlyOn(TID_UI));
	auto interaction = interaction_weak.lock();
	if (interaction != NULL) {
		HWND hWnd = interaction->GetInteractionView();
		if (::IsWindow(hWnd) && ::IsWindowVisible(hWnd)) {
			SetClassLongPtr(
				hWnd, GCLP_HCURSOR,
				static_cast<LONG>(
					reinterpret_cast<LONG_PTR>(cursor)));
			SetCursor(cursor);
		}
	}
}

//PRISM/Wangshaohui/20200811/#3784/for cef interaction
void BrowserClient::OnImeCompositionRangeChanged(
	CefRefPtr<CefBrowser> browser, const CefRange &selection_range,
	const CefRenderHandler::RectList &character_bounds)
{
	DCHECK(CefCurrentlyOn(TID_UI));
	auto interaction = interaction_weak.lock();
	if (interaction != NULL) {
		interaction->OnImeCompositionRangeChanged(
			browser, selection_range, character_bounds);
	}
}

#if CHROME_VERSION_BUILD >= 3683
static speaker_layout GetSpeakerLayout(CefAudioHandler::ChannelLayout cefLayout)
{
	switch (cefLayout) {
	case CEF_CHANNEL_LAYOUT_MONO:
		return SPEAKERS_MONO; /**< Channels: MONO */
	case CEF_CHANNEL_LAYOUT_STEREO:
		return SPEAKERS_STEREO; /**< Channels: FL, FR */
	case CEF_CHANNEL_LAYOUT_2POINT1:
		return SPEAKERS_2POINT1; /**< Channels: FL, FR, LFE */
	case CEF_CHANNEL_LAYOUT_2_2:
	case CEF_CHANNEL_LAYOUT_QUAD:
	case CEF_CHANNEL_LAYOUT_4_0:
		return SPEAKERS_4POINT0; /**< Channels: FL, FR, FC, RC */
	case CEF_CHANNEL_LAYOUT_4_1:
		return SPEAKERS_4POINT1; /**< Channels: FL, FR, FC, LFE, RC */
	case CEF_CHANNEL_LAYOUT_5_1:
	case CEF_CHANNEL_LAYOUT_5_1_BACK:
		return SPEAKERS_5POINT1; /**< Channels: FL, FR, FC, LFE, RL, RR */
	case CEF_CHANNEL_LAYOUT_7_1:
	case CEF_CHANNEL_LAYOUT_7_1_WIDE_BACK:
	case CEF_CHANNEL_LAYOUT_7_1_WIDE:
		return SPEAKERS_7POINT1; /**< Channels: FL, FR, FC, LFE, RL, RR, SL, SR */
	}
	return SPEAKERS_UNKNOWN;
}

void BrowserClient::OnAudioStreamStarted(CefRefPtr<CefBrowser> browser, int id,
					 int, ChannelLayout channel_layout,
					 int sample_rate, int)
{
	//PRISM/Wangshaohui/20200917/#3714/for check client
	if (!bs || !IsObjectValid(bs)) {
		return;
	}

	AudioStream &stream = bs->audio_streams[id];
	//PRISM/WangShaohui/20210406/#7680/UI block
	//if (!stream.source) {
	//	stream.source = obs_source_create_private("audio_line", nullptr,
	//						  nullptr);
	//	obs_source_release(stream.source);

	//	obs_source_add_active_child(bs->source, stream.source);

	//	std::lock_guard<std::mutex> lock(bs->audio_sources_mutex);
	//	bs->audio_sources.push_back(stream.source);
	//}
	CheckAudioSource(stream);

	stream.speakers = GetSpeakerLayout(channel_layout);
	stream.channels = get_audio_channels(stream.speakers);
	stream.sample_rate = sample_rate;
}

void BrowserClient::OnAudioStreamPacket(CefRefPtr<CefBrowser> browser, int id,
					const float **data, int frames,
					int64_t pts)
{
	//PRISM/Wangshaohui/20200917/#3714/for check client
	if (!bs || !IsObjectValid(bs)) {
		return;
	}

	AudioStream &stream = bs->audio_streams[id];

	//PRISM/WangShaohui/20210406/#7680/UI block
	if (!CheckAudioSource(stream)) {
		return;
	}

	struct obs_source_audio audio = {};

	const uint8_t **pcm = (const uint8_t **)data;
	for (int i = 0; i < stream.channels; i++)
		audio.data[i] = pcm[i];

	audio.samples_per_sec = stream.sample_rate;
	audio.frames = frames;
	audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
	audio.speakers = stream.speakers;
	audio.timestamp = (uint64_t)pts * 1000000LLU;

	obs_source_output_audio(stream.source, &audio);
}

void BrowserClient::OnAudioStreamStopped(CefRefPtr<CefBrowser> browser, int id)
{
	//PRISM/Wangshaohui/20200917/#3714/for check client
	if (!bs || !IsObjectValid(bs)) {
		return;
	}

	auto pair = bs->audio_streams.find(id);
	if (pair == bs->audio_streams.end()) {
		return;
	}

	AudioStream &stream = pair->second;
	{
		std::lock_guard<std::mutex> lock(bs->audio_sources_mutex);
		for (size_t i = 0; i < bs->audio_sources.size(); i++) {
			obs_source_t *source = bs->audio_sources[i];
			if (source == stream.source) {
				bs->audio_sources.erase(
					bs->audio_sources.begin() + i);
				break;
			}
		}
	}
	bs->audio_streams.erase(pair);
}
#endif

void BrowserClient::OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
			      int httpStatusCode)
{
	//PRISM/Wangshaohui/20200917/#3714/for check client
	if (!bs || !IsObjectValid(bs)) {
		return;
	}

	//PRISM/Zhangdewen/20200901/#for chat source
	// chat source: The event needs to be sent after the page is loaded
	bs->onBrowserLoadEnd();

	if (frame->IsMain()) {
		if (!bs->css.empty()) {
			std::string uriEncodedCSS =
				CefURIEncode(bs->css, false).ToString();

			std::string script;
			script +=
				"const obsCSS = document.createElement('style');";
			script += "obsCSS.innerHTML = decodeURIComponent(\"" +
				  uriEncodedCSS + "\");";
			script +=
				"document.querySelector('head').appendChild(obsCSS);";

			frame->ExecuteJavaScript(script, "", 0);
		}

		//PRISM/WangShaohui/20200310/#1332/adding logs for exceptions
		if (httpStatusCode != 200 && httpStatusCode != ERR_NONE &&
		    httpStatusCode != ERR_ABORTED) {
			plog(LOG_WARNING,
			     "obs-browser: OnLoadEnd Exception httpStatus:%d",
			     httpStatusCode);
		}
	}
}

//PRISM/WangShaohui/20200310/#1332/adding logs for exceptions
void BrowserClient::OnLoadError(CefRefPtr<CefBrowser> browser,
				CefRefPtr<CefFrame> frame,
				CefLoadHandler::ErrorCode errorCode,
				const CefString &errorText,
				const CefString &failedUrl)
{
	if (frame->IsMain() && errorCode != ERR_NONE &&
	    errorCode != ERR_ABORTED) {
		plog(LOG_WARNING,
		     "obs-browser: OnLoadError errorCode:%d errorText:%s",
		     errorCode, errorText.ToString().c_str());

		//PRISM/Zhangdewen/20201102/#5550/for chat source
		extern void browserLoadError(
			BrowserSource * bs,
			CefLoadHandler::ErrorCode errorCode);
		browserLoadError(bs, errorCode);
	}
}

//PRISM/WangShaohui/20210406/#7680/UI block
bool BrowserClient::CheckAudioSource(AudioStream &stream)
{
	if (stream.source) {
		return true;
	}

	if (obs_get_source_is_loading()) {
		return false;
	} else {
		stream.source = obs_source_create_private("audio_line", nullptr,
							  nullptr);
		obs_source_release(stream.source);

		obs_source_add_active_child(bs->source, stream.source);

		std::lock_guard<std::mutex> lock(bs->audio_sources_mutex);
		bs->audio_sources.push_back(stream.source);

		return true;
	}
}

//PRISM/Wangshaohui/20210114/noIssue/save CEF's log level
int TransCefLogLevel(cef_log_severity_t level)
{
	switch (level) {
	case LOGSEVERITY_FATAL:
		return LOG_ERROR;

	case LOGSEVERITY_ERROR:
	case LOGSEVERITY_WARNING:
		return LOG_WARNING;

	case LOGSEVERITY_DEBUG:
		return LOG_DEBUG;

	default:
		return LOG_INFO;
	}
}

bool BrowserClient::OnConsoleMessage(CefRefPtr<CefBrowser>,
#if CHROME_VERSION_BUILD >= 3282
				     cef_log_severity_t level,
#endif
				     const CefString &message,
				     const CefString &source, int line)
{
#if CHROME_VERSION_BUILD >= 3282
	if (level < LOGSEVERITY_ERROR)
		return false;
#endif

	//PRISM/Wangshaohui/20210114/noIssue/save CEF's log level
	//PRISM/Wangshaohui/20210913/#9672/send log to KR nelo
	blogex(true, TransCefLogLevel(level), NULL, 0,
	       "[CEF message] obs-browser: %s (source: %s:%d)",
	       message.ToString().c_str(), source.ToString().c_str(), line);
	return false;
}
