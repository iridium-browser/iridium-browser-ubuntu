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
#include "core/inspector/InspectorCSSAgent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/CSSPropertyNames.h"
#include "core/InspectorTypeBuilder.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSMediaRule.h"
#include "core/css/CSSRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQuery.h"
#include "core/css/MediaValues.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/StyleSheetList.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Node.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/VoidCallback.h"
#include "core/inspector/InspectorHistory.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorResourceAgent.h"
#include "core/inspector/InspectorResourceContentLoader.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutObjectInlines.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/shaping/SimpleShaper.h"
#include "platform/text/TextRun.h"
#include "wtf/CurrentTime.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringConcatenate.h"

namespace CSSAgentState {
static const char cssAgentEnabled[] = "cssAgentEnabled";
}

typedef blink::InspectorBackendDispatcher::CSSCommandHandler::EnableCallback EnableCallback;

namespace blink {

enum ForcePseudoClassFlags {
    PseudoNone = 0,
    PseudoHover = 1 << 0,
    PseudoFocus = 1 << 1,
    PseudoActive = 1 << 2,
    PseudoVisited = 1 << 3
};

static unsigned computePseudoClassMask(JSONArray* pseudoClassArray)
{
    DEFINE_STATIC_LOCAL(String, active, ("active"));
    DEFINE_STATIC_LOCAL(String, hover, ("hover"));
    DEFINE_STATIC_LOCAL(String, focus, ("focus"));
    DEFINE_STATIC_LOCAL(String, visited, ("visited"));
    if (!pseudoClassArray || !pseudoClassArray->length())
        return PseudoNone;

    unsigned result = PseudoNone;
    for (size_t i = 0; i < pseudoClassArray->length(); ++i) {
        RefPtr<JSONValue> pseudoClassValue = pseudoClassArray->get(i);
        String pseudoClass;
        bool success = pseudoClassValue->asString(&pseudoClass);
        if (!success)
            continue;
        if (pseudoClass == active)
            result |= PseudoActive;
        else if (pseudoClass == hover)
            result |= PseudoHover;
        else if (pseudoClass == focus)
            result |= PseudoFocus;
        else if (pseudoClass == visited)
            result |= PseudoVisited;
    }

    return result;
}

class InspectorCSSAgent::StyleSheetAction : public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(StyleSheetAction);
public:
    StyleSheetAction(const String& name)
        : InspectorHistory::Action(name)
    {
    }
};

class InspectorCSSAgent::InspectorResourceContentLoaderCallback final : public VoidCallback {
public:
    InspectorResourceContentLoaderCallback(InspectorCSSAgent*, PassRefPtrWillBeRawPtr<EnableCallback>);
    DECLARE_VIRTUAL_TRACE();
    virtual void handleEvent() override;

private:
    RawPtrWillBeMember<InspectorCSSAgent> m_cssAgent;
    RefPtrWillBeMember<EnableCallback> m_callback;
};

InspectorCSSAgent::InspectorResourceContentLoaderCallback::InspectorResourceContentLoaderCallback(InspectorCSSAgent* cssAgent, PassRefPtrWillBeRawPtr<EnableCallback> callback)
    : m_cssAgent(cssAgent)
    , m_callback(callback)
{
}

DEFINE_TRACE(InspectorCSSAgent::InspectorResourceContentLoaderCallback)
{
    visitor->trace(m_cssAgent);
    visitor->trace(m_callback);
    VoidCallback::trace(visitor);
}

void InspectorCSSAgent::InspectorResourceContentLoaderCallback::handleEvent()
{
    // enable always succeeds.
    if (!m_callback->isActive())
        return;

    m_cssAgent->wasEnabled();
    m_callback->sendSuccess();
}

class InspectorCSSAgent::SetStyleSheetTextAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetStyleSheetTextAction);
public:
    SetStyleSheetTextAction(InspectorStyleSheetBase* styleSheet, const String& text)
        : InspectorCSSAgent::StyleSheetAction("SetStyleSheetText")
        , m_styleSheet(styleSheet)
        , m_text(text)
    {
    }

    virtual bool perform(ExceptionState& exceptionState) override
    {
        if (!m_styleSheet->getText(&m_oldText))
            return false;
        return redo(exceptionState);
    }

    virtual bool undo(ExceptionState& exceptionState) override
    {
        return m_styleSheet->setText(m_oldText, exceptionState);
    }

    virtual bool redo(ExceptionState& exceptionState) override
    {
        return m_styleSheet->setText(m_text, exceptionState);
    }

    virtual String mergeId() override
    {
        return String::format("SetStyleSheetText %s", m_styleSheet->id().utf8().data());
    }

    virtual void merge(PassRefPtrWillBeRawPtr<Action> action) override
    {
        ASSERT(action->mergeId() == mergeId());

        SetStyleSheetTextAction* other = static_cast<SetStyleSheetTextAction*>(action.get());
        m_text = other->m_text;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_styleSheet);
        InspectorCSSAgent::StyleSheetAction::trace(visitor);
    }

private:
    RefPtrWillBeMember<InspectorStyleSheetBase> m_styleSheet;
    String m_text;
    String m_oldText;
};

class InspectorCSSAgent::SetPropertyTextAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetPropertyTextAction);
public:
    SetPropertyTextAction(InspectorStyleSheetBase* styleSheet, const InspectorCSSId& cssId, unsigned propertyIndex, const String& text, bool overwrite)
        : InspectorCSSAgent::StyleSheetAction("SetPropertyText")
        , m_styleSheet(styleSheet)
        , m_cssId(cssId)
        , m_propertyIndex(propertyIndex)
        , m_text(text)
        , m_overwrite(overwrite)
    {
    }

    virtual String toString() override
    {
        return mergeId() + ": " + m_oldStyleText + " -> " + m_text;
    }

    virtual bool perform(ExceptionState& exceptionState) override
    {
        return redo(exceptionState);
    }

    virtual bool undo(ExceptionState& exceptionState) override
    {
        String placeholder;
        return m_styleSheet->setStyleText(m_cssId, m_oldStyleText);
    }

    virtual bool redo(ExceptionState& exceptionState) override
    {
        if (!m_styleSheet->getStyleText(m_cssId, &m_oldStyleText))
            return false;
        bool result = m_styleSheet->setPropertyText(m_cssId, m_propertyIndex, m_text, m_overwrite, exceptionState);
        return result;
    }

    virtual String mergeId() override
    {
        return String::format("SetPropertyText %s:%u:%s", m_styleSheet->id().utf8().data(), m_propertyIndex, m_overwrite ? "true" : "false");
    }

    virtual void merge(PassRefPtrWillBeRawPtr<Action> action) override
    {
        ASSERT(action->mergeId() == mergeId());

        SetPropertyTextAction* other = static_cast<SetPropertyTextAction*>(action.get());
        m_text = other->m_text;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_styleSheet);
        InspectorCSSAgent::StyleSheetAction::trace(visitor);
    }

private:
    RefPtrWillBeMember<InspectorStyleSheetBase> m_styleSheet;
    InspectorCSSId m_cssId;
    unsigned m_propertyIndex;
    String m_text;
    String m_oldStyleText;
    bool m_overwrite;
};

class InspectorCSSAgent::SetRuleSelectorAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetRuleSelectorAction);
public:
    SetRuleSelectorAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, const String& selector)
        : InspectorCSSAgent::StyleSheetAction("SetRuleSelector")
        , m_styleSheet(styleSheet)
        , m_cssId(cssId)
        , m_selector(selector)
    {
    }

    virtual bool perform(ExceptionState& exceptionState) override
    {
        m_oldSelector = m_styleSheet->ruleSelector(m_cssId, exceptionState);
        if (exceptionState.hadException())
            return false;
        return redo(exceptionState);
    }

    virtual bool undo(ExceptionState& exceptionState) override
    {
        return m_styleSheet->setRuleSelector(m_cssId, m_oldSelector, exceptionState);
    }

    virtual bool redo(ExceptionState& exceptionState) override
    {
        return m_styleSheet->setRuleSelector(m_cssId, m_selector, exceptionState);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_styleSheet);
        InspectorCSSAgent::StyleSheetAction::trace(visitor);
    }

