/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Basic tests that verify our KURL's interface behaves the same as the
// original KURL's.

#include "platform/weborigin/KURL.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

namespace blink {

TEST(KURLTest, Getters) {
  struct GetterCase {
    const char* url;
    const char* protocol;
    const char* host;
    int port;
    const char* user;
    const char* pass;
    const char* path;
    const char* lastPathComponent;
    const char* query;
    const char* fragmentIdentifier;
    bool hasFragmentIdentifier;
  } cases[] = {
      {"http://www.google.com/foo/blah?bar=baz#ref", "http", "www.google.com",
       0, "", 0, "/foo/blah", "blah", "bar=baz", "ref", true},
      {// Non-ASCII code points in the fragment part. fragmentIdentifier()
       // shouldn't return it in percent-encoded form.
       "http://www.google.com/foo/blah?bar=baz#\xce\xb1\xce\xb2", "http",
       "www.google.com", 0, "", 0, "/foo/blah", "blah", "bar=baz",
       "\xce\xb1\xce\xb2", true},
      {"http://foo.com:1234/foo/bar/", "http", "foo.com", 1234, "", 0,
       "/foo/bar/", "bar", 0, 0, false},
      {"http://www.google.com?#", "http", "www.google.com", 0, "", 0, "/", 0,
       "", "", true},
      {"https://me:pass@google.com:23#foo", "https", "google.com", 23, "me",
       "pass", "/", 0, 0, "foo", true},
      {"javascript:hello!//world", "javascript", "", 0, "", 0, "hello!//world",
       "world", 0, 0, false},
      {// Recognize a query and a fragment in the path portion of a path
       // URL.
       "javascript:hello!?#/\\world", "javascript", "", 0, "", 0, "hello!",
       "hello!", "", "/\\world", true},
      {// lastPathComponent() method handles "parameters" in a path. path()
       // method doesn't.
       "http://a.com/hello;world", "http", "a.com", 0, "", 0, "/hello;world",
       "hello", 0, 0, false},
      {// IDNA
       "http://\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd/", "http",
       "xn--6qqa088eba", 0, "", 0, "/", 0, 0, 0, false},
  };

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(cases); i++) {
    const GetterCase& c = cases[i];

    const String& url = String::fromUTF8(c.url);

    const KURL kurl(ParsedURLString, url);

    // Casted to the String (or coverted to using fromUTF8() for
    // expectations which may include non-ASCII code points) so that the
    // contents are printed on failure.
    EXPECT_EQ(String(c.protocol), kurl.protocol()) << url;
    EXPECT_EQ(String(c.host), kurl.host()) << url;
    EXPECT_EQ(c.port, kurl.port()) << url;
    EXPECT_EQ(String(c.user), kurl.user()) << url;
    EXPECT_EQ(String(c.pass), kurl.pass()) << url;
    EXPECT_EQ(String(c.path), kurl.path()) << url;
    EXPECT_EQ(String(c.lastPathComponent), kurl.lastPathComponent()) << url;
    EXPECT_EQ(String(c.query), kurl.query()) << url;
    if (c.hasFragmentIdentifier)
      EXPECT_EQ(String::fromUTF8(c.fragmentIdentifier),
                kurl.fragmentIdentifier())
          << url;
    else
      EXPECT_TRUE(kurl.fragmentIdentifier().isNull()) << url;
  }
}

