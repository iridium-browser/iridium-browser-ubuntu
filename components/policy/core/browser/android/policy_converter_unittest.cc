// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/policy/core/browser/android/policy_converter.h"
#include "components/policy/core/common/schema.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::FundamentalValue;
using base::ListValue;
using base::StringValue;
using base::Value;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace policy {
namespace android {

class PolicyConverterTest : public testing::Test {
 public:
  void SetUp() override {
    const char kSchemaTemplate[] =
        "{"
        "  \"type\": \"object\","
        "  \"properties\": {"
        "    \"null\": { \"type\": \"null\" },"
        "    \"bool\": { \"type\": \"boolean\" },"
        "    \"int\": { \"type\": \"integer\" },"
        "    \"double\": { \"type\": \"number\" },"
        "    \"string\": { \"type\": \"string\" },"
        "    \"list\": {"
        "      \"type\": \"array\","
        "      \"items\": { \"type\": \"string\"}"
        "    },"
        "    \"dict\": { \"type\": \"object\" }"
        "  }"
        "}";

    std::string error;
    schema_ = Schema::Parse(kSchemaTemplate, &error);
    ASSERT_TRUE(schema_.valid()) << error;
  }

 protected:
  // Converts the passed in value to the passed in schema, and serializes the
  // result to JSON, to make it easier to compare with EXPECT_EQ.
  std::string Convert(Value* value, const Schema& value_schema) {
    std::unique_ptr<Value> converted_value(
        PolicyConverter::ConvertValueToSchema(std::unique_ptr<Value>(value),
                                              value_schema));

    std::string json_string;
    EXPECT_TRUE(
        base::JSONWriter::Write(*(converted_value.get()), &json_string));
    return json_string;
  }

  // Uses|PolicyConverter::ConvertJavaStringArrayToListValue| to convert the
  // passed in java array and serializes the result to JSON, to make it easier
  // to compare with EXPECT_EQ.
  std::string ConvertJavaStringArrayToListValue(
      JNIEnv* env,
      const JavaRef<jobjectArray>& java_array) {
    std::unique_ptr<ListValue> list =
        PolicyConverter::ConvertJavaStringArrayToListValue(env, java_array);

    std::string json_string;
    EXPECT_TRUE(base::JSONWriter::Write(*list, &json_string));

    return json_string;
  }

  // Converts the passed in values to a java string array
  ScopedJavaLocalRef<jobjectArray> MakeJavaStringArray(
      JNIEnv* env,
      std::vector<std::string> values) {
    jobjectArray java_array = (jobjectArray)env->NewObjectArray(
        values.size(), env->FindClass("java/lang/String"), nullptr);
    for (size_t i = 0; i < values.size(); i++) {
      env->SetObjectArrayElement(
          java_array, i,
          base::android::ConvertUTF8ToJavaString(env, values[i]).obj());
    }

    return ScopedJavaLocalRef<jobjectArray>(env, java_array);
  }

  Schema schema_;
};

TEST_F(PolicyConverterTest, ConvertToNullValue) {
  Schema null_schema = schema_.GetKnownProperty("null");
  ASSERT_TRUE(null_schema.valid());

  EXPECT_EQ("null", Convert(new StringValue("foo"), null_schema));
}

TEST_F(PolicyConverterTest, ConvertToBoolValue) {
  Schema bool_schema = schema_.GetKnownProperty("bool");
  ASSERT_TRUE(bool_schema.valid());

  EXPECT_EQ("true", Convert(new FundamentalValue(true), bool_schema));
  EXPECT_EQ("false", Convert(new FundamentalValue(false), bool_schema));
  EXPECT_EQ("true", Convert(new StringValue("true"), bool_schema));
  EXPECT_EQ("false", Convert(new StringValue("false"), bool_schema));
  EXPECT_EQ("\"narf\"", Convert(new StringValue("narf"), bool_schema));
  EXPECT_EQ("false", Convert(new FundamentalValue(0), bool_schema));
  EXPECT_EQ("true", Convert(new FundamentalValue(1), bool_schema));
  EXPECT_EQ("true", Convert(new FundamentalValue(42), bool_schema));
  EXPECT_EQ("true", Convert(new FundamentalValue(-1), bool_schema));
  EXPECT_EQ("\"1\"", Convert(new StringValue("1"), bool_schema));
  EXPECT_EQ("{}", Convert(new DictionaryValue(), bool_schema));
}

TEST_F(PolicyConverterTest, ConvertToIntValue) {
  Schema int_schema = schema_.GetKnownProperty("int");
  ASSERT_TRUE(int_schema.valid());

  EXPECT_EQ("23", Convert(new FundamentalValue(23), int_schema));
  EXPECT_EQ("42", Convert(new StringValue("42"), int_schema));
  EXPECT_EQ("-1", Convert(new StringValue("-1"), int_schema));
  EXPECT_EQ("\"poit\"", Convert(new StringValue("poit"), int_schema));
  EXPECT_EQ("false", Convert(new FundamentalValue(false), int_schema));
}

TEST_F(PolicyConverterTest, ConvertToDoubleValue) {
  Schema double_schema = schema_.GetKnownProperty("double");
  ASSERT_TRUE(double_schema.valid());

  EXPECT_EQ("3", Convert(new FundamentalValue(3), double_schema));
  EXPECT_EQ("3.14", Convert(new FundamentalValue(3.14), double_schema));
  EXPECT_EQ("2.71", Convert(new StringValue("2.71"), double_schema));
  EXPECT_EQ("\"zort\"", Convert(new StringValue("zort"), double_schema));
  EXPECT_EQ("true", Convert(new FundamentalValue(true), double_schema));
}

TEST_F(PolicyConverterTest, ConvertToStringValue) {
  Schema string_schema = schema_.GetKnownProperty("string");
  ASSERT_TRUE(string_schema.valid());

  EXPECT_EQ("\"troz\"", Convert(new StringValue("troz"), string_schema));
  EXPECT_EQ("4711", Convert(new FundamentalValue(4711), string_schema));
}

TEST_F(PolicyConverterTest, ConvertToListValue) {
  Schema list_schema = schema_.GetKnownProperty("list");
  ASSERT_TRUE(list_schema.valid());

  ListValue* list = new ListValue;
  list->AppendString("foo");
  list->AppendString("bar");
  EXPECT_EQ("[\"foo\",\"bar\"]", Convert(list, list_schema));
  EXPECT_EQ("[\"baz\",\"blurp\"]",
            Convert(new StringValue("[\"baz\", \"blurp\"]"), list_schema));
  EXPECT_EQ("\"hurz\"", Convert(new StringValue("hurz"), list_schema));
  EXPECT_EQ("19", Convert(new FundamentalValue(19), list_schema));
}

TEST_F(PolicyConverterTest, ConvertFromJavaListToListValue) {
  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_EQ("[\"foo\",\"bar\",\"baz\"]",
            ConvertJavaStringArrayToListValue(
                env, MakeJavaStringArray(env, {"foo", "bar", "baz"})));
  EXPECT_EQ("[]", ConvertJavaStringArrayToListValue(
                      env, MakeJavaStringArray(env, {})));
}

TEST_F(PolicyConverterTest, ConvertToDictionaryValue) {
  Schema dict_schema = schema_.GetKnownProperty("dict");
  ASSERT_TRUE(dict_schema.valid());

  DictionaryValue* dict = new DictionaryValue;
  dict->SetInteger("thx", 1138);
  EXPECT_EQ("{\"thx\":1138}", Convert(dict, dict_schema));
  EXPECT_EQ("{\"moose\":true}",
            Convert(new StringValue("{\"moose\": true}"), dict_schema));
  EXPECT_EQ("\"fnord\"", Convert(new StringValue("fnord"), dict_schema));
  EXPECT_EQ("1729", Convert(new FundamentalValue(1729), dict_schema));
}

}  // namespace android
}  // namespace policy
