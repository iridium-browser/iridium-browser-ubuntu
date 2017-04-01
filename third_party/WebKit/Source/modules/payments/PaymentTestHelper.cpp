// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentTestHelper.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "modules/payments/PaymentCurrencyAmount.h"
#include "modules/payments/PaymentMethodData.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {
namespace {

static int gUniqueId = 0;
// PaymentItem and PaymentShippingOption have identical structure
// except for the "id" field, which is present only in PaymentShippingOption.
template <typename PaymentItemOrPaymentShippingOption>
void setValues(PaymentItemOrPaymentShippingOption& original,
               PaymentTestDataToChange data,
               PaymentTestModificationType modificationType,
               const String& valueToUse) {
  PaymentCurrencyAmount itemAmount;
  if (data == PaymentTestDataCurrencyCode) {
    if (modificationType == PaymentTestOverwriteValue)
      itemAmount.setCurrency(valueToUse);
  } else {
    itemAmount.setCurrency("USD");
  }

  // Currency system is "urn:iso:std:iso:4217" by default.
  if (data == PaymentTestDataCurrencySystem) {
    if (modificationType == PaymentTestOverwriteValue)
      itemAmount.setCurrencySystem(valueToUse);
    else
      itemAmount.setCurrencySystem(String());  // null string.
  }

  if (data == PaymentTestDataValue) {
    if (modificationType == PaymentTestOverwriteValue)
      itemAmount.setValue(valueToUse);
  } else {
    itemAmount.setValue("9.99");
  }

  if (data != PaymentTestDataAmount || modificationType != PaymentTestRemoveKey)
    original.setAmount(itemAmount);

  if (data == PaymentTestDataLabel) {
    if (modificationType == PaymentTestOverwriteValue)
      original.setLabel(valueToUse);
  } else {
    original.setLabel("Label");
  }
}

}  // namespace

PaymentItem buildPaymentItemForTest(
    PaymentTestDataToChange data,
    PaymentTestModificationType modificationType,
    const String& valueToUse) {
  DCHECK_NE(data, PaymentTestDataId);
  PaymentItem item;
  setValues(item, data, modificationType, valueToUse);
  return item;
}

PaymentShippingOption buildShippingOptionForTest(
    PaymentTestDataToChange data,
    PaymentTestModificationType modificationType,
    const String& valueToUse) {
  PaymentShippingOption shippingOption;
  if (data == PaymentTestDataId) {
    if (modificationType == PaymentTestOverwriteValue)
      shippingOption.setId(valueToUse);
  } else {
    shippingOption.setId("id" + String::number(gUniqueId++));
  }
  setValues(shippingOption, data, modificationType, valueToUse);
  return shippingOption;
}

PaymentDetailsModifier buildPaymentDetailsModifierForTest(
    PaymentTestDetailToChange detail,
    PaymentTestDataToChange data,
    PaymentTestModificationType modificationType,
    const String& valueToUse) {
  PaymentItem total;
  if (detail == PaymentTestDetailModifierTotal)
    total = buildPaymentItemForTest(data, modificationType, valueToUse);
  else
    total = buildPaymentItemForTest();

  PaymentItem item;
  if (detail == PaymentTestDetailModifierItem)
    item = buildPaymentItemForTest(data, modificationType, valueToUse);
  else
    item = buildPaymentItemForTest();

  PaymentDetailsModifier modifier;
  modifier.setSupportedMethods(Vector<String>(1, "foo"));
  modifier.setTotal(total);
  modifier.setAdditionalDisplayItems(HeapVector<PaymentItem>(1, item));
  return modifier;
}

