#include "PLSDrawPenCore.h"
#include "../PLSDrawPenMgr.h"
#include <string>
#include <intrin.h>
#include "log/log.h"
#include "PLSGraphicsHandler.h"
#include "window-basic-main.hpp"
#include "pls/pls-obs-api.h"

constexpr auto Alpha = 0.6f;
constexpr auto Offset = 2; // line width offset
constexpr auto GlowRange = 1.0f;

constexpr auto drawing_pen = "drawpen drawing_pen";
constexpr auto drawing_highlighter = "drawpen drawing_highlighter";
constexpr auto drawing_glowpen = "drawpen drawing_glowpen";
constexpr auto drawing_shape = "drawpen drawing_shape";
constexpr auto drawing_flush_blend = "drawpen flush_blend";
constexpr auto drawing_flush_glow = "drawpen glush_glow";

constexpr auto key_save_cache = "drawpen_save_texture";
constexpr auto key_read_cache = "drawpen_read_texture";

constexpr auto glow_range_mult = 0.5f;
constexpr auto glow_bulge_mult = 2.f;

static const auto white_value = color_to_int(255, 255, 255, 255);

#define DELETE_TEX(tex)                  \
	if (tex) {                       \
		gs_texture_destroy(tex); \
		tex = nullptr;           \
	}

namespace {
bool WaitForGsGpuCompletion()
{
	bool completed = false;
	HRESULT hr;
	D3D11_QUERY_DESC queryDesc = {};
	ComPtr<ID3D11Query> query = nullptr;
	ComPtr<ID3D11DeviceContext> context = nullptr;
	ID3D11Device *device = nullptr;

	ULONGLONG startTime = GetTickCount64();
	int spinCount = 0;

	obs_enter_graphics();

	if (gs_get_device_type() != GS_DEVICE_DIRECT3D_11) {
		gs_flush();
		completed = true;
		goto exit;
	}

	device = static_cast<ID3D11Device *>(gs_get_device_obj());
	if (!device)
		goto exit;

	device->GetImmediateContext(context.Assign());
	if (!context)
		goto exit;

	queryDesc.Query = D3D11_QUERY_EVENT;

	hr = device->CreateQuery(&queryDesc, query.Assign());
	if (FAILED(hr) || !query)
		goto exit;

	context->End(query.Get());
	context->Flush();

	static constexpr auto timeout = 50; // in ms
	while (true) {
		const HRESULT waitHr = context->GetData(query.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
		if (waitHr == S_OK) {
			completed = true;
			break;
		}
		if (waitHr != S_FALSE)
			break;

		auto elapsedTime = GetTickCount64() - startTime;
		if (elapsedTime >= timeout)
			break;

		if (spinCount >= 200) {
			spinCount = 0;
			Sleep(0);
		} else {
			++spinCount;
			_mm_pause();
		}
	}

exit:
	obs_leave_graphics();
	return completed;
}
} // namespace

void PLSDrawPenCore::SaveTextureCallback(void *data)
{
	auto core = (PLSDrawPenCore *)data;
	if (core) {
		core->ReadTexture();
	}
}

void PLSDrawPenCore::ReadTextureCallback(void *data)
{
	auto core = (PLSDrawPenCore *)data;
	if (core) {
		core->WriteTexture();
	}
}

PLSDrawPenCore::PLSDrawPenCore()
{
	exitEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);

	PLSGraphicsHandler::Instance()->CreateDevice();

	if (!d2dRenderTarget)
		d2dRenderTarget = std::make_shared<PLSD2DRenderTarget>();

	if (!d2dRenderTargetGlowMask)
		d2dRenderTargetGlowMask = std::make_shared<PLSD2DRenderTarget>();

	if (!glowEffect)
		glowEffect = std::make_shared<PLSDrawPenGlowEffect>();

	if (!highlighterEffect)
		highlighterEffect = std::make_shared<PLSDrawPenHighlighterEffect>();

	if (!blendEffect)
		blendEffect = std::make_shared<PLSDrawPenMixEffect>();

	UpdateSharedTexture();

	renderThread = std::thread([this]() { DrawRender(); });
}

