#include "PLSDrawPenMgr.h"
#include "PLSDrawPenView.h"
#include "window-basic-main.hpp"
#include "pls/pls-obs-api.h"
#include "pls-performance.h"
#include <QPainter>
#include <liblog.h>
#include <log/module_names.h>

#if defined(__APPLE__)
#include "mac/PLSDrawPenMac.h"
#endif

PLSDrawPenMgr *PLSDrawPenMgr::Instance()
{
	static PLSDrawPenMgr instance;
	return &instance;
}

void PLSDrawPenMgr::Release()
{
#if defined(_WIN32)
	drawPenCore.reset();
	CloseHandle(movingEvent);
	CloseHandle(releasedEvent);
	CloseHandle(strokeChangedEvent);
	CloseHandle(rubberEvent);
#endif
}

PLSDrawPenMgr::PLSDrawPenMgr()
{
#if defined(_WIN32)
	movingEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	releasedEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	strokeChangedEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	rubberEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
#endif
}

void PLSDrawPenMgr::UpdateCursorPixmap()
{
	DrawType type = GetCurrentDrawType();
	switch (type) {
	case DrawType::DT_PEN:
		UpdatePenCursor();
		break;
	case DrawType::DT_HIGHLIGHTER:
		UpdateHighlighterCursor();
		break;
	case DrawType::DT_GLOW_PEN:
		UpdateGlowCursor();
		break;
	case DrawType::DT_2DSHAPE:
		cursorPixmap.load(":/resource/images/draw-pen/ic-cursor-shap.svg");
		break;
	case DrawType::DT_RUBBER:
		cursorPixmap.load(":/resource/images/draw-pen/ic-cursor-eraser.svg");
		break;
	default:
		break;
	}
}

void PLSDrawPenMgr::UpdatePenCursor()
{
	int r;
	int g;
	int b;
	int a;
	colorI_from_rgba(r, g, b, a, GetColor());

	float sacle = 1.0f;
	sacle = OBSBasic::Get()->GetPreviewScale();

	int diameter = (int)((float)(GetLineWidth(DrawType::DT_PEN) / 2) * sacle) + 2;
	int outerDiameter = diameter + 2;
	QPixmap pixmap(outerDiameter, outerDiameter);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
	painter.setPen(QColor(Qt::transparent));

	if (r == 255 && g == 255 && b == 255)
		painter.setBrush(QBrush(QColor(18, 18, 18)));
	else
		painter.setBrush(QBrush(Qt::white));

	painter.drawEllipse(QPoint(outerDiameter / 2, outerDiameter / 2), outerDiameter / 2, outerDiameter / 2);
	QColor co(r, g, b, a);
	painter.setBrush(QBrush(co));
	painter.drawEllipse(QPoint(outerDiameter / 2, outerDiameter / 2), diameter / 2, diameter / 2);

	cursorPixmap = pixmap;
}

void PLSDrawPenMgr::UpdateHighlighterCursor()
{
	int r;
	int g;
	int b;
	int a;
	colorI_from_rgba(r, g, b, a, GetColor());

	float sacle = 1.0f;
	sacle = OBSBasic::Get()->GetPreviewScale();
	int diameter = (int)((float)(GetLineWidth(DrawType::DT_HIGHLIGHTER) / 2) * sacle) + 2;

	QPixmap pixmap((int)(diameter * 0.7), diameter + 2);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

	if (r == 255 && g == 255 && b == 255)
		painter.setBrush(QBrush(QColor(18, 18, 18)));
	else
		painter.setPen(QColor(Qt::white));

	QColor co(r, g, b, a);
	painter.setBrush(QBrush(co));
	painter.setOpacity(0.8f);
	painter.drawRect(0, 0, int(diameter * 0.7), diameter + 2);

	cursorPixmap = pixmap;
}

