/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "bookmaker.h"

void IncludeWriter::constOut(const Definition* memberStart, const Definition& child,
    const Definition* bmhConst) {
    const char* bodyEnd = fDeferComment ? fDeferComment->fContentStart - 1 :
        memberStart->fContentStart;
    this->writeBlockTrim((int) (bodyEnd - fStart), fStart);  // may write nothing
    this->lf(2);
    this->writeCommentHeader();
    fIndent += 4;
    this->descriptionOut(bmhConst, SkipFirstLine::kYes, Phrase::kNo);
    fIndent -= 4;
    this->writeCommentTrailer();
    fStart = memberStart->fContentStart;
}

void IncludeWriter::descriptionOut(const Definition* def, SkipFirstLine skipFirstLine,
            Phrase phrase) {
    const char* commentStart = def->fContentStart;
    if (SkipFirstLine::kYes == skipFirstLine) {
        TextParser parser(def);
        SkAssertResult(parser.skipLine());
        commentStart = parser.fChar;
    }
    int commentLen = (int) (def->fContentEnd - commentStart);
    bool breakOut = false;
    SkDEBUGCODE(bool wroteCode = false);
    if (def->fDeprecated) {
        this->writeString(def->fToBeDeprecated ? "To be deprecated soon." : "Deprecated.");
        this->lfcr();
    }
    for (auto prop : def->fChildren) {
        switch (prop->fMarkType) {
            case MarkType::kCode: {
                bool literal = false;
                bool literalOutdent = false;
                commentLen = (int) (prop->fStart - commentStart);
                if (commentLen > 0) {
                    SkASSERT(commentLen < 1000);
                    if (Wrote::kNone != this->rewriteBlock(commentLen, commentStart, Phrase::kNo)) {
                        this->lf(2);
                    }
                }
                size_t childSize = prop->fChildren.size();
                if (childSize) {
                    SkASSERT(1 == childSize || 2 == childSize);  // incomplete
                    SkASSERT(MarkType::kLiteral == prop->fChildren[0]->fMarkType);
                    SkASSERT(1 == childSize || MarkType::kOutdent == prop->fChildren[1]->fMarkType);
                    commentStart = prop->fChildren[childSize - 1]->fContentStart;
                    literal = true;
                    literalOutdent = 2 == childSize &&
                            MarkType::kOutdent == prop->fChildren[1]->fMarkType;
                }
                commentLen = (int) (prop->fContentEnd - commentStart);
                SkASSERT(commentLen > 0);
                if (literal) {
                    if (!literalOutdent) {
                        fIndent += 4;
                    }
                    this->writeBlockIndent(commentLen, commentStart);
                    this->lf(2);
                    if (!literalOutdent) {
                        fIndent -= 4;
                    }
                    commentStart = prop->fTerminator;
                    SkDEBUGCODE(wroteCode = true);
                }
                } break;
            case MarkType::kDefinedBy:
                commentStart = prop->fTerminator;
                break;
            case MarkType::kBug: {
                string bugstr("(see skbug.com/" + string(prop->fContentStart,
                    prop->fContentEnd - prop->fContentStart) + ')');
                this->writeString(bugstr);
                this->lfcr();
            }
            case MarkType::kDeprecated:
            case MarkType::kPrivate:
                commentLen = (int) (prop->fStart - commentStart);
                if (commentLen > 0) {
                    SkASSERT(commentLen < 1000);
                    if (Wrote::kNone != this->rewriteBlock(commentLen, commentStart, Phrase::kNo)) {
                        this->lfcr();
                    }
                }
                commentStart = prop->fContentStart;
                if (def->fToBeDeprecated) {
                    commentStart += 4; // skip over "soon" // FIXME: this is awkward
                } else if (MarkType::kBug == prop->fMarkType) {
                    commentStart = prop->fContentEnd;
                }
                commentLen = (int) (prop->fContentEnd - commentStart);
                if (commentLen > 0) {
                    this->writeBlockIndent(commentLen, commentStart);
                    const char* end = commentStart + commentLen;
                    while (end > commentStart && ' ' == end[-1]) {
                        --end;
                    }
                    if (end > commentStart && '\n' == end[-1]) {
                        this->lfcr();
                    }
                }
                commentStart = prop->fTerminator;
                commentLen = (int) (def->fContentEnd - commentStart);
                break;
            case MarkType::kExperimental:
                this->writeString("EXPERIMENTAL:");
                this->writeSpace();
                commentStart = prop->fContentStart;
                commentLen = (int) (prop->fContentEnd - commentStart);
                if (commentLen > 0) {
                    if (Wrote::kNone != this->rewriteBlock(commentLen, commentStart, Phrase::kNo)) {
                        this->lfcr();
                    }
                }
                commentStart = prop->fTerminator;
                commentLen = (int) (def->fContentEnd - commentStart);
                break;
            case MarkType::kFormula: {
                commentLen = prop->fStart - commentStart;
                if (commentLen > 0) {
                    if (Wrote::kNone != this->rewriteBlock(commentLen, commentStart, Phrase::kNo)) {
                        if (commentLen > 1 && '\n' == prop->fStart[-1] &&
                                '\n' == prop->fStart[-2]) {
                            this->lf(1);
                        } else {
                            this->writeSpace();
                        }
                    }
                }
                int saveIndent = fIndent;
                if (fIndent < fColumn + 1) {
                    fIndent = fColumn + 1;
                }
                this->writeBlockIndent(prop->length(), prop->fContentStart);
                fIndent = saveIndent;
                commentStart = prop->fTerminator;
                commentLen = (int) (def->fContentEnd - commentStart);
                if (commentLen > 1 && '\n' == commentStart[0] && '\n' == commentStart[1]) {
                    this->lf(2);
                } else {
                    SkASSERT('\n' == prop->fTerminator[0]);
                    if ('.' != prop->fTerminator[1] && !fLinefeeds) {
                        this->writeSpace();
                    }
                }
                } break;
            case MarkType::kIn:
            case MarkType::kLine:
            case MarkType::kToDo:
                commentLen = (int) (prop->fStart - commentStart);
                if (commentLen > 0) {
                    SkASSERT(commentLen < 1000);
                    if (Wrote::kNone != this->rewriteBlock(commentLen, commentStart, Phrase::kNo)) {
                        this->lfcr();
                    }
                }
                commentStart = prop->fTerminator;
                commentLen = (int) (def->fContentEnd - commentStart);
                break;
            case MarkType::kList:
                commentLen = prop->fStart - commentStart;
                if (commentLen > 0) {
                    if (Wrote::kNone != this->rewriteBlock(commentLen, commentStart,
                            Phrase::kNo)) {
                        this->lfcr();
                    }
                }
                for (auto row : prop->fChildren) {
                    SkASSERT(MarkType::kRow == row->fMarkType);
                    for (auto column : row->fChildren) {
                        SkASSERT(MarkType::kColumn == column->fMarkType);
                        this->writeString("-");
                        this->writeSpace();
                        this->descriptionOut(column, SkipFirstLine::kNo, Phrase::kNo);
                        this->lf(1);
                    }
                }
                commentStart = prop->fTerminator;
                commentLen = (int) (def->fContentEnd - commentStart);
                if ('\n' == commentStart[0] && '\n' == commentStart[1]) {
                    this->lf(2);
                }
                break;
            case MarkType::kPhraseRef: {
                commentLen = prop->fStart - commentStart;
                if (commentLen > 0) {
                    this->rewriteBlock(commentLen, commentStart, Phrase::kNo);
                    // ince we don't do line wrapping, always insert LF before phrase
                    this->lfcr();   // TODO: remove this once rewriteBlock rewraps paragraphs
                }
                auto iter = fBmhParser->fPhraseMap.find(prop->fName);
                if (fBmhParser->fPhraseMap.end() == iter) {
                    return this->reportError<void>("missing phrase definition");
                }
                Definition* phraseDef = iter->second;
                this->rewriteBlock(phraseDef->length(), phraseDef->fContentStart, Phrase::kYes);
                commentStart = prop->fContentStart;
                commentLen = (int) (def->fContentEnd - commentStart);
                } break;
            default:
                commentLen = (int) (prop->fStart - commentStart);
                breakOut = true;
        }
        if (breakOut) {
            break;
        }
    }
    SkASSERT(wroteCode || (commentLen > 0 && commentLen < 1500) || def->fDeprecated);
    if (commentLen > 0) {
        this->rewriteBlock(commentLen, commentStart, phrase);
    }
}

