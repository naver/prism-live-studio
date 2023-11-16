//
//  PLSStrokeStruct-mac.m
//  QuartzDrawPenDemo
//
//  Created by Zhong Ling on 2023/1/30.
//

#import "PLSStrokeStruct.hpp"

@implementation PLSStrokeHelper

// MARK: - public

+ (CGFloat)getStrokeWidthFromThichnessMode:(ThicknessMode)thicknessMode
                                 brushMode:(BrushMode)brushMode
{
    CGFloat base = [self getBaseStrokeWidthFromThichnessMode:thicknessMode];
    switch (brushMode) {
        case Pen:
        case Glow:
        case ArrowLine:
        case Line:
        case Rectangle:
        case Ellipse:
        case Triangle:
            return base;
        case Highlighter:
            return base + 2.0;
        default:
            return base;
    }
}

+ (NSColor*)getStrockeColorFromColorMode:(ColorMode)colorMode
                               brushMode:(BrushMode)brushMode
{
    NSArray<NSNumber*>* rgba = [self getRGBAFromColorMode:colorMode];
    
    switch (brushMode) {
        case Pen:
        case ArrowLine:
        case Line:
        case Rectangle:
        case Ellipse:
        case Triangle:
            return [NSColor colorWithSRGBRed:rgba[0].floatValue
                                       green:rgba[1].floatValue
                                        blue:rgba[2].floatValue
                                       alpha:rgba[3].floatValue];
        
        case Glow:
        case Highlighter:
            return [NSColor colorWithSRGBRed:rgba[0].floatValue
                                       green:rgba[1].floatValue
                                        blue:rgba[2].floatValue
                                       alpha:0.6];
            
        default:
            return [NSColor colorWithSRGBRed:rgba[0].floatValue
                                       green:rgba[1].floatValue
                                        blue:rgba[2].floatValue
                                       alpha:rgba[3].floatValue];
    }
}

+ (CGFloat)getHeadWidthFromThichnessMode:(ThicknessMode)thicknessMode
{
    switch (thicknessMode) {
        case Thichness0:
            return 4.0 * 2;
        case Thichness1:
            return 8.0 * 2;
        case Thichness2:
            return 10.0 * 2;
        case Thichness3:
            return 14.0 * 2;
        case Thichness4:
            return 18.0 * 2;
        default:
            return 4.0 * 2;
    }
}

+ (CGFloat)getHeadLengthFromThichnessMode:(ThicknessMode)thicknessMode
{
    switch (thicknessMode) {
        case Thichness0:
            return 4.0 * 2;
        case Thichness1:
            return 8.0 * 2;
        case Thichness2:
            return 10.0 * 2;
        case Thichness3:
            return 14.0 * 2;
        case Thichness4:
            return 18.0 * 2;
        default:
            return 4.0 * 2;
    }
}

// MARK: -- private

+ (CGFloat)getBaseStrokeWidthFromThichnessMode:(ThicknessMode)thicknessMode
{
    switch (thicknessMode) {
        case Thichness0:
            return 4.0;
        case Thichness1:
            return 8.0;
        case Thichness2:
            return 10.0;
        case Thichness3:
            return 14.0;
        case Thichness4:
            return 18.0;
        default:
            return 4.0;
    }
}

+ (NSArray<NSNumber*>*)getRGBAFromColorMode:(ColorMode)colorMode
{
    switch (colorMode) {
        case Color0:
            return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:6.0/255.0],
                    [NSNumber numberWithFloat:247.0/255.0],
                    [NSNumber numberWithFloat:216.0/255.0],
                    [NSNumber numberWithFloat:255.0/255.0], nil];
        case Color1:
            return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:1/255.0],
                    [NSNumber numberWithFloat:165/255.0],
                    [NSNumber numberWithFloat:247/255.0],
                    [NSNumber numberWithFloat:255/255.0], nil];
        case Color2:
            return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:60/255.0],
                    [NSNumber numberWithFloat:95/255.0],
                    [NSNumber numberWithFloat:255/255.0],
                    [NSNumber numberWithFloat:255/255.0], nil];
        case Color3:
            return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:111/255.0],
                    [NSNumber numberWithFloat:217/255.0],
                    [NSNumber numberWithFloat:111/255.0],
                    [NSNumber numberWithFloat:255/255.0], nil];
        case Color4:
            return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:0/255.0],
                    [NSNumber numberWithFloat:0/255.0],
                    [NSNumber numberWithFloat:0/255.0],
                    [NSNumber numberWithFloat:255/255.0], nil];
        case Color5:
            return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:255/255.0],
                    [NSNumber numberWithFloat:255/255.0],
                    [NSNumber numberWithFloat:255/255.0],
                    [NSNumber numberWithFloat:255/255.0], nil];
        case Color6:
            return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:255/255.0],
                    [NSNumber numberWithFloat:77/255.0],
                    [NSNumber numberWithFloat:77/255.0],
                    [NSNumber numberWithFloat:255/255.0], nil];
        case Color7:
            return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:246/255.0],
                    [NSNumber numberWithFloat:19/255.0],
                    [NSNumber numberWithFloat:101/255.0],
                    [NSNumber numberWithFloat:255/255.0], nil];
        case Color8:
            return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:252/255.0],
                    [NSNumber numberWithFloat:64/255.0],
                    [NSNumber numberWithFloat:172/255.0],
                    [NSNumber numberWithFloat:255/255.0], nil];
        case Color9:
            return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:219/255.0],
                    [NSNumber numberWithFloat:6/255.0],
                    [NSNumber numberWithFloat:246/255.0],
                    [NSNumber numberWithFloat:255/255.0], nil];
        case Color10:
            return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:254/255.0],
                    [NSNumber numberWithFloat:241/255.0],
                    [NSNumber numberWithFloat:61/255.0],
                    [NSNumber numberWithFloat:255/255.0], nil];
        case Color11:
            return [NSArray arrayWithObjects:
                    [NSNumber numberWithFloat:252/255.0],
                    [NSNumber numberWithFloat:173/255.0],
                    [NSNumber numberWithFloat:37/255.0],
                    [NSNumber numberWithFloat:255/255.0], nil];
    }
}

@end