void PLSDrawPenMgr::UpdateGlowCursor()
{
	int r;
	int g;
	int b;
	int a;
	colorI_from_rgba(r, g, b, a, GetColor());

	float sacle = 1.0f;
	sacle = OBSBasic::Get()->GetPreviewScale();

	float weight = 0.6f;
	auto outerRadius = (int)((float)(GetLineWidth(DrawType::DT_GLOW_PEN) / 2) * sacle + 4);
	auto mediumRadius = (int)(outerRadius * 0.7);
	auto innerRadius = (int)((float)outerRadius * weight);
	QPixmap pixmap(outerRadius * 2, outerRadius * 2);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
	painter.setPen(Qt::transparent);
	QRadialGradient radiaGradient(outerRadius, outerRadius, outerRadius, outerRadius, outerRadius);
	QColor co(r, g, b, a);
	radiaGradient.setColorAt(0, co);
	radiaGradient.setColorAt(1, Qt::transparent);
	painter.setBrush(radiaGradient);
	painter.setOpacity(0.8);
	painter.drawEllipse(QPoint(outerRadius, outerRadius), outerRadius, outerRadius);

	if (r == 255 && g == 255 && b == 255)
		painter.setBrush(QBrush(QColor(18, 18, 18)));
	else
		painter.setBrush(QBrush(co));
	painter.setOpacity(0.8);
	painter.drawEllipse(QPoint(outerRadius, outerRadius), mediumRadius, mediumRadius);

	painter.setBrush(QBrush(Qt::white));
	painter.setOpacity(1.0);
	painter.drawEllipse(QPoint(outerRadius, outerRadius), innerRadius, innerRadius);

	cursorPixmap = pixmap;
}

void PLSDrawPenMgr::UpdateCurrentDrawPen(OBSScene scene)
{
	struct obs_video_info ovi;
	obs_get_video_info(&ovi);
	width = ovi.base_width;
	height = ovi.base_height;

	OBSSource curSceneSource = obs_scene_get_source(scene);
	QString name = QString::fromUtf8(obs_source_get_name(curSceneSource));

	auto temp = FindDrawPenData(name);

	{
		std::lock_guard<std::recursive_mutex> lock(lockInterfaces);
		drawPenInterface = temp;
	}

#if defined(_WIN32)
	if (temp && !drawPenCore) {
		drawPenCore = std::make_shared<PLSDrawPenCore>();
	}
	if (drawPenCore)
		drawPenCore->UpdateSharedTexture();
	needUpdateStrokes = true;
#endif
}

void PLSDrawPenMgr::UpdateTextureSize(uint32_t w, uint32_t h)
{
	PLS_PERFORMANCE_FUNCTION();
	width = w;
	height = h;

#if defined(_WIN32)
	if (drawPenCore)
		drawPenCore->UpdateSharedTexture();
#endif

	auto currentInterface = GetCurrentInterface();
	if (currentInterface)
		currentInterface->resize((float)width, (float)height);
}

void PLSDrawPenMgr::CopySceneEvent()
{
#if defined(_WIN32)
	SetEvent(strokeChangedEvent);
#endif
}

void PLSDrawPenMgr::AddSceneData(const QString &name)
{
#if defined(_WIN32)
	auto sceneData = std::make_shared<PLSDrawPenWin>();
#elif defined(__APPLE__)
	auto sceneData = std::make_shared<PLSDrawPenMac>();
	sceneData->setCallback(this, [](void *context, bool undoEmpty, bool redoEmpty) {
		if (context) {
			PLSDrawPenMgr *p = (PLSDrawPenMgr *)context;
			emit p->UndoDisabled(undoEmpty);
			emit p->RedoDisabled(redoEmpty);
		}
	});
#endif

	std::lock_guard<std::recursive_mutex> lock(lockInterfaces);
	sceneDrawPen[name] = sceneData;
}

void PLSDrawPenMgr::RenameSceneData(const QString &preName, const QString &nextName)
{
	std::lock_guard<std::recursive_mutex> lock(lockInterfaces);

	auto iter = sceneDrawPen.find(preName);
	if (iter == sceneDrawPen.end())
		return;

	std::shared_ptr<PLSDrawPenInterface> sceneData = iter->second;
	sceneDrawPen.erase(iter);
	sceneDrawPen[nextName] = sceneData;
}

