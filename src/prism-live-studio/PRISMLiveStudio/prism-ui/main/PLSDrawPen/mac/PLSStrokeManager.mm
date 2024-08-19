//
//  PLSStrokeManager.m
//  QuartzDrawPenDemo
//
//  Created by Zhong Ling on 2023/1/30.
//

#import "PLSStrokeManager.hpp"

#import "PLSStrokeManagerOCInterface.hpp"

#define MAX_STROKE_POINT_COUNT 500

#ifndef MAC_DEMO
#include <obs.h>
#include <graphics/graphics.h>
#include <pls/pls-obs-api.h>

#if 0
#define LIBUTILSAPI_LIB 1
#include "libutils-api-log.h"
#endif

#include "window-basic-main.hpp"

#endif

#ifndef MAC_DEMO
extern bool uploadToGPU(gs_texture_t *texture,
                        const uint8_t *data,
                        uint32_t width,
                        uint32_t height,
                        uint32_t stride,
                        bool flip);
#endif

typedef void(^RetainSelfBlock)(void);

@interface PLSStrokeManager ()
-(void)breakRetainCycly;
@end

@implementation PLSStrokeManager
{
    RetainSelfBlock _retainBlock;
}

// MARK: - PLSStrokeManagerImpl
PLSStrokeManagerImpl::PLSStrokeManagerImpl()
{
    PLSStrokeManager *object = [[PLSStrokeManager alloc] init];
    self = (__bridge void *)object;
    object->_retainBlock = ^{
        [object class];
    };
}

PLSStrokeManagerImpl::~PLSStrokeManagerImpl()
{
    [(__bridge id)self breakRetainCycly];
}

void PLSStrokeManagerImpl::beginDraw(unsigned int brushMode,
                                     unsigned int colorMode,
                                     unsigned int thicknessMode,
                                     PointF point)
{
    NSPoint p;
    p.x = point.x;
    p.y = point.y;
    [(__bridge id)self beginWithBushMode:(BrushMode)brushMode
                              startPoint:p
                               colorMode:(ColorMode)colorMode
                           thicknessMode:(ThicknessMode)thicknessMode];
}

void PLSStrokeManagerImpl::moveTo(PointF point)
{
    NSPoint p;
    p.x = point.x;
    p.y = point.y;
    [(__bridge id)self moveTo:p];
}
void PLSStrokeManagerImpl::endDraw(PointF point)
{
    NSPoint p;
    p.x = point.x;
    p.y = point.y;
    [(__bridge id)self endTo:p];
}
void PLSStrokeManagerImpl::eraseOn(PointF point)
{
    NSPoint p;
    p.x = point.x;
    p.y = point.y;
    [(__bridge id)self eraseOn:p];
}
void PLSStrokeManagerImpl::undo()
{
    [(__bridge id)self undo];
}
void PLSStrokeManagerImpl::redo()
{
    [(__bridge id)self redo];
}
void PLSStrokeManagerImpl::clear()
{
    [(__bridge id)self clear];
}
void PLSStrokeManagerImpl::resize(float width, float height)
{
    NSSize size;
    size.width = width;
    size.height = height;
    [(__bridge id)self resize:size];
}
void PLSStrokeManagerImpl::setVisible(bool visible)
{
    [((__bridge id)self) setIsVisible:visible];
}
bool PLSStrokeManagerImpl::visible()
{
    return [((__bridge id)self) getIsVisible];
}
bool PLSStrokeManagerImpl::undoEmpty()
{
    return [((__bridge id)self) undoEmpty];
}
bool PLSStrokeManagerImpl::redoEmpty()
{
    return [((__bridge id)self) redoEmpty];
}
void PLSStrokeManagerImpl::draw()
{
    [(__bridge id)self drawPensWithRedraw:FALSE rightNow:FALSE andDebugInfo:@"PLSStrokeManagerImpl::draw"];
}

void PLSStrokeManagerImpl::setCallback(void *context, DrawPenCallBacak cb)
{
    if (!cb) return;
    
    void (^myBlock)(void *context, bool undoEmpty, bool redoEmpty) = ^(void *context, bool undoEmpty, bool redoEmpty) {cb(context, undoEmpty, redoEmpty);};
    
    [(__bridge id)self setDrawPenBlock:myBlock context:context];
}

