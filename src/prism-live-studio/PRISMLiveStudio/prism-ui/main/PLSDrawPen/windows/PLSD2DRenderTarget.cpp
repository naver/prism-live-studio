#include "PLSD2DRenderTarget.h"
#include "../PLSDrawPenMgr.h"
#include "PLSD2DGeometry.h"

PLSD2DRenderTarget::~PLSD2DRenderTarget()
{
	obs_enter_graphics();
	gs_texture_destroy(sharedTexture);
	obs_leave_graphics();

	d2d1RenderTarget.Release();
	d2d1ColorBrush.Release();
	d2d1RoundStrokeStyle.Release();
	d2d1StraightStrokeStyle.Release();
}

bool PLSD2DRenderTarget::DrawBegin() const
{
	if (!d2d1RenderTarget)
		return false;

	d2d1RenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	d2d1RenderTarget->BeginDraw();
	d2d1RenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
	d2d1RenderTarget->Clear();
	return true;
}

void PLSD2DRenderTarget::DrawEnd() const
{
	ComPtr<ID3D11DeviceContext> d3d11DeviceContext = PLSGraphicsHandler::Instance()->GetD3DContext();
	if (!d2d1RenderTarget || !d3d11DeviceContext)
		return;
	d2d1RenderTarget->EndDraw();
	d3d11DeviceContext->Flush();
}

void PLSD2DRenderTarget::DrawCurve(const std::vector<PointF> &points, uint32_t rgba, FLOAT line, bool round, int offset) const
{
	if (!d2d1RenderTarget || !d2d1ColorBrush || points.empty())
		return;

	DrawBegin();

	auto pathGeometry = (ID2D1PathGeometry *)(PLSD2DGeometry::CalculateCurveGeometry(points));
	if (!pathGeometry) {
		DrawEnd();
		return;
	}

	D2D1_COLOR_F color;
	colorf_from_rgba(color.r, color.g, color.b, color.a, rgba);
	d2d1ColorBrush->SetColor(color);
	line += (FLOAT)offset;

	if (round)
		d2d1RenderTarget->DrawGeometry(pathGeometry, d2d1ColorBrush, line, d2d1RoundStrokeStyle);
	else
		d2d1RenderTarget->DrawGeometry(pathGeometry, d2d1ColorBrush, line, d2d1StraightStrokeStyle);

	DrawEnd();

	return;
}

void PLSD2DRenderTarget::Draw2DShape(const std::vector<PointF> &points, ShapeType type, uint32_t rgba, FLOAT line) const
{
	DrawBegin();
	if (points.size() < 2) {
		DrawEnd();
		return;
	}

	switch (type) {
	case ShapeType::ST_STRAIGHT_ARROW:
		DrawArrow(points, rgba, line);
		break;
	case ShapeType::ST_LINE:
		DrawLine(points, rgba, line);
		break;
	case ShapeType::ST_RECTANGLE:
		DrawRectangle(points, rgba, line);
		break;
	case ShapeType::ST_ROUND:
		DrawEllipse(points, rgba, line);
		break;
	case ShapeType::TRIANGLE:
		DrawTriangle(points, rgba, line);
		break;
	default:
		break;
	}

	DrawEnd();

	return;
}

void PLSD2DRenderTarget::DrawLine(const std::vector<PointF> &points, uint32_t rgba, FLOAT line) const
{
	if (!d2d1RenderTarget || !d2d1ColorBrush || points.empty())
		return;

	auto pathGeometry = (ID2D1PathGeometry *)(PLSD2DGeometry::CalculateLineGeometry(points));
	if (!pathGeometry)
		return;

	D2D1_COLOR_F color;
	colorf_from_rgba(color.r, color.g, color.b, color.a, rgba);

	d2d1ColorBrush->SetColor(color);
	d2d1RenderTarget->DrawGeometry(pathGeometry, d2d1ColorBrush, line, d2d1RoundStrokeStyle);
	return;
}

void PLSD2DRenderTarget::DrawRectangle(const std::vector<PointF> &points, uint32_t rgba, FLOAT line) const
{
	if (!d2d1RenderTarget || !d2d1ColorBrush)
		return;

	auto rectangleGeometry = (ID2D1RectangleGeometry *)(PLSD2DGeometry::CalculateRectangleGeometry(points));
	if (!rectangleGeometry)
		return;

	D2D1_COLOR_F color;
	colorf_from_rgba(color.r, color.g, color.b, color.a, rgba);

	d2d1ColorBrush->SetColor(color);
	d2d1RenderTarget->DrawGeometry(rectangleGeometry, d2d1ColorBrush, line, d2d1RoundStrokeStyle);

	return;
}

