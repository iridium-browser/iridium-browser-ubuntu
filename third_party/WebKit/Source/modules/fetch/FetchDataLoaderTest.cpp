// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchDataLoader.h"

#include "modules/fetch/BytesConsumerForDataConsumerHandle.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

namespace {

using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::StrictMock;
using ::testing::_;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using Checkpoint = StrictMock<::testing::MockFunction<void(int)>>;
using MockFetchDataLoaderClient = DataConsumerHandleTestUtil::MockFetchDataLoaderClient;
using MockHandle = DataConsumerHandleTestUtil::MockFetchDataConsumerHandle;
using MockReader = DataConsumerHandleTestUtil::MockFetchDataConsumerReader;

constexpr WebDataConsumerHandle::Result kOk = WebDataConsumerHandle::Ok;
constexpr WebDataConsumerHandle::Result kUnexpectedError = WebDataConsumerHandle::UnexpectedError;
constexpr WebDataConsumerHandle::Result kShouldWait = WebDataConsumerHandle::ShouldWait;
constexpr WebDataConsumerHandle::Result kDone = WebDataConsumerHandle::Done;
constexpr WebDataConsumerHandle::Flags kNone = WebDataConsumerHandle::FlagNone;
constexpr FetchDataConsumerHandle::Reader::BlobSizePolicy kDisallowBlobWithInvalidSize = FetchDataConsumerHandle::Reader::DisallowBlobWithInvalidSize;

constexpr char kQuickBrownFox[] = "Quick brown fox";
constexpr size_t kQuickBrownFoxLength = 15;
constexpr size_t kQuickBrownFoxLengthWithTerminatingNull = 16;

TEST(FetchDataLoaderTest, LoadAsBlob)
{
    WebDataConsumerHandle::Client *client = nullptr;
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle = MockHandle::create();

    // |reader| will be adopted by |obtainFetchDataReader|.
    MockReader* reader = MockReader::create().release();

    FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsBlobHandle("text/test");
    MockFetchDataLoaderClient* fetchDataLoaderClient = MockFetchDataLoaderClient::create();
    RefPtr<BlobDataHandle> blobDataHandle;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handle, obtainFetchDataReader(_)).WillOnce(DoAll(SaveArg<0>(&client), Return(ByMove(WTF::wrapUnique(reader)))));
    EXPECT_CALL(*reader, drainAsBlobDataHandle(kDisallowBlobWithInvalidSize)).WillOnce(Return(nullptr));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<2>(0), Return(kShouldWait)));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*reader, read(nullptr, kNone, 0, _)).WillOnce(DoAll(SetArgPointee<3>(0), Return(kShouldWait)));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(static_cast<const void*>(kQuickBrownFox)), SetArgPointee<2>(kQuickBrownFoxLengthWithTerminatingNull), Return(kOk)));
    EXPECT_CALL(*reader, endRead(kQuickBrownFoxLengthWithTerminatingNull)).WillOnce(Return(kOk));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(Return(kDone));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadedBlobHandleMock(_)).WillOnce(SaveArg<0>(&blobDataHandle));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(checkpoint, Call(4));

    checkpoint.Call(1);
    fetchDataLoader->start(new BytesConsumerForDataConsumerHandle(std::move(handle)), fetchDataLoaderClient);
    checkpoint.Call(2);
    ASSERT_TRUE(client);
    client->didGetReadable();
    checkpoint.Call(3);
    fetchDataLoader->cancel();
    checkpoint.Call(4);

    ASSERT_TRUE(blobDataHandle);
    EXPECT_EQ(kQuickBrownFoxLengthWithTerminatingNull, blobDataHandle->size());
    EXPECT_EQ(String("text/test"), blobDataHandle->type());
}