// MARK: - PLSStrokeManager

- (id)init
{
    self = [super init];
    self->_drawingActions = [NSMutableArray new];
    self->_undidActions = [NSMutableArray new];
    self->_currentStroke = NULL;
    self->_renderQueue = dispatch_queue_create("com.drawpen.render", DISPATCH_QUEUE_SERIAL);
    self->_drawPenBlock = NULL;
    self->_currentCanvasRawData = NULL;
    self->_previousCanvasRawData = NULL;
    self->_isVisible = YES;
    self->_lastDrawTime = nullptr;
    self->_maxStrokeNo = 0;
    
#ifndef MAC_DEMO
    self->_texture = nullptr;
    dispatch_async(dispatch_get_main_queue(), ^{
        pls_scene_update_canvas(OBSBasic::Get()->GetCurrentScene(), nullptr, false);
    });
#endif
    return self;
}

- (void)dealloc
{
#ifndef MAC_DEMO
    if (_texture && obs_initialized()) {
        obs_enter_graphics();
        gs_texture_destroy(_texture);
        obs_leave_graphics();
        _texture = nullptr;
    }
#endif
}

- (void)setDrawPenBlock:(OCDrawPenCallBacak)cb context:(void*)context
{
    self->_context = context;
    self->_drawPenBlock = cb;
}

- (NSPoint)convertPLSPoint:(CGPoint)p
{
#if MAC_DEMO
    return p;
#else
    return CGPointMake(p.x, self.size.height - p.y);
#endif
}

- (void)_beginWithBushMode:(BrushMode)mode
                startPoint:(NSPoint)point
                 colorMode:(ColorMode)colorMode
             thicknessMode:(ThicknessMode)thicknessMode
                  strokeID:(NSInteger)strokeID
{
    PLSStrokeBase *stroke = NULL;
    
    CGFloat strokeWidth = [PLSStrokeHelper getStrokeWidthFromThichnessMode:thicknessMode brushMode:mode];
    NSColor *strokeColor = [PLSStrokeHelper getStrockeColorFromColorMode:colorMode brushMode:mode];
    CGFloat headWidth = [PLSStrokeHelper getHeadWidthFromThichnessMode:thicknessMode];
    CGFloat headLength = [PLSStrokeHelper getHeadLengthFromThichnessMode:thicknessMode];
    
    if (strokeID < 0) {
        self.maxStrokeNo += 1;
        strokeID = self.maxStrokeNo;
    }
    
    
    switch (mode) {
        case Pen:
            stroke = [[PLSStrokePen alloc] initWithStrokeColor:strokeColor
                                                     brushMode:mode
                                                     colorMode:colorMode
                                                   strokeWidth:strokeWidth
                                                 thicknessMode:thicknessMode
                                                 useCtrlPoints:true
                                                      strokeID:strokeID];
            break;
            
        case Highlighter:
            stroke = [[PLSStrokeHighlighter alloc] initWithStrokeColor:strokeColor
                                                             brushMode:mode
                                                             colorMode:colorMode
                                                           strokeWidth:strokeWidth
                                                         thicknessMode:thicknessMode
                                                         useCtrlPoints:true
                                                              strokeID:strokeID];
            break;
            
        case Glow:
            stroke = [[PLSStrokeGlow alloc] initWithStrokeColor:strokeColor
                                                      brushMode:mode
                                                      colorMode:colorMode
                                                    strokeWidth:strokeWidth
                                                  thicknessMode:thicknessMode
                                                  useCtrlPoints:true
                                                       strokeID:strokeID];
            break;
            
        case ArrowLine:
            stroke = [[PLSStrokeArrowLine alloc] initWithStrokeColor:strokeColor
                                                           colorMode:colorMode
                                                         strokeWidth:strokeWidth
                                                       thicknessMode:thicknessMode
                                                       useCtrlPoints:false
                                                           headWidth:headWidth
                                                          headLength:headLength
                                                            strokeID:strokeID];
            break;
            
        case Line:
            stroke = [[PLSStrokeLine alloc] initWithStrokeColor:strokeColor
                                                      brushMode:mode
                                                      colorMode:colorMode
                                                    strokeWidth:strokeWidth
                                                  thicknessMode:thicknessMode
                                                  useCtrlPoints:false
                                                       strokeID:strokeID];
            break;
            
        case Rectangle:
            stroke = [[PLSStrokeRectangle alloc] initWithStrokeColor:strokeColor
                                                           brushMode:mode
                                                           colorMode:colorMode
                                                         strokeWidth:strokeWidth
                                                       thicknessMode:thicknessMode
                                                       useCtrlPoints:false
                                                            strokeID:strokeID];
            break;
            
        case Ellipse:
            stroke = [[PLSStrokeEllipse alloc] initWithStrokeColor:strokeColor
                                                         brushMode:mode
                                                         colorMode:colorMode
                                                       strokeWidth:strokeWidth
                                                     thicknessMode:thicknessMode
                                                     useCtrlPoints:false
                                                          strokeID:strokeID];
            break;
            
        case Triangle:
            stroke = [[PLSStrokeTriangle alloc] initWithStrokeColor:strokeColor
                                                          brushMode:mode
                                                          colorMode:colorMode
                                                        strokeWidth:strokeWidth
                                                      thicknessMode:thicknessMode
                                                      useCtrlPoints:false
                                                           strokeID:strokeID];
            break;
            
        default:
            assert(false);
            break;
    }
    [stroke beginFrom:point];
    [self.drawingActions addObject:stroke];
    self.currentStroke = stroke;
	// when begin a new stroke, clear all undid actions.
	[self.undidActions removeAllObjects];
    if (self.drawPenBlock) {
        self.drawPenBlock(self.context, [self undoEmpty], [self redoEmpty]);
    }
}