TEST(KURLTest, Setters) {
  // Replace the starting URL with the given components one at a time and
  // verify that we're always the same as the old KURL.
  //
  // Note that old KURL won't canonicalize the default port away, so we
  // can't set setting the http port to "80" (or even "0").
  //
  // We also can't test clearing the query.
  struct ExpectedComponentCase {
    const char* url;

    const char* protocol;
    const char* expectedProtocol;

    const char* host;
    const char* expectedHost;

    const int port;
    const char* expectedPort;

    const char* user;
    const char* expectedUser;

    const char* pass;
    const char* expectedPass;

    const char* path;
    const char* expectedPath;

    const char* query;
    const char* expectedQuery;
  } cases[] = {
      {"http://www.google.com/",
       // protocol
       "https", "https://www.google.com/",
       // host
       "news.google.com", "https://news.google.com/",
       // port
       8888, "https://news.google.com:8888/",
       // user
       "me", "https://me@news.google.com:8888/",
       // pass
       "pass", "https://me:pass@news.google.com:8888/",
       // path
       "/foo", "https://me:pass@news.google.com:8888/foo",
       // query
       "?q=asdf", "https://me:pass@news.google.com:8888/foo?q=asdf"},
      {"https://me:pass@google.com:88/a?f#b",
       // protocol
       "http", "http://me:pass@google.com:88/a?f#b",
       // host
       "goo.com", "http://me:pass@goo.com:88/a?f#b",
       // port
       92, "http://me:pass@goo.com:92/a?f#b",
       // user
       "", "http://:pass@goo.com:92/a?f#b",
       // pass
       "", "http://goo.com:92/a?f#b",
       // path
       "/", "http://goo.com:92/?f#b",
       // query
       0, "http://goo.com:92/#b"},
  };

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(cases); i++) {
    KURL kurl(ParsedURLString, cases[i].url);

    kurl.setProtocol(cases[i].protocol);
    EXPECT_STREQ(cases[i].expectedProtocol, kurl.getString().utf8().data());

    kurl.setHost(cases[i].host);
    EXPECT_STREQ(cases[i].expectedHost, kurl.getString().utf8().data());

    kurl.setPort(cases[i].port);
    EXPECT_STREQ(cases[i].expectedPort, kurl.getString().utf8().data());

    kurl.setUser(cases[i].user);
    EXPECT_STREQ(cases[i].expectedUser, kurl.getString().utf8().data());

    kurl.setPass(cases[i].pass);
    EXPECT_STREQ(cases[i].expectedPass, kurl.getString().utf8().data());

    kurl.setPath(cases[i].path);
    EXPECT_STREQ(cases[i].expectedPath, kurl.getString().utf8().data());

    kurl.setQuery(cases[i].query);
    EXPECT_STREQ(cases[i].expectedQuery, kurl.getString().utf8().data());

    // Refs are tested below. On the Safari 3.1 branch, we don't match their
    // KURL since we integrated a fix from their trunk.
  }
}

// Tests that KURL::decodeURLEscapeSequences works as expected
TEST(KURLTest, DecodeURLEscapeSequences) {
  struct DecodeCase {
    const char* input;
    const char* output;
  } decodeCases[] = {
      {"hello, world", "hello, world"},
      {"%01%02%03%04%05%06%07%08%09%0a%0B%0C%0D%0e%0f/",
       "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0B\x0C\x0D\x0e\x0f/"},
      {"%10%11%12%13%14%15%16%17%18%19%1a%1B%1C%1D%1e%1f/",
       "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1B\x1C\x1D\x1e\x1f/"},
      {"%20%21%22%23%24%25%26%27%28%29%2a%2B%2C%2D%2e%2f/",
       " !\"#$%&'()*+,-.//"},
      {"%30%31%32%33%34%35%36%37%38%39%3a%3B%3C%3D%3e%3f/",
       "0123456789:;<=>?/"},
      {"%40%41%42%43%44%45%46%47%48%49%4a%4B%4C%4D%4e%4f/",
       "@ABCDEFGHIJKLMNO/"},
      {"%50%51%52%53%54%55%56%57%58%59%5a%5B%5C%5D%5e%5f/",
       "PQRSTUVWXYZ[\\]^_/"},
      {"%60%61%62%63%64%65%66%67%68%69%6a%6B%6C%6D%6e%6f/",
       "`abcdefghijklmno/"},
      {"%70%71%72%73%74%75%76%77%78%79%7a%7B%7C%7D%7e%7f/",
       "pqrstuvwxyz{|}~\x7f/"},
      // Test un-UTF-8-ization.
      {"%e4%bd%a0%e5%a5%bd", "\xe4\xbd\xa0\xe5\xa5\xbd"},
  };

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(decodeCases); i++) {
    String input(decodeCases[i].input);
    String str = decodeURLEscapeSequences(input);
    EXPECT_STREQ(decodeCases[i].output, str.utf8().data());
  }

  // Our decode should decode %00
  String zero = decodeURLEscapeSequences("%00");
  EXPECT_STRNE("%00", zero.utf8().data());

  // Decode UTF-8.
  String decoded = decodeURLEscapeSequences("%e6%bc%a2%e5%ad%97");
  const UChar decodedExpected[] = {0x6F22, 0x5b57};
  EXPECT_EQ(String(decodedExpected, WTF_ARRAY_LENGTH(decodedExpected)),
            decoded);

  // Test the error behavior for invalid UTF-8 (we differ from WebKit here).
  String invalid = decodeURLEscapeSequences("%e4%a0%e5%a5%bd");
  UChar invalidExpectedHelper[4] = {0x00e4, 0x00a0, 0x597d, 0};
  String invalidExpected(
      reinterpret_cast<const ::UChar*>(invalidExpectedHelper), 3);
  EXPECT_EQ(invalidExpected, invalid);
}

