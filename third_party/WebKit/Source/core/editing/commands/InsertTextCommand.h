/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef InsertTextCommand_h
#define InsertTextCommand_h

#include "core/editing/commands/CompositeEditCommand.h"

namespace blink {

class InsertTextCommand : public CompositeEditCommand {
 public:
  enum RebalanceType {
    RebalanceLeadingAndTrailingWhitespaces,
    RebalanceAllWhitespaces
  };

  static InsertTextCommand* create(
      Document& document,
      const String& text,
      bool selectInsertedText = false,
      RebalanceType rebalanceType = RebalanceLeadingAndTrailingWhitespaces) {
    return new InsertTextCommand(document, text, selectInsertedText,
                                 rebalanceType);
  }

  String textDataForInputEvent() const final;

 protected:
  InsertTextCommand(Document&,
                    const String& text,
                    bool selectInsertedText,
                    RebalanceType);

  void doApply(EditingState*) override;

  Position positionInsideTextNode(const Position&, EditingState*);
  Position insertTab(const Position&, EditingState*);

  bool performTrivialReplace(const String&, bool selectInsertedText);
  bool performOverwrite(const String&, bool selectInsertedText);
  void setEndingSelectionWithoutValidation(const Position& startPosition,
                                           const Position& endPosition);

  friend class TypingCommand;

  String m_text;
  bool m_selectInsertedText;
  RebalanceType m_rebalanceType;
};

}  // namespace blink

#endif  // InsertTextCommand_h
