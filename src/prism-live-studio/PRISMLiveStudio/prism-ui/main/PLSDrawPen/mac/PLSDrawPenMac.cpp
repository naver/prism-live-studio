//
//  PLSStroke-mac.cpp
//  QuartzDrawPenDemo
//
//  Created by Zhong Ling on 2023/1/31.
//

#include "PLSDrawPenMac.h"

#include "PLSStrokeManagerOCInterface.hpp"
#include <cassert>

PLSDrawPenMac::PLSDrawPenMac()
{
	impl = new PLSStrokeManagerImpl();
}
PLSDrawPenMac::~PLSDrawPenMac()
{
	if (impl) {
		delete impl;
		impl = nullptr;
	}
}
void PLSDrawPenMac::beginDraw(unsigned int brushMode, unsigned int colorMode, unsigned int thicknessMode, PointF point)
{
	impl->beginDraw(brushMode, colorMode, thicknessMode, point);
}
void PLSDrawPenMac::beginDraw(PointF point)
{
	//    assert(false);
}
void PLSDrawPenMac::moveTo(PointF point)
{
	impl->moveTo(point);
}
void PLSDrawPenMac::endDraw(PointF point)
{
	impl->endDraw(point);
}
void PLSDrawPenMac::eraseOn(PointF point)
{
	impl->eraseOn(point);
}
void PLSDrawPenMac::undo()
{
	impl->undo();
}
void PLSDrawPenMac::redo()
{
	impl->redo();
}
void PLSDrawPenMac::clear()
{
	impl->clear();
}
void PLSDrawPenMac::resize(float width, float height)
{
	impl->resize(width, height);
}
void PLSDrawPenMac::setVisible(bool visible)
{
	impl->setVisible(visible);
}
bool PLSDrawPenMac::visible()
{
	return impl->visible();
}
bool PLSDrawPenMac::undoEmpty()
{
	return impl->undoEmpty();
}
bool PLSDrawPenMac::redoEmpty()
{
	return impl->redoEmpty();
}
void PLSDrawPenMac::drawPens()
{
	impl->draw();
}

void PLSDrawPenMac::setCallback(void *context, DrawPenCallBacak cb)
{
	impl->setCallback(context, cb);
}
