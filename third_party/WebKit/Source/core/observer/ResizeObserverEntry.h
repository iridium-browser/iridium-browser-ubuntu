// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeObserverEntry_h
#define ResizeObserverEntry_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;
class ClientRect;
class LayoutSize;

class ResizeObserverEntry final : public GarbageCollected<ResizeObserverEntry>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    explicit ResizeObserverEntry(Element* target);

    Element* target() const { return m_target; }
    // FIXME(atotic): should return DOMRectReadOnly once https://crbug.com/388780 lands
    ClientRect* contentRect() const { return m_contentRect; }
    LayoutSize contentSize() const;

    DECLARE_VIRTUAL_TRACE();

private:
    Member<Element> m_target;
    Member<ClientRect>  m_contentRect;
};

} // namespace blink

#endif // ResizeObserverEntry_h
