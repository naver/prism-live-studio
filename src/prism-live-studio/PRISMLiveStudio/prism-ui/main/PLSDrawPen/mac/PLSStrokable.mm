//
//  PLSStrokable.m
//  QuartzDrawPenDemo
//
//  Created by Zhong Ling on 2023/1/29.
//

#import "PLSStrokable.hpp"

#import <AppKit/AppKit.h>

// MARK: - PLSStrokeAction

@implementation PLSStrokeAction

- (id)initWithBaseStrokes:(NSArray<NSObject *> *)strokes actionType:(ActionType)actionType
{
    self->_baseStrokes = strokes;
    self->_actionType = actionType;
    return self;
}

@end

// MARK: - PLSStrokeActionDraw

@implementation PLSStrokeActionDraw

- (id)initWithBaseStrokes:(NSArray<NSObject*> *)strokes
{
    self = [self initWithBaseStrokes:strokes actionType:DrawAction];
    return self;
}

@end

// MARK: - PLSStrokeActionEraser

@implementation PLSStrokeActionEraser

- (id)initWithBaseStrokes:(NSArray<NSObject*> *)strokes
{
    self = [super initWithBaseStrokes:strokes actionType:EraseAction];
    return self;
}

@end

// MARK: - PLSStrokeActionClear

@implementation PLSStrokeActionClear

- (id)initWithBaseStrokes:(NSArray<NSObject*> *)strokes
{
    self = [super initWithBaseStrokes:strokes actionType:ClearAction];
    return self;
}

@end

// MARK: - PLSStrokeBase

@implementation PLSStrokeBase

- (id)initWithStrokeColor:(NSColor *)strokeColor
                brushMode:(BrushMode)brushMode
                colorMode:(ColorMode)colorMode
              strokeWidth:(CGFloat)strokeWidth
            thicknessMode:(ThicknessMode)thicknessMode
            useCtrlPoints:(BOOL)useCtrlPoints
                 strokeID:(NSInteger)strokeID
{
    self = [super initWithBaseStrokes:[NSArray array]];
    self->_strokeColor = strokeColor;
    self->_strokeWidth = strokeWidth;
    self->_thicknessMode = thicknessMode;
    self->_colorMode = colorMode;
    self->_eraseCount = 0;
    self->_cgPath = NULL;
    self->_checkErasePath = NULL;
    self->_useCtrlPoints = useCtrlPoints;
    self->_strokeID = strokeID;
    self->_brushMode = brushMode;
    _points = new std::vector<PointF>();
    _ctrlPoints = new std::vector<PointF>();
    return self;
}

- (void)recalculatePointsWithYOffset:(CGFloat)yOffset
{
	if (yOffset == 0.0)
		return;
	
	if (!self.points || !self.ctrlPoints)
		return;
	
	for (auto iter = self.points->begin(); iter != self.points->end(); iter++) {
		iter->y = iter->y + yOffset;
	}
	for (auto iter = self.ctrlPoints->begin(); iter != self.ctrlPoints->end(); iter++ ) {
		iter->y = iter->y + yOffset;
	}
}

- (void)dealloc
{
    if (_cgPath) {
        CGPathRelease(_cgPath);
        _cgPath = NULL;
    }
    
    if (_checkErasePath) {
        CGPathRelease(_checkErasePath);
        _checkErasePath = NULL;
    }
    
    if (_points) {
        delete _points;
        _points = NULL;
    }
    
    if (_ctrlPoints) {
        delete _ctrlPoints;
        _ctrlPoints = NULL;
    }
}

- (void)beginFrom:(NSPoint)point
{
    PointF p;
    p.x = point.x;
    p.y = point.y;
    self->_points->push_back(p);
    if (self.useCtrlPoints) {
        self->_ctrlPoints->clear();
		*_ctrlPoints = *_points;
    }
    
    if (_checkErasePath) {
        CGPathRelease(_checkErasePath);
        _checkErasePath = NULL;
    }
}

