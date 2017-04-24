/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGImageElement_h
#define SVGImageElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGAnimatedPreserveAspectRatio.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGImageLoader.h"
#include "core/svg/SVGURIReference.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGImageElement final : public SVGGraphicsElement,
                              public SVGURIReference {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGImageElement);

 public:
  DECLARE_NODE_FACTORY(SVGImageElement);
  DECLARE_VIRTUAL_TRACE();

  bool currentFrameHasSingleSecurityOrigin() const;

  SVGAnimatedLength* x() const { return m_x.get(); }
  SVGAnimatedLength* y() const { return m_y.get(); }
  SVGAnimatedLength* width() const { return m_width.get(); }
  SVGAnimatedLength* height() const { return m_height.get(); }
  SVGAnimatedPreserveAspectRatio* preserveAspectRatio() {
    return m_preserveAspectRatio.get();
  }

  // Exposed for testing.
  ImageResourceContent* cachedImage() const { return imageLoader().image(); }

 private:
  explicit SVGImageElement(Document&);

  bool isStructurallyExternal() const override {
    return !hrefString().isNull();
  }

  void collectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;

  void svgAttributeChanged(const QualifiedName&) override;

  void attachLayoutTree(const AttachContext& = AttachContext()) override;
  InsertionNotificationRequest insertedInto(ContainerNode*) override;

  LayoutObject* createLayoutObject(const ComputedStyle&) override;

  const AtomicString imageSourceURL() const override;

  bool haveLoadedRequiredResources() override;

  bool selfHasRelativeLengths() const override;
  void didMoveToNewDocument(Document& oldDocument) override;
  SVGImageLoader& imageLoader() const { return *m_imageLoader; }

  Member<SVGAnimatedLength> m_x;
  Member<SVGAnimatedLength> m_y;
  Member<SVGAnimatedLength> m_width;
  Member<SVGAnimatedLength> m_height;
  Member<SVGAnimatedPreserveAspectRatio> m_preserveAspectRatio;

  Member<SVGImageLoader> m_imageLoader;
  bool m_needsLoaderURIUpdate : 1;
};

}  // namespace blink

#endif  // SVGImageElement_h