private:
    RefPtrWillBeMember<InspectorStyleSheet> m_styleSheet;
    InspectorCSSId m_cssId;
    String m_selector;
    String m_oldSelector;
};

class InspectorCSSAgent::SetMediaTextAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetMediaTextAction);
public:
    SetMediaTextAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, const String& text)
        : InspectorCSSAgent::StyleSheetAction("SetMediaText")
        , m_styleSheet(styleSheet)
        , m_cssId(cssId)
        , m_text(text)
    {
    }

    virtual bool perform(ExceptionState& exceptionState) override
    {
        m_oldText = m_styleSheet->mediaRuleText(m_cssId, exceptionState);
        if (exceptionState.hadException())
            return false;
        return redo(exceptionState);
    }

    virtual bool undo(ExceptionState& exceptionState) override
    {
        return m_styleSheet->setMediaRuleText(m_cssId, m_oldText, exceptionState);
    }

    virtual bool redo(ExceptionState& exceptionState) override
    {
        return m_styleSheet->setMediaRuleText(m_cssId, m_text, exceptionState);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_styleSheet);
        InspectorCSSAgent::StyleSheetAction::trace(visitor);
    }

private:
    RefPtrWillBeMember<InspectorStyleSheet> m_styleSheet;
    InspectorCSSId m_cssId;
    String m_text;
    String m_oldText;
};

class InspectorCSSAgent::AddRuleAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(AddRuleAction);
public:
    AddRuleAction(InspectorStyleSheet* styleSheet, const String& ruleText, const SourceRange& location)
        : InspectorCSSAgent::StyleSheetAction("AddRule")
        , m_styleSheet(styleSheet)
        , m_ruleText(ruleText)
        , m_location(location)
    {
    }

    virtual bool perform(ExceptionState& exceptionState) override
    {
        return redo(exceptionState);
    }

    virtual bool undo(ExceptionState& exceptionState) override
    {
        return m_styleSheet->deleteRule(m_newId, m_oldText, exceptionState);
    }

    virtual bool redo(ExceptionState& exceptionState) override
    {
        if (!m_styleSheet->getText(&m_oldText))
            return false;
        CSSStyleRule* cssStyleRule = m_styleSheet->addRule(m_ruleText, m_location, exceptionState);
        if (exceptionState.hadException())
            return false;
        m_newId = m_styleSheet->ruleId(cssStyleRule);
        return true;
    }

    InspectorCSSId newRuleId() { return m_newId; }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_styleSheet);
        InspectorCSSAgent::StyleSheetAction::trace(visitor);
    }

private:
    RefPtrWillBeMember<InspectorStyleSheet> m_styleSheet;
    InspectorCSSId m_newId;
    String m_ruleText;
    String m_oldText;
    SourceRange m_location;
};

// static
CSSStyleRule* InspectorCSSAgent::asCSSStyleRule(CSSRule* rule)
{
    if (!rule || rule->type() != CSSRule::STYLE_RULE)
        return nullptr;
    return toCSSStyleRule(rule);
}

// static
CSSMediaRule* InspectorCSSAgent::asCSSMediaRule(CSSRule* rule)
{
    if (!rule || rule->type() != CSSRule::MEDIA_RULE)
        return nullptr;
    return toCSSMediaRule(rule);
}

InspectorCSSAgent::InspectorCSSAgent(InspectorDOMAgent* domAgent, InspectorPageAgent* pageAgent, InspectorResourceAgent* resourceAgent)
    : InspectorBaseAgent<InspectorCSSAgent, InspectorFrontend::CSS>("CSS")
    , m_domAgent(domAgent)
    , m_pageAgent(pageAgent)
    , m_resourceAgent(resourceAgent)
    , m_lastStyleSheetId(1)
    , m_styleSheetsPendingMutation(0)
    , m_styleDeclarationPendingMutation(false)
    , m_creatingViaInspectorStyleSheet(false)
    , m_isSettingStyleSheetText(false)
{
    m_domAgent->setDOMListener(this);
}

InspectorCSSAgent::~InspectorCSSAgent()
{
#if !ENABLE(OILPAN)
    ASSERT(!m_domAgent);
    reset();
#endif
}

void InspectorCSSAgent::discardAgent()
{
    m_domAgent->setDOMListener(nullptr);
    m_domAgent = nullptr;
}

void InspectorCSSAgent::restore()
{
    if (m_state->getBoolean(CSSAgentState::cssAgentEnabled))
        wasEnabled();
}

void InspectorCSSAgent::flushPendingProtocolNotifications()
{
    if (!m_invalidatedDocuments.size())
        return;
    WillBeHeapHashSet<RawPtrWillBeMember<Document> > invalidatedDocuments;
    m_invalidatedDocuments.swap(&invalidatedDocuments);
    for (Document* document: invalidatedDocuments)
        updateActiveStyleSheets(document, ExistingFrontendRefresh);
}

void InspectorCSSAgent::reset()
{
    m_idToInspectorStyleSheet.clear();
    m_idToInspectorStyleSheetForInlineStyle.clear();
    m_cssStyleSheetToInspectorStyleSheet.clear();
    m_documentToCSSStyleSheets.clear();
    m_invalidatedDocuments.clear();
    m_nodeToInspectorStyleSheet.clear();
    m_documentToViaInspectorStyleSheet.clear();
    resetNonPersistentData();
}

void InspectorCSSAgent::resetNonPersistentData()
{
    resetPseudoStates();
}

void InspectorCSSAgent::enable(ErrorString* errorString, PassRefPtrWillBeRawPtr<EnableCallback> prpCallback)
{
    if (!m_domAgent->enabled()) {
        *errorString = "DOM agent needs to be enabled first.";
        return;
    }
    m_state->setBoolean(CSSAgentState::cssAgentEnabled, true);
    if (!m_pageAgent->resourceContentLoader()) {
        wasEnabled();
        prpCallback->sendSuccess();
        return;
    }
    m_pageAgent->resourceContentLoader()->ensureResourcesContentLoaded(new InspectorCSSAgent::InspectorResourceContentLoaderCallback(this, prpCallback));
}

void InspectorCSSAgent::wasEnabled()
{
    if (!m_state->getBoolean(CSSAgentState::cssAgentEnabled)) {
        // We were disabled while fetching resources.
        return;
    }

    m_instrumentingAgents->setInspectorCSSAgent(this);
    WillBeHeapVector<RawPtrWillBeMember<Document> > documents = m_domAgent->documents();
    for (Document* document : documents)
        updateActiveStyleSheets(document, InitialFrontendLoad);
}

void InspectorCSSAgent::disable(ErrorString*)
{
    reset();
    m_instrumentingAgents->setInspectorCSSAgent(0);
    m_state->setBoolean(CSSAgentState::cssAgentEnabled, false);
}

void InspectorCSSAgent::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    if (frame == m_pageAgent->inspectedFrame()) {
        reset();
        m_editedStyleSheets.clear();
        m_editedStyleElements.clear();
    }
}

void InspectorCSSAgent::mediaQueryResultChanged()
{
    flushPendingProtocolNotifications();
    frontend()->mediaQueryResultChanged();
}

void InspectorCSSAgent::willMutateRules()
{
    ++m_styleSheetsPendingMutation;
}

