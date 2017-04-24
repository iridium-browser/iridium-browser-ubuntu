/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "web/ContextMenuClientImpl.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/dom/Document.h"
#include "core/editing/Editor.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/MediaError.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutPart.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/HistoryItem.h"
#include "core/page/ContextMenuController.h"
#include "core/page/Page.h"
#include "platform/ContextMenu.h"
#include "platform/FrameViewBase.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/text/TextBreakIterator.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebVector.h"
#include "public/web/WebContextMenuData.h"
#include "public/web/WebFormElement.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebMenuItemInfo.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebSearchableFormData.h"
#include "public/web/WebSpellCheckClient.h"
#include "public/web/WebViewClient.h"
#include "web/ContextMenuAllowedScope.h"
#include "web/WebDataSourceImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/text/WTFString.h"

namespace blink {

// Figure out the URL of a page or subframe. Returns |page_type| as the type,
// which indicates page or subframe, or ContextNodeType::kNone if the URL could
// not be determined for some reason.
static WebURL urlFromFrame(LocalFrame* frame) {
  if (frame) {
    DocumentLoader* dl = frame->loader().documentLoader();
    if (dl) {
      WebDataSource* ds = WebDataSourceImpl::fromDocumentLoader(dl);
      if (ds) {
        return ds->hasUnreachableURL() ? ds->unreachableURL()
                                       : ds->getRequest().url();
      }
    }
  }
  return WebURL();
}

static bool IsWhiteSpaceOrPunctuation(UChar c) {
  return isSpaceOrNewline(c) || WTF::Unicode::isPunct(c);
}

static String selectMisspellingAsync(LocalFrame* selectedFrame,
                                     String& description) {
  VisibleSelection selection =
      selectedFrame->selection().computeVisibleSelectionInDOMTreeDeprecated();
  if (selection.isNone())
    return String();

  // Caret and range selections always return valid normalized ranges.
  Range* selectionRange = createRange(selection.toNormalizedEphemeralRange());
  DocumentMarkerVector markers =
      selectedFrame->document()->markers().markersInRange(
          EphemeralRange(selectionRange), DocumentMarker::MisspellingMarkers());
  if (markers.size() != 1)
    return String();
  description = markers[0]->description();

  // Cloning a range fails only for invalid ranges.
  Range* markerRange = selectionRange->cloneRange();
  markerRange->setStart(markerRange->startContainer(),
                        markers[0]->startOffset());
  markerRange->setEnd(markerRange->endContainer(), markers[0]->endOffset());

  if (markerRange->text().stripWhiteSpace(&IsWhiteSpaceOrPunctuation) !=
      selectionRange->text().stripWhiteSpace(&IsWhiteSpaceOrPunctuation))
    return String();

  return markerRange->text();
}

bool ContextMenuClientImpl::shouldShowContextMenuFromTouch(
    const WebContextMenuData& data) {
  return m_webView->page()->settings().getAlwaysShowContextMenuOnTouch() ||
         !data.linkURL.isEmpty() ||
         data.mediaType == WebContextMenuData::MediaTypeImage ||
         data.mediaType == WebContextMenuData::MediaTypeVideo ||
         data.isEditable;
}

bool ContextMenuClientImpl::showContextMenu(const ContextMenu* defaultMenu,
                                            bool fromTouch) {
  // Displaying the context menu in this function is a big hack as we don't
  // have context, i.e. whether this is being invoked via a script or in
  // response to user input (Mouse event WM_RBUTTONDOWN,
  // Keyboard events KeyVK_APPS, Shift+F10). Check if this is being invoked
  // in response to the above input events before popping up the context menu.
  if (!ContextMenuAllowedScope::isContextMenuAllowed())
    return false;

  HitTestResult r = m_webView->page()->contextMenuController().hitTestResult();

  r.setToShadowHostIfInUserAgentShadowRoot();

  LocalFrame* selectedFrame = r.innerNodeFrame();

  WebContextMenuData data;
  data.mousePosition = selectedFrame->view()->contentsToViewport(
      r.roundedPointInInnerNodeFrame());

  // Compute edit flags.
  data.editFlags = WebContextMenuData::CanDoNone;
  if (toLocalFrame(m_webView->focusedCoreFrame())->editor().canUndo())
    data.editFlags |= WebContextMenuData::CanUndo;
  if (toLocalFrame(m_webView->focusedCoreFrame())->editor().canRedo())
    data.editFlags |= WebContextMenuData::CanRedo;
  if (toLocalFrame(m_webView->focusedCoreFrame())->editor().canCut())
    data.editFlags |= WebContextMenuData::CanCut;
  if (toLocalFrame(m_webView->focusedCoreFrame())->editor().canCopy())
    data.editFlags |= WebContextMenuData::CanCopy;
  if (toLocalFrame(m_webView->focusedCoreFrame())->editor().canPaste())
    data.editFlags |= WebContextMenuData::CanPaste;
  if (toLocalFrame(m_webView->focusedCoreFrame())->editor().canDelete())
    data.editFlags |= WebContextMenuData::CanDelete;
  // We can always select all...
  data.editFlags |= WebContextMenuData::CanSelectAll;
  data.editFlags |= WebContextMenuData::CanTranslate;

  // Links, Images, Media tags, and Image/Media-Links take preference over
  // all else.
  data.linkURL = r.absoluteLinkURL();

  if (r.innerNode()->isHTMLElement()) {
    HTMLElement* htmlElement = toHTMLElement(r.innerNode());
    if (!htmlElement->title().isEmpty()) {
      data.titleText = htmlElement->title();
    } else {
      data.titleText = htmlElement->altText();
    }
  }

  if (isHTMLCanvasElement(r.innerNode())) {
    data.mediaType = WebContextMenuData::MediaTypeCanvas;
    data.hasImageContents = true;
  } else if (!r.absoluteImageURL().isEmpty()) {
    data.srcURL = r.absoluteImageURL();
    data.mediaType = WebContextMenuData::MediaTypeImage;
    data.mediaFlags |= WebContextMenuData::MediaCanPrint;

    // An image can be null for many reasons, like being blocked, no image
    // data received from server yet.
    data.hasImageContents = r.image() && !r.image()->isNull();
    if (data.hasImageContents &&
        isHTMLImageElement(r.innerNodeOrImageMapImage())) {
      HTMLImageElement* imageElement =
          toHTMLImageElement(r.innerNodeOrImageMapImage());
      if (imageElement && imageElement->cachedImage())
        data.imageResponse =
            WrappedResourceResponse(imageElement->cachedImage()->response());
    }
  } else if (!r.absoluteMediaURL().isEmpty()) {
    data.srcURL = r.absoluteMediaURL();

    // We know that if absoluteMediaURL() is not empty, then this
    // is a media element.
    HTMLMediaElement* mediaElement = toHTMLMediaElement(r.innerNode());
    if (isHTMLVideoElement(*mediaElement))
      data.mediaType = WebContextMenuData::MediaTypeVideo;
    else if (isHTMLAudioElement(*mediaElement))
      data.mediaType = WebContextMenuData::MediaTypeAudio;

    if (mediaElement->error())
      data.mediaFlags |= WebContextMenuData::MediaInError;
    if (mediaElement->paused())
      data.mediaFlags |= WebContextMenuData::MediaPaused;
    if (mediaElement->muted())
      data.mediaFlags |= WebContextMenuData::MediaMuted;
    if (mediaElement->loop())
      data.mediaFlags |= WebContextMenuData::MediaLoop;
    if (mediaElement->supportsSave())
      data.mediaFlags |= WebContextMenuData::MediaCanSave;
    if (mediaElement->hasAudio())
      data.mediaFlags |= WebContextMenuData::MediaHasAudio;
    // Media controls can be toggled only for video player. If we toggle
    // controls for audio then the player disappears, and there is no way to
    // return it back. Don't set this bit for fullscreen video, since
    // toggling is ignored in that case.
    if (mediaElement->isHTMLVideoElement() && mediaElement->hasVideo() &&
        !mediaElement->isFullscreen())
      data.mediaFlags |= WebContextMenuData::MediaCanToggleControls;
    if (mediaElement->shouldShowControls())
      data.mediaFlags |= WebContextMenuData::MediaControls;
  } else if (isHTMLObjectElement(*r.innerNode()) ||
             isHTMLEmbedElement(*r.innerNode())) {
    LayoutObject* object = r.innerNode()->layoutObject();
    if (object && object->isLayoutPart()) {
      FrameViewBase* frameViewBase = toLayoutPart(object)->widget();
      if (frameViewBase && frameViewBase->isPluginContainer()) {
        data.mediaType = WebContextMenuData::MediaTypePlugin;
        WebPluginContainerImpl* plugin =
            toWebPluginContainerImpl(frameViewBase);
        WebString text = plugin->plugin()->selectionAsText();
        if (!text.isEmpty()) {
          data.selectedText = text;
          data.editFlags |= WebContextMenuData::CanCopy;
        }
        data.editFlags &= ~WebContextMenuData::CanTranslate;
        data.linkURL = plugin->plugin()->linkAtPosition(data.mousePosition);
        if (plugin->plugin()->supportsPaginatedPrint())
          data.mediaFlags |= WebContextMenuData::MediaCanPrint;

        HTMLPlugInElement* pluginElement = toHTMLPlugInElement(r.innerNode());
        data.srcURL =
            pluginElement->document().completeURL(pluginElement->url());
        data.mediaFlags |= WebContextMenuData::MediaCanSave;

        // Add context menu commands that are supported by the plugin.
        if (plugin->plugin()->canRotateView())
          data.mediaFlags |= WebContextMenuData::MediaCanRotate;
      }
    }
  }

  // If it's not a link, an image, a media element, or an image/media link,
  // show a selection menu or a more generic page menu.
  if (selectedFrame->document()->loader())
    data.frameEncoding = selectedFrame->document()->encodingName();

  // Send the frame and page URLs in any case.
  if (!m_webView->page()->mainFrame()->isLocalFrame()) {
    // TODO(kenrb): This works around the problem of URLs not being
    // available for top-level frames that are in a different process.
    // It mostly works to convert the security origin to a URL, but
    // extensions accessing that property will not get the correct value
    // in that case. See https://crbug.com/534561
    WebSecurityOrigin origin = m_webView->mainFrame()->getSecurityOrigin();
    if (!origin.isNull())
      data.pageURL = KURL(ParsedURLString, origin.toString());
  } else {
    data.pageURL = urlFromFrame(toLocalFrame(m_webView->page()->mainFrame()));
  }

  if (selectedFrame != m_webView->page()->mainFrame()) {
    data.frameURL = urlFromFrame(selectedFrame);
    HistoryItem* historyItem = selectedFrame->loader().currentItem();
    if (historyItem)
      data.frameHistoryItem = WebHistoryItem(historyItem);
  }

  // HitTestResult::isSelected() ensures clean layout by performing a hit test.
  if (r.isSelected()) {
    if (!isHTMLInputElement(*r.innerNode()) ||
        toHTMLInputElement(r.innerNode())->type() != InputTypeNames::password) {
      data.selectedText = selectedFrame->selectedText().stripWhiteSpace();
    }
  }

  if (r.isContentEditable()) {
    data.isEditable = true;

    // Spellchecker adds spelling markers to misspelled words and attaches
    // suggestions to these markers in the background. Therefore, when a
    // user right-clicks a mouse on a word, Chrome just needs to find a
    // spelling marker on the word instead of spellchecking it.
    String description;
    data.misspelledWord = selectMisspellingAsync(selectedFrame, description);
    if (description.length()) {
      Vector<String> suggestions;
      description.split('\n', suggestions);
      data.dictionarySuggestions = suggestions;
    } else if (m_webView->spellCheckClient()) {
      int misspelledOffset, misspelledLength;
      m_webView->spellCheckClient()->checkSpelling(
          data.misspelledWord, misspelledOffset, misspelledLength,
          &data.dictionarySuggestions);
    }

    HTMLFormElement* form = selectedFrame->selection().currentForm();
    if (form && isHTMLInputElement(*r.innerNode())) {
      HTMLInputElement& selectedElement = toHTMLInputElement(*r.innerNode());
      WebSearchableFormData ws = WebSearchableFormData(
          WebFormElement(form), WebInputElement(&selectedElement));
      if (ws.url().isValid())
        data.keywordURL = ws.url();
    }
  }

  if (selectedFrame->editor().selectionHasStyle(CSSPropertyDirection, "ltr") !=
      FalseTriState)
    data.writingDirectionLeftToRight |=
        WebContextMenuData::CheckableMenuItemChecked;
  if (selectedFrame->editor().selectionHasStyle(CSSPropertyDirection, "rtl") !=
      FalseTriState)
    data.writingDirectionRightToLeft |=
        WebContextMenuData::CheckableMenuItemChecked;

  data.referrerPolicy = static_cast<WebReferrerPolicy>(
      selectedFrame->document()->getReferrerPolicy());

  // Filter out custom menu elements and add them into the data.
  populateCustomMenuItems(defaultMenu, &data);

  if (isHTMLAnchorElement(r.URLElement())) {
    HTMLAnchorElement* anchor = toHTMLAnchorElement(r.URLElement());

    // Extract suggested filename for saving file.
    data.suggestedFilename = anchor->fastGetAttribute(HTMLNames::downloadAttr);

    // If the anchor wants to suppress the referrer, update the referrerPolicy
    // accordingly.
    if (anchor->hasRel(RelationNoReferrer))
      data.referrerPolicy = WebReferrerPolicyNever;

    data.linkText = anchor->innerText();
  }

  // Find the input field type.
  if (isHTMLInputElement(r.innerNode())) {
    HTMLInputElement* element = toHTMLInputElement(r.innerNode());
    if (element->type() == InputTypeNames::password)
      data.inputFieldType = WebContextMenuData::InputFieldTypePassword;
    else if (element->isTextField())
      data.inputFieldType = WebContextMenuData::InputFieldTypePlainText;
    else
      data.inputFieldType = WebContextMenuData::InputFieldTypeOther;
  } else {
    data.inputFieldType = WebContextMenuData::InputFieldTypeNone;
  }

  if (fromTouch && !shouldShowContextMenuFromTouch(data))
    return false;

  WebLocalFrameImpl* selectedWebFrame =
      WebLocalFrameImpl::fromFrame(selectedFrame);
  selectedWebFrame->setContextMenuNode(r.innerNodeOrImageMapImage());
  if (!selectedWebFrame->client())
    return false;

  selectedWebFrame->client()->showContextMenu(data);
  return true;
}

void ContextMenuClientImpl::clearContextMenu() {
  HitTestResult r = m_webView->page()->contextMenuController().hitTestResult();
  LocalFrame* selectedFrame = r.innerNodeFrame();
  if (!selectedFrame)
    return;

  WebLocalFrameImpl* selectedWebFrame =
      WebLocalFrameImpl::fromFrame(selectedFrame);
  selectedWebFrame->clearContextMenuNode();
}

static void populateSubMenuItems(const Vector<ContextMenuItem>& inputMenu,
                                 WebVector<WebMenuItemInfo>& subMenuItems) {
  Vector<WebMenuItemInfo> subItems;
  for (size_t i = 0; i < inputMenu.size(); ++i) {
    const ContextMenuItem* inputItem = &inputMenu.at(i);
    if (inputItem->action() < ContextMenuItemBaseCustomTag ||
        inputItem->action() > ContextMenuItemLastCustomTag)
      continue;

    WebMenuItemInfo outputItem;
    outputItem.label = inputItem->title();
    outputItem.icon = inputItem->icon();
    outputItem.enabled = inputItem->enabled();
    outputItem.checked = inputItem->checked();
    outputItem.action = static_cast<unsigned>(inputItem->action() -
                                              ContextMenuItemBaseCustomTag);
    switch (inputItem->type()) {
      case ActionType:
        outputItem.type = WebMenuItemInfo::Option;
        break;
      case CheckableActionType:
        outputItem.type = WebMenuItemInfo::CheckableOption;
        break;
      case SeparatorType:
        outputItem.type = WebMenuItemInfo::Separator;
        break;
      case SubmenuType:
        outputItem.type = WebMenuItemInfo::SubMenu;
        populateSubMenuItems(inputItem->subMenuItems(),
                             outputItem.subMenuItems);
        break;
    }
    subItems.push_back(outputItem);
  }

  WebVector<WebMenuItemInfo> outputItems(subItems.size());
  for (size_t i = 0; i < subItems.size(); ++i)
    outputItems[i] = subItems[i];
  subMenuItems.swap(outputItems);
}

void ContextMenuClientImpl::populateCustomMenuItems(
    const ContextMenu* defaultMenu,
    WebContextMenuData* data) {
  populateSubMenuItems(defaultMenu->items(), data->customItems);
}

}  // namespace blink