- (BOOL)moveToPoint:(NSPoint)point
{
    size_t size = self->_points->size();
    if (size > 0 && self->_points->at(size - 1).x == point.x && self->_points->at(size - 1).y == point.y) {
        return FALSE;
    }
    PointF p;
    p.x = point.x;
    p.y = point.y;
    self->_points->push_back(p);
    if (self.useCtrlPoints) {
        self->_ctrlPoints->clear();
		*_ctrlPoints = *_points;
    }
    return TRUE;
}

- (void)endTo:(NSPoint)point
{
    PointF p;
    p.x = point.x;
    p.y = point.y;
    self->_points->push_back(p);
    if (self.useCtrlPoints) {
        self->_ctrlPoints->clear();
		*_ctrlPoints = *_points;
    }
    
    if (_checkErasePath) {
        CGPathRelease(_checkErasePath);
        _checkErasePath = NULL;
    }
}

- (void)increaseEraseCount
{
    _eraseCount++;
}
- (void)decreaseEraseCount
{
    _eraseCount = std::max(0, _eraseCount - 1);
}

- (BOOL)getErased
{
    return self->_eraseCount > 0;
}

- (BOOL)containsPoint:(NSPoint)point
{
    if (!_cgPath) {
        return FALSE;
    }
    
    if (!_checkErasePath) {
        _checkErasePath = CGPathCreateCopyByStrokingPath(_cgPath, nil, _strokeWidth, kCGLineCapRound, kCGLineJoinRound, 1);
    }
    
    BOOL ret = CGPathContainsPoint(_checkErasePath, nil, point, false);
    return ret;
}

- (void)drawInContext:(CGContextRef)context size:(CGSize)size {
    assert(false);
}

- (CGAffineTransform)dqd_transformForStartPoint:(CGPoint)startPoint
                                       endPoint:(CGPoint)endPoint
                                         length:(CGFloat)length {
    CGFloat cosine = (endPoint.x - startPoint.x) / length;
    CGFloat sine = (endPoint.y - startPoint.y) / length;
    return (CGAffineTransform){ cosine, sine, -sine, cosine, startPoint.x, startPoint.y };
}

@end

// MARK: - PLSStrokePen

@implementation PLSStrokePen

- (void)drawInContext:(CGContextRef)context
                 size:(CGSize)size
{
    if (self.cgPath) {
        CGPathRelease(self.cgPath);
    }
    self.cgPath = CGPathCreateMutable();

    if (self.ctrlPoints->size() < 4) {
        return;
    }
    
    CGContextSaveGState(context);
    CGContextSetStrokeColorWithColor(context, self.strokeColor.CGColor);
    CGContextSetFillColorWithColor(context, NSColor.clearColor.CGColor);
    CGContextSetLineWidth(context, self.strokeWidth);
    CGContextSetLineCap(context, kCGLineCapRound);
    CGContextSetLineJoin(context, kCGLineJoinRound);
    
    int length = (int)self.ctrlPoints->size() / 4;
    for (int i = 0; i < length; i++)
    {
        int s = i * 4;
        CGPathMoveToPoint(self.cgPath,
                          nullptr,
                          self.ctrlPoints->at(s).x,
                          self.ctrlPoints->at(s).y);

        CGPathAddCurveToPoint(self.cgPath, nullptr,
                              self.ctrlPoints->at(s+1).x,
                              self.ctrlPoints->at(s+1).y,
                              self.ctrlPoints->at(s+2).x,
                              self.ctrlPoints->at(s+2).y,
                              self.ctrlPoints->at(s+3).x,
                              self.ctrlPoints->at(s+3).y);
        
    }
    CGContextAddPath(context, self.cgPath);
    CGContextDrawPath(context, kCGPathStroke);
    
    CGContextRestoreGState(context);
}

@end

// MARK: - PLSStrokeHighlighter

@implementation PLSStrokeHighlighter

