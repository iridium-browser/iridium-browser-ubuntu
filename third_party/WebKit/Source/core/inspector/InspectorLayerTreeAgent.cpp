/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/inspector/InspectorLayerTreeAgent.h"

#include "core/dom/DOMNodeIds.h"
#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/VisualViewport.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/ChromeClient.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/CompositingReasons.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/PictureSnapshot.h"
#include "platform/image-encoders/PNGImageEncoder.h"
#include "platform/transforms/TransformationMatrix.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebLayer.h"
#include "wtf/text/Base64.h"
#include "wtf/text/StringBuilder.h"
#include <memory>

namespace blink {

using protocol::Array;
unsigned InspectorLayerTreeAgent::s_lastSnapshotId;

inline String idForLayer(const GraphicsLayer* graphicsLayer) {
  return String::number(graphicsLayer->platformLayer()->id());
}

static std::unique_ptr<protocol::LayerTree::ScrollRect> buildScrollRect(
    const WebRect& rect,
    const String& type) {
  std::unique_ptr<protocol::DOM::Rect> rectObject =
      protocol::DOM::Rect::create()
          .setX(rect.x)
          .setY(rect.y)
          .setHeight(rect.height)
          .setWidth(rect.width)
          .build();
  std::unique_ptr<protocol::LayerTree::ScrollRect> scrollRectObject =
      protocol::LayerTree::ScrollRect::create()
          .setRect(std::move(rectObject))
          .setType(type)
          .build();
  return scrollRectObject;
}

static std::unique_ptr<Array<protocol::LayerTree::ScrollRect>>
buildScrollRectsForLayer(GraphicsLayer* graphicsLayer,
                         bool reportWheelScrollers) {
  std::unique_ptr<Array<protocol::LayerTree::ScrollRect>> scrollRects =
      Array<protocol::LayerTree::ScrollRect>::create();
  WebLayer* webLayer = graphicsLayer->platformLayer();
  WebVector<WebRect> nonFastScrollableRects =
      webLayer->nonFastScrollableRegion();
  for (size_t i = 0; i < nonFastScrollableRects.size(); ++i) {
    scrollRects->addItem(buildScrollRect(
        nonFastScrollableRects[i],
        protocol::LayerTree::ScrollRect::TypeEnum::RepaintsOnScroll));
  }
  WebVector<WebRect> touchEventHandlerRects =
      webLayer->touchEventHandlerRegion();
  for (size_t i = 0; i < touchEventHandlerRects.size(); ++i) {
    scrollRects->addItem(buildScrollRect(
        touchEventHandlerRects[i],
        protocol::LayerTree::ScrollRect::TypeEnum::TouchEventHandler));
  }
  if (reportWheelScrollers) {
    WebRect webRect(webLayer->position().x, webLayer->position().y,
                    webLayer->bounds().width, webLayer->bounds().height);
    scrollRects->addItem(buildScrollRect(
        webRect, protocol::LayerTree::ScrollRect::TypeEnum::WheelEventHandler));
  }
  return scrollRects->length() ? std::move(scrollRects) : nullptr;
}

static std::unique_ptr<protocol::LayerTree::Layer> buildObjectForLayer(
    GraphicsLayer* graphicsLayer,
    int nodeId,
    bool reportWheelEventListeners) {
  WebLayer* webLayer = graphicsLayer->platformLayer();
  std::unique_ptr<protocol::LayerTree::Layer> layerObject =
      protocol::LayerTree::Layer::create()
          .setLayerId(idForLayer(graphicsLayer))
          .setOffsetX(webLayer->position().x)
          .setOffsetY(webLayer->position().y)
          .setWidth(webLayer->bounds().width)
          .setHeight(webLayer->bounds().height)
          .setPaintCount(graphicsLayer->paintCount())
          .setDrawsContent(webLayer->drawsContent())
          .build();

  if (nodeId)
    layerObject->setBackendNodeId(nodeId);

  GraphicsLayer* parent = graphicsLayer->parent();
  if (parent)
    layerObject->setParentLayerId(idForLayer(parent));
  if (!graphicsLayer->contentsAreVisible())
    layerObject->setInvisible(true);
  const TransformationMatrix& transform = graphicsLayer->transform();
  if (!transform.isIdentity()) {
    TransformationMatrix::FloatMatrix4 flattenedMatrix;
    transform.toColumnMajorFloatArray(flattenedMatrix);
    std::unique_ptr<Array<double>> transformArray = Array<double>::create();
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(flattenedMatrix); ++i)
      transformArray->addItem(flattenedMatrix[i]);
    layerObject->setTransform(std::move(transformArray));
    const FloatPoint3D& transformOrigin = graphicsLayer->transformOrigin();
    // FIXME: rename these to setTransformOrigin*
    if (webLayer->bounds().width > 0)
      layerObject->setAnchorX(transformOrigin.x() / webLayer->bounds().width);
    else
      layerObject->setAnchorX(0.0);
    if (webLayer->bounds().height > 0)
      layerObject->setAnchorY(transformOrigin.y() / webLayer->bounds().height);
    else
      layerObject->setAnchorY(0.0);
    layerObject->setAnchorZ(transformOrigin.z());
  }
  std::unique_ptr<Array<protocol::LayerTree::ScrollRect>> scrollRects =
      buildScrollRectsForLayer(graphicsLayer, reportWheelEventListeners);
  if (scrollRects)
    layerObject->setScrollRects(std::move(scrollRects));
  return layerObject;
}

