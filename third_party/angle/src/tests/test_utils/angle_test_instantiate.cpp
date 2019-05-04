//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angle_test_instantiate.cpp: Adds support for filtering parameterized
// tests by platform, so we skip unsupported configs.

#include "test_utils/angle_test_instantiate.h"

#include <iostream>
#include <map>

#include "angle_gl.h"
#include "common/platform.h"
#include "test_utils/angle_test_configs.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"
#include "util/system_utils.h"

#if defined(ANGLE_PLATFORM_WINDOWS)
#    include "util/windows/WGLWindow.h"
#endif  // defined(ANGLE_PLATFORM_WINDOWS)

namespace angle
{
namespace
{
bool IsANGLEConfigSupported(const PlatformParameters &param, OSWindow *osWindow)
{
    std::unique_ptr<angle::Library> eglLibrary;

#if defined(ANGLE_USE_UTIL_LOADER)
    eglLibrary.reset(angle::OpenSharedLibrary(ANGLE_EGL_LIBRARY_NAME));
#endif

    EGLWindow *eglWindow =
        EGLWindow::New(param.majorVersion, param.minorVersion, param.eglParameters);
    bool result = eglWindow->initializeGL(osWindow, eglLibrary.get());
    eglWindow->destroyGL();
    EGLWindow::Delete(&eglWindow);
    return result;
}

bool IsWGLConfigSupported(const PlatformParameters &param, OSWindow *osWindow)
{
#if defined(ANGLE_PLATFORM_WINDOWS) && defined(ANGLE_USE_UTIL_LOADER)
    std::unique_ptr<angle::Library> openglLibrary(angle::OpenSharedLibrary("opengl32"));

    WGLWindow *wglWindow = WGLWindow::New(param.majorVersion, param.minorVersion);
    bool result          = wglWindow->initializeGL(osWindow, openglLibrary.get());
    wglWindow->destroyGL();
    WGLWindow::Delete(&wglWindow);
    return result;
#else
    return false;
#endif  // defined(ANGLE_PLATFORM_WINDOWS) && defined(ANGLE_USE_UTIL_LOADER)
}

bool IsNativeConfigSupported(const PlatformParameters &param, OSWindow *osWindow)
{
    // Not yet implemented.
    return false;
}
}  // namespace

bool IsPlatformAvailable(const PlatformParameters &param)
{
    switch (param.getRenderer())
    {
        case EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE:
            break;

        case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
#if !defined(ANGLE_ENABLE_D3D9)
            return false;
#else
            break;
#endif

        case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
#if !defined(ANGLE_ENABLE_D3D11)
            return false;
#else
            break;
#endif

        case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
        case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
#if !defined(ANGLE_ENABLE_OPENGL)
            return false;
#else
            break;
#endif

        case EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE:
#if !defined(ANGLE_ENABLE_VULKAN)
            return false;
#else
            break;
#endif

        case EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE:
#ifndef ANGLE_ENABLE_NULL
            return false;
#endif
            break;

        default:
            std::cout << "Unknown test platform: " << param << std::endl;
            return false;
    }

    static std::map<PlatformParameters, bool> paramAvailabilityCache;
    auto iter = paramAvailabilityCache.find(param);
    if (iter != paramAvailabilityCache.end())
    {
        return iter->second;
    }
    else
    {
        OSWindow *osWindow = OSWindow::New();
        bool result        = osWindow->initialize("CONFIG_TESTER", 1, 1);
        if (result)
        {
            switch (param.driver)
            {
                case GLESDriverType::AngleEGL:
                    result = IsANGLEConfigSupported(param, osWindow);
                    break;
                case GLESDriverType::SystemEGL:
                    result = IsNativeConfigSupported(param, osWindow);
                    break;
                case GLESDriverType::SystemWGL:
                    result = IsWGLConfigSupported(param, osWindow);
                    break;
            }
        }

        osWindow->destroy();
        OSWindow::Delete(&osWindow);

        paramAvailabilityCache[param] = result;

        if (!result)
        {
            std::cout << "Skipping tests using configuration " << param
                      << " because it is not available." << std::endl;
        }

        return result;
    }
}

}  // namespace angle
