// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/FetchFormDataConsumerHandle.h"

#include "modules/fetch/DataConsumerHandleUtil.h"
#include "modules/fetch/FetchBlobDataConsumerHandle.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"
#include "wtf/text/TextCodec.h"
#include "wtf/text/TextEncoding.h"
#include "wtf/text/WTFString.h"

#include <utility>

namespace blink {

using Result = FetchDataConsumerHandle::Result;

namespace {

bool isSimple(const FormData* formData)
{
    for (const auto& element : formData->elements()) {
        if (element.m_type != FormDataElement::data)
            return false;
    }
    return true;
}

} // namespace

class FetchFormDataConsumerHandle::Context : public ThreadSafeRefCounted<Context> {
    WTF_MAKE_NONCOPYABLE(Context);
public:
    virtual ~Context() {}
    virtual PassOwnPtr<FetchDataConsumerHandle::Reader> obtainReader(Client*) = 0;

protected:
    explicit Context() {}
};

class FetchFormDataConsumerHandle::SimpleContext final : public Context {
    class ReaderImpl;
public:
    static PassRefPtr<SimpleContext> create(const String& body) { return adoptRef(new SimpleContext(body)); }
    static PassRefPtr<SimpleContext> create(const void* data, size_t size) { return adoptRef(new SimpleContext(data, size)); }
    static PassRefPtr<SimpleContext> create(PassRefPtr<FormData> body) { return adoptRef(new SimpleContext(body)); }

    PassOwnPtr<Reader> obtainReader(Client* client) override
    {
        // For memory barrier.
        Mutex m;
        MutexLocker locker(m);
        return ReaderImpl::create(this, client);
    }

    PassRefPtr<FormData> drainFormData()
    {
        ASSERT(!m_formData || m_formData->isSafeToSendToAnotherThread());
        return m_formData.release();
    }

    Result read(void* data, size_t size, Flags flags, size_t* readSize)
    {
        *readSize = 0;
        if (size == 0 && m_formData)
            return WebDataConsumerHandle::Ok;

        flatten();
        RELEASE_ASSERT(m_flattenFormDataOffset <= m_flattenFormData.size());

        *readSize = std::min(size, m_flattenFormData.size() - m_flattenFormDataOffset);
        if (*readSize == 0)
            return WebDataConsumerHandle::Done;
        memcpy(data, &m_flattenFormData[m_flattenFormDataOffset], *readSize);
        m_flattenFormDataOffset += *readSize;
        RELEASE_ASSERT(m_flattenFormDataOffset <= m_flattenFormData.size());

        return WebDataConsumerHandle::Ok;
    }

    Result beginRead(const void** buffer, Flags flags, size_t* available)
    {
        *buffer = nullptr;
        *available = 0;

        flatten();
        RELEASE_ASSERT(m_flattenFormDataOffset <= m_flattenFormData.size());

        if (m_flattenFormData.size() == m_flattenFormDataOffset)
            return WebDataConsumerHandle::Done;
        *buffer = &m_flattenFormData[m_flattenFormDataOffset];
        *available = m_flattenFormData.size() - m_flattenFormDataOffset;
        return WebDataConsumerHandle::Ok;
    }

    Result endRead(size_t read)
    {
        RELEASE_ASSERT(m_flattenFormDataOffset <= m_flattenFormData.size());
        RELEASE_ASSERT(read <= m_flattenFormData.size() - m_flattenFormDataOffset);
        m_flattenFormDataOffset += read;

        return WebDataConsumerHandle::Ok;
    }

private:
    class ReaderImpl final : public FetchDataConsumerHandle::Reader {
        WTF_MAKE_NONCOPYABLE(ReaderImpl);
    public:
        static PassOwnPtr<ReaderImpl> create(PassRefPtr<SimpleContext> context, Client* client) { return adoptPtr(new ReaderImpl(context, client)); }
        Result read(void* data, size_t size, Flags flags, size_t* readSize) override
        {
            return m_context->read(data, size, flags, readSize);
        }
        Result beginRead(const void** buffer, Flags flags, size_t* available) override
        {
            return m_context->beginRead(buffer, flags, available);
        }
        Result endRead(size_t read) override
        {
            return m_context->endRead(read);
        }
        PassRefPtr<FormData> drainAsFormData() override
        {
            return m_context->drainFormData();
        }

