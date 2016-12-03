// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BytesConsumerForDataConsumerHandle.h"

#include "modules/fetch/BytesConsumer.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using Command = DataConsumerHandleTestUtil::Command;
using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;
using ReplayingHandle = DataConsumerHandleTestUtil::ReplayingHandle;
using Result = BytesConsumer::Result;
using ::testing::InSequence;
using ::testing::Return;

class BytesConsumerForDataConsumerHandleTest : public ::testing::Test {
protected:
    ~BytesConsumerForDataConsumerHandleTest()
    {
        ThreadHeap::collectAllGarbage();
    }
};

class MockClient : public GarbageCollectedFinalized<MockClient>, public BytesConsumer::Client {
    USING_GARBAGE_COLLECTED_MIXIN(MockClient);
public:
    static MockClient* create() { return new ::testing::StrictMock<MockClient>(); }
    MOCK_METHOD0(onStateChange, void());

protected:
    MockClient() {}
};

class MockDataConsumerHandle final : public FetchDataConsumerHandle {
public:
    class MockReaderProxy : public GarbageCollectedFinalized<MockReaderProxy> {
    public:
        MOCK_METHOD4(read, WebDataConsumerHandle::Result(void*, size_t, WebDataConsumerHandle::Flags, size_t*));
        MOCK_METHOD3(beginRead, WebDataConsumerHandle::Result(const void**, WebDataConsumerHandle::Flags, size_t*));
        MOCK_METHOD1(endRead, WebDataConsumerHandle::Result(size_t));
        MOCK_METHOD1(drainAsBlobDataHandle, PassRefPtr<BlobDataHandle>(FetchDataConsumerHandle::Reader::BlobSizePolicy));
        MOCK_METHOD0(drainAsFormData, PassRefPtr<EncodedFormData>());

        DEFINE_INLINE_TRACE() {}
    };

    MockDataConsumerHandle() : m_proxy(new MockReaderProxy) {}
    MockReaderProxy* proxy() { return m_proxy; }
    const char* debugName() const { return "MockDataConsumerHandle"; }

private:
    class Reader final : public FetchDataConsumerHandle::Reader {
    public:
        explicit Reader(MockReaderProxy* proxy) : m_proxy(proxy) {}
        Result read(void* buffer, size_t size, WebDataConsumerHandle::Flags flags, size_t* read) override
        {
            return m_proxy->read(buffer, size, flags, read);
        }
        Result beginRead(const void** buffer, Flags flags, size_t* available) override
        {
            return m_proxy->beginRead(buffer, flags, available);
        }
        Result endRead(size_t readSize) override
        {
            return m_proxy->endRead(readSize);
        }
        PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(BlobSizePolicy policy) override
        {
            return m_proxy->drainAsBlobDataHandle(policy);
        }
        PassRefPtr<EncodedFormData> drainAsFormData() override
        {
            return m_proxy->drainAsFormData();
        }

    private:
        Persistent<MockReaderProxy> m_proxy;
    };

    std::unique_ptr<FetchDataConsumerHandle::Reader> obtainFetchDataReader(Client*) override { return wrapUnique(new Reader(m_proxy)); }
    Persistent<MockReaderProxy> m_proxy;
};

} // namespace

TEST_F(BytesConsumerForDataConsumerHandleTest, Create)
{
    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Data, "hello"));
    handle->add(Command(Command::Done));
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
}

TEST_F(BytesConsumerForDataConsumerHandleTest, BecomeReadable)
{
    Checkpoint checkpoint;
    Persistent<MockClient> client = MockClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, onStateChange());
    EXPECT_CALL(checkpoint, Call(2));

    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Data, "hello"));
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(client);
    EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting, consumer->getPublicState());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting, consumer->getPublicState());
}

TEST_F(BytesConsumerForDataConsumerHandleTest, BecomeClosed)
{
    Checkpoint checkpoint;
    Persistent<MockClient> client = MockClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, onStateChange());
    EXPECT_CALL(checkpoint, Call(2));

    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Done));
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(client);
    EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting, consumer->getPublicState());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

TEST_F(BytesConsumerForDataConsumerHandleTest, BecomeErrored)
{
    Checkpoint checkpoint;
    Persistent<MockClient> client = MockClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, onStateChange());
    EXPECT_CALL(checkpoint, Call(2));

    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Error));
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(client);
    EXPECT_EQ(BytesConsumer::PublicState::ReadableOrWaiting, consumer->getPublicState());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    EXPECT_EQ(BytesConsumer::PublicState::Errored, consumer->getPublicState());
}

TEST_F(BytesConsumerForDataConsumerHandleTest, ClearClient)
{
    Checkpoint checkpoint;
    Persistent<MockClient> client = MockClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));

    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Error));
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(client);
    consumer->clearClient();

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
}

TEST_F(BytesConsumerForDataConsumerHandleTest, ReadWhenReadable)
{
    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Data, "hello"));
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(MockClient::create());

    char buffer[16];
    size_t read;
    Result r = consumer->read(buffer, sizeof(buffer), &read);
    ASSERT_EQ(Result::Ok, r);
    EXPECT_EQ("hello", String(buffer, read));
}

