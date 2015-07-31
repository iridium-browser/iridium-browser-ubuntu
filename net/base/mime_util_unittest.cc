// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/mime_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace net {

TEST(MimeUtilTest, ExtensionTest) {
  const struct {
    const base::FilePath::CharType* extension;
    const char* const mime_type;
    bool valid;
  } tests[] = {
    {FILE_PATH_LITERAL("png"), "image/png", true},
    {FILE_PATH_LITERAL("PNG"), "image/png", true},
    {FILE_PATH_LITERAL("css"), "text/css", true},
    {FILE_PATH_LITERAL("pjp"), "image/jpeg", true},
    {FILE_PATH_LITERAL("pjpeg"), "image/jpeg", true},
#if defined(OS_ANDROID)
    {FILE_PATH_LITERAL("m3u8"), "application/x-mpegurl", true},
#endif
    {FILE_PATH_LITERAL("not an extension / for sure"), "", false},
  };

  std::string mime_type;
  bool rv;

  for (size_t i = 0; i < arraysize(tests); ++i) {
    rv = GetMimeTypeFromExtension(tests[i].extension, &mime_type);
    EXPECT_EQ(tests[i].valid, rv);
    if (rv)
      EXPECT_EQ(tests[i].mime_type, mime_type);
  }
}

TEST(MimeUtilTest, FileTest) {
  const struct {
    const base::FilePath::CharType* file_path;
    const char* const mime_type;
    bool valid;
  } tests[] = {
      {FILE_PATH_LITERAL("c:\\foo\\bar.css"), "text/css", true},
      {FILE_PATH_LITERAL("c:\\foo\\bar.CSS"), "text/css", true},
      {FILE_PATH_LITERAL("c:\\blah"), "", false},
      {FILE_PATH_LITERAL("/usr/local/bin/mplayer"), "", false},
      {FILE_PATH_LITERAL("/home/foo/bar.css"), "text/css", true},
      {FILE_PATH_LITERAL("/blah."), "", false},
      {FILE_PATH_LITERAL("c:\\blah."), "", false},
  };

  std::string mime_type;
  bool rv;

  for (size_t i = 0; i < arraysize(tests); ++i) {
    rv = GetMimeTypeFromFile(base::FilePath(tests[i].file_path),
                                  &mime_type);
    EXPECT_EQ(tests[i].valid, rv);
    if (rv)
      EXPECT_EQ(tests[i].mime_type, mime_type);
  }
}

TEST(MimeUtilTest, LookupTypes) {
#if defined(OS_ANDROID)
  EXPECT_TRUE(IsSupportedMediaMimeType("application/vnd.apple.mpegurl"));
  EXPECT_TRUE(IsSupportedMediaMimeType("application/x-mpegurl"));
  EXPECT_TRUE(IsSupportedMediaMimeType("Application/X-MPEGURL"));
#endif
}

TEST(MimeUtilTest, StrictMediaMimeType) {
  EXPECT_TRUE(IsStrictMediaMimeType("video/webm"));
  EXPECT_TRUE(IsStrictMediaMimeType("Video/WEBM"));
  EXPECT_TRUE(IsStrictMediaMimeType("audio/webm"));

  EXPECT_TRUE(IsStrictMediaMimeType("audio/wav"));
  EXPECT_TRUE(IsStrictMediaMimeType("audio/x-wav"));

  EXPECT_TRUE(IsStrictMediaMimeType("video/ogg"));
  EXPECT_TRUE(IsStrictMediaMimeType("audio/ogg"));
  EXPECT_TRUE(IsStrictMediaMimeType("application/ogg"));

  EXPECT_TRUE(IsStrictMediaMimeType("audio/mpeg"));
  EXPECT_TRUE(IsStrictMediaMimeType("audio/mp3"));
  EXPECT_TRUE(IsStrictMediaMimeType("audio/x-mp3"));

  EXPECT_TRUE(IsStrictMediaMimeType("video/mp4"));
  EXPECT_TRUE(IsStrictMediaMimeType("video/x-m4v"));
  EXPECT_TRUE(IsStrictMediaMimeType("audio/mp4"));
  EXPECT_TRUE(IsStrictMediaMimeType("audio/x-m4a"));

  EXPECT_TRUE(IsStrictMediaMimeType("application/x-mpegurl"));
  EXPECT_TRUE(IsStrictMediaMimeType("application/vnd.apple.mpegurl"));

  EXPECT_FALSE(IsStrictMediaMimeType("video/unknown"));
  EXPECT_FALSE(IsStrictMediaMimeType("Video/UNKNOWN"));
  EXPECT_FALSE(IsStrictMediaMimeType("audio/unknown"));
  EXPECT_FALSE(IsStrictMediaMimeType("application/unknown"));
  EXPECT_FALSE(IsStrictMediaMimeType("unknown/unknown"));
}

