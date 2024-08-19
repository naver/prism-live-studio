#include "PLSDrawPenCore.h"
#include "../PLSDrawPenMgr.h"
#include <string>
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

PLSDrawPenCore::PLSDrawPenCore()
{
	exitEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

	PLSGraphicsHandler::Instance()->CreateDevice();

	if (!d2dRenderTarget)
		d2dRenderTarget = std::make_shared<PLSD2DRenderTarget>();

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
		obs_enter_graphics();
		if (renderTexture) {
			gs_texture_destroy(renderTexture);
			renderTexture = nullptr;
		}

		if (!renderTexture) {
			renderTexture = gs_texture_create(w, h, GS_BGRA, 1, nullptr, GS_RENDER_TARGET);
		}

		if (strokesTexture) {
			gs_texture_destroy(strokesTexture);
			strokesTexture = nullptr;
		}

		if (!strokesTexture) {
			strokesTexture = gs_texture_create(w, h, GS_BGRA, 1, nullptr, GS_RENDER_TARGET);
		}

		if (drawingTexture) {
			gs_texture_destroy(drawingTexture);
			drawingTexture = nullptr;
		}

		if (!drawingTexture) {
			drawingTexture = gs_texture_create(w, h, GS_BGRA, 1, nullptr, GS_RENDER_TARGET);
		}

		if (tempTexture) {
			gs_texture_destroy(tempTexture);
			tempTexture = nullptr;
		}

		if (!tempTexture) {
			tempTexture = gs_texture_create(w, h, GS_BGRA, 1, nullptr, GS_RENDER_TARGET);
		}

		obs_leave_graphics();

		if (d2dRenderTarget)
			d2dRenderTarget->ResizeSharedTexture(w, h);
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
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), drawingTexture, it.rgba, GlowRange, true);
		break;
	case DrawType::DT_HIGHLIGHTER:
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), drawingTexture, it.rgba, GlowRange, true);
		break;
	case DrawType::DT_GLOW_PEN:
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), drawingTexture, it.rgba, (FLOAT)it.lineWidth * 0.5f, true);
		obs_enter_graphics();
		profile_start(drawing_flush_glow);
		gs_flush();
		profile_end(drawing_flush_glow);
		obs_leave_graphics();
		break;
	case DrawType::DT_2DSHAPE:
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), drawingTexture, it.rgba, GlowRange, true);
		break;
	default:
		break;
	}
	return;
}

void PLSDrawPenCore::UpdateCanvasByVisible(bool visible)
{
	auto main = OBSBasic::Get();
	if (!main)
		return;
	if(visible)
		pls_scene_update_canvas(main->GetCurrentScene(), renderTexture, false);
	else
		pls_scene_update_canvas(main->GetCurrentScene(), nullptr, false);
}