TEST_F(BytesConsumerForDataConsumerHandleTest, ReadWhenWaiting)
{
    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(MockClient::create());

    char buffer[16];
    size_t read = 42;
    Result r = consumer->read(buffer, sizeof(buffer), &read);
    ASSERT_EQ(Result::ShouldWait, r);
    EXPECT_EQ(0u, read);
}

TEST_F(BytesConsumerForDataConsumerHandleTest, ReadWhenClosed)
{
    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Done));
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(MockClient::create());

    char buffer[16];
    size_t read = 42;
    Result r = consumer->read(buffer, sizeof(buffer), &read);
    ASSERT_EQ(Result::Done, r);
    EXPECT_EQ(0u, read);
}

TEST_F(BytesConsumerForDataConsumerHandleTest, ReadWhenErrored)
{
    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Error));
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(MockClient::create());

    char buffer[16];
    size_t read;
    Result r = consumer->read(buffer, sizeof(buffer), &read);
    ASSERT_EQ(Result::Error, r);
    EXPECT_EQ(BytesConsumer::Error("error"), consumer->getError());
}

TEST_F(BytesConsumerForDataConsumerHandleTest, TwoPhaseReadWhenReadable)
{
    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Data, "hello"));
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(MockClient::create());

    const char* buffer = nullptr;
    size_t available = 0;
    ASSERT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
    EXPECT_EQ("hello", String(buffer, available));

    ASSERT_EQ(Result::Ok, consumer->endRead(1));
    ASSERT_EQ(Result::Ok, consumer->beginRead(&buffer, &available));
    EXPECT_EQ("ello", String(buffer, available));

    ASSERT_EQ(Result::Ok, consumer->endRead(4));
    ASSERT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
}

TEST_F(BytesConsumerForDataConsumerHandleTest, TwoPhaseReadWhenWaiting)
{
    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(MockClient::create());
    const char* buffer = nullptr;
    size_t available = 0;
    ASSERT_EQ(Result::ShouldWait, consumer->beginRead(&buffer, &available));
}

TEST_F(BytesConsumerForDataConsumerHandleTest, TwoPhaseReadWhenClosed)
{
    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Done));
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(MockClient::create());
    const char* buffer = nullptr;
    size_t available = 0;
    ASSERT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
}

TEST_F(BytesConsumerForDataConsumerHandleTest, TwoPhaseReadWhenErrored)
{
    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Error));
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(MockClient::create());
    const char* buffer = nullptr;
    size_t available = 0;
    ASSERT_EQ(Result::Error, consumer->beginRead(&buffer, &available));
    EXPECT_EQ(BytesConsumer::Error("error"), consumer->getError());
}

TEST_F(BytesConsumerForDataConsumerHandleTest, Cancel)
{
    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    consumer->setClient(MockClient::create());
    consumer->cancel();
    const char* buffer = nullptr;
    size_t available = 0;
    ASSERT_EQ(Result::Done, consumer->beginRead(&buffer, &available));
}

TEST_F(BytesConsumerForDataConsumerHandleTest, drainAsBlobDataHandle)
{
    // FetchDataConsumerHandle::Reader::drainAsBlobDataHandle should return
    // nullptr from the second time, but we don't care that here.
    std::unique_ptr<MockDataConsumerHandle> handle = wrapUnique(new MockDataConsumerHandle);
    Persistent<MockDataConsumerHandle::MockReaderProxy> proxy = handle->proxy();
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(std::move(handle));
    consumer->setClient(MockClient::create());
    RefPtr<BlobDataHandle> blobDataHandle = BlobDataHandle::create();

    Checkpoint checkpoint;
    InSequence s;
    EXPECT_CALL(*proxy, drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::DisallowBlobWithInvalidSize)).WillOnce(Return(nullptr));
    EXPECT_CALL(*proxy, drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::AllowBlobWithInvalidSize)).WillOnce(Return(nullptr));
    EXPECT_CALL(*proxy, drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::AllowBlobWithInvalidSize)).WillOnce(Return(blobDataHandle));

    EXPECT_FALSE(consumer->drainAsBlobDataHandle(BytesConsumer::BlobSizePolicy::DisallowBlobWithInvalidSize));
    EXPECT_FALSE(consumer->drainAsBlobDataHandle(BytesConsumer::BlobSizePolicy::AllowBlobWithInvalidSize));
    EXPECT_TRUE(blobDataHandle);
    EXPECT_EQ(blobDataHandle, consumer->drainAsBlobDataHandle(BytesConsumer::BlobSizePolicy::AllowBlobWithInvalidSize));
    EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

TEST_F(BytesConsumerForDataConsumerHandleTest, drainAsFormData)
{
    std::unique_ptr<MockDataConsumerHandle> handle = wrapUnique(new MockDataConsumerHandle);
    Persistent<MockDataConsumerHandle::MockReaderProxy> proxy = handle->proxy();
    Persistent<BytesConsumer> consumer = new BytesConsumerForDataConsumerHandle(std::move(handle));
    consumer->setClient(MockClient::create());
    RefPtr<EncodedFormData> formData = EncodedFormData::create();

    Checkpoint checkpoint;
    InSequence s;
    EXPECT_CALL(*proxy, drainAsFormData()).WillOnce(Return(formData));

    EXPECT_EQ(formData, consumer->drainAsFormData());
    EXPECT_EQ(BytesConsumer::PublicState::Closed, consumer->getPublicState());
}

} // namespace blink