void InspectorCSSAgent::didMutateRules(CSSStyleSheet* styleSheet)
{
    --m_styleSheetsPendingMutation;
    ASSERT(m_styleSheetsPendingMutation >= 0);

    if (!styleSheetEditInProgress()) {
        Document* owner = styleSheet->ownerDocument();
        if (owner)
            owner->modifiedStyleSheet(styleSheet, FullStyleUpdate);
    }
}

void InspectorCSSAgent::willMutateStyle()
{
    m_styleDeclarationPendingMutation = true;
}

void InspectorCSSAgent::didMutateStyle(CSSStyleDeclaration* style, bool isInlineStyle)
{
    ASSERT(m_styleDeclarationPendingMutation);
    m_styleDeclarationPendingMutation = false;
    if (!styleSheetEditInProgress() && !isInlineStyle) {
        CSSStyleSheet* parentSheet = style->parentStyleSheet();
        Document* owner = parentSheet ? parentSheet->ownerDocument() : nullptr;
        if (owner)
            owner->modifiedStyleSheet(parentSheet, FullStyleUpdate);
    }
}

void InspectorCSSAgent::activeStyleSheetsUpdated(Document* document)
{
    if (styleSheetEditInProgress())
        return;

    m_invalidatedDocuments.add(document);
    if (m_creatingViaInspectorStyleSheet)
        flushPendingProtocolNotifications();
}

void InspectorCSSAgent::updateActiveStyleSheets(Document* document, StyleSheetsUpdateType styleSheetsUpdateType)
{
    WillBeHeapVector<RawPtrWillBeMember<CSSStyleSheet> > newSheetsVector;
    InspectorCSSAgent::collectAllDocumentStyleSheets(document, newSheetsVector);
    setActiveStyleSheets(document, newSheetsVector, styleSheetsUpdateType);
}

void InspectorCSSAgent::setActiveStyleSheets(Document* document, const WillBeHeapVector<RawPtrWillBeMember<CSSStyleSheet> >& allSheetsVector, StyleSheetsUpdateType styleSheetsUpdateType)
{
    bool isInitialFrontendLoad = styleSheetsUpdateType == InitialFrontendLoad;

    WillBeHeapHashSet<RawPtrWillBeMember<CSSStyleSheet> >* documentCSSStyleSheets = m_documentToCSSStyleSheets.get(document);
    if (!documentCSSStyleSheets) {
        documentCSSStyleSheets = new WillBeHeapHashSet<RawPtrWillBeMember<CSSStyleSheet> >();
        OwnPtrWillBeRawPtr<WillBeHeapHashSet<RawPtrWillBeMember<CSSStyleSheet> > > documentCSSStyleSheetsPtr = adoptPtrWillBeNoop(documentCSSStyleSheets);
        m_documentToCSSStyleSheets.set(document, documentCSSStyleSheetsPtr.release());
    }

    WillBeHeapHashSet<RawPtrWillBeMember<CSSStyleSheet> > removedSheets(*documentCSSStyleSheets);
    WillBeHeapVector<RawPtrWillBeMember<CSSStyleSheet> > addedSheets;
    for (CSSStyleSheet* cssStyleSheet : allSheetsVector) {
        if (removedSheets.contains(cssStyleSheet)) {
            removedSheets.remove(cssStyleSheet);
            if (isInitialFrontendLoad)
                addedSheets.append(cssStyleSheet);
        } else {
            addedSheets.append(cssStyleSheet);
        }
    }

    for (CSSStyleSheet* cssStyleSheet : removedSheets) {
        RefPtrWillBeRawPtr<InspectorStyleSheet> inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(cssStyleSheet);
        ASSERT(inspectorStyleSheet);

        documentCSSStyleSheets->remove(cssStyleSheet);
        if (m_idToInspectorStyleSheet.contains(inspectorStyleSheet->id())) {
            String id = unbindStyleSheet(inspectorStyleSheet.get());
            if (frontend() && !isInitialFrontendLoad)
                frontend()->styleSheetRemoved(id);
        }
    }

    for (CSSStyleSheet* cssStyleSheet : addedSheets) {
        bool isNew = isInitialFrontendLoad || !m_cssStyleSheetToInspectorStyleSheet.contains(cssStyleSheet);
        if (isNew) {
            InspectorStyleSheet* newStyleSheet = bindStyleSheet(cssStyleSheet);
            documentCSSStyleSheets->add(cssStyleSheet);
            if (frontend())
                frontend()->styleSheetAdded(newStyleSheet->buildObjectForStyleSheetInfo());
        }
    }

    if (documentCSSStyleSheets->isEmpty())
        m_documentToCSSStyleSheets.remove(document);
}

void InspectorCSSAgent::documentDetached(Document* document)
{
    m_invalidatedDocuments.remove(document);
    setActiveStyleSheets(document, WillBeHeapVector<RawPtrWillBeMember<CSSStyleSheet> >(), ExistingFrontendRefresh);
}

void InspectorCSSAgent::addEditedStyleSheet(const String& url, const String& content)
{
    m_editedStyleSheets.set(url, content);
}

bool InspectorCSSAgent::getEditedStyleSheet(const String& url, String* content)
{
    if (!m_editedStyleSheets.contains(url))
        return false;
    *content = m_editedStyleSheets.get(url);
    return true;
}

void InspectorCSSAgent::addEditedStyleElement(int backendNodeId, const String& content)
{
    m_editedStyleElements.set(backendNodeId, content);
}

bool InspectorCSSAgent::getEditedStyleElement(int backendNodeId, String* content)
{
    if (!m_editedStyleElements.contains(backendNodeId))
        return false;
    *content = m_editedStyleElements.get(backendNodeId);
    return true;
}

bool InspectorCSSAgent::forcePseudoState(Element* element, CSSSelector::PseudoType pseudoType)
{
    if (m_nodeIdToForcedPseudoState.isEmpty())
        return false;

    int nodeId = m_domAgent->boundNodeId(element);
    if (!nodeId)
        return false;

    NodeIdToForcedPseudoState::iterator it = m_nodeIdToForcedPseudoState.find(nodeId);
    if (it == m_nodeIdToForcedPseudoState.end())
        return false;

    unsigned forcedPseudoState = it->value;
    switch (pseudoType) {
    case CSSSelector::PseudoActive:
        return forcedPseudoState & PseudoActive;
    case CSSSelector::PseudoFocus:
        return forcedPseudoState & PseudoFocus;
    case CSSSelector::PseudoHover:
        return forcedPseudoState & PseudoHover;
    case CSSSelector::PseudoVisited:
        return forcedPseudoState & PseudoVisited;
    default:
        return false;
    }
}

void InspectorCSSAgent::getMediaQueries(ErrorString* errorString, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> >& medias)
{
    medias = TypeBuilder::Array<TypeBuilder::CSS::CSSMedia>::create();
    for (auto& style : m_idToInspectorStyleSheet) {
        RefPtrWillBeRawPtr<InspectorStyleSheet> styleSheet = style.value;
        collectMediaQueriesFromStyleSheet(styleSheet->pageStyleSheet(), medias.get());
        const CSSRuleVector& flatRules = styleSheet->flatRules();
        for (unsigned i = 0; i < flatRules.size(); ++i) {
            CSSRule* rule = flatRules.at(i).get();
            if (rule->type() == CSSRule::MEDIA_RULE || rule->type() == CSSRule::IMPORT_RULE)
                collectMediaQueriesFromRule(rule, medias.get());
        }
    }
}

