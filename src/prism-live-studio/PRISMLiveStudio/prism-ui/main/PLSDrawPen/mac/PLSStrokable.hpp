//
//  PLSStrokable.h
//  QuartzDrawPenDemo
//
//  Created by Zhong Ling on 2023/1/29.
//

#import <Foundation/Foundation.h>

#import <AppKit/NSBezierPath.h>
#include <vector>

#import "PLSStrokeStruct.hpp"
#include "PLSDrawPen/PLSDrawPenDefine.h"

typedef std::vector<PointF>* PointFsPointer;

#ifdef MAC_DEMO
#import "PLSCurveSmooth.h"
#import "PLSDrawpenDefine.h"
#else
#endif

// MARK: - PLSStrokeAction

@interface PLSStrokeAction : NSObject

- (id)initWithBaseStrokes:(NSArray<NSObject *> *)strokes actionType:(ActionType)actionType;

@property (copy, readonly) NSArray<NSObject *> *baseStrokes;
@property (assign, readonly) ActionType actionType;
@end

// MARK: - PLSStrokeDraw

@interface PLSStrokeActionDraw : PLSStrokeAction

- (id)initWithBaseStrokes:(NSArray<NSObject *> *)strokes;
@end

// MARK: - PLSStrokeEraser

@interface PLSStrokeActionEraser : PLSStrokeAction

- (id)initWithBaseStrokes:(NSArray<NSObject *> *)strokes;
@end

// MARK: - PLSStrokeClear

@interface PLSStrokeActionClear : PLSStrokeAction

- (id)initWithBaseStrokes:(NSArray<NSObject *> *)strokes;
@end

// MARK: - PLSStrokable

@protocol PLSStrokable <NSObject>

- (void)beginFrom:(NSPoint)point;
- (BOOL)moveToPoint:(NSPoint)point;
- (void)endTo:(NSPoint)point;

- (void)increaseEraseCount;
- (void)decreaseEraseCount;
- (BOOL)getErased;

- (void)drawInContext:(CGContextRef)context size:(CGSize)size;

- (BOOL)containsPoint:(NSPoint)point;
@end

// MARK: - PLSStrokeBase

@interface PLSStrokeBase : PLSStrokeActionDraw <PLSStrokable>

- (id)initWithStrokeColor:(NSColor *)strokeColor
		brushMode:(BrushMode)brushMode
		colorMode:(ColorMode)colorMode
	      strokeWidth:(CGFloat)strokeWidth
	    thicknessMode:(ThicknessMode)thicknessMode
	    useCtrlPoints:(BOOL)useCtrlPoints
		 strokeID:(NSInteger)strokeID;

- (void)recalculatePointsWithYOffset:(CGFloat)yOffset;

- (void)dealloc;

@property (copy, readonly) NSColor *strokeColor;
@property (assign, readonly) CGFloat strokeWidth;
@property (assign, readonly) int eraseCount;
@property (assign, readonly) BrushMode brushMode;
@property (assign, readonly) ColorMode colorMode;
@property (assign, readonly) ThicknessMode thicknessMode;
@property (assign, readwrite) PointFsPointer points;
@property (assign, readwrite) CGMutablePathRef cgPath;
@property (assign, readwrite) CGPathRef checkErasePath;
@property (assign, readwrite) PointFsPointer ctrlPoints;
@property (assign, readonly) BOOL useCtrlPoints;
@property (assign, readwrite) NSInteger strokeID;
@end

// MARK: - PLSStrokePen

@interface PLSStrokePen : PLSStrokeBase

@end

// MARK: - PLSStrokeHighlighter

@interface PLSStrokeHighlighter : PLSStrokeBase

@end

// MARK: - PLSStrokeGlow

@interface PLSStrokeGlow : PLSStrokeBase

@property (assign, readwrite) CGMutablePathRef cgSubPath;
@end

// MARK: - PLSStrokeLine

@interface PLSStrokeLine : PLSStrokeBase

@end

// MARK: - PLSStrokeTriangle

@interface PLSStrokeTriangle : PLSStrokeBase

@end

// MARK: - PLSStrokeEllipse

@interface PLSStrokeEllipse : PLSStrokeBase

@end

// MARK: - PLSStrokeRectangle

@interface PLSStrokeRectangle : PLSStrokeBase

@end

// MARK: - PLSStrokeArrowLine

@interface PLSStrokeArrowLine : PLSStrokeBase

- (id)initWithStrokeColor:(NSColor *)strokeColor
		colorMode:(ColorMode)colorMode
	      strokeWidth:(CGFloat)strokeWidth
	    thicknessMode:(ThicknessMode)thicknessMode
	    useCtrlPoints:(BOOL)useCtrlPoints
		headWidth:(CGFloat)headWidth
	       headLength:(CGFloat)headLength
		 strokeID:(NSInteger)strokeID;

@property (assign, readonly) CGFloat headWidth;
@property (assign, readonly) CGFloat headLength;
@end