std::shared_ptr<PLSDrawPenInterface> PLSDrawPenMgr::FindDrawPenData(const QString &name)
{
	std::lock_guard<std::recursive_mutex> lock(lockInterfaces);

	auto iter = sceneDrawPen.find(name);
	if (iter == sceneDrawPen.end())
		return nullptr;

	return iter->second;
}

void PLSDrawPenMgr::DeleteSceneData(const QString &name)
{
	std::lock_guard<std::recursive_mutex> lock(lockInterfaces);

	auto iter = sceneDrawPen.find(name);
	if (iter == sceneDrawPen.end())
		return;

	iter->second.reset();
	iter->second = nullptr;
	sceneDrawPen.erase(iter);
}

void PLSDrawPenMgr::DeleteAllData()
{
	std::lock_guard<std::recursive_mutex> lock(lockInterfaces);

	for (auto item : sceneDrawPen) {
		item.second.reset();
		item.second = nullptr;
	}
	sceneDrawPen.clear();

#if defined(_WIN32)
	if (drawPenCore)
		drawPenCore->UndoRedoCache().ClearCache();
#endif
}

void PLSDrawPenMgr::ResetProperties()
{
	linewidth = 0;
	rgba = C_SOLID_COLOR_0;
	colorIndex = 0;
	lineIndex = DEFAULT_LINE_WITDH_INDEX;
	curDrawType = DrawType::DT_PEN;
	curShapeType = ShapeType::ST_STRAIGHT_ARROW;
	return;
}

bool PLSDrawPenMgr::MousePosInPreview(vec2 pos) const
{
	obs_video_info ovi;
	vec2 screenSize;
	vec2_zero(&screenSize);

	if (obs_get_video_info(&ovi)) {
		screenSize.x = float(ovi.base_width);
		screenSize.y = float(ovi.base_height);
	}

	bool inx = (pos.x >= 0 && pos.x <= screenSize.x);
	bool iny = (pos.y >= 0 && pos.y <= screenSize.y);
	return (inx && iny);
}

void PLSDrawPenMgr::MousePressd(PointF point)
{
	auto currentInterface = GetCurrentInterface();
	if (!currentInterface)
		return;

	outAreaPressed = MousePosInPreview({point.x, point.y});
	if (!outAreaPressed)
		return;

	if (GetCurrentDrawType() == DrawType::DT_RUBBER) {
		currentInterface->eraseOn(point);
	} else {
#if (_WIN32)
		currentInterface->beginDraw(point);
#else //if(__APPLE__)

		int brushMode = -1;
		switch (curDrawType) {
		case DrawType::DT_PEN:
		case DrawType::DT_GLOW_PEN:
		case DrawType::DT_HIGHLIGHTER:
			brushMode = (int)curDrawType;
			break;
		case DrawType::DT_2DSHAPE:
			brushMode = 3 + (int)curShapeType;
			break;
		default:
			brushMode = -1;
			break;
		}
		if (brushMode < 0)
			return;
		currentInterface->resize(width, height);
		currentInterface->beginDraw(brushMode, (int)colorIndex, (int)lineIndex, point);
#endif
	}
}

void PLSDrawPenMgr::MouseMoved(PointF point)
{
	auto currentInterface = GetCurrentInterface();
	if (!currentInterface || !outAreaPressed)
		return;

	if (GetCurrentDrawType() == DrawType::DT_RUBBER) {
		currentInterface->eraseOn(point);
	} else
		currentInterface->moveTo(point);
}

void PLSDrawPenMgr::MouseReleased(PointF point)
{
	auto currentInterface = GetCurrentInterface();
	if (!currentInterface || !outAreaPressed)
		return;

	if (GetCurrentDrawType() == DrawType::DT_RUBBER) {
		currentInterface->eraseOn(point);
	} else {
		currentInterface->endDraw(point);
	}
}

void PLSDrawPenMgr::SetColor(uint32_t color)
{
	rgba = color;
	UpdateCursorPixmap();
}

void PLSDrawPenMgr::SetColorIndex(int index)
{
	colorIndex = index;
	if (index < colors.size())
		rgba = colors.at(index);
	UpdateCursorPixmap();
	PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "SetColor"}}, "Set Color %d", index);
}