void InspectorCSSAgent::getMatchedStylesForNode(ErrorString* errorString, int nodeId, const bool* excludePseudo, const bool* excludeInherited, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> >& matchedCSSRules, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PseudoIdMatches> >& pseudoIdMatches, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::InheritedStyleEntry> >& inheritedEntries)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element) {
        *errorString = "Node not found";
        return;
    }

    Element* originalElement = element;
    PseudoId elementPseudoId = element->pseudoId();
    if (elementPseudoId) {
        element = element->parentOrShadowHostElement();
        if (!element) {
            *errorString = "Pseudo element has no parent";
            return;
        }
    }

    Document* ownerDocument = element->ownerDocument();
    // A non-active document has no styles.
    if (!ownerDocument->isActive())
        return;

    // FIXME: It's really gross for the inspector to reach in and access StyleResolver
    // directly here. We need to provide the Inspector better APIs to get this information
    // without grabbing at internal style classes!

    // Matched rules.
    StyleResolver& styleResolver = ownerDocument->ensureStyleResolver();

    element->updateDistribution();
    RefPtrWillBeRawPtr<CSSRuleList> matchedRules = styleResolver.pseudoCSSRulesForElement(element, elementPseudoId, StyleResolver::AllCSSRules);
    matchedCSSRules = buildArrayForMatchedRuleList(matchedRules.get(), originalElement, NOPSEUDO);

    // Pseudo elements.
    if (!elementPseudoId && !asBool(excludePseudo)) {
        RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PseudoIdMatches> > pseudoElements = TypeBuilder::Array<TypeBuilder::CSS::PseudoIdMatches>::create();
        for (PseudoId pseudoId = FIRST_PUBLIC_PSEUDOID; pseudoId < AFTER_LAST_INTERNAL_PSEUDOID; pseudoId = static_cast<PseudoId>(pseudoId + 1)) {
            RefPtrWillBeRawPtr<CSSRuleList> matchedRules = styleResolver.pseudoCSSRulesForElement(element, pseudoId, StyleResolver::AllCSSRules);
            if (matchedRules && matchedRules->length()) {
                RefPtr<TypeBuilder::CSS::PseudoIdMatches> matches = TypeBuilder::CSS::PseudoIdMatches::create()
                    .setPseudoId(static_cast<int>(pseudoId))
                    .setMatches(buildArrayForMatchedRuleList(matchedRules.get(), element, pseudoId));
                pseudoElements->addItem(matches.release());
            }
        }

        pseudoIdMatches = pseudoElements.release();
    }

    // Inherited styles.
    if (!elementPseudoId && !asBool(excludeInherited)) {
        RefPtr<TypeBuilder::Array<TypeBuilder::CSS::InheritedStyleEntry> > entries = TypeBuilder::Array<TypeBuilder::CSS::InheritedStyleEntry>::create();
        Element* parentElement = element->parentElement();
        while (parentElement) {
            StyleResolver& parentStyleResolver = parentElement->ownerDocument()->ensureStyleResolver();
            RefPtrWillBeRawPtr<CSSRuleList> parentMatchedRules = parentStyleResolver.cssRulesForElement(parentElement, StyleResolver::AllCSSRules);
            RefPtr<TypeBuilder::CSS::InheritedStyleEntry> entry = TypeBuilder::CSS::InheritedStyleEntry::create()
                .setMatchedCSSRules(buildArrayForMatchedRuleList(parentMatchedRules.get(), parentElement, NOPSEUDO));
            if (parentElement->style() && parentElement->style()->length()) {
                InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(parentElement);
                if (styleSheet)
                    entry->setInlineStyle(styleSheet->buildObjectForStyle(styleSheet->styleForId(InspectorCSSId(styleSheet->id(), 0))));
            }

            entries->addItem(entry.release());
            parentElement = parentElement->parentElement();
        }

        inheritedEntries = entries.release();
    }
}

void InspectorCSSAgent::getInlineStylesForNode(ErrorString* errorString, int nodeId, RefPtr<TypeBuilder::CSS::CSSStyle>& inlineStyle, RefPtr<TypeBuilder::CSS::CSSStyle>& attributesStyle)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(element);
    if (!styleSheet)
        return;

    inlineStyle = styleSheet->buildObjectForStyle(element->style());
    RefPtr<TypeBuilder::CSS::CSSStyle> attributes = buildObjectForAttributesStyle(element);
    attributesStyle = attributes ? attributes.release() : nullptr;
}

void InspectorCSSAgent::getComputedStyleForNode(ErrorString* errorString, int nodeId, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty> >& style)
{
    Node* node = m_domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;

    RefPtrWillBeRawPtr<CSSComputedStyleDeclaration> computedStyleInfo = CSSComputedStyleDeclaration::create(node, true);
    RefPtrWillBeRawPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), computedStyleInfo, 0);
    style = inspectorStyle->buildArrayForComputedStyle();
}

void InspectorCSSAgent::collectPlatformFontsForLayoutObject(LayoutText* layoutObject, HashCountedSet<String>* fontStats)
{
    for (InlineTextBox* box = layoutObject->firstTextBox(); box; box = box->nextTextBox()) {
        const ComputedStyle& style = layoutObject->styleRef(box->isFirstLineStyle());
        const Font& font = style.font();
        TextRun run = box->constructTextRunForInspector(style, font);
        SimpleShaper shaper(&font, run);
        GlyphBuffer glyphBuffer;
        shaper.advance(run.length(), &glyphBuffer);
        for (unsigned i = 0; i < glyphBuffer.size(); ++i) {
            String familyName = glyphBuffer.fontDataAt(i)->platformData().fontFamilyName();
            if (familyName.isNull())
                familyName = "";
            fontStats->add(familyName);
        }
    }
}

void InspectorCSSAgent::getPlatformFontsForNode(ErrorString* errorString, int nodeId,
    String* cssFamilyName, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PlatformFontUsage> >& platformFonts)
{
    Node* node = m_domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;

    RefPtrWillBeRawPtr<CSSComputedStyleDeclaration> computedStyleInfo = CSSComputedStyleDeclaration::create(node, true);
    *cssFamilyName = computedStyleInfo->getPropertyValue(CSSPropertyFontFamily);

    WillBeHeapVector<RawPtrWillBeMember<Text> > textNodes;
    if (node->nodeType() == Node::TEXT_NODE) {
        if (node->layoutObject())
            textNodes.append(toText(node));
    } else {
        for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
            if (child->nodeType() == Node::TEXT_NODE && child->layoutObject())
                textNodes.append(toText(child));
        }
    }

    HashCountedSet<String> fontStats;
    for (size_t i = 0; i < textNodes.size(); ++i) {
        LayoutText* layoutObject = textNodes[i]->layoutObject();
        collectPlatformFontsForLayoutObject(layoutObject, &fontStats);

        if (!layoutObject->isTextFragment())
            continue;

        // If we're the remaining text from a first-letter then our previous
        // sibling has to be the first-letter layoutObject.
        LayoutObject* previous = layoutObject->previousSibling();
        if (!previous)
            continue;

        if (!previous->isPseudoElement() || !previous->node()->isFirstLetterPseudoElement())
            continue;

        // The first-letter pseudoElement only has one child, which is the
        // first-letter layoutObject.
        collectPlatformFontsForLayoutObject(toLayoutText(previous->slowFirstChild()), &fontStats);
    }

    platformFonts = TypeBuilder::Array<TypeBuilder::CSS::PlatformFontUsage>::create();
    for (auto& font : fontStats) {
        RefPtr<TypeBuilder::CSS::PlatformFontUsage> platformFont = TypeBuilder::CSS::PlatformFontUsage::create()
            .setFamilyName(font.key)
            .setGlyphCount(font.value);
        platformFonts->addItem(platformFont);
    }
}

void InspectorCSSAgent::getStyleSheetText(ErrorString* errorString, const String& styleSheetId, String* result)
{
    InspectorStyleSheetBase* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    inspectorStyleSheet->getText(result);
}

