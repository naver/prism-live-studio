#include "PLSD2DGeometry.h"
#include "../PLSDrawPenMgr.h"
#include "../PLSDrawPenDefine.h"

constexpr auto smoothcurve = "drawpen smoothcurve";

// Brush smoothing was removed; use the raw stroke points as-is, connected by
// straight (degenerate cubic bezier) segments so BezierCurve keeps working unchanged.
static void pls_build_raw_ctrl_pts(PointFs &ctrlPts, const PointFs &points)
{
	if (points.empty()) {
		return;
	}
	if (points.size() == 1) {
		for (int i = 0; i < 4; i++) {
			ctrlPts.push_back(points[0]);
		}
		return;
	}
	for (size_t i = 0; i + 1 < points.size(); i++) {
		ctrlPts.push_back(points[i]);
		ctrlPts.push_back(points[i]);
		ctrlPts.push_back(points[i + 1]);
		ctrlPts.push_back(points[i + 1]);
	}
}

void PLSD2DGeometry::BezierCurve(ID2D1PathGeometry *pathGeometry, const PointFs &ctrlPts, PointF point)
{
	if (!pathGeometry)
		return;
	ID2D1GeometrySink *pSink = nullptr;
	HRESULT hr = pathGeometry->Open(&pSink);
	if (SUCCEEDED(hr)) {
		D2D1_POINT_2F start = {point.x, point.y};
		pSink->BeginFigure(start, D2D1_FIGURE_BEGIN_FILLED);

		uint64_t i = 0;
		while (i < ctrlPts.size() && !(ctrlPts.size() % 4)) {

			D2D1_POINT_2F c1 = {ctrlPts[i + 1].x, ctrlPts[i + 1].y};
			D2D1_POINT_2F c2 = {ctrlPts[i + 2].x, ctrlPts[i + 2].y};
			D2D1_POINT_2F end = {ctrlPts[i + 3].x, ctrlPts[i + 3].y};
			pSink->AddBezier(D2D1::BezierSegment(c1, c2, end));
			i += 4;
		}

		pSink->EndFigure(D2D1_FIGURE_END_OPEN);
		pSink->Close();
		pSink->Release();
	}

	return;
}

ID2D1Geometry *PLSD2DGeometry::CalculateCurveGeometry(std::vector<PointF> points, bool profile)
{
	if (points.empty())
		return nullptr;

	ComPtr<ID2D1Factory> d2d1Factory = PLSGraphicsHandler::Instance()->GetD2DFactory();
	if (!d2d1Factory)
		return nullptr;

	ID2D1PathGeometry *pathGeometry = nullptr;
	HRESULT hr = d2d1Factory->CreatePathGeometry(&pathGeometry);
	if (FAILED(hr)) {
		return pathGeometry;
	}

	if (profile)
		profile_start(smoothcurve);

	PointFs ctrlPts;
	pls_build_raw_ctrl_pts(ctrlPts, points);

	if (profile)
		profile_end(smoothcurve);

	BezierCurve(pathGeometry, ctrlPts, points.front());
	ctrlPts.clear();

	return pathGeometry;
}

ID2D1Geometry *PLSD2DGeometry::CalculateLineGeometry(std::vector<PointF> points)
{
	if (points.empty())
		return nullptr;

	ComPtr<ID2D1Factory> d2d1Factory = PLSGraphicsHandler::Instance()->GetD2DFactory();
	if (!d2d1Factory)
		return nullptr;

	ID2D1PathGeometry *pathGeometry = nullptr;
	HRESULT hr = d2d1Factory->CreatePathGeometry(&pathGeometry);
	if (FAILED(hr)) {
		return pathGeometry;
	}

	ID2D1GeometrySink *pSink = nullptr;
	hr = pathGeometry->Open(&pSink);
	if (SUCCEEDED(hr)) {
		D2D1_POINT_2F start = {points.front().x, points.front().y};
		pSink->BeginFigure(start, D2D1_FIGURE_BEGIN_FILLED);
		pSink->AddLine(D2D1::Point2F(points.back().x, points.back().y));
		pSink->EndFigure(D2D1_FIGURE_END_OPEN);
		pSink->Close();
		pSink->Release();
	}

	return pathGeometry;
}

ID2D1Geometry *PLSD2DGeometry::CalculateRectangleGeometry(std::vector<PointF> points)
{
	if (points.empty())
		return nullptr;

	auto d2d1Factory = PLSGraphicsHandler::Instance()->GetD2DFactory();
	if (!d2d1Factory)
		return nullptr;
	ID2D1RectangleGeometry *rectangleGeometry = nullptr;
	HRESULT hr = d2d1Factory->CreateRectangleGeometry(D2D1::RectF(points.front().x, points.front().y, points.back().x, points.back().y), &rectangleGeometry);
	if (FAILED(hr)) {
		return nullptr;
	}
	return rectangleGeometry;
}