void IncludeWriter::enumHeaderOut(const RootDefinition* root,
        const Definition& child) {
    const Definition* enumDef = nullptr;
    const char* bodyEnd = fDeferComment ? fDeferComment->fContentStart - 1 :
            child.fContentStart;
    this->writeBlockTrim((int) (bodyEnd - fStart), fStart);  // may write nothing
    this->lf(2);
    if (fIndentNext) {
        fIndent += 4;
        fIndentNext = false;
    }
    fDeferComment = nullptr;
    fStart = child.fContentStart;
    const auto& nameDef = child.fTokens.front();
    string fullName;
    if (nullptr != nameDef.fContentEnd) {
        TextParser enumClassCheck(&nameDef);
        const char* start = enumClassCheck.fStart;
        size_t len = (size_t) (enumClassCheck.fEnd - start);
        bool enumClass = enumClassCheck.skipExact("class ");
        if (enumClass) {
            start = enumClassCheck.fChar;
            const char* end = enumClassCheck.anyOf(" \n;{");
            len = (size_t) (end - start);
        }
        string enumName(start, len);
        if (enumClass) {
            child.fChildren[0]->fName = enumName;
        }
        fullName = root->fName + "::" + enumName;
        enumDef = root->find(enumName, RootDefinition::AllowParens::kNo);
        if (!enumDef) {
            enumDef = root->find(fullName, RootDefinition::AllowParens::kNo);
        }
        if (!enumDef) {
            auto mapEntry = fBmhParser->fEnumMap.find(enumName);
            if (fBmhParser->fEnumMap.end() != mapEntry) {
                enumDef = &mapEntry->second;
            }
        }
        SkASSERT(enumDef);
        // child[0] should be #Code comment starts at child[0].fTerminator
            // though skip until #Code is found (in case there's a #ToDo, etc)
        // child[1] should be #Const comment ends at child[1].fStart
        // comment becomes enum header (if any)
    } else {
        string enumName(root->fName);
        enumName += "::_anonymous";
        if (fAnonymousEnumCount > 1) {
            enumName += '_' + to_string(fAnonymousEnumCount);
        }
        enumDef = root->find(enumName, RootDefinition::AllowParens::kNo);
        SkASSERT(enumDef);
        ++fAnonymousEnumCount;
    }
    Definition* codeBlock = nullptr;
    const char* commentStart = nullptr;
    bool wroteHeader = false;
    bool lastAnchor = false;
    SkDEBUGCODE(bool foundConst = false);
    for (auto test : enumDef->fChildren) {
        if (MarkType::kCode == test->fMarkType) {
            SkASSERT(!codeBlock);  // FIXME: check enum for correct order earlier
            codeBlock = test;
            commentStart = codeBlock->fTerminator;
            continue;
        }
        if (!codeBlock) {
            continue;
        }
        const char* commentEnd = test->fStart;
        if (!wroteHeader &&
                !this->contentFree((int) (commentEnd - commentStart), commentStart)) {
            if (fIndentNext) {
                fIndent += 4;
            }
            this->writeCommentHeader();
            this->writeString("\\enum");
            if (fullName.length() > 0) {
                this->writeSpace();
                this->writeString(fullName.c_str());
            }
            fIndent += 4;
            this->lfcr();
            wroteHeader = true;
        }
        if (lastAnchor) {
            if (commentEnd - commentStart > 1) {
                SkASSERT('\n' == commentStart[0]);
                if (' ' == commentStart[1]) {
                    this->writeSpace();
                }
            }
            lastAnchor = false;
        }
        this->rewriteBlock((int) (commentEnd - commentStart), commentStart, Phrase::kNo);
        if (MarkType::kAnchor == test->fMarkType) {
            bool newLine = commentEnd - commentStart > 1 &&
                '\n' == commentEnd[-1] && '\n' == commentEnd[-2];
            commentStart = test->fContentStart;
            commentEnd = test->fChildren[0]->fStart;
            if (newLine) {
                this->lf(2);
            } else {
                this->writeSpace();
            }
            this->rewriteBlock((int) (commentEnd - commentStart), commentStart, Phrase::kNo);
            lastAnchor = true;   // this->writeSpace();
        }
        commentStart = test->fTerminator;
        if (MarkType::kConst == test->fMarkType) {
            SkASSERT(codeBlock);  // FIXME: check enum for correct order earlier
            SkDEBUGCODE(foundConst = true);
            break;
        }
    }
    SkASSERT(codeBlock);
    SkASSERT(foundConst);
    if (wroteHeader) {
        fIndent -= 4;
        this->lfcr();
        this->writeCommentTrailer();
    }
    Definition* braceHolder = child.fChildren[0];
    if (KeyWord::kClass == braceHolder->fKeyWord) {
        braceHolder = braceHolder->fChildren[0];
    }
    bodyEnd = braceHolder->fContentStart;
    SkASSERT('{' == bodyEnd[0]);
    ++bodyEnd;
    this->lfcr();
    this->writeBlock((int) (bodyEnd - fStart), fStart); // write include "enum Name {"
    fIndent += 4;
    this->singleLF();
    fStart = bodyEnd;
    fEnumDef = enumDef;
}

void IncludeWriter::enumMembersOut(const RootDefinition* root, Definition& child) {
    // iterate through include tokens and find how much remains for 1 line comments
    // put ones that fit on same line, ones that are too big on preceding line?
    const Definition* currentEnumItem = nullptr;
    const char* commentStart = nullptr;
    const char* lastEnd = nullptr;
    int commentLen = 0;
    enum class State {
        kNoItem,
        kItemName,
        kItemValue,
        kItemComment,
    };
    State state = State::kNoItem;
    vector<IterState> iterStack;
    iterStack.emplace_back(child.fTokens.begin(), child.fTokens.end());
    IterState* iterState = &iterStack[0];
    Preprocessor preprocessor;
    for (int onePast = 0; onePast < 2; onePast += iterState->fDefIter == iterState->fDefEnd) {
        Definition* token = onePast ? nullptr : &*iterState->fDefIter++;
        if (this->enumPreprocessor(token, MemberPass::kOut, iterStack, &iterState,
                &preprocessor)) {
            continue;
        }
        if (token && State::kItemName == state) {
            TextParser enumLine(token->fFileName, lastEnd,
                    token->fContentStart, token->fLineCount);
            const char* end = enumLine.anyOf(",}=");
            SkASSERT(end);
            state = '=' == *end ? State::kItemValue : State::kItemComment;
            if (State::kItemValue == state) {  // write enum value
                this->indentToColumn(fEnumItemValueTab);
                this->writeString("=");
                this->writeSpace();
                lastEnd = token->fContentEnd;
                this->writeBlock((int) (lastEnd - token->fContentStart),
                        token->fContentStart); // write const value if any
                continue;
            }
        }
        if (token && State::kItemValue == state) {
            TextParser valueEnd(token->fFileName, lastEnd,
                    token->fContentStart, token->fLineCount);
            const char* end = valueEnd.anyOf(",}");
            if (!end) {  // write expression continuation
                if (' ' == lastEnd[0]) {
                    this->writeSpace();
                }
                this->writeBlock((int) (token->fContentEnd - lastEnd), lastEnd);
                continue;
            }
        }
        if (State::kNoItem != state) {
            this->writeString(",");
            SkASSERT(currentEnumItem);
            if (currentEnumItem->fShort) {
                this->indentToColumn(fEnumItemCommentTab);
                if (commentLen || currentEnumItem->fDeprecated) {
                    this->writeString("//!<");
                    this->writeSpace();
                    if (currentEnumItem->fDeprecated) {
                        this->writeString(child.fToBeDeprecated ? "to be deprecated soon"
                                : "deprecated");
                    } else {
                        this->rewriteBlock(commentLen, commentStart, Phrase::kNo);
                    }
                }
            }
            if (onePast) {
                fIndent -= 4;
            }
            this->lfcr();
            if (preprocessor.fStart) {
                SkASSERT(preprocessor.fEnd);
                int saveIndent = fIndent;
                fIndent = SkTMax(0, fIndent - 8);
                this->lf(2);
                this->writeBlock(
                        (int) (preprocessor.fEnd - preprocessor.fStart), preprocessor.fStart);
                this->lfcr();
                fIndent = saveIndent;
                preprocessor.reset();
            }
            if (token && State::kItemValue == state) {
                fStart = token->fContentStart;
            }
            state = State::kNoItem;
        }
        SkASSERT(State::kNoItem == state);
        if (onePast) {
            break;
        }
        SkASSERT(token);
        string itemName;
        if (!fEnumDef->isRoot()) {
            itemName = root->fName + "::";
            if (KeyWord::kClass == child.fParent->fKeyWord) {
                itemName += child.fParent->fName + "::";
            }
        }
        itemName += string(token->fContentStart, (int) (token->fContentEnd - token->fContentStart));
        for (auto& enumItem : fEnumDef->fChildren) {
            if (MarkType::kConst != enumItem->fMarkType) {
                continue;
            }
            if (itemName != enumItem->fName) {
                continue;
            }
            currentEnumItem = enumItem;
            break;
        }
        SkASSERT(currentEnumItem);
        // if description fits, it goes after item
        commentStart = currentEnumItem->fContentStart;
        const char* commentEnd;
        if (currentEnumItem->fChildren.size() > 0) {
            commentEnd = currentEnumItem->fChildren[0]->fStart;
        } else {
            commentEnd = currentEnumItem->fContentEnd;
        }
        TextParser enumComment(fFileName, commentStart, commentEnd, currentEnumItem->fLineCount);
        bool isDeprecated = false;
        if (enumComment.skipToLineStart()) {  // skip const value
            commentStart = enumComment.fChar;
            commentLen = (int) (commentEnd - commentStart);
        } else {
            const Definition* childDef = currentEnumItem->fChildren[0];
            isDeprecated = MarkType::kDeprecated == childDef->fMarkType;
            if (MarkType::kPrivate == childDef->fMarkType || isDeprecated) {
                commentStart = childDef->fContentStart;
                if (currentEnumItem->fToBeDeprecated) {
                    SkASSERT(isDeprecated);
                    commentStart += 4; // skip over "soon" // FIXME: this is awkward
                }
                commentLen = (int) (childDef->fContentEnd - commentStart);
            }
        }
        // FIXME: may assert here if there's no const value
        // should have detected and errored on that earlier when enum fContentStart was set
        SkASSERT((commentLen > 0 && commentLen < 1000) || isDeprecated);
        if (!currentEnumItem->fShort) {
            this->writeCommentHeader();
            fIndent += 4;
            bool wroteLineFeed = false;
            if (isDeprecated) {
                this->writeString(currentEnumItem->fToBeDeprecated
                        ? "To be deprecated soon." : "Deprecated.");
            }
            TextParserSave save(this);
            this->setForErrorReporting(currentEnumItem, commentStart);
            wroteLineFeed  = Wrote::kLF ==
                this->rewriteBlock(commentLen, commentStart, Phrase::kNo);
            save.restore();
            fIndent -= 4;
            if (wroteLineFeed || fColumn > 100 - 3 /* space * / */ ) {
                this->lfcr();
            } else {
                this->writeSpace();
            }
            this->writeCommentTrailer();
        }
        lastEnd = token->fContentEnd;
        this->lfcr();
        if (',' == fStart[0]) {
            ++fStart;
        }
        this->writeBlock((int) (lastEnd - fStart), fStart);  // enum item name
        fStart = token->fContentEnd;
        state = State::kItemName;
    }
}

