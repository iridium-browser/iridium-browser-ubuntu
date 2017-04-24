/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 *
 */

#ifndef HTMLImageElement_h
#define HTMLImageElement_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/html/FormAssociated.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/network/ResourceResponse.h"

namespace blink {

class HTMLFormElement;
class ImageCandidate;
class ShadowRoot;
class ImageBitmapOptions;

class CORE_EXPORT HTMLImageElement final
    : public HTMLElement,
      public CanvasImageSource,
      public ImageBitmapSource,
      public ActiveScriptWrappable<HTMLImageElement>,
      public FormAssociated {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HTMLImageElement);

 public:
  class ViewportChangeListener;

  static HTMLImageElement* create(Document&);
  static HTMLImageElement* create(Document&,
                                  bool createdByParser);
  static HTMLImageElement* createForJSConstructor(Document&);
  static HTMLImageElement* createForJSConstructor(Document&, unsigned width);
  static HTMLImageElement* createForJSConstructor(Document&,
                                                  unsigned width,
                                                  unsigned height);

  ~HTMLImageElement() override;
  DECLARE_VIRTUAL_TRACE();

  unsigned width();
  unsigned height();

  unsigned naturalWidth() const;
  unsigned naturalHeight() const;

  unsigned layoutBoxWidth() const;
  unsigned layoutBoxHeight() const;

  const String& currentSrc() const;

  bool isServerMap() const;

  String altText() const final;

  ImageResourceContent* cachedImage() const { return imageLoader().image(); }
  ImageResource* cachedImageResourceForImageDocument() const {
    return imageLoader().imageResourceForImageDocument();
  }
  void setImageResource(ImageResourceContent* i) { imageLoader().setImage(i); }

  void setLoadingImageDocument() { imageLoader().setLoadingImageDocument(); }

  void setHeight(unsigned);

  KURL src() const;
  void setSrc(const String&);

  void setWidth(unsigned);

  int x() const;
  int y() const;

  bool complete() const;

  bool hasPendingActivity() const final {
    return imageLoader().hasPendingActivity();
  }

  bool canContainRangeEndPoint() const override { return false; }

  const AtomicString imageSourceURL() const override;

  HTMLFormElement* formOwner() const override;
  void formRemovedFromTree(const Node& formRoot);
  virtual void ensureCollapsedOrFallbackContent();
  virtual void ensureFallbackForGeneratedContent();
  virtual void ensurePrimaryContent();
  bool isCollapsed() const;

  // CanvasImageSource implementation
  PassRefPtr<Image> getSourceImageForCanvas(SourceImageStatus*,
                                            AccelerationHint,
                                            SnapshotReason,
                                            const FloatSize&) const override;
  bool isSVGSource() const override;
  bool wouldTaintOrigin(SecurityOrigin*) const override;
  FloatSize elementSize(const FloatSize&) const override;
  FloatSize defaultDestinationSize(const FloatSize&) const override;
  const KURL& sourceURL() const override;
  bool isAccelerated() const override { return false; }
  bool isOpaque() const override;
  int sourceWidth() override;
  int sourceHeight() override;

  // public so that HTMLPictureElement can call this as well.
  void selectSourceURL(ImageLoader::UpdateFromElementBehavior);

  void setIsFallbackImage() { m_isFallbackImage = true; }

  FetchRequest::ResourceWidth getResourceWidth();
  float sourceSize(Element&);

  void forceReload() const;

  // ImageBitmapSource implementation
  IntSize bitmapSourceSize() const override;
  ScriptPromise createImageBitmap(ScriptState*,
                                  EventTarget&,
                                  Optional<IntRect> cropRect,
                                  const ImageBitmapOptions&,
                                  ExceptionState&) override;

  FormAssociated* toFormAssociatedOrNull() override { return this; };
  void associateWith(HTMLFormElement*) override;

 protected:
  // Controls how an image element appears in the layout. See:
  // https://html.spec.whatwg.org/multipage/embedded-content.html#image-request
  enum class LayoutDisposition : uint8_t {
    // Displayed as a partially or completely loaded image. Corresponds to the
    // `current request` state being: `unavailable`, `partially available`, or
    // `completely available`.
    PrimaryContent,
    // Showing a broken image icon and 'alt' text, if any. Corresponds to the
    // `current request` being in the `broken` state.
    FallbackContent,
    // No layout object. Corresponds to the `current request` being in the
    // `broken` state when the resource load failed with an error that has the
    // |shouldCollapseInitiator| flag set.
    Collapsed
  };

  explicit HTMLImageElement(Document&,
                            bool createdByParser = false);

  void didMoveToNewDocument(Document& oldDocument) override;

  void didAddUserAgentShadowRoot(ShadowRoot&) override;
  PassRefPtr<ComputedStyle> customStyleForLayoutObject() override;

 private:
  bool areAuthorShadowsAllowed() const override { return false; }

  void parseAttribute(const AttributeModificationParams&) override;
  bool isPresentationAttribute(const QualifiedName&) const override;
  void collectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;
  void setLayoutDisposition(LayoutDisposition, bool forceReattach = false);

  void attachLayoutTree(const AttachContext& = AttachContext()) override;
  LayoutObject* createLayoutObject(const ComputedStyle&) override;

  bool canStartSelection() const override { return false; }

  bool isURLAttribute(const Attribute&) const override;
  bool hasLegalLinkAttribute(const QualifiedName&) const override;
  const QualifiedName& subResourceAttributeName() const override;

  bool draggable() const override;

  InsertionNotificationRequest insertedInto(ContainerNode*) override;
  void removedFrom(ContainerNode*) override;
  bool shouldRegisterAsNamedItem() const override { return true; }
  bool shouldRegisterAsExtraNamedItem() const override { return true; }
  bool isInteractiveContent() const override;
  Image* imageContents() override;

  void resetFormOwner();
  ImageCandidate findBestFitImageFromPictureParent();
  void setBestFitURLAndDPRFromImageCandidate(const ImageCandidate&);
  HTMLImageLoader& imageLoader() const { return *m_imageLoader; }
  void notifyViewportChanged();
  void createMediaQueryListIfDoesNotExist();

  Member<HTMLImageLoader> m_imageLoader;
  Member<ViewportChangeListener> m_listener;
  Member<HTMLFormElement> m_form;
  AtomicString m_bestFitImageURL;
  float m_imageDevicePixelRatio;
  Member<HTMLSourceElement> m_source;
  LayoutDisposition m_layoutDisposition;
  unsigned m_formWasSetByParser : 1;
  unsigned m_elementCreatedByParser : 1;
  unsigned m_isFallbackImage : 1;

  ReferrerPolicy m_referrerPolicy;
};

}  // namespace blink

#endif  // HTMLImageElement_h
