// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/form_input_accessory_view_delegate.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

@protocol CRWWebViewProxy;

namespace ios_internal {
namespace autofill {
extern NSString* const kFormSuggestionAssistButtonPreviousElement;
extern NSString* const kFormSuggestionAssistButtonNextElement;
extern NSString* const kFormSuggestionAssistButtonDone;
}  // namespace autofill
}  // namespace ios_internal

@protocol FormInputAccessoryViewProvider;
@class FormInputAccessoryViewController;

// Block type to indicate that a FormInputAccessoryViewProvider has an accessory
// view to provide.
typedef void (^AccessoryViewAvailableCompletion)(
    BOOL inputAccessoryViewAvailable);

// Block type to provide an accessory view asynchronously.
typedef void (^AccessoryViewReadyCompletion)(
    UIView* view,
    id<FormInputAccessoryViewProvider> provider);

// Represents an object that can provide a custom keyboard input accessory view.
@protocol FormInputAccessoryViewProvider<NSObject>

// A delegate for form navigation.
@property(nonatomic, assign)
    id<FormInputAccessoryViewDelegate> accessoryViewDelegate;

// Determines asynchronously if this provider has a view available for the
// specified form/field and invokes |completionHandler| with the answer.
- (void)checkIfAccessoryViewAvailableForFormNamed:(const std::string&)formName
                                        fieldName:(const std::string&)fieldName
                                         webState:(web::WebState*)webState
                                completionHandler:
                                    (AccessoryViewAvailableCompletion)
                                        completionHandler;

// Asynchronously retrieves an accessory view from this provider for the
// specified form/field and returns it via |completionHandler|.
- (void)retrieveAccessoryViewForFormNamed:(const std::string&)formName
                                fieldName:(const std::string&)fieldName
                                    value:(const std::string&)value
                                     type:(const std::string&)type
                                 webState:(web::WebState*)webState
                        completionHandler:
                            (AccessoryViewReadyCompletion)completionHandler;

// Notifies this provider that the accessory view is going away.
- (void)inputAccessoryViewControllerDidReset:
        (FormInputAccessoryViewController*)controller;

// Notifies this provider that the accessory view frame is changing. If the
// view provided by this provider needs to change, the updated view should be
// returned using the completion received in
// |retrieveAccessoryViewForForm:field:value:type:webState:completionHandler:|.
- (void)resizeAccessoryView;

@end

// Creates and manages a custom input accessory view while the user is
// interacting with a form. Also handles hiding and showing the default
// accessory view elements.
@interface FormInputAccessoryViewController
    : NSObject<CRWWebStateObserver, FormInputAccessoryViewDelegate>

// Initializes a new controller with the specified |providers| of input
// accessory views.
- (instancetype)initWithWebState:(web::WebState*)webState
                       providers:(NSArray*)providers;

// Hides the default input accessory view and replaces it with one that shows
// |customView| and form navigation controls.
- (void)showCustomInputAccessoryView:(UIView*)customView;

// Restores the default input accessory view, removing (if necessary) any
// previously-added custom view.
- (void)restoreDefaultInputAccessoryView;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_CONTROLLER_H_
