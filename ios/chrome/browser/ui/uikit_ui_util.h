// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UIKIT_UI_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UIKIT_UI_UTIL_H_

#include <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ui_util.h"

// UI Util containing functions that require UIKit.

enum { FONT_HELVETICA, FONT_HELVETICA_NEUE, FONT_HELVETICA_NEUE_LIGHT };

// Utility function to set the |element|'s accessibility label to the localized
// message corresponding to |idsAccessibilityLabel| and its accessibility
// identifier to |englishUiAutomationName|.
// Call SetA11yLabelAndUiAutomationName() if |element| is accessible and its
// a11y label should be localized.
// By convention |englishUiAutomationName| must be equal to the English
// localized string corresponding to |idsAccessibilityLabel|.
// |englishUiAutomationName| is the name used in JavaScript UI Automation test
// scripts to identify the |element|.
void SetA11yLabelAndUiAutomationName(UIView* element,
                                     int idsAccessibilityLabel,
                                     NSString* englishUiAutomationName);

// Sets the given |button|'s width to exactly fit its image and text.  Does not
// modify the button's height.
void GetSizeButtonWidthToFit(UIButton* button);

// Translates the given |view|'s frame.  Sets a new frame instead of applying a
// transform to the existing frame.
void TranslateFrame(UIView* view, UIOffset offset);

// Returns a UIFont. |fontFace| is one of the defined enumerated values
// to avoid spelling mistakes.
UIFont* GetUIFont(int fontFace, bool isBold, CGFloat fontSize);

// Adds a border shadow around |view|.
void AddBorderShadow(UIView* view, CGFloat offset, UIColor* color);

// Adds a rounded-rectangle border shadow around a view.
void AddRoundedBorderShadow(UIView* view, CGFloat radius, UIColor* color);

// Captures and returns an autoreleased rendering of the |view|.
// The |view| is assumed to be opaque and the returned image does
// not have an alpha channel. The scale parameter is used as a scale factor
// for the rendering context. Using 0.0 as scale will result in the device's
// main screen scale to be used.
UIImage* CaptureView(UIView* view, CGFloat scale);

// Converts input image and returns a grey scaled version.
UIImage* GreyImage(UIImage* image);

// Returns the color that should be used for the background of all Settings
// pages.
UIColor* GetSettingsBackgroundColor();

// Returns the color used as the main color for primary action buttons.
UIColor* GetPrimaryActionButtonColor();

// Returns an UIColor with |rgb| and |alpha|. The caller should pass the RGB
// value in hexadecimal as this is the typical way they are provided by UX.
// For example a call to |UIColorFromRGB(0xFF7D40, 1.0)| returns an orange
// UIColor object.
inline UIColor* UIColorFromRGB(int rgb, CGFloat alpha = 1.0) {
  return [UIColor colorWithRed:((CGFloat)((rgb & 0xFF0000) >> 16)) / 255.0
                         green:((CGFloat)((rgb & 0x00FF00) >> 8)) / 255.0
                          blue:((CGFloat)(rgb & 0x0000FF)) / 255.0
                         alpha:alpha];
}

// Returns an image resized to |targetSize|. It first calculate the projection
// by calling CalculateProjection() and then create a new image of the desired
// size and project the correct subset of the originla image onto it.
//
// Image interpolation level for resizing is set to kCGInterpolationDefault.
//
// The resize always preserves the scale of the original image.
UIImage* ResizeImage(UIImage* image,
                     CGSize targetSize,
                     ProjectionMode projectionMode);

// Returns a slightly blurred image darkened enough to provide contrast for
// white text to be readable.
UIImage* DarkenImage(UIImage* image);

// Applies various effects to an image. This method can apply a blur over a
// |radius|, superimpose a |tintColor| (an alpha of 0.6 on the color is a good
// approximation to look like iOS tint colors) or saturate the image colors by
// applying a |saturationDeltaFactor| (negative to desaturate, positive to
// saturate). The optional |maskImage| is used to limit the effect of the blur
// and/or saturation to a portion of the image.
UIImage* BlurImage(UIImage* image,
                   CGFloat blurRadius,
                   UIColor* tintColor,
                   CGFloat saturationDeltaFactor,
                   UIImage* maskImage);

