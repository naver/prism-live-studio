#pragma once
#include "prism-msg-queue.h"
#include <obs.hpp>
#include "prism-cam-ground.h"

class PLSCamSegmen {
public:
	PLSCamSegmen(obs_source_t *source);
	virtual ~PLSCamSegmen();

	gs_texture *GetOutputVideo(gs_texture_t *texture);
	void CamAsyncTick();

	bool ParseSegmentationParam(obs_data_t *data);
	void ClearResWhenEffectOff();

private:
	void EffectTick();
	void RenderAllLayers(gs_texture_t *texture, bool flip = false);
	static void GroundExceptionHandler(void *data, int type, int subcode);
	void UpdateFinOutputTexture(const gs_texture *tex);
	void ReleaseOutputTexture();

private:
	std::shared_ptr<PLSCamGround> back_ground;
	std::shared_ptr<PLSCamGround> fore_ground;
	gs_texture *fin_output_texture;

	obs_source_t *dshow_source;
};