bool IncludeWriter::enumPreprocessor(Definition* token, MemberPass pass,
        vector<IterState>& iterStack, IterState** iterState, Preprocessor* preprocessor) {
    if (token && Definition::Type::kBracket == token->fType) {
        if (Bracket::kSlashSlash == token->fBracket) {
            if (MemberPass::kOut == pass) {
                fStart = token->fContentEnd;
            }
            return true;  // ignore old inline comments
        }
        if (Bracket::kSlashStar == token->fBracket) {
            if (MemberPass::kOut == pass) {
                fStart = token->fContentEnd + 1;
            }
            return true;  // ignore old inline comments
        }
        if (Bracket::kPound == token->fBracket) {  // preprocessor wraps member
            preprocessor->fDefinition = token;
            preprocessor->fStart = token->fContentStart;
            if (KeyWord::kIf == token->fKeyWord || KeyWord::kIfdef == token->fKeyWord) {
                iterStack.emplace_back(token->fTokens.begin(), token->fTokens.end());
                *iterState = &iterStack.back();
                preprocessor->fWord = true;
            } else if (KeyWord::kEndif == token->fKeyWord || KeyWord::kElif == token->fKeyWord
                    || KeyWord::kElse == token->fKeyWord) {
                iterStack.pop_back();
                *iterState = &iterStack.back();
                preprocessor->fEnd = token->fContentEnd;
                if (KeyWord::kElif == token->fKeyWord) {
                    iterStack.emplace_back(token->fTokens.begin(), token->fTokens.end());
                    *iterState = &iterStack.back();
                    preprocessor->fWord = true;
                }
            } else {
                SkASSERT(0); // incomplete
            }
            return true;
        }
        if (preprocessor->fDefinition) {
            if (Bracket::kParen == token->fBracket) {
                preprocessor->fEnd = token->fContentEnd;
                SkASSERT(')' == *preprocessor->fEnd);
                ++preprocessor->fEnd;
                return true;
            }
            SkASSERT(0);  // incomplete
        }
        return true;
    }
    if (token && Definition::Type::kWord != token->fType) {
        SkASSERT(0); // incomplete
    }
    if (preprocessor->fWord) {
        preprocessor->fWord = false;
        preprocessor->fEnd = token->fContentEnd;
        return true;
    }
    return false;
}

void IncludeWriter::enumSizeItems(const Definition& child) {
    enum class State {
        kNoItem,
        kItemName,
        kItemValue,
        kItemComment,
    };
    State state = State::kNoItem;
    int longestName = 0;
    int longestValue = 0;
    int valueLen = 0;
    const char* lastEnd = nullptr;
//    SkASSERT(child.fChildren.size() == 1 || child.fChildren.size() == 2);
    auto brace = child.fChildren[0];
    if (KeyWord::kClass == brace->fKeyWord) {
        brace = brace->fChildren[0];
    }
    SkASSERT(Bracket::kBrace == brace->fBracket);
    vector<IterState> iterStack;
    iterStack.emplace_back(brace->fTokens.begin(), brace->fTokens.end());
    IterState* iterState = &iterStack[0];
    Preprocessor preprocessor;
    while (iterState->fDefIter != iterState->fDefEnd) {
        auto& token = *iterState->fDefIter++;
        if (this->enumPreprocessor(&token, MemberPass::kCount, iterStack, &iterState,
                &preprocessor)) {
            continue;
        }
        if (State::kItemName == state) {
            TextParser enumLine(token.fFileName, lastEnd,
                    token.fContentStart, token.fLineCount);
            const char* end = enumLine.anyOf(",}=");
            SkASSERT(end);
            state = '=' == *end ? State::kItemValue : State::kItemComment;
            if (State::kItemValue == state) {
                valueLen = (int) (token.fContentEnd - token.fContentStart);
                lastEnd = token.fContentEnd;
                continue;
            }
        }
        if (State::kItemValue == state) {
            TextParser valueEnd(token.fFileName, lastEnd,
                    token.fContentStart, token.fLineCount);
            const char* end = valueEnd.anyOf(",}");
            if (!end) {  // write expression continuation
                valueLen += (int) (token.fContentEnd - lastEnd);
                continue;
            }
        }
        if (State::kNoItem != state) {
            longestValue = SkTMax(longestValue, valueLen);
            state = State::kNoItem;
        }
        SkASSERT(State::kNoItem == state);
        lastEnd = token.fContentEnd;
        longestName = SkTMax(longestName, (int) (lastEnd - token.fContentStart));
        state = State::kItemName;
    }
    if (State::kItemValue == state) {
        longestValue = SkTMax(longestValue, valueLen);
    }
    fEnumItemValueTab = longestName + fIndent + 1 /* space before = */ ;
    if (longestValue) {
        longestValue += 3; /* = space , */
    }
    fEnumItemCommentTab = fEnumItemValueTab + longestValue + 1 /* space before //!< */ ;
    // iterate through bmh children and see which comments fit on include lines
    for (auto& enumItem : fEnumDef->fChildren) {
        if (MarkType::kConst != enumItem->fMarkType) {
            continue;
        }
        TextParser enumLine(enumItem);
        enumLine.trimEnd();
        enumLine.skipToLineStart(); // skip const value
        const char* commentStart = enumLine.fChar;
        enumLine.skipLine();
        ptrdiff_t lineLen = enumLine.fChar - commentStart + 5 /* //!< space */ ;
        if (!enumLine.eof()) {
            enumLine.skipWhiteSpace();
        }
        enumItem->fShort = enumLine.eof() && fEnumItemCommentTab + lineLen < 100;
    }
}

// walk children and output complete method doxygen description
void IncludeWriter::methodOut(const Definition* method, const Definition& child) {
    if (fPendingMethod) {
        fIndent -= 4;
        fPendingMethod = false;
    }
    fBmhMethod = method;
    fMethodDef = &child;
    fContinuation = nullptr;
    fDeferComment = nullptr;
    const Definition* csParent = method->csParent();
    if (!csParent) {
        SkDebugf("");
    }
    if (csParent && (0 == fIndent || fIndentNext)) {
        fIndent += 4;
        fIndentNext = false;
    }
    this->writeCommentHeader();
    fIndent += 4;
    this->descriptionOut(method, SkipFirstLine::kNo, Phrase::kNo);
    // compute indention column
    size_t column = 0;
    bool hasParmReturn = false;
    for (auto methodPart : method->fChildren) {
        if (MarkType::kParam == methodPart->fMarkType) {
            column = SkTMax(column, methodPart->fName.length());
            hasParmReturn = true;
        } else if (MarkType::kReturn == methodPart->fMarkType) {
            hasParmReturn = true;
        }
    }
    if (hasParmReturn) {
        this->lf(2);
        column += fIndent + sizeof("@return ");
        int saveIndent = fIndent;
        for (auto methodPart : method->fChildren) {
            if (MarkType::kParam == methodPart->fMarkType) {
                this->writeString("@param");
                this->writeSpace();
                this->writeString(methodPart->fName.c_str());
            } else if (MarkType::kReturn == methodPart->fMarkType) {
                this->writeString("@return");
            } else {
                continue;
            }
            this->indentToColumn(column);
            fIndent = column;
#if 0
            const char* partStart = methodPart->fContentStart;
            const char* partEnd = methodPart->fContentEnd;
            while ('\n' == partEnd[-1]) {
                --partEnd;
            }
            while ('#' == partEnd[-1]) { // FIXME: so wrong; should not be before fContentEnd
                --partEnd;
            }
            int partLen = (int) (partEnd - partStart);
            // FIXME : detect this earlier; assert if #Return is empty
            SkASSERT(partLen > 0 && partLen < 300);  // may assert if param desc is especially long
            this->rewriteBlock(partLen, partStart, Phrase::kYes);
#else
            this->descriptionOut(methodPart, SkipFirstLine::kNo, Phrase::kYes);
#endif
            fIndent = saveIndent;
            this->lfcr();
        }
    } else {
        this->lfcr();
    }
    fIndent -= 4;
    this->lfcr();
    this->writeCommentTrailer();
    fBmhMethod = nullptr;
    fMethodDef = nullptr;
    fEnumDef = nullptr;
    fWroteMethod = true;
}

void IncludeWriter::structOut(const Definition* root, const Definition& child,
        const char* commentStart, const char* commentEnd) {
    this->writeCommentHeader();
    this->writeString("\\");
    SkASSERT(MarkType::kClass == child.fMarkType || MarkType::kStruct == child.fMarkType);
    this->writeString(MarkType::kClass == child.fMarkType ? "class" : "struct");
    this->writeSpace();
    this->writeString(child.fName.c_str());
    fIndent += 4;
    this->lfcr();
    if (child.fDeprecated) {
        this->writeString(child.fToBeDeprecated ? "to be deprecated soon" : "deprecated");
    } else {
        this->rewriteBlock((int)(commentEnd - commentStart), commentStart, Phrase::kNo);
    }
    fIndent -= 4;
    this->lfcr();
    this->writeCommentTrailer();
}

bool IncludeWriter::findEnumSubtopic(string undername, const Definition** rootDefPtr) const {
    const Definition* subtopic = fEnumDef->fParent;
    string subcheck = subtopic->fFiddle + '_' + undername;
    auto iter = fBmhParser->fTopicMap.find(subcheck);
    if (iter == fBmhParser->fTopicMap.end()) {
        return false;
    }
    *rootDefPtr = iter->second;
    return true;
}