void PLSD2DRenderTarget::DrawEllipse(const std::vector<PointF> &points, uint32_t rgba, FLOAT line) const
{
	if (!d2d1RenderTarget || !d2d1ColorBrush)
		return;

	auto ellipseGeometry = (ID2D1EllipseGeometry *)(PLSD2DGeometry::CalculateEllipseGeometry(points));
	if (!ellipseGeometry)
		return;

	D2D1_COLOR_F color;
	colorf_from_rgba(color.r, color.g, color.b, color.a, rgba);

	d2d1ColorBrush->SetColor(color);
	d2d1RenderTarget->DrawGeometry(ellipseGeometry, d2d1ColorBrush, line, d2d1RoundStrokeStyle);

	return;
}

void PLSD2DRenderTarget::DrawArrow(const std::vector<PointF> &points, uint32_t rgba, FLOAT line) const
{
	if (!d2d1RenderTarget || !d2d1ColorBrush)
		return;

	auto groupGeometry = (ID2D1GeometryGroup *)(PLSD2DGeometry::CalculateArrowGeometry(points, line));
	if (!groupGeometry)
		return;

	D2D1_COLOR_F color;
	colorf_from_rgba(color.r, color.g, color.b, color.a, rgba);
	d2d1ColorBrush->SetColor(color);

	std::array<ID2D1Geometry *, 2> ppGeometries = {};
	groupGeometry->GetSourceGeometries(ppGeometries.data(), 2);
	if (ppGeometries[1])
		d2d1RenderTarget->FillGeometry(ppGeometries[1], d2d1ColorBrush);

	d2d1RenderTarget->DrawGeometry(groupGeometry, d2d1ColorBrush, line, d2d1RoundStrokeStyle);

	return;
}

void PLSD2DRenderTarget::DrawTriangle(const std::vector<PointF> &points, uint32_t rgba, FLOAT line) const
{
	if (!d2d1RenderTarget || !d2d1ColorBrush || points.empty())
		return;

	auto pathGeometry = (ID2D1PathGeometry *)(PLSD2DGeometry::CalculateTriangleGeometry(points));
	if (!pathGeometry)
		return;

	D2D1_COLOR_F color;
	colorf_from_rgba(color.r, color.g, color.b, color.a, rgba);

	d2d1ColorBrush->SetColor(color);
	d2d1RenderTarget->DrawGeometry(pathGeometry, d2d1ColorBrush, line, d2d1RoundStrokeStyle);
	return;
}

void PLSD2DRenderTarget::DrawGeometry(ID2D1Geometry *geometry, uint32_t rgba, FLOAT line, const char *profile, bool round, int offset) const
{
	if (!d2d1RenderTarget || !geometry) {
		return;
	}

	if (profile)
		profile_start(profile);

	D2D1_COLOR_F color;
	colorf_from_rgba(color.r, color.g, color.b, color.a, rgba);
	d2d1ColorBrush->SetColor(color);
	line += (FLOAT)offset;

	if (round)
		d2d1RenderTarget->DrawGeometry(geometry, d2d1ColorBrush, line, d2d1RoundStrokeStyle);
	else
		d2d1RenderTarget->DrawGeometry(geometry, d2d1ColorBrush, line, d2d1StraightStrokeStyle);

	if (profile)
		profile_end(profile);
	return;
}

void PLSD2DRenderTarget::DrawArrowGeometry(ID2D1Geometry *geometry, uint32_t rgba, FLOAT line, const char *profile, bool, int offset) const
{
	if (!d2d1RenderTarget || !geometry) {
		return;
	}

	if (profile)
		profile_start(profile);

	D2D1_COLOR_F color;
	colorf_from_rgba(color.r, color.g, color.b, color.a, rgba);
	d2d1ColorBrush->SetColor(color);
	line += (FLOAT)offset;

	std::array<ID2D1Geometry *, 2> ppGeometries{};
	auto groupGeometry = (ID2D1GeometryGroup *)(geometry);
	groupGeometry->GetSourceGeometries(ppGeometries.data(), 2);
	if (ppGeometries[1])
		d2d1RenderTarget->FillGeometry(ppGeometries[1], d2d1ColorBrush);

	d2d1RenderTarget->DrawGeometry(geometry, d2d1ColorBrush, line, d2d1RoundStrokeStyle);

	if (profile)
		profile_end(profile);
	return;
}

