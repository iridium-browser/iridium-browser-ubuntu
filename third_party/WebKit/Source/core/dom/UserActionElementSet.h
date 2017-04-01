/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UserActionElementSet_h
#define UserActionElementSet_h

#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"

namespace blink {

class Node;
class Element;

class UserActionElementSet final {
  DISALLOW_NEW();

 public:
  bool isFocused(const Node* node) { return hasFlags(node, IsFocusedFlag); }
  bool isActive(const Node* node) { return hasFlags(node, IsActiveFlag); }
  bool isInActiveChain(const Node* node) {
    return hasFlags(node, InActiveChainFlag);
  }
  bool isDragged(const Node* node) { return hasFlags(node, IsDraggedFlag); }
  bool isHovered(const Node* node) { return hasFlags(node, IsHoveredFlag); }
  void setFocused(Node* node, bool enable) {
    setFlags(node, enable, IsFocusedFlag);
  }
  void setActive(Node* node, bool enable) {
    setFlags(node, enable, IsActiveFlag);
  }
  void setInActiveChain(Node* node, bool enable) {
    setFlags(node, enable, InActiveChainFlag);
  }
  void setDragged(Node* node, bool enable) {
    setFlags(node, enable, IsDraggedFlag);
  }
  void setHovered(Node* node, bool enable) {
    setFlags(node, enable, IsHoveredFlag);
  }

  UserActionElementSet();
  ~UserActionElementSet();

  void didDetach(Element&);

  DECLARE_TRACE();

 private:
  enum ElementFlags {
    IsActiveFlag = 1,
    InActiveChainFlag = 1 << 1,
    IsHoveredFlag = 1 << 2,
    IsFocusedFlag = 1 << 3,
    IsDraggedFlag = 1 << 4,
  };

  void setFlags(Node* node, bool enable, unsigned flags) {
    enable ? setFlags(node, flags) : clearFlags(node, flags);
  }
  void setFlags(Node*, unsigned);
  void clearFlags(Node*, unsigned);
  bool hasFlags(const Node*, unsigned flags) const;

  void setFlags(Element*, unsigned);
  void clearFlags(Element*, unsigned);
  bool hasFlags(const Element*, unsigned flags) const;

  typedef HeapHashMap<Member<Element>, unsigned> ElementFlagMap;
  ElementFlagMap m_elements;
};

}  // namespace blink

#endif  // UserActionElementSet_h
