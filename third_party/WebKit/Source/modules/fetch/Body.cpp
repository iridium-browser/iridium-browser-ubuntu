// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/Body.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMTypedArray.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/UseCounter.h"
#include "core/streams/ReadableByteStream.h"
#include "core/streams/ReadableByteStreamReader.h"
#include "core/streams/UnderlyingSource.h"
#include "modules/fetch/BodyStreamBuffer.h"

namespace blink {

class Body::BlobHandleReceiver final : public BodyStreamBuffer::BlobHandleCreatorClient {
public:
    explicit BlobHandleReceiver(Body* body)
        : m_body(body)
    {
    }
    void didCreateBlobHandle(PassRefPtr<BlobDataHandle> handle) override
    {
        ASSERT(m_body);
        m_body->readAsyncFromBlob(handle);
        m_body = nullptr;
    }
    void didFail(DOMException* exception) override
    {
        ASSERT(m_body);
        m_body->didBlobHandleReceiveError(exception);
        m_body = nullptr;
    }
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        BodyStreamBuffer::BlobHandleCreatorClient::trace(visitor);
        visitor->trace(m_body);
    }
private:
    Member<Body> m_body;
};

// This class is an ActiveDOMObject subclass only for holding the
// ExecutionContext used in |pullSource|.
class Body::ReadableStreamSource : public BodyStreamBuffer::Observer, public UnderlyingSource, public FileReaderLoaderClient, public ActiveDOMObject {
    USING_GARBAGE_COLLECTED_MIXIN(ReadableStreamSource);
public:
    enum State {
        Initial,
        Streaming,
        Closed,
        Errored,
    };
    ReadableStreamSource(ExecutionContext* executionContext, PassRefPtr<BlobDataHandle> handle)
        : ActiveDOMObject(executionContext)
        , m_blobDataHandle(handle ? handle : BlobDataHandle::create(BlobData::create(), 0))
        , m_state(Initial)
    {
        suspendIfNeeded();
    }

    ReadableStreamSource(ExecutionContext* executionContext, BodyStreamBuffer* buffer)
        : ActiveDOMObject(executionContext)
        , m_bodyStreamBuffer(buffer)
        , m_state(Initial)
    {
        suspendIfNeeded();
    }

    explicit ReadableStreamSource(ExecutionContext* executionContext)
        : ActiveDOMObject(executionContext)
        , m_blobDataHandle(BlobDataHandle::create(BlobData::create(), 0))
        , m_state(Initial)
    {
        suspendIfNeeded();
    }

    ~ReadableStreamSource() override { }

    State state() const { return m_state; }

    void startStream(ReadableByteStream* stream)
    {
        m_stream = stream;
        stream->didSourceStart();
    }
    // Creates a new BodyStreamBuffer to drain the data.
    BodyStreamBuffer* createDrainingStream()
    {
        ASSERT(m_state != Initial);

        auto drainingStreamBuffer = new BodyStreamBuffer(new Canceller(this));
        if (m_stream->stateInternal() == ReadableByteStream::Closed) {
            drainingStreamBuffer->close();
            return drainingStreamBuffer;
        }
        if (m_stream->stateInternal() == ReadableByteStream::Errored) {
            drainingStreamBuffer->error(exception());
            return drainingStreamBuffer;
        }

        ASSERT(!m_drainingStreamBuffer);
        // Take back the data in |m_stream|.
        Deque<std::pair<RefPtr<DOMArrayBufferView>, size_t>> tmp_queue;
        ASSERT(m_stream->stateInternal() == ReadableStream::Readable);
        m_stream->readInternal(tmp_queue);
        while (!tmp_queue.isEmpty()) {
            std::pair<RefPtr<DOMArrayBufferView>, size_t> data = tmp_queue.takeFirst();
            drainingStreamBuffer->write(data.first->buffer());
        }
        if (m_state == Closed)
            drainingStreamBuffer->close();

        m_drainingStreamBuffer = drainingStreamBuffer;
        return m_drainingStreamBuffer;
    }
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_bodyStreamBuffer);
        visitor->trace(m_drainingStreamBuffer);
        visitor->trace(m_stream);
        BodyStreamBuffer::Observer::trace(visitor);
        UnderlyingSource::trace(visitor);
        ActiveDOMObject::trace(visitor);
    }

    void close()
    {
        if (m_state == Closed) {
            // It is possible to call |close| from the source side (such
            // as blob loading finish) and from the consumer side (such as
            // calling |cancel|). Thus we should ignore it here.
            return;
        }
        m_state = Closed;
        if (m_drainingStreamBuffer)
            m_drainingStreamBuffer->close();
        m_stream->close();
    }
    void error()
    {
        m_state = Errored;
        if (m_drainingStreamBuffer)
            m_drainingStreamBuffer->error(exception());
        m_stream->error(exception());
    }