- (void)drawInContext:(CGContextRef)context
                 size:(CGSize)size
{
    if (self.ctrlPoints->size() < 4) {
        return;
    }

    if (self.cgPath) {
        CGPathRelease(self.cgPath);
    }
    self.cgPath = CGPathCreateMutable();
    
    CGContextSaveGState(context);
    
    CGContextSetStrokeColorWithColor(context, self.strokeColor.CGColor);
    CGContextSetFillColorWithColor(context, NSColor.clearColor.CGColor);
    CGContextSetLineWidth(context, self.strokeWidth);
    CGContextSetLineCap(context, kCGLineCapSquare);
    CGContextSetLineJoin(context, kCGLineJoinMiter);

    for (int i = 0; i < self.ctrlPoints->size() / 4; i++)
    {
        int s = i * 4;
        CGPathMoveToPoint(self.cgPath, nullptr, self.ctrlPoints->at(s).x, self.ctrlPoints->at(s).y);
        CGPathAddCurveToPoint(self.cgPath, nullptr, self.ctrlPoints->at(s+1).x,
                              self.ctrlPoints->at(s+1).y,
                              self.ctrlPoints->at(s+2).x,
                              self.ctrlPoints->at(s+2).y,
                              self.ctrlPoints->at(s+3).x,
                              self.ctrlPoints->at(s+3).y);
    }
    CGContextAddPath(context, self.cgPath);
    CGContextDrawPath(context, kCGPathStroke);
    
    CGContextRestoreGState(context);
}

@end

// MARK: - PLSStrokeGlow

@implementation PLSStrokeGlow
- (id)initWithStrokeColor:(NSColor *)strokeColor
                colorMode:(ColorMode)colorMode
              strokeWidth:(CGFloat)strokeWidth
            thicknessMode:(ThicknessMode)thicknessMode
            useCtrlPoints:(BOOL)useCtrlPoints
                 strokeID:(NSInteger)strokeID
{
    self = [super initWithStrokeColor:strokeColor
                            brushMode:Glow
                            colorMode:colorMode
                          strokeWidth:strokeWidth
                        thicknessMode:thicknessMode
                        useCtrlPoints:useCtrlPoints
                             strokeID:(NSInteger)strokeID];
    self->_cgSubPath = CGPathCreateMutable();
    return self;
}

- (void)dealloc
{
    if (self.cgPath) {
        CGPathRelease(self.cgPath);
        self.cgPath = nullptr;
    }
    
    if (self.cgSubPath) {
        CGPathRelease(self.cgSubPath);
        self.cgSubPath = nullptr;
    }
    
    if (self.checkErasePath) {
        CGPathRelease(self.checkErasePath);
        self.checkErasePath = nullptr;
    }
    
    if (self.points) {
        delete self.points;
        self.points = NULL;
    }
    
    if (self.ctrlPoints) {
        delete self.ctrlPoints;
        self.ctrlPoints = NULL;
    }
}

- (BOOL)containsPoint:(NSPoint)point
{
    if (!self.cgPath || !self.cgSubPath) {
        return FALSE;
    }
    
    if (!self.checkErasePath) {
        self.checkErasePath = CGPathCreateCopyByStrokingPath(self.cgPath, nil, self.strokeWidth, kCGLineCapRound, kCGLineJoinRound, 1);
    }
    
    return CGPathContainsPoint(self.checkErasePath, NULL, point, true);
}