// Returns a cropped image using |cropRect| on |image|.
UIImage* CropImage(UIImage* image, const CGRect& cropRect);

// Returns the interface orientation of the app.
UIInterfaceOrientation GetInterfaceOrientation();

// Returns the height of the keyboard in the current orientation.
CGFloat CurrentKeyboardHeight(NSValue* keyboardFrameValue);

// Create 1x1px image from |color|.
UIImage* ImageWithColor(UIColor* color);

// Returns a circular image of width |width| based on |image| scaled up or
// down. If the source image is not square, the image is first cropped.
UIImage* CircularImageFromImage(UIImage* image, CGFloat width);

// Returns the linear interpolated color from |firstColor| to |secondColor| by
// the given |fraction|. Requires that both colors are in RGB or monochrome
// color space. |fraction| is a decimal value between 0.0 and 1.0.
UIColor* InterpolateFromColorToColor(UIColor* firstColor,
                                     UIColor* secondColor,
                                     CGFloat fraction);

// Applies all |constraints| to all views in |subviewsDictionary| in the
// superview |parentView|.
void ApplyVisualConstraints(NSArray* constraints,
                            NSDictionary* subviewsDictionary,
                            UIView* parentView);

// Applies all |constraints| with |options| to all views in |subviewsDictionary|
// in the superview |parentView|.
void ApplyVisualConstraintsWithOptions(NSArray* constraints,
                                       NSDictionary* subviewsDictionary,
                                       NSLayoutFormatOptions options,
                                       UIView* parentView);

// Applies all |constraints| with |metrics| to all views in |subviewsDictionary|
// in the superview |parentView|
void ApplyVisualConstraintsWithMetrics(NSArray* constraints,
                                       NSDictionary* subviewsDictionary,
                                       NSDictionary* metrics,
                                       UIView* parentView);

// Applies all |constraints| with |metrics| and |options| to all views in
// |subviewsDictionary| in the superview |parentView|
void ApplyVisualConstraintsWithMetricsAndOptions(
    NSArray* constraints,
    NSDictionary* subviewsDictionary,
    NSDictionary* metrics,
    NSLayoutFormatOptions options,
    UIView* parentView);

// Adds a constraint that |subview| is center aligned horizontally in
// |parentView|.
// |subview| must be a subview of |parentView|.
void AddSameCenterXConstraint(UIView* parentView, UIView* subview);

// Adds a constraint that |subview1| and |subview2| are center aligned
// horizontally on |parentView|.
// |subview1| and |subview2| must be subview of |parentView|.
void AddSameCenterXConstraint(UIView *parentView, UIView *subview1,
                              UIView *subview2);

// Adds a constraint that |subview| is center aligned vertically in
// |parentView|.
// |subview| must be a subview of |parentView|.
void AddSameCenterYConstraint(UIView* parentView, UIView* subview);

// Adds a constraint that |subview1| and |subview2| are center aligned
// vertically on |parentView|.
// |subview1| and |subview2| must be subview of |parentView|.
void AddSameCenterYConstraint(UIView* parentView,
                              UIView* subview1,
                              UIView* subview2);

// Whether the |environment| has a compact horizontal size class.
bool IsCompact(id<UITraitEnvironment> environment);

// Whether the main application window's rootViewController has a compact
// horizontal size class.
bool IsCompact();

// Whether the |environment| has a compact iPad horizontal size class.
bool IsCompactTablet(id<UITraitEnvironment> environment);

// Whether the main application window's rootViewController has a compact
// iPad horizontal size class.
bool IsCompactTablet();

#endif  // IOS_CHROME_BROWSER_UI_UIKIT_UI_UTIL_H_
