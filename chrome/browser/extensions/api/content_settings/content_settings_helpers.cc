// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/content_settings/content_settings_helpers.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/url_pattern.h"

namespace {

const char kNoPathWildcardsError[] =
    "Path wildcards in file URL patterns are not allowed.";
const char kNoPathsError[] = "Specific paths are not allowed.";
const char kInvalidPatternError[] = "The pattern \"*\" is invalid.";

const char* const kContentSettingsTypeNames[] = {
  "cookies",
  "images",
  "javascript",
  "plugins",
  "popups",
  "location",
  "notifications",
  "auto-select-certificate",
  "fullscreen",
  "mouselock",
  "mixed-script",
  "media-stream",
  "media-stream-mic",
  "media-stream-camera",
  "register-protocol-handler",
  "ppapi-broker",
  "multiple-automatic-downloads"
};

// TODO(msramek): Assert that |kContentSettingsTypeNames| is synced with
// enum |ContentSettingsType|.
static_assert(arraysize(kContentSettingsTypeNames) <=
              CONTENT_SETTINGS_NUM_TYPES,
              "kContentSettingsTypeNames has an unexpected number of elements");

const char* const kContentSettingNames[] = {
  "default",
  "allow",
  "block",
  "ask",
  "session_only",
  "detect_important_content"
};
static_assert(arraysize(kContentSettingNames) <=
              CONTENT_SETTING_NUM_SETTINGS,
              "kContentSettingNames has an unexpected number of elements");

// TODO(bauerb): Move this someplace where it can be reused.
std::string GetDefaultPort(const std::string& scheme) {
  if (scheme == url::kHttpScheme)
    return "80";
  if (scheme == url::kHttpsScheme)
    return "443";
  NOTREACHED();
  return std::string();
}

}  // namespace

namespace extensions {
namespace content_settings_helpers {

ContentSettingsPattern ParseExtensionPattern(const std::string& pattern_str,
                                             std::string* error) {
  const int kAllowedSchemes =
      URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
      URLPattern::SCHEME_FILE;
  URLPattern url_pattern(kAllowedSchemes);
  URLPattern::ParseResult result = url_pattern.Parse(pattern_str);
  if (result != URLPattern::PARSE_SUCCESS) {
    *error = URLPattern::GetParseResultString(result);
    return ContentSettingsPattern();
  } else {
    scoped_ptr<ContentSettingsPattern::BuilderInterface> builder(
        ContentSettingsPattern::CreateBuilder(false));
    builder->WithHost(url_pattern.host());
    if (url_pattern.match_subdomains())
      builder->WithDomainWildcard();

    std::string scheme = url_pattern.scheme();
    if (scheme == "*")
      builder->WithSchemeWildcard();
    else
      builder->WithScheme(scheme);

    std::string port = url_pattern.port();
    if (port.empty() && scheme != "file") {
      if (scheme == "*")
        port = "*";
      else
        port = GetDefaultPort(scheme);
    }
    if (port == "*")
      builder->WithPortWildcard();
    else
      builder->WithPort(port);

    std::string path = url_pattern.path();
    if (scheme == "file") {
      // For file URLs we allow only exact path matches.
      if (path.find_first_of("*?") != std::string::npos) {
        *error = kNoPathWildcardsError;
        return ContentSettingsPattern();
      } else {
        builder->WithPath(path);
      }
    } else if (path != "/*") {
      // For other URLs we allow only paths which match everything.
      *error = kNoPathsError;
      return ContentSettingsPattern();
    }

    ContentSettingsPattern pattern = builder->Build();
    if (!pattern.IsValid())
      *error = kInvalidPatternError;
    return pattern;
  }
}

ContentSettingsType StringToContentSettingsType(
    const std::string& content_type) {
  for (size_t type = 0; type < arraysize(kContentSettingsTypeNames); ++type) {
    if (content_type == kContentSettingsTypeNames[type])
      return static_cast<ContentSettingsType>(type);
  }
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

const char* ContentSettingsTypeToString(ContentSettingsType type) {
  size_t index = static_cast<size_t>(type);
  DCHECK_LT(index, arraysize(kContentSettingsTypeNames));
  return kContentSettingsTypeNames[index];
}

bool StringToContentSetting(const std::string& setting_str,
                            ContentSetting* setting) {
  for (size_t type = 0; type < arraysize(kContentSettingNames); ++type) {
    if (setting_str == kContentSettingNames[type]) {
      *setting = static_cast<ContentSetting>(type);
      return true;
    }
  }
  return false;
}

const char* ContentSettingToString(ContentSetting setting) {
  size_t index = static_cast<size_t>(setting);
  DCHECK_LT(index, arraysize(kContentSettingNames));
  return kContentSettingNames[index];
}

}  // namespace content_settings_helpers
}  // namespace extensions
