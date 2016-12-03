/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "core/editing/commands/MoveSelectionCommand.h"

#include "core/dom/DocumentFragment.h"
#include "core/editing/commands/ReplaceSelectionCommand.h"

namespace blink {

MoveSelectionCommand::MoveSelectionCommand(DocumentFragment* fragment, const Position& position, bool smartInsert, bool smartDelete)
    : CompositeEditCommand(*position.document()), m_fragment(fragment), m_position(position), m_smartInsert(smartInsert), m_smartDelete(smartDelete)
{
    DCHECK(m_fragment);
}

void MoveSelectionCommand::doApply(EditingState* editingState)
{
    DCHECK(endingSelection().isNonOrphanedRange());

    Position pos = m_position;
    if (pos.isNull())
        return;

    // Update the position otherwise it may become invalid after the selection is deleted.
    Position selectionEnd = endingSelection().end();
    if (pos.isOffsetInAnchor() && selectionEnd.isOffsetInAnchor()
        && selectionEnd.computeContainerNode() == pos.computeContainerNode() && selectionEnd.offsetInContainerNode() < pos.offsetInContainerNode()) {
        pos = Position(pos.computeContainerNode(), pos.offsetInContainerNode() - selectionEnd.offsetInContainerNode());

        Position selectionStart = endingSelection().start();
        if (selectionStart.isOffsetInAnchor() && selectionStart.computeContainerNode() == pos.computeContainerNode())
            pos = Position(pos.computeContainerNode(), pos.offsetInContainerNode() + selectionStart.offsetInContainerNode());
    }

    deleteSelection(editingState, m_smartDelete);
    if (editingState->isAborted())
        return;

    // If the node for the destination has been removed as a result of the deletion,
    // set the destination to the ending point after the deletion.
    // Fixes: <rdar://problem/3910425> REGRESSION (Mail): Crash in ReplaceSelectionCommand;
    //        selection is empty, leading to null deref
    if (!pos.isConnected())
        pos = endingSelection().start();

    cleanupAfterDeletion(editingState, createVisiblePosition(pos));
    if (editingState->isAborted())
        return;

    setEndingSelection(VisibleSelection(pos, endingSelection().affinity(), endingSelection().isDirectional()));
    if (!pos.isConnected()) {
        // Document was modified out from under us.
        return;
    }
    ReplaceSelectionCommand::CommandOptions options = ReplaceSelectionCommand::SelectReplacement | ReplaceSelectionCommand::PreventNesting;
    if (m_smartInsert)
        options |= ReplaceSelectionCommand::SmartReplace;
    applyCommandToComposite(ReplaceSelectionCommand::create(document(), m_fragment, options), editingState);
}

InputEvent::InputType MoveSelectionCommand::inputType() const
{
    return InputEvent::InputType::Drag;
}

DEFINE_TRACE(MoveSelectionCommand)
{
    visitor->trace(m_fragment);
    visitor->trace(m_position);
    CompositeEditCommand::trace(visitor);
}

} // namespace blink
