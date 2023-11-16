#pragma once
#include "PLSGraphicsHandler.h"
#include "PLSDrawPenStroke.h"
#include <vector>
#include "obs.h"

class PLSD2DRenderTarget {
public:
	PLSD2DRenderTarget() = default;
	~PLSD2DRenderTarget();

	bool DrawBegin() const;
	void DrawEnd() const;
	void DrawCurve(const std::vector<PointF> &points, uint32_t rgba, FLOAT line, bool round = true, int offset = 0) const;
	void Draw2DShape(const std::vector<PointF> &points, ShapeType type, uint32_t rgba, FLOAT line) const;
	void DrawLine(const std::vector<PointF> &points, uint32_t rgba, FLOAT line) const;
	void DrawRectangle(const std::vector<PointF> &points, uint32_t rgba, FLOAT line) const;
	void DrawEllipse(const std::vector<PointF> &points, uint32_t rgba, FLOAT line) const;
	void DrawArrow(const std::vector<PointF> &points, uint32_t rgba, FLOAT line) const;
	void DrawTriangle(const std::vector<PointF> &points, uint32_t rgba, FLOAT line) const;

	void DrawGeometry(ID2D1Geometry *geometry, uint32_t rgba, FLOAT line, const char *profile = nullptr, bool round = true, int offset = 0) const;
	void DrawArrowGeometry(ID2D1Geometry *geometry, uint32_t rgba, FLOAT line, const char *profile = nullptr, bool round = true, int offset = 0) const;

	void Destroy();
	void ResizeSharedTexture(uint32_t w, uint32_t h);
	gs_texture_t *GetSharedTexture();

	bool ResetRenderTarget();

private:
	bool inited = false;
	HANDLE sharedHandle = nullptr;

	ComPtr<ID2D1RenderTarget> d2d1RenderTarget = nullptr;
	ComPtr<ID2D1SolidColorBrush> d2d1ColorBrush = nullptr;
	ComPtr<ID2D1StrokeStyle> d2d1RoundStrokeStyle = nullptr;
	ComPtr<ID2D1StrokeStyle> d2d1StraightStrokeStyle = nullptr;

	gs_texture_t *sharedTexture = nullptr;
};