private:
    class Canceller : public BodyStreamBuffer::Canceller {
    public:
        Canceller(ReadableStreamSource* source) : m_source(source) { }
        void cancel() override
        {
            m_source->cancel();
        }

        DEFINE_INLINE_VIRTUAL_TRACE()
        {
            visitor->trace(m_source);
            BodyStreamBuffer::Canceller::trace(visitor);
        }

    private:
        Member<ReadableStreamSource> m_source;
    };

    // UnderlyingSource functions.
    void pullSource() override
    {
        // Note that one |pull| is called only when |read| is called on the
        // associated ReadableByteStreamReader because we create a stream with
        // StrictStrategy.
        if (m_state == Initial) {
            m_state = Streaming;
            if (m_bodyStreamBuffer) {
                m_bodyStreamBuffer->registerObserver(this);
                onWrite();
                if (m_bodyStreamBuffer->hasError())
                    return onError();
                if (m_bodyStreamBuffer->isClosed())
                    return onClose();
            } else {
                FileReaderLoader::ReadType readType = FileReaderLoader::ReadAsArrayBuffer;
                m_loader = adoptPtr(new FileReaderLoader(readType, this));
                m_loader->start(executionContext(), m_blobDataHandle);
            }
        }
    }

    ScriptPromise cancelSource(ScriptState* scriptState, ScriptValue reason) override
    {
        cancel();
        return ScriptPromise::cast(scriptState, v8::Undefined(scriptState->isolate()));
    }

    // BodyStreamBuffer::Observer functions.
    void onWrite() override
    {
        ASSERT(m_state == Streaming);
        while (RefPtr<DOMArrayBuffer> buf = m_bodyStreamBuffer->read()) {
            write(buf);
        }
    }
    void onClose() override
    {
        ASSERT(m_state == Streaming);
        close();
        m_bodyStreamBuffer->unregisterObserver();
    }
    void onError() override
    {
        ASSERT(m_state == Streaming);
        error();
        m_bodyStreamBuffer->unregisterObserver();
    }

    // FileReaderLoaderClient functions.
    void didStartLoading() override { }
    void didReceiveData() override { }
    void didFinishLoading() override
    {
        ASSERT(m_state == Streaming);
        write(m_loader->arrayBufferResult());
        close();
    }
    void didFail(FileError::ErrorCode) override
    {
        ASSERT(m_state == Streaming);
        error();
    }

    void write(PassRefPtr<DOMArrayBuffer> buf)
    {
        if (m_drainingStreamBuffer) {
            m_drainingStreamBuffer->write(buf);
        } else {
            auto size = buf->byteLength();
            m_stream->enqueue(DOMUint8Array::create(buf, 0, size));
        }
    }
    void cancel()
    {
        if (m_bodyStreamBuffer) {
            m_bodyStreamBuffer->cancel();
            // We should not close the stream here, because it is canceller's
            // responsibility.
        } else {
            if (m_loader)
                m_loader->cancel();
            close();
        }
    }

    DOMException* exception()
    {
        if (m_state != Errored)
            return nullptr;
        if (m_bodyStreamBuffer) {
            ASSERT(m_bodyStreamBuffer->exception());
            return m_bodyStreamBuffer->exception();
        }
        return DOMException::create(NetworkError, "network error");
    }

    // Set when the data container of the Body is a BodyStreamBuffer.
    Member<BodyStreamBuffer> m_bodyStreamBuffer;
    // Set when the data container of the Body is a BlobDataHandle.
    RefPtr<BlobDataHandle> m_blobDataHandle;
    // Used to read the data from BlobDataHandle.
    OwnPtr<FileReaderLoader> m_loader;
    // Created when createDrainingStream is called to drain the data.
    Member<BodyStreamBuffer> m_drainingStreamBuffer;
    Member<ReadableByteStream> m_stream;
    State m_state;
};

