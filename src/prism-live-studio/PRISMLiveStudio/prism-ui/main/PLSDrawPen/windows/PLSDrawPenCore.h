#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d2d1.h>
#include <util/windows/ComPtr.hpp>
#include <map>
#include <thread>
#include "graphics/graphics.h"
#include "PLSDrawPenEffect.h"
#include "PLSD2DRenderTarget.h"
#include "PLSUndoRedoCache.h"

class PLSDrawPenCore {
public:
	PLSDrawPenCore();
	~PLSDrawPenCore();

	void UpdateSharedTexture();
	void ClearRenderTexture();

	void HandlerStrokeRender();

	void UpdateCanvasByVisible(bool visible);

	PLSUndoRedoCache &UndoRedoCache() { return cacheStrokeImages; }

private:
	void StrokeRenderHighlighterCallback();
	void StrokeBlendTexturesCallback();
	void StrokeRenderCallback();
	bool IsMaskTargetValid();

	void DrawRender();
	bool RenderDrawingToTarget(gs_texture_t *target);
	void RenderStrokesToTarget(gs_texture_t *target);
	void BlendTexturesToTarget(gs_texture_t *srcTop, gs_texture_t *srcBottom, gs_texture_t *target) const;
	void CopyTexture(gs_texture_t *dst, gs_texture_t *src) const;
	void ClearTexture(gs_texture_t *texture) const;
	void CheckRubberHitsStroke() const;

	void UpdateCanvas(bool save = false);
	bool CheckDeviceRebuilt();

	void CheckUpdateStrokesTexture();
	void RenderCanvesTextureToTarget(gs_texture_t *target);

	void DestroyTextures();

	static void SaveTextureCallback(void *data);
	static void ReadTextureCallback(void *data);

	void SaveCacheImage(std::vector<Stroke> input);
	void ReadTexture();
	bool ReadCacheImage(gs_texture_t *target);
	void WriteTexture();

	HANDLE exitEvent;
	std::thread renderThread;

	gs_stagesurf_t *saveStage = nullptr;
	gs_texture_t *syncTexture = nullptr;
	gs_texture_t *strokesTexture = nullptr;
	gs_texture_t *renderTexture = nullptr;
	gs_texture_t *drawingTexture = nullptr;
	gs_texture_t *tempTexture = nullptr;

	PLSUndoRedoCache cacheStrokeImages;
	CacheWriteInfo writeInfo;
	CacheReadInfo readInfo;

	uint32_t readBufferSize = 0;
	std::shared_ptr<uint8_t> readBuffer = nullptr;

	std::shared_ptr<PLSD2DRenderTarget> d2dRenderTarget = nullptr;
	std::shared_ptr<PLSD2DRenderTarget> d2dRenderTargetGlowMask = nullptr;
	std::shared_ptr<PLSDrawPenGlowEffect> glowEffect = nullptr;
	std::shared_ptr<PLSDrawPenMixEffect> blendEffect = nullptr;
	std::shared_ptr<PLSDrawPenHighlighterEffect> highlighterEffect = nullptr;

	uint32_t width = 0;
	uint32_t height = 0;

	Stroke stroke;
	bool maskReady = false;
};
