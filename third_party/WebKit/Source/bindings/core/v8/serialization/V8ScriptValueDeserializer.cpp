// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/V8ScriptValueDeserializer.h"

#include "bindings/core/v8/ToV8.h"
#include "core/dom/CompositorProxy.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMSharedArrayBuffer.h"
#include "core/dom/MessagePort.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "core/fileapi/FileList.h"
#include "core/frame/ImageBitmap.h"
#include "core/html/ImageData.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "public/platform/WebBlobInfo.h"
#include "wtf/CheckedNumeric.h"
#include "wtf/DateMath.h"

namespace blink {

namespace {

// The "Blink-side" serialization version, which defines how Blink will behave
// during the serialization process. The serialization format has two
// "envelopes": an outer one controlled by Blink and an inner one by V8.
//
// They are formatted as follows:
// [version tag] [Blink version] [version tag] [v8 version] ...
//
// Before version 16, there was only a single envelope and the version number
// for both parts was always equal.
//
// See also V8ScriptValueDeserializer.cpp.
const uint32_t kMinVersionForSeparateEnvelope = 16;

// Returns the number of bytes consumed reading the Blink version envelope, and
// sets |*version| to the version. If no Blink envelope was detected, zero is
// returned.
size_t readVersionEnvelope(SerializedScriptValue* serializedScriptValue,
                           uint32_t* outVersion) {
  const uint8_t* rawData = serializedScriptValue->data();
  const size_t length = serializedScriptValue->dataLengthInBytes();
  if (!length || rawData[0] != VersionTag)
    return 0;

  // Read a 32-bit unsigned integer from varint encoding.
  uint32_t version = 0;
  size_t i = 1;
  unsigned shift = 0;
  bool hasAnotherByte;
  do {
    if (i >= length)
      return 0;
    uint8_t byte = rawData[i];
    if (LIKELY(shift < 32)) {
      version |= static_cast<uint32_t>(byte & 0x7f) << shift;
      shift += 7;
    }
    hasAnotherByte = byte & 0x80;
    i++;
  } while (hasAnotherByte);

  // If the version in the envelope is too low, this was not a Blink version
  // envelope.
  if (version < kMinVersionForSeparateEnvelope)
    return 0;

  // Otherwise, we did read the envelope. Hurray!
  *outVersion = version;
  return i;
}

}  // namespace

V8ScriptValueDeserializer::V8ScriptValueDeserializer(
    RefPtr<ScriptState> scriptState,
    RefPtr<SerializedScriptValue> serializedScriptValue)
    : m_scriptState(std::move(scriptState)),
      m_serializedScriptValue(std::move(serializedScriptValue)),
      m_deserializer(m_scriptState->isolate(),
                     m_serializedScriptValue->data(),
                     m_serializedScriptValue->dataLengthInBytes(),
                     this) {
  m_deserializer.SetSupportsLegacyWireFormat(true);
}

v8::Local<v8::Value> V8ScriptValueDeserializer::deserialize() {
#if DCHECK_IS_ON()
  DCHECK(!m_deserializeInvoked);
  m_deserializeInvoked = true;
#endif

  v8::Isolate* isolate = m_scriptState->isolate();
  v8::EscapableHandleScope scope(isolate);
  v8::TryCatch tryCatch(isolate);
  v8::Local<v8::Context> context = m_scriptState->context();

  size_t versionEnvelopeSize =
      readVersionEnvelope(m_serializedScriptValue.get(), &m_version);
  if (versionEnvelopeSize) {
    const void* blinkEnvelope;
    bool readEnvelope = readRawBytes(versionEnvelopeSize, &blinkEnvelope);
    DCHECK(readEnvelope);
    DCHECK_GE(m_version, kMinVersionForSeparateEnvelope);
  } else {
    DCHECK_EQ(m_version, 0u);
  }

  bool readHeader;
  if (!m_deserializer.ReadHeader(context).To(&readHeader))
    return v8::Null(isolate);
  DCHECK(readHeader);

  // If there was no Blink envelope earlier, Blink shares the wire format
  // version from the V8 header.
  if (!m_version)
    m_version = m_deserializer.GetWireFormatVersion();

  // Prepare to transfer the provided transferables.
  transfer();

  v8::Local<v8::Value> value;
  if (!m_deserializer.ReadValue(context).ToLocal(&value))
    return v8::Null(isolate);
  return scope.Escape(value);
}

void V8ScriptValueDeserializer::transfer() {
  v8::Isolate* isolate = m_scriptState->isolate();
  v8::Local<v8::Context> context = m_scriptState->context();
  v8::Local<v8::Object> creationContext = context->Global();

  // Transfer array buffers.
  if (auto* arrayBufferContents =
          m_serializedScriptValue->getArrayBufferContentsArray()) {
    for (unsigned i = 0; i < arrayBufferContents->size(); i++) {
      WTF::ArrayBufferContents& contents = arrayBufferContents->at(i);
      if (contents.isShared()) {
        DOMSharedArrayBuffer* arrayBuffer =
            DOMSharedArrayBuffer::create(contents);
        v8::Local<v8::Value> wrapper =
            ToV8(arrayBuffer, creationContext, isolate);
        DCHECK(wrapper->IsSharedArrayBuffer());
        m_deserializer.TransferSharedArrayBuffer(
            i, v8::Local<v8::SharedArrayBuffer>::Cast(wrapper));
      } else {
        DOMArrayBuffer* arrayBuffer = DOMArrayBuffer::create(contents);
        v8::Local<v8::Value> wrapper =
            ToV8(arrayBuffer, creationContext, isolate);
        DCHECK(wrapper->IsArrayBuffer());
        m_deserializer.TransferArrayBuffer(
            i, v8::Local<v8::ArrayBuffer>::Cast(wrapper));
      }
    }
  }

  // Transfer image bitmaps.
  if (auto* imageBitmapContents =
          m_serializedScriptValue->getImageBitmapContentsArray()) {
    m_transferredImageBitmaps.reserveInitialCapacity(
        imageBitmapContents->size());
    for (const auto& image : *imageBitmapContents)
      m_transferredImageBitmaps.push_back(ImageBitmap::create(image));
  }
}

bool V8ScriptValueDeserializer::readUTF8String(String* string) {
  uint32_t utf8Length = 0;
  const void* utf8Data = nullptr;
  if (!readUint32(&utf8Length) || !readRawBytes(utf8Length, &utf8Data))
    return false;
  *string =
      String::fromUTF8(reinterpret_cast<const LChar*>(utf8Data), utf8Length);
  return true;
}

ScriptWrappable* V8ScriptValueDeserializer::readDOMObject(
    SerializationTag tag) {
  switch (tag) {
    case BlobTag: {
      if (version() < 3)
        return nullptr;
      String uuid, type;
      uint64_t size;
      if (!readUTF8String(&uuid) || !readUTF8String(&type) ||
          !readUint64(&size))
        return nullptr;
      return Blob::create(getOrCreateBlobDataHandle(uuid, type, size));
    }
    case BlobIndexTag: {
      if (version() < 6 || !m_blobInfoArray)
        return nullptr;
      uint32_t index = 0;
      if (!readUint32(&index) || index >= m_blobInfoArray->size())
        return nullptr;
      const WebBlobInfo& info = (*m_blobInfoArray)[index];
      return Blob::create(
          getOrCreateBlobDataHandle(info.uuid(), info.type(), info.size()));
    }
    case CompositorProxyTag: {
      uint64_t element;
      uint32_t properties;
      const uint32_t validPropertiesMask = static_cast<uint32_t>(
          (1u << CompositorMutableProperty::kNumProperties) - 1);
      if (!RuntimeEnabledFeatures::compositorWorkerEnabled() ||
          !readUint64(&element) || !readUint32(&properties) || !properties ||
          (properties & ~validPropertiesMask))
        return nullptr;
      return CompositorProxy::create(m_scriptState->getExecutionContext(),
                                     element, properties);
    }
    case FileTag:
      return readFile();
    case FileIndexTag:
      return readFileIndex();
    case FileListTag: {
      // This does not presently deduplicate a File object and its entry in a
      // FileList, which is non-standard behavior.
      uint32_t length;
      if (!readUint32(&length))
        return nullptr;
      FileList* fileList = FileList::create();
      for (uint32_t i = 0; i < length; i++) {
        if (File* file = readFile())
          fileList->append(file);
        else
          return nullptr;
      }
      return fileList;
    }
    case FileListIndexTag: {
      // This does not presently deduplicate a File object and its entry in a
      // FileList, which is non-standard behavior.
      uint32_t length;
      if (!readUint32(&length))
        return nullptr;
      FileList* fileList = FileList::create();
      for (uint32_t i = 0; i < length; i++) {
        if (File* file = readFileIndex())
          fileList->append(file);
        else
          return nullptr;
      }
      return fileList;
    }
    case ImageBitmapTag: {
      uint32_t originClean = 0, isPremultiplied = 0, width = 0, height = 0,
               pixelLength = 0;
      const void* pixels = nullptr;
      if (!readUint32(&originClean) || originClean > 1 ||
          !readUint32(&isPremultiplied) || isPremultiplied > 1 ||
          !readUint32(&width) || !readUint32(&height) ||
          !readUint32(&pixelLength) || !readRawBytes(pixelLength, &pixels))
        return nullptr;
      CheckedNumeric<uint32_t> computedPixelLength = width;
      computedPixelLength *= height;
      computedPixelLength *= 4;
      if (!computedPixelLength.IsValid() ||
          computedPixelLength.ValueOrDie() != pixelLength)
        return nullptr;
      return ImageBitmap::create(pixels, width, height, isPremultiplied,
                                 originClean);
    }
    case ImageBitmapTransferTag: {
      uint32_t index = 0;
      if (!readUint32(&index) || index >= m_transferredImageBitmaps.size())
        return nullptr;
      return m_transferredImageBitmaps[index].get();
    }
    case ImageDataTag: {
      uint32_t width = 0, height = 0, pixelLength = 0;
      const void* pixels = nullptr;
      if (!readUint32(&width) || !readUint32(&height) ||
          !readUint32(&pixelLength) || !readRawBytes(pixelLength, &pixels))
        return nullptr;
      CheckedNumeric<uint32_t> computedPixelLength = width;
      computedPixelLength *= height;
      computedPixelLength *= 4;
      if (!computedPixelLength.IsValid() ||
          computedPixelLength.ValueOrDie() != pixelLength)
        return nullptr;
      ImageData* imageData = ImageData::create(IntSize(width, height));
      if (!imageData)
        return nullptr;
      DOMUint8ClampedArray* pixelArray = imageData->data();
      DCHECK_EQ(pixelArray->length(), pixelLength);
      memcpy(pixelArray->data(), pixels, pixelLength);
      return imageData;
    }
    case MessagePortTag: {
      uint32_t index = 0;
      if (!readUint32(&index) || !m_transferredMessagePorts ||
          index >= m_transferredMessagePorts->size())
        return nullptr;
      return (*m_transferredMessagePorts)[index].get();
    }
    case OffscreenCanvasTransferTag: {
      uint32_t width = 0, height = 0, canvasId = 0, clientId = 0, sinkId = 0;
      if (!readUint32(&width) || !readUint32(&height) ||
          !readUint32(&canvasId) || !readUint32(&clientId) ||
          !readUint32(&sinkId))
        return nullptr;
      OffscreenCanvas* canvas = OffscreenCanvas::create(width, height);
      canvas->setPlaceholderCanvasId(canvasId);
      canvas->setFrameSinkId(clientId, sinkId);
      return canvas;
    }
    default:
      break;
  }
  return nullptr;
}

File* V8ScriptValueDeserializer::readFile() {
  if (version() < 3)
    return nullptr;
  String path, name, relativePath, uuid, type;
  uint32_t hasSnapshot = 0;
  uint64_t size = 0;
  double lastModifiedMs = 0;
  if (!readUTF8String(&path) || (version() >= 4 && !readUTF8String(&name)) ||
      (version() >= 4 && !readUTF8String(&relativePath)) ||
      !readUTF8String(&uuid) || !readUTF8String(&type) ||
      (version() >= 4 && !readUint32(&hasSnapshot)))
    return nullptr;
  if (hasSnapshot) {
    if (!readUint64(&size) || !readDouble(&lastModifiedMs))
      return nullptr;
    if (version() < 8)
      lastModifiedMs *= msPerSecond;
  }
  uint32_t isUserVisible = 1;
  if (version() >= 7 && !readUint32(&isUserVisible))
    return nullptr;
  const File::UserVisibility userVisibility =
      isUserVisible ? File::IsUserVisible : File::IsNotUserVisible;
  const uint64_t sizeForDataHandle = static_cast<uint64_t>(-1);
  return File::createFromSerialization(
      path, name, relativePath, userVisibility, hasSnapshot, size,
      lastModifiedMs, getOrCreateBlobDataHandle(uuid, type, sizeForDataHandle));
}

File* V8ScriptValueDeserializer::readFileIndex() {
  if (version() < 6 || !m_blobInfoArray)
    return nullptr;
  uint32_t index;
  if (!readUint32(&index) || index >= m_blobInfoArray->size())
    return nullptr;
  const WebBlobInfo& info = (*m_blobInfoArray)[index];
  // FIXME: transition WebBlobInfo.lastModified to be milliseconds-based also.
  double lastModifiedMs = info.lastModified() * msPerSecond;
  return File::createFromIndexedSerialization(
      info.filePath(), info.fileName(), info.size(), lastModifiedMs,
      getOrCreateBlobDataHandle(info.uuid(), info.type(), info.size()));
}

RefPtr<BlobDataHandle> V8ScriptValueDeserializer::getOrCreateBlobDataHandle(
    const String& uuid,
    const String& type,
    uint64_t size) {
  // The containing ssv may have a BDH for this uuid if this ssv is just being
  // passed from main to worker thread (for example). We use those values when
  // creating the new blob instead of cons'ing up a new BDH.
  //
  // FIXME: Maybe we should require that it work that way where the ssv must
  // have a BDH for any blobs it comes across during deserialization. Would
  // require callers to explicitly populate the collection of BDH's for blobs to
  // work, which would encourage lifetimes to be considered when passing ssv's
  // around cross process. At present, we get 'lucky' in some cases because the
  // blob in the src process happens to still exist at the time the dest process
  // is deserializing.
  // For example in sharedWorker.postMessage(...).
  BlobDataHandleMap& handles = m_serializedScriptValue->blobDataHandles();
  BlobDataHandleMap::const_iterator it = handles.find(uuid);
  if (it != handles.end())
    return it->value;
  return BlobDataHandle::create(uuid, type, size);
}

v8::MaybeLocal<v8::Object> V8ScriptValueDeserializer::ReadHostObject(
    v8::Isolate* isolate) {
  DCHECK_EQ(isolate, m_scriptState->isolate());
  ExceptionState exceptionState(isolate, ExceptionState::UnknownContext,
                                nullptr, nullptr);
  ScriptWrappable* wrappable = nullptr;
  SerializationTag tag = VersionTag;
  if (readTag(&tag))
    wrappable = readDOMObject(tag);
  if (!wrappable) {
    exceptionState.throwDOMException(DataCloneError,
                                     "Unable to deserialize cloned data.");
    return v8::MaybeLocal<v8::Object>();
  }
  v8::Local<v8::Object> creationContext = m_scriptState->context()->Global();
  v8::Local<v8::Value> wrapper = ToV8(wrappable, creationContext, isolate);
  DCHECK(wrapper->IsObject());
  return wrapper.As<v8::Object>();
}

}  // namespace blink
