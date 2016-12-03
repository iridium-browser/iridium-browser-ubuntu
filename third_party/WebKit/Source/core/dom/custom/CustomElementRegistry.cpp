// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementRegistry.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptCustomElementDefinitionBuilder.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementRegistrationOptions.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/custom/CEReactionsScope.h"
#include "core/dom/custom/CustomElement.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/CustomElementDefinitionBuilder.h"
#include "core/dom/custom/CustomElementDescriptor.h"
#include "core/dom/custom/CustomElementUpgradeReaction.h"
#include "core/dom/custom/CustomElementUpgradeSorter.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "wtf/Allocator.h"

namespace blink {

// Returns true if |name| is invalid.
static bool throwIfInvalidName(
    const AtomicString& name,
    ExceptionState& exceptionState)
{
    if (CustomElement::isValidName(name))
        return false;
    exceptionState.throwDOMException(
        SyntaxError,
        "\"" + name + "\" is not a valid custom element name");
    return true;
}


class CustomElementRegistry::NameIsBeingDefined final {
    STACK_ALLOCATED();
    DISALLOW_IMPLICIT_CONSTRUCTORS(NameIsBeingDefined);
public:
    NameIsBeingDefined(
        CustomElementRegistry* registry,
        const AtomicString& name)
        : m_registry(registry)
        , m_name(name)
    {
        DCHECK(!m_registry->m_namesBeingDefined.contains(name));
        m_registry->m_namesBeingDefined.add(name);
    }

    ~NameIsBeingDefined()
    {
        m_registry->m_namesBeingDefined.remove(m_name);
    }

private:
    Member<CustomElementRegistry> m_registry;
    const AtomicString& m_name;
};

CustomElementRegistry* CustomElementRegistry::create(
    const LocalDOMWindow* owner)
{
    CustomElementRegistry* registry = new CustomElementRegistry(owner);
    Document* document = owner->document();
    if (V0CustomElementRegistrationContext* v0 =
        document ? document->registrationContext() : nullptr)
        registry->entangle(v0);
    return registry;
}

CustomElementRegistry::CustomElementRegistry(const LocalDOMWindow* owner)
    : m_owner(owner)
    , m_v0 (new V0RegistrySet())
    , m_upgradeCandidates(new UpgradeCandidateMap())
{
}

DEFINE_TRACE(CustomElementRegistry)
{
    visitor->trace(m_definitions);
    visitor->trace(m_owner);
    visitor->trace(m_v0);
    visitor->trace(m_upgradeCandidates);
    visitor->trace(m_whenDefinedPromiseMap);
}

void CustomElementRegistry::define(
    ScriptState* scriptState,
    const AtomicString& name,
    const ScriptValue& constructor,
    const ElementRegistrationOptions& options,
    ExceptionState& exceptionState)
{
    ScriptCustomElementDefinitionBuilder builder(
        scriptState,
        this,
        constructor,
        exceptionState);
    define(name, builder, options, exceptionState);
}

// http://w3c.github.io/webcomponents/spec/custom/#dfn-element-definition
void CustomElementRegistry::define(
    const AtomicString& name,
    CustomElementDefinitionBuilder& builder,
    const ElementRegistrationOptions& options,
    ExceptionState& exceptionState)
{
    if (!builder.checkConstructorIntrinsics())
        return;

    if (throwIfInvalidName(name, exceptionState))
        return;

    if (m_namesBeingDefined.contains(name)) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "this name is already being defined in this registry");
        return;
    }
    NameIsBeingDefined defining(this, name);

    if (nameIsDefined(name) || v0NameIsDefined(name)) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "this name has already been used with this registry");
        return;
    }

    if (!builder.checkConstructorNotRegistered())
        return;

    // TODO(dominicc): Implement steps:
    // 5: localName
    // 6-7: extends processing

    // 8-9: observed attributes caching is done below, together with callbacks.
    // TODO(kojii): https://github.com/whatwg/html/issues/1373 for the ordering.
    // When it's resolved, revisit if this code needs changes.

    // TODO(dominicc): Add a test where the prototype getter destroys
    // the context.

    if (!builder.checkPrototype())
        return;

    // 8-9: observed attributes caching
    // 12-13: connected callback
    // 14-15: disconnected callback
    // 16-17: attribute changed callback

    if (!builder.rememberOriginalProperties())
        return;

    // TODO(dominicc): Add a test where retrieving the prototype
    // recursively calls define with the same name.

    CustomElementDescriptor descriptor(name, name);
    CustomElementDefinition* definition = builder.build(descriptor);
    CHECK(!exceptionState.hadException());
    CHECK(definition->descriptor() == descriptor);
    DefinitionMap::AddResult result =
        m_definitions.add(descriptor.name(), definition);
    CHECK(result.isNewEntry);

    HeapVector<Member<Element>> candidates;
    collectCandidates(descriptor, &candidates);
    for (Element* candidate : candidates)
        definition->enqueueUpgradeReaction(candidate);

    // 19: when-defined promise processing
    const auto& entry = m_whenDefinedPromiseMap.find(name);
    if (entry == m_whenDefinedPromiseMap.end())
        return;
    entry->value->resolve();
    m_whenDefinedPromiseMap.remove(entry);
}