InspectorLayerTreeAgent::InspectorLayerTreeAgent(
    InspectedFrames* inspectedFrames)
    : m_inspectedFrames(inspectedFrames), m_suppressLayerPaintEvents(false) {}

InspectorLayerTreeAgent::~InspectorLayerTreeAgent() {}

DEFINE_TRACE(InspectorLayerTreeAgent) {
  visitor->trace(m_inspectedFrames);
  InspectorBaseAgent::trace(visitor);
}

void InspectorLayerTreeAgent::restore() {
  // We do not re-enable layer agent automatically after navigation. This is
  // because it depends on DOMAgent and node ids in particular, so we let
  // front-end request document and re-enable the agent manually after this.
}

Response InspectorLayerTreeAgent::enable() {
  m_instrumentingAgents->addInspectorLayerTreeAgent(this);
  Document* document = m_inspectedFrames->root()->document();
  if (document &&
      document->lifecycle().state() >= DocumentLifecycle::CompositingClean)
    layerTreeDidChange();
  return Response::OK();
}

Response InspectorLayerTreeAgent::disable() {
  m_instrumentingAgents->removeInspectorLayerTreeAgent(this);
  m_snapshotById.clear();
  return Response::OK();
}

void InspectorLayerTreeAgent::layerTreeDidChange() {
  frontend()->layerTreeDidChange(buildLayerTree());
}

void InspectorLayerTreeAgent::didPaint(const GraphicsLayer* graphicsLayer,
                                       GraphicsContext&,
                                       const LayoutRect& rect) {
  if (m_suppressLayerPaintEvents)
    return;
  // Should only happen for FrameView paints when compositing is off. Consider
  // different instrumentation method for that.
  if (!graphicsLayer)
    return;

  std::unique_ptr<protocol::DOM::Rect> domRect = protocol::DOM::Rect::create()
                                                     .setX(rect.x())
                                                     .setY(rect.y())
                                                     .setWidth(rect.width())
                                                     .setHeight(rect.height())
                                                     .build();
  frontend()->layerPainted(idForLayer(graphicsLayer), std::move(domRect));
}

