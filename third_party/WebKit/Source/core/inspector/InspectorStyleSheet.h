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

#ifndef InspectorStyleSheet_h
#define InspectorStyleSheet_h

#include "core/InspectorTypeBuilder.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/inspector/InspectorStyleTextEditor.h"
#include "platform/JSONValues.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

class ParsedStyleSheet;

namespace blink {

class CSSMediaRule;
class CSSStyleDeclaration;
class CSSStyleRule;
class CSSStyleSheet;
class Document;
class Element;
class ExceptionState;
class InspectorCSSAgent;
class InspectorResourceAgent;
class InspectorStyleSheetBase;

typedef WillBeHeapVector<RefPtrWillBeMember<CSSRule> > CSSRuleVector;
typedef String ErrorString;
typedef Vector<unsigned> LineEndings;

class InspectorCSSId {
public:
    InspectorCSSId()
        : m_ordinal(0)
    {
    }

    InspectorCSSId(const String& styleSheetId, unsigned ordinal)
        : m_styleSheetId(styleSheetId)
        , m_ordinal(ordinal)
    {
    }

    bool isEmpty() const { return m_styleSheetId.isEmpty(); }

    const String& styleSheetId() const { return m_styleSheetId; }
    unsigned ordinal() const { return m_ordinal; }

private:
    String m_styleSheetId;
    unsigned m_ordinal;
};

struct InspectorStyleProperty {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    explicit InspectorStyleProperty(CSSPropertySourceData sourceData)
        : sourceData(sourceData)
        , hasSource(true)
    {
    }

    InspectorStyleProperty(CSSPropertySourceData sourceData, bool hasSource)
        : sourceData(sourceData)
        , hasSource(hasSource)
    {
    }

    bool hasRawText() const { return !rawText.isEmpty(); }

    DEFINE_INLINE_TRACE() { visitor->trace(sourceData); }

    CSSPropertySourceData sourceData;
    bool hasSource;
    String rawText;
};

class InspectorStyle final : public RefCountedWillBeGarbageCollectedFinalized<InspectorStyle> {
public:
    static PassRefPtrWillBeRawPtr<InspectorStyle> create(const InspectorCSSId&, PassRefPtrWillBeRawPtr<CSSStyleDeclaration>, InspectorStyleSheetBase* parentStyleSheet);

    CSSStyleDeclaration* cssStyle() const { return m_style.get(); }
    PassRefPtr<TypeBuilder::CSS::CSSStyle> buildObjectForStyle() const;
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty> > buildArrayForComputedStyle() const;
    bool setPropertyText(unsigned index, const String& text, bool overwrite, ExceptionState&);
    bool styleText(String* result) const;
    bool textForRange(const SourceRange&, String* result) const;

    DECLARE_TRACE();

private:
    InspectorStyle(const InspectorCSSId&, PassRefPtrWillBeRawPtr<CSSStyleDeclaration>, InspectorStyleSheetBase* parentStyleSheet);

    bool verifyPropertyText(const String& propertyText, bool canOmitSemicolon);
    void populateAllProperties(WillBeHeapVector<InspectorStyleProperty>& result) const;
    PassRefPtr<TypeBuilder::CSS::CSSStyle> styleWithProperties() const;
    PassRefPtrWillBeRawPtr<CSSRuleSourceData> extractSourceData() const;
    bool applyStyleText(const String&);
    String shorthandValue(const String& shorthandProperty) const;
    NewLineAndWhitespace& newLineAndWhitespaceDelimiters() const;
    inline Document* ownerDocument() const;

    InspectorCSSId m_styleId;
    RefPtrWillBeMember<CSSStyleDeclaration> m_style;
    RawPtrWillBeMember<InspectorStyleSheetBase> m_parentStyleSheet;
    mutable std::pair<String, String> m_format;
    mutable bool m_formatAcquired;
};

class InspectorStyleSheetBase : public RefCountedWillBeGarbageCollectedFinalized<InspectorStyleSheetBase> {
public:
    class Listener {
    public:
        Listener() { }
        virtual ~Listener() { }
        virtual void styleSheetChanged(InspectorStyleSheetBase*) = 0;
        virtual void willReparseStyleSheet() = 0;
        virtual void didReparseStyleSheet() = 0;
    };
    virtual ~InspectorStyleSheetBase() { }
    DEFINE_INLINE_VIRTUAL_TRACE() { }

    String id() const { return m_id; }