- (void)drawInContext:(CGContextRef)context
                 size:(CGSize)size
{
    if (self.ctrlPoints->size() < 4) {
        return;
    }
    
    if (self.cgPath) {
        CGPathRelease(self.cgPath);
    }
    self.cgPath = CGPathCreateMutable();
    
    if (self.cgSubPath) {
        CGPathRelease(self.cgSubPath);
    }
    self.cgSubPath = CGPathCreateMutable();
    
    CGContextSaveGState(context);
    CGContextSetStrokeColorWithColor(context, self.strokeColor.CGColor);
    CGContextSetFillColorWithColor(context, NSColor.clearColor.CGColor);
    CGContextSetLineWidth(context, self.strokeWidth);
    CGContextSetLineCap(context, kCGLineCapRound);
    CGContextSetLineJoin(context, kCGLineJoinRound);

    for (int i = 0; i < self.ctrlPoints->size() / 4; i++)
    {
        int s = i * 4;
        CGPathMoveToPoint(self.cgPath, nullptr, self.ctrlPoints->at(s).x, self.ctrlPoints->at(s).y);
        CGPathAddCurveToPoint(self.cgPath, nullptr, self.ctrlPoints->at(s+1).x,
                              self.ctrlPoints->at(s+1).y,
                              self.ctrlPoints->at(s+2).x,
                              self.ctrlPoints->at(s+2).y,
                              self.ctrlPoints->at(s+3).x,
                              self.ctrlPoints->at(s+3).y);
    }
    CGContextAddPath(context, self.cgPath);
    CGContextDrawPath(context, kCGPathStroke);
    CGContextRestoreGState(context);
    
    CGContextSaveGState(context);
    CGContextSetStrokeColorWithColor(context, NSColor.whiteColor.CGColor);
    CGContextSetFillColorWithColor(context, NSColor.clearColor.CGColor);
    CGContextSetLineWidth(context, self.strokeWidth * 0.5);
    CGContextSetLineCap(context, kCGLineCapRound);
    CGContextSetLineJoin(context, kCGLineJoinRound);

    for (int i = 0; i < self.ctrlPoints->size() / 4; i++)
    {
        int s = i * 4;
        CGPathMoveToPoint(self.cgSubPath, nullptr, self.ctrlPoints->at(s).x, self.ctrlPoints->at(s).y);
        CGPathAddCurveToPoint(self.cgSubPath, nullptr, self.ctrlPoints->at(s+1).x,
                              self.ctrlPoints->at(s+1).y,
                              self.ctrlPoints->at(s+2).x,
                              self.ctrlPoints->at(s+2).y,
                              self.ctrlPoints->at(s+3).x,
                              self.ctrlPoints->at(s+3).y);
    }
    CGContextAddPath(context, self.cgSubPath);
    CGContextDrawPath(context, kCGPathStroke);

    CGContextRestoreGState(context);
}

@end

// MARK: - PLSStrokeLine

@implementation PLSStrokeLine

- (void)drawInContext:(CGContextRef)context size:(CGSize)size
{
    if (self.points->size() < 2) {
        return;
    }
    
    if (self.cgPath) {
        CGPathRelease(self.cgPath);
    }

    CGContextSaveGState(context);
    CGContextSetStrokeColorWithColor(context, self.strokeColor.CGColor);
    CGContextSetFillColorWithColor(context, self.strokeColor.CGColor);
    CGContextSetLineWidth(context, self.strokeWidth - 2);
    CGContextSetLineCap(context, kCGLineCapRound);
    CGContextSetLineJoin(context, kCGLineJoinRound);

    self.cgPath = [self getPathFromPoint:NSMakePoint(self.points->front().x, self.points->front().y)
                                 toPoint:NSMakePoint(self.points->back().x, self.points->back().y)
                                   width:2];
    CGContextAddPath(context, self.cgPath);
    CGContextDrawPath(context, kCGPathFillStroke);
    CGContextRestoreGState(context);
}

- (CGMutablePathRef)getPathFromPoint:(CGPoint)startPoint
                             toPoint:(CGPoint)endPoint
                               width:(CGFloat)width {
    CGFloat length = hypotf(endPoint.x - startPoint.x, endPoint.y - startPoint.y);

    CGPoint points[4];
    [self getRectPoints:points forLength:length width:width];

    CGAffineTransform transform = [self dqd_transformForStartPoint:startPoint
                                                          endPoint:endPoint
                                                            length:length];

    CGMutablePathRef cgPath = CGPathCreateMutable();
    CGPathAddLines(cgPath, &transform, points, sizeof(points) / sizeof(*points));
    CGPathCloseSubpath(cgPath);

    return cgPath;
}