ScriptPromise Body::readAsync(ScriptState* scriptState, ResponseType type)
{
    if (bodyUsed())
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Already read"));

    // When the main thread sends a V8::TerminateExecution() signal to a worker
    // thread, any V8 API on the worker thread starts returning an empty
    // handle. This can happen in Body::readAsync. To avoid the situation, we
    // first check the ExecutionContext and return immediately if it's already
    // gone (which means that the V8::TerminateExecution() signal has been sent
    // to this worker thread).
    ExecutionContext* executionContext = scriptState->executionContext();
    if (!executionContext)
        return ScriptPromise();

    lockBody();
    m_responseType = type;

    ASSERT(!m_resolver);
    m_resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = m_resolver->promise();

    if (m_stream->stateInternal() == ReadableStream::Closed) {
        // We resolve the resolver manually in order not to use member
        // variables.
        switch (m_responseType) {
        case ResponseAsArrayBuffer:
            m_resolver->resolve(DOMArrayBuffer::create(nullptr, 0));
            break;
        case ResponseAsBlob: {
            OwnPtr<BlobData> blobData = BlobData::create();
            blobData->setContentType(mimeType());
            m_resolver->resolve(Blob::create(BlobDataHandle::create(blobData.release(), 0)));
            break;
        }
        case ResponseAsText:
            m_resolver->resolve(String());
            break;
        case ResponseAsFormData:
            // TODO(yhirano): Implement this.
            ASSERT_NOT_REACHED();
            break;
        case ResponseAsJSON: {
            ScriptState::Scope scope(m_resolver->scriptState());
            m_resolver->reject(V8ThrowException::createSyntaxError(m_resolver->scriptState()->isolate(), "Unexpected end of input"));
            break;
        }
        case ResponseUnknown:
            ASSERT_NOT_REACHED();
            break;
        }
        m_resolver.clear();
    } else if (m_stream->stateInternal() == ReadableStream::Errored) {
        m_resolver->reject(m_stream->storedException());
        m_resolver.clear();
    } else if (isBodyConsumed()) {
        m_streamSource->createDrainingStream()->readAllAndCreateBlobHandle(mimeType(), new BlobHandleReceiver(this));
    } else if (buffer()) {
        buffer()->readAllAndCreateBlobHandle(mimeType(), new BlobHandleReceiver(this));
    } else {
        readAsyncFromBlob(blobDataHandle());
    }
    return promise;
}

void Body::readAsyncFromBlob(PassRefPtr<BlobDataHandle> handle)
{
    FileReaderLoader::ReadType readType = FileReaderLoader::ReadAsText;
    RefPtr<BlobDataHandle> blobHandle = handle;
    if (!blobHandle)
        blobHandle = BlobDataHandle::create(BlobData::create(), 0);
    switch (m_responseType) {
    case ResponseAsArrayBuffer:
        readType = FileReaderLoader::ReadAsArrayBuffer;
        break;
    case ResponseAsBlob:
        if (blobHandle->size() != kuint64max) {
            // If the size of |blobHandle| is set correctly, creates Blob from
            // it.
            if (blobHandle->type() != mimeType()) {
                // A new BlobDataHandle is created to override the Blob's type.
                m_resolver->resolve(Blob::create(BlobDataHandle::create(blobHandle->uuid(), mimeType(), blobHandle->size())));
            } else {
                m_resolver->resolve(Blob::create(blobHandle));
            }
            m_stream->close();
            m_resolver.clear();
            return;
        }
        // If the size is not set, read as ArrayBuffer and create a new blob to
        // get the size.
        // FIXME: This workaround is not good for performance.
        // When we will stop using Blob as a base system of Body to support
        // stream, this problem should be solved.
        readType = FileReaderLoader::ReadAsArrayBuffer;
        break;
    case ResponseAsFormData:
        // FIXME: Implement this.
        ASSERT_NOT_REACHED();
        break;
    case ResponseAsJSON:
    case ResponseAsText:
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    m_loader = adoptPtr(new FileReaderLoader(readType, this));
    m_loader->start(m_resolver->scriptState()->executionContext(), blobHandle);

    return;
}

ScriptPromise Body::arrayBuffer(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsArrayBuffer);
}

ScriptPromise Body::blob(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsBlob);
}

ScriptPromise Body::formData(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsFormData);
}

ScriptPromise Body::json(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsJSON);
}

ScriptPromise Body::text(ScriptState* scriptState)
{
    return readAsync(scriptState, ResponseAsText);
}

ReadableByteStream* Body::body()
{
    UseCounter::count(executionContext(), UseCounter::FetchBodyStream);
    return m_stream;
}

bool Body::bodyUsed() const
{
    return m_bodyUsed || m_stream->isLocked();
}

void Body::lockBody(LockBodyOption option)
{
    ASSERT(!bodyUsed());
    if (option == PassBody)
        m_bodyUsed = true;
    ASSERT(!m_stream->isLocked());
    TrackExceptionState exceptionState;
    m_stream->getBytesReader(executionContext(), exceptionState);
    ASSERT(!exceptionState.hadException());
}

