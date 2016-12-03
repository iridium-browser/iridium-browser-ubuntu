// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BytesConsumerForDataConsumerHandle_h
#define BytesConsumerForDataConsumerHandle_h

#include "modules/ModulesExport.h"
#include "modules/fetch/BytesConsumer.h"
#include "modules/fetch/FetchFormDataConsumerHandle.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

#include <memory>

namespace blink {

class MODULES_EXPORT BytesConsumerForDataConsumerHandle final : public BytesConsumer, public WebDataConsumerHandle::Client {
    EAGERLY_FINALIZE();
    DECLARE_EAGER_FINALIZATION_OPERATOR_NEW();
public:
    explicit BytesConsumerForDataConsumerHandle(std::unique_ptr<FetchDataConsumerHandle>);
    ~BytesConsumerForDataConsumerHandle() override;

    Result read(char* buffer, size_t /* size */, size_t* readSize) override;
    Result beginRead(const char** buffer, size_t* available) override;
    PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(BlobSizePolicy) override;
    PassRefPtr<EncodedFormData> drainAsFormData() override;
    Result endRead(size_t readSize) override;
    void setClient(BytesConsumer::Client*) override;
    void clearClient() override;

    void cancel() override;
    PublicState getPublicState() const override;
    Error getError() const override
    {
        DCHECK(m_state == InternalState::Errored);
        return m_error;
    }
    String debugName() const override { return "BytesConsumerForDataConsumerHandle"; }

    // WebDataConsumerHandle::Client
    void didGetReadable() override;

    DECLARE_TRACE();

private:
    void close();
    void error();

    std::unique_ptr<FetchDataConsumerHandle::Reader> m_reader;
    Member<BytesConsumer::Client> m_client;
    InternalState m_state = InternalState::Waiting;
    Error m_error;
};

} // namespace blink

#endif // BytesConsumerForDataConsumerHandle_h
