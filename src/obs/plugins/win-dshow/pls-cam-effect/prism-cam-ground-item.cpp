#include "prism-cam-ground-item.h"
#include "obs-module.h"
#include <util/threading.h>
#include <util/platform.h>

static const float FLOAT_NEAR_ZERO = 0.0001f;

void media_state_changed(void *data, calldata_t *calldata)
{
	if (!data) {
		return;
	}
	PLSCamGroundItem *item = reinterpret_cast<PLSCamGroundItem *>(data);
	obs_source_t *media_source =
		(obs_source_t *)calldata_ptr(calldata, "source");
	if (media_source == item->video_source) {
		obs_media_state state =
			obs_source_media_get_state(media_source);
		if (state == OBS_MEDIA_STATE_ERROR) {
			plog(LOG_INFO, "Ground item state error:%d", state);
			item->exception_callback(
				item->pls_cam_effect,
				OBS_SOURCE_EXCEPTION_BG_FILE_ERROR, 0);
		} else if (state == OBS_MEDIA_STATE_PLAYING) {
			CAutoLockCS autoLock(item->item_lock);
			item->video_playing = true;
		}
	}
}

void media_network_changed(void *data, calldata_t *calldata)
{
	if (!data) {
		return;
	}
	PLSCamGroundItem *item = reinterpret_cast<PLSCamGroundItem *>(data);
	obs_source_t *media_source =
		(obs_source_t *)calldata_ptr(calldata, "source");
	bool network_off = calldata_bool(calldata, "network_off");
	if (media_source == item->video_source && network_off) {
		if (item->IsVideoFileNotExist()) {
			plog(LOG_INFO,
			     "Background file error when network off!");
			item->exception_callback(
				item->pls_cam_effect,
				OBS_SOURCE_EXCEPTION_BG_FILE_NETWORK_ERROR, 0);
		}
	}
}

PLSCamGroundItem::PLSCamGroundItem(obs_source_t *dshow_source, void *cam_effect,
				   ground_exception cb)
	: item_source(nullptr),
	  item_texture(nullptr),
	  str_guid(""),
	  item_render_effect(nullptr),
	  item_render_tech(nullptr),
	  param_h_flip(nullptr),
	  param_v_flip(nullptr),
	  param_image(nullptr),
	  image_source(nullptr),
	  video_source(nullptr),
	  exception_callback(cb),
	  need_video_pause(false),
	  need_video_play(false),
	  private_data_update(false),
	  item_texture_clear(false),
	  parent_source(dshow_source),
	  pls_cam_effect(cam_effect),
	  auto_fit(false),
	  video_playing(false)
{
	memset(&pre_rt_target, 0, sizeof(pre_rt_target));
}

PLSCamGroundItem::~PLSCamGroundItem()
{
	if (image_source) {
		obs_source_release(image_source);
	}

	if (video_source) {
		obs_source_dec_active(video_source);
		signal_handler_disconnect(
			obs_source_get_signal_handler(video_source),
			"media_state_changed", media_state_changed, this);
		signal_handler_disconnect(
			obs_source_get_signal_handler(video_source),
			"network_changed", media_network_changed, this);
		obs_source_release(video_source);
	}

	item_source = nullptr;

	obs_enter_graphics();
	if (item_texture) {
		gs_texture_destroy(item_texture);
		item_texture = nullptr;
	}

	if (item_render_effect) {
		gs_effect_destroy(item_render_effect);
		item_render_effect = nullptr;
	}
	obs_leave_graphics();
}

obs_source_t *PLSCamGroundItem::CreateItemSource(ITEM_TYPE type,
						 const std::string &file_path)
{
	obs_source_t *new_source = nullptr;
	if (type == ITEM_TYPE_VIDEO) {
		str_guid = GenerateGuid();
		new_source = obs_source_create_private(
			"ffmpeg_source", str_guid.c_str(), nullptr);
		if (!new_source) {
			plog(LOG_WARNING, "Create ffmpeg source failed");
			return nullptr;
		}
		obs_source_inc_active(new_source);

		signal_handler_connect_ref(
			obs_source_get_signal_handler(new_source),
			"media_state_changed", media_state_changed, this);
		signal_handler_connect_ref(
			obs_source_get_signal_handler(new_source),
			"network_changed", media_network_changed, this);
	} else if (type == ITEM_TYPE_IMAGE) {
		str_guid = GenerateGuid();
		new_source = obs_source_create_private(
			"image_source", str_guid.c_str(), nullptr);
		if (!new_source) {
			plog(LOG_WARNING, "Create image source failed");
			return nullptr;
		}
	} else {
		plog(LOG_WARNING, "item type invalid");
	}
	return new_source;
}

