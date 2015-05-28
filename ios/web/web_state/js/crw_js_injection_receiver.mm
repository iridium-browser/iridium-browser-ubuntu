// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/web/public/web_state/js/crw_js_injection_evaluator.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

@implementation CRWJSInjectionReceiver {
  // Used to evaluate JavaScripts.
  id<CRWJSInjectionEvaluator> _evaluator;

  // Map from a CRWJSInjectionManager class to its instance created for this
  // receiver.
  base::scoped_nsobject<NSMutableDictionary> _managers;
}

- (id)init {
  NOTREACHED();
  return [super init];
}

- (id)initWithEvaluator:(id<CRWJSInjectionEvaluator>)evaluator {
  DCHECK(evaluator);
  self = [super init];
  if (self) {
    _evaluator = evaluator;
    _managers.reset([[NSMutableDictionary alloc] init]);
  }
  return self;
}

#pragma mark -
#pragma mark CRWJSInjectionEvaluatorMethods

- (void)evaluateJavaScript:(NSString*)script
       stringResultHandler:(web::JavaScriptCompletion)handler {
  [_evaluator evaluateJavaScript:script stringResultHandler:handler];
}

- (BOOL)scriptHasBeenInjectedForClass:(Class)jsInjectionManagerClass
                       presenceBeacon:(NSString*)beacon {
  return [_evaluator scriptHasBeenInjectedForClass:jsInjectionManagerClass
                                    presenceBeacon:beacon];
}

- (void)injectScript:(NSString*)script forClass:(Class)jsInjectionManagerClass {
  [_evaluator injectScript:script forClass:jsInjectionManagerClass];
}

- (web::WebViewType)webViewType {
  return [_evaluator webViewType];
}

- (CRWJSInjectionManager*)instanceOfClass:(Class)jsInjectionManagerClass {
  DCHECK(_managers);
  CRWJSInjectionManager* manager =
      [_managers objectForKey:jsInjectionManagerClass];
  if (!manager) {
    base::scoped_nsobject<CRWJSInjectionManager> newManager(
        [[jsInjectionManagerClass alloc] initWithReceiver:self]);
    [_managers setObject:newManager forKey:jsInjectionManagerClass];
    manager = newManager;
  }
  DCHECK(manager);
  for (Class depedencyClass in [manager directDependencies]) {
    [self instanceOfClass:depedencyClass];
  }
  return manager;
}

@end

@implementation CRWJSInjectionReceiver (Testing)
- (NSDictionary*)managers {
  return _managers.get();
}
@end
