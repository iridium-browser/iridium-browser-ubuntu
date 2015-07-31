/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/inspector/InspectorStyleSheet.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptRegexp.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSMediaRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSSupportsRule.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserObserver.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/ContentSearchUtils.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorIdentifiers.h"
#include "core/inspector/InspectorResourceAgent.h"
#include "core/svg/SVGStyleElement.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextPosition.h"

using blink::TypeBuilder::Array;
using blink::RuleSourceDataList;
using blink::CSSRuleSourceData;
using blink::CSSStyleSheet;

namespace {

using namespace blink;

static CSSParserContext parserContextForDocument(Document *document)
{
    return document ? CSSParserContext(*document, 0) : strictCSSParserContext();
}

class StyleSheetHandler final : public CSSParserObserver {
public:
    StyleSheetHandler(const String& parsedText, Document* document, StyleSheetContents* styleSheetContents, RuleSourceDataList* result)
        : m_parsedText(parsedText)
        , m_document(document)
        , m_styleSheetContents(styleSheetContents)
        , m_result(result)
        , m_propertyRangeStart(UINT_MAX)
        , m_selectorRangeStart(UINT_MAX)
        , m_commentRangeStart(UINT_MAX)
        , m_mediaQueryExpValueRangeStart(UINT_MAX)
    {
        ASSERT(m_result);
    }

private:
    virtual void startRuleHeader(StyleRule::Type, unsigned) override;
    virtual void endRuleHeader(unsigned) override;
    virtual void startSelector(unsigned) override;
    virtual void endSelector(unsigned) override;
    virtual void startRuleBody(unsigned) override;
    virtual void endRuleBody(unsigned, bool) override;
    virtual void startProperty(unsigned) override;
    virtual void endProperty(bool, bool, unsigned, CSSParserError) override;
    virtual void startComment(unsigned) override;
    virtual void endComment(unsigned) override;
    virtual void startMediaQueryExp(unsigned offset) override;
    virtual void endMediaQueryExp(unsigned offset) override;
    virtual void startMediaQuery() override;
    virtual void endMediaQuery() override;

    void addNewRuleToSourceTree(PassRefPtrWillBeRawPtr<CSSRuleSourceData>);
    PassRefPtrWillBeRawPtr<CSSRuleSourceData> popRuleData();
    template <typename CharacterType> inline void setRuleHeaderEnd(const CharacterType*, unsigned);
    void fixUnparsedPropertyRanges(CSSRuleSourceData*);

    const String& m_parsedText;
    Document* m_document;
    // TODO(timloh): Remove when Bison parser is gone.
    StyleSheetContents* m_styleSheetContents;
    RawPtrWillBeMember<RuleSourceDataList> m_result;
    RuleSourceDataList m_currentRuleDataStack;
    RefPtrWillBeMember<CSSRuleSourceData> m_currentRuleData;
    unsigned m_propertyRangeStart;
    unsigned m_selectorRangeStart;
    unsigned m_commentRangeStart;
    RefPtrWillBeMember<CSSMediaQuerySourceData> m_currentMediaQueryData;
    unsigned m_mediaQueryExpValueRangeStart;
};

void StyleSheetHandler::startRuleHeader(StyleRule::Type type, unsigned offset)
{
    // Pop off data for a previous invalid rule.
    if (m_currentRuleData)
        m_currentRuleDataStack.removeLast();

    RefPtrWillBeRawPtr<CSSRuleSourceData> data = CSSRuleSourceData::create(type);
    data->ruleHeaderRange.start = offset;
    m_currentRuleData = data;
    m_currentRuleDataStack.append(data.release());
}

template <typename CharacterType>
inline void StyleSheetHandler::setRuleHeaderEnd(const CharacterType* dataStart, unsigned listEndOffset)
{
    while (listEndOffset > 1) {
        if (isHTMLSpace<CharacterType>(*(dataStart + listEndOffset - 1)))
            --listEndOffset;
        else
            break;
    }

    m_currentRuleDataStack.last()->ruleHeaderRange.end = listEndOffset;
    if (!m_currentRuleDataStack.last()->selectorRanges.isEmpty())
        m_currentRuleDataStack.last()->selectorRanges.last().end = listEndOffset;
}

void StyleSheetHandler::endRuleHeader(unsigned offset)
{
    ASSERT(!m_currentRuleDataStack.isEmpty());

    if (m_parsedText.is8Bit())
        setRuleHeaderEnd<LChar>(m_parsedText.characters8(), offset);
    else
        setRuleHeaderEnd<UChar>(m_parsedText.characters16(), offset);
}

void StyleSheetHandler::startSelector(unsigned offset)
{
    m_selectorRangeStart = offset;
}

void StyleSheetHandler::endSelector(unsigned offset)
{
    ASSERT(m_currentRuleDataStack.size());
    m_currentRuleDataStack.last()->selectorRanges.append(SourceRange(m_selectorRangeStart, offset));
    m_selectorRangeStart = UINT_MAX;
}

void StyleSheetHandler::startRuleBody(unsigned offset)
{
    m_currentRuleData.clear();
    ASSERT(!m_currentRuleDataStack.isEmpty());
    if (m_parsedText[offset] == '{')
        ++offset; // Skip the rule body opening brace.
    m_currentRuleDataStack.last()->ruleBodyRange.start = offset;
}

void StyleSheetHandler::endRuleBody(unsigned offset, bool error)
{
    ASSERT(!m_currentRuleDataStack.isEmpty());
    m_currentRuleDataStack.last()->ruleBodyRange.end = offset;
    m_propertyRangeStart = UINT_MAX;
    RefPtrWillBeRawPtr<CSSRuleSourceData> rule = popRuleData();
    if (error)
        return;

    fixUnparsedPropertyRanges(rule.get());
    addNewRuleToSourceTree(rule.release());
}

void StyleSheetHandler::addNewRuleToSourceTree(PassRefPtrWillBeRawPtr<CSSRuleSourceData> rule)
{
    if (m_currentRuleDataStack.isEmpty())
        m_result->append(rule);
    else
        m_currentRuleDataStack.last()->childRules.append(rule);
}

PassRefPtrWillBeRawPtr<CSSRuleSourceData> StyleSheetHandler::popRuleData()
{
    ASSERT(!m_currentRuleDataStack.isEmpty());
    m_currentRuleData.clear();
    RefPtrWillBeRawPtr<CSSRuleSourceData> data = m_currentRuleDataStack.last();
    m_currentRuleDataStack.removeLast();
    return data.release();
}

template <typename CharacterType>
static inline void fixUnparsedProperties(const CharacterType* characters, CSSRuleSourceData* ruleData)
{
    WillBeHeapVector<CSSPropertySourceData>& propertyData = ruleData->styleSourceData->propertyData;
    unsigned size = propertyData.size();
    if (!size)
        return;

    CSSPropertySourceData* nextData = &(propertyData.at(0));
    for (unsigned i = 0; i < size; ++i) {
        CSSPropertySourceData* currentData = nextData;
        nextData = i < size - 1 ? &(propertyData.at(i + 1)) : nullptr;

        if (currentData->parsedOk)
            continue;
        if (currentData->range.end > 0 && characters[currentData->range.end - 1] == ';')
            continue;

        unsigned propertyEnd;
        if (!nextData)
            propertyEnd = ruleData->ruleBodyRange.end - 1;
        else
            propertyEnd = nextData->range.start - 1;

        while (isHTMLSpace<CharacterType>(characters[propertyEnd]))
            --propertyEnd;

        // propertyEnd points at the last property text character.
        unsigned newPropertyEnd = propertyEnd + 1; // Exclusive of the last property text character.
        if (currentData->range.end != newPropertyEnd) {
            currentData->range.end = newPropertyEnd;
            unsigned valueStart = currentData->range.start + currentData->name.length();
            while (valueStart < propertyEnd && characters[valueStart] != ':')
                ++valueStart;
            if (valueStart < propertyEnd)
                ++valueStart; // Shift past the ':'.
            while (valueStart < propertyEnd && isHTMLSpace<CharacterType>(characters[valueStart]))
                ++valueStart;
            // Need to exclude the trailing ';' from the property value.
            currentData->value = String(characters + valueStart, propertyEnd - valueStart + (characters[propertyEnd] == ';' ? 0 : 1));
        }
    }
}

void StyleSheetHandler::fixUnparsedPropertyRanges(CSSRuleSourceData* ruleData)
{
    if (!ruleData->styleSourceData)
        return;

    if (m_parsedText.is8Bit()) {
        fixUnparsedProperties<LChar>(m_parsedText.characters8(), ruleData);
        return;
    }

    fixUnparsedProperties<UChar>(m_parsedText.characters16(), ruleData);
}

void StyleSheetHandler::startProperty(unsigned offset)
{
    if (m_currentRuleDataStack.isEmpty() || !m_currentRuleDataStack.last()->styleSourceData)
        return;
    m_propertyRangeStart = offset;
}

void StyleSheetHandler::endProperty(bool isImportant, bool isParsed, unsigned offset, CSSParserError errorType)
{
    // FIXME: This is the only place CSSParserError is every read!?
    if (errorType != NoCSSError)
        m_propertyRangeStart = UINT_MAX;

    if (m_propertyRangeStart == UINT_MAX || m_currentRuleDataStack.isEmpty() || !m_currentRuleDataStack.last()->styleSourceData)
        return;

    ASSERT(offset <= m_parsedText.length());
    if (offset < m_parsedText.length() && m_parsedText[offset] == ';') // Include semicolon into the property text.
        ++offset;

    const unsigned start = m_propertyRangeStart;
    const unsigned end = offset;
    ASSERT(start < end);
    String propertyString = m_parsedText.substring(start, end - start).stripWhiteSpace();
    if (propertyString.endsWith(';'))
        propertyString = propertyString.left(propertyString.length() - 1);
    size_t colonIndex = propertyString.find(':');
    ASSERT(colonIndex != kNotFound);

    String name = propertyString.left(colonIndex).stripWhiteSpace();
    String value = propertyString.substring(colonIndex + 1, propertyString.length()).stripWhiteSpace();
    m_currentRuleDataStack.last()->styleSourceData->propertyData.append(
        CSSPropertySourceData(name, value, isImportant, false, isParsed, SourceRange(start, end)));
    m_propertyRangeStart = UINT_MAX;
}

void StyleSheetHandler::startComment(unsigned offset)
{
    ASSERT(m_commentRangeStart == UINT_MAX);
    m_commentRangeStart = offset;
}

void StyleSheetHandler::endComment(unsigned offset)
{
    ASSERT(offset <= m_parsedText.length());

    unsigned startOffset = m_commentRangeStart;
    m_commentRangeStart = UINT_MAX;
    if (m_propertyRangeStart != UINT_MAX) {
        ASSERT(startOffset >= m_propertyRangeStart);
        // startProperty() is called automatically at the start of a style declaration.
        // Check if no text has been scanned yet, otherwise the comment is inside a property.
        if (!m_parsedText.substring(m_propertyRangeStart, startOffset).stripWhiteSpace().isEmpty())
            return;
        m_propertyRangeStart = UINT_MAX;
    }
    if (m_currentRuleDataStack.isEmpty() || !m_currentRuleDataStack.last()->ruleHeaderRange.end || !m_currentRuleDataStack.last()->styleSourceData)
        return;

    // The lexer is not inside a property AND it is scanning a declaration-aware rule body.
    String commentText = m_parsedText.substring(startOffset, offset - startOffset);

    ASSERT(commentText.startsWith("/*"));
    commentText = commentText.substring(2);

    // Require well-formed comments.
    if (!commentText.endsWith("*/"))
        return;
    commentText = commentText.substring(0, commentText.length() - 2).stripWhiteSpace();
    if (commentText.isEmpty())
        return;

    // FIXME: Use the actual rule type rather than STYLE_RULE?
    RuleSourceDataList sourceData;

    // FIXME: Use another subclass of CSSParserObserver and assert that
    // no comments are encountered (will not need m_document and m_styleSheetContents).
    StyleSheetHandler handler(commentText, m_document, m_styleSheetContents, &sourceData);
    RefPtrWillBeRawPtr<MutableStylePropertySet> tempMutableStyle = MutableStylePropertySet::create();
    CSSParser::parseDeclarationList(parserContextForDocument(m_document), tempMutableStyle.get(), commentText, &handler, m_styleSheetContents);
    WillBeHeapVector<CSSPropertySourceData>& commentPropertyData = sourceData.first()->styleSourceData->propertyData;
    if (commentPropertyData.size() != 1)
        return;
    CSSPropertySourceData& propertyData = commentPropertyData.at(0);
    if (propertyData.range.length() != commentText.length())
        return;

    m_currentRuleDataStack.last()->styleSourceData->propertyData.append(
        CSSPropertySourceData(propertyData.name, propertyData.value, false, true, true, SourceRange(startOffset, offset)));
}

void StyleSheetHandler::startMediaQueryExp(unsigned offset)
{
    ASSERT(m_currentMediaQueryData);
    m_mediaQueryExpValueRangeStart = offset;
}

void StyleSheetHandler::endMediaQueryExp(unsigned offset)
{
    ASSERT(m_currentMediaQueryData);
    ASSERT(offset >= m_mediaQueryExpValueRangeStart);
    ASSERT(offset <= m_parsedText.length());
    while (offset > m_mediaQueryExpValueRangeStart && isSpaceOrNewline(m_parsedText[offset - 1]))
        --offset;
    while (offset > m_mediaQueryExpValueRangeStart && isSpaceOrNewline(m_parsedText[m_mediaQueryExpValueRangeStart]))
        ++m_mediaQueryExpValueRangeStart;
    m_currentMediaQueryData->expData.append(CSSMediaQueryExpSourceData(SourceRange(m_mediaQueryExpValueRangeStart, offset)));
}

void StyleSheetHandler::startMediaQuery()
{
    ASSERT(m_currentRuleDataStack.size() && m_currentRuleDataStack.last()->mediaSourceData);
    RefPtrWillBeRawPtr<CSSMediaQuerySourceData> data = CSSMediaQuerySourceData::create();
    m_currentMediaQueryData = data;
    m_currentRuleDataStack.last()->mediaSourceData->queryData.append(data);
}

void StyleSheetHandler::endMediaQuery()
{
    m_currentMediaQueryData.clear();
}

} // namespace

