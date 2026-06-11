//
//  PLSStrokeStruct-mac.h
//  QuartzDrawPenDemo
//
//  Created by Zhong Ling on 2023/1/30.
//

#import <AppKit/AppKit.h>

// MARK: - Structs

typedef enum : NSUInteger { Pen = 0, Highlighter = 1, Glow = 2, ArrowLine = 3, Line = 4, Rectangle = 5, Ellipse = 6, Triangle = 7 } BrushMode;

typedef enum : NSUInteger { Thichness0 = 0, Thichness1 = 1, Thichness2 = 2, Thichness3 = 3, Thichness4 = 4 } ThicknessMode;

typedef enum : NSUInteger {
	Color0 = 0,
	Color1 = 1,
	Color2 = 2,
	Color3 = 3,
	Color4 = 4,
	Color5 = 5,
	Color6 = 6,
	Color7 = 7,
	Color8 = 8,
	Color9 = 9,
	Color10 = 10,
	Color11 = 11,
} ColorMode;

typedef enum : NSUInteger {
	DrawAction = 0,
	EraseAction = 1,
	ClearAction = 2,
} ActionType;

// MARK: - PLSStrokeHelper

@interface PLSStrokeHelper : NSObject

+ (CGFloat)getStrokeWidthFromThichnessMode:(ThicknessMode)thicknessMode brushMode:(BrushMode)brushMode;

+ (NSColor *)getStrockeColorFromColorMode:(ColorMode)colorMode brushMode:(BrushMode)brushMode;

+ (CGFloat)getHeadWidthFromThichnessMode:(ThicknessMode)thicknessMode;
+ (CGFloat)getHeadLengthFromThichnessMode:(ThicknessMode)thickMthicknessModeode;

@end