ID2D1Geometry *PLSD2DGeometry::CalculateEllipseGeometry(std::vector<PointF> points)
{
	if (points.empty())
		return nullptr;

	ComPtr<ID2D1Factory> d2d1Factory = PLSGraphicsHandler::Instance()->GetD2DFactory();
	if (!d2d1Factory)
		return nullptr;

	ID2D1EllipseGeometry *ellipseGeometry = nullptr;
	FLOAT radiusX = (points.back().x - points.front().x) / 2;
	FLOAT radiusY = (points.back().y - points.front().y) / 2;
	D2D1_POINT_2F center = D2D1::Point2F(points.front().x + radiusX, points.front().y + radiusY);

	HRESULT hr = d2d1Factory->CreateEllipseGeometry(D2D1::Ellipse(center, std::abs(radiusX), std::abs(radiusY)), &ellipseGeometry);
	if (FAILED(hr)) {
		return nullptr;
	}
	return ellipseGeometry;
}

ID2D1Geometry *PLSD2DGeometry::CalculateArrowGeometry(std::vector<PointF> points, FLOAT line)
{
	if (points.empty())
		return nullptr;

	ComPtr<ID2D1Factory> d2d1Factory = PLSGraphicsHandler::Instance()->GetD2DFactory();
	if (!d2d1Factory)
		return nullptr;

	std::array<D2D1_POINT_2F, 3> p{};
	D2D1_POINT_2F start(D2D1::Point2F(points.front().x, points.front().y));
	p[0].x = points.back().x;
	p[0].y = points.back().y;

	double slopy;
	double cosy;
	double siny;
	double length;

	length = (double)line * 5;

	slopy = atan2(((double)start.y - (double)p[0].y), ((double)start.x - (double)p[0].x));
	cosy = cos(slopy);
	siny = sin(slopy);

	p[1].x = (FLOAT)(p[0].x + length * cosy - (length / 2.0 * siny));
	p[1].y = (FLOAT)(p[0].y + length * siny + (length / 2.0 * cosy));

	p[2].x = (FLOAT)(p[0].x + length * cosy + (length / 2.0 * siny));
	p[2].y = (FLOAT)(p[0].y + length * siny - (length / 2.0 * cosy));

	ID2D1PathGeometry *triangleGeometry = nullptr;
	HRESULT hr = d2d1Factory->CreatePathGeometry(&triangleGeometry);
	if (FAILED(hr)) {
		return nullptr;
	}

	ID2D1GeometrySink *pSink = nullptr;
	hr = triangleGeometry->Open(&pSink);
	if (SUCCEEDED(hr)) {
		pSink->BeginFigure(p[0], D2D1_FIGURE_BEGIN_FILLED);
		pSink->AddLine(p[1]);
		pSink->AddLine(p[2]);
		pSink->EndFigure(D2D1_FIGURE_END_CLOSED);

		pSink->Close();
		pSink->Release();
	}

	ID2D1PathGeometry *lineGeometry = nullptr;
	hr = d2d1Factory->CreatePathGeometry(&lineGeometry);
	if (FAILED(hr)) {
		return nullptr;
	}

	ID2D1GeometrySink *pLineSink = nullptr;
	hr = lineGeometry->Open(&pLineSink);
	if (SUCCEEDED(hr)) {
		pLineSink->BeginFigure(start, D2D1_FIGURE_BEGIN_FILLED);
		pLineSink->AddLine(p[0]);
		pLineSink->EndFigure(D2D1_FIGURE_END_OPEN);
		pLineSink->Close();
		pLineSink->Release();
	}

	std::vector<ID2D1Geometry *> ppGeometries = {lineGeometry, triangleGeometry};
	ID2D1GeometryGroup *groupGeometry = nullptr;
	hr = d2d1Factory->CreateGeometryGroup(D2D1_FILL_MODE_ALTERNATE, ppGeometries.data(), (UINT32)ppGeometries.size(), &groupGeometry);
	if (FAILED(hr)) {
		return nullptr;
	}

	return groupGeometry;
}

ID2D1Geometry *PLSD2DGeometry::CalculateTriangleGeometry(std::vector<PointF> points)
{
	if (points.empty())
		return nullptr;

	ComPtr<ID2D1Factory> d2d1Factory = PLSGraphicsHandler::Instance()->GetD2DFactory();
	if (!d2d1Factory)
		return nullptr;

	ID2D1PathGeometry *pathGeometry = nullptr;
	HRESULT hr = d2d1Factory->CreatePathGeometry(&pathGeometry);
	if (FAILED(hr)) {
		return pathGeometry;
	}

	ID2D1GeometrySink *pSink = nullptr;
	hr = pathGeometry->Open(&pSink);
	if (SUCCEEDED(hr)) {

		D2D1_POINT_2F pt1 = {points.back().x, points.back().y};
		D2D1_POINT_2F pt2 = {points.front().x, points.back().y};
		FLOAT cx = points.front().x + (pt1.x - pt2.x) / 2;
		D2D1_POINT_2F pt3 = {cx, points.front().y};

		pSink->BeginFigure(pt1, D2D1_FIGURE_BEGIN_FILLED);
		pSink->AddLine(pt2);
		pSink->AddLine(pt3);
		pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
		pSink->Close();
		pSink->Release();
	}

	return pathGeometry;
}
