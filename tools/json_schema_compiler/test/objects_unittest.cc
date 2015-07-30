// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/objects.h"

#include "base/json/json_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace test::api::objects;

TEST(JsonSchemaCompilerObjectsTest, ObjectParamParamsCreate) {
  {
    scoped_ptr<base::ListValue> strings(new base::ListValue());
    strings->Append(new base::StringValue("one"));
    strings->Append(new base::StringValue("two"));
    scoped_ptr<base::DictionaryValue> info_value(new base::DictionaryValue());
    info_value->Set("strings", strings.release());
    info_value->Set("integer", new base::FundamentalValue(5));
    info_value->Set("boolean", new base::FundamentalValue(true));

    scoped_ptr<base::ListValue> params_value(new base::ListValue());
    params_value->Append(info_value.release());
    scoped_ptr<ObjectParam::Params> params(
        ObjectParam::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ((size_t) 2, params->info.strings.size());
    EXPECT_EQ("one", params->info.strings[0]);
    EXPECT_EQ("two", params->info.strings[1]);
    EXPECT_EQ(5, params->info.integer);
    EXPECT_TRUE(params->info.boolean);
  }
  {
    scoped_ptr<base::ListValue> strings(new base::ListValue());
    strings->Append(new base::StringValue("one"));
    strings->Append(new base::StringValue("two"));
    scoped_ptr<base::DictionaryValue> info_value(new base::DictionaryValue());
    info_value->Set("strings", strings.release());
    info_value->Set("integer", new base::FundamentalValue(5));

    scoped_ptr<base::ListValue> params_value(new base::ListValue());
    params_value->Append(info_value.release());
    scoped_ptr<ObjectParam::Params> params(
        ObjectParam::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerObjectsTest, ReturnsObjectResultCreate) {
  ReturnsObject::Results::Info info;
  info.state = FIRST_STATE_FOO;
  scoped_ptr<base::ListValue> results = ReturnsObject::Results::Create(info);

  base::DictionaryValue expected;
  expected.SetString("state", "foo");
  base::DictionaryValue* result = NULL;
  ASSERT_TRUE(results->GetDictionary(0, &result));
  ASSERT_TRUE(result->Equals(&expected));
}

TEST(JsonSchemaCompilerObjectsTest, OnObjectFiredCreate) {
  OnObjectFired::SomeObject object;
  object.state = FIRST_STATE_BAR;
  scoped_ptr<base::ListValue> results(OnObjectFired::Create(object));

  base::DictionaryValue expected;
  expected.SetString("state", "bar");
  base::DictionaryValue* result = NULL;
  ASSERT_TRUE(results->GetDictionary(0, &result));
  ASSERT_TRUE(result->Equals(&expected));
}