Definition* IncludeWriter::findMemberCommentBlock(const vector<Definition*>& bmhChildren,
        const string& name) const {
    for (auto memberDef : bmhChildren) {
        if (MarkType::kMember != memberDef->fMarkType) {
            continue;
        }
        string match = memberDef->fName;
        // if match.endsWith(name) ...
        if (match.length() >= name.length() &&
                0 == match.compare(match.length() - name.length(), name.length(), name)) {
            return memberDef;
        }
    }
    for (auto memberDef : bmhChildren) {
        if (MarkType::kSubtopic != memberDef->fMarkType && MarkType::kTopic != memberDef->fMarkType) {
            continue;
        }
        Definition* result = this->findMemberCommentBlock(memberDef->fChildren, name);
        if (result) {
            return result;
        }
    }
    return nullptr;
}

Definition* IncludeWriter::structMemberOut(const Definition* memberStart, const Definition& child) {
    const char* blockStart = !fWroteMethod && fDeferComment ? fLastComment->fContentEnd : fStart;
    const char* blockEnd = fWroteMethod && fDeferComment ? fDeferComment->fStart - 1 :
            memberStart->fStart;
    this->writeBlockTrim((int) (blockEnd - blockStart), blockStart);
    if (fIndentNext) {
        fIndent += 4;
        fIndentNext = false;
    }
    fWroteMethod = false;
    string name(child.fContentStart, (int) (child.fContentEnd - child.fContentStart));
    Definition* commentBlock = this->findMemberCommentBlock(fBmhStructDef->fChildren, name);
    if (!commentBlock) {
        return memberStart->reportError<Definition*>("member missing comment block");
    }
    if (!commentBlock->fShort) {
        const char* commentStart = commentBlock->fContentStart;
        ptrdiff_t commentLen = commentBlock->fContentEnd - commentStart;
        this->writeCommentHeader();
        bool wroteLineFeed = false;
        fIndent += 4;
        for (auto child : commentBlock->fChildren) {
            commentLen = child->fStart - commentStart;
            wroteLineFeed |= Wrote::kLF == this->rewriteBlock(commentLen, commentStart, Phrase::kNo);
            if (MarkType::kFormula == child->fMarkType) {
                this->writeSpace();
                this->writeBlock((int) (child->fContentEnd - child->fContentStart),
                        child->fContentStart);
            }
            commentStart = child->fTerminator;
        }
        commentLen = commentBlock->fContentEnd - commentStart;
        wroteLineFeed |= Wrote::kLF == this->rewriteBlock(commentLen, commentStart, Phrase::kNo);
        fIndent -= 4;
        if (wroteLineFeed || fColumn > 100 - 3 /* space * / */ ) {
            this->lfcr();
        } else {
            this->writeSpace();
        }
        this->writeCommentTrailer();
    }
    this->lfcr();
    this->writeBlock((int) (child.fStart - memberStart->fContentStart),
            memberStart->fContentStart);
    this->indentToColumn(fStructMemberTab);
    this->writeString(name.c_str());
    auto tokenIter = child.fParent->fTokens.begin();
    std::advance(tokenIter, child.fParentIndex + 1);
    Definition* valueStart = &*tokenIter;
    while (Definition::Type::kPunctuation != tokenIter->fType) {
        std::advance(tokenIter, 1);
        SkASSERT(child.fParent->fTokens.end() != tokenIter);
    }
    Definition* valueEnd = &*tokenIter;
    if (valueStart != valueEnd) {
        this->indentToColumn(fStructValueTab);
        this->writeString("=");
        this->writeSpace();
        this->writeBlock((int) (valueEnd->fStart - valueStart->fContentStart),
                valueStart->fContentStart);
    }
    this->writeString(";");
    if (commentBlock->fShort) {
        this->indentToColumn(fStructCommentTab);
        this->writeString("//!<");
        this->writeSpace();
        string extract = fBmhParser->extractText(commentBlock, BmhParser::TrimExtract::kYes);
        this->rewriteBlock(extract.length(), &extract.front(), Phrase::kNo);
    }
    this->lf(2);
    return valueEnd;
}

// iterate through bmh children and see which comments fit on include lines
void IncludeWriter::structSetMembersShort(const vector<Definition*>& bmhChildren) {
    for (auto memberDef : bmhChildren) {
        if (MarkType::kMember != memberDef->fMarkType) {
            continue;
        }
        string extract = fBmhParser->extractText(memberDef, BmhParser::TrimExtract::kYes);
        bool multiline = string::npos != extract.find('\n');
        if (multiline) {
            memberDef->fShort = false;
        } else {
            ptrdiff_t lineLen = extract.length() + 5 /* //!< space */ ;
            memberDef->fShort = fStructCommentTab + lineLen < 100;
        }
    }
    for (auto memberDef : bmhChildren) {
        if (MarkType::kSubtopic != memberDef->fMarkType && MarkType::kTopic != memberDef->fMarkType) {
            continue;
        }
        this->structSetMembersShort(memberDef->fChildren);
    }
}

void IncludeWriter::structSizeMembers(const Definition& child) {
    int longestType = 0;
    Definition* typeStart = nullptr;
    int longestName = 0;
    int longestValue = 0;
    SkASSERT(child.fChildren.size() == 1 || child.fChildren.size() == 2);
    bool inEnum = false;
    bool inMethod = false;
    bool inMember = false;
    auto brace = child.fChildren[0];
    SkASSERT(Bracket::kBrace == brace->fBracket);
    for (auto& token : brace->fTokens) {
        if (Definition::Type::kBracket == token.fType) {
            if (Bracket::kSlashSlash == token.fBracket) {
                continue;  // ignore old inline comments
            }
            if (Bracket::kSlashStar == token.fBracket) {
                continue;  // ignore old inline comments
            }
            if (Bracket::kParen == token.fBracket) {
                if (inMethod) {
                    continue;
                }
                break;
            }
            SkASSERT(0); // incomplete
        }
        if (Definition::Type::kKeyWord == token.fType) {
            switch (token.fKeyWord) {
                case KeyWord::kEnum:
                    inEnum = true;
                    break;
                case KeyWord::kConst:
                case KeyWord::kConstExpr:
                case KeyWord::kStatic:
                case KeyWord::kInt:
                case KeyWord::kUint8_t:
                case KeyWord::kUint16_t:
                case KeyWord::kUint32_t:
                case KeyWord::kUint64_t:
                case KeyWord::kSize_t:
                case KeyWord::kFloat:
                case KeyWord::kBool:
                case KeyWord::kVoid:
                    if (!typeStart) {
                        typeStart = &token;
                    }
                    break;
                default:
                    break;
            }
            continue;
        }
        if (Definition::Type::kPunctuation == token.fType) {
            if (inEnum) {
                SkASSERT(Punctuation::kSemicolon == token.fPunctuation);
                inEnum = false;
            }
            if (inMethod) {
                if (Punctuation::kColon == token.fPunctuation) {
                    inMethod = false;
                } else if (Punctuation::kLeftBrace == token.fPunctuation) {
                    inMethod = false;
                } else if (Punctuation::kSemicolon == token.fPunctuation) {
                    inMethod = false;
                } else {
                    SkASSERT(0);  // incomplete
                }
            }
            if (inMember) {
                SkASSERT(Punctuation::kSemicolon == token.fPunctuation);
                typeStart = nullptr;
                inMember = false;
            }
            continue;
        }
        if (Definition::Type::kWord != token.fType) {
            SkASSERT(0); // incomplete
        }
        if (MarkType::kMember == token.fMarkType) {
            TextParser typeStr(token.fFileName, typeStart->fContentStart, token.fContentStart,
                    token.fLineCount);
            typeStr.trimEnd();
            longestType = SkTMax(longestType, (int) (typeStr.fEnd - typeStr.fStart));
            longestName = SkTMax(longestName, (int) (token.fContentEnd - token.fContentStart));
            typeStart->fMemberStart = true;
            inMember = true;
            continue;
        }
        if (MarkType::kMethod == token.fMarkType) {
            inMethod = true;
            continue;
        }
        SkASSERT(MarkType::kNone == token.fMarkType);
        if (typeStart) {
            if (inMember) {
                longestValue =
                        SkTMax(longestValue, (int) (token.fContentEnd - token.fContentStart));
            }
        } else {
            typeStart = &token;
        }
    }
    fStructMemberTab = longestType + fIndent + 1 /* space before name */ ;
    fStructValueTab = fStructMemberTab + longestName + 2 /* space ; */ ;
    fStructCommentTab = fStructValueTab;
    if (longestValue) {
        fStructCommentTab += longestValue + 3 /* space = space */ ;
        fStructValueTab -= 1 /* ; */ ;
    }
    // iterate through bmh children and see which comments fit on include lines
    this->structSetMembersShort(fBmhStructDef->fChildren);
}

static bool find_start(const Definition* startDef, const char* start) {
    for (const auto& child : startDef->fTokens) {
        if (child.fContentStart == start) {
            return MarkType::kMethod == child.fMarkType;
        }
        if (child.fContentStart >= start) {
            break;
        }
        if (find_start(&child, start)) {
            return true;
        }
    }
    return false;
}