PLSDrawPenCore::~PLSDrawPenCore()
{
	if (renderThread.joinable()) {
		SetEvent(exitEvent);
		renderThread.join();
	}

	CloseHandle(exitEvent);

	PLSGraphicsHandler::Instance()->DestroyDevice();

	blendEffect.reset();
	highlighterEffect.reset();
	glowEffect.reset();
	d2dRenderTarget.reset();
	d2dRenderTargetGlowMask.reset();

	DestroyTextures();
	cacheStrokeImages.ClearCache();
}

//output size changed
void PLSDrawPenCore::UpdateSharedTexture()
{
	uint32_t w = 0;
	uint32_t h = 0;
	PLSDrawPenMgr::Instance()->GetSize(w, h);
	if (!w || !h)
		return;

	if (width != w || height != h) {
		cacheStrokeImages.ClearCache();

		obs_enter_graphics();

		DestroyTextures();

		saveStage = gs_stagesurface_create(w, h, GS_BGRA);
		syncTexture = gs_texture_create(w, h, GS_BGRA, 1, NULL, GS_DYNAMIC);
		renderTexture = gs_texture_create(w, h, GS_BGRA, 1, nullptr, GS_RENDER_TARGET);
		strokesTexture = gs_texture_create(w, h, GS_BGRA, 1, nullptr, GS_RENDER_TARGET);
		drawingTexture = gs_texture_create(w, h, GS_BGRA, 1, nullptr, GS_RENDER_TARGET);
		tempTexture = gs_texture_create(w, h, GS_BGRA, 1, nullptr, GS_RENDER_TARGET);

		obs_leave_graphics();

		if (d2dRenderTarget)
			d2dRenderTarget->ResizeSharedTexture(w, h);

		if (d2dRenderTargetGlowMask)
			d2dRenderTargetGlowMask->ResizeSharedTexture(w, h);
	}

	width = w;
	height = h;
}

void PLSDrawPenCore::ClearRenderTexture()
{
	if (renderTexture) {
		ClearTexture(renderTexture);
		UpdateCanvas();
	}
}

void PLSDrawPenCore::HandlerStrokeRender()
{
	StrokeRenderCallback();

	if (stroke.drawType == DrawType::DT_HIGHLIGHTER)
		StrokeRenderHighlighterCallback();

	StrokeBlendTexturesCallback();

	WaitForGsGpuCompletion();
}

void PLSDrawPenCore::StrokeRenderHighlighterCallback()
{
	CopyTexture(tempTexture, drawingTexture);
	if (highlighterEffect)
		highlighterEffect->RenderEffect(tempTexture, drawingTexture, true, Alpha);
}

void PLSDrawPenCore::StrokeBlendTexturesCallback()
{
	CopyTexture(tempTexture, strokesTexture);
	BlendTexturesToTarget(drawingTexture, tempTexture, strokesTexture);
	obs_enter_graphics();
	profile_start(drawing_flush_blend);
	gs_flush();
	profile_end(drawing_flush_blend);
	obs_leave_graphics();
}

void PLSDrawPenCore::StrokeRenderCallback()
{
	Stroke it = stroke;
	switch (it.drawType) {
	case DrawType::DT_PEN:
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), nullptr, drawingTexture, it.rgba, GlowRange, true);
		break;
	case DrawType::DT_HIGHLIGHTER:
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), nullptr, drawingTexture, it.rgba, GlowRange, true);
		break;
	case DrawType::DT_GLOW_PEN:
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), (maskReady && IsMaskTargetValid()) ? d2dRenderTargetGlowMask->GetSharedTexture() : nullptr, drawingTexture, it.rgba,
					 (FLOAT)it.lineWidth * glow_range_mult, true);
		obs_enter_graphics();
		profile_start(drawing_flush_glow);
		gs_flush();
		profile_end(drawing_flush_glow);
		obs_leave_graphics();
		break;
	case DrawType::DT_2DSHAPE:
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), nullptr, drawingTexture, it.rgba, GlowRange, true);
		break;
	default:
		break;
	}
	return;
}

