// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/fpdf_parser/include/cpdf_dictionary.h"
#include "core/fpdfapi/fpdf_parser/include/cpdf_indirect_object_holder.h"
#include "core/fpdfdoc/include/cpdf_formfield.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(cpdf_formfield, FPDF_GetFullName) {
  CFX_WideString name = FPDF_GetFullName(nullptr);
  EXPECT_TRUE(name.IsEmpty());

  CPDF_IndirectObjectHolder obj_holder;
  CPDF_Dictionary* root = new CPDF_Dictionary;
  obj_holder.AddIndirectObject(root);
  root->SetAtName("T", "foo");
  name = FPDF_GetFullName(root);
  EXPECT_STREQ("foo", name.UTF8Encode().c_str());

  CPDF_Dictionary* dict1 = new CPDF_Dictionary;
  obj_holder.AddIndirectObject(dict1);
  dict1->SetAtName("T", "bar");
  root->SetAtReference("Parent", &obj_holder, dict1);
  name = FPDF_GetFullName(root);
  EXPECT_STREQ("bar.foo", name.UTF8Encode().c_str());

  CPDF_Dictionary* dict2 = new CPDF_Dictionary;
  obj_holder.AddIndirectObject(dict2);
  dict1->SetAt("Parent", dict2);
  name = FPDF_GetFullName(root);
  EXPECT_STREQ("bar.foo", name.UTF8Encode().c_str());

  CPDF_Dictionary* dict3 = new CPDF_Dictionary;
  obj_holder.AddIndirectObject(dict3);
  dict3->SetAtName("T", "qux");
  dict2->SetAtReference("Parent", &obj_holder, dict3);
  name = FPDF_GetFullName(root);
  EXPECT_STREQ("qux.bar.foo", name.UTF8Encode().c_str());

  dict3->SetAtReference("Parent", &obj_holder, root);
  name = FPDF_GetFullName(root);
  EXPECT_STREQ("qux.bar.foo", name.UTF8Encode().c_str());
  name = FPDF_GetFullName(dict1);
  EXPECT_STREQ("foo.qux.bar", name.UTF8Encode().c_str());
  name = FPDF_GetFullName(dict2);
  EXPECT_STREQ("bar.foo.qux", name.UTF8Encode().c_str());
  name = FPDF_GetFullName(dict3);
  EXPECT_STREQ("bar.foo.qux", name.UTF8Encode().c_str());
}
