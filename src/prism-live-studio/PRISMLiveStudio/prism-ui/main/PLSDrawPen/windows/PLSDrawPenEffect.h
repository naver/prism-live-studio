#pragma once
#include <string>
#include "graphics/graphics.h"
#include "obs.h"

class PLSDrawPenGlowEffect {
public:
	PLSDrawPenGlowEffect();
	~PLSDrawPenGlowEffect();

	void RenderEffect(gs_texture_t *srcTexture, gs_texture_t *dstTexture, uint32_t rgba, float range, bool clear = false);

private:
	gs_effect_t *outerGlowEffect;
	gs_samplerstate_t *pointSampler;
	vec4 glowColor{1.0, 1.0, 1.0, 1.0};
};

class PLSDrawPenHighlighterEffect {
public:
	PLSDrawPenHighlighterEffect();
	~PLSDrawPenHighlighterEffect();

	void RenderEffect(gs_texture_t *srcTexture, gs_texture_t *dstTexture, bool clear = false, float opacity = 0.6f) const;

private:
	gs_effect_t *opaqueEffect;
};

class PLSDrawPenMixEffect {
public:
	PLSDrawPenMixEffect();
	~PLSDrawPenMixEffect();

	void RenderEffect(gs_texture_t *srcTop, gs_texture_t *srcBottom, gs_texture_t *target) const;

private:
	gs_effect_t *mixEffect = nullptr;
};
