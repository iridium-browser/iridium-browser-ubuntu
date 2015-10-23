// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "config.h"
#import "platform/mac/VersionUtilMac.h"

#include <sstream>
#include <string>
#include <sys/utsname.h>

namespace {

// Returns the running system's Darwin major version. Don't call this, it's
// an implementation detail and its result is meant to be cached by
// MacOSXMinorVersion.
int DarwinMajorVersionInternal()
{
    // The implementation of this method was copied from Chromium, with minor
    // modifications to avoid the use of methods in base/. For further details,
    // see
    // https://code.google.com/p/chromium/codesearch#chromium/src/base/mac/mac_util.mm
    struct utsname unameInfo;
    if (uname(&unameInfo) != 0)
        return 0;

    if (strcmp(unameInfo.sysname, "Darwin") != 0)
        return 0;

    std::string releaseString(unameInfo.release);
    size_t pos = releaseString.find_first_of('.');
    if (pos == std::string::npos)
        return 0;

    std::istringstream convert(releaseString.substr(0, pos));
    int majorVersion;
    if (!(convert >> majorVersion))
        return 0;

    return majorVersion;
}

// Returns the running system's Mac OS X minor version. This is the |y| value
// in 10.y or 10.y.z. Don't call this, it's an implementation detail and the
// result is meant to be cached by MacOSXMinorVersion.
int MacOSXMinorVersionInternal()
{
    int darwinMajorVersion = DarwinMajorVersionInternal();
    return darwinMajorVersion - 4;
}

// Returns the running system's Mac OS X minor version. This is the |y| value
// in 10.y or 10.y.z.
int MacOSXMinorVersion()
{
    static int minorVersion = MacOSXMinorVersionInternal();
    return minorVersion;
}

enum {
    LION_MINOR_VERSION = 7,
    MAVERICKS_MINOR_VERSION = 9,
    YOSEMITE_MINOR_VERSION = 10,
    EL_CAPITAN_MINOR_VERSION = 11,
};

} // namespace

namespace blink {

bool IsOSLionOrEarlier()
{
    return MacOSXMinorVersion() <= LION_MINOR_VERSION;
}

bool IsOSMavericksOrEarlier()
{
    return MacOSXMinorVersion() <= MAVERICKS_MINOR_VERSION;
}

bool IsOSMavericks()
{
    return MacOSXMinorVersion() == MAVERICKS_MINOR_VERSION;
}

bool IsOSYosemite()
{
    return MacOSXMinorVersion() == YOSEMITE_MINOR_VERSION;
}

bool IsOSElCapitan()
{
    return MacOSXMinorVersion() == EL_CAPITAN_MINOR_VERSION;
}

} // namespace blink