- (void)beginWithBushMode:(BrushMode)mode
               startPoint:(NSPoint)p
                colorMode:(ColorMode)colorMode
            thicknessMode:(ThicknessMode)thicknessMode
{
    __weak PLSStrokeManager *weakSelf = self;
    dispatch_async(_renderQueue, ^{
        if (weakSelf.currentStroke) { return; }
        
        NSPoint point = [weakSelf convertPLSPoint:p];
        
        //        PLS_INFO("DrawPen", "begin %f %f", point.x, point.y);
        [weakSelf _beginWithBushMode:mode
                          startPoint:point
                           colorMode:colorMode
                       thicknessMode:thicknessMode
                            strokeID:-1];
        [weakSelf _drawPensWithRedraw:FALSE
							 rightNow:TRUE
							  yOffset:0.0
						 andDebugInfo:@"beginWithBushMode"];
    });
}

- (void)moveTo:(NSPoint)p
{
    __weak PLSStrokeManager *weakSelf = self;
    dispatch_async(_renderQueue, ^{
        if (weakSelf.currentStroke == NULL) { return; }
        
        NSPoint point = [weakSelf convertPLSPoint:p];
        
        NSInteger strokeID = weakSelf.currentStroke.strokeID;
        
        if (weakSelf.currentStroke.points->size() > MAX_STROKE_POINT_COUNT) {
            BrushMode brushMode = weakSelf.currentStroke.brushMode;
            ColorMode colorMode = weakSelf.currentStroke.colorMode;
            ThicknessMode thicknessMode = weakSelf.currentStroke.thicknessMode;
            
            [weakSelf _endTo:point];

            NSLog(@"✨✨✨✨✨✨✨✨✨✨✨✨");
            
            dispatch_async(weakSelf.renderQueue, ^{
                [weakSelf _beginWithBushMode:brushMode
                                  startPoint:point
                                   colorMode:colorMode
                               thicknessMode:thicknessMode
                                    strokeID:strokeID];
            });
        } else if ([weakSelf.currentStroke moveToPoint:point]) {
            [weakSelf _drawPensWithRedraw:FALSE
								 rightNow:NO
								  yOffset:0.0
							 andDebugInfo:@"moveTo"];
        }
    });
}

- (void)endTo:(NSPoint)p
{
    __weak PLSStrokeManager *weakSelf = self;
    dispatch_async(_renderQueue, ^{
        if (weakSelf.currentStroke == NULL) { return; }
        
        NSPoint point = [weakSelf convertPLSPoint:p];
        [weakSelf _endTo:point];
    });
}

