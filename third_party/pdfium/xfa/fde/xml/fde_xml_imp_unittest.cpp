// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xfa/fde/xml/fde_xml_imp.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "xfa/fgas/crt/fgas_stream.h"

TEST(CFDE_XMLSyntaxParser, CData) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">\n"
      L"  <![CDATA[\n"
      L"    if (a[1] < 3)\n"
      L"      app.alert(\"Tclams\");\n"
      L"  ]]>\n"
      L"</script>";

  const FX_WCHAR* cdata =
      L"\n"
      L"    if (a[1] < 3)\n"
      L"      app.alert(\"Tclams\");\n"
      L"  ";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n  ", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::CData, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(cdata, data);

  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementClose, parser.DoSyntaxParse());
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, CDataWithInnerScript) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">\n"
      L"  <![CDATA[\n"
      L"    if (a[1] < 3)\n"
      L"      app.alert(\"Tclams\");\n"
      L"    </script>\n"
      L"  ]]>\n"
      L"</script>";

  const FX_WCHAR* cdata =
      L"\n"
      L"    if (a[1] < 3)\n"
      L"      app.alert(\"Tclams\");\n"
      L"    </script>\n"
      L"  ";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n  ", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::CData, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(cdata, data);

  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementClose, parser.DoSyntaxParse());
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, ArrowBangArrow) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">\n"
      L"  <!>\n"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n  ", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementClose, parser.DoSyntaxParse());
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, ArrowBangBracketArrow) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">\n"
      L"  <![>\n"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n  ", data);

  // Parser walks to end of input.

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, IncompleteCData) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">\n"
      L"  <![CDATA>\n"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n  ", data);

  // Parser walks to end of input.

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, UnClosedCData) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">\n"
      L"  <![CDATA[\n"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n  ", data);

  // Parser walks to end of input.

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, EmptyCData) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">\n"
      L"  <![CDATA[]]>\n"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n  ", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::CData, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementClose, parser.DoSyntaxParse());
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, Comment) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">\n"
      L"  <!-- A Comment -->\n"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n  ", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementClose, parser.DoSyntaxParse());
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, IncorrectCommentStart) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">\n"
      L"  <!- A Comment -->\n"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n  ", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementClose, parser.DoSyntaxParse());
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, CommentEmpty) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">\n"
      L"  <!---->\n"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n  ", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementClose, parser.DoSyntaxParse());
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, CommentThreeDash) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">\n"
      L"  <!--->\n"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n  ", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, CommentTwoDash) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">\n"
      L"  <!-->\n"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"\n  ", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, Entities) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">"
      L"&#66;"
      L"&#x54;"
      L"&#x00000000000000000048;"
      L"&#x0000000000000000AB48;"
      L"&#x0000000000000000000;"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"BTH\xab48", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementClose, parser.DoSyntaxParse());
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, EntityOverflowHex) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">"
      L"&#xaDBDFFFFF;"
      L"&#xafffffffffffffffffffffffffffffffff;"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"  ", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementClose, parser.DoSyntaxParse());
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}

TEST(CFDE_XMLSyntaxParser, EntityOverflowDecimal) {
  const FX_WCHAR* input =
      L"<script contentType=\"application/x-javascript\">"
      L"&#2914910205;"
      L"&#29149102052342342134521341234512351234213452315;"
      L"</script>";

  // We * sizeof(FX_WCHAR) because we pass in the uint8_t, not the FX_WCHAR.
  size_t len = FXSYS_wcslen(input) * sizeof(FX_WCHAR);
  CFX_RetainPtr<IFGAS_Stream> stream = IFGAS_Stream::CreateStream(
      reinterpret_cast<uint8_t*>(const_cast<FX_WCHAR*>(input)), len, 0);
  CFDE_XMLSyntaxParser parser;
  parser.Init(stream, 256);
  EXPECT_EQ(FDE_XmlSyntaxResult::ElementOpen, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::TagName, parser.DoSyntaxParse());

  CFX_WideString data;
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::AttriName, parser.DoSyntaxParse());
  parser.GetAttributeName(data);
  EXPECT_EQ(L"contentType", data);
  EXPECT_EQ(FDE_XmlSyntaxResult::AttriValue, parser.DoSyntaxParse());
  parser.GetAttributeValue(data);
  EXPECT_EQ(L"application/x-javascript", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementBreak, parser.DoSyntaxParse());
  EXPECT_EQ(FDE_XmlSyntaxResult::Text, parser.DoSyntaxParse());
  parser.GetTextData(data);
  EXPECT_EQ(L"  ", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::ElementClose, parser.DoSyntaxParse());
  parser.GetTagName(data);
  EXPECT_EQ(L"script", data);

  EXPECT_EQ(FDE_XmlSyntaxResult::EndOfString, parser.DoSyntaxParse());
}