class ParsedStyleSheet {
    WTF_MAKE_FAST_ALLOCATED(ParsedStyleSheet);
public:
    ParsedStyleSheet(CSSStyleSheet* pageStyleSheet);

    const String& text() const { ASSERT(m_hasText); return m_text; }
    void setText(const String&);
    bool hasText() const { return m_hasText; }
    bool ensureSourceData();
    bool hasSourceData() const { return m_sourceData; }
    PassRefPtrWillBeRawPtr<blink::CSSRuleSourceData> ruleSourceDataAt(unsigned) const;
    unsigned ruleCount() { return m_sourceData->size(); }

private:
    void flattenSourceData(RuleSourceDataList*);
    void setSourceData(PassOwnPtrWillBeRawPtr<RuleSourceDataList>);

    String m_text;
    bool m_hasText;
    OwnPtrWillBePersistent<RuleSourceDataList> m_sourceData;
    RefPtrWillBePersistent<CSSStyleSheet> m_pageStyleSheet;
};

ParsedStyleSheet::ParsedStyleSheet(CSSStyleSheet* pageStyleSheet)
    : m_hasText(false)
    , m_pageStyleSheet(pageStyleSheet)
{
}

void ParsedStyleSheet::setText(const String& text)
{
    m_hasText = true;
    m_text = text;
    setSourceData(nullptr);
}

void ParsedStyleSheet::flattenSourceData(RuleSourceDataList* dataList)
{
    for (size_t i = 0; i < dataList->size(); ++i) {
        RefPtrWillBeMember<CSSRuleSourceData>& data = dataList->at(i);

        // The m_sourceData->append()'ed types should be exactly the same as in collectFlatRules().
        switch (data->type) {
        case StyleRule::Style:
        case StyleRule::Import:
        case StyleRule::Page:
        case StyleRule::FontFace:
        case StyleRule::Viewport:
        case StyleRule::Keyframes:
            m_sourceData->append(data);
            break;
        case StyleRule::Media:
        case StyleRule::Supports:
            m_sourceData->append(data);
            flattenSourceData(&data->childRules);
            break;
        case StyleRule::Unknown:
        default:
            break;
        }
    }
}

bool ParsedStyleSheet::ensureSourceData()
{
    if (hasSourceData())
        return true;

    if (!hasText())
        return false;

    RefPtrWillBeRawPtr<StyleSheetContents> newStyleSheet = StyleSheetContents::create(strictCSSParserContext());
    OwnPtrWillBeRawPtr<RuleSourceDataList> result = adoptPtrWillBeNoop(new RuleSourceDataList());
    StyleSheetHandler handler(text(), m_pageStyleSheet->ownerDocument(), newStyleSheet.get(), result.get());
    CSSParser::parseSheet(parserContextForDocument(m_pageStyleSheet->ownerDocument()), newStyleSheet.get(), text(), TextPosition::minimumPosition(), &handler);
    setSourceData(result.release());
    return hasSourceData();
}

void ParsedStyleSheet::setSourceData(PassOwnPtrWillBeRawPtr<RuleSourceDataList> sourceData)
{
    if (!sourceData) {
        m_sourceData.clear();
        return;
    }
    m_sourceData = adoptPtrWillBeNoop(new RuleSourceDataList());

    // FIXME: This is a temporary solution to retain the original flat sourceData structure
    // containing only style rules, even though BisonCSSParser now provides the full rule source data tree.
    // Normally, we should just assign m_sourceData = sourceData;
    flattenSourceData(sourceData.get());
}

PassRefPtrWillBeRawPtr<blink::CSSRuleSourceData> ParsedStyleSheet::ruleSourceDataAt(unsigned index) const
{
    if (!hasSourceData() || index >= m_sourceData->size())
        return nullptr;

    return m_sourceData->at(index);
}