- (void)_endTo:(NSPoint)point {
    [self.currentStroke endTo:point];
    self.currentStroke = NULL;
    [self _drawPensWithRedraw:NO
					 rightNow:TRUE
					  yOffset:0.0
				 andDebugInfo:@"endTo"];
}

- (void)_eraseOn:(NSPoint)point
{
    if (self.drawingActions.count <= 0)
        return;
    
    NSInteger erasedStrokeID = -1;
    NSMutableArray *erasedStrokes = [NSMutableArray array];
    for (int i = (int)self.drawingActions.count - 1; i >= 0; i--) {
        PLSStrokeBase *pen = (PLSStrokeBase *)self.drawingActions[i];
        
        if (![pen isKindOfClass:[PLSStrokeBase class]]) {
            continue;
        }
        
        if (![pen getErased] && [pen containsPoint:point]) {
            [pen increaseEraseCount];
            [erasedStrokes addObject:pen];
            erasedStrokeID = pen.strokeID;
        }
        
        if (erasedStrokeID >= 0) {
            // loop afterwards
            for (int j = i + 1; j < self.drawingActions.count; j++) {
                PLSStrokeBase *nextPen = (PLSStrokeBase *)self.drawingActions[j];
                
                if (![nextPen isKindOfClass:[PLSStrokeBase class]]) {
                    break;
                }
                if (nextPen.strokeID != erasedStrokeID) {
                    break;
                } else {
                    [nextPen increaseEraseCount];
                    [erasedStrokes addObject:nextPen];
                }
            }
            
            // loop forwards
            for (int j = i - 1; j >= 0; j--) {
                PLSStrokeBase *prePen = (PLSStrokeBase *)self.drawingActions[j];
                
                if (![prePen isKindOfClass:[PLSStrokeBase class]]) {
                    break;
                }
                if (prePen.strokeID != erasedStrokeID) {
                    break;
                } else {
                    [prePen increaseEraseCount];
                    [erasedStrokes addObject:prePen];
                }
            }
            
            break;
        }
        
    }
    
    if (erasedStrokeID >= 0) {
        PLSStrokeActionEraser *eraser = [[PLSStrokeActionEraser alloc] initWithBaseStrokes:erasedStrokes];
        [self.drawingActions addObject:eraser];
        [self _drawPensWithRedraw:TRUE 
						 rightNow:TRUE
						  yOffset:0.0
					 andDebugInfo:@"_eraseOn"];
    }
    
    if (self.drawPenBlock) {
        self.drawPenBlock(self.context, [self undoEmpty], [self redoEmpty]);
    }
}

- (void)eraseOn:(NSPoint)p
{
    __weak PLSStrokeManager *weakSelf = self;
    dispatch_async(_renderQueue, ^{
        NSPoint point = [weakSelf convertPLSPoint:p];
        [weakSelf _eraseOn:point];
    });
    
}

- (void)_undo
{
    if (self.drawingActions.count <= 0)
        return ;
    
    PLSStrokeAction *lastAction = [self.drawingActions lastObject];
    [self.drawingActions removeObject:lastAction];
    [self.undidActions addObject:lastAction];
    
    switch (lastAction.actionType) {
        case ClearAction:
        case EraseAction:
        {
            NSArray<NSObject*>* strokes = lastAction.baseStrokes;
            BOOL bRefresh = NO;
            for (int i = 0; i < strokes.count; i++) {
                PLSStrokeBase *pen = (PLSStrokeBase *)strokes[i];
                if ([pen isKindOfClass:[PLSStrokeBase class]]) {
                    [pen decreaseEraseCount];
                    bRefresh = YES;
                }
            }
            if (bRefresh) {
                [self _drawPensWithRedraw:TRUE
								 rightNow:TRUE
								  yOffset:0.0
							 andDebugInfo:@"_undo 1"];
            }
        }
            break;
        case DrawAction:
        {
            NSInteger strokeID = -1;
            if ([lastAction isKindOfClass:[PLSStrokeBase class]]) {
                PLSStrokeBase *lastStroke = (PLSStrokeBase *)lastAction;
                [lastStroke increaseEraseCount];
                strokeID = lastStroke.strokeID;
            }
            
            if (strokeID >= 0) {
                for (int i = (int)self.drawingActions.count - 1; i >= 0; i--) {
                    PLSStrokeBase *stroke = (PLSStrokeBase *)self.drawingActions[i];
                    if ([stroke isKindOfClass:[PLSStrokeBase class]] && stroke.strokeID == strokeID) {
                        [stroke increaseEraseCount];
                        [self.drawingActions removeObject:stroke];
                        [self.undidActions addObject:stroke];
                    } else {
                        break;
                    }
                }
                
                [self _drawPensWithRedraw:TRUE
								 rightNow:TRUE
								  yOffset:0.0
							 andDebugInfo:@"_undo 2"];
            }
        }
            break;
            
        default:
            break;
    }
    
    if (self.drawPenBlock) {
        self.drawPenBlock(self.context, [self undoEmpty], [self redoEmpty]);
    }
}

