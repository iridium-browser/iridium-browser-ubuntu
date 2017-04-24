/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef DragController_h
#define DragController_h

#include "core/CoreExport.h"
#include "core/events/EventTarget.h"
#include "core/page/DragActions.h"
#include "platform/geometry/IntPoint.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Forward.h"

namespace blink {

class DataTransfer;
class Document;
class DragData;
class DragImage;
struct DragSession;
class DragState;
class LocalFrame;
class FrameSelection;
class HTMLInputElement;
class Node;
class Page;
class WebMouseEvent;

class CORE_EXPORT DragController final
    : public GarbageCollected<DragController> {
  WTF_MAKE_NONCOPYABLE(DragController);

 public:
  static DragController* create(Page*);

  DragSession dragEnteredOrUpdated(DragData*, LocalFrame& localRoot);
  void dragExited(DragData*, LocalFrame& localRoot);
  bool performDrag(DragData*, LocalFrame& localRoot);

  enum SelectionDragPolicy {
    ImmediateSelectionDragResolution,
    DelayedSelectionDragResolution,
  };
  Node* draggableNode(const LocalFrame*,
                      Node*,
                      const IntPoint&,
                      SelectionDragPolicy,
                      DragSourceAction&) const;
  void dragEnded();

  bool populateDragDataTransfer(LocalFrame* src,
                                const DragState&,
                                const IntPoint& dragOrigin);
  bool startDrag(LocalFrame* src,
                 const DragState&,
                 const WebMouseEvent& dragEvent,
                 const IntPoint& dragOrigin);

  DragState& dragState();

  DECLARE_TRACE();

 private:
  explicit DragController(Page*);

  DispatchEventResult dispatchTextInputEventFor(LocalFrame*, DragData*);
  bool canProcessDrag(DragData*, LocalFrame& localRoot);
  bool concludeEditDrag(DragData*);
  DragOperation operationForLoad(DragData*, LocalFrame& localRoot);
  bool tryDocumentDrag(DragData*,
                       DragDestinationAction,
                       DragSession&,
                       LocalFrame& localRoot);
  bool tryDHTMLDrag(DragData*, DragOperation&, LocalFrame& localRoot);
  DragOperation dragOperation(DragData*);
  void cancelDrag();
  bool dragIsMove(FrameSelection&, DragData*);
  bool isCopyKeyDown(DragData*);

  void mouseMovedIntoDocument(Document*);

  void doSystemDrag(DragImage*,
                    const IntPoint& dragLocation,
                    const IntPoint& dragOrigin,
                    DataTransfer*,
                    LocalFrame*,
                    bool forLink);

  Member<Page> m_page;

  // The document the mouse was last dragged over.
  Member<Document> m_documentUnderMouse;
  // The Document (if any) that initiated the drag.
  Member<Document> m_dragInitiator;

  Member<DragState> m_dragState;

  Member<HTMLInputElement> m_fileInputElementUnderMouse;
  bool m_documentIsHandlingDrag;

  DragDestinationAction m_dragDestinationAction;
  bool m_didInitiateDrag;
};

}  // namespace blink

#endif  // DragController_h