TEST(KURLTest, EncodeWithURLEscapeSequences) {
  struct EncodeCase {
    const char* input;
    const char* output;
  } encode_cases[] = {
      {"hello, world", "hello%2C%20world"},
      {"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F",
       "%01%02%03%04%05%06%07%08%09%0A%0B%0C%0D%0E%0F"},
      {"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F",
       "%10%11%12%13%14%15%16%17%18%19%1A%1B%1C%1D%1E%1F"},
      {" !\"#$%&'()*+,-./", "%20!%22%23%24%25%26%27()*%2B%2C-./"},
      {"0123456789:;<=>?", "0123456789%3A%3B%3C%3D%3E%3F"},
      {"@ABCDEFGHIJKLMNO", "%40ABCDEFGHIJKLMNO"},
      {"PQRSTUVWXYZ[\\]^_", "PQRSTUVWXYZ%5B%5C%5D%5E_"},
      {"`abcdefghijklmno", "%60abcdefghijklmno"},
      {"pqrstuvwxyz{|}~\x7f", "pqrstuvwxyz%7B%7C%7D~%7F"},
  };

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(encode_cases); i++) {
    String input(encode_cases[i].input);
    String expectedOutput(encode_cases[i].output);
    String output = encodeWithURLEscapeSequences(input);
    EXPECT_EQ(expectedOutput, output);
  }

  // Our encode escapes NULLs for safety, so we need to check that too.
  String input("\x00\x01", 2);
  String reference("%00%01");

  String output = encodeWithURLEscapeSequences(input);
  EXPECT_EQ(reference, output);

  // Also test that it gets converted to UTF-8 properly.
  UChar wideInputHelper[3] = {0x4f60, 0x597d, 0};
  String wideInput(reinterpret_cast<const ::UChar*>(wideInputHelper), 2);
  String wideReference("%E4%BD%A0%E5%A5%BD");
  String wideOutput = encodeWithURLEscapeSequences(wideInput);
  EXPECT_EQ(wideReference, wideOutput);

  // Encoding should not NFC-normalize the string.
  // Contain a combining character ('e' + COMBINING OGONEK).
  String combining(String::fromUTF8("\x65\xCC\xA8"));
  EXPECT_EQ(encodeWithURLEscapeSequences(combining), "e%CC%A8");
  // Contain a precomposed character corresponding to |combining|.
  String precomposed(String::fromUTF8("\xC4\x99"));
  EXPECT_EQ(encodeWithURLEscapeSequences(precomposed), "%C4%99");
}