- (void)undo
{
    __weak PLSStrokeManager *weakSelf = self;
    dispatch_async(_renderQueue, ^{
        [weakSelf _undo];
    });
}

- (void)_redo
{
    if (self.undidActions.count <= 0)
        return;
    
    PLSStrokeAction *lastAction = [self.undidActions lastObject];
    [self.undidActions removeObject:lastAction];
    [self.drawingActions addObject:lastAction];
    
    switch (lastAction.actionType) {
        case ClearAction:
        case EraseAction:
        {
            NSArray<NSObject*>* strokes = lastAction.baseStrokes;
            BOOL bRefresh = NO;
            for (int i = 0; i < strokes.count; i++) {
                PLSStrokeBase *pen = (PLSStrokeBase *)strokes[i];
                if ([pen isKindOfClass:[PLSStrokeBase class]]) {
                    [pen increaseEraseCount];
                    bRefresh = YES;
                }
            }
            if (bRefresh) {
                [self _drawPensWithRedraw:TRUE
								 rightNow:TRUE
								  yOffset:0.0
							 andDebugInfo:@"_redo 1"];
            }
        }
            break;
            
        case DrawAction:
        {
            NSInteger strokeID = -1;
            if ([lastAction isKindOfClass:[PLSStrokeBase class]]) {
                PLSStrokeBase *lastStroke = (PLSStrokeBase *)lastAction;
                [lastStroke decreaseEraseCount];
                strokeID = lastStroke.strokeID;
            }
            
            if (strokeID >= 0) {
                for (int i = (int)self.undidActions.count - 1; i >= 0; i--) {
                    PLSStrokeBase *stroke = (PLSStrokeBase *)self.undidActions[i];
                    if ([stroke isKindOfClass:[PLSStrokeBase class]] && stroke.strokeID == strokeID) {
                        [stroke decreaseEraseCount];
                        [self.undidActions removeObject:stroke];
                        [self.drawingActions addObject:stroke];
                    } else {
                        break;
                    }
                }
                
                [self _drawPensWithRedraw:TRUE
								 rightNow:TRUE
								  yOffset:0.0
							 andDebugInfo:@"_redo 2"];
            }
            
        }
            break;
            
        default:
            break;
    }
    
    if (self.drawPenBlock) {
        self.drawPenBlock(self.context, [self undoEmpty], [self redoEmpty]);
    }
}

- (void)redo
{
    __weak PLSStrokeManager *weakSelf = self;
    dispatch_async(_renderQueue, ^{
        [weakSelf _redo];
    });
}

- (void)_clear
{
    if (self.drawingActions.count <= 0)
        return;
    
    NSMutableArray *array = [NSMutableArray array];
    
    for (int i = 0; i < self.drawingActions.count; i++) {
        PLSStrokeBase *pen = (PLSStrokeBase *)self.drawingActions[i];
        if (![pen isKindOfClass:[PLSStrokeBase class]])
            continue;
        
        [pen increaseEraseCount];
        [array addObject:pen];
    }
    
    PLSStrokeActionClear *clearAction = [[PLSStrokeActionClear alloc] initWithBaseStrokes:array];
    [self.drawingActions addObject:clearAction];
    
    if (self.drawPenBlock) {
        self.drawPenBlock(self.context, [self undoEmpty], [self redoEmpty]);
    }
    
    [self _drawPensWithRedraw:TRUE
					 rightNow:TRUE
					  yOffset:0.0
				 andDebugInfo:@"_clear"];
}

