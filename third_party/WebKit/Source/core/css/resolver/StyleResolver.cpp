/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/css/resolver/StyleResolver.h"

#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/MediaTypeNames.h"
#include "core/StylePropertyShorthand.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/CSSInterpolationTypesMap.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/InvalidatableInterpolation.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/LegacyStyleInterpolation.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSValueList.h"
#include "core/css/ElementRuleCollector.h"
#include "core/css/FontFace.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/PageRuleCollector.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/AnimatedStyleBuilder.h"
#include "core/css/resolver/CSSVariableResolver.h"
#include "core/css/resolver/MatchResult.h"
#include "core/css/resolver/MediaQueryResult.h"
#include "core/css/resolver/ScopedStyleResolver.h"
#include "core/css/resolver/SelectorFilterParentScope.h"
#include "core/css/resolver/SharedStyleFinder.h"
#include "core/css/resolver/StyleAdjuster.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/css/resolver/StyleRuleUsageTracker.h"
#include "core/dom/CSSSelectorWatch.h"
#include "core/dom/FirstLetterPseudoElement.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/GeneratedChildren.h"
#include "core/style/StyleInheritedVariables.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElement.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/StdLibExtras.h"

namespace {

using namespace blink;

void setAnimationUpdateIfNeeded(StyleResolverState& state, Element& element) {
  // If any changes to CSS Animations were detected, stash the update away for
  // application after the layout object is updated if we're in the appropriate
  // scope.
  if (!state.animationUpdate().isEmpty())
    element.ensureElementAnimations().cssAnimations().setPendingUpdate(
        state.animationUpdate());
}

// Returns whether any @apply rule sets a custom property
bool cacheCustomPropertiesForApplyAtRules(StyleResolverState& state,
                                          const MatchedPropertiesRange& range) {
  bool ruleSetsCustomProperty = false;
  // TODO(timloh): @apply should also work with properties registered as
  // non-inherited.
  if (!state.style()->inheritedVariables())
    return false;
  for (const auto& matchedProperties : range) {
    const StylePropertySet& properties = *matchedProperties.properties;
    unsigned propertyCount = properties.propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
      StylePropertySet::PropertyReference current = properties.propertyAt(i);
      if (current.id() != CSSPropertyApplyAtRule)
        continue;
      AtomicString name(toCSSCustomIdentValue(current.value()).value());
      CSSVariableData* variableData =
          state.style()->inheritedVariables()->getVariable(name);
      if (!variableData)
        continue;
      StylePropertySet* customPropertySet = variableData->propertySet();
      if (!customPropertySet)
        continue;
      if (customPropertySet->findPropertyIndex(CSSPropertyVariable) != -1)
        ruleSetsCustomProperty = true;
      state.setCustomPropertySetForApplyAtRule(name, customPropertySet);
    }
  }
  return ruleSetsCustomProperty;
}

}  // namespace

