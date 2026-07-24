#pragma once

#include <QObject>
#include <map>
#include <mutex>
#include <QPixmap>
#include "frontend-api.h"
#include "PLSDrawPenInterface.h"
#include "PLSDrawPenDefine.h"
#include "pls/pls-action-util.h"
#if defined(_WIN32)
#include "windows/PLSDrawPenWin.h"
#include "windows/PLSDrawPenCore.h"
#include "windows/PLSDrawPenStroke.h"
#endif

class PLSDrawPenMgr : public QObject {
	Q_OBJECT

public:
	static PLSDrawPenMgr *Instance();
	~PLSDrawPenMgr() final = default;
	void Release();

private:
	PLSDrawPenMgr();
	void UpdatePenCursor();
	void UpdateHighlighterCursor();
	void UpdateGlowCursor();

public:
	void UpdateCursorPixmap();
	void UpdateCurrentDrawPen(OBSScene scene);
	void UpdateTextureSize(uint32_t w, uint32_t h);
	void CopySceneEvent();
	void AddSceneData(const QString &name);
	void RenameSceneData(const QString &preName, const QString &nextName);
	std::shared_ptr<PLSDrawPenInterface> FindDrawPenData(const QString &name);
	void DeleteSceneData(const QString &name);
	void DeleteAllData();
	void ResetProperties();

	bool MousePosInPreview(vec2 pos) const;
	bool MouseOutAreaPressed() const { return outAreaPressed; };
	void MousePressd(PointF point);
	void MouseMoved(PointF point);
	void MouseReleased(PointF point);
	void SetColor(uint32_t color);
	void SetColorIndex(int index);
	void SetLineWidthIndex(int index);
	void SetCurrentDrawType(DrawType type);
	void SetCurrentShapeType(ShapeType type);
	void UndoStroke();
	void RedoStroke();
	void ClearStrokes();
	void ClearAllStrokes();

	void OnDrawVisible(bool visible);

#if defined(_WIN32)
	void RemoveStroke(std::string const &id);
	std::vector<PointF> GetPoints();
	std::vector<Stroke> GetStrokes();
	void RemoveCaches(const std::vector<Stroke> &strokeList, const std::vector<Stroke> &redoList);
#endif

	uint32_t GetColor() const;
	int GetColorIndex() const;
	int GetLineWidth(DrawType type = DrawType::DT_INVALID) const;
	int GetLineWidthIndex() const;
	DrawType GetCurrentDrawType() const;
	ShapeType GetCurrentShapeType() const;
	void GetSize(uint32_t &w, uint32_t &h) const;

#if defined(_WIN32)
	HANDLE GetMouseMovingEvent() { return movingEvent; }
	HANDLE GetMouseReleaseEvent() { return releasedEvent; }
	HANDLE GetStrokeChangedEvent() { return strokeChangedEvent; }
	HANDLE GetRubberEvent() { return rubberEvent; }
#endif

	QPixmap GetCurrentCursorPixmap() const;
	bool UndoEmpty();
	bool RedoEmpty();
	bool DrawVisible();
	bool NeedUpdateStrokesToTarget();
	OBSScene GetCurrentScene();
	gs_texture_t *GetSceneCanvasTexture();
	std::shared_ptr<PLSDrawPenInterface> GetCurrentInterface();
	const char *GetPenActionName();

signals:
	void UndoDisabled(bool disabled);
	void RedoDisabled(bool disabled);

private:
	bool outAreaPressed = true;
	std::atomic<bool> needUpdateStrokes = false;

	std::recursive_mutex lockInterfaces;
	std::shared_ptr<PLSDrawPenInterface> drawPenInterface = nullptr;
	std::map<QString, std::shared_ptr<PLSDrawPenInterface>> sceneDrawPen; // key:scene name  value:scene drawpen data

#if defined(_WIN32)
	std::shared_ptr<PLSDrawPenCore> drawPenCore = nullptr;
	HANDLE movingEvent;
	HANDLE strokeChangedEvent;
	HANDLE releasedEvent;
	HANDLE rubberEvent;
#endif
	QPixmap cursorPixmap;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t linewidth = 0;
	uint32_t rgba = C_SOLID_COLOR_0;
	int colorIndex = 0;
	int lineIndex = DEFAULT_LINE_WITDH_INDEX;
	DrawType curDrawType = DrawType::DT_PEN;
	ShapeType curShapeType = ShapeType::ST_STRAIGHT_ARROW;
};