void PLSCamGroundItem::RenderItemToRT(gs_texture *rt_tex, bool flip)
{
	CAutoLockCS autoLock(item_lock);
	if (need_video_pause) {
		obs_source_media_stop(video_source);
		need_video_pause = false;
	}

	if (need_video_play) {
		obs_source_media_restart(video_source);
		need_video_play = false;
	}

	if (item_texture_clear) {
		ClearItemTexture();
		item_texture_clear = false;
		//source update delayed, need to skip one frame to refresh image texture
		return;
	}

	if (!item_data.enable) {
		return;
	}

	uint32_t width = obs_source_get_width(item_source);
	uint32_t height = obs_source_get_height(item_source);
	if (width <= 0 || height <= 0)
		return;

	if (item_texture) {
		uint32_t tex_width = gs_texture_get_width(item_texture);
		uint32_t tex_height = gs_texture_get_height(item_texture);
		if (tex_width != width || tex_height != height) {
			gs_texture_destroy(item_texture);
			item_texture = nullptr;
		}
	}
	if (!item_texture) {
		item_texture = gs_texture_create(width, height, GS_RGBA, 1,
						 NULL, GS_RENDER_TARGET);
	}

	//render item to item_texture
	gs_viewport_push();
	gs_projection_push();
	gs_blend_state_push();

	if (item_data.item_type == ITEM_TYPE_VIDEO &&
	    item_source == video_source) {
		if (obs_source_is_async_active(item_source) && video_playing) {
			gs_set_render_target(rt_tex, NULL);
			struct vec4 clear_color;
			vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);
			gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);
		}
	}

	gs_set_render_target(item_texture, NULL);
	SetRenderSize(0, 0, width, height);
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	obs_source_video_render(item_source);

	gs_blend_state_pop();
	gs_projection_pop();
	gs_viewport_pop();

	RenderItemTextureToRT(rt_tex, flip);
	return;
}

void PLSCamGroundItem::RenderItemTextureToRT(gs_texture *rt_tex, bool flip)
{
	if (!rt_tex) {
		return;
	}

	if (!item_render_effect) {
		bool res = InitItemRenderEffect();
		if (!res) {
			plog(LOG_WARNING,
			     "PLSCamGroundItem: item render effect is invalid");
			return;
		}
	}

	if (auto_fit) {
		CalcSrcItemRect(rt_tex);
	}

	gs_texture_t *pre_rt_tex = gs_get_render_target();
	RenderBegin(rt_tex);
	// mix item_texture to rt_tex
	gs_technique_begin(item_render_tech);
	gs_technique_begin_pass(item_render_tech, 0);

	gs_effect_set_texture(param_image, item_texture);
	gs_effect_set_bool(param_h_flip, item_data.h_flip);
	bool vflip = item_data.v_flip;
	if (flip) {
		vflip = !vflip;
	}
	gs_effect_set_bool(param_v_flip, vflip);

	float scale_x = 1.f;
	float scale_y = 1.f;
	uint32_t rt_width = gs_texture_get_width(rt_tex);
	uint32_t rt_height = gs_texture_get_height(rt_tex);
	CalcScale(rt_width, rt_height, scale_x, scale_y);
	gs_matrix_scale3f(scale_x, scale_y, 1.f);
	if (item_data.rt_source.cx < FLOAT_NEAR_ZERO ||
	    item_data.rt_source.cy < FLOAT_NEAR_ZERO) {
		gs_draw_sprite(item_texture, 0, 0, 0);
	} else {
		uint32_t width = gs_texture_get_width(item_texture);
		uint32_t height = gs_texture_get_height(item_texture);
		gs_draw_sprite_subregion(
			item_texture, 0,
			static_cast<uint32_t>(item_data.rt_source.x * width),
			static_cast<uint32_t>(item_data.rt_source.y * height),
			static_cast<uint32_t>(item_data.rt_source.cx * width),
			static_cast<uint32_t>(item_data.rt_source.cy * height));
	}

	gs_technique_end_pass(item_render_tech);
	gs_technique_end(item_render_tech);

	RenderEnd();
	gs_set_render_target(pre_rt_tex, nullptr);
}

