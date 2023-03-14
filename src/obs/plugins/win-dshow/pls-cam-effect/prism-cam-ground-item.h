#pragma once
#include <windows.h>
#include <obs.hpp>
#include <string>
#include "prism-handle-wrapper.h"
#include "prism-cam-ground-def.h"
#include <atomic>

class PLSCamGroundItem {
public:
	PLSCamGroundItem(obs_source_t *dshow_source, void *cam_effect,
			 ground_exception cb);
	~PLSCamGroundItem();

	//render interface, render ground item to rt_tex
	void RenderItemToRT(gs_texture *rt_tex, bool flip);

	//render item texture to rt_tex
	void RenderItemTextureToRT(gs_texture *rt_tex, bool flip);

	//set render source rect
	float Saturate(float x);
	void SetSourceRect(float x, float y, float cx, float cy);
	bool SetTargetRect(float x, float y, float cx, float cy);
	void SetPrivateData(const item_private_data &data);

	//for video and web, create source, render source and mix to ground
	obs_source_t *CreateItemSource(ITEM_TYPE type,
				       const std::string &file_path);

	void SetRenderSize(uint32_t offset_x, uint32_t offset_y, uint32_t width,
			   uint32_t heigh);

	const std::string GetGuid() { return str_guid; };
	bool InitItemRenderEffect();
	void ResetBGState();

	bool Tick();
	bool UpdateImageSource(obs_source_t *source, const std::string &path);
	bool UpdateVideoSource(obs_source_t *source, const std::string &path);
	bool StopVideo();
	void SetClearItemTexture(bool clear);
	void ClearResWhenEffectOff(); //render thread, tick
	void SetAutoFit(bool token);
	void CalcItemRenderRect(gs_texture_t *rt_tex);
	void CalcSrcItemRect(gs_texture_t *rt_tex);
	bool IsVideoFileNotExist();

	void UpdateActionInfo(const char *filename, bool motion,
			      bool prism_resource);

private:
	std::string GenerateGuid();
	void RenderBegin(gs_texture *rt_tex);
	void RenderEnd();
	void CalcScale(int rt_width, int rt_height, float &scale_x,
		       float &scale_y);
	void ClearItemTexture();
	bool ThumbnailFileCheck();
	void NotifyActionChanged();

private:
	item_private_data latest_data; //update from ui
	item_private_data item_data;   //current item data
	obs_source_t *item_source;     //source for current items,
	gs_texture_t *item_texture;    //texture item texture
	std::string str_guid;          //item token
	gs_effect_t *item_render_effect;
	gs_technique_t *item_render_tech;
	gs_eparam_t *param_h_flip;
	gs_eparam_t *param_v_flip;
	gs_eparam_t *param_image;
	bool need_video_pause;
	bool need_video_play;
	bool private_data_update;
	bool item_texture_clear;
	item_rect pre_rt_target;
	obs_source_t *parent_source;
	bool auto_fit;

public:
	CCriticalSection item_lock;
	obs_source_t *image_source;
	obs_source_t *video_source;
	ground_exception exception_callback;
	void *pls_cam_effect;
	bool video_playing;
};