void InspectorCSSAgent::setStyleSheetText(ErrorString* errorString, const String& styleSheetId, const String& text)
{
    InspectorStyleSheetBase* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet) {
        *errorString = "Style sheet with id " + styleSheetId + " not found";
        return;
    }

    TrackExceptionState exceptionState;
    m_domAgent->history()->perform(adoptRefWillBeNoop(new SetStyleSheetTextAction(inspectorStyleSheet, text)), exceptionState);
    *errorString = InspectorDOMAgent::toErrorString(exceptionState);
}

static bool extractRangeComponent(ErrorString* errorString, const RefPtr<JSONObject>& range, const String& component, unsigned& result)
{
    int parsedValue = 0;
    if (!range->getNumber(component, &parsedValue) || parsedValue < 0) {
        *errorString = "range." + component + " must be a non-negative integer";
        return false;
    }
    result = parsedValue;
    return true;
}

static bool jsonRangeToSourceRange(ErrorString* errorString, InspectorStyleSheetBase* inspectorStyleSheet, const RefPtr<JSONObject>& range, SourceRange* sourceRange)
{
    unsigned startLineNumber = 0;
    unsigned startColumn = 0;
    unsigned endLineNumber = 0;
    unsigned endColumn = 0;
    if (!extractRangeComponent(errorString, range, "startLine", startLineNumber)
        || !extractRangeComponent(errorString, range, "startColumn", startColumn)
        || !extractRangeComponent(errorString, range, "endLine", endLineNumber)
        || !extractRangeComponent(errorString, range, "endColumn", endColumn))
        return false;

    unsigned startOffset = 0;
    unsigned endOffset = 0;
    bool success = inspectorStyleSheet->lineNumberAndColumnToOffset(startLineNumber, startColumn, &startOffset)
        && inspectorStyleSheet->lineNumberAndColumnToOffset(endLineNumber, endColumn, &endOffset);
    if (!success) {
        *errorString = "Specified range is out of bounds";
        return false;
    }

    if (startOffset > endOffset) {
        *errorString = "Range start must not succeed its end";
        return false;
    }
    sourceRange->start = startOffset;
    sourceRange->end = endOffset;
    return true;
}

void InspectorCSSAgent::setPropertyText(ErrorString* errorString, const String& styleSheetId, const RefPtr<JSONObject>& range, const String& text, RefPtr<TypeBuilder::CSS::CSSStyle>& result)
{
    InspectorStyleSheetBase* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;
    SourceRange propertyRange;
    if (!jsonRangeToSourceRange(errorString, inspectorStyleSheet, range, &propertyRange))
        return;
    InspectorCSSId compoundId;
    unsigned propertyIndex;
    bool overwrite;
    if (!inspectorStyleSheet->findPropertyByRange(propertyRange, &compoundId, &propertyIndex, &overwrite)) {
        *errorString = "Source range didn't match any existing property source range nor any property insertion point";
        return;
    }

    TrackExceptionState exceptionState;
    bool success = m_domAgent->history()->perform(adoptRefWillBeNoop(new SetPropertyTextAction(inspectorStyleSheet, compoundId, propertyIndex, text, overwrite)), exceptionState);
    if (success)
        result = inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(compoundId));
    *errorString = InspectorDOMAgent::toErrorString(exceptionState);
}

void InspectorCSSAgent::setRuleSelector(ErrorString* errorString, const String& styleSheetId, const RefPtr<JSONObject>& range, const String& selector, RefPtr<TypeBuilder::CSS::CSSRule>& result)
{
    InspectorStyleSheet* inspectorStyleSheet = assertInspectorStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet) {
        *errorString = "Stylesheet not found";
        return;
    }
    SourceRange selectorRange;
    if (!jsonRangeToSourceRange(errorString, inspectorStyleSheet, range, &selectorRange))
        return;
    InspectorCSSId compoundId;
    if (!inspectorStyleSheet->findRuleBySelectorRange(selectorRange, &compoundId)) {
        *errorString = "Source range didn't match any rule selector source range";
        return;
    }

    TrackExceptionState exceptionState;
    bool success = m_domAgent->history()->perform(adoptRefWillBeNoop(new SetRuleSelectorAction(inspectorStyleSheet, compoundId, selector)), exceptionState);
    if (success) {
        CSSStyleRule* rule = inspectorStyleSheet->ruleForId(compoundId);
        result = inspectorStyleSheet->buildObjectForRule(rule, buildMediaListChain(rule));
    }
    *errorString = InspectorDOMAgent::toErrorString(exceptionState);
}

void InspectorCSSAgent::setMediaText(ErrorString* errorString, const String& styleSheetId, const RefPtr<JSONObject>& range, const String& text, RefPtr<TypeBuilder::CSS::CSSMedia>& result)
{
    InspectorStyleSheet* inspectorStyleSheet = assertInspectorStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet) {
        *errorString = "Stylesheet not found";
        return;
    }
    SourceRange textRange;
    if (!jsonRangeToSourceRange(errorString, inspectorStyleSheet, range, &textRange))
        return;
    InspectorCSSId compoundId;
    if (!inspectorStyleSheet->findMediaRuleByRange(textRange, &compoundId)) {
        *errorString = "Source range didn't match any media rule source range";
        return;
    }

    TrackExceptionState exceptionState;
    bool success = m_domAgent->history()->perform(adoptRefWillBeNoop(new SetMediaTextAction(inspectorStyleSheet, compoundId, text)), exceptionState);
    if (success) {
        CSSMediaRule* rule = inspectorStyleSheet->mediaRuleForId(compoundId);
        String sourceURL = rule->parentStyleSheet()->contents()->baseURL();
        if (sourceURL.isEmpty())
            sourceURL = InspectorDOMAgent::documentURLString(rule->parentStyleSheet()->ownerDocument());
        result = buildMediaObject(rule->media(), MediaListSourceMediaRule, sourceURL, rule->parentStyleSheet());
    }
    *errorString = InspectorDOMAgent::toErrorString(exceptionState);
}

void InspectorCSSAgent::createStyleSheet(ErrorString* errorString, const String& frameId, TypeBuilder::CSS::StyleSheetId* outStyleSheetId)
{
    LocalFrame* frame = m_pageAgent->frameForId(frameId);
    if (!frame) {
        *errorString = "Frame not found";
        return;
    }

    Document* document = frame->document();
    if (!document) {
        *errorString = "Frame does not have a document";
        return;
    }

    InspectorStyleSheet* inspectorStyleSheet = viaInspectorStyleSheet(document, true);
    if (!inspectorStyleSheet) {
        *errorString = "No target stylesheet found";
        return;
    }

    updateActiveStyleSheets(document, ExistingFrontendRefresh);

    *outStyleSheetId = inspectorStyleSheet->id();
}

void InspectorCSSAgent::addRule(ErrorString* errorString, const String& styleSheetId, const String& ruleText, const RefPtr<JSONObject>& location, RefPtr<TypeBuilder::CSS::CSSRule>& result)
{
    InspectorStyleSheet* inspectorStyleSheet = assertInspectorStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;
    SourceRange ruleLocation;
    if (!jsonRangeToSourceRange(errorString, inspectorStyleSheet, location, &ruleLocation))
        return;

    TrackExceptionState exceptionState;
    RefPtrWillBeRawPtr<AddRuleAction> action = adoptRefWillBeNoop(new AddRuleAction(inspectorStyleSheet, ruleText, ruleLocation));
    bool success = m_domAgent->history()->perform(action, exceptionState);
    if (!success) {
        *errorString = InspectorDOMAgent::toErrorString(exceptionState);
        return;
    }

    InspectorCSSId ruleId = action->newRuleId();
    CSSStyleRule* rule = inspectorStyleSheet->ruleForId(ruleId);
    result = inspectorStyleSheet->buildObjectForRule(rule, buildMediaListChain(rule));
}