namespace blink {

enum MediaListSource {
    MediaListSourceLinkedSheet,
    MediaListSourceInlineSheet,
    MediaListSourceMediaRule,
    MediaListSourceImportRule
};

static PassRefPtr<TypeBuilder::CSS::SourceRange> buildSourceRangeObject(const SourceRange& range, const LineEndings* lineEndings)
{
    if (!lineEndings)
        return nullptr;
    TextPosition start = TextPosition::fromOffsetAndLineEndings(range.start, *lineEndings);
    TextPosition end = TextPosition::fromOffsetAndLineEndings(range.end, *lineEndings);

    RefPtr<TypeBuilder::CSS::SourceRange> result = TypeBuilder::CSS::SourceRange::create()
        .setStartLine(start.m_line.zeroBasedInt())
        .setStartColumn(start.m_column.zeroBasedInt())
        .setEndLine(end.m_line.zeroBasedInt())
        .setEndColumn(end.m_column.zeroBasedInt());
    return result.release();
}

static PassRefPtrWillBeRawPtr<CSSRuleList> asCSSRuleList(CSSRule* rule)
{
    if (!rule)
        return nullptr;

    if (rule->type() == CSSRule::MEDIA_RULE)
        return toCSSMediaRule(rule)->cssRules();

    if (rule->type() == CSSRule::SUPPORTS_RULE)
        return toCSSSupportsRule(rule)->cssRules();

    return nullptr;
}

PassRefPtrWillBeRawPtr<InspectorStyle> InspectorStyle::create(const InspectorCSSId& styleId, PassRefPtrWillBeRawPtr<CSSStyleDeclaration> style, InspectorStyleSheetBase* parentStyleSheet)
{
    return adoptRefWillBeNoop(new InspectorStyle(styleId, style, parentStyleSheet));
}

InspectorStyle::InspectorStyle(const InspectorCSSId& styleId, PassRefPtrWillBeRawPtr<CSSStyleDeclaration> style, InspectorStyleSheetBase* parentStyleSheet)
    : m_styleId(styleId)
    , m_style(style)
    , m_parentStyleSheet(parentStyleSheet)
    , m_formatAcquired(false)
{
    ASSERT(m_style);
}

PassRefPtr<TypeBuilder::CSS::CSSStyle> InspectorStyle::buildObjectForStyle() const
{
    RefPtr<TypeBuilder::CSS::CSSStyle> result = styleWithProperties();
    if (!m_styleId.isEmpty())
        result->setStyleSheetId(m_styleId.styleSheetId());

    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = extractSourceData();
    if (sourceData)
        result->setRange(buildSourceRangeObject(sourceData->ruleBodyRange, m_parentStyleSheet->lineEndings()));

    return result.release();
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty> > InspectorStyle::buildArrayForComputedStyle() const
{
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty> > result = TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty>::create();
    WillBeHeapVector<InspectorStyleProperty> properties;
    populateAllProperties(properties);

    for (auto& property : properties) {
        const CSSPropertySourceData& propertyEntry = property.sourceData;
        RefPtr<TypeBuilder::CSS::CSSComputedStyleProperty> entry = TypeBuilder::CSS::CSSComputedStyleProperty::create()
            .setName(propertyEntry.name)
            .setValue(propertyEntry.value);
        result->addItem(entry);
    }

    return result.release();
}

bool InspectorStyle::verifyPropertyText(const String& propertyText, bool canOmitSemicolon)
{
    DEFINE_STATIC_LOCAL(String, bogusPropertyName, ("-webkit-boguz-propertee"));
    RefPtrWillBeRawPtr<MutableStylePropertySet> tempMutableStyle = MutableStylePropertySet::create();
    RuleSourceDataList sourceData;
    RefPtrWillBeRawPtr<StyleSheetContents> styleSheetContents = StyleSheetContents::create(strictCSSParserContext());
    String declarationText = propertyText + (canOmitSemicolon ? ";" : " ") + bogusPropertyName + ": none";
    StyleSheetHandler handler(declarationText, ownerDocument(), styleSheetContents.get(), &sourceData);
    CSSParser::parseDeclarationList(parserContextForDocument(ownerDocument()), tempMutableStyle.get(), declarationText, &handler, styleSheetContents.get());
    WillBeHeapVector<CSSPropertySourceData>& propertyData = sourceData.first()->styleSourceData->propertyData;
    unsigned propertyCount = propertyData.size();

    // At least one property + the bogus property added just above should be present.
    if (propertyCount < 2)
        return false;

    // Check for the proper propertyText termination (the parser could at least restore to the PROPERTY_NAME state).
    if (propertyData.at(propertyCount - 1).name != bogusPropertyName)
        return false;

    return true;
}

bool InspectorStyle::setPropertyText(unsigned index, const String& propertyText, bool overwrite, ExceptionState& exceptionState)
{
    ASSERT(m_parentStyleSheet);

    if (!m_parentStyleSheet->ensureParsedDataReady()) {
        exceptionState.throwDOMException(NotFoundError, "The parent style sheet's data hasn't been processed.");
        return false;
    }

    if (!propertyText.stripWhiteSpace().isEmpty()) {
        if (!verifyPropertyText(propertyText, false) && !verifyPropertyText(propertyText, true)) {
            exceptionState.throwDOMException(SyntaxError, "The property '" + propertyText + "' could not be set.");
            return false;
        }
    }

    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = extractSourceData();
    if (!sourceData) {
        exceptionState.throwDOMException(NotFoundError, "The property '" + propertyText + "' could not be set.");
        return false;
    }

    String text;
    bool success = styleText(&text);
    if (!success) {
        exceptionState.throwDOMException(NotFoundError, "The property '" + propertyText + "' could not be set.");
        return false;
    }

    WillBeHeapVector<InspectorStyleProperty> allProperties;
    populateAllProperties(allProperties);

    InspectorStyleTextEditor editor(&allProperties, text, sourceData->ruleBodyRange, newLineAndWhitespaceDelimiters());
    if (overwrite) {
        if (index >= allProperties.size()) {
            exceptionState.throwDOMException(IndexSizeError, "The index provided (" + String::number(index) + ") is greater than or equal to the maximum bound (" + String::number(allProperties.size()) + ").");
            return false;
        }
        editor.replaceProperty(index, propertyText);
    } else {
        editor.insertProperty(index, propertyText);
    }

    return m_parentStyleSheet->setStyleText(m_styleId, editor.styleText());
}

bool InspectorStyle::styleText(String* result) const
{
    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = extractSourceData();
    if (!sourceData)
        return false;

    return textForRange(sourceData->ruleBodyRange, result);
}

bool InspectorStyle::textForRange(const SourceRange& range, String* result) const
{
    String styleSheetText;
    bool success = m_parentStyleSheet->getText(&styleSheetText);
    if (!success)
        return false;

    ASSERT(0 <= range.start);
    ASSERT(range.start <= range.end);
    ASSERT(range.end <= styleSheetText.length());
    *result = styleSheetText.substring(range.start, range.end - range.start);
    return true;
}

void InspectorStyle::populateAllProperties(WillBeHeapVector<InspectorStyleProperty>& result) const
{
    HashSet<String> sourcePropertyNames;

    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = extractSourceData();
    if (sourceData && sourceData->styleSourceData) {
        WillBeHeapVector<CSSPropertySourceData>& sourcePropertyData = sourceData->styleSourceData->propertyData;
        for (const auto& data : sourcePropertyData) {
            InspectorStyleProperty p(data, true);
            bool isPropertyTextKnown = textForRange(p.sourceData.range, &p.rawText);
            ASSERT_UNUSED(isPropertyTextKnown, isPropertyTextKnown);
            result.append(p);
            sourcePropertyNames.add(data.name.lower());
        }
    }

    for (int i = 0, size = m_style->length(); i < size; ++i) {
        String name = m_style->item(i);
        if (!sourcePropertyNames.add(name.lower()).isNewEntry)
            continue;

        String value = m_style->getPropertyValue(name);
        if (value.isEmpty())
            continue;
        result.append(InspectorStyleProperty(CSSPropertySourceData(name, value, !m_style->getPropertyPriority(name).isEmpty(), false, true, SourceRange()), false));
    }
}

PassRefPtr<TypeBuilder::CSS::CSSStyle> InspectorStyle::styleWithProperties() const
{
    RefPtr<Array<TypeBuilder::CSS::CSSProperty> > propertiesObject = Array<TypeBuilder::CSS::CSSProperty>::create();
    RefPtr<Array<TypeBuilder::CSS::ShorthandEntry> > shorthandEntries = Array<TypeBuilder::CSS::ShorthandEntry>::create();
    HashSet<String> foundShorthands;
    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = extractSourceData();

    WillBeHeapVector<InspectorStyleProperty> properties;
    populateAllProperties(properties);

    for (auto& styleProperty : properties) {
        const CSSPropertySourceData& propertyEntry = styleProperty.sourceData;
        const String& name = propertyEntry.name;

        RefPtr<TypeBuilder::CSS::CSSProperty> property = TypeBuilder::CSS::CSSProperty::create()
            .setName(name)
            .setValue(propertyEntry.value);
        propertiesObject->addItem(property);

        // Default "parsedOk" == true.
        if (!propertyEntry.parsedOk)
            property->setParsedOk(false);
        if (styleProperty.hasRawText())
            property->setText(styleProperty.rawText);

        if (propertyEntry.important)
            property->setImportant(true);
        if (styleProperty.hasSource) {
            property->setRange(buildSourceRangeObject(propertyEntry.range, m_parentStyleSheet ? m_parentStyleSheet->lineEndings() : nullptr));
            if (!propertyEntry.disabled) {
                ASSERT_UNUSED(sourceData, sourceData);
                property->setImplicit(false);
            }
            property->setDisabled(propertyEntry.disabled);
        } else if (!propertyEntry.disabled) {
            bool implicit = m_style->isPropertyImplicit(name);
            // Default "implicit" == false.
            if (implicit)
                property->setImplicit(true);

            String shorthand = m_style->getPropertyShorthand(name);
            if (!shorthand.isEmpty()) {
                if (foundShorthands.add(shorthand).isNewEntry) {
                    RefPtr<TypeBuilder::CSS::ShorthandEntry> entry = TypeBuilder::CSS::ShorthandEntry::create()
                        .setName(shorthand)
                        .setValue(shorthandValue(shorthand));
                    shorthandEntries->addItem(entry);
                }
            }
        }
    }

    RefPtr<TypeBuilder::CSS::CSSStyle> result = TypeBuilder::CSS::CSSStyle::create()
        .setCssProperties(propertiesObject)
        .setShorthandEntries(shorthandEntries);
    return result.release();
}

PassRefPtrWillBeRawPtr<CSSRuleSourceData> InspectorStyle::extractSourceData() const
{
    if (!m_parentStyleSheet || !m_parentStyleSheet->ensureParsedDataReady())
        return nullptr;
    return m_parentStyleSheet->ruleSourceDataAt(m_styleId.ordinal());
}

String InspectorStyle::shorthandValue(const String& shorthandProperty) const
{
    String value = m_style->getPropertyValue(shorthandProperty);
    if (value.isEmpty()) {
        StringBuilder builder;

        for (unsigned i = 0; i < m_style->length(); ++i) {
            String individualProperty = m_style->item(i);
            if (m_style->getPropertyShorthand(individualProperty) != shorthandProperty)
                continue;
            if (m_style->isPropertyImplicit(individualProperty))
                continue;
            String individualValue = m_style->getPropertyValue(individualProperty);
            if (individualValue == "initial")
                continue;
            if (!builder.isEmpty())
                builder.append(' ');
            builder.append(individualValue);
        }

        return builder.toString();
    }
    return value;
}

NewLineAndWhitespace& InspectorStyle::newLineAndWhitespaceDelimiters() const
{
    DEFINE_STATIC_LOCAL(String, defaultPrefix, ("    "));

    if (m_formatAcquired)
        return m_format;

    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = extractSourceData();
    WillBeHeapVector<CSSPropertySourceData>* sourcePropertyData = sourceData ? &(sourceData->styleSourceData->propertyData) : nullptr;
    int propertyCount = sourcePropertyData ? sourcePropertyData->size() : 0;
    if (!propertyCount) {
        m_format.first = "\n";
        m_format.second = defaultPrefix;
        return m_format; // Do not remember the default formatting and attempt to acquire it later.
    }

    String styleSheetText;
    bool success = m_parentStyleSheet->getText(&styleSheetText);
    ASSERT_UNUSED(success, success);

    m_formatAcquired = true;

    String candidatePrefix = defaultPrefix;
    StringBuilder formatLineFeed;
    StringBuilder prefix;
    int scanStart = sourceData->ruleBodyRange.start;
    int propertyIndex = 0;
    bool isFullPrefixScanned = false;
    bool lineFeedTerminated = false;
    while (propertyIndex < propertyCount) {
        const blink::CSSPropertySourceData& currentProperty = sourcePropertyData->at(propertyIndex++);

        bool processNextProperty = false;
        int scanEnd = currentProperty.range.start;
        for (int i = scanStart; i < scanEnd; ++i) {
            UChar ch = styleSheetText[i];
            bool isLineFeed = isHTMLLineBreak(ch);
            if (isLineFeed) {
                if (!lineFeedTerminated)
                    formatLineFeed.append(ch);
                prefix.clear();
            } else if (isHTMLSpace<UChar>(ch))
                prefix.append(ch);
            else {
                candidatePrefix = prefix.toString();
                prefix.clear();
                scanStart = currentProperty.range.end;
                ++propertyIndex;
                processNextProperty = true;
                break;
            }
            if (!isLineFeed && formatLineFeed.length())
                lineFeedTerminated = true;
        }
        if (!processNextProperty) {
            isFullPrefixScanned = true;
            break;
        }
    }

    m_format.first = formatLineFeed.toString();
    m_format.second = isFullPrefixScanned ? prefix.toString() : candidatePrefix;
    return m_format;
}

Document* InspectorStyle::ownerDocument() const
{
    return m_parentStyleSheet->ownerDocument();
}

DEFINE_TRACE(InspectorStyle)
{
    visitor->trace(m_style);
    visitor->trace(m_parentStyleSheet);
}

InspectorStyleSheetBase::InspectorStyleSheetBase(const String& id, Listener* listener)
    : m_id(id)
    , m_listener(listener)
    , m_lineEndings(adoptPtr(new LineEndings()))
{
}

bool InspectorStyleSheetBase::setPropertyText(const InspectorCSSId& id, unsigned propertyIndex, const String& text, bool overwrite, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<InspectorStyle> inspectorStyle = inspectorStyleForId(id);
    if (!inspectorStyle) {
        exceptionState.throwDOMException(NotFoundError, "No property could be found for the given ID.");
        return false;
    }
    return inspectorStyle->setPropertyText(propertyIndex, text, overwrite, exceptionState);
}

bool InspectorStyleSheetBase::getStyleText(const InspectorCSSId& id, String* text)
{
    RefPtrWillBeRawPtr<InspectorStyle> inspectorStyle = inspectorStyleForId(id);
    if (!inspectorStyle)
        return false;
    return inspectorStyle->styleText(text);
}

void InspectorStyleSheetBase::onStyleSheetTextChanged()
{
    m_lineEndings = adoptPtr(new LineEndings());
    if (listener())
        listener()->styleSheetChanged(this);
}

PassRefPtr<TypeBuilder::CSS::CSSStyle> InspectorStyleSheetBase::buildObjectForStyle(CSSStyleDeclaration* style)
{
    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = nullptr;
    if (ensureParsedDataReady())
        sourceData = ruleSourceDataAt(styleId(style).ordinal());

    InspectorCSSId id = styleId(style);
    if (id.isEmpty()) {
        // Any rule coming from User Agent and not from DefaultStyleSheet will not have id.
        // See InspectorCSSAgent::buildObjectForRule for details.
        RefPtrWillBeRawPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(id, style, this);
        return inspectorStyle->buildObjectForStyle();
    }
    RefPtrWillBeRawPtr<InspectorStyle> inspectorStyle = inspectorStyleForId(id);
    RefPtr<TypeBuilder::CSS::CSSStyle> result = inspectorStyle->buildObjectForStyle();

    // Style text cannot be retrieved without stylesheet, so set cssText here.
    if (sourceData) {
        String sheetText;
        bool success = getText(&sheetText);
        if (success) {
            const SourceRange& bodyRange = sourceData->ruleBodyRange;
            result->setCssText(sheetText.substring(bodyRange.start, bodyRange.end - bodyRange.start));
        }
    }

    return result.release();
}

const LineEndings* InspectorStyleSheetBase::lineEndings()
{
    if (m_lineEndings->size() > 0)
        return m_lineEndings.get();
    String text;
    if (getText(&text))
        m_lineEndings = WTF::lineEndings(text);
    return m_lineEndings.get();
}

bool InspectorStyleSheetBase::lineNumberAndColumnToOffset(unsigned lineNumber, unsigned columnNumber, unsigned* offset)
{
    const LineEndings* endings = lineEndings();
    if (lineNumber >= endings->size())
        return false;
    unsigned charactersInLine = lineNumber > 0 ? endings->at(lineNumber) - endings->at(lineNumber - 1) - 1 : endings->at(0);
    if (columnNumber > charactersInLine)
        return false;
    TextPosition position(OrdinalNumber::fromZeroBasedInt(lineNumber), OrdinalNumber::fromZeroBasedInt(columnNumber));
    *offset = position.toOffset(*endings).zeroBasedInt();
    return true;
}

bool InspectorStyleSheetBase::findPropertyByRange(const SourceRange& sourceRange, InspectorCSSId* ruleId, unsigned* propertyIndex, bool* overwrite)
{
    if (!ensureParsedDataReady())
        return false;
    for (size_t i = 0; i < ruleCount(); ++i) {
        RefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceData = ruleSourceDataAt(i);
        RefPtrWillBeRawPtr<CSSStyleSourceData> styleSourceData = ruleSourceData->styleSourceData;
        if (!styleSourceData)
            continue;
        if (ruleSourceData->ruleBodyRange.end < sourceRange.start || sourceRange.end < ruleSourceData->ruleBodyRange.start)
            continue;
        WillBeHeapVector<CSSPropertySourceData>& propertyData = styleSourceData->propertyData;
        for (size_t j = 0; j < propertyData.size(); ++j) {
            CSSPropertySourceData& property = propertyData.at(j);
            unsigned styleStart = ruleSourceData->ruleBodyRange.start;
            if (sourceRange.length() && property.range.start == sourceRange.start && property.range.end == sourceRange.end) {
                *ruleId = InspectorCSSId(id(), i);
                *propertyIndex = j;
                *overwrite = true;
                return true;
            }
            if (!sourceRange.length() && styleStart <= sourceRange.start && sourceRange.start <= property.range.start) {
                *ruleId = InspectorCSSId(id(), i);
                *propertyIndex = j;
                *overwrite = false;
                return true;
            }
        }
        if (!sourceRange.length() && ruleSourceData->ruleBodyRange.start <= sourceRange.start && sourceRange.start <= ruleSourceData->ruleBodyRange.end) {
            *ruleId = InspectorCSSId(id(), i);
            *propertyIndex = propertyData.size();
            *overwrite = false;
            return true;
        }
    }
    return false;
}

PassRefPtrWillBeRawPtr<InspectorStyleSheet> InspectorStyleSheet::create(InspectorResourceAgent* resourceAgent, const String& id, PassRefPtrWillBeRawPtr<CSSStyleSheet> pageStyleSheet, TypeBuilder::CSS::StyleSheetOrigin::Enum origin, const String& documentURL, InspectorCSSAgent* cssAgent)
{
    return adoptRefWillBeNoop(new InspectorStyleSheet(resourceAgent, id, pageStyleSheet, origin, documentURL, cssAgent));
}

InspectorStyleSheet::InspectorStyleSheet(InspectorResourceAgent* resourceAgent, const String& id, PassRefPtrWillBeRawPtr<CSSStyleSheet> pageStyleSheet, TypeBuilder::CSS::StyleSheetOrigin::Enum origin, const String& documentURL, InspectorCSSAgent* cssAgent)
    : InspectorStyleSheetBase(id, cssAgent)
    , m_cssAgent(cssAgent)
    , m_resourceAgent(resourceAgent)
    , m_pageStyleSheet(pageStyleSheet)
    , m_origin(origin)
    , m_documentURL(documentURL)
{
    m_parsedStyleSheet = adoptPtr(new ParsedStyleSheet(m_pageStyleSheet.get()));
}

InspectorStyleSheet::~InspectorStyleSheet()
{
}

DEFINE_TRACE(InspectorStyleSheet)
{
    visitor->trace(m_cssAgent);
    visitor->trace(m_resourceAgent);
    visitor->trace(m_pageStyleSheet);
    visitor->trace(m_flatRules);
    InspectorStyleSheetBase::trace(visitor);
}

static String styleSheetURL(CSSStyleSheet* pageStyleSheet)
{
    if (pageStyleSheet && !pageStyleSheet->contents()->baseURL().isEmpty())
        return pageStyleSheet->contents()->baseURL().string();
    return emptyString();
}

String InspectorStyleSheet::finalURL() const
{
    String url = styleSheetURL(m_pageStyleSheet.get());
    return url.isEmpty() ? m_documentURL : url;
}

bool InspectorStyleSheet::setText(const String& text, ExceptionState& exceptionState)
{
    updateText(text);
    m_flatRules.clear();

    if (listener())
        listener()->willReparseStyleSheet();

    {
        // Have a separate scope for clearRules() (bug 95324).
        CSSStyleSheet::RuleMutationScope mutationScope(m_pageStyleSheet.get());
        m_pageStyleSheet->contents()->clearRules();
        m_pageStyleSheet->clearChildRuleCSSOMWrappers();
    }
    {
        CSSStyleSheet::RuleMutationScope mutationScope(m_pageStyleSheet.get());
        m_pageStyleSheet->contents()->parseString(text);
    }

    if (listener())
        listener()->didReparseStyleSheet();
    onStyleSheetTextChanged();
    m_pageStyleSheet->ownerDocument()->styleResolverChanged(FullStyleUpdate);
    return true;
}

String InspectorStyleSheet::ruleSelector(const InspectorCSSId& id, ExceptionState& exceptionState)
{
    CSSStyleRule* rule = ruleForId(id);
    if (!rule) {
        exceptionState.throwDOMException(NotFoundError, "No rule was found for the given ID.");
        return "";
    }
    return rule->selectorText();
}

bool InspectorStyleSheet::setRuleSelector(const InspectorCSSId& id, const String& selector, ExceptionState& exceptionState)
{
    CSSStyleRule* rule = ruleForId(id);
    if (!rule) {
        exceptionState.throwDOMException(NotFoundError, "No rule was found for the given ID.");
        return false;
    }
    CSSStyleSheet* styleSheet = rule->parentStyleSheet();
    if (!styleSheet || !ensureParsedDataReady()) {
        exceptionState.throwDOMException(NotFoundError, "No stylesheet could be found in which to set the selector.");
        return false;
    }

    if (!verifySelectorText(selector)) {
        exceptionState.throwDOMException(SyntaxError, "Selector text is not valid.");
        return false;
    }

    rule->setSelectorText(selector);
    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = ruleSourceDataAt(id.ordinal());
    ASSERT(sourceData);

    String sheetText = m_parsedStyleSheet->text();
    sheetText.replace(sourceData->ruleHeaderRange.start, sourceData->ruleHeaderRange.length(), selector);
    updateText(sheetText);
    onStyleSheetTextChanged();
    return true;
}

String InspectorStyleSheet::mediaRuleText(const InspectorCSSId& id, ExceptionState& exceptionState)
{
    CSSMediaRule* rule = mediaRuleForId(id);
    if (!rule) {
        exceptionState.throwDOMException(NotFoundError, "No media rule was found for the given ID.");
        return "";
    }
    return rule->media()->mediaText();
}

bool InspectorStyleSheet::setMediaRuleText(const InspectorCSSId& id, const String& text, ExceptionState& exceptionState)
{
    CSSMediaRule* rule = mediaRuleForId(id);
    if (!rule) {
        exceptionState.throwDOMException(NotFoundError, "No media rule was found for the given ID.");
        return false;
    }
    CSSStyleSheet* styleSheet = rule->parentStyleSheet();
    if (!styleSheet || !ensureParsedDataReady()) {
        exceptionState.throwDOMException(NotFoundError, "No stylesheet could be found in which to set the media text.");
        return false;
    }
    if (!verifyMediaText(text)) {
        exceptionState.throwDOMException(SyntaxError, "Media text is not valid.");
        return false;
    }

    rule->media()->setMediaText(text);
    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = ruleSourceDataAt(id.ordinal());
    ASSERT(sourceData && sourceData->mediaSourceData);

    String sheetText = m_parsedStyleSheet->text();
    sheetText.replace(sourceData->ruleHeaderRange.start, sourceData->ruleHeaderRange.length(), text);
    updateText(sheetText);
    onStyleSheetTextChanged();
    return true;
}

unsigned InspectorStyleSheet::ruleIndexBySourceRange(const CSSMediaRule* parentMediaRule, const SourceRange& sourceRange)
{
    unsigned index = 0;
    for (size_t i = 0; i < m_flatRules.size(); ++i) {
        RefPtrWillBeRawPtr<CSSRule> rule = m_flatRules.at(i);
        if (rule->parentRule() != parentMediaRule)
            continue;
        RefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceData = m_parsedStyleSheet->ruleSourceDataAt(i);
        if (ruleSourceData->ruleBodyRange.end < sourceRange.start)
            ++index;
    }
    return index;
}

CSSStyleRule* InspectorStyleSheet::insertCSSOMRuleInStyleSheet(const SourceRange& sourceRange, const String& ruleText, ExceptionState& exceptionState)
{
    unsigned index = ruleIndexBySourceRange(nullptr, sourceRange);
    m_pageStyleSheet->insertRule(ruleText, index, exceptionState);
    CSSRule* rule = m_pageStyleSheet->item(index);
    CSSStyleRule* styleRule = InspectorCSSAgent::asCSSStyleRule(rule);
    if (!styleRule) {
        m_pageStyleSheet->deleteRule(index, ASSERT_NO_EXCEPTION);
        exceptionState.throwDOMException(SyntaxError, "The rule '" + ruleText + "' could not be added in style sheet.");
        return nullptr;
    }
    return styleRule;
}

CSSStyleRule* InspectorStyleSheet::insertCSSOMRuleInMediaRule(CSSMediaRule* mediaRule, const SourceRange& sourceRange, const String& ruleText, ExceptionState& exceptionState)
{
    unsigned index = ruleIndexBySourceRange(mediaRule, sourceRange);
    mediaRule->insertRule(ruleText, index, exceptionState);
    CSSRule* rule = mediaRule->item(index);
    CSSStyleRule* styleRule = InspectorCSSAgent::asCSSStyleRule(rule);
    if (!styleRule) {
        mediaRule->deleteRule(index, ASSERT_NO_EXCEPTION);
        exceptionState.throwDOMException(SyntaxError, "The rule '" + ruleText + "' could not be added in media rule.");
        return nullptr;
    }
    return styleRule;
}

CSSStyleRule* InspectorStyleSheet::insertCSSOMRuleBySourceRange(const SourceRange& sourceRange, const String& ruleText, ExceptionState& exceptionState)
{
    int containingRuleIndex = -1;
    unsigned containingRuleLength = 0;
    for (size_t i = 0; i < m_parsedStyleSheet->ruleCount(); ++i) {
        RefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceData = m_parsedStyleSheet->ruleSourceDataAt(i);
        if (ruleSourceData->ruleHeaderRange.start < sourceRange.start && sourceRange.start < ruleSourceData->ruleBodyRange.start) {
            exceptionState.throwDOMException(NotFoundError, "Cannot insert rule inside rule selector.");
            return nullptr;
        }
        if (sourceRange.start < ruleSourceData->ruleBodyRange.start || ruleSourceData->ruleBodyRange.end < sourceRange.start)
            continue;
        if (containingRuleIndex == -1 || containingRuleLength > ruleSourceData->ruleBodyRange.length()) {
            containingRuleIndex = i;
            containingRuleLength = ruleSourceData->ruleBodyRange.length();
        }
    }
    if (containingRuleIndex == -1)
        return insertCSSOMRuleInStyleSheet(sourceRange, ruleText, exceptionState);
    RefPtrWillBeRawPtr<CSSRule> rule = m_flatRules.at(containingRuleIndex);
    if (rule->type() != CSSRule::MEDIA_RULE) {
        exceptionState.throwDOMException(NotFoundError, "Cannot insert rule in non-media rule.");
        return nullptr;
    }
    return insertCSSOMRuleInMediaRule(toCSSMediaRule(rule.get()), sourceRange, ruleText, exceptionState);
}

bool InspectorStyleSheet::verifyRuleText(const String& ruleText)
{
    DEFINE_STATIC_LOCAL(String, bogusPropertyName, ("-webkit-boguz-propertee"));
    RuleSourceDataList sourceData;
    RefPtrWillBeRawPtr<StyleSheetContents> styleSheetContents = StyleSheetContents::create(strictCSSParserContext());
    String text = ruleText + " div { " + bogusPropertyName + ": none; }";
    StyleSheetHandler handler(text, ownerDocument(), styleSheetContents.get(), &sourceData);
    CSSParser::parseSheet(parserContextForDocument(ownerDocument()), styleSheetContents.get(), text, TextPosition::minimumPosition(), &handler);
    unsigned ruleCount = sourceData.size();

    // Exactly two rules should be parsed.
    if (ruleCount != 2)
        return false;

    // Added rule must be style rule.
    if (!sourceData.at(0)->styleSourceData)
        return false;

    WillBeHeapVector<CSSPropertySourceData>& propertyData = sourceData.at(1)->styleSourceData->propertyData;
    unsigned propertyCount = propertyData.size();

    // Exactly one property should be in rule.
    if (propertyCount != 1)
        return false;

    // Check for the property name.
    if (propertyData.at(0).name != bogusPropertyName)
        return false;

    return true;
}

bool InspectorStyleSheet::verifySelectorText(const String& selectorText)
{
    DEFINE_STATIC_LOCAL(String, bogusPropertyName, ("-webkit-boguz-propertee"));
    RuleSourceDataList sourceData;
    RefPtrWillBeRawPtr<StyleSheetContents> styleSheetContents = StyleSheetContents::create(strictCSSParserContext());
    String text = selectorText + " { " + bogusPropertyName + ": none; }";
    StyleSheetHandler handler(text, ownerDocument(), styleSheetContents.get(), &sourceData);
    CSSParser::parseSheet(parserContextForDocument(ownerDocument()), styleSheetContents.get(), text, TextPosition::minimumPosition(), &handler);

    // Exactly one rule should be parsed.
    unsigned ruleCount = sourceData.size();
    if (ruleCount != 1 || sourceData.at(0)->type != StyleRule::Style)
        return false;

    // Exactly one property should be in style rule.
    WillBeHeapVector<CSSPropertySourceData>& propertyData = sourceData.at(0)->styleSourceData->propertyData;
    unsigned propertyCount = propertyData.size();
    if (propertyCount != 1)
        return false;

    // Check for the property name.
    if (propertyData.at(0).name != bogusPropertyName)
        return false;

    return true;
}

bool InspectorStyleSheet::verifyMediaText(const String& mediaText)
{
    DEFINE_STATIC_LOCAL(String, bogusPropertyName, ("-webkit-boguz-propertee"));
    RuleSourceDataList sourceData;
    RefPtrWillBeRawPtr<StyleSheetContents> styleSheetContents = StyleSheetContents::create(strictCSSParserContext());
    String text = "@media " + mediaText + " { div { " + bogusPropertyName + ": none; } }";
    StyleSheetHandler handler(text, ownerDocument(), styleSheetContents.get(), &sourceData);
    CSSParser::parseSheet(parserContextForDocument(ownerDocument()), styleSheetContents.get(), text, TextPosition::minimumPosition(), &handler);

    // Exactly one media rule should be parsed.
    unsigned ruleCount = sourceData.size();
    if (ruleCount != 1 || sourceData.at(0)->type != StyleRule::Media)
        return false;

    // Media rule should have exactly one style rule child.
    RuleSourceDataList& childSourceData = sourceData.at(0)->childRules;
    ruleCount = childSourceData.size();
    if (ruleCount != 1 || !childSourceData.at(0)->styleSourceData)
        return false;

    // Exactly one property should be in style rule.
    WillBeHeapVector<CSSPropertySourceData>& propertyData = childSourceData.at(0)->styleSourceData->propertyData;
    unsigned propertyCount = propertyData.size();
    if (propertyCount != 1)
        return false;

    // Check for the property name.
    if (propertyData.at(0).name != bogusPropertyName)
        return false;

    return true;
}

CSSStyleRule* InspectorStyleSheet::addRule(const String& ruleText, const SourceRange& location, ExceptionState& exceptionState)
{
    if (!ensureParsedDataReady()) {
        exceptionState.throwDOMException(NotFoundError, "Cannot parse style sheet.");
        return nullptr;
    }

    if (location.start != location.end) {
        exceptionState.throwDOMException(NotFoundError, "Source range must be collapsed.");
        return nullptr;
    }

    if (!verifyRuleText(ruleText)) {
        exceptionState.throwDOMException(SyntaxError, "Rule text is not valid.");
        return nullptr;
    }

    String text;
    bool success = getText(&text);
    if (!success) {
        exceptionState.throwDOMException(NotFoundError, "The rule '" + ruleText + "' could not be added.");
        return nullptr;
    }

    ensureFlatRules();
    CSSStyleRule* styleRule = insertCSSOMRuleBySourceRange(location, ruleText, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    text.insert(ruleText, location.start);

    updateText(text);
    m_flatRules.clear();

    onStyleSheetTextChanged();
    return styleRule;
}

bool InspectorStyleSheet::deleteRule(const InspectorCSSId& id, const String& oldText, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<CSSStyleRule> rule = ruleForId(id);
    if (!rule) {
        exceptionState.throwDOMException(NotFoundError, "No style rule could be found for the provided ID.");
        return false;
    }
    CSSStyleSheet* styleSheet = rule->parentStyleSheet();
    if (!styleSheet || !ensureParsedDataReady()) {
        exceptionState.throwDOMException(NotFoundError, "No parent stylesheet could be found.");
        return false;
    }

    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = ruleSourceDataAt(id.ordinal());
    if (!sourceData) {
        exceptionState.throwDOMException(NotFoundError, "No style rule could be found for the provided ID.");
        return false;
    }

    CSSRule* parentRule = rule->parentRule();
    if (parentRule) {
        if (parentRule->type() != CSSRule::MEDIA_RULE) {
            exceptionState.throwDOMException(NotFoundError, "Cannot remove rule from non-media rule.");
            return false;
        }
        CSSMediaRule* parentMediaRule = toCSSMediaRule(parentRule);
        size_t index = 0;
        while (index < parentMediaRule->length() && parentMediaRule->item(index) != rule)
            ++index;
        ASSERT(index < parentMediaRule->length());
        parentMediaRule->deleteRule(index, exceptionState);
    } else {
        size_t index = 0;
        while (index < styleSheet->length() && styleSheet->item(index) != rule)
            ++index;
        ASSERT(index < styleSheet->length());
        styleSheet->deleteRule(index, exceptionState);
    }
    // |rule| MAY NOT be addressed after this line!

    if (exceptionState.hadException())
        return false;

    updateText(oldText);
    m_flatRules.clear();
    onStyleSheetTextChanged();
    return true;
}

void InspectorStyleSheet::updateText(const String& newText)
{
    Element* element = ownerStyleElement();
    if (element)
        m_cssAgent->addEditedStyleElement(DOMNodeIds::idForNode(element), newText);
    else
        m_cssAgent->addEditedStyleSheet(finalURL(), newText);
    m_parsedStyleSheet->setText(newText);
}

CSSStyleRule* InspectorStyleSheet::ruleForId(const InspectorCSSId& id) const
{
    ASSERT(!id.isEmpty());
    ensureFlatRules();
    return InspectorCSSAgent::asCSSStyleRule(id.ordinal() >= m_flatRules.size() ? nullptr : m_flatRules.at(id.ordinal()).get());
}

CSSMediaRule* InspectorStyleSheet::mediaRuleForId(const InspectorCSSId& id) const
{
    ASSERT(!id.isEmpty());
    ensureFlatRules();
    return InspectorCSSAgent::asCSSMediaRule(id.ordinal() >= m_flatRules.size() ? nullptr : m_flatRules.at(id.ordinal()).get());
}

PassRefPtr<TypeBuilder::CSS::CSSStyleSheetHeader> InspectorStyleSheet::buildObjectForStyleSheetInfo() const
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return nullptr;

    Document* document = styleSheet->ownerDocument();
    LocalFrame* frame = document ? document->frame() : nullptr;

    RefPtr<TypeBuilder::CSS::CSSStyleSheetHeader> result = TypeBuilder::CSS::CSSStyleSheetHeader::create()
        .setStyleSheetId(id())
        .setOrigin(m_origin)
        .setDisabled(styleSheet->disabled())
        .setSourceURL(url())
        .setTitle(styleSheet->title())
        .setFrameId(frame ? InspectorIdentifiers<LocalFrame>::identifier(frame) : "")
        .setIsInline(styleSheet->isInline() && !startsAtZero())
        .setStartLine(styleSheet->startPositionInSource().m_line.zeroBasedInt())
        .setStartColumn(styleSheet->startPositionInSource().m_column.zeroBasedInt());

    if (hasSourceURL())
        result->setHasSourceURL(true);

    if (styleSheet->ownerNode())
        result->setOwnerNode(DOMNodeIds::idForNode(styleSheet->ownerNode()));

    String sourceMapURLValue = sourceMapURL();
    if (!sourceMapURLValue.isEmpty())
        result->setSourceMapURL(sourceMapURLValue);
    return result.release();
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::Selector> > InspectorStyleSheet::selectorsFromSource(const CSSRuleSourceData* sourceData, const String& sheetText)
{
    ScriptRegexp comment("/\\*[^]*?\\*/", TextCaseSensitive, MultilineEnabled);
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::Selector> > result = TypeBuilder::Array<TypeBuilder::CSS::Selector>::create();
    const SelectorRangeList& ranges = sourceData->selectorRanges;
    for (size_t i = 0, size = ranges.size(); i < size; ++i) {
        const SourceRange& range = ranges.at(i);
        String selector = sheetText.substring(range.start, range.length());

        // We don't want to see any comments in the selector components, only the meaningful parts.
        int matchLength;
        int offset = 0;
        while ((offset = comment.match(selector, offset, &matchLength)) >= 0)
            selector.replace(offset, matchLength, "");

        RefPtr<TypeBuilder::CSS::Selector> simpleSelector = TypeBuilder::CSS::Selector::create()
            .setValue(selector.stripWhiteSpace());
        simpleSelector->setRange(buildSourceRangeObject(range, lineEndings()));
        result->addItem(simpleSelector.release());
    }
    return result.release();
}

PassRefPtr<TypeBuilder::CSS::SelectorList> InspectorStyleSheet::buildObjectForSelectorList(CSSStyleRule* rule)
{
    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = nullptr;
    if (ensureParsedDataReady())
        sourceData = ruleSourceDataAt(styleId(rule->style()).ordinal());
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::Selector> > selectors;

    // This intentionally does not rely on the source data to avoid catching the trailing comments (before the declaration starting '{').
    String selectorText = rule->selectorText();

    if (sourceData)
        selectors = selectorsFromSource(sourceData.get(), m_parsedStyleSheet->text());
    else {
        selectors = TypeBuilder::Array<TypeBuilder::CSS::Selector>::create();
        const CSSSelectorList& selectorList = rule->styleRule()->selectorList();
        for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(*selector))
            selectors->addItem(TypeBuilder::CSS::Selector::create().setValue(selector->selectorText()).release());
    }
    RefPtr<TypeBuilder::CSS::SelectorList> result = TypeBuilder::CSS::SelectorList::create()
        .setSelectors(selectors)
        .setText(selectorText)
        .release();
    return result.release();
}

static bool canBind(TypeBuilder::CSS::StyleSheetOrigin::Enum origin)
{
    return origin != TypeBuilder::CSS::StyleSheetOrigin::User_agent && origin != TypeBuilder::CSS::StyleSheetOrigin::Injected;
}

PassRefPtr<TypeBuilder::CSS::CSSRule> InspectorStyleSheet::buildObjectForRule(CSSStyleRule* rule, PassRefPtr<Array<TypeBuilder::CSS::CSSMedia> > mediaStack)
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return nullptr;

    RefPtr<TypeBuilder::CSS::CSSRule> result = TypeBuilder::CSS::CSSRule::create()
        .setSelectorList(buildObjectForSelectorList(rule))
        .setOrigin(m_origin)
        .setStyle(buildObjectForStyle(rule->style()));

    if (canBind(m_origin)) {
        InspectorCSSId id(ruleId(rule));
        if (!id.isEmpty())
            result->setStyleSheetId(id.styleSheetId());
    }

    if (mediaStack)
        result->setMedia(mediaStack);

    return result.release();
}

bool InspectorStyleSheet::getText(String* result) const
{
    if (!ensureText())
        return false;
    *result = m_parsedStyleSheet->text();
    return true;
}

CSSStyleDeclaration* InspectorStyleSheet::styleForId(const InspectorCSSId& id) const
{
    CSSStyleRule* rule = ruleForId(id);
    if (!rule)
        return nullptr;

    return rule->style();
}

PassRefPtr<TypeBuilder::CSS::SourceRange> InspectorStyleSheet::ruleHeaderSourceRange(const CSSRule* rule)
{
    if (!ensureParsedDataReady())
        return nullptr;

    ensureFlatRules();
    size_t index = m_flatRules.find(rule);
    // FIXME(lusnikov): m_flatRules are not always aligned with the m_parsedStyleSheet rule source
    // datas due to the CSSOM operations that add/remove rules without changing source.
    // This is a design issue. See crbug.com/178410
    if (index == kNotFound || index >= m_parsedStyleSheet->ruleCount())
        return nullptr;
    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = m_parsedStyleSheet->ruleSourceDataAt(static_cast<unsigned>(index));
    return buildSourceRangeObject(sourceData->ruleHeaderRange, lineEndings());
}

PassRefPtr<TypeBuilder::CSS::SourceRange> InspectorStyleSheet::mediaQueryExpValueSourceRange(const CSSRule* rule, size_t mediaQueryIndex, size_t mediaQueryExpIndex)
{
    if (!ensureParsedDataReady())
        return nullptr;
    ensureFlatRules();
    size_t index = m_flatRules.find(rule);
    if (index == kNotFound || index >= m_parsedStyleSheet->ruleCount())
        return nullptr;
    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = m_parsedStyleSheet->ruleSourceDataAt(static_cast<unsigned>(index));
    if (!sourceData->mediaSourceData || mediaQueryIndex >= sourceData->mediaSourceData->queryData.size())
        return nullptr;
    RefPtrWillBeRawPtr<CSSMediaQuerySourceData> mediaQueryData = sourceData->mediaSourceData->queryData.at(mediaQueryIndex);
    if (mediaQueryExpIndex >= mediaQueryData->expData.size())
        return nullptr;
    return buildSourceRangeObject(mediaQueryData->expData.at(mediaQueryExpIndex).valueRange, lineEndings());
}

PassRefPtrWillBeRawPtr<InspectorStyle> InspectorStyleSheet::inspectorStyleForId(const InspectorCSSId& id)
{
    CSSStyleDeclaration* style = styleForId(id);
    if (!style)
        return nullptr;

    return InspectorStyle::create(id, style, this);
}

unsigned InspectorStyleSheet::ruleCount()
{
    return m_parsedStyleSheet->ruleCount();
}

String InspectorStyleSheet::sourceURL() const
{
    if (!m_sourceURL.isNull())
        return m_sourceURL;
    if (m_origin != TypeBuilder::CSS::StyleSheetOrigin::Regular) {
        m_sourceURL = "";
        return m_sourceURL;
    }

    String styleSheetText;
    bool success = getText(&styleSheetText);
    if (success) {
        bool deprecated;
        String commentValue = ContentSearchUtils::findSourceURL(styleSheetText, ContentSearchUtils::CSSMagicComment, &deprecated);
        if (!commentValue.isEmpty()) {
            // FIXME: add deprecated console message here.
            m_sourceURL = commentValue;
            return commentValue;
        }
    }
    m_sourceURL = "";
    return m_sourceURL;
}

String InspectorStyleSheet::url() const
{
    // "sourceURL" is present only for regular rules, otherwise "origin" should be used in the frontend.
    if (m_origin != TypeBuilder::CSS::StyleSheetOrigin::Regular)
        return String();

    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return String();

    if (hasSourceURL())
        return sourceURL();

    if (styleSheet->isInline() && startsAtZero())
        return String();

    return finalURL();
}

bool InspectorStyleSheet::hasSourceURL() const
{
    return !sourceURL().isEmpty();
}

bool InspectorStyleSheet::startsAtZero() const
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return true;