namespace blink {

using namespace HTMLNames;

ComputedStyle* StyleResolver::s_styleNotYetAvailable;

static StylePropertySet* leftToRightDeclaration() {
  DEFINE_STATIC_LOCAL(MutableStylePropertySet, leftToRightDecl,
                      (MutableStylePropertySet::create(HTMLQuirksMode)));
  if (leftToRightDecl.isEmpty())
    leftToRightDecl.setProperty(CSSPropertyDirection, CSSValueLtr);
  return &leftToRightDecl;
}

static StylePropertySet* rightToLeftDeclaration() {
  DEFINE_STATIC_LOCAL(MutableStylePropertySet, rightToLeftDecl,
                      (MutableStylePropertySet::create(HTMLQuirksMode)));
  if (rightToLeftDecl.isEmpty())
    rightToLeftDecl.setProperty(CSSPropertyDirection, CSSValueRtl);
  return &rightToLeftDecl;
}

static void collectScopedResolversForHostedShadowTrees(
    const Element& element,
    HeapVector<Member<ScopedStyleResolver>, 8>& resolvers) {
  ElementShadow* shadow = element.shadow();
  if (!shadow)
    return;

  // Adding scoped resolver for active shadow roots for shadow host styling.
  for (ShadowRoot* shadowRoot = &shadow->youngestShadowRoot(); shadowRoot;
       shadowRoot = shadowRoot->olderShadowRoot()) {
    if (ScopedStyleResolver* resolver = shadowRoot->scopedStyleResolver())
      resolvers.push_back(resolver);
  }
}

StyleResolver::StyleResolver(Document& document) : m_document(document) {
  updateMediaType();
}

StyleResolver::~StyleResolver() {}

void StyleResolver::dispose() {
  m_matchedPropertiesCache.clear();
}

void StyleResolver::setRuleUsageTracker(StyleRuleUsageTracker* tracker) {
  m_tracker = tracker;
}

void StyleResolver::addToStyleSharingList(Element& element) {
  DCHECK(RuntimeEnabledFeatures::styleSharingEnabled());
  // Never add elements to the style sharing list if we're not in a recalcStyle,
  // otherwise we could leave stale pointers in there.
  if (!document().inStyleRecalc())
    return;
  INCREMENT_STYLE_STATS_COUNTER(document().styleEngine(), sharedStyleCandidates,
                                1);
  StyleSharingList& list = styleSharingList();
  if (list.size() >= styleSharingListSize)
    list.removeLast();
  list.prepend(&element);
}

StyleSharingList& StyleResolver::styleSharingList() {
  m_styleSharingLists.resize(styleSharingMaxDepth);

  // We never put things at depth 0 into the list since that's only the <html>
  // element and it has no siblings or cousins to share with.
  unsigned depth =
      std::max(std::min(m_styleSharingDepth, styleSharingMaxDepth), 1u) - 1u;

  if (!m_styleSharingLists[depth])
    m_styleSharingLists[depth] = new StyleSharingList;
  return *m_styleSharingLists[depth];
}

void StyleResolver::clearStyleSharingList() {
  m_styleSharingLists.resize(0);
}

static inline ScopedStyleResolver* scopedResolverFor(const Element& element) {
  // Ideally, returning element->treeScope().scopedStyleResolver() should be
  // enough, but ::cue and custom pseudo elements like ::-webkit-meter-bar
  // pierce through a shadow dom boundary, yet they are not part of boundary
  // crossing rules. The assumption here is that these rules only pierce through
  // one boundary and that the scope of these elements do not have a style
  // resolver due to the fact that VTT scopes and UA shadow trees don't have
  // <style> elements. This is backed up by the DCHECKs below.

  TreeScope* treeScope = &element.treeScope();
  if (ScopedStyleResolver* resolver = treeScope->scopedStyleResolver()) {
    DCHECK(element.shadowPseudoId().isEmpty());
    DCHECK(!element.isVTTElement());
    return resolver;
  }

  treeScope = treeScope->parentTreeScope();
  if (!treeScope)
    return nullptr;
  if (element.shadowPseudoId().isEmpty() && !element.isVTTElement())
    return nullptr;
  return treeScope->scopedStyleResolver();
}

static void matchHostRules(const Element& element,
                           ElementRuleCollector& collector) {
  ElementShadow* shadow = element.shadow();
  if (!shadow)
    return;

  for (ShadowRoot* shadowRoot = &shadow->oldestShadowRoot(); shadowRoot;
       shadowRoot = shadowRoot->youngerShadowRoot()) {
    if (ScopedStyleResolver* resolver = shadowRoot->scopedStyleResolver()) {
      collector.clearMatchedRules();
      resolver->collectMatchingShadowHostRules(collector);
      collector.sortAndTransferMatchedRules();
      collector.finishAddingAuthorRulesForTreeScope();
    }
  }
}

static void matchSlottedRules(const Element& element,
                              ElementRuleCollector& collector) {
  HTMLSlotElement* slot = element.assignedSlot();
  if (!slot)
    return;

  HeapVector<Member<ScopedStyleResolver>> resolvers;
  for (; slot; slot = slot->assignedSlot()) {
    if (ScopedStyleResolver* resolver = slot->treeScope().scopedStyleResolver())
      resolvers.push_back(resolver);
  }
  for (auto it = resolvers.rbegin(); it != resolvers.rend(); ++it) {
    collector.clearMatchedRules();
    (*it)->collectMatchingTreeBoundaryCrossingRules(collector);
    collector.sortAndTransferMatchedRules();
    collector.finishAddingAuthorRulesForTreeScope();
  }
}

static void matchElementScopeRules(const Element& element,
                                   ScopedStyleResolver* elementScopeResolver,
                                   ElementRuleCollector& collector) {
  if (elementScopeResolver) {
    collector.clearMatchedRules();
    elementScopeResolver->collectMatchingAuthorRules(collector);
    elementScopeResolver->collectMatchingTreeBoundaryCrossingRules(collector);
    collector.sortAndTransferMatchedRules();
  }

  if (element.isStyledElement() && element.inlineStyle() &&
      !collector.isCollectingForPseudoElement()) {
    // Inline style is immutable as long as there is no CSSOM wrapper.
    bool isInlineStyleCacheable = !element.inlineStyle()->isMutable();
    collector.addElementStyleProperties(element.inlineStyle(),
                                        isInlineStyleCacheable);
  }

  collector.finishAddingAuthorRulesForTreeScope();
}

static bool shouldCheckScope(const Element& element,
                             const Node& scopingNode,
                             bool isInnerTreeScope) {
  if (isInnerTreeScope && element.treeScope() != scopingNode.treeScope()) {
    // Check if |element| may be affected by a ::content rule in |scopingNode|'s
    // style.  If |element| is a descendant of a shadow host which is ancestral
    // to |scopingNode|, the |element| should be included for rule collection.
    // Skip otherwise.
    const TreeScope* scope = &scopingNode.treeScope();
    while (scope && scope->parentTreeScope() != &element.treeScope())
      scope = scope->parentTreeScope();
    Element* shadowHost = scope ? scope->rootNode().ownerShadowHost() : nullptr;
    return shadowHost && element.isDescendantOf(shadowHost);
  }

  // When |element| can be distributed to |scopingNode| via <shadow>, ::content
  // rule can match, thus the case should be included.
  if (!isInnerTreeScope &&
      scopingNode.parentOrShadowHostNode() ==
          element.treeScope().rootNode().parentOrShadowHostNode())
    return true;

  // Obviously cases when ancestor scope has /deep/ or ::shadow rule should be
  // included.  Skip otherwise.
  return scopingNode.treeScope()
      .scopedStyleResolver()
      ->hasDeepOrShadowSelector();
}

void StyleResolver::matchScopedRules(const Element& element,
                                     ElementRuleCollector& collector) {
  // Match rules from treeScopes in the reverse tree-of-trees order, since the
  // cascading order for normal rules is such that when comparing rules from
  // different shadow trees, the rule from the tree which comes first in the
  // tree-of-trees order wins. From other treeScopes than the element's own
  // scope, only tree-boundary-crossing rules may match.

  ScopedStyleResolver* elementScopeResolver = scopedResolverFor(element);

  if (!document().mayContainV0Shadow()) {
    matchSlottedRules(element, collector);
    matchElementScopeRules(element, elementScopeResolver, collector);
    return;
  }

  bool matchElementScopeDone = !elementScopeResolver && !element.inlineStyle();

  const auto& treeBoundaryCrossingScopes =
      document().styleEngine().treeBoundaryCrossingScopes();
  for (auto it = treeBoundaryCrossingScopes.rbegin();
       it != treeBoundaryCrossingScopes.rend(); ++it) {
    const TreeScope& scope = (*it)->containingTreeScope();
    ScopedStyleResolver* resolver = scope.scopedStyleResolver();
    DCHECK(resolver);

    bool isInnerTreeScope =
        element.containingTreeScope().isInclusiveAncestorOf(scope);
    if (!shouldCheckScope(element, **it, isInnerTreeScope))
      continue;

    if (!matchElementScopeDone &&
        scope.isInclusiveAncestorOf(element.containingTreeScope())) {
      matchElementScopeDone = true;

      // At this point, the iterator has either encountered the scope for the
      // element itself (if that scope has boundary-crossing rules), or the
      // iterator has moved to a scope which appears before the element's scope
      // in the tree-of-trees order.  Try to match all rules from the element's
      // scope.

      matchElementScopeRules(element, elementScopeResolver, collector);
      if (resolver == elementScopeResolver) {
        // Boundary-crossing rules already collected in matchElementScopeRules.
        continue;
      }
    }

    collector.clearMatchedRules();
    resolver->collectMatchingTreeBoundaryCrossingRules(collector);
    collector.sortAndTransferMatchedRules();
    collector.finishAddingAuthorRulesForTreeScope();
  }

  if (!matchElementScopeDone)
    matchElementScopeRules(element, elementScopeResolver, collector);
}

void StyleResolver::matchAuthorRules(const Element& element,
                                     ElementRuleCollector& collector) {
  if (document().shadowCascadeOrder() != ShadowCascadeOrder::ShadowCascadeV1) {
    matchAuthorRulesV0(element, collector);
    return;
  }

  matchHostRules(element, collector);
  matchScopedRules(element, collector);
}

void StyleResolver::matchAuthorRulesV0(const Element& element,
                                       ElementRuleCollector& collector) {
  collector.clearMatchedRules();

  CascadeOrder cascadeOrder = 0;
  HeapVector<Member<ScopedStyleResolver>, 8> resolversInShadowTree;
  collectScopedResolversForHostedShadowTrees(element, resolversInShadowTree);

  // Apply :host and :host-context rules from inner scopes.
  for (int j = resolversInShadowTree.size() - 1; j >= 0; --j)
    resolversInShadowTree.at(j)->collectMatchingShadowHostRules(collector,
                                                                ++cascadeOrder);

  // Apply normal rules from element scope.
  if (ScopedStyleResolver* resolver = scopedResolverFor(element))
    resolver->collectMatchingAuthorRules(collector, ++cascadeOrder);

  // Apply /deep/ and ::shadow rules from outer scopes, and ::content from
  // inner.
  collectTreeBoundaryCrossingRulesV0CascadeOrder(element, collector);
  collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchUARules(ElementRuleCollector& collector) {
  collector.setMatchingUARules(true);

  CSSDefaultStyleSheets& defaultStyleSheets = CSSDefaultStyleSheets::instance();
  RuleSet* userAgentStyleSheet = m_printMediaType
                                     ? defaultStyleSheets.defaultPrintStyle()
                                     : defaultStyleSheets.defaultStyle();
  matchRuleSet(collector, userAgentStyleSheet);

  // In quirks mode, we match rules from the quirks user agent sheet.
  if (document().inQuirksMode())
    matchRuleSet(collector, defaultStyleSheets.defaultQuirksStyle());

  // If document uses view source styles (in view source mode or in xml viewer
  // mode), then we match rules from the view source style sheet.
  if (document().isViewSource())
    matchRuleSet(collector, defaultStyleSheets.defaultViewSourceStyle());

  collector.finishAddingUARules();
  collector.setMatchingUARules(false);
}

void StyleResolver::matchRuleSet(ElementRuleCollector& collector,
                                 RuleSet* rules) {
  collector.clearMatchedRules();
  collector.collectMatchingRules(MatchRequest(rules));
  collector.sortAndTransferMatchedRules();
}

DISABLE_CFI_PERF
void StyleResolver::matchAllRules(StyleResolverState& state,
                                  ElementRuleCollector& collector,
                                  bool includeSMILProperties) {
  matchUARules(collector);

  // Now check author rules, beginning first with presentational attributes
  // mapped from HTML.
  if (state.element()->isStyledElement()) {
    collector.addElementStyleProperties(
        state.element()->presentationAttributeStyle());

    // Now we check additional mapped declarations.
    // Tables and table cells share an additional mapped rule that must be
    // applied after all attributes, since their mapped style depends on the
    // values of multiple attributes.
    collector.addElementStyleProperties(
        state.element()->additionalPresentationAttributeStyle());

    if (state.element()->isHTMLElement()) {
      bool isAuto;
      TextDirection textDirection =
          toHTMLElement(state.element())
              ->directionalityIfhasDirAutoAttribute(isAuto);
      if (isAuto) {
        state.setHasDirAutoAttribute(true);
        collector.addElementStyleProperties(textDirection == TextDirection::kLtr
                                                ? leftToRightDeclaration()
                                                : rightToLeftDeclaration());
      }
    }
  }

  matchAuthorRules(*state.element(), collector);

  if (state.element()->isStyledElement()) {
    // For Shadow DOM V1, inline style is already collected in
    // matchScopedRules().
    if (document().shadowCascadeOrder() !=
            ShadowCascadeOrder::ShadowCascadeV1 &&
        state.element()->inlineStyle()) {
      // Inline style is immutable as long as there is no CSSOM wrapper.
      bool isInlineStyleCacheable =
          !state.element()->inlineStyle()->isMutable();
      collector.addElementStyleProperties(state.element()->inlineStyle(),
                                          isInlineStyleCacheable);
    }

    // Now check SMIL animation override style.
    if (includeSMILProperties && state.element()->isSVGElement())
      collector.addElementStyleProperties(
          toSVGElement(state.element())->animatedSMILStyleProperties(),
          false /* isCacheable */);
  }

  collector.finishAddingAuthorRulesForTreeScope();
}

void StyleResolver::collectTreeBoundaryCrossingRulesV0CascadeOrder(
    const Element& element,
    ElementRuleCollector& collector) {
  const auto& treeBoundaryCrossingScopes =
      document().styleEngine().treeBoundaryCrossingScopes();
  if (treeBoundaryCrossingScopes.isEmpty())
    return;

  // When comparing rules declared in outer treescopes, outer's rules win.
  CascadeOrder outerCascadeOrder = treeBoundaryCrossingScopes.size() * 2;
  // When comparing rules declared in inner treescopes, inner's rules win.
  CascadeOrder innerCascadeOrder = treeBoundaryCrossingScopes.size();

  for (const auto& scopingNode : treeBoundaryCrossingScopes) {
    // Skip rule collection for element when tree boundary crossing rules of
    // scopingNode's scope can never apply to it.
    bool isInnerTreeScope = element.containingTreeScope().isInclusiveAncestorOf(
        scopingNode->containingTreeScope());
    if (!shouldCheckScope(element, *scopingNode, isInnerTreeScope))
      continue;

    CascadeOrder cascadeOrder =
        isInnerTreeScope ? innerCascadeOrder : outerCascadeOrder;
    scopingNode->treeScope()
        .scopedStyleResolver()
        ->collectMatchingTreeBoundaryCrossingRules(collector, cascadeOrder);

    ++innerCascadeOrder;
    --outerCascadeOrder;
  }
}

PassRefPtr<ComputedStyle> StyleResolver::styleForDocument(Document& document) {
  const LocalFrame* frame = document.frame();

  RefPtr<ComputedStyle> documentStyle = ComputedStyle::create();
  documentStyle->setRtlOrdering(document.visuallyOrdered() ? EOrder::kVisual
                                                           : EOrder::kLogical);
  documentStyle->setZoom(frame && !document.printing() ? frame->pageZoomFactor()
                                                       : 1);
  FontDescription documentFontDescription = documentStyle->getFontDescription();
  documentFontDescription.setLocale(
      LayoutLocale::get(document.contentLanguage()));
  documentStyle->setFontDescription(documentFontDescription);
  documentStyle->setZIndex(0);
  documentStyle->setIsStackingContext(true);
  documentStyle->setUserModify(document.inDesignMode() ? READ_WRITE
                                                       : READ_ONLY);
  // These are designed to match the user-agent stylesheet values for the
  // document element so that the common case doesn't need to create a new
  // ComputedStyle in Document::inheritHtmlAndBodyElementStyles.
  documentStyle->setDisplay(EDisplay::Block);
  documentStyle->setPosition(AbsolutePosition);

  // Document::inheritHtmlAndBodyElementStyles will set the final overflow
  // style values, but they should initially be auto to avoid premature
  // scrollbar removal in PaintLayerScrollableArea::updateAfterStyleChange.
  documentStyle->setOverflowX(EOverflow::Auto);
  documentStyle->setOverflowY(EOverflow::Auto);

  document.setupFontBuilder(*documentStyle);

  return documentStyle.release();
}

void StyleResolver::adjustComputedStyle(StyleResolverState& state,
                                        Element* element) {
  StyleAdjuster::adjustComputedStyle(state.mutableStyleRef(),
                                     *state.parentStyle(), element);
}

// Start loading resources referenced by this style.
void StyleResolver::loadPendingResources(StyleResolverState& state) {
  state.elementStyleResources().loadPendingResources(state.style());
}

static const ComputedStyle* calculateBaseComputedStyle(
    StyleResolverState& state,
    const Element* animatingElement) {
  if (!animatingElement)
    return nullptr;

  ElementAnimations* elementAnimations = animatingElement->elementAnimations();
  if (!elementAnimations)
    return nullptr;

  if (CSSAnimations::isAnimatingCustomProperties(elementAnimations)) {
    state.setIsAnimatingCustomProperties(true);
    // TODO(alancutter): Use the base computed style optimisation in the
    // presence of custom property animations that don't affect pre-animated
    // computed values.
    return nullptr;
  }

  return elementAnimations->baseComputedStyle();
}

static void updateBaseComputedStyle(StyleResolverState& state,
                                    Element* animatingElement) {
  if (!animatingElement || state.isAnimatingCustomProperties())
    return;

  ElementAnimations* elementAnimations = animatingElement->elementAnimations();
  if (elementAnimations)
    elementAnimations->updateBaseComputedStyle(state.style());
}

PassRefPtr<ComputedStyle> StyleResolver::styleForElement(
    Element* element,
    const ComputedStyle* defaultParent,
    StyleSharingBehavior sharingBehavior,
    RuleMatchingBehavior matchingBehavior) {
  DCHECK(document().frame());
  DCHECK(document().settings());

  // Once an element has a layoutObject, we don't try to destroy it, since
  // otherwise the layoutObject will vanish if a style recalc happens during
  // loading.
  if (sharingBehavior == AllowStyleSharing && !document().isRenderingReady() &&
      !element->layoutObject()) {
    if (!s_styleNotYetAvailable) {
      s_styleNotYetAvailable = ComputedStyle::create().leakRef();
      s_styleNotYetAvailable->setDisplay(EDisplay::None);
      s_styleNotYetAvailable->font().update(
          document().styleEngine().fontSelector());
    }

    document().setHasNodesWithPlaceholderStyle();
    return s_styleNotYetAvailable;
  }

  document().styleEngine().incStyleForElementCount();
  INCREMENT_STYLE_STATS_COUNTER(document().styleEngine(), elementsStyled, 1);

  SelectorFilterParentScope::ensureParentStackIsPushed();

  ElementResolveContext elementContext(*element);

  if (RuntimeEnabledFeatures::styleSharingEnabled() &&
      sharingBehavior == AllowStyleSharing &&
      (defaultParent || elementContext.parentStyle())) {
    if (RefPtr<ComputedStyle> sharedStyle =
            document().styleEngine().findSharedStyle(elementContext))
      return sharedStyle.release();
  }

  StyleResolverState state(document(), elementContext, defaultParent);

  const ComputedStyle* baseComputedStyle =
      calculateBaseComputedStyle(state, element);

  if (baseComputedStyle) {
    state.setStyle(ComputedStyle::clone(*baseComputedStyle));
    if (!state.parentStyle())
      state.setParentStyle(initialStyleForElement());
  } else {
    if (state.parentStyle()) {
      RefPtr<ComputedStyle> style = ComputedStyle::create();
      style->inheritFrom(*state.parentStyle(),
                         isAtShadowBoundary(element)
                             ? ComputedStyleBase::AtShadowBoundary
                             : ComputedStyleBase::NotAtShadowBoundary);
      state.setStyle(std::move(style));
    } else {
      state.setStyle(initialStyleForElement());
      state.setParentStyle(ComputedStyle::clone(*state.style()));
    }
  }

  // contenteditable attribute (implemented by -webkit-user-modify) should
  // be propagated from shadow host to distributed node.
  if (state.distributedToInsertionPoint()) {
    if (Element* parent = element->parentElement()) {
      if (ComputedStyle* styleOfShadowHost = parent->mutableComputedStyle())
        state.style()->setUserModify(styleOfShadowHost->userModify());
    }
  }

  if (element->isLink()) {
    state.style()->setIsLink(true);
    EInsideLink linkState = state.elementLinkState();
    if (linkState != EInsideLink::kNotInsideLink) {
      bool forceVisited = InspectorInstrumentation::forcePseudoState(
          element, CSSSelector::PseudoVisited);
      if (forceVisited)
        linkState = EInsideLink::kInsideVisitedLink;
    }
    state.style()->setInsideLink(linkState);
  }

  if (!baseComputedStyle) {
    document().styleEngine().ensureUAStyleForElement(*element);

    ElementRuleCollector collector(state.elementContext(), m_selectorFilter,
                                   state.style());

    matchAllRules(state, collector,
                  matchingBehavior != MatchAllRulesExcludingSMIL);

    // TODO(dominicc): Remove this counter when Issue 590014 is fixed.
    if (element->hasTagName(HTMLNames::summaryTag)) {
      MatchedPropertiesRange properties =
          collector.matchedResult().authorRules();
      for (auto it = properties.begin(); it != properties.end(); ++it) {
        const CSSValue* value =
            it->properties->getPropertyCSSValue(CSSPropertyDisplay);
        if (value && value->isIdentifierValue() &&
            toCSSIdentifierValue(*value).getValueID() == CSSValueBlock)
          UseCounter::count(
              element->document(),
              UseCounter::SummaryElementWithDisplayBlockAuthorRule);
      }
    }

    if (m_tracker)
      addMatchedRulesToTracker(collector);

    if (element->computedStyle() &&
        element->computedStyle()->textAutosizingMultiplier() !=
            state.style()->textAutosizingMultiplier()) {
      // Preserve the text autosizing multiplier on style recalc. Autosizer will
      // update it during layout if needed.
      // NOTE: this must occur before applyMatchedProperties for correct
      // computation of font-relative lengths.
      state.style()->setTextAutosizingMultiplier(
          element->computedStyle()->textAutosizingMultiplier());
      state.style()->setUnique();
    }

    if (state.hasDirAutoAttribute())
      state.style()->setSelfOrAncestorHasDirAutoAttribute(true);

    applyMatchedPropertiesAndCustomPropertyAnimations(
        state, collector.matchedResult(), element);
    applyCallbackSelectors(state);

    // Cache our original display.
    state.style()->setOriginalDisplay(state.style()->display());

    adjustComputedStyle(state, element);

    updateBaseComputedStyle(state, element);
  } else {
    INCREMENT_STYLE_STATS_COUNTER(document().styleEngine(), baseStylesUsed, 1);
  }

  // FIXME: The CSSWG wants to specify that the effects of animations are
  // applied before important rules, but this currently happens here as we
  // require adjustment to have happened before deciding which properties to
  // transition.
  if (applyAnimatedStandardProperties(state, element)) {
    INCREMENT_STYLE_STATS_COUNTER(document().styleEngine(), stylesAnimated, 1);
    adjustComputedStyle(state, element);
  }

  if (isHTMLBodyElement(*element))
    document().textLinkColors().setTextColor(state.style()->color());

  setAnimationUpdateIfNeeded(state, *element);

  if (state.style()->hasViewportUnits())
    document().setHasViewportUnits();

  if (state.style()->hasRemUnits())
    document().styleEngine().setUsesRemUnit(true);

  // Now return the style.
  return state.takeStyle();
}

// TODO(alancutter): Create compositor keyframe values directly instead of
// intermediate AnimatableValues.
PassRefPtr<AnimatableValue> StyleResolver::createAnimatableValueSnapshot(
    Element& element,
    const ComputedStyle& baseStyle,
    const ComputedStyle* parentStyle,
    CSSPropertyID property,
    const CSSValue* value) {
  // TODO(alancutter): Avoid creating a StyleResolverState just to apply a
  // single value on a ComputedStyle.
  StyleResolverState state(element.document(), &element, parentStyle);
  state.setStyle(ComputedStyle::clone(baseStyle));
  if (value) {
    StyleBuilder::applyProperty(property, state, *value);
    state.fontBuilder().createFont(
        state.document().styleEngine().fontSelector(), state.mutableStyleRef());
  }
  return CSSAnimatableValueFactory::create(property, *state.style());
}

PseudoElement* StyleResolver::createPseudoElement(Element* parent,
                                                  PseudoId pseudoId) {
  if (pseudoId == PseudoIdFirstLetter)
    return FirstLetterPseudoElement::create(parent);
  return PseudoElement::create(parent, pseudoId);
}

PseudoElement* StyleResolver::createPseudoElementIfNeeded(Element& parent,
                                                          PseudoId pseudoId) {
  LayoutObject* parentLayoutObject = parent.layoutObject();
  if (!parentLayoutObject)
    return nullptr;

  // The first letter pseudo element has to look up the tree and see if any
  // of the ancestors are first letter.
  if (pseudoId < FirstInternalPseudoId && pseudoId != PseudoIdFirstLetter &&
      !parentLayoutObject->style()->hasPseudoStyle(pseudoId))
    return nullptr;

  if (pseudoId == PseudoIdBackdrop && !parent.isInTopLayer())
    return nullptr;

  if (pseudoId == PseudoIdFirstLetter &&
      (parent.isSVGElement() ||
       !FirstLetterPseudoElement::firstLetterTextLayoutObject(parent)))
    return nullptr;

  if (!canHaveGeneratedChildren(*parentLayoutObject))
    return nullptr;

  ComputedStyle* parentStyle = parentLayoutObject->mutableStyle();
  if (ComputedStyle* cachedStyle =
          parentStyle->getCachedPseudoStyle(pseudoId)) {
    if (!pseudoElementLayoutObjectIsNeeded(cachedStyle))
      return nullptr;
    return createPseudoElement(&parent, pseudoId);
  }

  StyleResolverState state(document(), &parent, parentStyle);
  if (!pseudoStyleForElementInternal(parent, pseudoId, parentStyle, state))
    return nullptr;
  RefPtr<ComputedStyle> style = state.takeStyle();
  DCHECK(style);
  parentStyle->addCachedPseudoStyle(style);

  if (!pseudoElementLayoutObjectIsNeeded(style.get()))
    return nullptr;

  PseudoElement* pseudo = createPseudoElement(&parent, pseudoId);

  setAnimationUpdateIfNeeded(state, *pseudo);
  if (ElementAnimations* elementAnimations = pseudo->elementAnimations())
    elementAnimations->cssAnimations().maybeApplyPendingUpdate(pseudo);
  return pseudo;
}

bool StyleResolver::pseudoStyleForElementInternal(
    Element& element,
    const PseudoStyleRequest& pseudoStyleRequest,
    const ComputedStyle* parentStyle,
    StyleResolverState& state) {
  DCHECK(document().frame());
  DCHECK(document().settings());
  DCHECK(pseudoStyleRequest.pseudoId != PseudoIdFirstLineInherited);
  DCHECK(state.parentStyle());

  SelectorFilterParentScope::ensureParentStackIsPushed();

  Element* pseudoElement = element.pseudoElement(pseudoStyleRequest.pseudoId);

  const ComputedStyle* baseComputedStyle =
      calculateBaseComputedStyle(state, pseudoElement);

  if (baseComputedStyle) {
    state.setStyle(ComputedStyle::clone(*baseComputedStyle));
  } else if (pseudoStyleRequest.allowsInheritance(state.parentStyle())) {
    RefPtr<ComputedStyle> style = ComputedStyle::create();
    style->inheritFrom(*state.parentStyle());
    state.setStyle(std::move(style));
  } else {
    state.setStyle(initialStyleForElement());
    state.setParentStyle(ComputedStyle::clone(*state.style()));
  }

  state.style()->setStyleType(pseudoStyleRequest.pseudoId);

  // Since we don't use pseudo-elements in any of our quirk/print
  // user agent rules, don't waste time walking those rules.

  if (!baseComputedStyle) {
    // Check UA, user and author rules.
    ElementRuleCollector collector(state.elementContext(), m_selectorFilter,
                                   state.style());
    collector.setPseudoStyleRequest(pseudoStyleRequest);

    matchUARules(collector);
    matchAuthorRules(*state.element(), collector);
    collector.finishAddingAuthorRulesForTreeScope();

    if (m_tracker)
      addMatchedRulesToTracker(collector);

    if (!collector.matchedResult().hasMatchedProperties())
      return false;

    applyMatchedPropertiesAndCustomPropertyAnimations(
        state, collector.matchedResult(), pseudoElement);
    applyCallbackSelectors(state);

    // Cache our original display.
    state.style()->setOriginalDisplay(state.style()->display());

    // FIXME: Passing 0 as the Element* introduces a lot of complexity
    // in the adjustComputedStyle code.
    adjustComputedStyle(state, 0);

    updateBaseComputedStyle(state, pseudoElement);
  }

  // FIXME: The CSSWG wants to specify that the effects of animations are
  // applied before important rules, but this currently happens here as we
  // require adjustment to have happened before deciding which properties to
  // transition.
  if (applyAnimatedStandardProperties(state, pseudoElement))
    adjustComputedStyle(state, 0);

  document().styleEngine().incStyleForElementCount();
  INCREMENT_STYLE_STATS_COUNTER(document().styleEngine(), pseudoElementsStyled,
                                1);

  if (state.style()->hasViewportUnits())
    document().setHasViewportUnits();

  return true;
}

PassRefPtr<ComputedStyle> StyleResolver::pseudoStyleForElement(
    Element* element,
    const PseudoStyleRequest& pseudoStyleRequest,
    const ComputedStyle* parentStyle) {
  DCHECK(parentStyle);
  if (!element)
    return nullptr;

  StyleResolverState state(document(), element, parentStyle);
  if (!pseudoStyleForElementInternal(*element, pseudoStyleRequest, parentStyle,
                                     state)) {
    if (pseudoStyleRequest.type == PseudoStyleRequest::ForRenderer)
      return nullptr;
    return state.takeStyle();
  }

  if (PseudoElement* pseudoElement =
          element->pseudoElement(pseudoStyleRequest.pseudoId))
    setAnimationUpdateIfNeeded(state, *pseudoElement);

  // Now return the style.
  return state.takeStyle();
}

PassRefPtr<ComputedStyle> StyleResolver::styleForPage(int pageIndex) {
  // m_rootElementStyle will be set to the document style.
  StyleResolverState state(document(), document().documentElement());

  RefPtr<ComputedStyle> style = ComputedStyle::create();
  const ComputedStyle* rootElementStyle = state.rootElementStyle()
                                              ? state.rootElementStyle()
                                              : document().computedStyle();
  DCHECK(rootElementStyle);
  style->inheritFrom(*rootElementStyle);
  state.setStyle(std::move(style));

  PageRuleCollector collector(rootElementStyle, pageIndex);

  collector.matchPageRules(
      CSSDefaultStyleSheets::instance().defaultPrintStyle());

  if (ScopedStyleResolver* scopedResolver = document().scopedStyleResolver())
    scopedResolver->matchPageRules(collector);

  bool inheritedOnly = false;

  NeedsApplyPass needsApplyPass;
  const MatchResult& result = collector.matchedResult();
  applyMatchedProperties<AnimationPropertyPriority, UpdateNeedsApplyPass>(
      state, result.allRules(), false, inheritedOnly, needsApplyPass);
  applyMatchedProperties<HighPropertyPriority, CheckNeedsApplyPass>(
      state, result.allRules(), false, inheritedOnly, needsApplyPass);

  // If our font got dirtied, go ahead and update it now.
  updateFont(state);

  applyMatchedProperties<LowPropertyPriority, CheckNeedsApplyPass>(
      state, result.allRules(), false, inheritedOnly, needsApplyPass);

  loadPendingResources(state);

  // Now return the style.
  return state.takeStyle();
}

PassRefPtr<ComputedStyle> StyleResolver::initialStyleForElement() {
  RefPtr<ComputedStyle> style = ComputedStyle::create();
  FontBuilder fontBuilder(document());
  fontBuilder.setInitial(style->effectiveZoom());
  fontBuilder.createFont(document().styleEngine().fontSelector(), *style);
  return style.release();
}

PassRefPtr<ComputedStyle> StyleResolver::styleForText(Text* textNode) {
  DCHECK(textNode);

  Node* parentNode = LayoutTreeBuilderTraversal::parent(*textNode);
  if (!parentNode || !parentNode->computedStyle())
    return initialStyleForElement();
  return parentNode->mutableComputedStyle();
}

void StyleResolver::updateFont(StyleResolverState& state) {
  state.fontBuilder().createFont(document().styleEngine().fontSelector(),
                                 state.mutableStyleRef());
  state.setConversionFontSizes(CSSToLengthConversionData::FontSizes(
      state.style(), state.rootElementStyle()));
  state.setConversionZoom(state.style()->effectiveZoom());
}

void StyleResolver::addMatchedRulesToTracker(
    const ElementRuleCollector& collector) {
  collector.addMatchedRulesToTracker(m_tracker);
}

StyleRuleList* StyleResolver::styleRulesForElement(Element* element,
                                                   unsigned rulesToInclude) {
  DCHECK(element);
  StyleResolverState state(document(), element);
  ElementRuleCollector collector(state.elementContext(), m_selectorFilter,
                                 state.style());
  collector.setMode(SelectorChecker::CollectingStyleRules);
  collectPseudoRulesForElement(*element, collector, PseudoIdNone,
                               rulesToInclude);
  return collector.matchedStyleRuleList();
}

CSSRuleList* StyleResolver::pseudoCSSRulesForElement(Element* element,
                                                     PseudoId pseudoId,
                                                     unsigned rulesToInclude) {
  DCHECK(element);
  StyleResolverState state(document(), element);
  ElementRuleCollector collector(state.elementContext(), m_selectorFilter,
                                 state.style());
  collector.setMode(SelectorChecker::CollectingCSSRules);
  collectPseudoRulesForElement(*element, collector, pseudoId, rulesToInclude);

  if (m_tracker)
    addMatchedRulesToTracker(collector);
  return collector.matchedCSSRuleList();
}

CSSRuleList* StyleResolver::cssRulesForElement(Element* element,
                                               unsigned rulesToInclude) {
  return pseudoCSSRulesForElement(element, PseudoIdNone, rulesToInclude);
}

void StyleResolver::collectPseudoRulesForElement(
    const Element& element,
    ElementRuleCollector& collector,
    PseudoId pseudoId,
    unsigned rulesToInclude) {
  collector.setPseudoStyleRequest(PseudoStyleRequest(pseudoId));

  if (rulesToInclude & UAAndUserCSSRules)
    matchUARules(collector);

  if (rulesToInclude & AuthorCSSRules) {
    collector.setSameOriginOnly(!(rulesToInclude & CrossOriginCSSRules));
    collector.setIncludeEmptyRules(rulesToInclude & EmptyCSSRules);
    matchAuthorRules(element, collector);
  }
}

bool StyleResolver::applyAnimatedStandardProperties(
    StyleResolverState& state,
    const Element* animatingElement) {
  Element* element = state.element();
  DCHECK(element);

  // The animating element may be this element, or its pseudo element. It is
  // null when calculating the style for a potential pseudo element that has
  // yet to be created.
  DCHECK(animatingElement == element || !animatingElement ||
         animatingElement->parentOrShadowHostElement() == element);

  if (state.style()->animations() ||
      (animatingElement && animatingElement->hasAnimations())) {
    if (!state.isAnimationInterpolationMapReady())
      calculateAnimationUpdate(state, animatingElement);
  } else if (!state.style()->transitions()) {
    return false;
  }

  CSSAnimations::calculateCompositorAnimationUpdate(
      state.animationUpdate(), animatingElement, *element, *state.style(),
      state.parentStyle(), wasViewportResized());
  CSSAnimations::calculateTransitionUpdate(state.animationUpdate(),
                                           animatingElement, *state.style());

  CSSAnimations::snapshotCompositorKeyframes(
      *element, state.animationUpdate(), *state.style(), state.parentStyle());

  if (state.animationUpdate().isEmpty())
    return false;

  if (state.style()->insideLink() != EInsideLink::kNotInsideLink) {
    DCHECK(state.applyPropertyToRegularStyle());
    state.setApplyPropertyToVisitedLinkStyle(true);
  }

  const ActiveInterpolationsMap& activeInterpolationsMapForAnimations =
      state.animationUpdate().activeInterpolationsForAnimations();
  const ActiveInterpolationsMap& activeInterpolationsMapForTransitions =
      state.animationUpdate().activeInterpolationsForTransitions();
  // TODO(crbug.com/644148): Apply animations on custom properties.
  applyAnimatedProperties<HighPropertyPriority>(
      state, activeInterpolationsMapForAnimations);
  applyAnimatedProperties<HighPropertyPriority>(
      state, activeInterpolationsMapForTransitions);

  updateFont(state);

  applyAnimatedProperties<LowPropertyPriority>(
      state, activeInterpolationsMapForAnimations);
  applyAnimatedProperties<LowPropertyPriority>(
      state, activeInterpolationsMapForTransitions);

  // Start loading resources used by animations.
  loadPendingResources(state);

  DCHECK(!state.fontBuilder().fontDirty());

  state.setApplyPropertyToVisitedLinkStyle(false);

  return true;
}

StyleRuleKeyframes* StyleResolver::findKeyframesRule(
    const Element* element,
    const AtomicString& animationName) {
  HeapVector<Member<ScopedStyleResolver>, 8> resolvers;
  collectScopedResolversForHostedShadowTrees(*element, resolvers);
  if (ScopedStyleResolver* scopedResolver =
          element->treeScope().scopedStyleResolver())
    resolvers.push_back(scopedResolver);

  for (auto& resolver : resolvers) {
    if (StyleRuleKeyframes* keyframesRule =
            resolver->keyframeStylesForAnimation(animationName.impl()))
      return keyframesRule;
  }

  for (auto& resolver : resolvers)
    resolver->setHasUnresolvedKeyframesRule();
  return nullptr;
}

template <CSSPropertyPriority priority>
void StyleResolver::applyAnimatedProperties(
    StyleResolverState& state,
    const ActiveInterpolationsMap& activeInterpolationsMap) {
  // TODO(alancutter): Don't apply presentation attribute animations here,
  // they should instead apply in
  // SVGElement::collectStyleForPresentationAttribute().
  for (const auto& entry : activeInterpolationsMap) {
    CSSPropertyID property = entry.key.isCSSProperty()
                                 ? entry.key.cssProperty()
                                 : entry.key.presentationAttribute();
    if (!CSSPropertyPriorityData<priority>::propertyHasPriority(property))
      continue;
    const Interpolation& interpolation = *entry.value.front();
    if (interpolation.isInvalidatableInterpolation()) {
      CSSInterpolationTypesMap map(state.document().propertyRegistry());
      InterpolationEnvironment environment(map, state);
      InvalidatableInterpolation::applyStack(entry.value, environment);
    } else {
      // TODO(alancutter): Remove this old code path once animations have
      // completely migrated to InterpolationTypes.
      toLegacyStyleInterpolation(interpolation).apply(state);
    }
  }
}

static inline bool isValidCueStyleProperty(CSSPropertyID id) {
  switch (id) {
    case CSSPropertyBackground:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyColor:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontSize:
    case CSSPropertyFontStretch:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontWeight:
    case CSSPropertyLineHeight:
    case CSSPropertyOpacity:
    case CSSPropertyOutline:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyVisibility:
    case CSSPropertyWhiteSpace:
    // FIXME: 'text-decoration' shorthand to be handled when available.
    // See https://chromiumcodereview.appspot.com/19516002 for details.
    case CSSPropertyTextDecoration:
    case CSSPropertyTextShadow:
    case CSSPropertyBorderStyle:
      return true;
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyTextDecorationSkip:
      DCHECK(RuntimeEnabledFeatures::css3TextDecorationsEnabled());
      return true;
    case CSSPropertyFontVariationSettings:
      DCHECK(RuntimeEnabledFeatures::cssVariableFontsEnabled());
      return true;
    default:
      break;
  }
  return false;
}

static inline bool isValidFirstLetterStyleProperty(CSSPropertyID id) {
  switch (id) {
    // Valid ::first-letter properties listed in spec:
    // http://www.w3.org/TR/css3-selectors/#application-in-css
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyColor:
    case CSSPropertyFloat:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontKerning:
    case CSSPropertyFontSize:
    case CSSPropertyFontStretch:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontVariantNumeric:
    case CSSPropertyFontWeight:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLineHeight:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyTextTransform:
    case CSSPropertyVerticalAlign:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitBorderAfter:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitBorderBefore:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderEnd:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitBorderStart:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWordSpacing:
      return true;
    case CSSPropertyFontVariationSettings:
      DCHECK(RuntimeEnabledFeatures::cssVariableFontsEnabled());
      return true;
    case CSSPropertyTextDecoration:
      DCHECK(!RuntimeEnabledFeatures::css3TextDecorationsEnabled());
      return true;
    case CSSPropertyTextDecorationColor:
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextDecorationSkip:
      DCHECK(RuntimeEnabledFeatures::css3TextDecorationsEnabled());
      return true;

    // text-shadow added in text decoration spec:
    // http://www.w3.org/TR/css-text-decor-3/#text-shadow-property
    case CSSPropertyTextShadow:
    // box-shadox added in CSS3 backgrounds spec:
    // http://www.w3.org/TR/css3-background/#placement
    case CSSPropertyBoxShadow:
    // Properties that we currently support outside of spec.
    case CSSPropertyVisibility:
      return true;

    default:
      return false;
  }
}

static bool shouldIgnoreTextTrackAuthorStyle(const Document& document) {
  Settings* settings = document.settings();
  if (!settings)
    return false;
  // Ignore author specified settings for text tracks when any of the user
  // settings are present.
  if (!settings->getTextTrackBackgroundColor().isEmpty() ||
      !settings->getTextTrackFontFamily().isEmpty() ||
      !settings->getTextTrackFontStyle().isEmpty() ||
      !settings->getTextTrackFontVariant().isEmpty() ||
      !settings->getTextTrackTextColor().isEmpty() ||
      !settings->getTextTrackTextShadow().isEmpty() ||
      !settings->getTextTrackTextSize().isEmpty())
    return true;
  return false;
}

static inline bool isPropertyInWhitelist(
    PropertyWhitelistType propertyWhitelistType,
    CSSPropertyID property,
    const Document& document) {
  if (propertyWhitelistType == PropertyWhitelistNone)
    return true;  // Early bail for the by far most common case.

  if (propertyWhitelistType == PropertyWhitelistFirstLetter)
    return isValidFirstLetterStyleProperty(property);

  if (propertyWhitelistType == PropertyWhitelistCue)
    return isValidCueStyleProperty(property) &&
           !shouldIgnoreTextTrackAuthorStyle(document);

  NOTREACHED();
  return true;
}

// This method expands the 'all' shorthand property to longhand properties
// and applies the expanded longhand properties.
template <CSSPropertyPriority priority>
void StyleResolver::applyAllProperty(
    StyleResolverState& state,
    const CSSValue& allValue,
    bool inheritedOnly,
    PropertyWhitelistType propertyWhitelistType) {
  // The 'all' property doesn't apply to variables:
  // https://drafts.csswg.org/css-variables/#defining-variables
  if (priority == ResolveVariables)
    return;

  unsigned startCSSProperty = CSSPropertyPriorityData<priority>::first();
  unsigned endCSSProperty = CSSPropertyPriorityData<priority>::last();

  for (unsigned i = startCSSProperty; i <= endCSSProperty; ++i) {
    CSSPropertyID propertyId = static_cast<CSSPropertyID>(i);

    // StyleBuilder does not allow any expanded shorthands.
    if (isShorthandProperty(propertyId))
      continue;

    // all shorthand spec says:
    // The all property is a shorthand that resets all CSS properties
    // except direction and unicode-bidi.
    // c.f. http://dev.w3.org/csswg/css-cascade/#all-shorthand
    // We skip applyProperty when a given property is unicode-bidi or
    // direction.
    if (!CSSProperty::isAffectedByAllProperty(propertyId))
      continue;

    if (!isPropertyInWhitelist(propertyWhitelistType, propertyId, document()))
      continue;

    // When hitting matched properties' cache, only inherited properties will be
    // applied.
    if (inheritedOnly && !CSSPropertyMetadata::isInheritedProperty(propertyId))
      continue;

    StyleBuilder::applyProperty(propertyId, state, allValue);
  }
}

template <CSSPropertyPriority priority,
          StyleResolver::ShouldUpdateNeedsApplyPass shouldUpdateNeedsApplyPass>
void StyleResolver::applyPropertiesForApplyAtRule(
    StyleResolverState& state,
    const CSSValue& value,
    bool isImportant,
    NeedsApplyPass& needsApplyPass,
    PropertyWhitelistType propertyWhitelistType) {
  state.style()->setHasVariableReferenceFromNonInheritedProperty();
  if (!state.style()->inheritedVariables())
    return;
  const String& name = toCSSCustomIdentValue(value).value();
  const StylePropertySet* propertySet =
      state.customPropertySetForApplyAtRule(name);
  bool inheritedOnly = false;
  if (propertySet) {
    applyProperties<priority, shouldUpdateNeedsApplyPass>(
        state, propertySet, isImportant, inheritedOnly, needsApplyPass,
        propertyWhitelistType);
  }
}

template <CSSPropertyPriority priority,
          StyleResolver::ShouldUpdateNeedsApplyPass shouldUpdateNeedsApplyPass>
void StyleResolver::applyProperties(
    StyleResolverState& state,
    const StylePropertySet* properties,
    bool isImportant,
    bool inheritedOnly,
    NeedsApplyPass& needsApplyPass,
    PropertyWhitelistType propertyWhitelistType) {
  unsigned propertyCount = properties->propertyCount();
  for (unsigned i = 0; i < propertyCount; ++i) {
    StylePropertySet::PropertyReference current = properties->propertyAt(i);
    CSSPropertyID property = current.id();

    if (property == CSSPropertyApplyAtRule) {
      DCHECK(!inheritedOnly);
      applyPropertiesForApplyAtRule<priority, shouldUpdateNeedsApplyPass>(
          state, current.value(), isImportant, needsApplyPass,
          propertyWhitelistType);
      continue;
    }

    if (property == CSSPropertyAll && isImportant == current.isImportant()) {
      if (shouldUpdateNeedsApplyPass) {
        needsApplyPass.set(AnimationPropertyPriority, isImportant);
        needsApplyPass.set(HighPropertyPriority, isImportant);
        needsApplyPass.set(LowPropertyPriority, isImportant);
      }
      applyAllProperty<priority>(state, current.value(), inheritedOnly,
                                 propertyWhitelistType);
      continue;
    }

    if (shouldUpdateNeedsApplyPass)
      needsApplyPass.set(priorityForProperty(property), current.isImportant());

    if (isImportant != current.isImportant())
      continue;

    if (!isPropertyInWhitelist(propertyWhitelistType, property, document()))
      continue;

    if (inheritedOnly && !current.isInherited()) {
      // If the property value is explicitly inherited, we need to apply further
      // non-inherited properties as they might override the value inherited
      // here. For this reason we don't allow declarations with explicitly
      // inherited properties to be cached.
      DCHECK(!current.value().isInheritedValue());
      continue;
    }

    if (!CSSPropertyPriorityData<priority>::propertyHasPriority(property))
      continue;

    StyleBuilder::applyProperty(property, state, current.value());
  }
}

template <CSSPropertyPriority priority,
          StyleResolver::ShouldUpdateNeedsApplyPass shouldUpdateNeedsApplyPass>
void StyleResolver::applyMatchedProperties(StyleResolverState& state,
                                           const MatchedPropertiesRange& range,
                                           bool isImportant,
                                           bool inheritedOnly,
                                           NeedsApplyPass& needsApplyPass) {
  if (range.isEmpty())
    return;

  if (!shouldUpdateNeedsApplyPass && !needsApplyPass.get(priority, isImportant))
    return;

  if (state.style()->insideLink() != EInsideLink::kNotInsideLink) {
    for (const auto& matchedProperties : range) {
      unsigned linkMatchType = matchedProperties.m_types.linkMatchType;
      // FIXME: It would be nicer to pass these as arguments but that requires
      // changes in many places.
      state.setApplyPropertyToRegularStyle(linkMatchType &
                                           CSSSelector::MatchLink);
      state.setApplyPropertyToVisitedLinkStyle(linkMatchType &
                                               CSSSelector::MatchVisited);

      applyProperties<priority, shouldUpdateNeedsApplyPass>(
          state, matchedProperties.properties.get(), isImportant, inheritedOnly,
          needsApplyPass, static_cast<PropertyWhitelistType>(
                              matchedProperties.m_types.whitelistType));
    }
    state.setApplyPropertyToRegularStyle(true);
    state.setApplyPropertyToVisitedLinkStyle(false);
    return;
  }
  for (const auto& matchedProperties : range) {
    applyProperties<priority, shouldUpdateNeedsApplyPass>(
        state, matchedProperties.properties.get(), isImportant, inheritedOnly,
        needsApplyPass, static_cast<PropertyWhitelistType>(
                            matchedProperties.m_types.whitelistType));
  }
}

static unsigned computeMatchedPropertiesHash(
    const MatchedProperties* properties,
    unsigned size) {
  return StringHasher::hashMemory(properties, sizeof(MatchedProperties) * size);
}

void StyleResolver::invalidateMatchedPropertiesCache() {
  m_matchedPropertiesCache.clear();
}

void StyleResolver::setResizedForViewportUnits() {
  DCHECK(!m_wasViewportResized);
  m_wasViewportResized = true;
  document().styleEngine().updateActiveStyle();
  m_matchedPropertiesCache.clearViewportDependent();
}

void StyleResolver::clearResizedForViewportUnits() {
  m_wasViewportResized = false;
}

void StyleResolver::applyMatchedPropertiesAndCustomPropertyAnimations(
    StyleResolverState& state,
    const MatchResult& matchResult,
    const Element* animatingElement) {
  CacheSuccess cacheSuccess = applyMatchedCache(state, matchResult);
  NeedsApplyPass needsApplyPass;
  if (!cacheSuccess.isFullCacheHit()) {
    applyCustomProperties(state, matchResult, false, cacheSuccess,
                          needsApplyPass);
    applyMatchedAnimationProperties(state, matchResult, cacheSuccess,
                                    needsApplyPass);
  }
  if (state.style()->animations() ||
      (animatingElement && animatingElement->hasAnimations())) {
    calculateAnimationUpdate(state, animatingElement);
    if (state.isAnimatingCustomProperties()) {
      cacheSuccess.setFailed();
      applyCustomProperties(state, matchResult, true, cacheSuccess,
                            needsApplyPass);
    }
  }
  if (!cacheSuccess.isFullCacheHit()) {
    applyMatchedStandardProperties(state, matchResult, cacheSuccess,
                                   needsApplyPass);
  }
}

StyleResolver::CacheSuccess StyleResolver::applyMatchedCache(
    StyleResolverState& state,
    const MatchResult& matchResult) {
  const Element* element = state.element();
  DCHECK(element);

  unsigned cacheHash =
      matchResult.isCacheable()
          ? computeMatchedPropertiesHash(matchResult.matchedProperties().data(),
                                         matchResult.matchedProperties().size())
          : 0;
  bool isInheritedCacheHit = false;
  bool isNonInheritedCacheHit = false;
  const CachedMatchedProperties* cachedMatchedProperties =
      cacheHash
          ? m_matchedPropertiesCache.find(cacheHash, state,
                                          matchResult.matchedProperties())
          : nullptr;

  if (cachedMatchedProperties && MatchedPropertiesCache::isCacheable(state)) {
    INCREMENT_STYLE_STATS_COUNTER(document().styleEngine(),
                                  matchedPropertyCacheHit, 1);
    // We can build up the style by copying non-inherited properties from an
    // earlier style object built using the same exact style declarations. We
    // then only need to apply the inherited properties, if any, as their values
    // can depend on the element context. This is fast and saves memory by
    // reusing the style data structures.
    state.style()->copyNonInheritedFromCached(
        *cachedMatchedProperties->computedStyle);
    if (state.parentStyle()->inheritedDataShared(
            *cachedMatchedProperties->parentComputedStyle) &&
        !isAtShadowBoundary(element) &&
        (!state.distributedToInsertionPoint() ||
         state.style()->userModify() == READ_ONLY)) {
      INCREMENT_STYLE_STATS_COUNTER(document().styleEngine(),
                                    matchedPropertyCacheInheritedHit, 1);

      EInsideLink linkStatus = state.style()->insideLink();
      // If the cache item parent style has identical inherited properties to
      // the current parent style then the resulting style will be identical
      // too. We copy the inherited properties over from the cache and are done.
      state.style()->inheritFrom(*cachedMatchedProperties->computedStyle);

      // Unfortunately the link status is treated like an inherited property. We
      // need to explicitly restore it.
      state.style()->setInsideLink(linkStatus);

      updateFont(state);
      isInheritedCacheHit = true;
    }

    isNonInheritedCacheHit = true;
  }

  return CacheSuccess(isInheritedCacheHit, isNonInheritedCacheHit, cacheHash,
                      cachedMatchedProperties);
}

void StyleResolver::applyCustomProperties(StyleResolverState& state,
                                          const MatchResult& matchResult,
                                          bool applyAnimations,
                                          const CacheSuccess& cacheSuccess,
                                          NeedsApplyPass& needsApplyPass) {
  DCHECK(!cacheSuccess.isFullCacheHit());
  bool applyInheritedOnly = cacheSuccess.shouldApplyInheritedOnly();

  // TODO(leviw): We need the proper bit for tracking whether we need to do
  // this work.
  applyMatchedProperties<ResolveVariables, UpdateNeedsApplyPass>(
      state, matchResult.authorRules(), false, applyInheritedOnly,
      needsApplyPass);
  applyMatchedProperties<ResolveVariables, CheckNeedsApplyPass>(
      state, matchResult.authorRules(), true, applyInheritedOnly,
      needsApplyPass);
  if (applyAnimations) {
    applyAnimatedProperties<ResolveVariables>(
        state, state.animationUpdate().activeInterpolationsForAnimations());
  }
  // TODO(leviw): stop recalculating every time
  CSSVariableResolver::resolveVariableDefinitions(state);

  if (RuntimeEnabledFeatures::cssApplyAtRulesEnabled()) {
    if (cacheCustomPropertiesForApplyAtRules(state,
                                             matchResult.authorRules())) {
      applyMatchedProperties<ResolveVariables, UpdateNeedsApplyPass>(
          state, matchResult.authorRules(), false, applyInheritedOnly,
          needsApplyPass);
      applyMatchedProperties<ResolveVariables, CheckNeedsApplyPass>(
          state, matchResult.authorRules(), true, applyInheritedOnly,
          needsApplyPass);
      if (applyAnimations) {
        applyAnimatedProperties<ResolveVariables>(
            state, state.animationUpdate().activeInterpolationsForAnimations());
      }
      CSSVariableResolver::resolveVariableDefinitions(state);
    }
  }
}

void StyleResolver::applyMatchedAnimationProperties(
    StyleResolverState& state,
    const MatchResult& matchResult,
    const CacheSuccess& cacheSuccess,
    NeedsApplyPass& needsApplyPass) {
  DCHECK(!cacheSuccess.isFullCacheHit());
  bool applyInheritedOnly = cacheSuccess.shouldApplyInheritedOnly();

  applyMatchedProperties<AnimationPropertyPriority, UpdateNeedsApplyPass>(
      state, matchResult.allRules(), false, applyInheritedOnly, needsApplyPass);
  applyMatchedProperties<AnimationPropertyPriority, CheckNeedsApplyPass>(
      state, matchResult.allRules(), true, applyInheritedOnly, needsApplyPass);
}

void StyleResolver::calculateAnimationUpdate(StyleResolverState& state,
                                             const Element* animatingElement) {
  DCHECK(state.style()->animations() ||
         (animatingElement && animatingElement->hasAnimations()));
  DCHECK(!state.isAnimationInterpolationMapReady());

  CSSAnimations::calculateAnimationUpdate(
      state.animationUpdate(), animatingElement, *state.element(),
      *state.style(), state.parentStyle(), this);

  state.setIsAnimationInterpolationMapReady();

  if (state.isAnimatingCustomProperties())
    return;
  for (const auto& propertyHandle :
       state.animationUpdate().activeInterpolationsForAnimations().keys()) {
    if (CSSAnimations::isCustomPropertyHandle(propertyHandle)) {
      state.setIsAnimatingCustomProperties(true);
      return;
    }
  }
}

void StyleResolver::applyMatchedStandardProperties(
    StyleResolverState& state,
    const MatchResult& matchResult,
    const CacheSuccess& cacheSuccess,
    NeedsApplyPass& needsApplyPass) {
  INCREMENT_STYLE_STATS_COUNTER(document().styleEngine(), matchedPropertyApply,
                                1);

  DCHECK(!cacheSuccess.isFullCacheHit());
  bool applyInheritedOnly = cacheSuccess.shouldApplyInheritedOnly();

  // Now we have all of the matched rules in the appropriate order. Walk the
  // rules and apply high-priority properties first, i.e., those properties that
  // other properties depend on.  The order is (1) high-priority not important,
  // (2) high-priority important, (3) normal not important and (4) normal
  // important.
  applyMatchedProperties<HighPropertyPriority, CheckNeedsApplyPass>(
      state, matchResult.allRules(), false, applyInheritedOnly, needsApplyPass);
  for (auto range : ImportantAuthorRanges(matchResult)) {
    applyMatchedProperties<HighPropertyPriority, CheckNeedsApplyPass>(
        state, range, true, applyInheritedOnly, needsApplyPass);
  }
  applyMatchedProperties<HighPropertyPriority, CheckNeedsApplyPass>(
      state, matchResult.uaRules(), true, applyInheritedOnly, needsApplyPass);

  if (UNLIKELY(isSVGForeignObjectElement(state.element()))) {
    // LayoutSVGRoot handles zooming for the whole SVG subtree, so foreignObject
    // content should not be scaled again.
    //
    // FIXME: The following hijacks the zoom property for foreignObject so that
    // children of foreignObject get the correct font-size in case of zooming.
    // 'zoom' has HighPropertyPriority, along with other font-related properties
    // used as input to the FontBuilder, so resetting it here may cause the
    // FontBuilder to recompute the font used as inheritable font for
    // foreignObject content. If we want to support zoom on foreignObject we'll
    // need to find another way of handling the SVG zoom model.
    state.setEffectiveZoom(ComputedStyle::initialZoom());
  }

  if (cacheSuccess.cachedMatchedProperties &&
      cacheSuccess.cachedMatchedProperties->computedStyle->effectiveZoom() !=
          state.style()->effectiveZoom()) {
    state.fontBuilder().didChangeEffectiveZoom();
    applyInheritedOnly = false;
  }

  // If our font got dirtied, go ahead and update it now.
  updateFont(state);

  // Many properties depend on the font. If it changes we just apply all
  // properties.
  if (cacheSuccess.cachedMatchedProperties &&
      cacheSuccess.cachedMatchedProperties->computedStyle
              ->getFontDescription() != state.style()->getFontDescription())
    applyInheritedOnly = false;

  // Registered custom properties are computed after high priority properties.
  CSSVariableResolver::computeRegisteredVariables(state);

  // Now do the normal priority UA properties.
  applyMatchedProperties<LowPropertyPriority, CheckNeedsApplyPass>(
      state, matchResult.uaRules(), false, applyInheritedOnly, needsApplyPass);

  // Cache the UA properties to pass them to LayoutTheme in adjustComputedStyle.
  state.cacheUserAgentBorderAndBackground();

  // Now do the author and user normal priority properties and all the
  // !important properties.
  applyMatchedProperties<LowPropertyPriority, CheckNeedsApplyPass>(
      state, matchResult.authorRules(), false, applyInheritedOnly,
      needsApplyPass);
  for (auto range : ImportantAuthorRanges(matchResult)) {
    applyMatchedProperties<LowPropertyPriority, CheckNeedsApplyPass>(
        state, range, true, applyInheritedOnly, needsApplyPass);
  }
  applyMatchedProperties<LowPropertyPriority, CheckNeedsApplyPass>(
      state, matchResult.uaRules(), true, applyInheritedOnly, needsApplyPass);

  if (state.style()->hasAppearance() && !applyInheritedOnly) {
    // Check whether the final border and background differs from the cached UA
    // ones.  When there is a partial match in the MatchedPropertiesCache, these
    // flags will already be set correctly and the value stored in
    // cacheUserAgentBorderAndBackground is incorrect, so doing this check again
    // would give the wrong answer.
    state.style()->setHasAuthorBackground(hasAuthorBackground(state));
    state.style()->setHasAuthorBorder(hasAuthorBorder(state));
  }

  loadPendingResources(state);

  if (!state.isAnimatingCustomProperties() &&
      !cacheSuccess.cachedMatchedProperties && cacheSuccess.cacheHash &&
      MatchedPropertiesCache::isCacheable(state)) {
    INCREMENT_STYLE_STATS_COUNTER(document().styleEngine(),
                                  matchedPropertyCacheAdded, 1);
    m_matchedPropertiesCache.add(*state.style(), *state.parentStyle(),
                                 cacheSuccess.cacheHash,
                                 matchResult.matchedProperties());
  }

  DCHECK(!state.fontBuilder().fontDirty());
}

bool StyleResolver::hasAuthorBackground(const StyleResolverState& state) {
  const CachedUAStyle* cachedUAStyle = state.cachedUAStyle();
  if (!cachedUAStyle)
    return false;

  FillLayer oldFill = cachedUAStyle->backgroundLayers;
  FillLayer newFill = state.style()->backgroundLayers();
  // Exclude background-repeat from comparison by resetting it.
  oldFill.setRepeatX(NoRepeatFill);
  oldFill.setRepeatY(NoRepeatFill);
  newFill.setRepeatX(NoRepeatFill);
  newFill.setRepeatY(NoRepeatFill);

  return (oldFill != newFill ||
          cachedUAStyle->backgroundColor != state.style()->backgroundColor());
}

bool StyleResolver::hasAuthorBorder(const StyleResolverState& state) {
  const CachedUAStyle* cachedUAStyle = state.cachedUAStyle();
  return cachedUAStyle && (cachedUAStyle->border != state.style()->border());
}

void StyleResolver::applyCallbackSelectors(StyleResolverState& state) {
  RuleSet* watchedSelectorsRuleSet =
      document().styleEngine().watchedSelectorsRuleSet();
  if (!watchedSelectorsRuleSet)
    return;

  ElementRuleCollector collector(state.elementContext(), m_selectorFilter,
                                 state.style());
  collector.setMode(SelectorChecker::CollectingStyleRules);
  collector.setIncludeEmptyRules(true);

  MatchRequest matchRequest(watchedSelectorsRuleSet);
  collector.collectMatchingRules(matchRequest);
  collector.sortAndTransferMatchedRules();

  if (m_tracker)
    addMatchedRulesToTracker(collector);

  StyleRuleList* rules = collector.matchedStyleRuleList();
  if (!rules)
    return;
  for (size_t i = 0; i < rules->size(); i++)
    state.style()->addCallbackSelector(
        rules->at(i)->selectorList().selectorsText());
}

void StyleResolver::computeFont(ComputedStyle* style,
                                const StylePropertySet& propertySet) {
  CSSPropertyID properties[] = {
      CSSPropertyFontSize,   CSSPropertyFontFamily,      CSSPropertyFontStretch,
      CSSPropertyFontStyle,  CSSPropertyFontVariantCaps, CSSPropertyFontWeight,
      CSSPropertyLineHeight,
  };

  // TODO(timloh): This is weird, the style is being used as its own parent
  StyleResolverState state(document(), nullptr, style);
  state.setStyle(style);

  for (CSSPropertyID property : properties) {
    if (property == CSSPropertyLineHeight)
      updateFont(state);
    StyleBuilder::applyProperty(property, state,
                                *propertySet.getPropertyCSSValue(property));
  }
}

void StyleResolver::updateMediaType() {
  if (FrameView* view = document().view()) {
    bool wasPrint = m_printMediaType;
    m_printMediaType =
        equalIgnoringCase(view->mediaType(), MediaTypeNames::print);
    if (wasPrint != m_printMediaType)
      m_matchedPropertiesCache.clearViewportDependent();
  }
}

DEFINE_TRACE(StyleResolver) {
  visitor->trace(m_matchedPropertiesCache);
  visitor->trace(m_selectorFilter);
  visitor->trace(m_styleSharingLists);
  visitor->trace(m_document);
  visitor->trace(m_tracker);
}

}  // namespace blink