TEST(FetchDataLoaderTest, LoadAsBlobFailed)
{
    WebDataConsumerHandle::Client *client = nullptr;
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle = MockHandle::create();

    // |reader| is adopted by |obtainFetchDataReader|.
    MockReader* reader = MockReader::create().release();

    FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsBlobHandle("text/test");
    MockFetchDataLoaderClient* fetchDataLoaderClient = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handle, obtainFetchDataReader(_)).WillOnce(DoAll(SaveArg<0>(&client), Return(ByMove(WTF::wrapUnique(reader)))));
    EXPECT_CALL(*reader, drainAsBlobDataHandle(kDisallowBlobWithInvalidSize)).WillOnce(Return(nullptr));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<2>(0), Return(kShouldWait)));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*reader, read(nullptr, kNone, 0, _)).WillOnce(DoAll(SetArgPointee<3>(0), Return(kShouldWait)));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(static_cast<const void*>(kQuickBrownFox)), SetArgPointee<2>(kQuickBrownFoxLengthWithTerminatingNull), Return(kOk)));
    EXPECT_CALL(*reader, endRead(kQuickBrownFoxLengthWithTerminatingNull)).WillOnce(Return(kOk));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(Return(kUnexpectedError));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadFailed());
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(checkpoint, Call(4));

    checkpoint.Call(1);
    fetchDataLoader->start(new BytesConsumerForDataConsumerHandle(std::move(handle)), fetchDataLoaderClient);
    checkpoint.Call(2);
    ASSERT_TRUE(client);
    client->didGetReadable();
    checkpoint.Call(3);
    fetchDataLoader->cancel();
    checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsBlobCancel)
{
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle = MockHandle::create();

    // |reader| will be adopted by |obtainFetchDataReader|.
    MockReader* reader = MockReader::create().release();

    FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsBlobHandle("text/test");
    MockFetchDataLoaderClient* fetchDataLoaderClient = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handle, obtainFetchDataReader(_)).WillOnce(Return(ByMove(WTF::wrapUnique(reader))));
    EXPECT_CALL(*reader, drainAsBlobDataHandle(kDisallowBlobWithInvalidSize)).WillOnce(Return(nullptr));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<2>(0), Return(kShouldWait)));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(checkpoint, Call(3));

    checkpoint.Call(1);
    fetchDataLoader->start(new BytesConsumerForDataConsumerHandle(std::move(handle)), fetchDataLoaderClient);
    checkpoint.Call(2);
    fetchDataLoader->cancel();
    checkpoint.Call(3);
}

TEST(FetchDataLoaderTest, LoadAsBlobViaDrainAsBlobDataHandleWithSameContentType)
{
    std::unique_ptr<BlobData> blobData = BlobData::create();
    blobData->appendBytes(kQuickBrownFox, kQuickBrownFoxLengthWithTerminatingNull);
    blobData->setContentType("text/test");
    RefPtr<BlobDataHandle> inputBlobDataHandle = BlobDataHandle::create(std::move(blobData), kQuickBrownFoxLengthWithTerminatingNull);

    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle = MockHandle::create();

    // |reader| will be adopted by |obtainFetchDataReader|.
    MockReader* reader = MockReader::create().release();

    FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsBlobHandle("text/test");
    MockFetchDataLoaderClient* fetchDataLoaderClient = MockFetchDataLoaderClient::create();
    RefPtr<BlobDataHandle> blobDataHandle;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handle, obtainFetchDataReader(_)).WillOnce(Return(ByMove(WTF::wrapUnique(reader))));
    EXPECT_CALL(*reader, drainAsBlobDataHandle(kDisallowBlobWithInvalidSize)).WillOnce(Return(inputBlobDataHandle));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadedBlobHandleMock(_)).WillOnce(SaveArg<0>(&blobDataHandle));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(checkpoint, Call(3));

    checkpoint.Call(1);
    fetchDataLoader->start(new BytesConsumerForDataConsumerHandle(std::move(handle)), fetchDataLoaderClient);
    checkpoint.Call(2);
    fetchDataLoader->cancel();
    checkpoint.Call(3);

    ASSERT_TRUE(blobDataHandle);
    EXPECT_EQ(inputBlobDataHandle, blobDataHandle);
    EXPECT_EQ(kQuickBrownFoxLengthWithTerminatingNull, blobDataHandle->size());
    EXPECT_EQ(String("text/test"), blobDataHandle->type());
}

