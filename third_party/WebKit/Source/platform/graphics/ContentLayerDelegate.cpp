/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "platform/graphics/ContentLayerDelegate.h"

#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "public/platform/WebDisplayItemList.h"
#include "public/platform/WebRect.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

ContentLayerDelegate::ContentLayerDelegate(GraphicsLayer* graphicsLayer)
    : m_graphicsLayer(graphicsLayer) {}

ContentLayerDelegate::~ContentLayerDelegate() {}

gfx::Rect ContentLayerDelegate::paintableRegion() {
  IntRect interestRect = m_graphicsLayer->interestRect();
  return gfx::Rect(interestRect.x(), interestRect.y(), interestRect.width(),
                   interestRect.height());
}

void ContentLayerDelegate::paintContents(
    WebDisplayItemList* webDisplayItemList,
    WebContentLayerClient::PaintingControlSetting paintingControl) {
  TRACE_EVENT0("blink,benchmark", "ContentLayerDelegate::paintContents");

  PaintController& paintController = m_graphicsLayer->getPaintController();
  paintController.setDisplayItemConstructionIsDisabled(
      paintingControl ==
      WebContentLayerClient::DisplayListConstructionDisabled);
  paintController.setSubsequenceCachingIsDisabled(
      paintingControl == WebContentLayerClient::SubsequenceCachingDisabled);

  if (paintingControl == WebContentLayerClient::PartialInvalidation)
    m_graphicsLayer->client()->invalidateTargetElementForTesting();

  // We also disable caching when Painting or Construction are disabled. In both
  // cases we would like to compare assuming the full cost of recording, not the
  // cost of re-using cached content.
  if (paintingControl == WebContentLayerClient::DisplayListCachingDisabled ||
      paintingControl == WebContentLayerClient::DisplayListPaintingDisabled ||
      paintingControl == WebContentLayerClient::DisplayListConstructionDisabled)
    paintController.invalidateAll();

  GraphicsContext::DisabledMode disabledMode = GraphicsContext::NothingDisabled;
  if (paintingControl == WebContentLayerClient::DisplayListPaintingDisabled ||
      paintingControl == WebContentLayerClient::DisplayListConstructionDisabled)
    disabledMode = GraphicsContext::FullyDisabled;

  // Anything other than PaintDefaultBehavior is for testing. In non-testing
  // scenarios, it is an error to call GraphicsLayer::paint. Actual painting
  // occurs in FrameView::paintTree(); this method merely copies the painted
  // output to the WebDisplayItemList.
  if (paintingControl != PaintDefaultBehavior)
    m_graphicsLayer->paint(nullptr, disabledMode);

  paintController.paintArtifact().appendToWebDisplayItemList(
      webDisplayItemList);

  paintController.setDisplayItemConstructionIsDisabled(false);
  paintController.setSubsequenceCachingIsDisabled(false);
}

size_t ContentLayerDelegate::approximateUnsharedMemoryUsage() const {
  return m_graphicsLayer->getPaintController().approximateUnsharedMemoryUsage();
}

}  // namespace blink