    private:
        ReaderImpl(PassRefPtr<SimpleContext> context, Client* client) : m_context(context), m_notifier(client) {}

        RefPtr<SimpleContext> m_context;
        NotifyOnReaderCreationHelper m_notifier;
    };

    explicit SimpleContext(const String& body)
        : m_formData(FormData::create(UTF8Encoding().encode(body, WTF::EntitiesForUnencodables)))
        , m_flattenFormDataOffset(0) {}
    explicit SimpleContext(const void* data, size_t size) : m_formData(FormData::create(data, size)) , m_flattenFormDataOffset(0) {}
    explicit SimpleContext(PassRefPtr<FormData> body) : m_formData(body->deepCopy()) , m_flattenFormDataOffset(0) {}

    void flatten()
    {
        if (!m_formData) {
            // It is already drained or flatten.
            return;
        }
        ASSERT(m_formData->isSafeToSendToAnotherThread());
        m_formData->flatten(m_flattenFormData);
        m_formData = nullptr;
    }

    // either one of |m_formData| and |m_flattenFormData| is usable at a time.
    RefPtr<FormData> m_formData;
    Vector<char> m_flattenFormData;
    size_t m_flattenFormDataOffset;
};

class FetchFormDataConsumerHandle::ComplexContext final : public Context {
    class ReaderImpl;
public:
    static PassRefPtr<ComplexContext> create(ExecutionContext* executionContext,
        PassRefPtr<FormData> formData,
        FetchBlobDataConsumerHandle::LoaderFactory* factory)
    {
        return adoptRef(new ComplexContext(executionContext, formData, factory));
    }

    PassOwnPtr<FetchFormDataConsumerHandle::Reader> obtainReader(Client* client) override
    {
        // For memory barrier.
        Mutex m;
        MutexLocker locker(m);
        return ReaderImpl::create(this, client);
    }

private:
    class ReaderImpl final : public FetchDataConsumerHandle::Reader {
        WTF_MAKE_NONCOPYABLE(ReaderImpl);
    public:
        static PassOwnPtr<ReaderImpl> create(PassRefPtr<ComplexContext> context, Client* client) { return adoptPtr(new ReaderImpl(context, client)); }
        Result read(void* data, size_t size, Flags flags, size_t* readSize) override
        {
            Result r = m_reader->read(data, size, flags, readSize);
            if (!(size == 0 && (r == Ok || r == ShouldWait))) {
                m_context->drainFormData();
            }
            return r;
        }
        Result beginRead(const void** buffer, Flags flags, size_t* available) override
        {
            m_context->drainFormData();
            return m_reader->beginRead(buffer, flags, available);
        }
        Result endRead(size_t read) override
        {
            return m_reader->endRead(read);
        }
        PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(BlobSizePolicy policy) override
        {
            RefPtr<BlobDataHandle> handle = m_reader->drainAsBlobDataHandle(policy);
            if (handle) {
                m_context->drainFormData();
            }
            return handle.release();
        }
        PassRefPtr<FormData> drainAsFormData() override
        {
            RefPtr<FormData> formData = m_context->drainFormData();
            if (formData) {
                // Drain blob from the underlying handle to mark data as read.
                RefPtr<BlobDataHandle> handle = m_reader->drainAsBlobDataHandle(AllowBlobWithInvalidSize);
                // Here we assume we can always get the valid handle. That is
                // in fact not specified at FetchDataConsumerHandle level, but
                // |m_context->m_handle| is a FetchBlobDataConsumerHandle.
                ASSERT(handle);
            }
            return formData.release();
        }
    private:
        ReaderImpl(PassRefPtr<ComplexContext> context, Client* client) : m_context(context), m_reader(m_context->m_handle->obtainReader(client)) {}

        RefPtr<ComplexContext> m_context;
        OwnPtr<FetchDataConsumerHandle::Reader> m_reader;
    };

