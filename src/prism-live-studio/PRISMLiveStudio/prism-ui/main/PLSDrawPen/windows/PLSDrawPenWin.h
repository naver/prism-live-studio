#pragma once
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <d2d1.h>
#include <util/windows/ComPtr.hpp>
#include <QObject>
#include "PLSDrawPenStroke.h"
#include "../PLSDrawPenInterface.h"

template<class Interface> inline void SafeRelease(Interface **ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != nullptr) {
		(*ppInterfaceToRelease)->Release();

		(*ppInterfaceToRelease) = nullptr;
	}
}

class CCSection {
	CRITICAL_SECTION m_cs;

public:
	CCSection() { InitializeCriticalSection(&m_cs); }
	~CCSection() { DeleteCriticalSection(&m_cs); }

	void Lock() { EnterCriticalSection(&m_cs); }
	void Unlock() { LeaveCriticalSection(&m_cs); }
};

class CAutoLockCS {
	CCSection &m_Lock;

public:
	explicit CAutoLockCS(CCSection &cs) : m_Lock(cs) { m_Lock.Lock(); }

	~CAutoLockCS() { m_Lock.Unlock(); }
};

class PLSDrawPenWin : public QObject, public PLSDrawPenInterface {
	Q_OBJECT
public:
	PLSDrawPenWin();
	~PLSDrawPenWin();

	PLSDrawPenWin &operator=(const PLSDrawPenWin &) = default;
	PLSDrawPenWin &operator=(PLSDrawPenWin &&) = default;

	virtual void beginDraw(unsigned int brushMode, unsigned int colorMode, unsigned int thicknessMode, PointF point);
	virtual void beginDraw(PointF point);
	virtual void moveTo(PointF point);
	virtual void endDraw(PointF point);
	virtual void eraseOn(PointF point);
	virtual void undo();
	virtual void redo();
	virtual void clear();
	virtual void resize(float width, float height);
	virtual void setVisible(bool visible);
	virtual bool visible() { return drawVisible; };
	virtual bool undoEmpty() { return undoStrokes.empty(); };
	virtual bool redoEmpty() { return redoStrokes.empty(); };

	virtual void drawPens();
	virtual void setCallback(void *context, DrawPenCallBacak cb);

	void RemoveStroke(std::string const &id);
	std::vector<PointF> GetPoints();
	std::vector<Stroke> GetStrokes();

private:
	ID2D1Geometry *calculateGeometry(const std::vector<PointF> &points);
	void pushUndoStrokes(const Stroke &stroke);
	void pushRedoStrokes(const Stroke &stroke);
	void popUndoStrokes();
	void popRedoStrokes();

signals:
	void UndoDisabled(bool disabled);
	void RedoDisabled(bool disabled);

private:
	CCSection lock;
	bool drawVisible = true;
	bool undoDisabled = true;
	bool redoDisabled = true;
	std::vector<PointF> points{};
	std::vector<Stroke> strokes{};
	std::vector<Stroke> undoStrokes{}; // Undo strokes
	std::vector<Stroke> redoStrokes{}; // Undo strokes
};