- (void)getRectPoints:(CGPoint[4])points
            forLength:(CGFloat)length
                width:(CGFloat)width {
    points[0] = CGPointMake(0, width / 2);
    points[1] = CGPointMake(length, width / 2);
    points[2] = CGPointMake(length, -width / 2);
    points[3] = CGPointMake(0, -width / 2);
}

@end

// MARK: - PLSStrokeTriangle

@implementation PLSStrokeTriangle

- (void)drawInContext:(CGContextRef)context
                 size:(CGSize)size
{
    if (self.points->size() < 2) {
        return;
    }
    
    if (self.cgPath) {
        CGPathRelease(self.cgPath);
    }
    self.cgPath = CGPathCreateMutable();
    
    CGContextSaveGState(context);
    
    CGContextSetStrokeColorWithColor(context, self.strokeColor.CGColor);
    CGContextSetFillColorWithColor(context, NSColor.clearColor.CGColor);
    CGContextSetLineWidth(context, self.strokeWidth);
    CGContextSetLineCap(context, kCGLineCapRound);
    CGContextSetLineJoin(context, kCGLineJoinRound);
    CGPoint points[3];
    points[0] = NSMakePoint(self.points->front().x, self.points->front().y);
    points[1].x = self.points->front().x - (self.points->back().x - self.points->front().x);
    points[1].y = self.points->back().y;
    points[2] = NSMakePoint(self.points->back().x, self.points->back().y);
    CGPathMoveToPoint(self.cgPath, nullptr, points[0].x, points[0].y);
    for (int i = 1; i < 3; i++) {
        CGPathAddLineToPoint(self.cgPath, nullptr, points[i].x, points[i].y);
    }
    CGPathCloseSubpath(self.cgPath);
    CGContextAddPath(context, self.cgPath);
    CGContextDrawPath(context, kCGPathStroke);
    
    CGContextRestoreGState(context);
}

@end

// MARK: - PLSStrokeEllipse

@implementation PLSStrokeEllipse

- (void)drawInContext:(CGContextRef)context
                 size:(CGSize)size
{
    if (self.points->size() < 2) {
        return;
    }
    
    if (self.cgPath) {
        CGPathRelease(self.cgPath);
    }
    self.cgPath = CGPathCreateMutable();
    
    CGContextSaveGState(context);
    
    CGContextSetStrokeColorWithColor(context, self.strokeColor.CGColor);
    CGContextSetFillColorWithColor(context, NSColor.clearColor.CGColor);
    CGContextSetLineWidth(context, self.strokeWidth);

    CGRect rect;
    rect.origin.x = MIN(self.points->front().x, self.points->back().x);
    rect.origin.y = MIN(self.points->front().y, self.points->back().y);
    rect.size.width = fabs(self.points->front().x - self.points->back().x);
    rect.size.height = fabs(self.points->front().y - self.points->back().y);

    CGPathAddEllipseInRect(self.cgPath, nullptr, rect);
    CGContextAddPath(context, self.cgPath);
    CGContextDrawPath(context, kCGPathStroke);
    
    CGContextRestoreGState(context);
}

@end

// MARK: - PLSStrokeRectangle

@implementation PLSStrokeRectangle

- (void)drawInContext:(CGContextRef)context
                 size:(CGSize)size
{
    if (self.points->size() < 2) {
        return;
    }
    
    if (self.cgPath) {
        CGPathRelease(self.cgPath);
    }
    self.cgPath = CGPathCreateMutable();
    
    CGContextSaveGState(context);
    
    CGContextSetStrokeColorWithColor(context, self.strokeColor.CGColor);
    CGContextSetFillColorWithColor(context, NSColor.clearColor.CGColor);
    CGContextSetLineWidth(context, self.strokeWidth);

    CGRect rect;
    rect.origin.x = MIN(self.points->front().x, self.points->back().x);
    rect.origin.y = MIN(self.points->front().y, self.points->back().y);
    rect.size.width = fabs(self.points->front().x - self.points->back().x);
    rect.size.height = fabs(self.points->front().y - self.points->back().y);
    
    CGPathAddRect(self.cgPath, nullptr, rect);
    CGContextAddPath(context, self.cgPath);
    CGContextDrawPath(context, kCGPathStroke);
    
    CGContextRestoreGState(context);
}

