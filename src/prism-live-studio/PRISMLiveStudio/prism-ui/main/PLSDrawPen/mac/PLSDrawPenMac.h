//
//  PLSStroke-mac.hpp
//  QuartzDrawPenDemo
//
//  Created by Zhong Ling on 2023/1/31.
//

#ifndef PLSStroke_mac_hpp
#define PLSStroke_mac_hpp

#ifdef MAC_DEMO
#include "PLSDrawPenInterface.h"
#else
#include "../PLSDrawPenInterface.h"
#endif

class PLSStrokeManagerImpl;

class PLSDrawPenMac : public PLSDrawPenInterface {
public:
	PLSDrawPenMac();
	~PLSDrawPenMac();

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
	virtual bool visible();
	virtual bool undoEmpty();
	virtual bool redoEmpty();

	virtual void drawPens();
	virtual void setCallback(void *context, DrawPenCallBacak cb);

private:
	PLSStrokeManagerImpl *impl;
	DrawPenCallBacak cb;
};

#endif /* PLSStroke_mac_hpp */
