// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_METHOD_SELECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_METHOD_SELECTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#include "ios/web/public/payments/payment_request.h"

namespace autofill {
class CreditCard;
}

@class PaymentMethodSelectionViewController;

@protocol PaymentMethodSelectionViewControllerDelegate<NSObject>

- (void)paymentMethodSelectionViewController:
            (PaymentMethodSelectionViewController*)controller
                       selectedPaymentMethod:
                           (autofill::CreditCard*)paymentMethod;
- (void)paymentMethodSelectionViewControllerDidReturn:
    (PaymentMethodSelectionViewController*)controller;

@end

// View controller responsible for presenting the available payment methods for
// selection by the user and communicating their choice to the supplied
// delegate. Also offers a button to add a new payment method.
@interface PaymentMethodSelectionViewController : CollectionViewController

// The payment methods available to fulfill the payment request.
@property(nonatomic, assign) std::vector<autofill::CreditCard*> paymentMethods;

// The payment method selected by the user, if any.
@property(nonatomic, assign) autofill::CreditCard* selectedPaymentMethod;

// The delegate to be notified when the user selects a payment method or chooses
// to return without selecting one.
@property(nonatomic, weak) id<PaymentMethodSelectionViewControllerDelegate>
    delegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_METHOD_SELECTION_VIEW_CONTROLLER_H_