    return styleSheet->startPositionInSource() == TextPosition::minimumPosition();
}

String InspectorStyleSheet::sourceMapURL() const
{
    if (m_origin != TypeBuilder::CSS::StyleSheetOrigin::Regular)
        return String();

    String styleSheetText;
    bool success = getText(&styleSheetText);
    if (success) {
        bool deprecated;
        String commentValue = ContentSearchUtils::findSourceMapURL(styleSheetText, ContentSearchUtils::CSSMagicComment, &deprecated);
        if (!commentValue.isEmpty()) {
            // FIXME: add deprecated console message here.
            return commentValue;
        }
    }
    return m_pageStyleSheet->contents()->sourceMapURL();
}

InspectorCSSId InspectorStyleSheet::styleId(CSSStyleDeclaration* style) const
{
    unsigned index = ruleIndexByStyle(style);
    if (index != UINT_MAX)
        return InspectorCSSId(id(), index);
    return InspectorCSSId();
}

bool InspectorStyleSheet::findRuleBySelectorRange(const SourceRange& sourceRange, InspectorCSSId* ruleId)
{
    if (!ensureParsedDataReady())
        return false;
    for (size_t i = 0; i < ruleCount(); ++i) {
        RefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceData = ruleSourceDataAt(i);
        if (!ruleSourceData->styleSourceData)
            continue;
        if (ruleSourceData->ruleHeaderRange.start == sourceRange.start && ruleSourceData->ruleHeaderRange.end == sourceRange.end) {
            *ruleId = InspectorCSSId(id(), i);
            return true;
        }
    }
    return false;
}