PaymentDetails buildPaymentDetailsForTest(
    PaymentTestDetailToChange detail,
    PaymentTestDataToChange data,
    PaymentTestModificationType modificationType,
    const String& valueToUse) {
  PaymentItem total;
  if (detail == PaymentTestDetailTotal)
    total = buildPaymentItemForTest(data, modificationType, valueToUse);
  else
    total = buildPaymentItemForTest();

  PaymentItem item;
  if (detail == PaymentTestDetailItem)
    item = buildPaymentItemForTest(data, modificationType, valueToUse);
  else
    item = buildPaymentItemForTest();

  PaymentShippingOption shippingOption;
  if (detail == PaymentTestDetailShippingOption)
    shippingOption =
        buildShippingOptionForTest(data, modificationType, valueToUse);
  else
    shippingOption = buildShippingOptionForTest();

  PaymentDetailsModifier modifier;
  if (detail == PaymentTestDetailModifierTotal ||
      detail == PaymentTestDetailModifierItem)
    modifier = buildPaymentDetailsModifierForTest(detail, data,
                                                  modificationType, valueToUse);
  else
    modifier = buildPaymentDetailsModifierForTest();

  PaymentDetails result;
  result.setTotal(total);
  result.setDisplayItems(HeapVector<PaymentItem>(1, item));
  result.setShippingOptions(
      HeapVector<PaymentShippingOption>(1, shippingOption));
  result.setModifiers(HeapVector<PaymentDetailsModifier>(1, modifier));

  if (detail == PaymentTestDetailError)
    result.setError(valueToUse);

  return result;
}

PaymentDetails buildPaymentDetailsErrorMsgForTest(const String& valueToUse) {
  return buildPaymentDetailsForTest(PaymentTestDetailError, PaymentTestDataNone,
                                    PaymentTestOverwriteValue, valueToUse);
}

HeapVector<PaymentMethodData> buildPaymentMethodDataForTest() {
  HeapVector<PaymentMethodData> methodData(1, PaymentMethodData());
  methodData[0].setSupportedMethods(Vector<String>(1, "foo"));
  return methodData;
}

payments::mojom::blink::PaymentResponsePtr buildPaymentResponseForTest() {
  payments::mojom::blink::PaymentResponsePtr result =
      payments::mojom::blink::PaymentResponse::New();
  return result;
}

void makePaymentRequestOriginSecure(Document& document) {
  document.setSecurityOrigin(
      SecurityOrigin::create(KURL(KURL(), "https://www.example.com/")));
}

PaymentRequestMockFunctionScope::PaymentRequestMockFunctionScope(
    ScriptState* scriptState)
    : m_scriptState(scriptState) {}

PaymentRequestMockFunctionScope::~PaymentRequestMockFunctionScope() {
  v8::MicrotasksScope::PerformCheckpoint(m_scriptState->isolate());
  for (MockFunction* mockFunction : m_mockFunctions) {
    testing::Mock::VerifyAndClearExpectations(mockFunction);
  }
}

v8::Local<v8::Function> PaymentRequestMockFunctionScope::expectCall(
    String* captor) {
  m_mockFunctions.push_back(new MockFunction(m_scriptState, captor));
  EXPECT_CALL(*m_mockFunctions.back(), call(testing::_));
  return m_mockFunctions.back()->bind();
}

v8::Local<v8::Function> PaymentRequestMockFunctionScope::expectCall() {
  m_mockFunctions.push_back(new MockFunction(m_scriptState));
  EXPECT_CALL(*m_mockFunctions.back(), call(testing::_));
  return m_mockFunctions.back()->bind();
}

v8::Local<v8::Function> PaymentRequestMockFunctionScope::expectNoCall() {
  m_mockFunctions.push_back(new MockFunction(m_scriptState));
  EXPECT_CALL(*m_mockFunctions.back(), call(testing::_)).Times(0);
  return m_mockFunctions.back()->bind();
}

ACTION_P(SaveValueIn, captor) {
  *captor = toCoreString(arg0.v8Value()
                             ->ToString(arg0.getScriptState()->context())
                             .ToLocalChecked());
}

PaymentRequestMockFunctionScope::MockFunction::MockFunction(
    ScriptState* scriptState)
    : ScriptFunction(scriptState) {
  ON_CALL(*this, call(testing::_)).WillByDefault(testing::ReturnArg<0>());
}

PaymentRequestMockFunctionScope::MockFunction::MockFunction(
    ScriptState* scriptState,
    String* captor)
    : ScriptFunction(scriptState), m_value(captor) {
  ON_CALL(*this, call(testing::_))
      .WillByDefault(
          testing::DoAll(SaveValueIn(m_value), testing::ReturnArg<0>()));
}

v8::Local<v8::Function> PaymentRequestMockFunctionScope::MockFunction::bind() {
  return bindToV8Function();
}

}  // namespace blink
