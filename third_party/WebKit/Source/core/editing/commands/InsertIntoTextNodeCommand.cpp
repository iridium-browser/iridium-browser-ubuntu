/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
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

#include "core/editing/commands/InsertIntoTextNodeCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutText.h"

namespace blink {

InsertIntoTextNodeCommand::InsertIntoTextNodeCommand(Text* node,
                                                     unsigned offset,
                                                     const String& text)
    : SimpleEditCommand(node->document()),
      m_node(node),
      m_offset(offset),
      m_text(text) {
  DCHECK(m_node);
  DCHECK_LE(m_offset, m_node->length());
  DCHECK(!m_text.isEmpty());
}

void InsertIntoTextNodeCommand::doApply(EditingState*) {
  bool passwordEchoEnabled =
      document().settings() && document().settings()->getPasswordEchoEnabled();
  if (passwordEchoEnabled)
    document().updateStyleAndLayoutIgnorePendingStylesheets();

  if (!hasEditableStyle(*m_node))
    return;

  if (passwordEchoEnabled) {
    LayoutText* layoutText = m_node->layoutObject();
    if (layoutText && layoutText->isSecure())
      layoutText->momentarilyRevealLastTypedCharacter(m_offset +
                                                      m_text.length() - 1);
  }

  m_node->insertData(m_offset, m_text, IGNORE_EXCEPTION_FOR_TESTING);
}

void InsertIntoTextNodeCommand::doUnapply() {
  if (!hasEditableStyle(*m_node))
    return;

  m_node->deleteData(m_offset, m_text.length(), IGNORE_EXCEPTION_FOR_TESTING);
}

DEFINE_TRACE(InsertIntoTextNodeCommand) {
  visitor->trace(m_node);
  SimpleEditCommand::trace(visitor);
}

}  // namespace blink