bool InspectorStyleSheet::findMediaRuleByRange(const SourceRange& sourceRange, InspectorCSSId* ruleId)
{
    if (!ensureParsedDataReady())
        return false;
    for (size_t i = 0; i < ruleCount(); ++i) {
        RefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceData = ruleSourceDataAt(i);
        if (!ruleSourceData->mediaSourceData)
            continue;
        if (ruleSourceData->ruleHeaderRange.start == sourceRange.start && ruleSourceData->ruleHeaderRange.end == sourceRange.end) {
            *ruleId = InspectorCSSId(id(), i);
            return true;
        }
    }
    return false;
}

const CSSRuleVector& InspectorStyleSheet::flatRules()
{
    ensureFlatRules();
    return m_flatRules;
}

Document* InspectorStyleSheet::ownerDocument() const
{
    return m_pageStyleSheet->ownerDocument();
}

PassRefPtrWillBeRawPtr<CSSRuleSourceData> InspectorStyleSheet::ruleSourceDataAt(unsigned ruleIndex) const
{
    return m_parsedStyleSheet->ruleSourceDataAt(ruleIndex);
}

unsigned InspectorStyleSheet::ruleIndexByStyle(CSSStyleDeclaration* pageStyle) const
{
    ensureFlatRules();
    for (unsigned i = 0, size = m_flatRules.size(); i < size; ++i) {
        CSSStyleRule* styleRule = InspectorCSSAgent::asCSSStyleRule(m_flatRules.at(i).get());
        if (styleRule && styleRule->style() == pageStyle)
            return i;
    }
    return UINT_MAX;
}