void PLSDrawPenCore::DrawRender()
{
	std::array<HANDLE, 6> events = {
		exitEvent,
		PLSDrawPenMgr::Instance()->GetMouseMovingEvent(),
		PLSDrawPenMgr::Instance()->GetMouseReleaseEvent(),
		PLSDrawPenMgr::Instance()->GetStrokeChangedEvent(),
		PLSDrawPenMgr::Instance()->GetRubberEvent(),
		PLSDrawPenMgr::Instance()->GetVisibleEvent(),
	};

	CheckDeviceRebuilt();

	while (true) {
		auto dwEvent = WaitForMultipleObjects(6, events.data(), FALSE, INFINITE);
		// events[0] : exitEvent was signaled
		if (dwEvent == WAIT_OBJECT_0)
			break;

		if (dwEvent == WAIT_OBJECT_0 + 1) {
			CheckUpdateStrokesTexture();
			if (RenderDrawingToTarget(drawingTexture)) {
				BlendTexturesToTarget(drawingTexture, strokesTexture, renderTexture);
				UpdateCanvas();
			}
			ResetEvent(PLSDrawPenMgr::Instance()->GetMouseMovingEvent());
		} else if (dwEvent == WAIT_OBJECT_0 + 2) {
			CopyTexture(strokesTexture, renderTexture);
			UpdateCanvas(true);
			ResetEvent(PLSDrawPenMgr::Instance()->GetMouseReleaseEvent());
		} else if (dwEvent == WAIT_OBJECT_0 + 3) {
			ResetEvent(PLSDrawPenMgr::Instance()->GetStrokeChangedEvent());
			RenderStrokesToTarget(strokesTexture);
			CopyTexture(renderTexture, strokesTexture);
			UpdateCanvas(true);
		} else if (dwEvent == WAIT_OBJECT_0 + 4) {
			CheckRubberHitsStroke();
			ResetEvent(PLSDrawPenMgr::Instance()->GetRubberEvent());
		} else if (dwEvent == WAIT_OBJECT_0 + 5) {
			ResetEvent(PLSDrawPenMgr::Instance()->GetVisibleEvent());
			RenderCanvesTextureToTarget(renderTexture);
		}
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
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), target, curRgba, GlowRange, true);
		break;
	case DrawType::DT_HIGHLIGHTER:
		d2dRenderTarget->DrawCurve(points, curRgba, curLine, false, Offset);
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), target, curRgba, GlowRange, true);

		CopyTexture(tempTexture, target);
		if (highlighterEffect)
			highlighterEffect->RenderEffect(tempTexture, target, true, Alpha);
		break;
	case DrawType::DT_GLOW_PEN:
		d2dRenderTarget->DrawCurve(points, color_to_int(255, 255, 255, 255), curLine);
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), target, curRgba, curLine * 0.5f, true);
		break;
	case DrawType::DT_2DSHAPE:
		d2dRenderTarget->Draw2DShape(points, curShape, curRgba, curLine);
		glowEffect->RenderEffect(d2dRenderTarget->GetSharedTexture(), target, curRgba, GlowRange, true);
		break;
	default:
		break;
	}
	return true;
}

static void render_highlighter(void *data)
{
	auto core = (PLSDrawPenCore *)data;
	if (core) {
		core->StrokeRenderHighlighterCallback();
	}
	return;
}

static void blend_textures(void *data)
{
	auto core = (PLSDrawPenCore *)data;
	if (core) {
		core->StrokeBlendTexturesCallback();
	}
	return;
}

static void render_stroke(void *data)
{
	auto core = (PLSDrawPenCore *)data;
	if (core)
		core->StrokeRenderCallback();
	return;
}

//render drawn strokes to strokesTexture
void PLSDrawPenCore::RenderStrokesToTarget(gs_texture_t *target)
{
	if (!d2dRenderTarget || !glowEffect)
		return;

	ClearTexture(target);

	bool drawBegin = false;
	std::vector<Stroke> strokes = PLSDrawPenMgr::Instance()->GetStrokes();
	Stroke nextStroke = strokes.empty() ? Stroke() : strokes.front();
	for (size_t index = 0; index < strokes.size(); ++index) {
		auto it = strokes[index];
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
			d2dRenderTarget->DrawGeometry(it.geometry, color_to_int(255, 255, 255, 255), (FLOAT)it.lineWidth, drawing_glowpen);
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

		nextStroke = (index + 1) < strokes.size() ? strokes[index + 1] : Stroke();
		bool needRender = nextStroke.drawType != it.drawType || nextStroke.rgba != it.rgba || (it.drawType == DrawType::DT_GLOW_PEN && nextStroke.lineWidth != it.lineWidth);
		if (needRender) {
			d2dRenderTarget->DrawEnd();
			drawBegin = false;
			obs_queue_task(OBS_TASK_GRAPHICS, render_stroke, this, true);
			if (it.drawType == DrawType::DT_HIGHLIGHTER)
				obs_queue_task(OBS_TASK_GRAPHICS, render_highlighter, this, true);
			obs_queue_task(OBS_TASK_GRAPHICS, blend_textures, this, true);
		}
	}
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