TEST(KURLTest, RemoveWhitespace) {
  struct {
    const char* input;
    const char* expected;
  } cases[] = {
      {"ht\ntps://example.com/yay?boo#foo", "https://example.com/yay?boo#foo"},
      {"ht\ttps://example.com/yay?boo#foo", "https://example.com/yay?boo#foo"},
      {"ht\rtps://example.com/yay?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://exa\nmple.com/yay?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://exa\tmple.com/yay?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://exa\rmple.com/yay?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/y\nay?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/y\tay?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/y\ray?boo#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/yay?b\noo#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/yay?b\too#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/yay?b\roo#foo", "https://example.com/yay?boo#foo"},
      {"https://example.com/yay?boo#f\noo", "https://example.com/yay?boo#foo"},
      {"https://example.com/yay?boo#f\too", "https://example.com/yay?boo#foo"},
      {"https://example.com/yay?boo#f\roo", "https://example.com/yay?boo#foo"},
  };

  for (const auto& test : cases) {
    const KURL input(ParsedURLString, test.input);
    const KURL expected(ParsedURLString, test.expected);
    EXPECT_EQ(input, expected);
    EXPECT_TRUE(input.whitespaceRemoved());
    EXPECT_FALSE(expected.whitespaceRemoved());
  }
}

TEST(KURLTest, ResolveEmpty) {
  KURL emptyBase;

  // WebKit likes to be able to resolve absolute input agains empty base URLs,
  // which would normally be invalid since the base URL is invalid.
  const char abs[] = "http://www.google.com/";
  KURL resolveAbs(emptyBase, abs);
  EXPECT_TRUE(resolveAbs.isValid());
  EXPECT_STREQ(abs, resolveAbs.getString().utf8().data());

  // Resolving a non-relative URL agains the empty one should still error.
  const char rel[] = "foo.html";
  KURL resolveErr(emptyBase, rel);
  EXPECT_FALSE(resolveErr.isValid());
}

// WebKit will make empty URLs and set components on them. kurl doesn't allow
// replacements on invalid URLs, but here we do.
TEST(KURLTest, ReplaceInvalid) {
  KURL kurl;

  EXPECT_FALSE(kurl.isValid());
  EXPECT_TRUE(kurl.isEmpty());
  EXPECT_STREQ("", kurl.getString().utf8().data());

  kurl.setProtocol("http");
  // GKURL will say that a URL with just a scheme is invalid, KURL will not.
  EXPECT_FALSE(kurl.isValid());
  EXPECT_FALSE(kurl.isEmpty());
  // At this point, we do things slightly differently if there is only a scheme.
  // We check the results here to make it more obvious what is going on, but it
  // shouldn't be a big deal if these change.
  EXPECT_STREQ("http:", kurl.getString().utf8().data());

  kurl.setHost("www.google.com");
  EXPECT_TRUE(kurl.isValid());
  EXPECT_FALSE(kurl.isEmpty());
  EXPECT_STREQ("http://www.google.com/", kurl.getString().utf8().data());

  kurl.setPort(8000);
  EXPECT_TRUE(kurl.isValid());
  EXPECT_FALSE(kurl.isEmpty());
  EXPECT_STREQ("http://www.google.com:8000/", kurl.getString().utf8().data());

  kurl.setPath("/favicon.ico");
  EXPECT_TRUE(kurl.isValid());
  EXPECT_FALSE(kurl.isEmpty());
  EXPECT_STREQ("http://www.google.com:8000/favicon.ico",
               kurl.getString().utf8().data());

  // Now let's test that giving an invalid replacement fails. Invalid
  // protocols fail without modifying the URL, which should remain valid.
  EXPECT_FALSE(kurl.setProtocol("f/sj#@"));
  EXPECT_TRUE(kurl.isValid());
}