bool PLSDrawPenCore::IsMaskTargetValid()
{
	obs_enter_graphics();
	bool valid = d2dRenderTargetGlowMask && d2dRenderTargetGlowMask->IsRenderTargetInited();
	obs_leave_graphics();
	return valid;
}

void PLSDrawPenCore::UpdateCanvasByVisible(bool visible)
{
	auto main = OBSBasic::Get();
	if (!main)
		return;
	if (visible)
		pls_scene_update_canvas(main->GetCurrentScene(), renderTexture, false);
	else
		pls_scene_update_canvas(main->GetCurrentScene(), nullptr, false);
}

void PLSDrawPenCore::DrawRender()
{
	std::vector<HANDLE> events = {
		exitEvent,
		PLSDrawPenMgr::Instance()->GetMouseMovingEvent(),
		PLSDrawPenMgr::Instance()->GetMouseReleaseEvent(),
		PLSDrawPenMgr::Instance()->GetStrokeChangedEvent(),
		PLSDrawPenMgr::Instance()->GetRubberEvent(),
	};

	while (true) {
		auto dwEvent = WaitForMultipleObjects(events.size(), events.data(), FALSE, INFINITE);

		if (dwEvent == WAIT_OBJECT_0)
			break; // events[0] : exitEvent was signaled

		if (PLSGraphicsHandler::Instance()->NeedRebuild()) {
			CheckDeviceRebuilt();
		}

		if (!PLSGraphicsHandler::Instance()->InitedDevice() || !d2dRenderTarget || !d2dRenderTarget->IsRenderTargetInited()) {
			WaitForSingleObject(exitEvent, 200);
			continue; // device is not inited, skip render
		}

		if (dwEvent == WAIT_OBJECT_0 + 1) {
			ResetEvent(PLSDrawPenMgr::Instance()->GetMouseMovingEvent()); // firstly reset event
			CheckUpdateStrokesTexture();
			if (RenderDrawingToTarget(drawingTexture)) {
				BlendTexturesToTarget(drawingTexture, strokesTexture, renderTexture);
				UpdateCanvas();
			}
		} else if (dwEvent == WAIT_OBJECT_0 + 2) {
			ResetEvent(PLSDrawPenMgr::Instance()->GetMouseReleaseEvent()); // firstly reset event
			CopyTexture(strokesTexture, renderTexture);
			UpdateCanvas(true);
			SaveCacheImage(PLSDrawPenMgr::Instance()->GetStrokes());
		} else if (dwEvent == WAIT_OBJECT_0 + 3) {
			ResetEvent(PLSDrawPenMgr::Instance()->GetStrokeChangedEvent()); // firstly reset event
			RenderStrokesToTarget(strokesTexture);
			CopyTexture(renderTexture, strokesTexture);
			UpdateCanvas(true);
		} else if (dwEvent == WAIT_OBJECT_0 + 4) {
			ResetEvent(PLSDrawPenMgr::Instance()->GetRubberEvent()); // firstly reset event
			CheckRubberHitsStroke();
		}

		pls_on_drawpen_updated(PLSDrawPenMgr::Instance()->GetCurrentScene());
	}
}

