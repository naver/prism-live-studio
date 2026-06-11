#pragma once
#include <vector>
#include <string>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <util/windows/ComPtr.hpp>
#include "../PLSDrawPenDefine.h"

struct Stroke {
	bool show = true;
	int lineWidth;
	long long index = -1;
	uint32_t rgba;
	std::string id;
	ShapeType shapeType;
	DrawType drawType;

	ID2D1Geometry *geometry;
	std::vector<PointF> points;
	std::vector<Stroke> batchStrokes{};
};