TEST(KURLTest, Valid_HTTP_FTP_URLsHaveHosts) {
  // Since the suborigin schemes are added at the content layer, its
  // necessary it explicitly add them as standard schemes for this test. If
  // this is needed in the future across mulitple KURLTests, then KURLTest
  // should probably be converted to a test fixture with a proper SetUp()
  // method.
  url::AddStandardScheme("http-so", url::SCHEME_WITH_PORT);
  url::AddStandardScheme("https-so", url::SCHEME_WITH_PORT);

  KURL kurl(ParsedURLString, "foo://www.google.com/");
  EXPECT_TRUE(kurl.setProtocol("http"));
  EXPECT_TRUE(kurl.protocolIs("http"));
  EXPECT_TRUE(kurl.protocolIsInHTTPFamily());
  EXPECT_TRUE(kurl.isValid());

  EXPECT_TRUE(kurl.setProtocol("http-so"));
  EXPECT_TRUE(kurl.protocolIs("http-so"));
  EXPECT_TRUE(kurl.isValid());

  EXPECT_TRUE(kurl.setProtocol("https"));
  EXPECT_TRUE(kurl.protocolIs("https"));
  EXPECT_TRUE(kurl.isValid());

  EXPECT_TRUE(kurl.setProtocol("https-so"));
  EXPECT_TRUE(kurl.protocolIs("https-so"));
  EXPECT_TRUE(kurl.isValid());

  EXPECT_TRUE(kurl.setProtocol("ftp"));
  EXPECT_TRUE(kurl.protocolIs("ftp"));
  EXPECT_TRUE(kurl.isValid());

  kurl = KURL(KURL(), "http://");
  EXPECT_FALSE(kurl.protocolIs("http"));

  kurl = KURL(KURL(), "http://wide#鸡");
  EXPECT_TRUE(kurl.protocolIs("http"));
  EXPECT_EQ(kurl.protocol(), "http");

  kurl = KURL(KURL(), "http-so://foo");
  EXPECT_TRUE(kurl.protocolIs("http-so"));

  kurl = KURL(KURL(), "https://foo");
  EXPECT_TRUE(kurl.protocolIs("https"));

  kurl = KURL(KURL(), "https-so://foo");
  EXPECT_TRUE(kurl.protocolIs("https-so"));

  kurl = KURL(KURL(), "ftp://foo");
  EXPECT_TRUE(kurl.protocolIs("ftp"));

  kurl = KURL(KURL(), "http://host/");
  EXPECT_TRUE(kurl.isValid());
  kurl.setHost("");
  EXPECT_FALSE(kurl.isValid());

  kurl = KURL(KURL(), "http-so://host/");
  EXPECT_TRUE(kurl.isValid());
  kurl.setHost("");
  EXPECT_FALSE(kurl.isValid());

  kurl = KURL(KURL(), "https://host/");
  EXPECT_TRUE(kurl.isValid());
  kurl.setHost("");
  EXPECT_FALSE(kurl.isValid());

  kurl = KURL(KURL(), "https-so://host/");
  EXPECT_TRUE(kurl.isValid());
  kurl.setHost("");
  EXPECT_FALSE(kurl.isValid());

  kurl = KURL(KURL(), "ftp://host/");
  EXPECT_TRUE(kurl.isValid());
  kurl.setHost("");
  EXPECT_FALSE(kurl.isValid());

  kurl = KURL(KURL(), "http:///noodles/pho.php");
  EXPECT_STREQ("http://noodles/pho.php", kurl.getString().utf8().data());
  EXPECT_STREQ("noodles", kurl.host().utf8().data());
  EXPECT_TRUE(kurl.isValid());

  kurl = KURL(KURL(), "https://username:password@/");
  EXPECT_FALSE(kurl.isValid());

  kurl = KURL(KURL(), "https://username:password@host/");
  EXPECT_TRUE(kurl.isValid());
}

TEST(KURLTest, Path) {
  const char initial[] = "http://www.google.com/path/foo";
  KURL kurl(ParsedURLString, initial);

  // Clear by setting a null string.
  String nullString;
  EXPECT_TRUE(nullString.isNull());
  kurl.setPath(nullString);
  EXPECT_STREQ("http://www.google.com/", kurl.getString().utf8().data());
}