std::unique_ptr<Array<protocol::LayerTree::Layer>>
InspectorLayerTreeAgent::buildLayerTree() {
  PaintLayerCompositor* compositor = paintLayerCompositor();
  if (!compositor || !compositor->inCompositingMode())
    return nullptr;

  LayerIdToNodeIdMap layerIdToNodeIdMap;
  std::unique_ptr<Array<protocol::LayerTree::Layer>> layers =
      Array<protocol::LayerTree::Layer>::create();
  buildLayerIdToNodeIdMap(compositor->rootLayer(), layerIdToNodeIdMap);
  int scrollingLayerId = m_inspectedFrames->root()
                             ->view()
                             ->layerForScrolling()
                             ->platformLayer()
                             ->id();
  bool haveBlockingWheelEventHandlers =
      m_inspectedFrames->root()->chromeClient().eventListenerProperties(
          m_inspectedFrames->root(), WebEventListenerClass::MouseWheel) ==
      WebEventListenerProperties::Blocking;

  gatherGraphicsLayers(rootGraphicsLayer(), layerIdToNodeIdMap, layers,
                       haveBlockingWheelEventHandlers, scrollingLayerId);
  return layers;
}

void InspectorLayerTreeAgent::buildLayerIdToNodeIdMap(
    PaintLayer* root,
    LayerIdToNodeIdMap& layerIdToNodeIdMap) {
  if (root->hasCompositedLayerMapping()) {
    if (Node* node = root->layoutObject().generatingNode()) {
      GraphicsLayer* graphicsLayer =
          root->compositedLayerMapping()->childForSuperlayers();
      layerIdToNodeIdMap.set(graphicsLayer->platformLayer()->id(),
                             idForNode(node));
    }
  }
  for (PaintLayer* child = root->firstChild(); child;
       child = child->nextSibling())
    buildLayerIdToNodeIdMap(child, layerIdToNodeIdMap);
  if (!root->layoutObject().isLayoutIFrame())
    return;
  FrameView* childFrameView =
      toFrameView(toLayoutPart(root->layoutObject()).widget());
  LayoutViewItem childLayoutViewItem = childFrameView->layoutViewItem();
  if (!childLayoutViewItem.isNull()) {
    if (PaintLayerCompositor* childCompositor =
            childLayoutViewItem.compositor())
      buildLayerIdToNodeIdMap(childCompositor->rootLayer(), layerIdToNodeIdMap);
  }
}

void InspectorLayerTreeAgent::gatherGraphicsLayers(
    GraphicsLayer* root,
    HashMap<int, int>& layerIdToNodeIdMap,
    std::unique_ptr<Array<protocol::LayerTree::Layer>>& layers,
    bool hasWheelEventHandlers,
    int scrollingLayerId) {
  int layerId = root->platformLayer()->id();
  if (m_pageOverlayLayerIds.find(layerId) != WTF::kNotFound)
    return;
  layers->addItem(buildObjectForLayer(
      root, layerIdToNodeIdMap.at(layerId),
      hasWheelEventHandlers && layerId == scrollingLayerId));
  for (size_t i = 0, size = root->children().size(); i < size; ++i)
    gatherGraphicsLayers(root->children()[i], layerIdToNodeIdMap, layers,
                         hasWheelEventHandlers, scrollingLayerId);
}

int InspectorLayerTreeAgent::idForNode(Node* node) {
  return DOMNodeIds::idForNode(node);
}

PaintLayerCompositor* InspectorLayerTreeAgent::paintLayerCompositor() {
  LayoutViewItem layoutView = m_inspectedFrames->root()->contentLayoutItem();
  PaintLayerCompositor* compositor =
      layoutView.isNull() ? nullptr : layoutView.compositor();
  return compositor;
}

GraphicsLayer* InspectorLayerTreeAgent::rootGraphicsLayer() {
  return m_inspectedFrames->root()
      ->host()
      ->visualViewport()
      .rootGraphicsLayer();
}

static GraphicsLayer* findLayerById(GraphicsLayer* root, int layerId) {
  if (root->platformLayer()->id() == layerId)
    return root;
  for (size_t i = 0, size = root->children().size(); i < size; ++i) {
    if (GraphicsLayer* layer = findLayerById(root->children()[i], layerId))
      return layer;
  }
  return nullptr;
}