// https://html.spec.whatwg.org/multipage/scripting.html#dom-customelementsregistry-get
ScriptValue CustomElementRegistry::get(const AtomicString& name)
{
    CustomElementDefinition* definition = definitionForName(name);
    if (!definition) {
        // Binding layer converts |ScriptValue()| to script specific value,
        // e.g. |undefined| for v8.
        return ScriptValue();
    }
    return definition->getConstructorForScript();
}

CustomElementDefinition* CustomElementRegistry::definitionFor(const CustomElementDescriptor& desc) const
{
    CustomElementDefinition* definition = definitionForName(desc.name());
    if (!definition)
        return nullptr;
    // The definition for a customized built-in element, such as
    // <button is="my-button"> should not be provided for an
    // autonomous element, such as <my-button>, even though the
    // name "my-button" matches.
    return definition->descriptor() == desc ? definition : nullptr;
}

bool CustomElementRegistry::nameIsDefined(const AtomicString& name) const
{
    return m_definitions.contains(name);
}

void CustomElementRegistry::entangle(V0CustomElementRegistrationContext* v0)
{
    m_v0->add(v0);
    v0->setV1(this);
}

bool CustomElementRegistry::v0NameIsDefined(const AtomicString& name)
{
    for (const auto& v0 : *m_v0) {
        if (v0->nameIsDefined(name))
            return true;
    }
    return false;
}

CustomElementDefinition* CustomElementRegistry::definitionForName(
    const AtomicString& name) const
{
    return m_definitions.get(name);
}

void CustomElementRegistry::addCandidate(Element* candidate)
{
    const AtomicString& name = candidate->localName();
    if (nameIsDefined(name) || v0NameIsDefined(name))
        return;
    UpgradeCandidateMap::iterator it = m_upgradeCandidates->find(name);
    UpgradeCandidateSet* set;
    if (it != m_upgradeCandidates->end()) {
        set = it->value;
    } else {
        set = m_upgradeCandidates->add(name, new UpgradeCandidateSet())
            .storedValue
            ->value;
    }
    set->add(candidate);
}

// https://html.spec.whatwg.org/multipage/scripting.html#dom-customelementsregistry-whendefined
ScriptPromise CustomElementRegistry::whenDefined(
    ScriptState* scriptState,
    const AtomicString& name,
    ExceptionState& exceptionState)
{
    if (throwIfInvalidName(name, exceptionState))
        return ScriptPromise();
    CustomElementDefinition* definition = definitionForName(name);
    if (definition)
        return ScriptPromise::castUndefined(scriptState);
    ScriptPromiseResolver* resolver = m_whenDefinedPromiseMap.get(name);
    if (resolver)
        return resolver->promise();
    ScriptPromiseResolver* newResolver =
        ScriptPromiseResolver::create(scriptState);
    m_whenDefinedPromiseMap.add(name, newResolver);
    return newResolver->promise();
}

void CustomElementRegistry::collectCandidates(
    const CustomElementDescriptor& desc,
    HeapVector<Member<Element>>* elements)
{
    UpgradeCandidateMap::iterator it = m_upgradeCandidates->find(desc.name());
    if (it == m_upgradeCandidates->end())
        return;
    CustomElementUpgradeSorter sorter;
    for (Element* element : *it.get()->value) {
        if (!element || !desc.matches(*element))
            continue;
        sorter.add(element);
    }

    m_upgradeCandidates->remove(it);

    Document* document = m_owner->document();
    if (!document)
        return;

    sorter.sorted(elements, document);
}

} // namespace blink
