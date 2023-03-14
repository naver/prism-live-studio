#pragma once
#include "prism-cam-ground-item.h"
#include "prism-handle-wrapper.h"
#include <vector>

class PLSCamGround {
public:
	PLSCamGround(obs_source_t *dshow_source, void *cam_effect,
		     ground_exception cb, GROUND_TYPE type);
	~PLSCamGround();

public:
	//render ground to rt_tex
	void UpdateGroundTexture(uint32_t width, uint32_t height,
				 gs_color_format fmt);
	bool RenderGround(gs_texture_t *rt_tex, bool flip);
	void BlurGroundTexture(gs_texture_t *rt_tex);
	void BlurGroundTexthreHV(gs_texture_t *rt_tex, gs_technique_t *tech);

	//init effect if needed
	bool InitGaussEffect();

	void SetBackgroundInfo(const ground_private_data gd_data,
			       item_private_data &item_data);

	void Tick();
	void ClearBGTexture();
	void SetClearAllTextureToken();
	void ClearResWhenEffectOff();
	void BlendGroundTexture(gs_texture_t *rt_tex);
	void NotifyBackgroundAction(const ground_private_data gd_data);

private:
	CCriticalSection param_lock;
	std::vector<PLSCamGroundItem *> vecGroundItems;
	gs_texture_t *ground_texture;
	gs_effect_t *gauss_effect;
	gs_technique_t *gauss_tech;
	ground_private_data ground_data;
	ground_exception exception_callback;
	bool background_clear;
	obs_source_t *parent_source;
	void *pls_cam_effect;
	GROUND_TYPE ground_type;
};