void PLSDrawPenMgr::SetLineWidthIndex(int index)
{
	lineIndex = index;
	UpdateCursorPixmap();
	PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "SetLineWidth"}}, "Set Line Width %d", index);
}

void PLSDrawPenMgr::SetCurrentDrawType(DrawType type)
{
	curDrawType = type;
	UpdateCursorPixmap();

	switch (type) {
	case DrawType::DT_PEN:
		PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "Pen"}}, "Pen Clicked");
		break;

	case DrawType::DT_HIGHLIGHTER:
		PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "Hightlighter"}}, "Hightlighter Clicked");
		break;

	case DrawType::DT_GLOW_PEN:
		PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "GlowPen"}}, "Glow Pen Clicked");
		break;

	case DrawType::DT_2DSHAPE:
		PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "2DShape"}}, "2D Shape Clicked");
		break;

	case DrawType::DT_RUBBER:
		PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "Rubber"}}, "Rubber Clicked");
		break;

	default:
		break;
	}
}

void PLSDrawPenMgr::SetCurrentShapeType(ShapeType type)
{
	curShapeType = type;
	UpdateCursorPixmap();

	switch (type) {
	case ShapeType::ST_STRAIGHT_ARROW:
		PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "StraightLine"}}, "Straight Line Clicked");
		break;

	case ShapeType::ST_LINE:
		PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "Line"}}, "Line Clicked");
		break;

	case ShapeType::ST_RECTANGLE:
		PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "Rectangele"}}, "Rectangele Clicked");
		break;

	case ShapeType::ST_ROUND:
		PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "Round"}}, "Round Clicked");
		break;

	case ShapeType::TRIANGLE:
		PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "Triangle"}}, "Triangle Clicked");
		break;

	default:
		break;
	}
}

void PLSDrawPenMgr::UndoStroke()
{
	PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "Undo"}}, "Undo Clicked");
	pls_on_drawpen_event(PLSDrawPenMgr::Instance()->GetCurrentScene(), ACTION_UNDO);

	auto currentInterface = GetCurrentInterface();
	if (currentInterface)
		currentInterface->undo();
}

void PLSDrawPenMgr::RedoStroke()
{
	PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "Redo"}}, "Redo Clicked");
	pls_on_drawpen_event(PLSDrawPenMgr::Instance()->GetCurrentScene(), ACTION_REDO);

	auto currentInterface = GetCurrentInterface();
	if (currentInterface)
		currentInterface->redo();
}

void PLSDrawPenMgr::ClearStrokes()
{
	PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-SHORTCUT", "Clear"}}, "Clear Clicked");
	pls_on_drawpen_event(PLSDrawPenMgr::Instance()->GetCurrentScene(), ACTION_CLEAR);

	auto currentInterface = GetCurrentInterface();
	if (currentInterface)
		currentInterface->clear();
}

void PLSDrawPenMgr::ClearAllStrokes()
{
	std::lock_guard<std::recursive_mutex> lock(lockInterfaces);

	for (const auto &[key, vakue] : sceneDrawPen) {
		if (vakue)
			vakue->clear();
	}
}

void PLSDrawPenMgr::OnDrawVisible(bool visible)
{
	if (visible) {
		UpdateCursorPixmap();
#if defined(_WIN32)
		if (drawPenCore && !needUpdateStrokes)
			drawPenCore->UpdateCanvasByVisible(visible);
#endif
	} else {
		auto main = OBSBasic::Get();
		if (!main)
			return;
		pls_scene_update_canvas(main->GetCurrentScene(), nullptr, false);
	}

	auto currentInterface = GetCurrentInterface();
	if (currentInterface)
		currentInterface->setVisible(visible);
}

#if defined(_WIN32)

void PLSDrawPenMgr::RemoveStroke(std::string const &id)
{
	auto currentInterface = GetCurrentInterface();
	if (!currentInterface)
		return;

	auto drawPenWin = dynamic_cast<PLSDrawPenWin *>(currentInterface.get());
	drawPenWin->RemoveStroke(id);
}

