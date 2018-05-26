/*
 * Copyright (C) 2016-2018 The Brenwill Workshop Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "ShellMVK.h"
#include <mach/mach_time.h>
#include <mach-o/dyld.h>
#include <cassert>
#include <sstream>
#include <dlfcn.h>
#include "Helpers.h"
#include "Game.h"

PosixTimer::PosixTimer() {
    _tsBase = mach_absolute_time();
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    _tsPeriod = (double)timebase.numer / (double)timebase.denom;
}

double PosixTimer::get() { return (double)(mach_absolute_time() - _tsBase) * _tsPeriod / 1e9; }

ShellMVK::ShellMVK(Game& game) : Shell(game) {
    _timer = PosixTimer();
    _current_time = _timer.get();
    _profile_start_time = _current_time;
    _profile_present_count = 0;

#ifdef VK_USE_PLATFORM_IOS_MVK
    instance_extensions_.push_back(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
#endif
#ifdef VK_USE_PLATFORM_MACOS_MVK
    instance_extensions_.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#endif

    init_vk();
}

ShellMVK::~ShellMVK() {
    destroy_context();
    cleanup_vk();
}

PFN_vkGetInstanceProcAddr ShellMVK::load_vk() {
    const char filename[] = "libvulkan.1.dylib";
    void* handle = NULL;
    void* symbol = NULL;

#ifdef UNINSTALLED_LOADER
    // Try to load the loader from the defined location.
    handle = dlopen(UNINSTALLED_LOADER, RTLD_LAZY);
#endif
    // If still no loader, try in the bundle executable directory.
    if (!handle) {
        unsigned int bufferSize = 512;
        std::vector<char> buffer(bufferSize + 1);
        if (_NSGetExecutablePath(&buffer[0], &bufferSize)) {
            buffer.resize(bufferSize);
            _NSGetExecutablePath(&buffer[0], &bufferSize);
        }
        std::string s = &buffer[0];
        size_t i = s.rfind("smoketest");
        s.replace(i, std::string::npos, filename);
        handle = dlopen(s.c_str(), RTLD_LAZY);
    }
    // If still no luck, try the default system libs with the default lib name.
    if (!handle) handle = dlopen(filename, RTLD_LAZY);

    if (handle) symbol = dlsym(handle, "vkGetInstanceProcAddr");

    if (!handle || !symbol) {
        std::stringstream ss;
        ss << "failed to load " << dlerror();

        if (handle) dlclose(handle);

        throw std::runtime_error(ss.str());
    }

    return reinterpret_cast<PFN_vkGetInstanceProcAddr>(symbol);
}

bool ShellMVK::can_present(VkPhysicalDevice phy, uint32_t queue_family) { return true; }

VkSurfaceKHR ShellMVK::create_surface(VkInstance instance) {
    VkSurfaceKHR surface;

    VkResult err;
#ifdef VK_USE_PLATFORM_IOS_MVK
    VkIOSSurfaceCreateInfoMVK surface_info;
    surface_info.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
    surface_info.pNext = NULL;
    surface_info.flags = 0;
    surface_info.pView = _view;
    err = vk::CreateIOSSurfaceMVK(instance, &surface_info, NULL, &surface);
#endif
#ifdef VK_USE_PLATFORM_MACOS_MVK
    VkMacOSSurfaceCreateInfoMVK surface_info;
    surface_info.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    surface_info.pNext = NULL;
    surface_info.flags = 0;
    surface_info.pView = _view;
    err = vk::CreateMacOSSurfaceMVK(instance, &surface_info, NULL, &surface);
#endif
    assert(!err);

    return surface;
}

void ShellMVK::update_and_draw() {
    acquire_back_buffer();

    double t = _timer.get();
    add_game_time(static_cast<float>(t - _current_time));

    present_back_buffer();

    _current_time = t;

    _profile_present_count++;
    if (_current_time - _profile_start_time >= 5.0) {
        const double fps = _profile_present_count / (_current_time - _profile_start_time);
        std::stringstream ss;
        ss << _profile_present_count << " presents in " << _current_time - _profile_start_time << " seconds "
           << "(FPS: " << fps << ")";
        log(LOG_INFO, ss.str().c_str());

        _profile_start_time = _current_time;
        _profile_present_count = 0;
    }
}

void ShellMVK::run(void* view) {
    _view = view;  // not retained
    create_context();
    resize_swapchain(settings_.initial_width, settings_.initial_height);
}
