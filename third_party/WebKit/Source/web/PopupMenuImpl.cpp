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

class PopupMenuCSSFontSelector : public CSSFontSelector {
public:
    static PassRefPtrWillBeRawPtr<PopupMenuCSSFontSelector> create(Document* document, CSSFontSelector* ownerFontSelector)
    {
        return adoptRefWillBeNoop(new PopupMenuCSSFontSelector(document, ownerFontSelector));
    }

    // We don't override willUseFontData() for now because the old PopupListBox
    // only worked with fonts loaded when opening the popup.
    virtual PassRefPtr<FontData> getFontData(const FontDescription&, const AtomicString&) override;

    DECLARE_VIRTUAL_TRACE();

private:
    PopupMenuCSSFontSelector(Document* document, CSSFontSelector* ownerFontSelector)
        : CSSFontSelector(document)
        , m_ownerFontSelector(ownerFontSelector)
    {
    }
    RefPtrWillBeMember<CSSFontSelector> m_ownerFontSelector;
};

PassRefPtr<FontData> PopupMenuCSSFontSelector::getFontData(const FontDescription& description, const AtomicString& name)
{
    return m_ownerFontSelector->getFontData(description, name);
}

DEFINE_TRACE(PopupMenuCSSFontSelector)
{
    visitor->trace(m_ownerFontSelector);
    CSSFontSelector::trace(visitor);
}

PassRefPtrWillBeRawPtr<PopupMenuImpl> PopupMenuImpl::create(ChromeClientImpl* chromeClient, PopupMenuClient* client)
{
    return adoptRefWillBeNoop(new PopupMenuImpl(chromeClient, client));
}

PopupMenuImpl::PopupMenuImpl(ChromeClientImpl* chromeClient, PopupMenuClient* client)
    : m_chromeClient(chromeClient)
    , m_client(client)
    , m_popup(nullptr)
    , m_needsUpdate(false)
{
}

PopupMenuImpl::~PopupMenuImpl()
{
    ASSERT(!m_popup);
}

IntSize PopupMenuImpl::contentSize()
{
    return IntSize();
}

void PopupMenuImpl::writeDocument(SharedBuffer* data)
{
    IntRect anchorRectInScreen = m_chromeClient->viewportToScreen(m_client->elementRectRelativeToViewport());

    PagePopupClient::addString("<!DOCTYPE html><head><meta charset='UTF-8'><style>\n", data);
    data->append(Platform::current()->loadResource("pickerCommon.css"));
    data->append(Platform::current()->loadResource("listPicker.css"));
    PagePopupClient::addString("</style></head><body><div id=main>Loading...</div><script>\n"
        "window.dialogArguments = {\n", data);
    addProperty("selectedIndex", m_client->selectedIndex(), data);
    PagePopupClient::addString("children: [\n", data);
    for (HTMLElement& child : Traversal<HTMLElement>::childrenOf(ownerElement())) {
        if (isHTMLOptionElement(child))
            addOption(toHTMLOptionElement(child), data);
        if (isHTMLOptGroupElement(child))
            addOptGroup(toHTMLOptGroupElement(child), data);
        if (isHTMLHRElement(child))
            addSeparator(toHTMLHRElement(child), data);
    }
    PagePopupClient::addString("],\n", data);
    addProperty("anchorRectInScreen", anchorRectInScreen, data);
    const ComputedStyle* ownerStyle = ownerElement().computedStyle();
    Color backgroundColor = ownerStyle->visitedDependentColor(CSSPropertyBackgroundColor);
#if OS(LINUX)
    // On other platforms, the <option> background color is the same as the
    // <select> background color. On Linux, that makes the <option>
    // background color very dark, so by default, try to use a lighter
    // background color for <option>s.
    if (LayoutTheme::theme().systemColor(CSSValueButtonface) == backgroundColor)
        backgroundColor = LayoutTheme::theme().systemColor(CSSValueMenu);
#endif
    addProperty("backgroundColor", backgroundColor.serialized(), data);
    bool isRTL = !ownerStyle->isLeftToRightDirection();
    addProperty("isRTL", isRTL, data);
    addProperty("paddingStart", isRTL ? m_client->clientPaddingRight().toDouble() : m_client->clientPaddingLeft().toDouble(), data);
    PagePopupClient::addString("};\n", data);
    data->append(Platform::current()->loadResource("pickerCommon.js"));
    data->append(Platform::current()->loadResource("listPicker.js"));
    PagePopupClient::addString("</script></body>\n", data);
}

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
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return 0;
}