bool IncludeWriter::populate(Definition* def, ParentPair* prevPair, RootDefinition* root) {
    if (!def->fTokens.size()) {
        return true;
    }
    ParentPair pair = { def, prevPair };
    // write bulk of original include up to class, method, enum, etc., excepting preceding comment
    // find associated bmh object
    // write any associated comments in Doxygen form
    // skip include comment
    // if there is a series of same named methods, write one set of comments, then write all methods
    string methodName;
    const Definition* method = nullptr;
    const Definition* clonedMethod = nullptr;
    const Definition* memberStart = nullptr;
    const Definition* memberEnd = nullptr;
    fContinuation = nullptr;
    bool inStruct = false;
    bool inConstructor = false;
    bool inInline = false;
    bool eatOperator = false;
    bool sawConst = false;
    bool staticOnly = false;
    const Definition* requireDense = nullptr;
    const Definition* startDef = nullptr;
    for (auto& child : def->fTokens) {
        if (KeyWord::kOperator == child.fKeyWord && method &&
                Definition::MethodType::kOperator == method->fMethodType) {
            eatOperator = true;
            continue;
        }
        if (eatOperator) {
            if (Bracket::kSquare == child.fBracket || Bracket::kParen == child.fBracket) {
                continue;
            }
            eatOperator = false;
            fContinuation = nullptr;
            if (KeyWord::kConst == child.fKeyWord) {
                continue;
            }
        }
        if (memberEnd) {
            if (memberEnd != &child) {
                continue;
            }
            startDef = &child;
            fStart = child.fContentStart + 1;
            memberEnd = nullptr;
        }
        if (child.fPrivate) {
            if (MarkType::kMethod == child.fMarkType) {
                inInline = true;
            }
            continue;
        }
        if (inInline) {
            if (Definition::Type::kKeyWord == child.fType) {
                SkASSERT(MarkType::kMethod != child.fMarkType);
                continue;
            }
            if (Definition::Type::kPunctuation == child.fType) {
                if (Punctuation::kLeftBrace == child.fPunctuation) {
                    inInline = false;
                } else {
                    SkASSERT(Punctuation::kAsterisk == child.fPunctuation);
                }
                continue;
            }
            if (Definition::Type::kWord == child.fType) {
                string name(child.fContentStart, child.fContentEnd - child.fContentStart);
                SkASSERT(string::npos != name.find("::"));
                continue;
            }
            if (Definition::Type::kBracket == child.fType) {
                SkASSERT(Bracket::kParen == child.fBracket);
                continue;
            }
        }
        if (fContinuation) {
            if (Definition::Type::kKeyWord == child.fType) {
                if (KeyWord::kFriend == child.fKeyWord ||
                        KeyWord::kSK_API == child.fKeyWord) {
                    continue;
                }
                const IncludeKey& includeKey = kKeyWords[(int) child.fKeyWord];
                if (KeyProperty::kNumber == includeKey.fProperty) {
                    continue;
                }
            }
            if (Definition::Type::kBracket == child.fType) {
                if (Bracket::kAngle == child.fBracket) {
                    continue;
                }
                if (Bracket::kParen == child.fBracket) {
                    if (!clonedMethod) {
                        if (inConstructor) {
                            fContinuation = child.fContentStart;
                        }
                        continue;
                    }
                    int alternate = 1;
                    ptrdiff_t childLen = child.fContentEnd - child.fContentStart;
                    SkASSERT(')' == child.fContentStart[childLen]);
                    ++childLen;
                    do {
                        TextParser params(clonedMethod->fFileName, clonedMethod->fStart,
                            clonedMethod->fContentStart, clonedMethod->fLineCount);
                        params.skipToEndBracket('(');
                        if (params.startsWith(child.fContentStart, childLen)) {
                            this->methodOut(clonedMethod, child);
                            break;
                        }
                        ++alternate;
                        string alternateMethod = methodName + '_' + to_string(alternate);
                        clonedMethod = root->find(alternateMethod,
                                RootDefinition::AllowParens::kNo);
                    } while (clonedMethod);
                    if (!clonedMethod) {
                        return this->reportError<bool>("cloned method not found");
                    }
                    clonedMethod = nullptr;
                    continue;
                }
            }
            if (Definition::Type::kWord == child.fType) {
                if (clonedMethod) {
                    continue;
                }
                size_t len = (size_t) (child.fContentEnd - child.fContentStart);
                const char operatorStr[] = "operator";
                size_t operatorLen = sizeof(operatorStr) - 1;
                if (len >= operatorLen && !strncmp(child.fContentStart, operatorStr, operatorLen)) {
                    fContinuation = child.fContentEnd;
                    continue;
                }
            }
            if (Definition::Type::kPunctuation == child.fType &&
                    (Punctuation::kSemicolon == child.fPunctuation ||
                    Punctuation::kLeftBrace == child.fPunctuation ||
                    (Punctuation::kColon == child.fPunctuation && inConstructor))) {
                SkASSERT(fContinuation[0] == '(');
                const char* continueEnd = child.fContentStart;
                while (continueEnd > fContinuation && isspace(continueEnd[-1])) {
                    --continueEnd;
                }
                methodName += string(fContinuation, continueEnd - fContinuation);
                method = root->find(methodName, RootDefinition::AllowParens::kNo);
                if (!method) {
                    if (fBmhStructDef && fBmhStructDef->fDeprecated) {
                        fContinuation = nullptr;
                        continue;
                    }
                    return child.reportError<bool>("method not found");
                }
                this->methodOut(method, child);
                continue;
            }
            if (Definition::Type::kPunctuation == child.fType &&
                    Punctuation::kAsterisk == child.fPunctuation &&
                    clonedMethod) {
                continue;
            }
            if (inConstructor) {
                continue;
            }
            method = root->find(methodName + "()", RootDefinition::AllowParens::kNo);
            if (method && MarkType::kDefinedBy == method->fMarkType) {
                method = method->fParent;
            }
            if (method) {
                if (method->fCloned) {
                    clonedMethod = method;
                    continue;
                }
                this->methodOut(method, child);
                continue;
            } else if (fBmhStructDef && fBmhStructDef->fDeprecated) {
                fContinuation = nullptr;
                continue;
            }
            return child.reportError<bool>("method not found");
        }
        if (Bracket::kSlashSlash == child.fBracket || Bracket::kSlashStar == child.fBracket) {
            if (!fDeferComment) {
                fDeferComment = &child;
            }
            fLastComment = &child;
            continue;
        }
        if (MarkType::kMethod == child.fMarkType) {
            if (this->internalName(child)) {
                continue;
            }
            const char* bodyEnd = fDeferComment ? fDeferComment->fContentStart - 1 :
                    fAttrDeprecated ? fAttrDeprecated->fContentStart - 1 :
                    child.fContentStart;
            if (Definition::Type::kBracket == def->fType && Bracket::kDebugCode == def->fBracket) {
                auto tokenIter = def->fParent->fTokens.begin();
                std::advance(tokenIter, def->fParentIndex - 1);
                Definition* prior = &*tokenIter;
                if (Definition::Type::kBracket == def->fType &&
                        Bracket::kSlashStar == prior->fBracket) {
                    bodyEnd = prior->fContentStart - 1;
                }
            }
            // FIXME: roll end-trimming into writeBlockTrim call
            while (fStart < bodyEnd && ' ' >= bodyEnd[-1]) {
                --bodyEnd;
            }
            int blockSize = (int) (bodyEnd - fStart);
            if (blockSize) {
                string debugstr(fStart, blockSize);
                this->writeBlock(blockSize, fStart);
            }
            startDef = &child;
            fStart = child.fContentStart;
            auto mapFind = fBmhParser->fMethodMap.find(child.fName);
            if (fBmhParser->fMethodMap.end() != mapFind) {
                inConstructor = false;
                method = &mapFind->second;
            } else {
                methodName = root->fName + "::" + child.fName;
                inConstructor = root->fName == child.fName;
                method = root->find(methodName, RootDefinition::AllowParens::kNo);
            }
            fContinuation = child.fContentEnd;
            if (!method) {
                continue;
            }
            if (method->fCloned) {
                clonedMethod = method;
                continue;
            }
            this->methodOut(method, child);
            if (fAttrDeprecated) {
                startDef = fAttrDeprecated;
                fStart = fAttrDeprecated->fContentStart;
                fAttrDeprecated = nullptr;
            }
            continue;
        }
        if (Definition::Type::kKeyWord == child.fType) {
            if (fIndentNext) {
    // too soon
#if 0  // makes struct Lattice indent when it oughtn't
                if (KeyWord::kEnum == child.fKeyWord) {
                    fIndent += 4;
                }
                if (KeyWord::kPublic != child.fKeyWord) {
                    fIndentNext = false;
                }
#endif
            }
            switch (child.fKeyWord) {
                case KeyWord::kStruct:
                case KeyWord::kClass:
                    fStructMemberTab = 0;
                    // if struct contains members, compute their name and comment tabs
                    if (child.fChildren.size() > 0) {
                        const ParentPair* testPair = &pair;
                        while ((testPair = testPair->fPrev)) {
                            if (KeyWord::kClass == testPair->fParent->fKeyWord) {
                                inStruct = fInStruct = true;
                                break;
                            }
                        }
                    }
                    if (fInStruct) {
                        // try child; root+child; root->parent+child; etc.
                        int trial = 0;
                        const RootDefinition* search = root;
                        const Definition* parent = search->fParent;
                        do {
                            string name;
                            if (0 == trial) {
                                name = child.fName;
                            } else if (1 == trial) {
                                name = root->fName + "::" + child.fName;
                            } else {
                                SkASSERT(parent);
                                name = parent->fName + "::" + child.fName;
                                search = parent->asRoot();
                                parent = search->fParent;
                            }
                            fBmhStructDef = search->find(name, RootDefinition::AllowParens::kNo);
                        } while (!fBmhStructDef && ++trial);
                        root = const_cast<RootDefinition*>(fBmhStructDef->asRoot());
                        SkASSERT(root);
                        fIndent += 4;
                        this->structSizeMembers(child);
                        fIndent -= 4;
                        SkASSERT(!fIndentNext);
                        fIndentNext = true;
                    }
                    if (child.fChildren.size() > 0) {
                        const char* bodyEnd = fDeferComment ? fDeferComment->fContentStart - 1 :
                                child.fContentStart;
                        this->writeBlockTrim((int) (bodyEnd - fStart), fStart);
                        if (fPendingMethod) {
                            fIndent -= 4;
                            fPendingMethod = false;
                        }
                        startDef = requireDense ? requireDense : &child;
                        fStart = requireDense ? requireDense->fContentStart : child.fContentStart;
                        requireDense = nullptr;
                        if (!fInStruct && child.fName != root->fName) {
                            root = &fBmhParser->fClassMap[child.fName];
                            fRootTopic = root->fParent;
                            SkASSERT(!root->fVisited);
                            root->clearVisited();
                            fIndent = 0;
                            fBmhStructDef = root;
                        }
                        if (child.fName == root->fName) {
                            if (Definition* parent = root->fParent) {
                                if (MarkType::kTopic == parent->fMarkType ||
                                        MarkType::kSubtopic == parent->fMarkType) {
                                    const char* commentStart = root->fContentStart;
                                    const char* commentEnd = root->fChildren[0]->fStart;
                                    this->structOut(root, *root, commentStart, commentEnd);
                                } else {
                                    SkASSERT(0); // incomplete
                                }
                            } else {
                                SkASSERT(0); // incomplete
                            }
                        } else {
                            SkASSERT(fInStruct);
                #if 0
                            fBmhStructDef = root->find(child.fName, RootDefinition::AllowParens::kNo);
                            if (nullptr == fBmhStructDef) {
                                fBmhStructDef = root->find(root->fName + "::" + child.fName,
                                        RootDefinition::AllowParens::kNo);
                            }
                            if (!fBmhStructDef) {
                                this->lf(2);
                                fIndent = 0;
                                this->writeBlock((int) (fStart - bodyEnd), bodyEnd);
                                this->lfcr();
                                continue;
                            }
                #endif
                            Definition* codeBlock = nullptr;
                            Definition* nextBlock = nullptr;
                            for (auto test : fBmhStructDef->fChildren) {
                                if (MarkType::kCode == test->fMarkType) {
                                    SkASSERT(!codeBlock);  // FIXME: check enum for correct order earlier
                                    codeBlock = test;
                                    continue;
                                }
                                if (codeBlock) {
                                    nextBlock = test;
                                    break;
                                }
                            }
                            // FIXME: trigger error earlier if inner #Struct or #Class is missing #Code
                            if (!fBmhStructDef->fDeprecated) {
                                SkASSERT(codeBlock);
                                SkASSERT(nextBlock);  // FIXME: check enum for correct order earlier
                                const char* commentStart = codeBlock->fTerminator;
                                const char* commentEnd = nextBlock->fStart;
                                fIndentNext = true;
                                this->structOut(root, *fBmhStructDef, commentStart, commentEnd);
                            }
                        }
                        fDeferComment = nullptr;
                    } else {
                       ; // empty forward reference, nothing to do here
                    }
                    break;
                case KeyWord::kEnum: {
                    fInEnum = true;
                    this->enumHeaderOut(root, child);
                    this->enumSizeItems(child);
                } break;
                case KeyWord::kConst:
                case KeyWord::kConstExpr:
                    sawConst = !memberStart || staticOnly;
                    if (!memberStart) {
                        memberStart = &child;
                        staticOnly = true;
                    }
                    break;
                case KeyWord::kStatic:
                    if (!memberStart) {
                        memberStart = &child;
                        staticOnly = true;
                    }
                    break;
                case KeyWord::kInt:
                case KeyWord::kUint8_t:
                case KeyWord::kUint16_t:
                case KeyWord::kUint32_t:
                case KeyWord::kUint64_t:
                case KeyWord::kUnsigned:
                case KeyWord::kSize_t:
                case KeyWord::kFloat:
                case KeyWord::kBool:
                case KeyWord::kChar:
                case KeyWord::kVoid:
                    staticOnly = false;
                    if (!memberStart) {
                        memberStart = &child;
                    }
                    break;
                case KeyWord::kPublic:
                case KeyWord::kPrivate:
                case KeyWord::kProtected:
                case KeyWord::kFriend:
                case KeyWord::kInline:
                case KeyWord::kSK_API:
                case KeyWord::kTemplate:
                case KeyWord::kTypedef:
                    break;
                case KeyWord::kSK_BEGIN_REQUIRE_DENSE:
                    requireDense = &child;
                    break;
                default:
                    SkASSERT(0);
            }
            if (KeyWord::kUint8_t == child.fKeyWord) {
                continue;
            } else {
                if (fInEnum && KeyWord::kClass == child.fChildren[0]->fKeyWord) {
                    if (!this->populate(child.fChildren[0], &pair, root)) {
                        return false;
                    }
                } else {
                    if (!this->populate(&child, &pair, root)) {
                        return false;
                    }
                    if (KeyWord::kClass == child.fKeyWord || KeyWord::kStruct == child.fKeyWord) {
                        if (fInStruct) {
                            fInStruct = false;
                            do {
                                SkASSERT(root);
                                root = const_cast<RootDefinition*>(root->fParent->asRoot());
                            } while (MarkType::kTopic == root->fMarkType ||
                                    MarkType::kSubtopic == root->fMarkType);
                            SkASSERT(MarkType::kStruct == root->fMarkType ||
                            MarkType::kClass == root->fMarkType);
                            fPendingMethod = false;
                            if (startDef) {
                                fPendingMethod = find_start(startDef, fStart);
                            }
                            fOutdentNext = !fPendingMethod;
                        }
                    }
                }
            }
            continue;
        }
        if (Definition::Type::kBracket == child.fType) {
            if (KeyWord::kEnum == child.fParent->fKeyWord ||
                    (KeyWord::kClass == child.fParent->fKeyWord && child.fParent->fParent &&
                    KeyWord::kEnum == child.fParent->fParent->fKeyWord)) {
                SkASSERT(Bracket::kBrace == child.fBracket);
                this->enumMembersOut(root, child);
                this->writeString("};");
                this->lf(2);
                startDef = child.fParent;
                fStart = child.fParent->fContentEnd;
                SkASSERT(';' == fStart[0]);
                ++fStart;
                fDeferComment = nullptr;
                fInEnum = false;
                if (fIndentNext) {
//                    fIndent -= 4;
                    fIndentNext = false;
                }
                continue;
            }
            if (fAttrDeprecated) {
                continue;
            }
            fDeferComment = nullptr;
            if (KeyWord::kClass == def->fKeyWord || KeyWord::kStruct == def->fKeyWord) {
                fIndentNext = true;
            }
            if (!this->populate(&child, &pair, root)) {
                return false;
            }
            continue;
        }
        if (Definition::Type::kWord == child.fType) {
            if (MarkType::kMember == child.fMarkType) {
                if (!memberStart) {
                    auto iter = def->fTokens.begin();
                    std::advance(iter, child.fParentIndex - 1);
                    memberStart = &*iter;
                    staticOnly = false;
                    if (!fStructMemberTab) {
                        SkASSERT(KeyWord::kStruct == def->fParent->fKeyWord);
                        fIndent += 4;
                        this->structSizeMembers(*def->fParent);
                        fIndent -= 4;
//                        SkASSERT(!fIndentNext);
                        fIndentNext = true;
                    }
                }
                SkASSERT(fBmhStructDef);
                if (!fBmhStructDef->fDeprecated) {
                    memberEnd = this->structMemberOut(memberStart, child);
                    startDef = &child;
                    fStart = child.fContentEnd + 1;
                    fDeferComment = nullptr;
                }
            } else if (MarkType::kNone == child.fMarkType && sawConst
                    && fEnumDef && !fEnumDef->fDeprecated) {
                const Definition* bmhConst = nullptr;
                string match;
                if (root) {
                    match = root->fName + "::";
                }
                match += string(child.fContentStart, child.fContentEnd - child.fContentStart);
                for (auto enumChild : fEnumDef->fChildren) {
                    if (MarkType::kConst == enumChild->fMarkType && enumChild->fName == match) {
                        bmhConst = enumChild;
                        break;
                    }
                }
                if (bmhConst) {
                    this->constOut(memberStart, child, bmhConst);
                    fDeferComment = nullptr;
                    sawConst = false;
                }
            }
            if (child.fMemberStart) {
                memberStart = &child;
                staticOnly = false;
            }
            if (kAttrDeprecatedLen == (size_t) (child.fContentEnd - child.fContentStart) &&
                    !strncmp(gAttrDeprecated, child.fStart, kAttrDeprecatedLen)) {
                fAttrDeprecated = &child;
            }
            continue;
        }
        if (Definition::Type::kPunctuation == child.fType) {
            if (Punctuation::kSemicolon == child.fPunctuation) {
                memberStart = nullptr;
                sawConst = false;
                staticOnly = false;
                if (inStruct) {
                    fInStruct = false;
                }
                continue;
            }
            if (Punctuation::kLeftBrace == child.fPunctuation ||
                    Punctuation::kColon == child.fPunctuation ||
                    Punctuation::kAsterisk == child.fPunctuation
                ) {
                continue;
            }
        }
    }
    return true;
}