TEST(MimeUtilTest, MatchesMimeType) {
  // MIME types are case insensitive.
  EXPECT_TRUE(MatchesMimeType("VIDEO/*", "video/x-mpeg"));
  EXPECT_TRUE(MatchesMimeType("video/*", "VIDEO/X-MPEG"));

  EXPECT_TRUE(MatchesMimeType("*", "video/x-mpeg"));
  EXPECT_TRUE(MatchesMimeType("video/*", "video/x-mpeg"));
  EXPECT_TRUE(MatchesMimeType("video/*", "video/*"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg", "video/x-mpeg"));
  EXPECT_TRUE(MatchesMimeType("application/*+xml",
                                   "application/html+xml"));
  EXPECT_TRUE(MatchesMimeType("application/*+xml", "application/+xml"));
  EXPECT_TRUE(MatchesMimeType("application/*+json",
                                   "application/x-myformat+json"));
  EXPECT_TRUE(MatchesMimeType("aaa*aaa", "aaaaaa"));
  EXPECT_TRUE(MatchesMimeType("*", std::string()));
  EXPECT_FALSE(MatchesMimeType("video/", "video/x-mpeg"));
  EXPECT_FALSE(MatchesMimeType("VIDEO/", "Video/X-MPEG"));
  EXPECT_FALSE(MatchesMimeType(std::string(), "video/x-mpeg"));
  EXPECT_FALSE(MatchesMimeType(std::string(), std::string()));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg", std::string()));
  EXPECT_FALSE(MatchesMimeType("application/*+xml", "application/xml"));
  EXPECT_FALSE(MatchesMimeType("application/*+xml",
                                    "application/html+xmlz"));
  EXPECT_FALSE(MatchesMimeType("application/*+xml",
                                    "applcation/html+xml"));
  EXPECT_FALSE(MatchesMimeType("aaa*aaa", "aaaaa"));

  EXPECT_TRUE(MatchesMimeType("*", "video/x-mpeg;param=val"));
  EXPECT_TRUE(MatchesMimeType("*", "Video/X-MPEG;PARAM=VAL"));
  EXPECT_TRUE(MatchesMimeType("video/*", "video/x-mpeg;param=val"));
  EXPECT_FALSE(MatchesMimeType("video/*;param=val", "video/mpeg"));
  EXPECT_FALSE(MatchesMimeType("Video/*;PARAM=VAL", "VIDEO/Mpeg"));
  EXPECT_FALSE(MatchesMimeType("video/*;param=val", "video/mpeg;param=other"));
  EXPECT_TRUE(MatchesMimeType("video/*;param=val", "video/mpeg;param=val"));
  EXPECT_TRUE(MatchesMimeType("Video/*;PARAM=Val", "VIDEO/Mpeg;Param=Val"));
  EXPECT_FALSE(MatchesMimeType("Video/*;PARAM=VAL", "VIDEO/Mpeg;Param=Val"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg", "video/x-mpeg;param=val"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param=val",
                              "video/x-mpeg;param=val"));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg;param2=val2",
                               "video/x-mpeg;param=val"));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg;param2=val2",
                               "video/x-mpeg;param2=val"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param=val",
                              "video/x-mpeg;param=val;param2=val2"));
  EXPECT_TRUE(MatchesMimeType("Video/X-Mpeg;Param=Val",
                              "VIDEO/X-MPEG;PARAM=Val;PARAM2=val2"));
  EXPECT_TRUE(MatchesMimeType("Video/X-Mpeg;Param=VAL",
                              "VIDEO/X-MPEG;PARAM=VAL;PARAM2=val2"));
  EXPECT_FALSE(MatchesMimeType("Video/X-Mpeg;Param=val",
                               "VIDEO/X-MPEG;PARAM=VAL;PARAM2=val2"));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg;param=VAL;param2=val2",
                               "video/x-mpeg;param=val;param2=val2"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param2=val2;param=val",
                              "video/x-mpeg;param=val;param2=val2"));
  EXPECT_FALSE(MatchesMimeType("video/x-mpeg;param3=val3;param=val",
                               "video/x-mpeg;param=val;param2=val2"));
  EXPECT_TRUE(MatchesMimeType("video/x-mpeg;param=val ;param2=val2 ",
                              "video/x-mpeg;param=val;param2=val2"));

  EXPECT_TRUE(MatchesMimeType("*/*;param=val", "video/x-mpeg;param=val"));
  EXPECT_FALSE(MatchesMimeType("*/*;param=val", "video/x-mpeg;param=val2"));

  EXPECT_TRUE(MatchesMimeType("*", "*"));
  EXPECT_TRUE(MatchesMimeType("*", "*/*"));
  EXPECT_TRUE(MatchesMimeType("*/*", "*/*"));
  EXPECT_TRUE(MatchesMimeType("*/*", "*"));
  EXPECT_TRUE(MatchesMimeType("video/*", "video/*"));
  EXPECT_FALSE(MatchesMimeType("video/*", "*/*"));
  EXPECT_FALSE(MatchesMimeType("video/*;param=val", "video/*"));
  EXPECT_TRUE(MatchesMimeType("video/*;param=val", "video/*;param=val"));
  EXPECT_FALSE(MatchesMimeType("video/*;param=val", "video/*;param=val2"));

  EXPECT_TRUE(MatchesMimeType("ab*cd", "abxxxcd"));
  EXPECT_TRUE(MatchesMimeType("ab*cd", "abx/xcd"));
  EXPECT_TRUE(MatchesMimeType("ab/*cd", "ab/xxxcd"));
}