void InspectorCSSAgent::forcePseudoState(ErrorString* errorString, int nodeId, const RefPtr<JSONArray>& forcedPseudoClasses)
{
    Element* element = m_domAgent->assertElement(errorString, nodeId);
    if (!element)
        return;

    unsigned forcedPseudoState = computePseudoClassMask(forcedPseudoClasses.get());
    NodeIdToForcedPseudoState::iterator it = m_nodeIdToForcedPseudoState.find(nodeId);
    unsigned currentForcedPseudoState = it == m_nodeIdToForcedPseudoState.end() ? 0 : it->value;
    bool needStyleRecalc = forcedPseudoState != currentForcedPseudoState;
    if (!needStyleRecalc)
        return;

    if (forcedPseudoState)
        m_nodeIdToForcedPseudoState.set(nodeId, forcedPseudoState);
    else
        m_nodeIdToForcedPseudoState.remove(nodeId);
    element->ownerDocument()->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::Inspector));
}

PassRefPtr<TypeBuilder::CSS::CSSMedia> InspectorCSSAgent::buildMediaObject(const MediaList* media, MediaListSource mediaListSource, const String& sourceURL, CSSStyleSheet* parentStyleSheet)
{
    // Make certain compilers happy by initializing |source| up-front.
    TypeBuilder::CSS::CSSMedia::Source::Enum source = TypeBuilder::CSS::CSSMedia::Source::InlineSheet;
    switch (mediaListSource) {
    case MediaListSourceMediaRule:
        source = TypeBuilder::CSS::CSSMedia::Source::MediaRule;
        break;
    case MediaListSourceImportRule:
        source = TypeBuilder::CSS::CSSMedia::Source::ImportRule;
        break;
    case MediaListSourceLinkedSheet:
        source = TypeBuilder::CSS::CSSMedia::Source::LinkedSheet;
        break;
    case MediaListSourceInlineSheet:
        source = TypeBuilder::CSS::CSSMedia::Source::InlineSheet;
        break;
    }

    const MediaQuerySet* queries = media->queries();
    const WillBeHeapVector<OwnPtrWillBeMember<MediaQuery> >& queryVector = queries->queryVector();
    LocalFrame* frame = nullptr;
    if (parentStyleSheet) {
        if (Document* document = parentStyleSheet->ownerDocument())
            frame = document->frame();
    }
    OwnPtr<MediaQueryEvaluator> mediaEvaluator = adoptPtr(new MediaQueryEvaluator(frame));

    InspectorStyleSheet* inspectorStyleSheet = parentStyleSheet ? m_cssStyleSheetToInspectorStyleSheet.get(parentStyleSheet) : nullptr;
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::MediaQuery> > mediaListArray = TypeBuilder::Array<TypeBuilder::CSS::MediaQuery>::create();
    RefPtr<MediaValues> mediaValues = MediaValues::createDynamicIfFrameExists(frame);
    bool hasMediaQueryItems = false;
    for (size_t i = 0; i < queryVector.size(); ++i) {
        MediaQuery* query = queryVector.at(i).get();
        const ExpressionHeapVector& expressions = query->expressions();
        RefPtr<TypeBuilder::Array<TypeBuilder::CSS::MediaQueryExpression> > expressionArray = TypeBuilder::Array<TypeBuilder::CSS::MediaQueryExpression>::create();
        bool hasExpressionItems = false;
        for (size_t j = 0; j < expressions.size(); ++j) {
            MediaQueryExp* mediaQueryExp = expressions.at(j).get();
            MediaQueryExpValue expValue = mediaQueryExp->expValue();
            if (!expValue.isValue)
                continue;
            const char* valueName = CSSPrimitiveValue::unitTypeToString(expValue.unit);
            RefPtr<TypeBuilder::CSS::MediaQueryExpression> mediaQueryExpression = TypeBuilder::CSS::MediaQueryExpression::create()
                .setValue(expValue.value)
                .setUnit(String(valueName))
                .setFeature(mediaQueryExp->mediaFeature());

            if (inspectorStyleSheet && media->parentRule()) {
                RefPtr<TypeBuilder::CSS::SourceRange> valueRange = inspectorStyleSheet->mediaQueryExpValueSourceRange(media->parentRule(), i, j);
                if (valueRange)
                    mediaQueryExpression->setValueRange(valueRange);
            }

            int computedLength;
            if (mediaValues->computeLength(expValue.value, expValue.unit, computedLength))
                mediaQueryExpression->setComputedLength(computedLength);

            expressionArray->addItem(mediaQueryExpression);
            hasExpressionItems = true;
        }
        if (!hasExpressionItems)
            continue;
        RefPtr<TypeBuilder::CSS::MediaQuery> mediaQuery = TypeBuilder::CSS::MediaQuery::create()
            .setActive(mediaEvaluator->eval(query, nullptr))
            .setExpressions(expressionArray);
        mediaListArray->addItem(mediaQuery);
        hasMediaQueryItems = true;
    }

    RefPtr<TypeBuilder::CSS::CSSMedia> mediaObject = TypeBuilder::CSS::CSSMedia::create()
        .setText(media->mediaText())
        .setSource(source);
    if (hasMediaQueryItems)
        mediaObject->setMediaList(mediaListArray);

    if (inspectorStyleSheet && mediaListSource != MediaListSourceLinkedSheet)
        mediaObject->setParentStyleSheetId(inspectorStyleSheet->id());

    if (!sourceURL.isEmpty()) {
        mediaObject->setSourceURL(sourceURL);

        CSSRule* parentRule = media->parentRule();
        if (!parentRule)
            return mediaObject.release();
        InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(parentRule->parentStyleSheet());
        RefPtr<TypeBuilder::CSS::SourceRange> mediaRange = inspectorStyleSheet->ruleHeaderSourceRange(parentRule);
        if (mediaRange)
            mediaObject->setRange(mediaRange);
    }
    return mediaObject.release();
}

bool InspectorCSSAgent::collectMediaQueriesFromStyleSheet(CSSStyleSheet* styleSheet, TypeBuilder::Array<TypeBuilder::CSS::CSSMedia>* mediaArray)
{
    bool addedItems = false;
    MediaList* mediaList = styleSheet->media();
    String sourceURL;
    if (mediaList && mediaList->length()) {
        Document* doc = styleSheet->ownerDocument();
        if (doc)
            sourceURL = doc->url();
        else if (!styleSheet->contents()->baseURL().isEmpty())
            sourceURL = styleSheet->contents()->baseURL();
        else
            sourceURL = "";
        mediaArray->addItem(buildMediaObject(mediaList, styleSheet->ownerNode() ? MediaListSourceLinkedSheet : MediaListSourceInlineSheet, sourceURL, styleSheet));
        addedItems = true;
    }
    return addedItems;
}

