// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/PopupMenuImpl.h"

#include "core/HTMLNames.h"
#include "core/css/CSSFontSelector.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLHRElement.h"
#include "core/html/HTMLOptGroupElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/PagePopup.h"
#include "platform/geometry/IntRect.h"
#include "platform/text/PlatformLocale.h"
#include "public/platform/Platform.h"
#include "public/web/WebColorChooser.h"
#include "web/ChromeClientImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

namespace {

const char* fontWeightToString(FontWeight weight)
{
    switch (weight) {
    case FontWeight100:
        return "100";
    case FontWeight200:
        return "200";
    case FontWeight300:
        return "300";
    case FontWeight400:
        return "400";
    case FontWeight500:
        return "500";
    case FontWeight600:
        return "600";
    case FontWeight700:
        return "700";
    case FontWeight800:
        return "800";
    case FontWeight900:
        return "900";
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

const char* fontVariantToString(FontVariant variant)
{
    switch (variant) {
    case FontVariantNormal:
        return "normal";
    case FontVariantSmallCaps:
        return "small-caps";
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

// TODO crbug.com/516675 Add stretch to serialization

const char* fontStyleToString(FontStyle style)
{
    switch (style) {
    case FontStyleNormal:
        return "normal";
    case FontStyleOblique:
        return "oblique";
    case FontStyleItalic:
        return "italic";
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

const char* textTransformToString(ETextTransform transform)
{
    switch (transform) {
    case CAPITALIZE:
        return "capitalize";
    case UPPERCASE:
        return "uppercase";
    case LOWERCASE:
        return "lowercase";
    case TTNONE:
        return "none";
    }
    ASSERT_NOT_REACHED();
    return "";
}

} // anonymous namespace

class PopupMenuCSSFontSelector : public CSSFontSelector, private CSSFontSelectorClient {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PopupMenuCSSFontSelector);
public:
    static PassRefPtrWillBeRawPtr<PopupMenuCSSFontSelector> create(Document* document, CSSFontSelector* ownerFontSelector)
    {
        return adoptRefWillBeNoop(new PopupMenuCSSFontSelector(document, ownerFontSelector));
    }

    ~PopupMenuCSSFontSelector();

    // We don't override willUseFontData() for now because the old PopupListBox
    // only worked with fonts loaded when opening the popup.
    PassRefPtr<FontData> getFontData(const FontDescription&, const AtomicString&) override;

    DECLARE_VIRTUAL_TRACE();

private:
    PopupMenuCSSFontSelector(Document*, CSSFontSelector*);

    void fontsNeedUpdate(CSSFontSelector*) override;

    RefPtrWillBeMember<CSSFontSelector> m_ownerFontSelector;
};

PopupMenuCSSFontSelector::PopupMenuCSSFontSelector(Document* document, CSSFontSelector* ownerFontSelector)
    : CSSFontSelector(document)
    , m_ownerFontSelector(ownerFontSelector)
{
    m_ownerFontSelector->registerForInvalidationCallbacks(this);
}

PopupMenuCSSFontSelector::~PopupMenuCSSFontSelector()
{
#if !ENABLE(OILPAN)
    m_ownerFontSelector->unregisterForInvalidationCallbacks(this);
#endif
}

PassRefPtr<FontData> PopupMenuCSSFontSelector::getFontData(const FontDescription& description, const AtomicString& name)
{
    return m_ownerFontSelector->getFontData(description, name);
}

void PopupMenuCSSFontSelector::fontsNeedUpdate(CSSFontSelector* fontSelector)
{
    dispatchInvalidationCallbacks();
}

DEFINE_TRACE(PopupMenuCSSFontSelector)
{
    visitor->trace(m_ownerFontSelector);
    CSSFontSelector::trace(visitor);
    CSSFontSelectorClient::trace(visitor);
}

// ----------------------------------------------------------------

class PopupMenuImpl::ItemIterationContext {
    STACK_ALLOCATED();
public:
    ItemIterationContext(const ComputedStyle& style, SharedBuffer* buffer)
        : m_baseStyle(style)
        , m_backgroundColor(style.visitedDependentColor(CSSPropertyBackgroundColor))
        , m_listIndex(0)
        , m_isInGroup(false)
        , m_buffer(buffer)
    {
        ASSERT(m_buffer);
#if OS(LINUX)
        // On other platforms, the <option> background color is the same as the
        // <select> background color. On Linux, that makes the <option>
        // background color very dark, so by default, try to use a lighter
        // background color for <option>s.
        if (LayoutTheme::theme().systemColor(CSSValueButtonface) == m_backgroundColor)
            m_backgroundColor = LayoutTheme::theme().systemColor(CSSValueMenu);
#endif
    }

    void serializeBaseStyle()
    {
        ASSERT(!m_isInGroup);
        PagePopupClient::addString("baseStyle: {", m_buffer);
        addProperty("backgroundColor", m_backgroundColor.serialized(), m_buffer);
        addProperty("color", baseStyle().visitedDependentColor(CSSPropertyColor).serialized(), m_buffer);
        addProperty("textTransform", String(textTransformToString(baseStyle().textTransform())), m_buffer);
        addProperty("fontSize", baseFont().computedPixelSize(), m_buffer);
        addProperty("fontStyle", String(fontStyleToString(baseFont().style())), m_buffer);
        addProperty("fontVariant", String(fontVariantToString(baseFont().variant())), m_buffer);

        PagePopupClient::addString("fontFamily: [", m_buffer);
        for (const FontFamily* f = &baseFont().family(); f; f = f->next()) {
            addJavaScriptString(f->family().string(), m_buffer);
            if (f->next())
                PagePopupClient::addString(",", m_buffer);
        }
        PagePopupClient::addString("]", m_buffer);
        PagePopupClient::addString("},\n", m_buffer);
    }

    Color backgroundColor() const { return m_isInGroup ? m_groupStyle->visitedDependentColor(CSSPropertyBackgroundColor) : m_backgroundColor; }
    // Do not use baseStyle() for background-color, use backgroundColor()
    // instead.
    const ComputedStyle& baseStyle() { return m_isInGroup ? *m_groupStyle : m_baseStyle; }
    const FontDescription& baseFont() { return m_isInGroup ? m_groupStyle->fontDescription() : m_baseStyle.fontDescription(); }
    void startGroupChildren(const ComputedStyle& groupStyle)
    {
        ASSERT(!m_isInGroup);
        PagePopupClient::addString("children: [", m_buffer);
        m_isInGroup = true;
        m_groupStyle = &groupStyle;
    }
    void finishGroupIfNecessary()
    {
        if (!m_isInGroup)
            return;
        PagePopupClient::addString("],},\n", m_buffer);
        m_isInGroup = false;
        m_groupStyle = nullptr;
    }

    const ComputedStyle& m_baseStyle;
    Color m_backgroundColor;
    const ComputedStyle* m_groupStyle;

    unsigned m_listIndex;
    bool m_isInGroup;
    SharedBuffer* m_buffer;
};

// ----------------------------------------------------------------

PassRefPtrWillBeRawPtr<PopupMenuImpl> PopupMenuImpl::create(ChromeClientImpl* chromeClient, HTMLSelectElement& ownerElement)
{
    return adoptRefWillBeNoop(new PopupMenuImpl(chromeClient, ownerElement));
}

PopupMenuImpl::PopupMenuImpl(ChromeClientImpl* chromeClient, HTMLSelectElement& ownerElement)
    : m_chromeClient(chromeClient)
    , m_ownerElement(ownerElement)
    , m_popup(nullptr)
    , m_needsUpdate(false)
{
}

PopupMenuImpl::~PopupMenuImpl()
{
    ASSERT(!m_popup);
}

DEFINE_TRACE(PopupMenuImpl)
{
    visitor->trace(m_ownerElement);
    PopupMenu::trace(visitor);
}

IntSize PopupMenuImpl::contentSize()
{
    return IntSize();
}

void PopupMenuImpl::writeDocument(SharedBuffer* data)
{
    HTMLSelectElement& ownerElement = *m_ownerElement;
    IntRect anchorRectInScreen = m_chromeClient->viewportToScreen(ownerElement.elementRectRelativeToViewport());

    PagePopupClient::addString("<!DOCTYPE html><head><meta charset='UTF-8'><style>\n", data);
    data->append(Platform::current()->loadResource("pickerCommon.css"));
    data->append(Platform::current()->loadResource("listPicker.css"));
    PagePopupClient::addString("</style></head><body><div id=main>Loading...</div><script>\n"
        "window.dialogArguments = {\n", data);
    addProperty("selectedIndex", ownerElement.optionToListIndex(ownerElement.selectedIndex()), data);
    const ComputedStyle* ownerStyle = ownerElement.computedStyle();
    ItemIterationContext context(*ownerStyle, data);
    context.serializeBaseStyle();
    PagePopupClient::addString("children: [\n", data);
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement>>& items = ownerElement.listItems();
    for (; context.m_listIndex < items.size(); ++context.m_listIndex) {
        Element& child = *items[context.m_listIndex];
        if (!isHTMLOptGroupElement(child.parentNode()))
            context.finishGroupIfNecessary();
        if (isHTMLOptionElement(child))
            addOption(context, toHTMLOptionElement(child));
        else if (isHTMLOptGroupElement(child))
            addOptGroup(context, toHTMLOptGroupElement(child));
        else if (isHTMLHRElement(child))
            addSeparator(context, toHTMLHRElement(child));
    }
    context.finishGroupIfNecessary();
    PagePopupClient::addString("],\n", data);

    addProperty("anchorRectInScreen", anchorRectInScreen, data);
    bool isRTL = !ownerStyle->isLeftToRightDirection();
    addProperty("isRTL", isRTL, data);
    addProperty("paddingStart", isRTL ? ownerElement.clientPaddingRight().toDouble() : ownerElement.clientPaddingLeft().toDouble(), data);
    PagePopupClient::addString("};\n", data);
    data->append(Platform::current()->loadResource("pickerCommon.js"));
    data->append(Platform::current()->loadResource("listPicker.js"));
    PagePopupClient::addString("</script></body>\n", data);
}

void PopupMenuImpl::addElementStyle(ItemIterationContext& context, HTMLElement& element)
{
    const ComputedStyle* style = m_ownerElement->itemComputedStyle(element);
    ASSERT(style);
    SharedBuffer* data = context.m_buffer;
    // TODO(tkent): We generate unnecessary "style: {\n},\n" even if no
    // additional style.
    PagePopupClient::addString("style: {\n", data);
    if (style->visibility() == HIDDEN)
        addProperty("visibility", String("hidden"), data);
    if (style->display() == NONE)
        addProperty("display", String("none"), data);
    const ComputedStyle& baseStyle = context.baseStyle();
    if (baseStyle.direction() != style->direction())
        addProperty("direction", String(style->direction() == RTL ? "rtl" : "ltr"), data);
    if (isOverride(style->unicodeBidi()))
        addProperty("unicodeBidi", String("bidi-override"), data);
    Color foregroundColor = style->visitedDependentColor(CSSPropertyColor);
    if (baseStyle.visitedDependentColor(CSSPropertyColor) != foregroundColor)
        addProperty("color", foregroundColor.serialized(), data);
    Color backgroundColor = style->visitedDependentColor(CSSPropertyBackgroundColor);
    if (context.backgroundColor() != backgroundColor && backgroundColor != Color::transparent)
        addProperty("backgroundColor", backgroundColor.serialized(), data);
    const FontDescription& baseFont = context.baseFont();
    const FontDescription& fontDescription = style->font().fontDescription();
    if (baseFont.computedPixelSize() != fontDescription.computedPixelSize())
        addProperty("fontSize", fontDescription.computedPixelSize(), data);
    // Our UA stylesheet has font-weight:normal for OPTION.
    if (FontWeightNormal != fontDescription.weight())
        addProperty("fontWeight", String(fontWeightToString(fontDescription.weight())), data);
    if (baseFont.family() != fontDescription.family()) {
        PagePopupClient::addString("fontFamily: [\n", data);
        for (const FontFamily* f = &fontDescription.family(); f; f = f->next()) {
            addJavaScriptString(f->family().string(), data);
            if (f->next())
                PagePopupClient::addString(",\n", data);
        }
        PagePopupClient::addString("],\n", data);
    }
    if (baseFont.style() != fontDescription.style())
        addProperty("fontStyle", String(fontStyleToString(fontDescription.style())), data);
    if (baseFont.variant() != fontDescription.variant())
        addProperty("fontVariant", String(fontVariantToString(fontDescription.variant())), data);
    if (baseStyle.textTransform() != style->textTransform())
        addProperty("textTransform", String(textTransformToString(style->textTransform())), data);

    PagePopupClient::addString("},\n", data);
}

void PopupMenuImpl::addOption(ItemIterationContext& context, HTMLOptionElement& element)
{
    SharedBuffer* data = context.m_buffer;
    PagePopupClient::addString("{", data);
    addProperty("label", element.text(), data);
    addProperty("value", context.m_listIndex, data);
    if (!element.title().isEmpty())
        addProperty("title", element.title(), data);
    const AtomicString& ariaLabel = element.fastGetAttribute(HTMLNames::aria_labelAttr);
    if (!ariaLabel.isEmpty())
        addProperty("ariaLabel", ariaLabel, data);
    if (element.isDisabledFormControl())
        addProperty("disabled", true, data);
    addElementStyle(context, element);
    PagePopupClient::addString("},", data);
}

void PopupMenuImpl::addOptGroup(ItemIterationContext& context, HTMLOptGroupElement& element)
{
    SharedBuffer* data = context.m_buffer;
    PagePopupClient::addString("{\n", data);
    PagePopupClient::addString("type: \"optgroup\",\n", data);
    addProperty("label", element.groupLabelText(), data);
    addProperty("title", element.title(), data);
    addProperty("ariaLabel", element.fastGetAttribute(HTMLNames::aria_labelAttr), data);
    addProperty("disabled", element.isDisabledFormControl(), data);
    addElementStyle(context, element);
    context.startGroupChildren(*m_ownerElement->itemComputedStyle(element));
    // We should call ItemIterationContext::finishGroupIfNecessary() later.
}

void PopupMenuImpl::addSeparator(ItemIterationContext& context, HTMLHRElement& element)
{
    SharedBuffer* data = context.m_buffer;
    PagePopupClient::addString("{\n", data);
    PagePopupClient::addString("type: \"separator\",\n", data);
    addProperty("title", element.title(), data);
    addProperty("ariaLabel", element.fastGetAttribute(HTMLNames::aria_labelAttr), data);
    addProperty("disabled", element.isDisabledFormControl(), data);
    addElementStyle(context, element);
    PagePopupClient::addString("},\n", data);
}

void PopupMenuImpl::selectFontsFromOwnerDocument(Document& document)
{
    Document& ownerDocument = ownerElement().document();
    document.styleEngine().setFontSelector(PopupMenuCSSFontSelector::create(&document, ownerDocument.styleEngine().fontSelector()));
}

void PopupMenuImpl::setValueAndClosePopup(int numValue, const String& stringValue)
{
    ASSERT(m_popup);
    ASSERT(m_ownerElement);
    RefPtrWillBeRawPtr<PopupMenuImpl> protector(this);
    bool success;
    int listIndex = stringValue.toInt(&success);
    ASSERT(success);
    m_ownerElement->valueChanged(listIndex);
    if (m_popup)
        m_chromeClient->closePagePopup(m_popup);
    // We dispatch events on the owner element to match the legacy behavior.
    // Other browsers dispatch click events before and after showing the popup.
    if (m_ownerElement) {
        PlatformMouseEvent event;
        RefPtrWillBeRawPtr<Element> owner = &ownerElement();
        owner->dispatchMouseEvent(event, EventTypeNames::mouseup);
        owner->dispatchMouseEvent(event, EventTypeNames::click);
    }
}

void PopupMenuImpl::setValue(const String& value)
{
    ASSERT(m_ownerElement);
    bool success;
    int listIndex = value.toInt(&success);
    ASSERT(success);
    m_ownerElement->provisionalSelectionChanged(listIndex);
}

void PopupMenuImpl::didClosePopup()
{
    // Clearing m_popup first to prevent from trying to close the popup again.
    m_popup = nullptr;
    RefPtrWillBeRawPtr<PopupMenuImpl> protector(this);
    if (m_ownerElement)
        m_ownerElement->popupDidHide();
}

Element& PopupMenuImpl::ownerElement()
{
    return *m_ownerElement;
}

Locale& PopupMenuImpl::locale()
{
    return Locale::defaultLocale();
}

void PopupMenuImpl::closePopup()
{
    if (m_popup)
        m_chromeClient->closePagePopup(m_popup);
    if (m_ownerElement)
        m_ownerElement->popupDidCancel();
}

void PopupMenuImpl::dispose()
{
    if (m_popup)
        m_chromeClient->closePagePopup(m_popup);
}

void PopupMenuImpl::show(const FloatQuad& /*controlPosition*/, const IntSize& /*controlSize*/, int /*index*/)
{
    ASSERT(!m_popup);
    m_popup = m_chromeClient->openPagePopup(this);
}

void PopupMenuImpl::hide()
{
    if (m_popup)
        m_chromeClient->closePagePopup(m_popup);
}

void PopupMenuImpl::updateFromElement()
{
    if (m_needsUpdate)
        return;
    m_needsUpdate = true;
    ownerElement().document().postTask(FROM_HERE, createSameThreadTask(&PopupMenuImpl::update, PassRefPtrWillBeRawPtr<PopupMenuImpl>(this)));
}

void PopupMenuImpl::update()
{
    if (!m_popup || !m_ownerElement)
        return;
    ownerElement().document().updateLayoutTreeIfNeeded();
    // disconnectClient() might have been called.
    if (!m_ownerElement)
        return;
    m_needsUpdate = false;
    RefPtr<SharedBuffer> data = SharedBuffer::create();
    PagePopupClient::addString("window.updateData = {\n", data.get());
    PagePopupClient::addString("type: \"update\",\n", data.get());
    ItemIterationContext context(*m_ownerElement->computedStyle(), data.get());
    context.serializeBaseStyle();
    PagePopupClient::addString("children: [", data.get());
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement>>& items = m_ownerElement->listItems();
    for (; context.m_listIndex < items.size(); ++context.m_listIndex) {
        Element& child = *items[context.m_listIndex];
        if (!isHTMLOptGroupElement(child.parentNode()))
            context.finishGroupIfNecessary();
        if (isHTMLOptionElement(child))
            addOption(context, toHTMLOptionElement(child));
        else if (isHTMLOptGroupElement(child))
            addOptGroup(context, toHTMLOptGroupElement(child));
        else if (isHTMLHRElement(child))
            addSeparator(context, toHTMLHRElement(child));
    }
    context.finishGroupIfNecessary();
    PagePopupClient::addString("],\n", data.get());
    PagePopupClient::addString("}\n", data.get());
    m_popup->postMessage(String::fromUTF8(data->data(), data->size()));
}


void PopupMenuImpl::disconnectClient()
{
    m_ownerElement = nullptr;
    // Cannot be done during finalization, so instead done when the
    // layout object is destroyed and disconnected.
    dispose();
}

} // namespace blink
