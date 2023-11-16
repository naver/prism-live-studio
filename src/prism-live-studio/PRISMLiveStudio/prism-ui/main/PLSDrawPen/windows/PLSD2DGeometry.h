#pragma once
#include "PLSGraphicsHandler.h"
#include "obs.h"

class PLSD2DGeometry {
public:
	static void BezierCurve(ID2D1PathGeometry *pathGeometry, const PointFs &ctrlPts, PointF point);

	static ID2D1Geometry *CalculateCurveGeometry(std::vector<PointF> points, bool profile = false);
	static ID2D1Geometry *CalculateLineGeometry(std::vector<PointF> points);
	static ID2D1Geometry *CalculateRectangleGeometry(std::vector<PointF> points);
	static ID2D1Geometry *CalculateEllipseGeometry(std::vector<PointF> points);
	static ID2D1Geometry *CalculateArrowGeometry(std::vector<PointF> points, FLOAT line);
	static ID2D1Geometry *CalculateTriangleGeometry(std::vector<PointF> points);
};