- (void)clear
{
    __weak PLSStrokeManager *weakSelf = self;
    dispatch_async(_renderQueue, ^{
        [weakSelf _clear];
    });
}

- (bool)undoEmpty
{
    return self.drawingActions.count <= 0;
}

- (bool)redoEmpty
{
    return self.undidActions.count <= 0;
}

- (bool)getIsVisible
{
    return self.isVisible;
}

- (void)setIsVisible:(BOOL)visible
{
    _isVisible = visible;
    [self drawPensWithRedraw:TRUE rightNow:TRUE andDebugInfo:@"setIsVisible"];
}

- (void)resize:(NSSize)size
{
    __weak PLSStrokeManager *weakSelf = self;
    dispatch_async(self.renderQueue, ^{
        if (weakSelf.size.width == size.width && weakSelf.size.height == size.height)
            return;
		
		CGFloat yOffset = size.height - weakSelf.size.height;
        weakSelf.size = size;
        
#ifndef MAC_DEMO
        if (self.texture) {
            obs_enter_graphics();
            gs_texture_destroy(self.texture);
            obs_leave_graphics();
        }
        obs_enter_graphics();
        self.texture = gs_texture_create(self.size.width, self.size.height, GS_RGBA, 1, nullptr, GS_DYNAMIC);
        obs_leave_graphics();
#endif
        
        [weakSelf _drawPensWithRedraw:TRUE 
							 rightNow:TRUE
							  yOffset:yOffset
						 andDebugInfo:@"resize"];
    });
}

- (void)_rebuildCacheImageData
{
    int iW = (int)self.size.width;
    int iH = (int)self.size.height;
    if (self.currentCanvasRawData)
        free(self.currentCanvasRawData);
    if (self.previousCanvasRawData)
        free(self.previousCanvasRawData);
    self.currentCanvasRawData = (GLubyte *)calloc(iW * iH * 4, sizeof(GLubyte));
    self.previousCanvasRawData = (GLubyte *)calloc(iW * iH * 4, sizeof(GLubyte));
}

#ifndef MAC_DEMO
- (void)_updateTexture
{
    int iW = (int)self.size.width;
    int iH = (int)self.size.height;
    
    obs_enter_graphics();
    if (!self.texture)
        self.texture = gs_texture_create(self.size.width, self.size.height, GS_RGBA, 1, nullptr, GS_DYNAMIC);
    uploadToGPU(self.texture, self.currentCanvasRawData, self.size.width, self.size.height, iW * 4, false);
    pls_scene_update_canvas(OBSBasic::Get()->GetCurrentScene(), self.texture, false);
    obs_leave_graphics();
}
#endif