//render drawing stroke to renderTexture
bool PLSDrawPenCore::RenderDrawingToTarget(gs_texture_t *target)
{
	if (!d2dRenderTarget || !glowEffect)
		return false;

	std::vector<PointF> points = PLSDrawPenMgr::Instance()->GetPoints();
	if (points.empty()) {
		ClearTexture(target);
		return false;
	}

	uint32_t curRgba = PLSDrawPenMgr::Instance()->GetColor();
	auto curLine = (FLOAT)PLSDrawPenMgr::Instance()->GetLineWidth();
	ShapeType curShape = PLSDrawPenMgr::Instance()->GetCurrentShapeType();
	DrawType type = PLSDrawPenMgr::Instance()->GetCurrentDrawType();

	switch (type) {
	case DrawType::DT_PEN:
		d2dRenderTarget->DrawCurve(points, curRgba, curLine);
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), nullptr, target, curRgba, GlowRange, true);
		WaitForGsGpuCompletion();
		break;
	case DrawType::DT_HIGHLIGHTER:
		d2dRenderTarget->DrawCurve(points, curRgba, curLine, false, Offset);
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), nullptr, target, curRgba, GlowRange, true);

		WaitForGsGpuCompletion();

		CopyTexture(tempTexture, target);
		if (highlighterEffect)
			highlighterEffect->RenderEffect(tempTexture, target, true, Alpha);
		break;
	case DrawType::DT_GLOW_PEN: {
		d2dRenderTarget->DrawCurve(points, white_value, curLine);

		auto maskValid = IsMaskTargetValid();
		if (maskValid)
			maskValid = d2dRenderTargetGlowMask->DrawCurve(points, white_value, curLine * glow_bulge_mult);

		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), maskValid ? d2dRenderTargetGlowMask->GetSharedTexture() : nullptr, target, curRgba, curLine * glow_range_mult, true);

		WaitForGsGpuCompletion();
		break;
	}
	case DrawType::DT_2DSHAPE:
		d2dRenderTarget->Draw2DShape(points, curShape, curRgba, curLine);
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), nullptr, target, curRgba, GlowRange, true);
		WaitForGsGpuCompletion();
		break;
	default:
		break;
	}
	return true;
}

static void render_stroke(void *data)
{
	auto core = (PLSDrawPenCore *)data;
	if (core)
		core->HandlerStrokeRender();
}

//render drawn strokes to strokesTexture
void PLSDrawPenCore::RenderStrokesToTarget(gs_texture_t *target)
{
	if (!target || !d2dRenderTarget || !glowEffect)
		return;

	if (ReadCacheImage(target))
		return;

	ClearTexture(target);

	bool drawBegin = false;
	std::vector<const Stroke *> glowItems;
	std::vector<Stroke> strokes = PLSDrawPenMgr::Instance()->GetStrokes();
	Stroke nextStroke = strokes.empty() ? Stroke() : strokes.front();
	for (size_t index = 0; index < strokes.size(); ++index) {
		const auto &it = strokes[index];
		stroke = it;

		if (!drawBegin)
			drawBegin = d2dRenderTarget->DrawBegin();

		switch (it.drawType) {
		case DrawType::DT_PEN:
			d2dRenderTarget->DrawGeometry(it.geometry, it.rgba, (FLOAT)it.lineWidth, drawing_pen);
			break;
		case DrawType::DT_HIGHLIGHTER:
			d2dRenderTarget->DrawGeometry(it.geometry, it.rgba, (FLOAT)it.lineWidth, drawing_highlighter, false, Offset);
			break;
		case DrawType::DT_GLOW_PEN:
			glowItems.push_back(&it);
			d2dRenderTarget->DrawGeometry(it.geometry, white_value, (FLOAT)it.lineWidth, drawing_glowpen);
			break;
		case DrawType::DT_2DSHAPE:
			if (it.shapeType == ShapeType::ST_STRAIGHT_ARROW)
				d2dRenderTarget->DrawArrowGeometry(it.geometry, it.rgba, (FLOAT)it.lineWidth, drawing_shape);
			else
				d2dRenderTarget->DrawGeometry(it.geometry, it.rgba, (FLOAT)it.lineWidth, drawing_shape);
			break;
		default:
			break;
		}

		bool nextValid = (index + 1) < strokes.size();
		nextStroke = nextValid ? strokes[index + 1] : Stroke();
		bool needRender = nextStroke.drawType != it.drawType || nextStroke.rgba != it.rgba || (it.drawType == DrawType::DT_GLOW_PEN && nextStroke.lineWidth != it.lineWidth);
		if (!nextValid || needRender) {
			d2dRenderTarget->DrawEnd();
			drawBegin = false;
			maskReady = false;

			if (it.drawType == DrawType::DT_GLOW_PEN && !glowItems.empty() && IsMaskTargetValid()) {
				if (d2dRenderTargetGlowMask->DrawBegin()) {
					for (const auto &itr : glowItems)
						d2dRenderTargetGlowMask->DrawGeometry(itr->geometry, white_value, (FLOAT)itr->lineWidth * glow_bulge_mult, drawing_glowpen);

					d2dRenderTargetGlowMask->DrawEnd();
					maskReady = true;
				}
			}
			glowItems.clear();

			obs_queue_task(OBS_TASK_GRAPHICS, render_stroke, this, true);
		}
	}

	SaveCacheImage(strokes);
}

