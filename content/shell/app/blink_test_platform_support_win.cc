// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/blink_test_platform_support.h"

#include <windows.h>
#include <stddef.h>
#include <iostream>
#include <list>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/win/direct_write.h"

#define SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(struct_name, member) \
    offsetof(struct_name, member) + \
    (sizeof static_cast<struct_name*>(0)->member)
#define NONCLIENTMETRICS_SIZE_PRE_VISTA \
    SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(NONCLIENTMETRICS, lfMessageFont)

namespace content {

namespace {

bool SetupFonts() {
  // Load Ahem font.
  // AHEM____.TTF is copied to the directory of DumpRenderTree.exe by
  // WebKit.gyp.
  base::FilePath base_path;
  PathService::Get(base::DIR_MODULE, &base_path);
  base::FilePath font_path =
      base_path.Append(FILE_PATH_LITERAL("/AHEM____.TTF"));

  const char kRegisterFontFiles[] = "register-font-files";
  // DirectWrite sandbox registration.
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(kRegisterFontFiles,
                                 base::WideToUTF8(font_path.value()));

  return true;
}

}  // namespace

bool CheckLayoutSystemDeps() {
  std::list<std::string> errors;

  // This metric will be 17 when font size is "Normal".
  // The size of drop-down menus depends on it.
  if (::GetSystemMetrics(SM_CXVSCROLL) != 17)
    errors.push_back("Must use normal size fonts (96 dpi).");

  // Check that we're using the default system fonts.
  OSVERSIONINFO version_info = {0};
  version_info.dwOSVersionInfoSize = sizeof(version_info);
  ::GetVersionEx(&version_info);
  bool is_vista_or_later = (version_info.dwMajorVersion >= 6);
  NONCLIENTMETRICS metrics = {0};
  metrics.cbSize = is_vista_or_later ? sizeof(NONCLIENTMETRICS)
                                     : NONCLIENTMETRICS_SIZE_PRE_VISTA;
  bool success = !!::SystemParametersInfo(
      SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0);
  CHECK(success);
  LOGFONTW* system_fonts[] =
      {&metrics.lfStatusFont, &metrics.lfMenuFont, &metrics.lfSmCaptionFont};
  const wchar_t* required_font = is_vista_or_later ? L"Segoe UI" : L"Tahoma";
  int required_font_size = is_vista_or_later ? -12 : -11;
  for (size_t i = 0; i < arraysize(system_fonts); ++i) {
    if (system_fonts[i]->lfHeight != required_font_size ||
        wcscmp(required_font, system_fonts[i]->lfFaceName)) {
      errors.push_back(is_vista_or_later
                           ? "Must use either the Aero or Basic theme."
                           : "Must use the default XP theme (Luna).");
      break;
    }
  }

  for (std::list<std::string>::iterator it = errors.begin(); it != errors.end();
       ++it) {
    std::cerr << *it << "\n";
  }
  return errors.empty();
}

bool BlinkTestPlatformInitialize() {
  return SetupFonts();
}

}  // namespace content