bool InspectorStyleSheet::ensureParsedDataReady()
{
    return ensureText() && m_parsedStyleSheet->ensureSourceData();
}

bool InspectorStyleSheet::ensureText() const
{
    if (m_parsedStyleSheet->hasText())
        return true;

    String text;
    bool success = originalStyleSheetText(&text);
    if (success)
        m_parsedStyleSheet->setText(text);
    // No need to clear m_flatRules here - it's empty.

    return success;
}

template <typename RuleList>
static void collectFlatRules(RuleList ruleList, CSSRuleVector* result)
{
    if (!ruleList)
        return;

    for (unsigned i = 0, size = ruleList->length(); i < size; ++i) {
        CSSRule* rule = ruleList->item(i);

        // The result->append()'ed types should be exactly the same as in ParsedStyleSheet::flattenSourceData().
        switch (rule->type()) {
        case CSSRule::STYLE_RULE:
        case CSSRule::IMPORT_RULE:
        case CSSRule::CHARSET_RULE:
        case CSSRule::PAGE_RULE:
        case CSSRule::FONT_FACE_RULE:
        case CSSRule::VIEWPORT_RULE:
        case CSSRule::KEYFRAMES_RULE:
            result->append(rule);
            break;
        case CSSRule::MEDIA_RULE:
        case CSSRule::SUPPORTS_RULE:
            result->append(rule);
            collectFlatRules(asCSSRuleList(rule), result);
            break;
        default:
            break;
        }
    }
}