Response InspectorLayerTreeAgent::layerById(const String& layerId,
                                            GraphicsLayer*& result) {
  bool ok;
  int id = layerId.toInt(&ok);
  if (!ok)
    return Response::Error("Invalid layer id");
  PaintLayerCompositor* compositor = paintLayerCompositor();
  if (!compositor)
    return Response::Error("Not in compositing mode");

  result = findLayerById(rootGraphicsLayer(), id);
  if (!result)
    return Response::Error("No layer matching given id found");
  return Response::OK();
}

Response InspectorLayerTreeAgent::compositingReasons(
    const String& layerId,
    std::unique_ptr<Array<String>>* reasonStrings) {
  GraphicsLayer* graphicsLayer = nullptr;
  Response response = layerById(layerId, graphicsLayer);
  if (!response.isSuccess())
    return response;
  CompositingReasons reasonsBitmask = graphicsLayer->getCompositingReasons();
  *reasonStrings = Array<String>::create();
  for (size_t i = 0; i < kNumberOfCompositingReasons; ++i) {
    if (!(reasonsBitmask & kCompositingReasonStringMap[i].reason))
      continue;
    (*reasonStrings)->addItem(kCompositingReasonStringMap[i].shortName);
#ifndef _NDEBUG
    reasonsBitmask &= ~kCompositingReasonStringMap[i].reason;
#endif
  }
  ASSERT(!reasonsBitmask);
  return Response::OK();
}

Response InspectorLayerTreeAgent::makeSnapshot(const String& layerId,
                                               String* snapshotId) {
  GraphicsLayer* layer = nullptr;
  Response response = layerById(layerId, layer);
  if (!response.isSuccess())
    return response;
  if (!layer->drawsContent())
    return Response::Error("Layer does not draw content");

  IntSize size = expandedIntSize(layer->size());

  IntRect interestRect(IntPoint(0, 0), size);
  m_suppressLayerPaintEvents = true;
  layer->paint(&interestRect);
  m_suppressLayerPaintEvents = false;

  GraphicsContext context(layer->getPaintController());
  context.beginRecording(interestRect);
  layer->getPaintController().paintArtifact().replay(context);
  RefPtr<PictureSnapshot> snapshot =
      adoptRef(new PictureSnapshot(ToSkPicture(context.endRecording())));

  *snapshotId = String::number(++s_lastSnapshotId);
  bool newEntry = m_snapshotById.insert(*snapshotId, snapshot).isNewEntry;
  DCHECK(newEntry);
  return Response::OK();
}

Response InspectorLayerTreeAgent::loadSnapshot(
    std::unique_ptr<Array<protocol::LayerTree::PictureTile>> tiles,
    String* snapshotId) {
  if (!tiles->length())
    return Response::Error("Invalid argument, no tiles provided");
  Vector<RefPtr<PictureSnapshot::TilePictureStream>> decodedTiles;
  decodedTiles.grow(tiles->length());
  for (size_t i = 0; i < tiles->length(); ++i) {
    protocol::LayerTree::PictureTile* tile = tiles->get(i);
    decodedTiles[i] = adoptRef(new PictureSnapshot::TilePictureStream());
    decodedTiles[i]->layerOffset.set(tile->getX(), tile->getY());
    if (!base64Decode(tile->getPicture(), decodedTiles[i]->data))
      return Response::Error("Invalid base64 encoding");
  }
  RefPtr<PictureSnapshot> snapshot = PictureSnapshot::load(decodedTiles);
  if (!snapshot)
    return Response::Error("Invalid snapshot format");
  if (snapshot->isEmpty())
    return Response::Error("Empty snapshot");

  *snapshotId = String::number(++s_lastSnapshotId);
  bool newEntry = m_snapshotById.insert(*snapshotId, snapshot).isNewEntry;
  DCHECK(newEntry);
  return Response::OK();
}

Response InspectorLayerTreeAgent::releaseSnapshot(const String& snapshotId) {
  SnapshotById::iterator it = m_snapshotById.find(snapshotId);
  if (it == m_snapshotById.end())
    return Response::Error("Snapshot not found");
  m_snapshotById.remove(it);
  return Response::OK();
}