bool IncludeWriter::populate(BmhParser& bmhParser) {
    bool allPassed = true;
    for (auto& includeMapper : fIncludeMap) {
        size_t lastSlash = includeMapper.first.rfind('/');
        if (string::npos == lastSlash) {
            lastSlash = includeMapper.first.rfind('\\');
        }
        if (string::npos == lastSlash || lastSlash >= includeMapper.first.length() - 1) {
            return this->reportError<bool>("malformed include name");
        }
        string fileName = includeMapper.first.substr(lastSlash + 1);
        if (".h" != fileName.substr(fileName.length() - 2)) {
            return this->reportError<bool>("expected fileName.h");
        }
        string skClassName = fileName.substr(0, fileName.length() - 2);
        fOut = fopen(fileName.c_str(), "wb");
        if (!fOut) {
            SkDebugf("could not open output file %s\n", fileName.c_str());
            return false;
        }
        if (bmhParser.fClassMap.end() == bmhParser.fClassMap.find(skClassName)) {
            return this->reportError<bool>("could not find bmh class");
        }
        fBmhParser = &bmhParser;
        RootDefinition* root = &bmhParser.fClassMap[skClassName];
        fRootTopic = root->fParent;
        root->clearVisited();
        fStart = includeMapper.second.fContentStart;
        fEnd = includeMapper.second.fContentEnd;
        fAnonymousEnumCount = 1;
        allPassed &= this->populate(&includeMapper.second, nullptr, root);
        this->writeBlock((int) (fEnd - fStart), fStart);
        fIndent = 0;
        this->lfcr();
        this->writePending();
        fclose(fOut);
        fflush(fOut);
        size_t slash = fFileName.find_last_of('/');
        if (string::npos == slash) {
            slash = 0;
        }
        size_t back = fFileName.find_last_of('\\');
        if (string::npos == back) {
            back = 0;
        }
        string dir = fFileName.substr(0, SkTMax(slash, back) + 1);
        string readname = dir + fileName;
        if (this->writtenFileDiffers(fileName, readname)) {
            SkDebugf("wrote updated %s\n", fileName.c_str());
        } else {
            remove(fileName.c_str());
        }
    }
    return allPassed;
}