void InspectorStyleSheet::ensureFlatRules() const
{
    // We are fine with redoing this for empty stylesheets as this will run fast.
    if (m_flatRules.isEmpty())
        collectFlatRules(pageStyleSheet(), &m_flatRules);
}

bool InspectorStyleSheet::setStyleText(const InspectorCSSId& id, const String& text)
{
    CSSStyleDeclaration* style = styleForId(id);
    if (!style)
        return false;

    if (!ensureParsedDataReady())
        return false;

    String patchedStyleSheetText;
    bool success = styleSheetTextWithChangedStyle(style, text, &patchedStyleSheetText);
    if (!success)
        return false;

    TrackExceptionState exceptionState;
    style->setCSSText(text, exceptionState);
    if (!exceptionState.hadException()) {
        updateText(patchedStyleSheetText);
        onStyleSheetTextChanged();
    }

    return !exceptionState.hadException();
}

bool InspectorStyleSheet::styleSheetTextWithChangedStyle(CSSStyleDeclaration* style, const String& newStyleText, String* result)
{
    if (!style)
        return false;
    if (!ensureParsedDataReady())
        return false;

    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceData = ruleSourceDataAt(styleId(style).ordinal());
    unsigned bodyStart = sourceData->ruleBodyRange.start;
    unsigned bodyEnd = sourceData->ruleBodyRange.end;
    ASSERT(bodyStart <= bodyEnd);

    String text = m_parsedStyleSheet->text();
    ASSERT_WITH_SECURITY_IMPLICATION(bodyEnd <= text.length()); // bodyEnd is exclusive

    text.replace(bodyStart, bodyEnd - bodyStart, newStyleText);
    *result = text;
    return true;
}

