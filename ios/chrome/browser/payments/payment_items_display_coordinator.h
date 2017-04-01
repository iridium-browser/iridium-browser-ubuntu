// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_ITEMS_DISPLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_ITEMS_DISPLAY_COORDINATOR_H_

#import <UIKit/UIKit.h>
#include <vector>

#import "ios/chrome/browser/chrome_coordinator.h"
#import "ios/chrome/browser/payments/payment_items_display_view_controller.h"

@class PaymentItemsDisplayCoordinator;

// Protocol for communicating with the delegate supplied to
// PaymentItemsDisplayCoordinator.
@protocol PaymentItemsDisplayCoordinatorDelegate<NSObject>

// Notifies the delegate that the user has chosen to return to the previous
// screen.
- (void)paymentItemsDisplayCoordinatorDidReturn:
    (PaymentItemsDisplayCoordinator*)coordinator;

// Notifies the delegate that the user has confirmed the payment.
- (void)paymentItemsDisplayCoordinatorDidConfirm:
    (PaymentItemsDisplayCoordinator*)coordinator;

@end

// Coordinator responsible for creating and presenting the payment items display
// view controller. This view controller will be presented by the view
// controller provided in the initializer.
@interface PaymentItemsDisplayCoordinator : ChromeCoordinator

// The payment total to be displayed to the user.
@property(nonatomic, assign) web::PaymentItem total;

// The line items to be displayed to the user.
@property(nonatomic, assign) std::vector<web::PaymentItem> paymentItems;

// Whether the pay button should be enabled.
@property(nonatomic, assign) BOOL payButtonEnabled;

// The delegate to be notified when the user selects touches the return button
// or the pay button.
@property(nonatomic, weak) id<PaymentItemsDisplayCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_ITEMS_DISPLAY_COORDINATOR_H_