    virtual Document* ownerDocument() const = 0;
    virtual bool setText(const String&, ExceptionState&) = 0;
    virtual bool getText(String* result) const = 0;
    bool setPropertyText(const InspectorCSSId&, unsigned propertyIndex, const String& text, bool overwrite, ExceptionState&);

    virtual bool setStyleText(const InspectorCSSId&, const String&) = 0;
    bool getStyleText(const InspectorCSSId&, String*);

    virtual CSSStyleDeclaration* styleForId(const InspectorCSSId&) const = 0;
    virtual InspectorCSSId styleId(CSSStyleDeclaration*) const = 0;

    PassRefPtr<TypeBuilder::CSS::CSSStyle> buildObjectForStyle(CSSStyleDeclaration*);
    bool findPropertyByRange(const SourceRange&, InspectorCSSId*, unsigned* propertyIndex, bool* overwrite);
    bool lineNumberAndColumnToOffset(unsigned lineNumber, unsigned columnNumber, unsigned* offset);

protected:
    InspectorStyleSheetBase(const String& id, Listener*);

    Listener* listener() const { return m_listener; }
    void onStyleSheetTextChanged();
    const LineEndings* lineEndings();

    virtual PassRefPtrWillBeRawPtr<InspectorStyle> inspectorStyleForId(const InspectorCSSId&) = 0;
    virtual unsigned ruleCount() = 0;

    // Also accessed by friend class InspectorStyle.
    virtual PassRefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceDataAt(unsigned) const = 0;
    virtual bool ensureParsedDataReady() = 0;

private:
    friend class InspectorStyle;

    String m_id;
    Listener* m_listener;
    OwnPtr<LineEndings> m_lineEndings;
};

class InspectorStyleSheet : public InspectorStyleSheetBase {
public:
    static PassRefPtrWillBeRawPtr<InspectorStyleSheet> create(InspectorResourceAgent*, const String& id, PassRefPtrWillBeRawPtr<CSSStyleSheet> pageStyleSheet, TypeBuilder::CSS::StyleSheetOrigin::Enum, const String& documentURL, InspectorCSSAgent*);

    virtual ~InspectorStyleSheet();
    DECLARE_VIRTUAL_TRACE();

    String finalURL() const;
    virtual Document* ownerDocument() const override;
    virtual bool setText(const String&, ExceptionState&) override;
    virtual bool getText(String* result) const override;
    String ruleSelector(const InspectorCSSId&, ExceptionState&);
    bool setRuleSelector(const InspectorCSSId&, const String& selector, ExceptionState&);
    String mediaRuleText(const InspectorCSSId&, ExceptionState&);
    bool setMediaRuleText(const InspectorCSSId&, const String& text, ExceptionState&);
    CSSStyleRule* addRule(const String& ruleText, const SourceRange& location, ExceptionState&);
    bool deleteRule(const InspectorCSSId&, const String& oldText, ExceptionState&);

    CSSStyleSheet* pageStyleSheet() const { return m_pageStyleSheet.get(); }

    PassRefPtr<TypeBuilder::CSS::CSSStyleSheetHeader> buildObjectForStyleSheetInfo() const;
    PassRefPtr<TypeBuilder::CSS::CSSRule> buildObjectForRule(CSSStyleRule*, PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> >);

    PassRefPtr<TypeBuilder::CSS::SourceRange> ruleHeaderSourceRange(const CSSRule*);
    PassRefPtr<TypeBuilder::CSS::SourceRange> mediaQueryExpValueSourceRange(const CSSRule*, size_t mediaQueryIndex, size_t mediaQueryExpIndex);

    InspectorCSSId ruleId(CSSStyleRule*) const;
    CSSStyleRule* ruleForId(const InspectorCSSId&) const;
    CSSMediaRule* mediaRuleForId(const InspectorCSSId&) const;

    virtual InspectorCSSId styleId(CSSStyleDeclaration*) const override;
    virtual CSSStyleDeclaration* styleForId(const InspectorCSSId&) const override;
    virtual bool setStyleText(const InspectorCSSId&, const String&) override;

    bool findRuleBySelectorRange(const SourceRange&, InspectorCSSId*);
    bool findMediaRuleByRange(const SourceRange&, InspectorCSSId*);

    const CSSRuleVector& flatRules();

protected:
    virtual PassRefPtrWillBeRawPtr<InspectorStyle> inspectorStyleForId(const InspectorCSSId&) override;
    virtual unsigned ruleCount() override;