// Test that setting the query to different things works. Thq query is handled
// a littler differently than some of the other components.
TEST(KURLTest, Query) {
  const char initial[] = "http://www.google.com/search?q=awesome";
  KURL kurl(ParsedURLString, initial);

  // Clear by setting a null string.
  String nullString;
  EXPECT_TRUE(nullString.isNull());
  kurl.setQuery(nullString);
  EXPECT_STREQ("http://www.google.com/search", kurl.getString().utf8().data());

  // Clear by setting an empty string.
  kurl = KURL(ParsedURLString, initial);
  String emptyString("");
  EXPECT_FALSE(emptyString.isNull());
  kurl.setQuery(emptyString);
  EXPECT_STREQ("http://www.google.com/search?", kurl.getString().utf8().data());

  // Set with something that begins in a question mark.
  const char question[] = "?foo=bar";
  kurl.setQuery(question);
  EXPECT_STREQ("http://www.google.com/search?foo=bar",
               kurl.getString().utf8().data());

  // Set with something that doesn't begin in a question mark.
  const char query[] = "foo=bar";
  kurl.setQuery(query);
  EXPECT_STREQ("http://www.google.com/search?foo=bar",
               kurl.getString().utf8().data());
}

TEST(KURLTest, Ref) {
  KURL kurl(ParsedURLString, "http://foo/bar#baz");

  // Basic ref setting.
  KURL cur(ParsedURLString, "http://foo/bar");
  cur.setFragmentIdentifier("asdf");
  EXPECT_STREQ("http://foo/bar#asdf", cur.getString().utf8().data());
  cur = kurl;
  cur.setFragmentIdentifier("asdf");
  EXPECT_STREQ("http://foo/bar#asdf", cur.getString().utf8().data());

  // Setting a ref to the empty string will set it to "#".
  cur = KURL(ParsedURLString, "http://foo/bar");
  cur.setFragmentIdentifier("");
  EXPECT_STREQ("http://foo/bar#", cur.getString().utf8().data());
  cur = kurl;
  cur.setFragmentIdentifier("");
  EXPECT_STREQ("http://foo/bar#", cur.getString().utf8().data());

  // Setting the ref to the null string will clear it altogether.
  cur = KURL(ParsedURLString, "http://foo/bar");
  cur.setFragmentIdentifier(String());
  EXPECT_STREQ("http://foo/bar", cur.getString().utf8().data());
  cur = kurl;
  cur.setFragmentIdentifier(String());
  EXPECT_STREQ("http://foo/bar", cur.getString().utf8().data());
}

TEST(KURLTest, Empty) {
  KURL kurl;

  // First test that regular empty URLs are the same.
  EXPECT_TRUE(kurl.isEmpty());
  EXPECT_FALSE(kurl.isValid());
  EXPECT_TRUE(kurl.isNull());
  EXPECT_TRUE(kurl.getString().isNull());
  EXPECT_TRUE(kurl.getString().isEmpty());

  // Test resolving a null URL on an empty string.
  KURL kurl2(kurl, "");
  EXPECT_FALSE(kurl2.isNull());
  EXPECT_TRUE(kurl2.isEmpty());
  EXPECT_FALSE(kurl2.isValid());
  EXPECT_FALSE(kurl2.getString().isNull());
  EXPECT_TRUE(kurl2.getString().isEmpty());
  EXPECT_FALSE(kurl2.getString().isNull());
  EXPECT_TRUE(kurl2.getString().isEmpty());

  // Resolve the null URL on a null string.
  KURL kurl22(kurl, String());
  EXPECT_FALSE(kurl22.isNull());
  EXPECT_TRUE(kurl22.isEmpty());
  EXPECT_FALSE(kurl22.isValid());
  EXPECT_FALSE(kurl22.getString().isNull());
  EXPECT_TRUE(kurl22.getString().isEmpty());
  EXPECT_FALSE(kurl22.getString().isNull());
  EXPECT_TRUE(kurl22.getString().isEmpty());

  // Test non-hierarchical schemes resolving. The actual URLs will be different.
  // WebKit's one will set the string to "something.gif" and we'll set it to an
  // empty string. I think either is OK, so we just check our behavior.
  KURL kurl3(KURL(ParsedURLString, "data:foo"), "something.gif");
  EXPECT_TRUE(kurl3.isEmpty());
  EXPECT_FALSE(kurl3.isValid());

  // Test for weird isNull string input,
  // see: http://bugs.webkit.org/show_bug.cgi?id=16487
  KURL kurl4(ParsedURLString, kurl.getString());
  EXPECT_TRUE(kurl4.isEmpty());
  EXPECT_FALSE(kurl4.isValid());
  EXPECT_TRUE(kurl4.getString().isNull());
  EXPECT_TRUE(kurl4.getString().isEmpty());

  // Resolving an empty URL on an invalid string.
  KURL kurl5(KURL(), "foo.js");
  // We'll be empty in this case, but KURL won't be. Should be OK.
  // EXPECT_EQ(kurl5.isEmpty(), kurl5.isEmpty());
  // EXPECT_EQ(kurl5.getString().isEmpty(), kurl5.getString().isEmpty());
  EXPECT_FALSE(kurl5.isValid());
  EXPECT_FALSE(kurl5.getString().isNull());

  // Empty string as input
  KURL kurl6(ParsedURLString, "");
  EXPECT_TRUE(kurl6.isEmpty());
  EXPECT_FALSE(kurl6.isValid());
  EXPECT_FALSE(kurl6.getString().isNull());
  EXPECT_TRUE(kurl6.getString().isEmpty());

  // Non-empty but invalid C string as input.
  KURL kurl7(ParsedURLString, "foo.js");
  // WebKit will actually say this URL has the string "foo.js" but is invalid.
  // We don't do that.
  // EXPECT_EQ(kurl7.isEmpty(), kurl7.isEmpty());
  EXPECT_FALSE(kurl7.isValid());
  EXPECT_FALSE(kurl7.getString().isNull());
}