bool Body::isBodyConsumed() const
{
    if (m_streamSource->state() != m_streamSource->Initial) {
        // Some data is pulled from the source.
        return true;
    }
    if (m_stream->stateInternal() == ReadableStream::Closed) {
        // Return true if the blob handle is originally not empty.
        RefPtr<BlobDataHandle> handle = blobDataHandle();
        return handle && handle->size();
    }
    if (m_stream->stateInternal() == ReadableStream::Errored) {
        // The stream is errored. That means an effort to read data was made.
        return true;
    }
    return false;
}

void Body::setBody(ReadableStreamSource* source)
{
    m_streamSource = source;
    m_stream = new ReadableByteStream(m_streamSource, new ReadableByteStream::StrictStrategy);
    m_streamSource->startStream(m_stream);
}

BodyStreamBuffer* Body::createDrainingStream()
{
    return m_streamSource->createDrainingStream();
}

void Body::stop()
{
    // Canceling the load will call didFail which will remove the resolver.
    if (m_loader)
        m_loader->cancel();
}

bool Body::hasPendingActivity() const
{
    if (executionContext()->activeDOMObjectsAreStopped())
        return false;
    if (m_resolver)
        return true;
    if (m_stream->isLocked())
        return true;
    return false;
}

Body::ReadableStreamSource* Body::createBodySource(PassRefPtr<BlobDataHandle> handle)
{
    return new ReadableStreamSource(executionContext(), handle);
}

Body::ReadableStreamSource* Body::createBodySource(BodyStreamBuffer* buffer)
{
    return new ReadableStreamSource(executionContext(), buffer);
}

DEFINE_TRACE(Body)
{
    visitor->trace(m_resolver);
    visitor->trace(m_stream);
    visitor->trace(m_streamSource);
    ActiveDOMObject::trace(visitor);
}

Body::Body(ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_bodyUsed(false)
    , m_responseType(ResponseType::ResponseUnknown)
    , m_streamSource(new ReadableStreamSource(context))
    , m_stream(new ReadableByteStream(m_streamSource, new ReadableByteStream::StrictStrategy))
{
    m_streamSource->startStream(m_stream);
}

void Body::resolveJSON(const String& string)
{
    ASSERT(m_responseType == ResponseAsJSON);
    ScriptState::Scope scope(m_resolver->scriptState());
    v8::Isolate* isolate = m_resolver->scriptState()->isolate();
    v8::Local<v8::String> inputString = v8String(isolate, string);
    v8::TryCatch trycatch;
    v8::Local<v8::Value> parsed;
    if (v8Call(v8::JSON::Parse(isolate, inputString), parsed, trycatch))
        m_resolver->resolve(parsed);
    else
        m_resolver->reject(trycatch.Exception());
}

// FileReaderLoaderClient functions.
void Body::didStartLoading() { }
void Body::didReceiveData() { }
void Body::didFinishLoading()
{
    if (!executionContext() || executionContext()->activeDOMObjectsAreStopped())
        return;

    switch (m_responseType) {
    case ResponseAsArrayBuffer:
        m_resolver->resolve(m_loader->arrayBufferResult());
        break;
    case ResponseAsBlob: {
        ASSERT(blobDataHandle()->size() == kuint64max);
        OwnPtr<BlobData> blobData = BlobData::create();
        RefPtr<DOMArrayBuffer> buffer = m_loader->arrayBufferResult();
        blobData->appendBytes(buffer->data(), buffer->byteLength());
        blobData->setContentType(mimeType());
        const size_t length = blobData->length();
        m_resolver->resolve(Blob::create(BlobDataHandle::create(blobData.release(), length)));
        break;
    }
    case ResponseAsFormData:
        ASSERT_NOT_REACHED();
        break;
    case ResponseAsJSON:
        resolveJSON(m_loader->stringResult());
        break;
    case ResponseAsText:
        m_resolver->resolve(m_loader->stringResult());
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    m_streamSource->close();
    m_resolver.clear();
}

void Body::didFail(FileError::ErrorCode code)
{
    if (!executionContext() || executionContext()->activeDOMObjectsAreStopped())
        return;

    m_streamSource->error();
    if (m_resolver) {
        // FIXME: We should reject the promise.
        m_resolver->resolve("");
        m_resolver.clear();
    }
}

void Body::didBlobHandleReceiveError(DOMException* exception)
{
    if (!m_resolver)
        return;
    m_streamSource->error();
    m_resolver->reject(exception);
    m_resolver.clear();
}

} // namespace blink