TEST(MimeUtilTest, CommonMediaMimeType) {
#if defined(OS_ANDROID)
  bool HLSSupported;
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 14)
    HLSSupported = false;
  else
    HLSSupported = true;
#endif

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/webm"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/webm"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/wav"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/x-wav"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/ogg"));
  EXPECT_TRUE(IsSupportedMediaMimeType("application/ogg"));
#if defined(OS_ANDROID)
  EXPECT_FALSE(IsSupportedMediaMimeType("video/ogg"));
  EXPECT_EQ(HLSSupported, IsSupportedMediaMimeType("application/x-mpegurl"));
  EXPECT_EQ(HLSSupported,
            IsSupportedMediaMimeType("application/vnd.apple.mpegurl"));
#else
  EXPECT_TRUE(IsSupportedMediaMimeType("video/ogg"));
  EXPECT_FALSE(IsSupportedMediaMimeType("application/x-mpegurl"));
  EXPECT_FALSE(IsSupportedMediaMimeType("application/vnd.apple.mpegurl"));
#endif  // OS_ANDROID

#if defined(USE_PROPRIETARY_CODECS)
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/mp4"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/x-m4a"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/mp4"));
  EXPECT_TRUE(IsSupportedMediaMimeType("video/x-m4v"));

  EXPECT_TRUE(IsSupportedMediaMimeType("audio/mp3"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/x-mp3"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/mpeg"));
  EXPECT_TRUE(IsSupportedMediaMimeType("audio/aac"));

#if defined(ENABLE_MPEG2TS_STREAM_PARSER)
  EXPECT_TRUE(IsSupportedMediaMimeType("video/mp2t"));
#else
  EXPECT_FALSE(IsSupportedMediaMimeType("video/mp2t"));