TEST(KURLTest, UserPass) {
  const char* src = "http://user:pass@google.com/";
  KURL kurl(ParsedURLString, src);

  // Clear just the username.
  kurl.setUser("");
  EXPECT_EQ("http://:pass@google.com/", kurl.getString());

  // Clear just the password.
  kurl = KURL(ParsedURLString, src);
  kurl.setPass("");
  EXPECT_EQ("http://user@google.com/", kurl.getString());

  // Now clear both.
  kurl.setUser("");
  EXPECT_EQ("http://google.com/", kurl.getString());
}

TEST(KURLTest, Offsets) {
  const char* src1 = "http://user:pass@google.com/foo/bar.html?baz=query#ref";
  KURL kurl1(ParsedURLString, src1);

  EXPECT_EQ(17u, kurl1.hostStart());
  EXPECT_EQ(27u, kurl1.hostEnd());
  EXPECT_EQ(27u, kurl1.pathStart());
  EXPECT_EQ(40u, kurl1.pathEnd());
  EXPECT_EQ(32u, kurl1.pathAfterLastSlash());

  const char* src2 = "http://google.com/foo/";
  KURL kurl2(ParsedURLString, src2);

  EXPECT_EQ(7u, kurl2.hostStart());
  EXPECT_EQ(17u, kurl2.hostEnd());
  EXPECT_EQ(17u, kurl2.pathStart());
  EXPECT_EQ(22u, kurl2.pathEnd());
  EXPECT_EQ(22u, kurl2.pathAfterLastSlash());

  const char* src3 = "javascript:foobar";
  KURL kurl3(ParsedURLString, src3);

  EXPECT_EQ(11u, kurl3.hostStart());
  EXPECT_EQ(11u, kurl3.hostEnd());
  EXPECT_EQ(11u, kurl3.pathStart());
  EXPECT_EQ(17u, kurl3.pathEnd());
  EXPECT_EQ(11u, kurl3.pathAfterLastSlash());
}

TEST(KURLTest, DeepCopy) {
  const char url[] = "http://www.google.com/";
  KURL src(ParsedURLString, url);
  EXPECT_TRUE(src.getString() ==
              url);  // This really just initializes the cache.
  KURL dest = src.copy();
  EXPECT_TRUE(dest.getString() ==
              url);  // This really just initializes the cache.

  // The pointers should be different for both UTF-8 and UTF-16.
  EXPECT_NE(dest.getString().impl(), src.getString().impl());
}

