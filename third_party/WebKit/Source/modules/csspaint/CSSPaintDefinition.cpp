// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/CSSPaintDefinition.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/cssom/FilteredComputedStylePropertyMap.h"
#include "core/dom/ExecutionContext.h"
#include "core/layout/LayoutObject.h"
#include "modules/csspaint/PaintRenderingContext2D.h"
#include "modules/csspaint/PaintSize.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/PaintGeneratedImage.h"
#include "platform/graphics/RecordingImageBufferSurface.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

IntSize getSpecifiedSize(const IntSize& size, float zoom) {
  float unZoomFactor = 1 / zoom;
  auto unZoomFn = [unZoomFactor](int a) -> int {
    return round(a * unZoomFactor);
  };
  return IntSize(unZoomFn(size.width()), unZoomFn(size.height()));
}

}  // namespace

CSSPaintDefinition* CSSPaintDefinition::create(
    ScriptState* scriptState,
    v8::Local<v8::Function> constructor,
    v8::Local<v8::Function> paint,
    Vector<CSSPropertyID>& nativeInvalidationProperties,
    Vector<AtomicString>& customInvalidationProperties,
    Vector<CSSSyntaxDescriptor>& inputArgumentTypes,
    bool hasAlpha) {
  return new CSSPaintDefinition(
      scriptState, constructor, paint, nativeInvalidationProperties,
      customInvalidationProperties, inputArgumentTypes, hasAlpha);
}

CSSPaintDefinition::CSSPaintDefinition(
    ScriptState* scriptState,
    v8::Local<v8::Function> constructor,
    v8::Local<v8::Function> paint,
    Vector<CSSPropertyID>& nativeInvalidationProperties,
    Vector<AtomicString>& customInvalidationProperties,
    Vector<CSSSyntaxDescriptor>& inputArgumentTypes,
    bool hasAlpha)
    : m_scriptState(scriptState),
      m_constructor(scriptState->isolate(), constructor),
      m_paint(scriptState->isolate(), paint),
      m_didCallConstructor(false),
      m_hasAlpha(hasAlpha) {
  m_nativeInvalidationProperties.swap(nativeInvalidationProperties);
  m_customInvalidationProperties.swap(customInvalidationProperties);
  m_inputArgumentTypes.swap(inputArgumentTypes);
}

CSSPaintDefinition::~CSSPaintDefinition() {}

PassRefPtr<Image> CSSPaintDefinition::paint(
    const LayoutObject& layoutObject,
    const IntSize& size,
    float zoom,
    const CSSStyleValueVector* paintArguments) {
  DCHECK(paintArguments);

  const IntSize specifiedSize = getSpecifiedSize(size, zoom);

  ScriptState::Scope scope(m_scriptState.get());

  maybeCreatePaintInstance();

  v8::Isolate* isolate = m_scriptState->isolate();
  v8::Local<v8::Object> instance = m_instance.newLocal(isolate);

  // We may have failed to create an instance class, in which case produce an
  // invalid image.
  if (isUndefinedOrNull(instance))
    return nullptr;

  DCHECK(layoutObject.node());

  PaintRenderingContext2D* renderingContext = PaintRenderingContext2D::create(
      ImageBuffer::create(WTF::wrapUnique(
          new RecordingImageBufferSurface(size, nullptr /* fallbackFactory */,
                                          m_hasAlpha ? NonOpaque : Opaque))),
      m_hasAlpha, zoom);
  PaintSize* paintSize = PaintSize::create(specifiedSize);
  StylePropertyMap* styleMap = FilteredComputedStylePropertyMap::create(
      CSSComputedStyleDeclaration::create(layoutObject.node()),
      m_nativeInvalidationProperties, m_customInvalidationProperties,
      layoutObject.node());

  v8::Local<v8::Value> argv[] = {
      ToV8(renderingContext, m_scriptState->context()->Global(), isolate),
      ToV8(paintSize, m_scriptState->context()->Global(), isolate),
      ToV8(styleMap, m_scriptState->context()->Global(), isolate),
      ToV8(*paintArguments, m_scriptState->context()->Global(), isolate)};

  v8::Local<v8::Function> paint = m_paint.newLocal(isolate);

  v8::TryCatch block(isolate);
  block.SetVerbose(true);

  V8ScriptRunner::callFunction(paint, m_scriptState->getExecutionContext(),
                               instance, WTF_ARRAY_LENGTH(argv), argv, isolate);

  // The paint function may have produced an error, in which case produce an
  // invalid image.
  if (block.HasCaught()) {
    return nullptr;
  }

  return PaintGeneratedImage::create(
      renderingContext->imageBuffer()->getRecord(), specifiedSize);
}

void CSSPaintDefinition::maybeCreatePaintInstance() {
  if (m_didCallConstructor)
    return;

  DCHECK(m_instance.isEmpty());

  v8::Isolate* isolate = m_scriptState->isolate();
  v8::Local<v8::Function> constructor = m_constructor.newLocal(isolate);
  DCHECK(!isUndefinedOrNull(constructor));

  v8::Local<v8::Object> paintInstance;
  if (V8ObjectConstructor::newInstance(isolate, constructor)
          .ToLocal(&paintInstance)) {
    m_instance.set(isolate, paintInstance);
  }

  m_didCallConstructor = true;
}

}  // namespace blink
