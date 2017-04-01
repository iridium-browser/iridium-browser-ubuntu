/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DragCaretController_h
#define DragCaretController_h

#include "core/dom/SynchronousMutationObserver.h"
#include "core/editing/CaretBase.h"
#include <memory>

namespace blink {

class DragCaretController final
    : public GarbageCollectedFinalized<DragCaretController>,
      public SynchronousMutationObserver {
  WTF_MAKE_NONCOPYABLE(DragCaretController);
  USING_GARBAGE_COLLECTED_MIXIN(DragCaretController);

 public:
  static DragCaretController* create();

  virtual ~DragCaretController();

  void paintDragCaret(LocalFrame*, GraphicsContext&, const LayoutPoint&) const;

  bool hasCaretIn(const LayoutBlock&) const;
  bool isContentRichlyEditable() const;

  bool hasCaret() const { return m_position.isNotNull(); }
  const PositionWithAffinity& caretPosition() { return m_position; }
  void setCaretPosition(const PositionWithAffinity&);
  void clear() { setCaretPosition(PositionWithAffinity()); }

  DECLARE_TRACE();

 private:
  DragCaretController();

  // Implementations of |SynchronousMutationObserver|
  void nodeChildrenWillBeRemoved(ContainerNode&) final;
  void nodeWillBeRemoved(Node&) final;

  PositionWithAffinity m_position;
  // caret rect in coords local to the layoutObject responsible for painting the
  // caret
  LayoutRect m_caretLocalRect;
  const std::unique_ptr<CaretBase> m_caretBase;
};

}  // namespace blink

#endif  // DragCaretController_h