void PLSCamGroundItem::CalcItemRenderRect(gs_texture_t *rt_tex)
{
	if (!rt_tex || !item_texture) {
		item_data.rt_target.x = 0.f;
		item_data.rt_target.y = 0.f;
		item_data.rt_target.cx = 1.f;
		item_data.rt_target.cy = 1.f;
		return;
	}
	uint32_t des_w = gs_texture_get_width(rt_tex);
	uint32_t des_h = gs_texture_get_height(rt_tex);
	uint32_t item_w = gs_texture_get_width(item_texture);
	uint32_t item_h = gs_texture_get_height(item_texture);
	if (des_w <= 0 || des_h <= 0 || item_w <= 0 || item_h <= 0) {
		item_data.rt_target.x = 0.f;
		item_data.rt_target.y = 0.f;
		item_data.rt_target.cx = 1.f;
		item_data.rt_target.cy = 1.f;
		return;
	}

	float item_rt_width = 0.f;
	float item_rt_height = 0.f;
	float des_scale = (float)des_w / (float)des_h;
	float item_scale = (float)item_w / (float)item_h;
	if (item_scale >= des_scale) {
		//width match, height scale
		item_rt_width = (float)des_w;
		item_rt_height = (float)des_w / (float)item_w * (float)item_h;
	} else {
		//height match, width scale
		item_rt_height = (float)des_h;
		item_rt_width = (float)des_h / (float)item_h * (float)item_w;
	}
	item_data.rt_target.cx = item_rt_width / (float)des_w;
	item_data.rt_target.cy = item_rt_height / (float)des_h;
	item_data.rt_target.x = (1.0 - item_data.rt_target.cx) * 0.5f;
	item_data.rt_target.y = (1.0 - item_data.rt_target.cy) * 0.5f;
}

void PLSCamGroundItem::CalcSrcItemRect(gs_texture_t *rt_tex)
{
	if (!rt_tex || !item_texture) {
		item_data.rt_source.x = 0.f;
		item_data.rt_source.y = 0.f;
		item_data.rt_source.cx = 1.f;
		item_data.rt_source.cy = 1.f;
		return;
	}
	uint32_t des_w = gs_texture_get_width(rt_tex);
	uint32_t des_h = gs_texture_get_height(rt_tex);
	uint32_t item_w = gs_texture_get_width(item_texture);
	uint32_t item_h = gs_texture_get_height(item_texture);
	if (des_w <= 0 || des_h <= 0 || item_w <= 0 || item_h <= 0) {
		item_data.rt_source.x = 0.f;
		item_data.rt_source.y = 0.f;
		item_data.rt_source.cx = 1.f;
		item_data.rt_source.cy = 1.f;
		return;
	}

	float item_src_width = 0.f;
	float item_src_height = 0.f;
	float des_scale = (float)des_w / (float)des_h;
	float item_scale = (float)item_w / (float)item_h;
	if (item_scale > des_scale) {
		//height match, width scale
		item_src_height = (float)item_h;
		item_src_width = (float)item_h * des_scale;
	} else {
		//width match, height scale
		item_src_width = item_w;
		item_src_height = item_w * (float)des_h / (float)des_w;
	}

	item_data.rt_source.cx = item_src_width / (float)item_w;
	item_data.rt_source.cy = item_src_height / (float)item_h;
	item_data.rt_source.x = (1.0 - item_data.rt_source.cx) * 0.5f;
	item_data.rt_source.y = (1.0 - item_data.rt_source.cy) * 0.5f;
}