// change Xxx_Xxx to xxx xxx
static string ConvertRef(const string str, bool first) {
    string substitute;
    for (char c : str) {
        if ('_' == c) {
            c = ' ';  // change Xxx_Xxx to xxx xxx
        } else if (isupper(c) && !first) {
            c = tolower(c);
        }
        substitute += c;
        first = false;
    }
    return substitute;
}

string IncludeWriter::resolveMethod(const char* start, const char* end, bool first) {
    string methodname(start, end - start);
    if (string::npos != methodname.find("()")) {
        return "";
    }
    string substitute;
    auto rootDefIter = fBmhParser->fMethodMap.find(methodname);
    if (fBmhParser->fMethodMap.end() != rootDefIter) {
        substitute = methodname + "()";
    } else {
        RootDefinition* parent = nullptr;
        for (auto candidate : fRootTopic->fChildren) {
            if (MarkType::kClass == candidate->fMarkType
                    || MarkType::kStruct == candidate->fMarkType) {
                parent = candidate->asRoot();
                break;
            }
        }
        SkASSERT(parent);
        auto defRef = parent->find(parent->fName + "::" + methodname,
                RootDefinition::AllowParens::kNo);
        if (defRef && MarkType::kMethod == defRef->fMarkType) {
            substitute = methodname + "()";
        }
    }
    if (fMethodDef && methodname == fMethodDef->fName) {
        TextParser report(fBmhMethod);
        report.reportError("method should not include references to itself");
        return "";
    }
    if (fBmhMethod) {
        for (auto child : fBmhMethod->fChildren) {
            if (MarkType::kParam != child->fMarkType) {
                continue;
            }
            if (methodname == child->fName) {
                return "";
            }
        }
    }
    return substitute;
}

string IncludeWriter::resolveRef(const char* start, const char* end, bool first,
        RefType* refType) {
        // look up Xxx_Xxx
    string undername(start, end - start);
    for (const auto& external : fBmhParser->fExternals) {
        if (external.fName == undername) {
            *refType = RefType::kExternal;
            return external.fName;
        }
    }
    *refType = RefType::kNormal;
    SkASSERT(string::npos == undername.find(' '));
    const Definition* rootDef = nullptr;
    string substitute;
    {
        auto rootDefIter = fBmhParser->fTopicMap.find(undername);
        if (fBmhParser->fTopicMap.end() != rootDefIter) {
            rootDef = rootDefIter->second;
        } else {
            string prefixedName = fRootTopic->fName + '_' + undername;
            rootDefIter = fBmhParser->fTopicMap.find(prefixedName);
            if (fBmhParser->fTopicMap.end() != rootDefIter) {
                rootDef = rootDefIter->second;
            } else if (fBmhStructDef) {
                string localPrefix = fBmhStructDef->fFiddle + '_' + undername;
                rootDefIter = fBmhParser->fTopicMap.find(localPrefix);
                if (fBmhParser->fTopicMap.end() != rootDefIter) {
                    rootDef = rootDefIter->second;
                }
                if (!rootDef) {
                    size_t doubleColon = fBmhStructDef->fName.rfind("::");
                    if (string::npos != doubleColon && undername
                            == fBmhStructDef->fName.substr(doubleColon + 2)) {
                        substitute = fBmhStructDef->fName;
                    }
                }
            }
            if (!rootDef && !substitute.length()) {
                auto aliasIter = fBmhParser->fAliasMap.find(undername);
                if (fBmhParser->fAliasMap.end() != aliasIter) {
                    rootDef = aliasIter->second;
                } else if (fInEnum && fEnumDef && this->findEnumSubtopic(undername, &rootDef)) {
                    ;
                } else if (!first) {
                    this->fChar = start;
                    this->reportError("reference unfound");
                    return "";
                }
            }
        }
    }
    if (rootDef) {
        MarkType rootType = rootDef->fMarkType;
        bool isTopic = MarkType::kSubtopic == rootType || MarkType::kTopic == rootType;
        auto substituteParent = MarkType::kAlias == rootType ? rootDef->fParent :
                isTopic ? rootDef : nullptr;
        if (substituteParent) {
            for (auto child : substituteParent->fChildren) {
                if (MarkType::kSubstitute == child->fMarkType) {
                    substitute = string(child->fContentStart,
                            (int) (child->fContentEnd - child->fContentStart));
                    break;
                }
            }
        }
        if (!substitute.length()) {
            string match = rootDef->fName;
            size_t index;
            while (string::npos != (index = match.find('_'))) {
                match.erase(index, 1);
            }
            string skmatch = "Sk" + match;
            auto parent = substituteParent ? substituteParent : rootDef;
            for (auto child : parent->fChildren) {
                // there may be more than one
                // prefer the one mostly closely matching in text
                if ((MarkType::kClass == child->fMarkType ||
                    MarkType::kStruct == child->fMarkType ||
                    (MarkType::kEnum == child->fMarkType && !child->fAnonymous) ||
                    MarkType::kEnumClass == child->fMarkType) && (match == child->fName ||
                    skmatch == child->fName)) {
                    substitute = child->fName;
                    break;
                }
            }
        }
        if (!substitute.length()) {
            for (auto child : rootDef->fChildren) {
                // there may be more than one
                // if so, it's a bug since it's unknown which is the right one
                if (MarkType::kClass == child->fMarkType ||
                        MarkType::kStruct == child->fMarkType ||
                        (MarkType::kEnum == child->fMarkType && !child->fAnonymous) ||
                        MarkType::kEnumClass == child->fMarkType) {
                    SkASSERT("" == substitute);
                    substitute = child->fName;
                    if (MarkType::kEnum == child->fMarkType) {
                        size_t parentClassEnd = substitute.find("::");
                        SkASSERT(string::npos != parentClassEnd);
                        string subEnd = substitute.substr(parentClassEnd + 2);
                        if (fInEnum) {
                            substitute = subEnd;
                        }
                        if (subEnd == undername) {
                            break;
                        }
                    }
                }
            }
        }
        if (!substitute.length()) {
            const Definition* parent = rootDef;
            do {
                parent = parent->fParent;
            } while (parent && (MarkType::kSubtopic == parent->fMarkType
                        || MarkType::kTopic == parent->fMarkType));
            if (parent) {
                if (MarkType::kClass == parent->fMarkType ||
                        MarkType::kStruct == parent->fMarkType ||
                        (MarkType::kEnum == parent->fMarkType && !parent->fAnonymous) ||
                        MarkType::kEnumClass == parent->fMarkType) {
                    if (parent->fParent != fRootTopic) {
                        substitute = parent->fName;
                        substitute += ' ';
                        substitute += ConvertRef(rootDef->fName, false);
                    } else {
                        substitute += ConvertRef(undername, first);
                    }
                }
            }
        }
    }
    // Ensure first word after period is capitalized if substitute is lower cased.
    if (first && isupper(start[0]) && substitute.length() > 0 && islower(substitute[0])) {
        substitute[0] = start[0];
    }
    return substitute;
}

int IncludeWriter::lookupMethod(const PunctuationState punctuation, const Word word,
        const int lastSpace, const int run, int lastWrite, const char* data,
        bool hasIndirection) {
    int wordStart = lastSpace;
    while (' ' >= data[wordStart]) {
        ++wordStart;
    }
    const int wordEnd = PunctuationState::kDelimiter == punctuation ||
            PunctuationState::kParen == punctuation ||
            PunctuationState::kPeriod == punctuation ? run - 1 : run;
    string temp;
    if (hasIndirection && '(' != data[wordEnd - 1] && ')' != data[wordEnd - 1]) {
        // FIXME: hard-coded to assume a.b or a->b is a.b() or a->b().
        // need to check class a for member b to see if this is so
        TextParser parser(fFileName, &data[wordStart], &data[wordEnd], fLineCount);
        const char* indirection = parser.anyOf(".>");
        if (&data[wordEnd] <= &indirection[2] || 'f' != indirection[1] ||
                !isupper(indirection[2])) {
            temp = string(&data[wordStart], wordEnd - wordStart) + "()";
        }
    } else {
        temp = this->resolveMethod(&data[wordStart], &data[wordEnd], Word::kFirst == word);
    }
    if (temp.length()) {
        if (wordStart > lastWrite) {
            SkASSERT(data[wordStart - 1] >= ' ');
            if (' ' == data[lastWrite]) {
                this->writeSpace();
            }
            this->writeBlockTrim(wordStart - lastWrite, &data[lastWrite]);
            if (' ' == data[wordStart - 1]) {
                this->writeSpace();
            }
        }
        SkASSERT(temp[temp.length() - 1] > ' ');
        this->writeString(temp.c_str());
        lastWrite = wordEnd;
    }
    return lastWrite;
}