bool InspectorCSSAgent::collectMediaQueriesFromRule(CSSRule* rule, TypeBuilder::Array<TypeBuilder::CSS::CSSMedia>* mediaArray)
{
    MediaList* mediaList;
    String sourceURL;
    CSSStyleSheet* parentStyleSheet = nullptr;
    bool isMediaRule = true;
    bool addedItems = false;
    if (rule->type() == CSSRule::MEDIA_RULE) {
        CSSMediaRule* mediaRule = toCSSMediaRule(rule);
        mediaList = mediaRule->media();
        parentStyleSheet = mediaRule->parentStyleSheet();
    } else if (rule->type() == CSSRule::IMPORT_RULE) {
        CSSImportRule* importRule = toCSSImportRule(rule);
        mediaList = importRule->media();
        parentStyleSheet = importRule->parentStyleSheet();
        isMediaRule = false;
    } else {
        mediaList = nullptr;
    }

    if (parentStyleSheet) {
        sourceURL = parentStyleSheet->contents()->baseURL();
        if (sourceURL.isEmpty())
            sourceURL = InspectorDOMAgent::documentURLString(parentStyleSheet->ownerDocument());
    } else {
        sourceURL = "";
    }

    if (mediaList && mediaList->length()) {
        mediaArray->addItem(buildMediaObject(mediaList, isMediaRule ? MediaListSourceMediaRule : MediaListSourceImportRule, sourceURL, parentStyleSheet));
        addedItems = true;
    }
    return addedItems;
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> > InspectorCSSAgent::buildMediaListChain(CSSRule* rule)
{
    if (!rule)
        return nullptr;
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> > mediaArray = TypeBuilder::Array<TypeBuilder::CSS::CSSMedia>::create();
    bool hasItems = false;
    CSSRule* parentRule = rule;
    while (parentRule) {
        hasItems = collectMediaQueriesFromRule(parentRule, mediaArray.get()) || hasItems;
        if (parentRule->parentRule()) {
            parentRule = parentRule->parentRule();
        } else {
            CSSStyleSheet* styleSheet = parentRule->parentStyleSheet();
            while (styleSheet) {
                hasItems = collectMediaQueriesFromStyleSheet(styleSheet, mediaArray.get()) || hasItems;
                parentRule = styleSheet->ownerRule();
                if (parentRule)
                    break;
                styleSheet = styleSheet->parentStyleSheet();
            }
        }
    }
    return hasItems ? mediaArray : nullptr;
}

InspectorStyleSheetForInlineStyle* InspectorCSSAgent::asInspectorStyleSheet(Element* element)
{
    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(element);
    if (it != m_nodeToInspectorStyleSheet.end())
        return it->value.get();

    CSSStyleDeclaration* style = element->style();
    if (!style)
        return nullptr;

    String newStyleSheetId = String::number(m_lastStyleSheetId++);
    RefPtrWillBeRawPtr<InspectorStyleSheetForInlineStyle> inspectorStyleSheet = InspectorStyleSheetForInlineStyle::create(newStyleSheetId, element, this);
    m_idToInspectorStyleSheetForInlineStyle.set(newStyleSheetId, inspectorStyleSheet);
    m_nodeToInspectorStyleSheet.set(element, inspectorStyleSheet);
    return inspectorStyleSheet.get();
}

Element* InspectorCSSAgent::elementForId(ErrorString* errorString, int nodeId)
{
    Node* node = m_domAgent->nodeForId(nodeId);
    if (!node) {
        *errorString = "No node with given id found";
        return nullptr;
    }
    if (!node->isElementNode()) {
        *errorString = "Not an element node";
        return nullptr;
    }
    return toElement(node);
}

// static
void InspectorCSSAgent::collectAllDocumentStyleSheets(Document* document, WillBeHeapVector<RawPtrWillBeMember<CSSStyleSheet> >& result)
{
    const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet>> activeStyleSheets = document->styleEngine().activeStyleSheetsForInspector();
    for (const auto& style : activeStyleSheets) {
        CSSStyleSheet* styleSheet = style.get();
        InspectorCSSAgent::collectStyleSheets(styleSheet, result);
    }
}

// static
void InspectorCSSAgent::collectStyleSheets(CSSStyleSheet* styleSheet, WillBeHeapVector<RawPtrWillBeMember<CSSStyleSheet> >& result)
{
    result.append(styleSheet);
    for (unsigned i = 0, size = styleSheet->length(); i < size; ++i) {
        CSSRule* rule = styleSheet->item(i);
        if (rule->type() == CSSRule::IMPORT_RULE) {
            CSSStyleSheet* importedStyleSheet = toCSSImportRule(rule)->styleSheet();
            if (importedStyleSheet)
                InspectorCSSAgent::collectStyleSheets(importedStyleSheet, result);
        }
    }
}

InspectorStyleSheet* InspectorCSSAgent::bindStyleSheet(CSSStyleSheet* styleSheet)
{
    RefPtrWillBeRawPtr<InspectorStyleSheet> inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(styleSheet);
    if (!inspectorStyleSheet) {
        String id = String::number(m_lastStyleSheetId++);
        Document* document = styleSheet->ownerDocument();
        inspectorStyleSheet = InspectorStyleSheet::create(m_resourceAgent, id, styleSheet, detectOrigin(styleSheet, document), InspectorDOMAgent::documentURLString(document), this);
        m_idToInspectorStyleSheet.set(id, inspectorStyleSheet);
        m_cssStyleSheetToInspectorStyleSheet.set(styleSheet, inspectorStyleSheet);
        if (m_creatingViaInspectorStyleSheet)
            m_documentToViaInspectorStyleSheet.add(document, inspectorStyleSheet);
    }
    return inspectorStyleSheet.get();
}

String InspectorCSSAgent::unbindStyleSheet(InspectorStyleSheet* inspectorStyleSheet)
{
    String id = inspectorStyleSheet->id();
    m_idToInspectorStyleSheet.remove(id);
    if (inspectorStyleSheet->pageStyleSheet())
        m_cssStyleSheetToInspectorStyleSheet.remove(inspectorStyleSheet->pageStyleSheet());
    return id;
}

InspectorStyleSheet* InspectorCSSAgent::viaInspectorStyleSheet(Document* document, bool createIfAbsent)
{
    if (!document) {
        ASSERT(!createIfAbsent);
        return nullptr;
    }

    if (!document->isHTMLDocument() && !document->isSVGDocument())
        return nullptr;

    RefPtrWillBeRawPtr<InspectorStyleSheet> inspectorStyleSheet = m_documentToViaInspectorStyleSheet.get(document);
    if (inspectorStyleSheet || !createIfAbsent)
        return inspectorStyleSheet.get();

    TrackExceptionState exceptionState;
    RefPtrWillBeRawPtr<Element> styleElement = document->createElement("style", exceptionState);
    if (!exceptionState.hadException())
        styleElement->setAttribute("type", "text/css", exceptionState);
    if (!exceptionState.hadException()) {
        ContainerNode* targetNode;
        // HEAD is absent in ImageDocuments, for example.
        if (document->head())
            targetNode = document->head();
        else if (document->body())
            targetNode = document->body();
        else
            return nullptr;

        InlineStyleOverrideScope overrideScope(document);
        m_creatingViaInspectorStyleSheet = true;
        targetNode->appendChild(styleElement, exceptionState);
        // At this point the added stylesheet will get bound through the updateActiveStyleSheets() invocation.
        // We just need to pick the respective InspectorStyleSheet from m_documentToViaInspectorStyleSheet.
        m_creatingViaInspectorStyleSheet = false;
    }

    if (exceptionState.hadException())
        return nullptr;

    return m_documentToViaInspectorStyleSheet.get(document);
}

InspectorStyleSheet* InspectorCSSAgent::assertInspectorStyleSheetForId(ErrorString* errorString, const String& styleSheetId)
{
    IdToInspectorStyleSheet::iterator it = m_idToInspectorStyleSheet.find(styleSheetId);
    if (it == m_idToInspectorStyleSheet.end()) {
        *errorString = "No style sheet with given id found";
        return nullptr;
    }
    return it->value.get();
}

InspectorStyleSheetBase* InspectorCSSAgent::assertStyleSheetForId(ErrorString* errorString, const String& styleSheetId)
{
    String placeholder;
    InspectorStyleSheetBase* result = assertInspectorStyleSheetForId(&placeholder, styleSheetId);
    if (result)
        return result;
    IdToInspectorStyleSheetForInlineStyle::iterator it = m_idToInspectorStyleSheetForInlineStyle.find(styleSheetId);
    if (it == m_idToInspectorStyleSheetForInlineStyle.end()) {
        *errorString = "No style sheet with given id found";
        return nullptr;
    }
    return it->value.get();
}

TypeBuilder::CSS::StyleSheetOrigin::Enum InspectorCSSAgent::detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument)
{
    if (m_creatingViaInspectorStyleSheet)
        return TypeBuilder::CSS::StyleSheetOrigin::Inspector;

    TypeBuilder::CSS::StyleSheetOrigin::Enum origin = TypeBuilder::CSS::StyleSheetOrigin::Regular;
    if (pageStyleSheet && !pageStyleSheet->ownerNode() && pageStyleSheet->href().isEmpty())
        origin = TypeBuilder::CSS::StyleSheetOrigin::User_agent;
    else if (pageStyleSheet && pageStyleSheet->ownerNode() && pageStyleSheet->ownerNode()->isDocumentNode())
        origin = TypeBuilder::CSS::StyleSheetOrigin::Injected;
    else {
        InspectorStyleSheet* viaInspectorStyleSheetForOwner = viaInspectorStyleSheet(ownerDocument, false);
        if (viaInspectorStyleSheetForOwner && pageStyleSheet == viaInspectorStyleSheetForOwner->pageStyleSheet())
            origin = TypeBuilder::CSS::StyleSheetOrigin::Inspector;
    }
    return origin;
}

