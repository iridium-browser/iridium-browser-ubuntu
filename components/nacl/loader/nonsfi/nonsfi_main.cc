// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nonsfi/nonsfi_main.h"

#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "native_client/src/include/elf_auxv.h"

#if defined(OS_NACL_NONSFI)
#include "native_client/src/public/nonsfi/elf_loader.h"
#include "ppapi/nacl_irt/irt_interfaces.h"
#else
#include "base/memory/scoped_ptr.h"
#include "components/nacl/loader/nonsfi/elf_loader.h"
#include "components/nacl/loader/nonsfi/irt_interfaces.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/public/nacl_desc.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#endif

namespace nacl {
namespace nonsfi {
namespace {

typedef void (*EntryPointType)(uintptr_t*);

class PluginMainDelegate : public base::PlatformThread::Delegate {
 public:
  explicit PluginMainDelegate(EntryPointType entry_point)
      : entry_point_(entry_point) {
  }

  ~PluginMainDelegate() override {}

  void ThreadMain() override {
    base::PlatformThread::SetName("NaClMainThread");

    // This will only happen once per process, so we give the permission to
    // create Singletons.
    base::ThreadRestrictions::SetSingletonAllowed(true);
    uintptr_t info[] = {
      0,  // Do not use fini.
      0,  // envc.
      0,  // argc.
      0,  // Null terminate for argv.
      0,  // Null terminate for envv.
      AT_SYSINFO,
#if defined(OS_NACL_NONSFI)
      reinterpret_cast<uintptr_t>(&chrome_irt_query),
#else
      reinterpret_cast<uintptr_t>(&NaClIrtInterface),
#endif
      AT_NULL,
      0,  // Null terminate for auxv.
    };
    entry_point_(info);
  }

 private:
  EntryPointType entry_point_;
};

// Default stack size of the plugin main thread. We heuristically chose 16M.
const size_t kStackSize = (16 << 20);

#if !defined(OS_NACL_NONSFI)
struct NaClDescUnrefer {
  void operator()(struct NaClDesc* desc) const {
    NaClDescUnref(desc);
  }
};
#endif

}  // namespace

void MainStart(int nexe_file) {
#if defined(OS_NACL_NONSFI)
  EntryPointType entry_point =
      reinterpret_cast<EntryPointType>(NaClLoadElfFile(nexe_file));
#else
  ::scoped_ptr<struct NaClDesc, NaClDescUnrefer> desc(
       NaClDescIoMakeFromHandle(nexe_file, NACL_ABI_O_RDONLY));
  ElfImage image;
  if (image.Read(desc.get()) != LOAD_OK) {
    LOG(ERROR) << "LoadModuleRpc: Failed to read binary.";
    return;
  }

  if (image.Load(desc.get()) != LOAD_OK) {
    LOG(ERROR) << "LoadModuleRpc: Failed to load the image";
    return;
  }

  EntryPointType entry_point =
      reinterpret_cast<EntryPointType>(image.entry_point());
#endif
  if (!base::PlatformThread::CreateNonJoinable(
          kStackSize, new PluginMainDelegate(entry_point))) {
    LOG(ERROR) << "LoadModuleRpc: Failed to create plugin main thread.";
    return;
  }
}

}  // namespace nonsfi
}  // namespace nacl