bool PLSCamGroundItem::IsVideoFileNotExist()
{
	if (item_data.item_type == ITEM_TYPE_VIDEO && item_data.motion) {
		if (!os_is_file_exist(item_data.path.c_str())) {
			return true;
		}
	}
	return false;
}

float PLSCamGroundItem::Saturate(float x)
{
	float value = max(0.f, x);
	return min(value, 1.f);
}

void PLSCamGroundItem::SetSourceRect(float x, float y, float cx, float cy)
{
	//CAutoLockCS autoLock(item_lock);
	item_data.rt_source.x = Saturate(x);
	item_data.rt_source.y = Saturate(y);
	item_data.rt_source.cx = Saturate(cx);
	item_data.rt_source.cy = Saturate(cy);
}

bool PLSCamGroundItem::SetTargetRect(float x, float y, float cx, float cy)
{
	//CAutoLockCS autoLock(item_lock);
	if (fabs(pre_rt_target.x - x) > FLOAT_NEAR_ZERO ||
	    fabs(pre_rt_target.y - y) > FLOAT_NEAR_ZERO ||
	    fabs(pre_rt_target.cx - cx) > FLOAT_NEAR_ZERO ||
	    fabs(pre_rt_target.cy - cy) > FLOAT_NEAR_ZERO) {
		item_data.rt_target.x = Saturate(x);
		item_data.rt_target.y = Saturate(y);
		item_data.rt_target.cx = Saturate(cx);
		item_data.rt_target.cy = Saturate(cy);
		pre_rt_target.x = x;
		pre_rt_target.y = y;
		pre_rt_target.cx = cx;
		pre_rt_target.cy = cy;
		return true;
	}
	return false;
}

void PLSCamGroundItem::SetPrivateData(const item_private_data &data)
{
	CAutoLockCS autoLock(item_lock);
	latest_data.item_type = data.item_type;
	memcpy(&latest_data.rt_source, &data.rt_source, sizeof(data.rt_source));
	memcpy(&latest_data.rt_target, &data.rt_target, sizeof(data.rt_target));
	latest_data.path = data.path;
	latest_data.stop_moiton_path = data.stop_moiton_path;
	latest_data.v_flip = data.v_flip;
	latest_data.h_flip = data.h_flip;
	latest_data.motion = data.motion;
	latest_data.enable = data.enable;
	latest_data.ui_motion_id = data.ui_motion_id;
	latest_data.thumbnail_file_path = data.thumbnail_file_path;
	latest_data.is_prism_resource = data.is_prism_resource;
	private_data_update = true;
}

void PLSCamGroundItem::SetRenderSize(uint32_t offset_x, uint32_t offset_y,
				     uint32_t width, uint32_t height)
{
	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);

	gs_ortho(0.0f, width, 0.0f, height, -100.0f, 100.0f);
	gs_set_viewport(offset_x, offset_y, width, height);
}

bool PLSCamGroundItem::InitItemRenderEffect()
{
	char *effect_path = obs_module_file("item_render.effect");
	item_render_effect = gs_effect_create_from_file(effect_path, NULL);
	bfree(effect_path);

	item_render_tech = gs_effect_get_technique(item_render_effect, "Draw");
	param_v_flip =
		gs_effect_get_param_by_name(item_render_effect, "v_flip");
	param_h_flip =
		gs_effect_get_param_by_name(item_render_effect, "h_flip");
	param_image = gs_effect_get_param_by_name(item_render_effect, "image");

	return item_render_tech && param_v_flip && param_h_flip && param_image;
}

void PLSCamGroundItem::ResetBGState()
{
	if (video_source) {
		obs_source_media_stop(video_source);
		obs_source_clear_video_cache(video_source);
	}
	latest_data.path = item_data.path = "";
}