PassRefPtr<TypeBuilder::CSS::CSSRule> InspectorCSSAgent::buildObjectForRule(CSSStyleRule* rule)
{
    if (!rule)
        return nullptr;

    // CSSRules returned by StyleResolver::pseudoCSSRulesForElement lack parent pointers if they are coming from
    // user agent stylesheets. To work around this issue, we use CSSOM wrapper created by inspector.
    if (!rule->parentStyleSheet()) {
        if (!m_inspectorUserAgentStyleSheet)
            m_inspectorUserAgentStyleSheet = CSSStyleSheet::create(CSSDefaultStyleSheets::instance().defaultStyleSheet());
        rule->setParentStyleSheet(m_inspectorUserAgentStyleSheet.get());
    }
    return bindStyleSheet(rule->parentStyleSheet())->buildObjectForRule(rule, buildMediaListChain(rule));
}

static inline bool matchesPseudoElement(const CSSSelector* selector, PseudoId elementPseudoId)
{
    // According to http://www.w3.org/TR/css3-selectors/#pseudo-elements, "Only one pseudo-element may appear per selector."
    // As such, check the last selector in the tag history.
    for (; !selector->isLastInTagHistory(); ++selector) { }
    PseudoId selectorPseudoId = selector->matchesPseudoElement() ? CSSSelector::pseudoId(selector->pseudoType()) : NOPSEUDO;

    // FIXME: This only covers the case of matching pseudo-element selectors against PseudoElements.
    // We should come up with a solution for matching pseudo-element selectors against ordinary Elements, too.
    return selectorPseudoId == elementPseudoId;
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> > InspectorCSSAgent::buildArrayForMatchedRuleList(CSSRuleList* ruleList, Element* element, PseudoId matchesForPseudoId)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> > result = TypeBuilder::Array<TypeBuilder::CSS::RuleMatch>::create();
    if (!ruleList)
        return result.release();

    for (unsigned i = 0, size = ruleList->length(); i < size; ++i) {
        CSSStyleRule* rule = asCSSStyleRule(ruleList->item(i));
        RefPtr<TypeBuilder::CSS::CSSRule> ruleObject = buildObjectForRule(rule);
        if (!ruleObject)
            continue;
        RefPtr<TypeBuilder::Array<int> > matchingSelectors = TypeBuilder::Array<int>::create();
        const CSSSelectorList& selectorList = rule->styleRule()->selectorList();
        long index = 0;
        PseudoId elementPseudoId = matchesForPseudoId ? matchesForPseudoId : element->pseudoId();
        for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(*selector)) {
            const CSSSelector* firstTagHistorySelector = selector;
            bool matched = false;
            if (elementPseudoId)
                matched = matchesPseudoElement(selector, elementPseudoId); // Modifies |selector|.
            else
                matched = element->matches(firstTagHistorySelector->selectorText(), IGNORE_EXCEPTION);
            if (matched)
                matchingSelectors->addItem(index);
            ++index;
        }
        RefPtr<TypeBuilder::CSS::RuleMatch> match = TypeBuilder::CSS::RuleMatch::create()
            .setRule(ruleObject.release())
            .setMatchingSelectors(matchingSelectors.release());
        result->addItem(match);
    }

    return result;
}

PassRefPtr<TypeBuilder::CSS::CSSStyle> InspectorCSSAgent::buildObjectForAttributesStyle(Element* element)
{
    if (!element->isStyledElement())
        return nullptr;

    // FIXME: Ugliness below.
    StylePropertySet* attributeStyle = const_cast<StylePropertySet*>(element->presentationAttributeStyle());
    if (!attributeStyle)
        return nullptr;

    MutableStylePropertySet* mutableAttributeStyle = toMutableStylePropertySet(attributeStyle);

    RefPtrWillBeRawPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), mutableAttributeStyle->ensureCSSStyleDeclaration(), 0);
    return inspectorStyle->buildObjectForStyle();
}

void InspectorCSSAgent::didRemoveDocument(Document* document)
{
    if (document)
        m_documentToViaInspectorStyleSheet.remove(document);
}

void InspectorCSSAgent::didRemoveDOMNode(Node* node)
{
    if (!node)
        return;

    int nodeId = m_domAgent->boundNodeId(node);
    if (nodeId)
        m_nodeIdToForcedPseudoState.remove(nodeId);

    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(node);
    if (it == m_nodeToInspectorStyleSheet.end())
        return;

    m_idToInspectorStyleSheetForInlineStyle.remove(it->value->id());
    m_nodeToInspectorStyleSheet.remove(node);
}

void InspectorCSSAgent::didModifyDOMAttr(Element* element)
{
    if (!element)
        return;

    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(element);
    if (it == m_nodeToInspectorStyleSheet.end())
        return;

    it->value->didModifyElementAttribute();
}

void InspectorCSSAgent::styleSheetChanged(InspectorStyleSheetBase* styleSheet)
{
    flushPendingProtocolNotifications();
    frontend()->styleSheetChanged(styleSheet->id());
}

void InspectorCSSAgent::willReparseStyleSheet()
{
    ASSERT(!m_isSettingStyleSheetText);
    m_isSettingStyleSheetText = true;
}

void InspectorCSSAgent::didReparseStyleSheet()
{
    ASSERT(m_isSettingStyleSheetText);
    m_isSettingStyleSheetText = false;
}

void InspectorCSSAgent::resetPseudoStates()
{
    WillBeHeapHashSet<RawPtrWillBeMember<Document> > documentsToChange;
    for (auto& state : m_nodeIdToForcedPseudoState) {
        Element* element = toElement(m_domAgent->nodeForId(state.key));
        if (element && element->ownerDocument())
            documentsToChange.add(element->ownerDocument());
    }

    m_nodeIdToForcedPseudoState.clear();
    for (auto& document : documentsToChange)
        document->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::Inspector));
}

DEFINE_TRACE(InspectorCSSAgent)
{
    visitor->trace(m_domAgent);
    visitor->trace(m_pageAgent);
    visitor->trace(m_resourceAgent);
#if ENABLE(OILPAN)
    visitor->trace(m_idToInspectorStyleSheet);
    visitor->trace(m_idToInspectorStyleSheetForInlineStyle);
    visitor->trace(m_cssStyleSheetToInspectorStyleSheet);
    visitor->trace(m_documentToCSSStyleSheets);
    visitor->trace(m_invalidatedDocuments);
    visitor->trace(m_nodeToInspectorStyleSheet);
    visitor->trace(m_documentToViaInspectorStyleSheet);
#endif
    visitor->trace(m_inspectorUserAgentStyleSheet);
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink

