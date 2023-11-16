#include <qobject.h>
#include "PLSDrawPenWin.h"
#include "PLSD2DGeometry.h"
#include "../PLSDrawPenMgr.h"

constexpr auto curvegeometry = "drawpen calculate_curve_geometry";
constexpr auto linegeometry = "drawpen calculate_line_geometry";
constexpr auto rectanglegeometry = "drawpen calculate_rectangle_geometry";
constexpr auto ellipsegeometry = "drawpen calculate_ellipse_geometry";
constexpr auto arrowgeometry = "drawpen calculate_arrow_geometry";
constexpr auto trianglegeometry = "drawpen calculate_triangle_geometry";

static const char *generate_guid()
{
	static std::array<char, 64> buf{0};
	GUID guid;
	if (S_OK == ::CoCreateGuid(&guid)) {
		_snprintf(buf.data(), sizeof(buf), "{%08X-%04X-%04x-%02X%02X-%02X%02X%02X%02X%02X%02X}", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
			  guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	}
	return buf.data();
}

PLSDrawPenWin::PLSDrawPenWin()
{
	points.clear();
	strokes.clear();
	undoStrokes.clear();
	redoStrokes.clear();
	connect(this, &PLSDrawPenWin::UndoDisabled, PLSDrawPenMgr::Instance(), &PLSDrawPenMgr::UndoDisabled);
	connect(this, &PLSDrawPenWin::RedoDisabled, PLSDrawPenMgr::Instance(), &PLSDrawPenMgr::RedoDisabled);
}

PLSDrawPenWin::~PLSDrawPenWin()
{
	disconnect(this, &PLSDrawPenWin::UndoDisabled, PLSDrawPenMgr::Instance(), &PLSDrawPenMgr::UndoDisabled);
	disconnect(this, &PLSDrawPenWin::RedoDisabled, PLSDrawPenMgr::Instance(), &PLSDrawPenMgr::RedoDisabled);
	points.clear();
	strokes.clear();
	undoStrokes.clear();
	redoStrokes.clear();
}

void PLSDrawPenWin::beginDraw(unsigned int brushMode, unsigned int colorMode, unsigned int thicknessMode, PointF point) {}

void PLSDrawPenWin::beginDraw(PointF point)
{
	CAutoLockCS AutoLock(lock);
	if (!points.empty())
		points.clear();
	points.push_back(point);

	SetEvent(PLSDrawPenMgr::Instance()->GetMouseMovingEvent());
	return;
}

void PLSDrawPenWin::moveTo(PointF point)
{
	CAutoLockCS AutoLock(lock);
	points.push_back(point);

	SetEvent(PLSDrawPenMgr::Instance()->GetMouseMovingEvent());
	return;
}

void PLSDrawPenWin::endDraw(PointF point)
{
	CAutoLockCS AutoLock(lock);

	if (points.empty())
		return;

	if (DrawType curDrawType = PLSDrawPenMgr::Instance()->GetCurrentDrawType(); curDrawType != DrawType::DT_RUBBER) {
		struct Stroke stroke;
		if (curDrawType == DrawType::DT_2DSHAPE) {
			if (points.size() < 2) {
				points.clear();
				return;
			}
			stroke.points.push_back(points.front());
			stroke.points.push_back(points.back());
		} else {
			stroke.points.assign(points.begin(), points.end());
		}

		stroke.drawType = curDrawType;
		stroke.lineWidth = PLSDrawPenMgr::Instance()->GetLineWidth();
		stroke.shapeType = PLSDrawPenMgr::Instance()->GetCurrentShapeType();
		stroke.rgba = PLSDrawPenMgr::Instance()->GetColor();
		stroke.id = generate_guid();
		stroke.geometry = calculateGeometry(points);
		strokes.push_back(stroke);
		pushUndoStrokes(stroke);
		if (!redoStrokes.empty()) {
			redoStrokes.clear();
			emit RedoDisabled(true);
		}
	}

	SetEvent(PLSDrawPenMgr::Instance()->GetMouseReleaseEvent());

	points.clear();
	return;
}

void PLSDrawPenWin::eraseOn(PointF point)
{
	CAutoLockCS AutoLock(lock);
	if (!points.empty())
		points.clear();
	points.push_back(point);
	SetEvent(PLSDrawPenMgr::Instance()->GetRubberEvent());
}

void PLSDrawPenWin::undo()
{
	CAutoLockCS AutoLock(lock);
	if (undoStrokes.empty())
		return;

	auto item = undoStrokes.back();

	if (item.show) {
		auto finder = [item](Stroke const &stroke) { return item.id == stroke.id; };
		auto it = std::find_if(strokes.begin(), strokes.end(), finder);
		if (it != strokes.end()) {
			strokes.erase(it);
		}
	} else {
		if (item.batchStrokes.empty()) {
			if (item.index >= 0 && item.index < strokes.size())
				strokes.insert(strokes.begin() + item.index, item);
			else
				strokes.push_back(item);
		} else {
			for (const auto &it : item.batchStrokes) {
				strokes.push_back(it);
			}
		}
	}
	pushRedoStrokes(item);
	popUndoStrokes();

	SetEvent(PLSDrawPenMgr::Instance()->GetStrokeChangedEvent());
	return;
}

void PLSDrawPenWin::redo()
{
	CAutoLockCS AutoLock(lock);
	if (redoStrokes.empty())
		return;
	auto item = redoStrokes.back();
	if (item.show)
		strokes.push_back(item);
	else {
		if (item.batchStrokes.empty()) {
			auto finder = [item](Stroke const &stroke) { return item.id == stroke.id; };
			auto it = std::find_if(strokes.begin(), strokes.end(), finder);
			if (it != strokes.end()) {
				strokes.erase(it);
			}
		} else {
			strokes.clear();
		}
	}

	pushUndoStrokes(item);
	popRedoStrokes();

	SetEvent(PLSDrawPenMgr::Instance()->GetStrokeChangedEvent());
	return;
}

void PLSDrawPenWin::clear()
{
	CAutoLockCS AutoLock(lock);
	if (strokes.empty())
		return;

	Stroke bStroke;
	bStroke.show = false;
	bStroke.id = generate_guid();
	for (const auto &item : strokes) {
		bStroke.batchStrokes.push_back(item);
	}
	pushUndoStrokes(bStroke);

	strokes.clear();
	if (!redoStrokes.empty()) {
		redoStrokes.clear();
		emit RedoDisabled(true);
	}

	SetEvent(PLSDrawPenMgr::Instance()->GetStrokeChangedEvent());
	return;
}

void PLSDrawPenWin::resize(float width, float height)
{
	if (visible()) {
		SetEvent(PLSDrawPenMgr::Instance()->GetStrokeChangedEvent());
	}
}

void PLSDrawPenWin::setVisible(bool visible)
{
	if (visible)
		SetEvent(PLSDrawPenMgr::Instance()->GetStrokeChangedEvent());
	drawVisible = visible;
}

void PLSDrawPenWin::drawPens() {}

void PLSDrawPenWin::setCallback(void *context, DrawPenCallBacak cb) {}

void PLSDrawPenWin::RemoveStroke(std::string const &id)
{
	CAutoLockCS AutoLock(lock);

	auto finder = [id](Stroke const &stroke) { return id == stroke.id; };
	auto item = std::find_if(strokes.begin(), strokes.end(), finder);
	if (item != strokes.end()) {
		item->show = false;
		item->index = std::distance(strokes.begin(), item);
		pushUndoStrokes(*item);
		strokes.erase(item);
		if (!redoStrokes.empty()) {
			redoStrokes.clear();
			emit RedoDisabled(true);
		}
	}
	SetEvent(PLSDrawPenMgr::Instance()->GetStrokeChangedEvent());
	return;
}

std::vector<PointF> PLSDrawPenWin::GetPoints()
{
	CAutoLockCS AutoLock(lock);

	return points;
}

std::vector<Stroke> PLSDrawPenWin::GetStrokes()
{
	CAutoLockCS AutoLock(lock);
	return strokes;
}

ID2D1Geometry *PLSDrawPenWin::calculateGeometry(const std::vector<PointF> &points_)
{
	ID2D1Geometry *geometry = nullptr;
	switch (PLSDrawPenMgr::Instance()->GetCurrentDrawType()) {
	case DrawType::DT_PEN:
		profile_start(curvegeometry);
		geometry = PLSD2DGeometry::CalculateCurveGeometry(points_, true);
		profile_end(curvegeometry);
		break;
	case DrawType::DT_HIGHLIGHTER:
		profile_start(curvegeometry);
		geometry = PLSD2DGeometry::CalculateCurveGeometry(points_, true);
		profile_end(curvegeometry);
		break;
	case DrawType::DT_GLOW_PEN:
		profile_start(curvegeometry);
		geometry = PLSD2DGeometry::CalculateCurveGeometry(points_, true);
		profile_end(curvegeometry);
		break;
	case DrawType::DT_2DSHAPE:
		switch (PLSDrawPenMgr::Instance()->GetCurrentShapeType()) {
		case ShapeType::ST_STRAIGHT_ARROW:
			profile_start(arrowgeometry);
			geometry = PLSD2DGeometry::CalculateArrowGeometry(points_, (FLOAT)PLSDrawPenMgr::Instance()->GetLineWidth());
			profile_end(arrowgeometry);
			break;
		case ShapeType::ST_LINE:
			profile_start(linegeometry);
			geometry = PLSD2DGeometry::CalculateLineGeometry(points_);
			profile_end(linegeometry);
			break;
		case ShapeType::ST_RECTANGLE:
			profile_start(rectanglegeometry);
			geometry = PLSD2DGeometry::CalculateRectangleGeometry(points_);
			profile_end(rectanglegeometry);
			break;
		case ShapeType::ST_ROUND:
			profile_start(ellipsegeometry);
			geometry = PLSD2DGeometry::CalculateEllipseGeometry(points_);
			profile_end(ellipsegeometry);
			break;
		case ShapeType::TRIANGLE:
			profile_start(trianglegeometry);
			geometry = PLSD2DGeometry::CalculateTriangleGeometry(points_);
			profile_end(trianglegeometry);
			break;
		default:
			break;
		}
		break;
	case DrawType::DT_RUBBER:
		break;
	default:
		break;
	}
	return geometry;
}

void PLSDrawPenWin::pushUndoStrokes(const Stroke &stroke)
{
	undoStrokes.push_back(stroke);
	if (undoStrokes.size() == 1)
		emit UndoDisabled(false);
}

void PLSDrawPenWin::pushRedoStrokes(const Stroke &stroke)
{
	redoStrokes.push_back(stroke);
	if (redoStrokes.size() == 1)
		emit RedoDisabled(false);
}

void PLSDrawPenWin::popUndoStrokes()
{
	undoStrokes.pop_back();
	if (undoStrokes.empty())
		emit UndoDisabled(true);
}

void PLSDrawPenWin::popRedoStrokes()
{
	redoStrokes.pop_back();
	if (redoStrokes.empty())
		emit RedoDisabled(true);
}