void PLSD2DRenderTarget::Destroy()
{
	inited = false;
	sharedHandle = nullptr;
	obs_enter_graphics();
	if (sharedTexture) {
		gs_texture_destroy(sharedTexture);
		sharedTexture = nullptr;
	}
	obs_leave_graphics();
}

void PLSD2DRenderTarget::ResizeSharedTexture(uint32_t w, uint32_t h)
{
	obs_enter_graphics();
	uint32_t cx = sharedTexture ? gs_texture_get_width(sharedTexture) : 0;
	uint32_t cy = sharedTexture ? gs_texture_get_height(sharedTexture) : 0;
	obs_leave_graphics();
	if (w != cx || h != cy) {
		ResetRenderTarget();
	}
	return;
}

gs_texture_t *PLSD2DRenderTarget::GetSharedTexture()
{
	if (!sharedTexture) {
		obs_enter_graphics();
		auto handle = (uint64_t)sharedHandle;
		sharedTexture = gs_texture_open_shared((uint32_t)handle);
		obs_leave_graphics();
	}
	return sharedTexture;
}

bool PLSD2DRenderTarget::ResetRenderTarget()
{
	uint32_t w = 0;
	uint32_t h = 0;
	PLSDrawPenMgr::Instance()->GetSize(w, h);
	if (!w || !h)
		return false;

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = w;
	desc.Height = h;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.CPUAccessFlags = 0;

	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.Usage = D3D11_USAGE_DEFAULT;
	//set share texture
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	ComPtr<ID3D11Device> d3d11Device = PLSGraphicsHandler::Instance()->GetD3D11Device();
	if (!d3d11Device)
		return false;

	ComPtr<ID3D11Texture2D> texture = nullptr;
	HRESULT hr = d3d11Device->CreateTexture2D(&desc, nullptr, texture.Assign());
	if (FAILED(hr) || !texture) {
		return false;
	}

	ComPtr<IDXGIResource> dxgiRes = nullptr;
	hr = texture->QueryInterface(__uuidof(IDXGIResource), (LPVOID *)(dxgiRes.Assign()));
	if (FAILED(hr)) {
		return false;
	}

	hr = dxgiRes->GetSharedHandle(&sharedHandle);
	if (FAILED(hr)) {
		return false;
	}

	texture->GetDesc(&desc);

	ComPtr<IDXGISurface1> dxgiSurface = nullptr;
	hr = d3d11Device->OpenSharedResource(sharedHandle, __uuidof(IDXGISurface1), (LPVOID *)(dxgiSurface.Assign()));
	if (FAILED(hr)) {
		return false;
	}

	ComPtr<ID2D1Factory> d2d1Factory = PLSGraphicsHandler::Instance()->GetD2DFactory();
	if (!d2d1Factory)
		return false;

	auto dsProps = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));
	hr = d2d1Factory->CreateDxgiSurfaceRenderTarget(dxgiSurface.Get(), &dsProps, &d2d1RenderTarget);
	if (FAILED(hr)) {
		return false;
	}

	hr = d2d1RenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), d2d1ColorBrush.Assign());
	if (FAILED(hr)) {
		return false;
	}

	hr = d2d1Factory->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND, 1.0f, D2D1_DASH_STYLE_SOLID, 10.0f),
					    nullptr, 0, d2d1RoundStrokeStyle.Assign());
	if (FAILED(hr)) {
		return false;
	}

	hr = d2d1Factory->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_SQUARE, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_LINE_JOIN_BEVEL, 1.0f, D2D1_DASH_STYLE_SOLID, 10.0f),
					    nullptr, 0, d2d1StraightStrokeStyle.Assign());
	if (FAILED(hr)) {
		return false;
	}

	obs_enter_graphics();
	if (sharedTexture) {
		gs_texture_destroy(sharedTexture);
		sharedTexture = nullptr;
	}
	auto handle = (uint64_t)sharedHandle;
	sharedTexture = gs_texture_open_shared((uint32_t)handle);
	obs_leave_graphics();

	inited = true;
	return true;
}