void PLSDrawPenCore::BlendTexturesToTarget(gs_texture_t *srcTop, gs_texture_t *srcBottom, gs_texture_t *target) const
{
	if (!blendEffect || !target)
		return;

	blendEffect->RenderEffect(srcTop, srcBottom, target);
}

void PLSDrawPenCore::CopyTexture(gs_texture_t *dst, gs_texture_t *src) const
{
	if (!dst)
		return;

	if (!src) {
		ClearTexture(dst);
	} else {
		obs_enter_graphics();
		gs_copy_texture(dst, src);
		obs_leave_graphics();
	}
}

void PLSDrawPenCore::ClearTexture(gs_texture_t *texture) const
{
	obs_enter_graphics();
	gs_texture_t *pre_rt = gs_get_render_target();
	gs_projection_push();
	gs_set_render_target(texture, nullptr);
	struct vec4 clear_color = {0};
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);
	gs_set_render_target(pre_rt, nullptr);
	gs_projection_pop();
	obs_leave_graphics();
}

void PLSDrawPenCore::CheckRubberHitsStroke() const
{
	DrawType type = PLSDrawPenMgr::Instance()->GetCurrentDrawType();
	if (type != DrawType::DT_RUBBER)
		return;

	std::vector<PointF> points = PLSDrawPenMgr::Instance()->GetPoints();
	if (points.empty())
		return;

	std::vector<Stroke> strokes = PLSDrawPenMgr::Instance()->GetStrokes();
	for (auto const &it : strokes) {
		if (!points.empty() && it.geometry) {
			BOOL contains = false;
			D2D1_POINT_2F p{points.back().x, points.back().y};
			auto line = (FLOAT)it.lineWidth;
			if (it.drawType == DrawType::DT_HIGHLIGHTER)
				line += (FLOAT)Offset;

			HRESULT hr = it.geometry->StrokeContainsPoint(p, line, nullptr, D2D1::Matrix3x2F::Identity(), 0, &contains);
			if (SUCCEEDED(hr) && contains) {
				PLSDrawPenMgr::Instance()->RemoveStroke(it.id);
				break;
			}
		}
	}
	return;
}

void PLSDrawPenCore::UpdateCanvas(bool save)
{
	auto main = OBSBasic::Get();
	if (!main)
		return;
	pls_scene_update_canvas(main->GetCurrentScene(), renderTexture, save);
}

bool PLSDrawPenCore::CheckDeviceRebuilt()
{
	bool ret = PLSGraphicsHandler::Instance()->RebuildDevice();
	if (!ret)
		return ret;

	if (!d2dRenderTarget)
		d2dRenderTarget = std::make_shared<PLSD2DRenderTarget>();

	if (!d2dRenderTargetGlowMask)
		d2dRenderTargetGlowMask = std::make_shared<PLSD2DRenderTarget>();

	d2dRenderTargetGlowMask->ResetRenderTarget();

	ret = d2dRenderTarget->ResetRenderTarget();
	PLSDrawPenMgr::Instance()->ClearAllStrokes();

	return ret;
}

void PLSDrawPenCore::CheckUpdateStrokesTexture()
{
	if (PLSDrawPenMgr::Instance()->NeedUpdateStrokesToTarget()) {
		RenderCanvesTextureToTarget(strokesTexture);
	}
	return;
}

void PLSDrawPenCore::RenderCanvesTextureToTarget(gs_texture_t *target)
{
	gs_texture_t *sceneCanves = PLSDrawPenMgr::Instance()->GetSceneCanvasTexture();
	obs_enter_graphics();
	uint32_t tex_width = gs_texture_get_width(sceneCanves);
	uint32_t tex_height = gs_texture_get_height(sceneCanves);
	obs_leave_graphics();
	if (tex_width == width && tex_height == height)
		CopyTexture(target, sceneCanves);
	else {
		RenderStrokesToTarget(target);
	}
}