#endif
#else
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/mp4"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/x-m4a"));
  EXPECT_FALSE(IsSupportedMediaMimeType("video/mp4"));
  EXPECT_FALSE(IsSupportedMediaMimeType("video/x-m4v"));

  EXPECT_FALSE(IsSupportedMediaMimeType("audio/mp3"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/x-mp3"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/mpeg"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/aac"));
#endif  // USE_PROPRIETARY_CODECS
  EXPECT_FALSE(IsSupportedMediaMimeType("video/mp3"));

  EXPECT_FALSE(IsSupportedMediaMimeType("video/unknown"));
  EXPECT_FALSE(IsSupportedMediaMimeType("audio/unknown"));
  EXPECT_FALSE(IsSupportedMediaMimeType("unknown/unknown"));
}

// Note: codecs should only be a list of 2 or fewer; hence the restriction of
// results' length to 2.
TEST(MimeUtilTest, ParseCodecString) {
  const struct {
    const char* const original;
    size_t expected_size;
    const char* const results[2];
  } tests[] = {
    { "\"bogus\"",                  1, { "bogus" }            },
    { "0",                          1, { "0" }                },
    { "avc1.42E01E, mp4a.40.2",     2, { "avc1",   "mp4a" }   },
    { "\"mp4v.20.240, mp4a.40.2\"", 2, { "mp4v",   "mp4a" }   },
    { "mp4v.20.8, samr",            2, { "mp4v",   "samr" }   },
    { "\"theora, vorbis\"",         2, { "theora", "vorbis" } },
    { "",                           0, { }                    },
    { "\"\"",                       0, { }                    },
    { "\"   \"",                    0, { }                    },
    { ",",                          2, { "", "" }             },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::vector<std::string> codecs_out;
    ParseCodecString(tests[i].original, &codecs_out, true);
    ASSERT_EQ(tests[i].expected_size, codecs_out.size());
    for (size_t j = 0; j < tests[i].expected_size; ++j)
      EXPECT_EQ(tests[i].results[j], codecs_out[j]);
  }

  // Test without stripping the codec type.
  std::vector<std::string> codecs_out;
  ParseCodecString("avc1.42E01E, mp4a.40.2", &codecs_out, false);
  ASSERT_EQ(2u, codecs_out.size());
  EXPECT_EQ("avc1.42E01E", codecs_out[0]);
  EXPECT_EQ("mp4a.40.2", codecs_out[1]);
}

TEST(MimeUtilTest, TestParseMimeTypeWithoutParameter) {
  std::string nonAscii("application/nonutf8");
  EXPECT_TRUE(ParseMimeTypeWithoutParameter(nonAscii, NULL, NULL));
#if defined(OS_WIN)
  nonAscii.append(base::WideToUTF8(L"\u2603"));
#else
  nonAscii.append("\u2603");  // unicode snowman
#endif
  EXPECT_FALSE(ParseMimeTypeWithoutParameter(nonAscii, NULL, NULL));

  std::string top_level_type;
  std::string subtype;
  EXPECT_TRUE(ParseMimeTypeWithoutParameter(
      "application/mime", &top_level_type, &subtype));
  EXPECT_EQ("application", top_level_type);
  EXPECT_EQ("mime", subtype);

  // Various allowed subtype forms.
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("application/json", NULL, NULL));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter(
      "application/x-suggestions+json", NULL, NULL));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("application/+json", NULL, NULL));

  // Upper case letters are allowed.
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("text/mime", NULL, NULL));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("TEXT/mime", NULL, NULL));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("Text/mime", NULL, NULL));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("TeXt/mime", NULL, NULL));

  // Experimental types are also considered to be valid.
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("x-video/mime", NULL, NULL));
  EXPECT_TRUE(ParseMimeTypeWithoutParameter("X-Video/mime", NULL, NULL));

  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text", NULL, NULL));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text/", NULL, NULL));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text/ ", NULL, NULL));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("te(xt/ ", NULL, NULL));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("text/()plain", NULL, NULL));

  EXPECT_FALSE(ParseMimeTypeWithoutParameter("x-video", NULL, NULL));
  EXPECT_FALSE(ParseMimeTypeWithoutParameter("x-video/", NULL, NULL));

  EXPECT_FALSE(ParseMimeTypeWithoutParameter("application/a/b/c", NULL, NULL));

  //EXPECT_TRUE(ParseMimeTypeWithoutParameter("video/mime;parameter"));
}