@end

@implementation PLSStrokeArrowLine

- (id)initWithStrokeColor:(NSColor *)strokeColor
                colorMode:(ColorMode)colorMode
              strokeWidth:(CGFloat)strokeWidth
            thicknessMode:(ThicknessMode)thicknessMode
            useCtrlPoints:(BOOL)useCtrlPoints
                headWidth:(CGFloat)headWidth
               headLength:(CGFloat)headLength
                 strokeID:(NSInteger)strokeID
{
    self = [super initWithStrokeColor:strokeColor
                            brushMode:ArrowLine
                            colorMode:colorMode
                          strokeWidth:strokeWidth
                        thicknessMode:thicknessMode
                        useCtrlPoints:true
                             strokeID:(NSInteger)strokeID];
    self->_headLength = headLength;
    self->_headWidth = headWidth;
    return self;
}

- (void)drawInContext:(CGContextRef)context
                 size:(CGSize)size
{
    if (self.points->size() < 2) {
        return;
    }
    
    if (self.cgPath) {
        CGPathRelease(self.cgPath);
    }

    CGContextSaveGState(context);
    
    CGContextSetStrokeColorWithColor(context, self.strokeColor.CGColor);
    CGContextSetFillColorWithColor(context, self.strokeColor.CGColor);
    CGContextSetLineWidth(context, self.strokeWidth - 2);
    CGContextSetLineCap(context, kCGLineCapRound);
    CGContextSetLineJoin(context, kCGLineJoinRound);

    self.cgPath = [self dqd_bezierPathWithArrowFromPoint:NSMakePoint(self.points->front().x, self.points->front().y)
                                                 toPoint:NSMakePoint(self.points->back().x, self.points->back().y)
                                               tailWidth:2
                                               headWidth:self.headWidth
                                              headLength:self.headLength];
    CGContextAddPath(context, self.cgPath);
    CGContextDrawPath(context, kCGPathFillStroke);
    
    CGContextRestoreGState(context);
}

/// [wiki](https://stackoverflow.com/questions/13528898/how-can-i-draw-an-arrow-using-core-graphics)
- (CGMutablePathRef)dqd_bezierPathWithArrowFromPoint:(CGPoint)startPoint
                                           toPoint:(CGPoint)endPoint
                                         tailWidth:(CGFloat)tailWidth
                                         headWidth:(CGFloat)headWidth
                                        headLength:(CGFloat)headLength {
    CGFloat length = hypotf(endPoint.x - startPoint.x, endPoint.y - startPoint.y);

    CGPoint points[7];
    [self dqd_getAxisAlignedArrowPoints:points
                              forLength:length
                              tailWidth:tailWidth
                              headWidth:headWidth
                             headLength:headLength];

    CGAffineTransform transform = [self dqd_transformForStartPoint:startPoint
                                                          endPoint:endPoint
                                                            length:length];

    CGMutablePathRef cgPath = CGPathCreateMutable();
    CGPathAddLines(cgPath, &transform, points, sizeof(points) / sizeof(*points));
    CGPathCloseSubpath(cgPath);

    return cgPath;
}

- (void)dqd_getAxisAlignedArrowPoints:(CGPoint[7])points
                            forLength:(CGFloat)length
                            tailWidth:(CGFloat)tailWidth
                            headWidth:(CGFloat)headWidth
                           headLength:(CGFloat)headLength {
    CGFloat tailLength = length - headLength;
    points[0] = CGPointMake(0, tailWidth / 2);
    points[1] = CGPointMake(tailLength, tailWidth / 2);
    points[2] = CGPointMake(tailLength, headWidth / 2);
    points[3] = CGPointMake(length, 0);
    points[4] = CGPointMake(tailLength, -headWidth / 2);
    points[5] = CGPointMake(tailLength, -tailWidth / 2);
    points[6] = CGPointMake(0, -tailWidth / 2);
}

@end