bool PLSCamGroundItem::Tick()
{
	//update item data
	if (!image_source) {
		image_source = CreateItemSource(ITEM_TYPE_IMAGE, "");
	}

	if (!video_source) {
		video_source = CreateItemSource(ITEM_TYPE_VIDEO, "");
	}

	CAutoLockCS autoLock(item_lock);

	if (!private_data_update) {
		return false;
	}

	if (!latest_data.enable) {
		ResetBGState();
		item_data.enable = latest_data.enable;
		private_data_update = false;
		return true;
	}

	if (item_data.path != latest_data.path ||
	    item_data.ui_motion_id != latest_data.ui_motion_id) {
		//background file changed, check thumbnail file
		bool res = ThumbnailFileCheck();
		if (!res) {
			private_data_update = false;
			return false;
		}
	}

	//only if select background file changed to clear background texture
	bool res = false;
	item_data.h_flip = latest_data.h_flip;
	item_data.v_flip = latest_data.v_flip;
	SetSourceRect(latest_data.rt_source.x, latest_data.rt_source.y,
		      latest_data.rt_source.cx, latest_data.rt_source.cy);

	//target rect changed, clear background
	res = SetTargetRect(latest_data.rt_target.x, latest_data.rt_target.y,
			    latest_data.rt_target.cx, latest_data.rt_target.cy);

	if (item_data.path == latest_data.path &&
	    item_data.ui_motion_id == latest_data.ui_motion_id) {
		//path not changed, check motion state
		if (item_data.motion != latest_data.motion ||
		    item_data.enable != latest_data.enable) {
			//motion changed
			plog(LOG_INFO,
			     "Motion off state changed, from %d to %d",
			     item_data.motion ? 0 : 1,
			     latest_data.motion ? 0 : 1);
			if (latest_data.motion) {
				//motion switch  off ---->  on
				if (latest_data.item_type == ITEM_TYPE_VIDEO) {
					need_video_play = true;
					item_source = video_source;
				} else if (latest_data.item_type ==
					   ITEM_TYPE_IMAGE) {
					//do nothing
				}
			} else {
				////motion switch  on ---->  off
				if (latest_data.item_type == ITEM_TYPE_VIDEO) {
					need_video_pause = true;
					item_source = image_source;
				}
			}
			item_texture_clear = true;

			//action log
			NotifyActionChanged();
		}
	} else {
		plog(LOG_INFO,
		     "Background file path changed, path[%s][%s] to new path[%s][%s]",
		     item_data.path.c_str(), item_data.stop_moiton_path.c_str(),
		     latest_data.path.c_str(),
		     latest_data.stop_moiton_path.c_str());

		//path changed
		video_playing = false;
		if (latest_data.item_type == ITEM_TYPE_IMAGE) {
			need_video_pause = true;
			UpdateImageSource(image_source, latest_data.path);
		} else if (latest_data.item_type == ITEM_TYPE_VIDEO) {
			UpdateVideoSource(video_source, latest_data.path);
			UpdateImageSource(image_source,
					  latest_data.stop_moiton_path);
		}

		if (latest_data.motion == item_data.motion) {
			//path changed, motion not changed
			if (!latest_data.motion) {
				//motion off
				if (latest_data.item_type == ITEM_TYPE_VIDEO) {
					item_source = image_source;
				} else {
					//do nothing
				}
				item_source = image_source;
			} else {
				//motion on
				if (latest_data.item_type == ITEM_TYPE_VIDEO) {
					item_source = video_source;
					need_video_play = true;
				} else {
					item_source = image_source;
				}
			}
		} else {
			plog(LOG_INFO,
			     "Motion off state changed, from %d to %d",
			     item_data.motion ? 0 : 1,
			     latest_data.motion ? 0 : 1);
			//motion changed
			if (latest_data.motion) {
				//motion off --> on
				if (latest_data.item_type == ITEM_TYPE_VIDEO) {
					need_video_play = true;
					item_source = video_source;
				} else {
					item_source = image_source;
				}
			} else {
				//motion on ---> off
				if (latest_data.item_type == ITEM_TYPE_VIDEO) {
					need_video_pause = true;
				}
				item_source = image_source;
			}
		}
		item_texture_clear = true;
		//res = true;

		//action log
		NotifyActionChanged();
	}

	item_data.enable = latest_data.enable;
	item_data.motion = latest_data.motion;
	item_data.item_type = latest_data.item_type;
	item_data.path = latest_data.path;
	item_data.stop_moiton_path = latest_data.stop_moiton_path;
	item_data.ui_motion_id = latest_data.ui_motion_id;
	item_data.is_prism_resource = latest_data.is_prism_resource;
	private_data_update = false;
	return res;
}