void PLSDrawPenCore::DestroyTextures()
{
	obs_enter_graphics();

	gs_stagesurface_destroy(saveStage);
	DELETE_TEX(syncTexture);
	DELETE_TEX(renderTexture);
	DELETE_TEX(strokesTexture);
	DELETE_TEX(drawingTexture);
	DELETE_TEX(tempTexture);

	obs_leave_graphics();
}

void PLSDrawPenCore::SaveCacheImage(std::vector<Stroke> strokes)
{
	if (strokes.empty())
		return;

	bool valid = false;
	obs_enter_graphics();
	do {
		if (!saveStage || !strokesTexture) {
			assert(false);
			break;
		}

		auto width = (size_t)gs_texture_get_width(strokesTexture);
		auto height = (size_t)gs_texture_get_height(strokesTexture);
		size_t size = width * height;

		static constexpr size_t maxResolution = 2560 * 1600; // 2K
		valid = size > 0 && size <= maxResolution;

	} while (0);
	obs_leave_graphics();

	if (!valid)
		return;

	auto hash = PLSUndoRedoCache::GetHashValue(strokes);
	if (!hash)
		return;

	if (!cacheStrokeImages.NeedInsertCache(hash))
		return;

	writeInfo = CacheWriteInfo();
	obs_queue_task(OBS_TASK_GRAPHICS, SaveTextureCallback, this, true);

	if (writeInfo.result)
		cacheStrokeImages.InsertCache(hash, writeInfo.linesize, writeInfo.height, writeInfo.data);
}

void PLSDrawPenCore::ReadTexture()
{
	obs_enter_graphics();

	if (saveStage && strokesTexture) {
		profile_start(key_save_cache);

		writeInfo.width = gs_texture_get_width(strokesTexture);
		writeInfo.height = gs_texture_get_height(strokesTexture);

		gs_stage_texture(saveStage, strokesTexture);

		uint8_t *src = NULL;
		uint32_t linesize = 0;
		if (gs_stagesurface_map(saveStage, &src, &linesize)) {
			auto length = linesize * writeInfo.height;
			if (length > readBufferSize || !readBuffer) {
				readBufferSize = 0;
				readBuffer = nullptr;
			}

			if (!readBuffer) {
				auto ptr = new (std::nothrow) uint8_t[length];
				assert(ptr);
				if (ptr) {
					readBufferSize = length;
					readBuffer = std::shared_ptr<uint8_t>(ptr, std::default_delete<uint8_t[]>());
				}
			}

			if (readBuffer && readBufferSize >= length) {
				memcpy(readBuffer.get(), src, length);
				writeInfo.linesize = linesize;
				writeInfo.data = readBuffer;
				writeInfo.result = true;
			}

			gs_stagesurface_unmap(saveStage);
		}

		profile_end(key_save_cache);
	}

	obs_leave_graphics();
}

bool PLSDrawPenCore::ReadCacheImage(gs_texture_t *target)
{
	if (!target || !syncTexture) {
		assert(false);
		return false;
	}

	std::vector<Stroke> strokes = PLSDrawPenMgr::Instance()->GetStrokes();
	if (strokes.empty())
		return false;

	auto hash = PLSUndoRedoCache::GetHashValue(strokes);
	if (!hash)
		return false;

	readInfo = CacheReadInfo();
	readInfo.target = target;

	uint32_t length = 0;
	readInfo.data = cacheStrokeImages.ReadCache(hash, readInfo.linesize, length);
	if (!readInfo.data || !readInfo.linesize)
		return false;

	obs_queue_task(OBS_TASK_GRAPHICS, ReadTextureCallback, this, true);
	return readInfo.result;
}

void PLSDrawPenCore::WriteTexture()
{
	obs_enter_graphics();
	if (syncTexture && readInfo.target && readInfo.data.get() && readInfo.linesize > 0) {
		profile_start(key_read_cache);
		gs_texture_set_image(syncTexture, readInfo.data.get(), readInfo.linesize, false);
		CopyTexture(readInfo.target, syncTexture);
		profile_end(key_read_cache);

		readInfo.result = true;
	}
	obs_leave_graphics();
}