InspectorCSSId InspectorStyleSheet::ruleId(CSSStyleRule* rule) const
{
    return styleId(rule->style());
}

bool InspectorStyleSheet::originalStyleSheetText(String* result) const
{
    bool success = inlineStyleSheetText(result);
    if (!success)
        success = resourceStyleSheetText(result);
    return success;
}

bool InspectorStyleSheet::resourceStyleSheetText(String* result) const
{
    if (m_origin == TypeBuilder::CSS::StyleSheetOrigin::Injected || m_origin == TypeBuilder::CSS::StyleSheetOrigin::User_agent)
        return false;

    if (!ownerDocument())
        return false;

    KURL url(ParsedURLString, m_pageStyleSheet->href());
    if (m_cssAgent->getEditedStyleSheet(url, result))
        return true;

    bool base64Encoded;
    bool success = m_resourceAgent->fetchResourceContent(ownerDocument(), url, result, &base64Encoded);
    return success && !base64Encoded;
}

Element* InspectorStyleSheet::ownerStyleElement() const
{
    Node* ownerNode = m_pageStyleSheet->ownerNode();
    if (!ownerNode || !ownerNode->isElementNode())
        return nullptr;
    Element* ownerElement = toElement(ownerNode);

    if (!isHTMLStyleElement(ownerElement) && !isSVGStyleElement(ownerElement))
        return nullptr;
    return ownerElement;
}

bool InspectorStyleSheet::inlineStyleSheetText(String* result) const
{
    Element* ownerElement = ownerStyleElement();
    if (!ownerElement)
        return false;
    if (m_cssAgent->getEditedStyleElement(DOMNodeIds::idForNode(ownerElement), result))
        return true;
    *result = ownerElement->textContent();
    return true;
}

PassRefPtrWillBeRawPtr<InspectorStyleSheetForInlineStyle> InspectorStyleSheetForInlineStyle::create(const String& id, PassRefPtrWillBeRawPtr<Element> element, Listener* listener)
{
    return adoptRefWillBeNoop(new InspectorStyleSheetForInlineStyle(id, element, listener));
}

InspectorStyleSheetForInlineStyle::InspectorStyleSheetForInlineStyle(const String& id, PassRefPtrWillBeRawPtr<Element> element, Listener* listener)
    : InspectorStyleSheetBase(id, listener)
    , m_element(element)
    , m_ruleSourceData(nullptr)
    , m_isStyleTextValid(false)
{
    ASSERT(m_element);
    m_inspectorStyle = InspectorStyle::create(InspectorCSSId(id, 0), inlineStyle(), this);
    m_styleText = m_element->isStyledElement() ? m_element->getAttribute("style").string() : String();
}

void InspectorStyleSheetForInlineStyle::didModifyElementAttribute()
{
    m_isStyleTextValid = false;
    if (m_element->isStyledElement() && m_element->style() != m_inspectorStyle->cssStyle())
        m_inspectorStyle = InspectorStyle::create(InspectorCSSId(id(), 0), inlineStyle(), this);
    m_ruleSourceData.clear();
}

bool InspectorStyleSheetForInlineStyle::setText(const String& text, ExceptionState& exceptionState)
{
    bool success = setStyleText(InspectorCSSId(id(), 0), text);
    if (!success)
        exceptionState.throwDOMException(SyntaxError, "Style sheet text is invalid.");
    return success;
}

bool InspectorStyleSheetForInlineStyle::getText(String* result) const
{
    if (!m_isStyleTextValid) {
        m_styleText = elementStyleText();
        m_isStyleTextValid = true;
    }
    *result = m_styleText;
    return true;
}

bool InspectorStyleSheetForInlineStyle::setStyleText(const InspectorCSSId& id, const String& text)
{
    CSSStyleDeclaration* style = styleForId(id);
    if (!style)
        return false;
    ASSERT_UNUSED(style, style == inlineStyle());
    TrackExceptionState exceptionState;

    {
        InspectorCSSAgent::InlineStyleOverrideScope overrideScope(m_element->ownerDocument());
        m_element->setAttribute("style", AtomicString(text), exceptionState);
    }
    if (!exceptionState.hadException()) {
        m_styleText = text;
        m_isStyleTextValid = true;
        m_ruleSourceData.clear();
        onStyleSheetTextChanged();
    }
    return !exceptionState.hadException();
}

Document* InspectorStyleSheetForInlineStyle::ownerDocument() const
{
    return &m_element->document();
}

bool InspectorStyleSheetForInlineStyle::ensureParsedDataReady()
{
    // The "style" property value can get changed indirectly, e.g. via element.style.borderWidth = "2px".
    const String& currentStyleText = elementStyleText();
    if (m_styleText != currentStyleText) {
        m_ruleSourceData.clear();
        m_styleText = currentStyleText;
        m_isStyleTextValid = true;
    }

    if (m_ruleSourceData)
        return true;

    m_ruleSourceData = getStyleAttributeData();

    bool success = !!m_ruleSourceData;
    if (!success) {
        m_ruleSourceData = CSSRuleSourceData::create(StyleRule::Style);
        return false;
    }

    return true;
}

PassRefPtrWillBeRawPtr<InspectorStyle> InspectorStyleSheetForInlineStyle::inspectorStyleForId(const InspectorCSSId& id)
{
    ASSERT_UNUSED(id, !id.ordinal());
    return m_inspectorStyle;
}

CSSStyleDeclaration* InspectorStyleSheetForInlineStyle::inlineStyle() const
{
    return m_element->style();
}

const String& InspectorStyleSheetForInlineStyle::elementStyleText() const
{
    return m_element->getAttribute("style").string();
}

PassRefPtrWillBeRawPtr<CSSRuleSourceData> InspectorStyleSheetForInlineStyle::getStyleAttributeData() const
{
    if (!m_element->isStyledElement())
        return nullptr;

    if (m_styleText.isEmpty()) {
        RefPtrWillBeRawPtr<CSSRuleSourceData> result = CSSRuleSourceData::create(StyleRule::Style);
        result->ruleBodyRange.start = 0;
        result->ruleBodyRange.end = 0;
        return result.release();
    }

    RefPtrWillBeRawPtr<MutableStylePropertySet> tempDeclaration = MutableStylePropertySet::create();
    RuleSourceDataList ruleSourceDataResult;
    StyleSheetHandler handler(m_styleText, &m_element->document(), m_element->document().elementSheet().contents(), &ruleSourceDataResult);
    CSSParser::parseDeclarationList(parserContextForDocument(&m_element->document()), tempDeclaration.get(), m_styleText, &handler, m_element->document().elementSheet().contents());
    return ruleSourceDataResult.first().release();
}

DEFINE_TRACE(InspectorStyleSheetForInlineStyle)
{
    visitor->trace(m_element);
    visitor->trace(m_ruleSourceData);
    visitor->trace(m_inspectorStyle);
    InspectorStyleSheetBase::trace(visitor);
}

} // namespace blink