const char* fontVariantToString(FontVariant variant)
{
    switch (variant) {
    case FontVariantNormal:
        return "normal";
    case FontVariantSmallCaps:
        return "small-caps";
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return 0;
}

const char* fontStyleToString(FontStyle style)
{
    switch (style) {
    case FontStyleNormal:
        return "normal";
    case FontStyleItalic:
        return "italic";
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return 0;
}

void PopupMenuImpl::addElementStyle(HTMLElement& element, SharedBuffer* data)
{
    const ComputedStyle* style = m_client->computedStyleForItem(element);
    ASSERT(style);
    PagePopupClient::addString("style: {\n", data);
    addProperty("color", style->visitedDependentColor(CSSPropertyColor).serialized(), data);
    addProperty("backgroundColor", style->visitedDependentColor(CSSPropertyBackgroundColor).serialized(), data);
    const FontDescription& fontDescription = style->font().fontDescription();
    addProperty("fontSize", fontDescription.computedPixelSize(), data);
    addProperty("fontWeight", String(fontWeightToString(fontDescription.weight())), data);
    PagePopupClient::addString("fontFamily: [\n", data);
    for (const FontFamily* f = &fontDescription.family(); f; f = f->next()) {
        addJavaScriptString(f->family().string(), data);
        if (f->next())
            PagePopupClient::addString(",\n", data);
    }
    PagePopupClient::addString("],\n", data);
    addProperty("fontStyle", String(fontStyleToString(fontDescription.style())), data);
    addProperty("fontVariant", String(fontVariantToString(fontDescription.variant())), data);
    addProperty("visibility", String(style->visibility() == HIDDEN ? "hidden" : "visible"), data);
    addProperty("display", String(style->display() == NONE ? "none" : "block"), data);
    addProperty("direction", String(style->direction() == RTL ? "rtl" : "ltr"), data);
    addProperty("unicodeBidi", String(isOverride(style->unicodeBidi()) ? "bidi-override" : "normal"), data);
    PagePopupClient::addString("},\n", data);
}

void PopupMenuImpl::addOption(HTMLOptionElement& element, SharedBuffer* data)
{
    PagePopupClient::addString("{\n", data);
    PagePopupClient::addString("type: \"option\",\n", data);
    addProperty("label", element.text(), data);
    addProperty("title", element.title(), data);
    addProperty("value", element.listIndex(), data);
    addProperty("ariaLabel", element.fastGetAttribute(HTMLNames::aria_labelAttr), data);
    addProperty("disabled", element.isDisabledFormControl(), data);
    addElementStyle(element, data);
    PagePopupClient::addString("},\n", data);
}

void PopupMenuImpl::addOptGroup(HTMLOptGroupElement& element, SharedBuffer* data)
{
    PagePopupClient::addString("{\n", data);
    PagePopupClient::addString("type: \"optgroup\",\n", data);
    addProperty("label", element.groupLabelText(), data);
    addProperty("title", element.title(), data);
    addProperty("ariaLabel", element.fastGetAttribute(HTMLNames::aria_labelAttr), data);
    addProperty("disabled", element.isDisabledFormControl(), data);
    addElementStyle(element, data);
    PagePopupClient::addString("children: [", data);
    for (HTMLElement& child : Traversal<HTMLElement>::childrenOf(element)) {
        if (isHTMLOptionElement(child))
            addOption(toHTMLOptionElement(child), data);
        if (isHTMLOptGroupElement(child))
            addOptGroup(toHTMLOptGroupElement(child), data);
        if (isHTMLHRElement(child))
            addSeparator(toHTMLHRElement(child), data);
    }
    PagePopupClient::addString("],\n", data);
    PagePopupClient::addString("},\n", data);
}

void PopupMenuImpl::addSeparator(HTMLHRElement& element, SharedBuffer* data)
{
    PagePopupClient::addString("{\n", data);
    PagePopupClient::addString("type: \"separator\",\n", data);
    addProperty("title", element.title(), data);
    addProperty("ariaLabel", element.fastGetAttribute(HTMLNames::aria_labelAttr), data);
    addProperty("disabled", element.isDisabledFormControl(), data);
    addElementStyle(element, data);
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
    ASSERT(m_client);
    RefPtrWillBeRawPtr<PopupMenuImpl> protector(this);
    bool success;
    int listIndex = stringValue.toInt(&success);
    ASSERT(success);
    m_client->selectionChanged(listIndex);
    m_client->valueChanged(listIndex);
    if (m_popup)
        m_chromeClient->closePagePopup(m_popup);
    // We dispatch events on the owner element to match the legacy behavior.
    // Other browsers dispatch click events before and after showing the popup.
    if (m_client) {
        PlatformMouseEvent event;
        RefPtrWillBeRawPtr<Element> owner = &ownerElement();
        owner->dispatchMouseEvent(event, EventTypeNames::mouseup);
        owner->dispatchMouseEvent(event, EventTypeNames::click);
    }
}

void PopupMenuImpl::setValue(const String& value)
{
    ASSERT(m_client);
    bool success;
    int listIndex = value.toInt(&success);
    ASSERT(success);
    m_client->provisionalSelectionChanged(listIndex);
}

void PopupMenuImpl::didClosePopup()
{
    // Clearing m_popup first to prevent from trying to close the popup again.
    m_popup = nullptr;
    RefPtrWillBeRawPtr<PopupMenuImpl> protector(this);
    if (m_client)
        m_client->popupDidHide();
}

Element& PopupMenuImpl::ownerElement()
{
    return m_client->ownerElement();
}

Locale& PopupMenuImpl::locale()
{
    return Locale::defaultLocale();
}

void PopupMenuImpl::closePopup()
{
    if (m_popup)
        m_chromeClient->closePagePopup(m_popup);
    if (m_client)
        m_client->popupDidCancel();
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
    if (!m_popup || !m_client)
        return;
    ownerElement().document().updateRenderTreeIfNeeded();
    if (!m_client)
        return;
    m_needsUpdate = false;
    RefPtr<SharedBuffer> data = SharedBuffer::create();
    PagePopupClient::addString("window.updateData = {\n", data.get());
    PagePopupClient::addString("type: \"update\",\n", data.get());
    PagePopupClient::addString("children: [", data.get());
    for (HTMLElement& child : Traversal<HTMLElement>::childrenOf(ownerElement())) {
        if (isHTMLOptionElement(child))
            addOption(toHTMLOptionElement(child), data.get());
        if (isHTMLOptGroupElement(child))
            addOptGroup(toHTMLOptGroupElement(child), data.get());
        if (isHTMLHRElement(child))
            addSeparator(toHTMLHRElement(child), data.get());
    }
    PagePopupClient::addString("],\n", data.get());
    PagePopupClient::addString("}\n", data.get());
    m_popup->postMessage(String::fromUTF8(data->data(), data->size()));
}


void PopupMenuImpl::disconnectClient()
{
    m_client = nullptr;
    // Cannot be done during finalization, so instead done when the
    // render object is destroyed and disconnected.
    dispose();
}

} // namespace blink
