//
//  PLSDrawPenInterface.h
//  QuartzDrawPenDemo
//
//  Created by Zhong Ling on 2023/2/1.
//

#ifndef PLSDrawPenInterface_h
#define PLSDrawPenInterface_h

#include "PLSDrawPenDefine.h"

typedef void(DrawPenCallBacak)(void *context, bool undoEmpty, bool redoEmpty);

class PLSDrawPenInterface {
public:
	virtual void beginDraw(unsigned int brushMode, unsigned int colorMode, unsigned int thicknessMode, PointF point) = 0;
	virtual void beginDraw(PointF point) = 0;
	virtual void moveTo(PointF point) = 0;
	virtual void endDraw(PointF point) = 0;

	virtual void eraseOn(PointF point) = 0;
	virtual void undo() = 0;
	virtual void redo() = 0;
	virtual void clear() = 0;

	virtual void resize(float width, float height) = 0;
	virtual void setVisible(bool visible) = 0;
	virtual bool visible() = 0;
	virtual bool undoEmpty() = 0;
	virtual bool redoEmpty() = 0;

	virtual void drawPens() = 0;

	virtual void setCallback(void *context, DrawPenCallBacak cb) = 0;
};

#endif /* PLSDrawPenInterface_h */