std::vector<PointF> PLSDrawPenMgr::GetPoints()
{
	auto currentInterface = GetCurrentInterface();
	if (currentInterface) {
		auto drawPenWin = dynamic_cast<PLSDrawPenWin *>(currentInterface.get());
		return drawPenWin->GetPoints();
	}
	return std::vector<PointF>{};
}

std::vector<Stroke> PLSDrawPenMgr::GetStrokes()
{
	auto currentInterface = GetCurrentInterface();
	if (currentInterface) {
		auto drawPenWin = dynamic_cast<PLSDrawPenWin *>(currentInterface.get());
		return drawPenWin->GetStrokes();
	}
	return std::vector<Stroke>{};
}

void PLSDrawPenMgr::RemoveCaches(const std::vector<Stroke> &strokeList, const std::vector<Stroke> &redoList)
{
	if (redoList.empty())
		return;

	if (!drawPenCore)
		return;

	std::vector<Stroke> strokesTemp = strokeList;

	auto count = (int)redoList.size();
	for (int i = count - 1; i >= 0; --i) {
		strokesTemp.push_back(redoList.at(i));

		auto hash = PLSUndoRedoCache::GetHashValue(strokesTemp);
		if (hash)
			drawPenCore->UndoRedoCache().RemoveCache(hash);
	}
}
#endif

uint32_t PLSDrawPenMgr::GetColor() const
{
	return rgba;
}

int PLSDrawPenMgr::GetColorIndex() const
{
	return colorIndex;
}

int PLSDrawPenMgr::GetLineWidth(DrawType type) const
{
	if (DrawType::DT_INVALID != type) {
		return get_line_width(lineIndex, type);
	} else {
		return get_line_width(lineIndex, curDrawType);
	}
}

int PLSDrawPenMgr::GetLineWidthIndex() const
{
	return lineIndex;
}

DrawType PLSDrawPenMgr::GetCurrentDrawType() const
{
	return curDrawType;
}

ShapeType PLSDrawPenMgr::GetCurrentShapeType() const
{
	return curShapeType;
}

void PLSDrawPenMgr::GetSize(uint32_t &w, uint32_t &h) const
{
	w = width;
	h = height;
}

QPixmap PLSDrawPenMgr::GetCurrentCursorPixmap() const
{
	return cursorPixmap;
}

bool PLSDrawPenMgr::UndoEmpty()
{
	auto currentInterface = GetCurrentInterface();
	if (currentInterface)
		return currentInterface->undoEmpty();
	return true;
}

bool PLSDrawPenMgr::RedoEmpty()
{
	auto currentInterface = GetCurrentInterface();
	if (currentInterface)
		return currentInterface->redoEmpty();
	return true;
}

bool PLSDrawPenMgr::DrawVisible()
{
	auto currentInterface = GetCurrentInterface();
	if (currentInterface)
		return currentInterface->visible();
	return true;
}

bool PLSDrawPenMgr::NeedUpdateStrokesToTarget()
{
	bool temp = needUpdateStrokes;
	needUpdateStrokes = false;
	return temp;
}

OBSScene PLSDrawPenMgr::GetCurrentScene()
{
	auto main = OBSBasic::Get();
	if (!main)
		return nullptr;

	OBSScene scene = main->GetCurrentScene();
	return scene;
}

gs_texture_t *PLSDrawPenMgr::GetSceneCanvasTexture()
{
	OBSScene scene = GetCurrentScene();
	if (!scene)
		return nullptr;

	return pls_scene_get_canvas(scene);
}

std::shared_ptr<PLSDrawPenInterface> PLSDrawPenMgr::GetCurrentInterface()
{
	std::lock_guard<std::recursive_mutex> lock(lockInterfaces);
	return drawPenInterface;
}

const char *PLSDrawPenMgr::GetPenActionName()
{
	switch (curDrawType) {
	case DrawType::DT_PEN:
		return ACTION_PEN;
	case DrawType::DT_HIGHLIGHTER:
		return ACTION_HIGHLIGHT;
	case DrawType::DT_GLOW_PEN:
		return ACTION_GLOW;
	case DrawType::DT_2DSHAPE:
		return ACTION_SHAPE;
	case DrawType::DT_RUBBER:
		return ACTION_ERASER;
	default:
		assert(false);
		return "";
	}
}
