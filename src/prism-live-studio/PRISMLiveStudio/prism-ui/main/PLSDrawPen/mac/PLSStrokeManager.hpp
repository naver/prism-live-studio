//
//  PLSStrokeManager.h
//  QuartzDrawPenDemo
//
//  Created by Zhong Ling on 2023/1/30.
//

#import <Foundation/Foundation.h>

#import "PLSStrokable.hpp"

#ifndef MAC_DEMO
#include <graphics/graphics.h>
#endif

typedef void (^OCDrawPenCallBacak)(void *_Nonnull context, bool isUndoable, bool isRedoable);

NS_ASSUME_NONNULL_BEGIN

// MARK: - PLSStrokeManager

@interface PLSStrokeManager : NSObject

- (id)init;

- (void)beginWithBushMode:(BrushMode)mode startPoint:(NSPoint)point colorMode:(ColorMode)colorMode thicknessMode:(ThicknessMode)thicknessMode;

- (void)moveTo:(NSPoint)point;
- (void)endTo:(NSPoint)point;
- (void)eraseOn:(NSPoint)point;
- (void)undo;
- (void)redo;
- (void)clear;
- (void)resize:(NSSize)size;
- (bool)undoEmpty;
- (bool)redoEmpty;
- (bool)getVisible;
- (void)setVisible:(BOOL)visible;
- (void)drawPensWithRedraw:(BOOL)bForce rightNow:(BOOL)rightNow andDebugInfo:(NSString *)info;

@property (retain, readwrite) NSMutableArray<PLSStrokeAction *> *drawingActions;
@property (retain, readwrite) NSMutableArray<PLSStrokeAction *> *undidActions;
@property (retain, readwrite) PLSStrokeBase *currentStroke;
@property (assign, readwrite) CGSize size;
@property (retain, readwrite) dispatch_queue_t renderQueue;
@property (nonatomic, assign, readwrite) BOOL isVisible;

@property (retain, readonly) OCDrawPenCallBacak drawPenBlock;

@property (assign, readwrite) GLubyte *currentCanvasRawData;
@property (assign, readwrite) GLubyte *previousCanvasRawData;

#ifndef MAC_DEMO
@property (assign, readwrite) gs_texture_t *texture;
#endif

@property (assign, readwrite) NSInteger maxStrokeNo;

@property (retain, readwrite) NSDate *lastDrawTime;

@property (assign, readwrite) void *context;

@end

NS_ASSUME_NONNULL_END