TEST(FetchDataLoaderTest, LoadAsBlobViaDrainAsBlobDataHandleWithDifferentContentType)
{
    std::unique_ptr<BlobData> blobData = BlobData::create();
    blobData->appendBytes(kQuickBrownFox, kQuickBrownFoxLengthWithTerminatingNull);
    blobData->setContentType("text/different");
    RefPtr<BlobDataHandle> inputBlobDataHandle = BlobDataHandle::create(std::move(blobData), kQuickBrownFoxLengthWithTerminatingNull);

    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle = MockHandle::create();

    // |reader| will be adopted by |obtainFetchDataReader|.
    MockReader* reader = MockReader::create().release();

    FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsBlobHandle("text/test");
    MockFetchDataLoaderClient* fetchDataLoaderClient = MockFetchDataLoaderClient::create();
    RefPtr<BlobDataHandle> blobDataHandle;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handle, obtainFetchDataReader(_)).WillOnce(Return(ByMove(WTF::wrapUnique(reader))));
    EXPECT_CALL(*reader, drainAsBlobDataHandle(kDisallowBlobWithInvalidSize)).WillOnce(Return(inputBlobDataHandle));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadedBlobHandleMock(_)).WillOnce(SaveArg<0>(&blobDataHandle));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(checkpoint, Call(3));

    checkpoint.Call(1);
    fetchDataLoader->start(new BytesConsumerForDataConsumerHandle(std::move(handle)), fetchDataLoaderClient);
    checkpoint.Call(2);
    fetchDataLoader->cancel();
    checkpoint.Call(3);

    ASSERT_TRUE(blobDataHandle);
    EXPECT_NE(inputBlobDataHandle, blobDataHandle);
    EXPECT_EQ(kQuickBrownFoxLengthWithTerminatingNull, blobDataHandle->size());
    EXPECT_EQ(String("text/test"), blobDataHandle->type());
}

TEST(FetchDataLoaderTest, LoadAsArrayBuffer)
{
    WebDataConsumerHandle::Client *client = nullptr;
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle = MockHandle::create();

    // |reader| will be adopted by |obtainFetchDataReader|.
    MockReader* reader = MockReader::create().release();

    FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsArrayBuffer();
    MockFetchDataLoaderClient* fetchDataLoaderClient = MockFetchDataLoaderClient::create();
    DOMArrayBuffer* arrayBuffer = nullptr;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handle, obtainFetchDataReader(_)).WillOnce(DoAll(SaveArg<0>(&client), Return(ByMove(WTF::wrapUnique(reader)))));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<2>(0), Return(kShouldWait)));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*reader, read(nullptr, kNone, 0, _)).WillOnce(DoAll(SetArgPointee<3>(0), Return(kShouldWait)));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(static_cast<const void*>(kQuickBrownFox)), SetArgPointee<2>(kQuickBrownFoxLengthWithTerminatingNull), Return(kOk)));
    EXPECT_CALL(*reader, endRead(kQuickBrownFoxLengthWithTerminatingNull)).WillOnce(Return(kOk));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(Return(kDone));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadedArrayBufferMock(_)).WillOnce(SaveArg<0>(&arrayBuffer));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(checkpoint, Call(4));

    checkpoint.Call(1);
    fetchDataLoader->start(new BytesConsumerForDataConsumerHandle(std::move(handle)), fetchDataLoaderClient);
    checkpoint.Call(2);
    ASSERT_TRUE(client);
    client->didGetReadable();
    checkpoint.Call(3);
    fetchDataLoader->cancel();
    checkpoint.Call(4);

    ASSERT_TRUE(arrayBuffer);
    ASSERT_EQ(kQuickBrownFoxLengthWithTerminatingNull, arrayBuffer->byteLength());
    EXPECT_STREQ(kQuickBrownFox, static_cast<const char*>(arrayBuffer->data()));
}