    explicit ComplexContext(ExecutionContext* executionContext, PassRefPtr<FormData> body, FetchBlobDataConsumerHandle::LoaderFactory* factory)
    {
        OwnPtr<BlobData> blobData = BlobData::create();
        for (const auto& element : body->elements()) {
            switch (element.m_type) {
            case FormDataElement::data:
                blobData->appendBytes(element.m_data.data(), element.m_data.size());
                break;
            case FormDataElement::encodedFile:
                blobData->appendFile(element.m_filename, element.m_fileStart, element.m_fileLength, element.m_expectedFileModificationTime);
                break;
            case FormDataElement::encodedBlob:
                if (element.m_optionalBlobDataHandle)
                    blobData->appendBlob(element.m_optionalBlobDataHandle, 0, element.m_optionalBlobDataHandle->size());
                break;
            case FormDataElement::encodedFileSystemURL:
                blobData->appendFileSystemURL(element.m_fileSystemURL, element.m_fileStart, element.m_fileLength, element.m_expectedFileModificationTime);
                break;
            }
        }
        // Here we handle body->boundary() as a C-style string. See
        // FormDataBuilder::generateUniqueBoundaryString.
        blobData->setContentType(AtomicString("multipart/form-data; boundary=", AtomicString::ConstructFromLiteral) + body->boundary().data());
        auto size = blobData->length();
        if (factory) {
            // For testing
            m_handle = FetchBlobDataConsumerHandle::create(executionContext, BlobDataHandle::create(blobData.release(), size), factory);
        } else {
            m_handle = FetchBlobDataConsumerHandle::create(executionContext, BlobDataHandle::create(blobData.release(), size));
        }
        // It is important to initialize |m_formData| here, because even
        // read-only operations may make the form data unsharable with implicit
        // ref-counting.
        m_formData = body->deepCopy();
    }
    PassRefPtr<FormData> drainFormData()
    {
        ASSERT(!m_formData || m_formData->isSafeToSendToAnotherThread());
        return m_formData.release();
    }

    RefPtr<FormData> m_formData;
    OwnPtr<FetchDataConsumerHandle> m_handle;
};

PassOwnPtr<FetchDataConsumerHandle> FetchFormDataConsumerHandle::create(const String& body)
{
    return adoptPtr(new FetchFormDataConsumerHandle(body));
}
PassOwnPtr<FetchDataConsumerHandle> FetchFormDataConsumerHandle::create(PassRefPtr<DOMArrayBuffer> body)
{
    return adoptPtr(new FetchFormDataConsumerHandle(body->data(), body->byteLength()));
}
PassOwnPtr<FetchDataConsumerHandle> FetchFormDataConsumerHandle::create(PassRefPtr<DOMArrayBufferView> body)
{
    return adoptPtr(new FetchFormDataConsumerHandle(body->baseAddress(), body->byteLength()));
}
PassOwnPtr<FetchDataConsumerHandle> FetchFormDataConsumerHandle::create(const void* data, size_t size)
{
    return adoptPtr(new FetchFormDataConsumerHandle(data, size));
}
PassOwnPtr<FetchDataConsumerHandle> FetchFormDataConsumerHandle::create(ExecutionContext* executionContext, PassRefPtr<FormData> body)
{
    return adoptPtr(new FetchFormDataConsumerHandle(executionContext, body));
}
PassOwnPtr<FetchDataConsumerHandle> FetchFormDataConsumerHandle::createForTest(
    ExecutionContext* executionContext,
    PassRefPtr<FormData> body,
    FetchBlobDataConsumerHandle::LoaderFactory* loaderFactory)
{
    return adoptPtr(new FetchFormDataConsumerHandle(executionContext, body, loaderFactory));
}

FetchFormDataConsumerHandle::FetchFormDataConsumerHandle(const String& body) : m_context(SimpleContext::create(body)) {}
FetchFormDataConsumerHandle::FetchFormDataConsumerHandle(const void* data, size_t size) : m_context(SimpleContext::create(data, size)) {}
FetchFormDataConsumerHandle::FetchFormDataConsumerHandle(ExecutionContext* executionContext,
    PassRefPtr<FormData> body,
    FetchBlobDataConsumerHandle::LoaderFactory* loaderFactory)
{
    if (isSimple(body.get())) {
        m_context = SimpleContext::create(body);
    } else {
        m_context = ComplexContext::create(executionContext, body, loaderFactory);
    }
}
FetchFormDataConsumerHandle::~FetchFormDataConsumerHandle() {}

FetchDataConsumerHandle::Reader* FetchFormDataConsumerHandle::obtainReaderInternal(Client* client)
{
    return m_context->obtainReader(client).leakPtr();
}

} // namespace blink
