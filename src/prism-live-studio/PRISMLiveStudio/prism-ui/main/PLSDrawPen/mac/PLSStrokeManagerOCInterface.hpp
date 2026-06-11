//
//  PLSStrokeManagerOCInterface.h
//  QuartzDrawPenDemo
//
//  Created by Zhong Ling on 2023/1/31.
//

#ifndef PLSStrokeManagerOCInterface_h
#define PLSStrokeManagerOCInterface_h

#ifdef MAC_DEMO
#import "PLSCurveSmooth.h"
#import "PLSDrawPenInterface.h"
#else
#import "../PLSDrawPenInterface.h"
#endif

class PLSStrokeManagerImpl {
public:
	PLSStrokeManagerImpl();
	~PLSStrokeManagerImpl();

	void beginDraw(unsigned int brushMode, unsigned int colorMode, unsigned int thicknessMode, PointF point);
	void moveTo(PointF point);
	void endDraw(PointF point);
	void eraseOn(PointF point);
	void undo();
	void redo();
	void clear();
	void resize(float width, float height);
	void setVisible(bool visible);
	bool visible();
	bool undoEmpty();
	bool redoEmpty();
	void draw();
	void setCallback(void *context, DrawPenCallBacak cb);

private:
	void *self;
	DrawPenCallBacak cb;
};

#endif /* PLSStrokeManagerOCInterface_h */
