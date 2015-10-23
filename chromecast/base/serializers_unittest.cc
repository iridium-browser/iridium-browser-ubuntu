// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/values.h"
#include "chromecast/base/serializers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {
const char kEmptyJsonString[] = "{}";
const char kEmptyJsonFileString[] = "{\n\n}\n";
const char kProperJsonString[] =
    "{\n"
    "   \"compound\": {\n"
    "      \"a\": 1,\n"
    "      \"b\": 2\n"
    "   },\n"
    "   \"some_String\": \"1337\",\n"
    "   \"some_int\": 42,\n"
    "   \"the_list\": [ \"val1\", \"val2\" ]\n"
    "}\n";
const char kPoorlyFormedJsonString[] = "{\"key\":";
const char kTestKey[] = "test_key";
const char kTestValue[] = "test_value";
const char kTempfileName[] = "temp";

}  // namespace

TEST(DeserializeFromJson, EmptyString) {
  std::string str;
  scoped_ptr<base::Value> value = DeserializeFromJson(str);
  EXPECT_EQ(nullptr, value.get());
}

TEST(DeserializeFromJson, EmptyJsonObject) {
  std::string str = kEmptyJsonString;
  scoped_ptr<base::Value> value = DeserializeFromJson(str);
  EXPECT_NE(nullptr, value.get());
}

TEST(DeserializeFromJson, ProperJsonObject) {
  std::string str = kProperJsonString;
  scoped_ptr<base::Value> value = DeserializeFromJson(str);
  EXPECT_NE(nullptr, value.get());
}

TEST(DeserializeFromJson, PoorlyFormedJsonObject) {
  std::string str = kPoorlyFormedJsonString;
  scoped_ptr<base::Value> value = DeserializeFromJson(str);
  EXPECT_EQ(nullptr, value.get());
}

TEST(SerializeToJson, BadValue) {
  base::BinaryValue value(scoped_ptr<char[]>(new char[12]), 12);
  scoped_ptr<std::string> str = SerializeToJson(value);
  EXPECT_EQ(nullptr, str.get());
}

TEST(SerializeToJson, EmptyValue) {
  base::DictionaryValue value;
  scoped_ptr<std::string> str = SerializeToJson(value);
  ASSERT_NE(nullptr, str.get());
  EXPECT_EQ(kEmptyJsonString, *str);
}

TEST(SerializeToJson, PopulatedValue) {
  base::DictionaryValue orig_value;
  orig_value.SetString(kTestKey, kTestValue);
  scoped_ptr<std::string> str = SerializeToJson(orig_value);
  ASSERT_NE(nullptr, str.get());

  scoped_ptr<base::Value> new_value = DeserializeFromJson(*str);
  ASSERT_NE(nullptr, new_value.get());
  EXPECT_TRUE(new_value->Equals(&orig_value));
}

class ScopedTempFile {
 public:
  ScopedTempFile() {
    // Create a temporary file
    base::CreateNewTempDirectory("", &dir_);
    file_ = dir_.Append(kTempfileName);
  }

  ~ScopedTempFile() {
    // Remove the temp directory.
    base::DeleteFile(dir_, true);
  }

  const base::FilePath& file() const { return file_; }
  const base::FilePath& dir() const { return dir_; }

  std::size_t Write(const char* str) {
    return static_cast<std::size_t>(base::WriteFile(file_, str, strlen(str)));
  }

  std::string Read() {
    std::string result;
    ReadFileToString(file_, &result);
    return result;
  }

 private:
  base::FilePath file_;
  base::FilePath dir_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTempFile);
};

TEST(DeserializeJsonFromFile, NoFile) {
  ScopedTempFile temp;

  ASSERT_TRUE(base::IsDirectoryEmpty(temp.dir()));
  scoped_ptr<base::Value> value = DeserializeJsonFromFile(temp.file());
  EXPECT_EQ(nullptr, value.get());
}

TEST(DeserializeJsonFromFile, EmptyString) {
  ScopedTempFile temp;
  EXPECT_EQ(strlen(""), temp.Write(""));
  scoped_ptr<base::Value> value = DeserializeJsonFromFile(temp.file());
  EXPECT_EQ(nullptr, value.get());
}

TEST(DeserializeJsonFromFile, EmptyJsonObject) {
  ScopedTempFile temp;
  EXPECT_EQ(strlen(kEmptyJsonString), temp.Write(kEmptyJsonString));
  scoped_ptr<base::Value> value = DeserializeJsonFromFile(temp.file());
  EXPECT_NE(nullptr, value.get());
}

TEST(DeserializeJsonFromFile, ProperJsonObject) {
  ScopedTempFile temp;
  EXPECT_EQ(strlen(kProperJsonString), temp.Write(kProperJsonString));
  scoped_ptr<base::Value> value = DeserializeJsonFromFile(temp.file());
  EXPECT_NE(nullptr, value.get());
}

TEST(DeserializeJsonFromFile, PoorlyFormedJsonObject) {
  ScopedTempFile temp;
  EXPECT_EQ(strlen(kPoorlyFormedJsonString),
            temp.Write(kPoorlyFormedJsonString));
  scoped_ptr<base::Value> value = DeserializeJsonFromFile(temp.file());
  EXPECT_EQ(nullptr, value.get());
}

TEST(SerializeJsonToFile, BadValue) {
  ScopedTempFile temp;

  base::BinaryValue value(scoped_ptr<char[]>(new char[12]), 12);
  ASSERT_FALSE(SerializeJsonToFile(temp.file(), value));
  std::string str(temp.Read());
  EXPECT_TRUE(str.empty());
}

TEST(SerializeJsonToFile, EmptyValue) {
  ScopedTempFile temp;

  base::DictionaryValue value;
  ASSERT_TRUE(SerializeJsonToFile(temp.file(), value));
  std::string str(temp.Read());
  ASSERT_FALSE(str.empty());
  EXPECT_EQ(kEmptyJsonFileString, str);
}

TEST(SerializeJsonToFile, PopulatedValue) {
  ScopedTempFile temp;

  base::DictionaryValue orig_value;
  orig_value.SetString(kTestKey, kTestValue);
  ASSERT_TRUE(SerializeJsonToFile(temp.file(), orig_value));
  std::string str(temp.Read());
  ASSERT_FALSE(str.empty());

  scoped_ptr<base::Value> new_value = DeserializeJsonFromFile(temp.file());
  ASSERT_NE(nullptr, new_value.get());
  EXPECT_TRUE(new_value->Equals(&orig_value));
}

}  // namespace chromecast