bool PLSCamGroundItem::ThumbnailFileCheck()
{
	if (!latest_data.thumbnail_file_path.empty()) {
		bool res =
			os_file_exists(latest_data.thumbnail_file_path.c_str());
		if (!res && exception_callback) {
			plog(LOG_INFO,
			     "Thumbnail file[%s] do not exist, switch to original",
			     latest_data.thumbnail_file_path.c_str());
			exception_callback(pls_cam_effect,
					   OBS_SOURCE_EXCEPTION_BG_FILE_ERROR,
					   0);
			return false;
		}
	}
	return true;
}

void PLSCamGroundItem::NotifyActionChanged()
{
	if (parent_source && item_source) {
		obs_data_t *item_settings =
			obs_source_get_settings(item_source);
		if (item_settings) {
			std::string bg_file_path;
			if (image_source && item_source == image_source) {
				bg_file_path = obs_data_get_string(
					item_settings, "file");

			} else if (video_source &&
				   item_source == video_source) {
				bg_file_path = obs_data_get_string(
					item_settings, "local_file");
			}
			obs_data_release(item_settings);
			if (bg_file_path.empty()) {
				return;
			}

			std::string::size_type pos =
				bg_file_path.find_last_of('/');
			std::string filename = "";
			if (pos != std::string::npos) {
				filename = bg_file_path.substr(pos + 1);
			}
			UpdateActionInfo(filename.c_str(), latest_data.motion,
					 latest_data.is_prism_resource);
		}
	}
}

void PLSCamGroundItem::UpdateActionInfo(const char *filename, bool motion,
					bool prism_resource)
{
	if (!filename) {
		return;
	}

	//event2: type(motion, static)
	std::string event2 = "";
	if (motion) {
		event2 = VB_ACTIONLOG_MOTION;
	} else {
		event2 = VB_ACTIONLOG_STATIC;
	}

	//event3: item id or custom
	//target: if custom, file name
	std::string event3 = "";
	std::string target = "";
	if (prism_resource) {
		event3 = latest_data.ui_motion_id;
	} else {
		event3 = VB_ACTIONLOG_CUSTOM;
		target = filename;
	}
	obs_source_action_event_notify(parent_source,
				       OBS_SOURCE_VIRTUAL_BACKGROUND_STATUS,
				       VB_ACTIONLOG_MODULE, event2.c_str(),
				       event3.c_str(), target.c_str());
}

bool PLSCamGroundItem::UpdateImageSource(obs_source_t *source,
					 const std::string &path)
{
	if (!source || source != image_source) {
		return false;
	}

	bool res = os_file_exists(path.c_str());
	if (!res && exception_callback) {
		plog(LOG_INFO, "Background file[%s] do not exist",
		     path.c_str());
		exception_callback(pls_cam_effect,
				   OBS_SOURCE_EXCEPTION_BG_FILE_ERROR, 0);
		return false;
	}

	obs_data_t *private_settings = obs_source_get_settings(source);
	if (!private_settings) {
		return false;
	}
	obs_data_set_string(private_settings, "file", path.c_str());
	obs_source_update(source, private_settings);
	obs_data_release(private_settings);

	return true;
}

bool PLSCamGroundItem::UpdateVideoSource(obs_source_t *source,
					 const std::string &path)
{
	if (!source || source != video_source) {
		return false;
	}

	obs_data_t *private_settings = obs_source_get_settings(source);
	if (!private_settings) {
		return false;
	}
	obs_data_set_bool(private_settings, "is_local_file", true);
	obs_data_set_string(private_settings, "local_file", path.c_str());
	obs_data_set_bool(private_settings, "looping", true);
	obs_data_set_bool(private_settings, "hw_decode", false);
	obs_data_set_bool(private_settings, "virtual_background_source", true);
	obs_source_update(source, private_settings);
	obs_data_release(private_settings);

	return true;
}