TEST(MimeUtilTest, TestIsValidTopLevelMimeType) {
  EXPECT_TRUE(IsValidTopLevelMimeType("application"));
  EXPECT_TRUE(IsValidTopLevelMimeType("audio"));
  EXPECT_TRUE(IsValidTopLevelMimeType("example"));
  EXPECT_TRUE(IsValidTopLevelMimeType("image"));
  EXPECT_TRUE(IsValidTopLevelMimeType("message"));
  EXPECT_TRUE(IsValidTopLevelMimeType("model"));
  EXPECT_TRUE(IsValidTopLevelMimeType("multipart"));
  EXPECT_TRUE(IsValidTopLevelMimeType("text"));
  EXPECT_TRUE(IsValidTopLevelMimeType("video"));

  EXPECT_TRUE(IsValidTopLevelMimeType("TEXT"));
  EXPECT_TRUE(IsValidTopLevelMimeType("Text"));
  EXPECT_TRUE(IsValidTopLevelMimeType("TeXt"));

  EXPECT_FALSE(IsValidTopLevelMimeType("mime"));
  EXPECT_FALSE(IsValidTopLevelMimeType(""));
  EXPECT_FALSE(IsValidTopLevelMimeType("/"));
  EXPECT_FALSE(IsValidTopLevelMimeType(" "));

  EXPECT_TRUE(IsValidTopLevelMimeType("x-video"));
  EXPECT_TRUE(IsValidTopLevelMimeType("X-video"));

  EXPECT_FALSE(IsValidTopLevelMimeType("x-"));
}

TEST(MimeUtilTest, TestGetExtensionsForMimeType) {
  const struct {
    const char* const mime_type;
    size_t min_expected_size;
    const char* const contained_result;
  } tests[] = {
    { "text/plain", 2, "txt" },
    { "*",          0, NULL  },
    { "message/*",  1, "eml" },
    { "MeSsAge/*",  1, "eml" },
    { "image/bmp",  1, "bmp" },
    { "video/*",    6, "mp4" },
#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_IOS)
    { "video/*",    6, "mpg" },
#else
    { "video/*",    6, "mpeg" },
#endif
    { "audio/*",    6, "oga" },
    { "aUDIo/*",    6, "wav" },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::vector<base::FilePath::StringType> extensions;
    GetExtensionsForMimeType(tests[i].mime_type, &extensions);
    ASSERT_TRUE(tests[i].min_expected_size <= extensions.size());

    if (!tests[i].contained_result)
      continue;

    bool found = false;
    for (size_t j = 0; !found && j < extensions.size(); ++j) {
#if defined(OS_WIN)
      if (extensions[j] == base::UTF8ToWide(tests[i].contained_result))
        found = true;
#else
      if (extensions[j] == tests[i].contained_result)
        found = true;
#endif
    }
    ASSERT_TRUE(found) << "Must find at least the contained result within "
                       << tests[i].mime_type;
  }
}


TEST(MimeUtilTest, TestAddMultipartValueForUpload) {
  const char ref_output[] =
      "--boundary\r\nContent-Disposition: form-data;"
      " name=\"value name\"\r\nContent-Type: content type"
      "\r\n\r\nvalue\r\n"
      "--boundary\r\nContent-Disposition: form-data;"
      " name=\"value name\"\r\n\r\nvalue\r\n"
      "--boundary--\r\n";
  std::string post_data;
  AddMultipartValueForUpload("value name", "value", "boundary",
                             "content type", &post_data);
  AddMultipartValueForUpload("value name", "value", "boundary",
                             "", &post_data);
  AddMultipartFinalDelimiterForUpload("boundary", &post_data);
  EXPECT_STREQ(ref_output, post_data.c_str());
}

}  // namespace net