TEST(FetchDataLoaderTest, LoadAsArrayBufferFailed)
{
    WebDataConsumerHandle::Client *client = nullptr;
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle = MockHandle::create();

    // |reader| will be adopted by |obtainFetchDataReader|.
    MockReader* reader = MockReader::create().release();

    FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsArrayBuffer();
    MockFetchDataLoaderClient* fetchDataLoaderClient = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handle, obtainFetchDataReader(_)).WillOnce(DoAll(SaveArg<0>(&client), Return(ByMove(WTF::wrapUnique(reader)))));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<2>(0), Return(kShouldWait)));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*reader, read(nullptr, kNone, 0, _)).WillOnce(DoAll(SetArgPointee<3>(0), Return(kShouldWait)));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(static_cast<const void*>(kQuickBrownFox)), SetArgPointee<2>(kQuickBrownFoxLengthWithTerminatingNull), Return(kOk)));
    EXPECT_CALL(*reader, endRead(kQuickBrownFoxLengthWithTerminatingNull)).WillOnce(Return(kOk));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(Return(kUnexpectedError));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadFailed());
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(checkpoint, Call(4));

    checkpoint.Call(1);
    fetchDataLoader->start(new BytesConsumerForDataConsumerHandle(std::move(handle)), fetchDataLoaderClient);
    checkpoint.Call(2);
    ASSERT_TRUE(client);
    client->didGetReadable();
    checkpoint.Call(3);
    fetchDataLoader->cancel();
    checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsArrayBufferCancel)
{
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle = MockHandle::create();

    // |reader| will be adopted by |obtainFetchDataReader|.
    MockReader* reader = MockReader::create().release();
    FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsArrayBuffer();
    MockFetchDataLoaderClient* fetchDataLoaderClient = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handle, obtainFetchDataReader(_)).WillOnce(Return(ByMove(WTF::wrapUnique(reader))));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<2>(0), Return(kShouldWait)));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(checkpoint, Call(3));

    checkpoint.Call(1);
    fetchDataLoader->start(new BytesConsumerForDataConsumerHandle(std::move(handle)), fetchDataLoaderClient);
    checkpoint.Call(2);
    fetchDataLoader->cancel();
    checkpoint.Call(3);
}