int IncludeWriter::lookupReference(const PunctuationState punctuation, const Word word,
        const int start, const int run, int lastWrite, const char last, const char* data) {
    const int end = PunctuationState::kDelimiter == punctuation ||
            PunctuationState::kParen == punctuation ||
            PunctuationState::kPeriod == punctuation ? run - 1 : run;
    RefType refType = RefType::kUndefined;
    string resolved = string(&data[start], (size_t) (end - start));
    string temp = this->resolveRef(&data[start], &data[end], Word::kFirst == word, &refType);
    if (!temp.length()) {
        if (Word::kFirst != word && '_' != last) {
            temp = ConvertRef(resolved, false);
        }
    }
    if (temp.length()) {
        if (start > lastWrite) {
            SkASSERT(data[start - 1] >= ' ');
            if (' ' == data[lastWrite]) {
                this->writeSpace();
            }
            this->writeBlockTrim(start - lastWrite, &data[lastWrite]);
            if (' ' == data[start - 1]) {
                this->writeSpace();
            }
        }
        SkASSERT(temp[temp.length() - 1] > ' ');
        this->writeString(temp.c_str());
        lastWrite = end;
    }
    return lastWrite;
}

/* returns true if rewriteBlock wrote linefeeds */
IncludeWriter::Wrote IncludeWriter::rewriteBlock(int size, const char* data, Phrase phrase) {
    bool wroteLineFeeds = false;
    while (size > 0 && data[0] <= ' ') {
        --size;
        ++data;
    }
    while (size > 0 && data[size - 1] <= ' ') {
        --size;
    }
    if (0 == size) {
        return Wrote::kNone;
    }
    int run = 0;
    Word word = Word::kStart;
    PunctuationState punctuation = Phrase::kNo == phrase ?
            PunctuationState::kStart : PunctuationState::kSpace;
    int start = 0;
    int lastWrite = 0;
    int lineFeeds = 0;
    int lastPrintable = 0;
    int lastSpace = -1;
    char c = 0;
    char last = 0;
    bool embeddedIndirection = false;
    bool embeddedSymbol = false;
    bool hasLower = false;
    bool hasUpper = false;
    bool hasIndirection = false;
    bool hasSymbol = false;
    while (run < size) {
        last = c;
        c = data[run];
        SkASSERT(' ' <= c || '\n' == c);
        if (lineFeeds && ' ' < c) {
            if (lastPrintable >= lastWrite) {
                if (' ' == data[lastWrite]) {
                    this->writeSpace();
                    lastWrite++;
                }
                this->writeBlock(lastPrintable - lastWrite + 1, &data[lastWrite]);
            }
            if (lineFeeds > 1) {
                this->lf(2);
            }
            this->lfcr(); // defer the indent until non-whitespace is seen
            lastWrite = run;
            lineFeeds = 0;
        }
        if (' ' < c) {
            lastPrintable = run;
        }
        switch (c) {
            case '\n':
                ++lineFeeds;
                wroteLineFeeds = true;
            case ' ':
                switch (word) {
                    case Word::kStart:
                        break;
                    case Word::kUnderline:
                    case Word::kCap:
                    case Word::kFirst:
                        if (!hasLower) {
                            break;
                        }
                        lastWrite = this->lookupReference(punctuation, word, start, run,
                                lastWrite, last, data);
                        break;
                    case Word::kMixed:
                        if (hasUpper && hasLower && !hasSymbol && lastSpace > 0) {
                            lastWrite = this->lookupMethod(punctuation, word, lastSpace, run,
                                    lastWrite, data, hasIndirection);
                        }
                        break;
                    default:
                        SkASSERT(0);
                }
                punctuation = PunctuationState::kPeriod == punctuation ||
                        (PunctuationState::kStart == punctuation && ' ' >= last) ?
                        PunctuationState::kStart : PunctuationState::kSpace;
                word = Word::kStart;
                embeddedIndirection = false;
                embeddedSymbol = false;
                hasLower = false;
                hasUpper = false;
                hasIndirection = false;
                hasSymbol = false;
                lastSpace = run;
                break;
            case '.': case ',': case ';': case ':': case ')':
                switch (word) {
                    case Word::kStart:
                        punctuation = PunctuationState::kDelimiter;
                    case Word::kCap:
                    case Word::kFirst:
                    case Word::kUnderline:
                    case Word::kMixed:
                        if (PunctuationState::kDelimiter == punctuation ||
                                PunctuationState::kPeriod == punctuation) {
                            word = Word::kMixed;
                        }
                        punctuation = '.' == c ? PunctuationState::kPeriod :
                                PunctuationState::kDelimiter;
                        break;
                    default:
                        SkASSERT(0);
                }
                ('.' == c ? embeddedIndirection : embeddedSymbol) = true;
                break;
            case '>':
                if ('-' == last) {
                    embeddedIndirection = true;
                    break;
                }
            case '\'': // possessive apostrophe isn't treated as delimiting punctation
            case '\"': // quote is passed straight through
            case '=':
            case '!':  // assumed not to be punctuation, but a programming symbol
            case '&': case '<': case '{': case '}': case '/': case '*': case '[': case ']':
                word = Word::kMixed;
                embeddedSymbol = true;
                break;
            case '(':
                if (' ' == last) {
                    punctuation = PunctuationState::kParen;
                } else {
                    word = Word::kMixed;
                }
                embeddedSymbol = true;
                break;
            case '_':
                switch (word) {
                    case Word::kStart:
                        word = Word::kMixed;
                        break;
                    case Word::kCap:
                    case Word::kFirst:
                    case Word::kUnderline:
                        word = Word::kUnderline;
                        break;
                    case Word::kMixed:
                        break;
                    default:
                        SkASSERT(0);
                }
                hasSymbol |= embeddedSymbol;
                break;
            case '+':
                // hackery to allow C++
                SkASSERT('C' == last || '+' == last);  // FIXME: don't allow + outside of #Formula
                break;
            case 'A': case 'B': case 'C': case 'D': case 'E':
            case 'F': case 'G': case 'H': case 'I': case 'J':
            case 'K': case 'L': case 'M': case 'N': case 'O':
            case 'P': case 'Q': case 'R': case 'S': case 'T':
            case 'U': case 'V': case 'W': case 'X': case 'Y':
            case 'Z':
                switch (word) {
                    case Word::kStart:
                        word = PunctuationState::kStart == punctuation ? Word::kFirst : Word::kCap;
                        start = run;
                        break;
                    case Word::kCap:
                    case Word::kFirst:
                        if (!isupper(last) && '~' != last) {
                            word = Word::kMixed;
                        }
                        break;
                    case Word::kUnderline:
                        // some word in Xxx_XXX_Xxx can be all upper, but all can't: XXX_XXX
                        if ('_' != last && !isupper(last)) {
                            word = Word::kMixed;
                        }
                        break;
                    case Word::kMixed:
                        break;
                    default:
                        SkASSERT(0);
                }
                hasUpper = true;
                if (PunctuationState::kPeriod == punctuation ||
                        PunctuationState::kDelimiter == punctuation) {
                    word = Word::kMixed;
                }
                hasIndirection |= embeddedIndirection;
                hasSymbol |= embeddedSymbol;
                break;
            case 'a': case 'b': case 'c': case 'd': case 'e':
            case 'f': case 'g': case 'h': case 'i': case 'j':
            case 'k': case 'l': case 'm': case 'n': case 'o':
            case 'p': case 'q': case 'r': case 's': case 't':
            case 'u': case 'v': case 'w': case 'x': case 'y':
            case 'z':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
            case '-':
                switch (word) {
                    case Word::kStart:
                        word = Word::kMixed;
                        break;
                    case Word::kMixed:
                    case Word::kCap:
                    case Word::kFirst:
                    case Word::kUnderline:
                        break;
                    default:
                        SkASSERT(0);
                }
                hasLower = true;
                punctuation = PunctuationState::kStart;
                hasIndirection |= embeddedIndirection;
                hasSymbol |= embeddedSymbol;
                break;
            case '~':
                SkASSERT(Word::kStart == word);
                word = PunctuationState::kStart == punctuation ? Word::kFirst : Word::kCap;
                start = run;
                hasUpper = true;
                hasIndirection |= embeddedIndirection;
                hasSymbol |= embeddedSymbol;
                break;
            default:
                SkASSERT(0);
        }
        ++run;
    }
    if ((word == Word::kCap || word == Word::kFirst || word == Word::kUnderline) && hasLower) {
        lastWrite = this->lookupReference(punctuation, word, start, run, lastWrite, last, data);
    } else if (word == Word::kMixed && hasUpper && hasLower && !hasSymbol && lastSpace > 0) {
        lastWrite = this->lookupMethod(punctuation, word, lastSpace, run, lastWrite, data,
                hasIndirection && !hasSymbol);
    }
    if (run > lastWrite) {
        if (' ' == data[lastWrite]) {
            this->writeSpace();
        }
        this->writeBlock(run - lastWrite, &data[lastWrite]);
    }
    return wroteLineFeeds ? Wrote::kLF : Wrote::kChars;
}