- (void)_drawPensWithRedraw:(BOOL)needsRedraw
                   rightNow:(BOOL)rightNow
				 yOffset:(CGFloat)yOffset
               andDebugInfo:(NSString *)info
{
    if (!self.isVisible) {
#if MAC_DEMO
        dispatch_async(dispatch_get_main_queue(), ^{
            [[NSNotificationCenter defaultCenter] postNotificationName:@"drawPenDidUpdate" object:[NSImage imageNamed:@""]];
        });
#endif
        return;
    }
    
    auto timeIntervalSinceNow = -self.lastDrawTime.timeIntervalSinceNow * 1000;
    if (needsRedraw || rightNow || self.lastDrawTime == nullptr || timeIntervalSinceNow >= 50) { // limit draw frequency
        self.lastDrawTime = [NSDate now];
        
        if (needsRedraw || self.currentCanvasRawData == nullptr || self.previousCanvasRawData == nullptr) {
            [self _rebuildCacheImageData];
        }
        
        NSDate *start = [NSDate now];
        int iW = (int)self.size.width;
        int iH = (int)self.size.height;
        
        CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        // http://www.uiimage.com/CGBitmapInfo-deep-analysis
        CGContextRef imageContext = CGBitmapContextCreate(self.currentCanvasRawData, iW, iH, 8, iW * 4, colorSpace, kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast); // `kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast` == `GS_RGBA`
        if (needsRedraw) {
            for (int i = 0; i < self.drawingActions.count; i++) {
                PLSStrokeBase *pen = (PLSStrokeBase *)self.drawingActions[i];
                if (pen && [pen isKindOfClass:[PLSStrokeBase class]] && [pen eraseCount] <= 0) {
					[pen recalculatePointsWithYOffset:yOffset];
                    [pen drawInContext:imageContext size:self.size];
                }
            }
        } else {
            memcpy(self.currentCanvasRawData, self.previousCanvasRawData, iW * iH * 4 * sizeof(GLubyte));
            PLSStrokeBase *pen = (PLSStrokeBase *)self.drawingActions.lastObject;
            if (pen && [pen isKindOfClass:[PLSStrokeBase class]]) {
				[pen recalculatePointsWithYOffset:yOffset];
                [pen drawInContext:imageContext size:self.size];
            }
        }
        
#if MAC_DEMO
        CGImageRef cgImage = CGBitmapContextCreateImage(imageContext);
        NSImage *image = [[NSImage alloc] initWithCGImage:cgImage size:CGSizeMake(600, 500)];
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        NSArray *copiedObjects = [NSArray arrayWithObject:image];
        [pasteboard writeObjects:copiedObjects];
        dispatch_async(dispatch_get_main_queue(), ^{
            [[NSNotificationCenter defaultCenter] postNotificationName:@"drawPenDidUpdate" object:image];
        });
        CGImageRelease(cgImage);
#endif
        
        CGContextRelease(imageContext);
        CGColorSpaceRelease(colorSpace);
        
        /// At the end of stroke, `currentStroke` is set to null.
        if (!self.currentStroke) {
            memcpy(self.previousCanvasRawData, self.currentCanvasRawData, iW * iH * 4 * sizeof(GLubyte));
        }
        
#ifndef MAC_DEMO
        [self _updateTexture];
#endif
        
        NSDate *end = [NSDate now];
        NSTimeInterval secs = [end timeIntervalSinceDate:start];
        NSLog(@">>>> %f %lu", secs * 1000, (unsigned long)self.drawingActions.count);
        NSLog(@"draw %@ needsRedraw:%d rightNow: %d", info, needsRedraw, rightNow);
        if (needsRedraw)
            NSLog(@"redraw %@", info);
        else
            NSLog(@"draw %@", info);
    } else {
        NSLog(@"skipped %@", info);
    }
}

- (void)drawPensWithRedraw:(BOOL)needsRedraw rightNow:(BOOL)rightNow andDebugInfo:(NSString *)info
{
    __weak PLSStrokeManager *weakSelf = self;
    dispatch_async(self.renderQueue, ^{
        [weakSelf _drawPensWithRedraw:needsRedraw
							 rightNow:rightNow
							  yOffset:0.0
						 andDebugInfo:info];
    });
}

- (void)breakRetainCycly
{
    _retainBlock = nil;
}

@end

#ifndef MAC_DEMO
bool uploadToGPU(gs_texture_t *texture,
                 const uint8_t *data,
                 uint32_t width,
                 uint32_t height,
                 uint32_t stride,
                 bool flip)
{
    if (!texture  || !data || !width || !height)
        return false;
    
    uint8_t *ptr = nullptr;
    uint32_t pitch = 0;
    if (gs_texture_map(texture, &ptr, &pitch)) {
        if (flip) {
            uint32_t copy_line_size = std::min(stride, pitch);
            for (int i = height - 1; i >= 0; --i) {
                memcpy(ptr, data + i * static_cast<size_t>(stride), copy_line_size);
                ptr += pitch;
            }
        } else {
            if (stride == pitch) {
                memcpy(ptr, data, static_cast<size_t>(stride) * height);
            } else {
                uint32_t copy_line_size = std::min(stride, pitch);
                for (uint32_t i = 0; i < height; ++i) {
                    memcpy(ptr, data + i * static_cast<size_t>(stride), copy_line_size);
                    ptr += pitch;
                }
            }
        }
        gs_texture_unmap(texture);
        return true;
    }
    return false;
}
#endif