TEST(FetchDataLoaderTest, LoadAsString)
{
    WebDataConsumerHandle::Client *client = nullptr;
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle = MockHandle::create();

    // |reader| will be adopted by |obtainFetchDataReader|.
    MockReader* reader = MockReader::create().release();

    FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsString();
    MockFetchDataLoaderClient* fetchDataLoaderClient = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handle, obtainFetchDataReader(_)).WillOnce(DoAll(SaveArg<0>(&client), Return(ByMove(WTF::wrapUnique(reader)))));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<2>(0), Return(kShouldWait)));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*reader, read(nullptr, kNone, 0, _)).WillOnce(DoAll(SetArgPointee<3>(0), Return(kShouldWait)));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(static_cast<const void*>(kQuickBrownFox)), SetArgPointee<2>(kQuickBrownFoxLength), Return(kOk)));
    EXPECT_CALL(*reader, endRead(kQuickBrownFoxLength)).WillOnce(Return(kOk));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(Return(kDone));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadedString(String(kQuickBrownFox)));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(checkpoint, Call(4));

    checkpoint.Call(1);
    fetchDataLoader->start(new BytesConsumerForDataConsumerHandle(std::move(handle)), fetchDataLoaderClient);
    checkpoint.Call(2);
    ASSERT_TRUE(client);
    client->didGetReadable();
    checkpoint.Call(3);
    fetchDataLoader->cancel();
    checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsStringWithNullBytes)
{
    WebDataConsumerHandle::Client *client = nullptr;
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle = MockHandle::create();

    // |reader| will be adopted by |obtainFetchDataReader|.
    MockReader* reader = MockReader::create().release();

    FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsString();
    MockFetchDataLoaderClient* fetchDataLoaderClient = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handle, obtainFetchDataReader(_)).WillOnce(DoAll(SaveArg<0>(&client), Return(ByMove(WTF::wrapUnique(reader)))));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<2>(0), Return(kShouldWait)));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*reader, read(nullptr, kNone, 0, _)).WillOnce(DoAll(SetArgPointee<3>(0), Return(kShouldWait)));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(static_cast<const void*>("Quick\0brown\0fox")), SetArgPointee<2>(16), Return(kOk)));
    EXPECT_CALL(*reader, endRead(kQuickBrownFoxLengthWithTerminatingNull)).WillOnce(Return(kOk));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(Return(kDone));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadedString(String("Quick\0brown\0fox", 16)));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(checkpoint, Call(4));

    checkpoint.Call(1);
    fetchDataLoader->start(new BytesConsumerForDataConsumerHandle(std::move(handle)), fetchDataLoaderClient);
    checkpoint.Call(2);
    ASSERT_TRUE(client);
    client->didGetReadable();
    checkpoint.Call(3);
    fetchDataLoader->cancel();
    checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsStringError)
{
    WebDataConsumerHandle::Client *client = nullptr;
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle = MockHandle::create();

    // |reader| will be adopted by |obtainFetchDataReader|.
    MockReader* reader = MockReader::create().release();

    FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsString();
    MockFetchDataLoaderClient* fetchDataLoaderClient = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handle, obtainFetchDataReader(_)).WillOnce(DoAll(SaveArg<0>(&client), Return(ByMove(WTF::wrapUnique(reader)))));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<2>(0), Return(kShouldWait)));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*reader, read(nullptr, kNone, 0, _)).WillOnce(DoAll(SetArgPointee<3>(0), Return(kShouldWait)));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(static_cast<const void*>(kQuickBrownFox)), SetArgPointee<2>(kQuickBrownFoxLength), Return(kOk)));
    EXPECT_CALL(*reader, endRead(kQuickBrownFoxLength)).WillOnce(Return(kOk));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(Return(kUnexpectedError));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(*fetchDataLoaderClient, didFetchDataLoadFailed());
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(checkpoint, Call(4));

    checkpoint.Call(1);
    fetchDataLoader->start(new BytesConsumerForDataConsumerHandle(std::move(handle)), fetchDataLoaderClient);
    checkpoint.Call(2);
    ASSERT_TRUE(client);
    client->didGetReadable();
    checkpoint.Call(3);
    fetchDataLoader->cancel();
    checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsStringCancel)
{
    Checkpoint checkpoint;

    std::unique_ptr<MockHandle> handle = MockHandle::create();

    // |reader| will be adopted by |obtainFetchDataReader|.
    MockReader* reader = MockReader::create().release();

    FetchDataLoader* fetchDataLoader = FetchDataLoader::createLoaderAsString();
    MockFetchDataLoaderClient* fetchDataLoaderClient = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handle, obtainFetchDataReader(_)).WillOnce(Return(ByMove(WTF::wrapUnique(reader))));
    EXPECT_CALL(*reader, beginRead(_, kNone, _)).WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<2>(0), Return(kShouldWait)));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(checkpoint, Call(3));

    checkpoint.Call(1);
    fetchDataLoader->start(new BytesConsumerForDataConsumerHandle(std::move(handle)), fetchDataLoaderClient);
    checkpoint.Call(2);
    fetchDataLoader->cancel();
    checkpoint.Call(3);
}

} // namespace

} // namespace blink
