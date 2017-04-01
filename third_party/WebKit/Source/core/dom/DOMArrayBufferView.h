// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMArrayBufferView_h
#define DOMArrayBufferView_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMSharedArrayBuffer.h"
#include "wtf/typed_arrays/ArrayBufferView.h"

namespace blink {

class CORE_EXPORT DOMArrayBufferView
    : public GarbageCollectedFinalized<DOMArrayBufferView>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  typedef WTF::ArrayBufferView::ViewType ViewType;
  static const ViewType TypeInt8 = WTF::ArrayBufferView::TypeInt8;
  static const ViewType TypeUint8 = WTF::ArrayBufferView::TypeUint8;
  static const ViewType TypeUint8Clamped =
      WTF::ArrayBufferView::TypeUint8Clamped;
  static const ViewType TypeInt16 = WTF::ArrayBufferView::TypeInt16;
  static const ViewType TypeUint16 = WTF::ArrayBufferView::TypeUint16;
  static const ViewType TypeInt32 = WTF::ArrayBufferView::TypeInt32;
  static const ViewType TypeUint32 = WTF::ArrayBufferView::TypeUint32;
  static const ViewType TypeFloat32 = WTF::ArrayBufferView::TypeFloat32;
  static const ViewType TypeFloat64 = WTF::ArrayBufferView::TypeFloat64;
  static const ViewType TypeDataView = WTF::ArrayBufferView::TypeDataView;

  virtual ~DOMArrayBufferView() {}

  DOMArrayBuffer* buffer() const {
    DCHECK(!isShared());
    if (!m_domArrayBuffer)
      m_domArrayBuffer = DOMArrayBuffer::create(view()->buffer());

    return static_cast<DOMArrayBuffer*>(m_domArrayBuffer.get());
  }

  DOMSharedArrayBuffer* bufferShared() const {
    DCHECK(isShared());
    if (!m_domArrayBuffer)
      m_domArrayBuffer = DOMSharedArrayBuffer::create(view()->buffer());

    return static_cast<DOMSharedArrayBuffer*>(m_domArrayBuffer.get());
  }

  DOMArrayBufferBase* bufferBase() const {
    if (isShared())
      return bufferShared();

    return buffer();
  }

  const WTF::ArrayBufferView* view() const { return m_bufferView.get(); }
  WTF::ArrayBufferView* view() { return m_bufferView.get(); }

  ViewType type() const { return view()->type(); }
  const char* typeName() { return view()->typeName(); }
  void* baseAddress() const { return view()->baseAddress(); }
  unsigned byteOffset() const { return view()->byteOffset(); }
  unsigned byteLength() const { return view()->byteLength(); }
  unsigned typeSize() const { return view()->typeSize(); }
  void setNeuterable(bool flag) { return view()->setNeuterable(flag); }
  bool isShared() const { return view()->isShared(); }

  v8::Local<v8::Object> wrap(v8::Isolate*,
                             v8::Local<v8::Object> creationContext) override {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(m_domArrayBuffer); }

 protected:
  explicit DOMArrayBufferView(PassRefPtr<WTF::ArrayBufferView> bufferView)
      : m_bufferView(bufferView) {
    DCHECK(m_bufferView);
  }
  DOMArrayBufferView(PassRefPtr<WTF::ArrayBufferView> bufferView,
                     DOMArrayBufferBase* domArrayBuffer)
      : m_bufferView(bufferView), m_domArrayBuffer(domArrayBuffer) {
    DCHECK(m_bufferView);
    DCHECK(m_domArrayBuffer);
    DCHECK_EQ(m_domArrayBuffer->buffer(), m_bufferView->buffer());
  }

 private:
  RefPtr<WTF::ArrayBufferView> m_bufferView;
  mutable Member<DOMArrayBufferBase> m_domArrayBuffer;
};

}  // namespace blink

#endif  // DOMArrayBufferView_h