Response InspectorLayerTreeAgent::snapshotById(const String& snapshotId,
                                               const PictureSnapshot*& result) {
  SnapshotById::iterator it = m_snapshotById.find(snapshotId);
  if (it == m_snapshotById.end())
    return Response::Error("Snapshot not found");
  result = it->value.get();
  return Response::OK();
}

Response InspectorLayerTreeAgent::replaySnapshot(const String& snapshotId,
                                                 Maybe<int> fromStep,
                                                 Maybe<int> toStep,
                                                 Maybe<double> scale,
                                                 String* dataURL) {
  const PictureSnapshot* snapshot = nullptr;
  Response response = snapshotById(snapshotId, snapshot);
  if (!response.isSuccess())
    return response;
  std::unique_ptr<Vector<char>> base64Data = snapshot->replay(
      fromStep.fromMaybe(0), toStep.fromMaybe(0), scale.fromMaybe(1.0));
  if (!base64Data)
    return Response::Error("Image encoding failed");
  StringBuilder url;
  url.append("data:image/png;base64,");
  url.reserveCapacity(url.length() + base64Data->size());
  url.append(base64Data->begin(), base64Data->size());
  *dataURL = url.toString();
  return Response::OK();
}

static void parseRect(protocol::DOM::Rect* object, FloatRect* rect) {
  *rect = FloatRect(object->getX(), object->getY(), object->getWidth(),
                    object->getHeight());
}

Response InspectorLayerTreeAgent::profileSnapshot(
    const String& snapshotId,
    Maybe<int> minRepeatCount,
    Maybe<double> minDuration,
    Maybe<protocol::DOM::Rect> clipRect,
    std::unique_ptr<protocol::Array<protocol::Array<double>>>* outTimings) {
  const PictureSnapshot* snapshot = nullptr;
  Response response = snapshotById(snapshotId, snapshot);
  if (!response.isSuccess())
    return response;
  FloatRect rect;
  if (clipRect.isJust())
    parseRect(clipRect.fromJust(), &rect);
  std::unique_ptr<PictureSnapshot::Timings> timings =
      snapshot->profile(minRepeatCount.fromMaybe(1), minDuration.fromMaybe(0),
                        clipRect.isJust() ? &rect : 0);
  *outTimings = Array<Array<double>>::create();
  for (size_t i = 0; i < timings->size(); ++i) {
    const Vector<double>& row = (*timings)[i];
    std::unique_ptr<Array<double>> outRow = Array<double>::create();
    for (size_t j = 0; j < row.size(); ++j)
      outRow->addItem(row[j]);
    (*outTimings)->addItem(std::move(outRow));
  }
  return Response::OK();
}

Response InspectorLayerTreeAgent::snapshotCommandLog(
    const String& snapshotId,
    std::unique_ptr<Array<protocol::DictionaryValue>>* commandLog) {
  const PictureSnapshot* snapshot = nullptr;
  Response response = snapshotById(snapshotId, snapshot);
  if (!response.isSuccess())
    return response;
  protocol::ErrorSupport errors;
  std::unique_ptr<protocol::Value> logValue = protocol::StringUtil::parseJSON(
      snapshot->snapshotCommandLog()->toJSONString());
  *commandLog =
      Array<protocol::DictionaryValue>::fromValue(logValue.get(), &errors);
  if (errors.hasErrors())
    return Response::Error(errors.errors());
  return Response::OK();
}

void InspectorLayerTreeAgent::willAddPageOverlay(const GraphicsLayer* layer) {
  m_pageOverlayLayerIds.push_back(layer->platformLayer()->id());
}

void InspectorLayerTreeAgent::didRemovePageOverlay(const GraphicsLayer* layer) {
  size_t index = m_pageOverlayLayerIds.find(layer->platformLayer()->id());
  if (index == WTF::kNotFound)
    return;
  m_pageOverlayLayerIds.remove(index);
}

}  // namespace blink