bool PLSCamGroundItem::StopVideo()
{
	if (!video_source) {
		return false;
	}
	obs_media_state st = obs_source_media_get_state(video_source);
	if (OBS_MEDIA_STATE_PLAYING == st) {
		obs_source_media_stop(video_source);
	}
	return true;
}

void PLSCamGroundItem::SetClearItemTexture(bool clear)
{
	CAutoLockCS autoLock(item_lock);
	item_texture_clear = true;
}

void PLSCamGroundItem::ClearResWhenEffectOff()
{
	CAutoLockCS autoLock(item_lock);

	if (!private_data_update) {
		return;
	}

	if (!latest_data.enable && item_data.enable) {
		ResetBGState();
		item_data.enable = latest_data.enable;
		private_data_update = false;
		return;
	}
}

void PLSCamGroundItem::SetAutoFit(bool token)
{
	CAutoLockCS autoLock(item_lock);
	auto_fit = token;
}

std::string PLSCamGroundItem::GenerateGuid()
{
	GUID guid;
	CoCreateGuid(&guid);

	char buf[64] = {0};
	sprintf_s(buf, sizeof(buf) / sizeof(buf[0]),
		  "{%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X}", guid.Data1,
		  guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
		  guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5],
		  guid.Data4[6], guid.Data4[7]);

	return std::string(buf);
}

void PLSCamGroundItem::RenderBegin(gs_texture *rt_tex)
{
	uint32_t rt_width = gs_texture_get_width(rt_tex);
	uint32_t rt_height = gs_texture_get_height(rt_tex);

	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();
	gs_matrix_identity();

	gs_set_render_target(rt_tex, nullptr);

	if (item_data.rt_target.cx < FLOAT_NEAR_ZERO ||
	    item_data.rt_target.cy < FLOAT_NEAR_ZERO) {
		SetRenderSize(0, 0, rt_width, rt_height);
	} else {
		SetRenderSize(
			static_cast<uint32_t>(item_data.rt_target.x * rt_width),
			static_cast<uint32_t>(item_data.rt_target.y *
					      rt_height),
			static_cast<uint32_t>(item_data.rt_target.cx *
					      rt_width),
			static_cast<uint32_t>(item_data.rt_target.cy *
					      rt_height));
	}

	//alpha blend
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
}

void PLSCamGroundItem::RenderEnd()
{
	gs_blend_state_pop();
	gs_matrix_pop();
	gs_projection_pop();
	gs_viewport_pop();
}

void PLSCamGroundItem::CalcScale(int rt_width, int rt_height, float &scale_x,
				 float &scale_y)
{
	if (!item_texture) {
		scale_x = 1.f;
		scale_y = 1.f;
		return;
	}

	uint32_t item_tex_width = gs_texture_get_width(item_texture);
	uint32_t item_tex_height = gs_texture_get_height(item_texture);
	if (item_tex_width == 0 || item_tex_height == 0) {
		scale_x = 1.f;
		scale_y = 1.f;
		return;
	}
	float source_width = (float)item_tex_width;
	float source_height = (float)item_tex_height;
	if (item_data.rt_source.cx > FLOAT_NEAR_ZERO &&
	    item_data.rt_source.cy > FLOAT_NEAR_ZERO) {
		source_width = item_data.rt_source.cx * item_tex_width;
		source_height = item_data.rt_source.cy * item_tex_height;
	}

	float dst_width = (float)rt_width;
	float dst_height = (float)rt_height;
	if (item_data.rt_target.cx > FLOAT_NEAR_ZERO &&
	    item_data.rt_target.cy > FLOAT_NEAR_ZERO) {
		dst_width *= item_data.rt_target.cx;
		dst_height *= item_data.rt_target.cy;
	}

	scale_x = dst_width / source_width;
	scale_y = dst_height / source_height;
}

void PLSCamGroundItem::ClearItemTexture()
{
	if (!item_texture) {
		return;
	}
	obs_enter_graphics();
	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);

	gs_viewport_push();
	gs_projection_push();

	gs_texture_t *pre_tex = gs_get_render_target();
	gs_set_render_target(item_texture, NULL);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	gs_set_render_target(pre_tex, nullptr);
	gs_projection_pop();
	gs_viewport_pop();

	obs_leave_graphics();
}