    // Also accessed by friend class InspectorStyle.
    virtual PassRefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceDataAt(unsigned) const override;
    virtual bool ensureParsedDataReady() override;

private:
    InspectorStyleSheet(InspectorResourceAgent*, const String& id, PassRefPtrWillBeRawPtr<CSSStyleSheet> pageStyleSheet, TypeBuilder::CSS::StyleSheetOrigin::Enum, const String& documentURL, InspectorCSSAgent*);
    unsigned ruleIndexBySourceRange(const CSSMediaRule* parentMediaRule, const SourceRange&);
    CSSStyleRule* insertCSSOMRuleInStyleSheet(const SourceRange&, const String& ruleText, ExceptionState&);
    CSSStyleRule* insertCSSOMRuleInMediaRule(CSSMediaRule*, const SourceRange&, const String& ruleText, ExceptionState&);
    CSSStyleRule* insertCSSOMRuleBySourceRange(const SourceRange&, const String& ruleText, ExceptionState&);
    bool verifyRuleText(const String& ruleText);
    bool verifySelectorText(const String& selectorText);
    bool verifyMediaText(const String& mediaText);
    unsigned ruleIndexByStyle(CSSStyleDeclaration*) const;
    String sourceMapURL() const;
    String sourceURL() const;
    bool ensureText() const;
    void ensureFlatRules() const;
    bool styleSheetTextWithChangedStyle(CSSStyleDeclaration*, const String& newStyleText, String* result);
    bool originalStyleSheetText(String* result) const;
    bool resourceStyleSheetText(String* result) const;
    bool inlineStyleSheetText(String* result) const;
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::Selector> > selectorsFromSource(const CSSRuleSourceData*, const String&);
    PassRefPtr<TypeBuilder::CSS::SelectorList> buildObjectForSelectorList(CSSStyleRule*);
    String url() const;
    bool hasSourceURL() const;
    bool startsAtZero() const;

    void updateText(const String& newText);
    Element* ownerStyleElement() const;

    RawPtrWillBeMember<InspectorCSSAgent> m_cssAgent;
    RawPtrWillBeMember<InspectorResourceAgent> m_resourceAgent;
    RefPtrWillBeMember<CSSStyleSheet> m_pageStyleSheet;
    TypeBuilder::CSS::StyleSheetOrigin::Enum m_origin;
    String m_documentURL;
    OwnPtr<ParsedStyleSheet> m_parsedStyleSheet;
    mutable CSSRuleVector m_flatRules;
    mutable String m_sourceURL;
};

class InspectorStyleSheetForInlineStyle final : public InspectorStyleSheetBase {
public:
    static PassRefPtrWillBeRawPtr<InspectorStyleSheetForInlineStyle> create(const String& id, PassRefPtrWillBeRawPtr<Element>, Listener*);

    void didModifyElementAttribute();
    virtual Document* ownerDocument() const override;
    virtual bool setText(const String&, ExceptionState&) override;
    virtual bool getText(String* result) const override;

    virtual CSSStyleDeclaration* styleForId(const InspectorCSSId& id) const override { ASSERT_UNUSED(id, !id.ordinal()); return inlineStyle(); }
    virtual InspectorCSSId styleId(CSSStyleDeclaration* style) const override { return InspectorCSSId(id(), 0); }
    virtual bool setStyleText(const InspectorCSSId&, const String&) override;

    DECLARE_VIRTUAL_TRACE();

protected:
    virtual PassRefPtrWillBeRawPtr<InspectorStyle> inspectorStyleForId(const InspectorCSSId&) override;
    virtual unsigned ruleCount() override { return 1; }

    // Also accessed by friend class InspectorStyle.
    virtual bool ensureParsedDataReady() override;
    virtual PassRefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceDataAt(unsigned ruleIndex) const override { ASSERT_UNUSED(ruleIndex, !ruleIndex); return m_ruleSourceData; }

private:
    InspectorStyleSheetForInlineStyle(const String& id, PassRefPtrWillBeRawPtr<Element>, Listener*);
    CSSStyleDeclaration* inlineStyle() const;
    const String& elementStyleText() const;
    PassRefPtrWillBeRawPtr<CSSRuleSourceData> getStyleAttributeData() const;

    RefPtrWillBeMember<Element> m_element;
    RefPtrWillBeMember<CSSRuleSourceData> m_ruleSourceData;
    RefPtrWillBeMember<InspectorStyle> m_inspectorStyle;

    // Contains "style" attribute value.
    mutable String m_styleText;
    mutable bool m_isStyleTextValid;
};


} // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::InspectorStyleProperty);

#endif // !defined(InspectorStyleSheet_h)