TEST(KURLTest, DeepCopyInnerURL) {
  const char url[] = "filesystem:http://www.google.com/temporary/test.txt";
  const char innerURL[] = "http://www.google.com/temporary";
  KURL src(ParsedURLString, url);
  EXPECT_TRUE(src.getString() == url);
  EXPECT_TRUE(src.innerURL()->getString() == innerURL);
  KURL dest = src.copy();
  EXPECT_TRUE(dest.getString() == url);
  EXPECT_TRUE(dest.innerURL()->getString() == innerURL);
}

TEST(KURLTest, LastPathComponent) {
  KURL url1(ParsedURLString, "http://host/path/to/file.txt");
  EXPECT_EQ("file.txt", url1.lastPathComponent());

  KURL invalidUTF8(ParsedURLString, "http://a@9%aa%:/path/to/file.txt");
  EXPECT_EQ(String(), invalidUTF8.lastPathComponent());
}

TEST(KURLTest, IsHierarchical) {
  KURL url1(ParsedURLString, "http://host/path/to/file.txt");
  EXPECT_TRUE(url1.isHierarchical());

  KURL invalidUTF8(ParsedURLString, "http://a@9%aa%:/path/to/file.txt");
  EXPECT_FALSE(invalidUTF8.isHierarchical());
}

TEST(KURLTest, PathAfterLastSlash) {
  KURL url1(ParsedURLString, "http://host/path/to/file.txt");
  EXPECT_EQ(20u, url1.pathAfterLastSlash());

  KURL invalidUTF8(ParsedURLString, "http://a@9%aa%:/path/to/file.txt");
  EXPECT_EQ(0u, invalidUTF8.pathAfterLastSlash());
}

TEST(KURLTest, ProtocolIsInHTTPFamily) {
  KURL url1(ParsedURLString, "http://host/path/to/file.txt");
  EXPECT_TRUE(url1.protocolIsInHTTPFamily());

  KURL invalidUTF8(ParsedURLString, "http://a@9%aa%:/path/to/file.txt");
  EXPECT_FALSE(invalidUTF8.protocolIsInHTTPFamily());
}

TEST(KURLTest, ProtocolIs) {
  KURL url1(ParsedURLString, "foo://bar");
  EXPECT_TRUE(url1.protocolIs("foo"));
  EXPECT_FALSE(url1.protocolIs("foo-bar"));

  KURL url2(ParsedURLString, "foo-bar:");
  EXPECT_TRUE(url2.protocolIs("foo-bar"));
  EXPECT_FALSE(url2.protocolIs("foo"));

  KURL invalidUTF8(ParsedURLString, "http://a@9%aa%:");
  EXPECT_FALSE(invalidUTF8.protocolIs("http"));

  KURL capital(KURL(), "HTTP://www.example.text");
  EXPECT_TRUE(capital.protocolIs("http"));
  EXPECT_EQ(capital.protocol(), "http");
}

TEST(KURLTest, strippedForUseAsReferrer) {
  struct ReferrerCase {
    const char* input;
    const char* output;
  } referrerCases[] = {
      {"data:text/html;charset=utf-8,<html></html>", ""},
      {"javascript:void(0);", ""},
      {"about:config", ""},
      {"https://www.google.com/", "https://www.google.com/"},
      {"http://me@news.google.com:8888/", "http://news.google.com:8888/"},
      {"http://:pass@news.google.com:8888/foo",
       "http://news.google.com:8888/foo"},
      {"http://me:pass@news.google.com:8888/", "http://news.google.com:8888/"},
      {"https://www.google.com/a?f#b", "https://www.google.com/a?f"},
      {"file:///tmp/test.html", ""},
      {"https://www.google.com/#", "https://www.google.com/"},
  };

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(referrerCases); i++) {
    KURL kurl(ParsedURLString, referrerCases[i].input);
    String referrer = kurl.strippedForUseAsReferrer();
    EXPECT_STREQ(referrerCases[i].output, referrer.utf8().data());
  }
}

}  // namespace blink
