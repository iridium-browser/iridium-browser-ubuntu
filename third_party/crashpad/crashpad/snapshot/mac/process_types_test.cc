// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "snapshot/mac/process_types.h"

#include <AvailabilityMacros.h>
#include <mach/mach.h>
#include <string.h>

#include <vector>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/mac/dyld.h"
#include "util/mac/mac_util.h"
#include "util/misc/implicit_cast.h"

namespace crashpad {
namespace test {
namespace {

#define TEST_STRING(process_reader, self_view, proctype_view, field)        \
  do {                                                                      \
    if (self_view->field) {                                                 \
      std::string proctype_string;                                          \
      ASSERT_TRUE(process_reader.Memory()->ReadCString(proctype_view.field, \
                                                       &proctype_string));  \
      EXPECT_EQ(self_view->field, proctype_string);                         \
    }                                                                       \
  } while (false)

TEST(ProcessTypes, DyldImagesSelf) {
  // Get the in-process view of dyld_all_image_infos, and check it for sanity.
  const struct dyld_all_image_infos* self_image_infos =
      _dyld_get_all_image_infos();
  int mac_os_x_minor_version = MacOSXMinorVersion();
  if (mac_os_x_minor_version >= 12) {
    EXPECT_GE(self_image_infos->version, 15u);
  } else if (mac_os_x_minor_version >= 9) {
    EXPECT_GE(self_image_infos->version, 13u);
  } else if (mac_os_x_minor_version >= 7) {
    EXPECT_GE(self_image_infos->version, 8u);
  } else if (mac_os_x_minor_version >= 6) {
    EXPECT_GE(self_image_infos->version, 2u);
  } else {
    EXPECT_GE(self_image_infos->version, 1u);
  }

  EXPECT_GT(self_image_infos->infoArrayCount, 1u);
  if (self_image_infos->version >= 2) {
    EXPECT_TRUE(self_image_infos->libSystemInitialized);
  }
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
  if (self_image_infos->version >= 9) {
    EXPECT_EQ(self_image_infos, self_image_infos->dyldAllImageInfosAddress);
  }
#endif

  // Get the out-of-process view of dyld_all_image_infos, and work with it
  // through the process_types interface.
  task_dyld_info_data_t dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
  kern_return_t kr = task_info(mach_task_self(),
                               TASK_DYLD_INFO,
                               reinterpret_cast<task_info_t>(&dyld_info),
                               &count);
  ASSERT_EQ(KERN_SUCCESS, kr);

  EXPECT_EQ(reinterpret_cast<mach_vm_address_t>(self_image_infos),
            dyld_info.all_image_info_addr);
  EXPECT_GT(dyld_info.all_image_info_size, 1u);

  // This field is only present in the OS X 10.7 SDK (at build time) and kernel
  // (at run time).
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
  if (MacOSXMinorVersion() >= 7) {
#if !defined(ARCH_CPU_64_BITS)
    EXPECT_EQ(TASK_DYLD_ALL_IMAGE_INFO_32, dyld_info.all_image_info_format);
#else
    EXPECT_EQ(TASK_DYLD_ALL_IMAGE_INFO_64, dyld_info.all_image_info_format);
#endif
  }
#endif

  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(mach_task_self()));

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_12
  const uint32_t kDyldAllImageInfosVersionInSDK = 15;
#elif MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_9
  const uint32_t kDyldAllImageInfosVersionInSDK = 14;
#elif MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
  const uint32_t kDyldAllImageInfosVersionInSDK = 12;
#elif MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
  const uint32_t kDyldAllImageInfosVersionInSDK = 7;
#else
  const uint32_t kDyldAllImageInfosVersionInSDK = 1;
#endif

  // Make sure that the size of the structure as declared in the SDK matches the
  // size expected for the version of the structure that the SDK describes.
  EXPECT_EQ(sizeof(dyld_all_image_infos),
            process_types::dyld_all_image_infos::ExpectedSizeForVersion(
                &process_reader, kDyldAllImageInfosVersionInSDK));

  process_types::dyld_all_image_infos proctype_image_infos;
  ASSERT_TRUE(proctype_image_infos.Read(&process_reader,
                                        dyld_info.all_image_info_addr));

  ASSERT_EQ(self_image_infos->version, proctype_image_infos.version);

  if (proctype_image_infos.version >= 1) {
    EXPECT_EQ(self_image_infos->infoArrayCount,
              proctype_image_infos.infoArrayCount);
    EXPECT_EQ(reinterpret_cast<uint64_t>(self_image_infos->infoArray),
              proctype_image_infos.infoArray);
    EXPECT_EQ(reinterpret_cast<uint64_t>(self_image_infos->notification),
              proctype_image_infos.notification);
    EXPECT_EQ(self_image_infos->processDetachedFromSharedRegion,
              proctype_image_infos.processDetachedFromSharedRegion);
  }
  if (proctype_image_infos.version >= 2) {
    EXPECT_EQ(self_image_infos->libSystemInitialized,
              proctype_image_infos.libSystemInitialized);
    EXPECT_EQ(
        reinterpret_cast<uint64_t>(self_image_infos->dyldImageLoadAddress),
        proctype_image_infos.dyldImageLoadAddress);
  }
  if (proctype_image_infos.version >= 3) {
    EXPECT_EQ(reinterpret_cast<uint64_t>(self_image_infos->jitInfo),
              proctype_image_infos.jitInfo);
  }
  if (proctype_image_infos.version >= 5) {
    EXPECT_EQ(reinterpret_cast<uint64_t>(self_image_infos->dyldVersion),
              proctype_image_infos.dyldVersion);
    EXPECT_EQ(reinterpret_cast<uint64_t>(self_image_infos->errorMessage),
              proctype_image_infos.errorMessage);
    EXPECT_EQ(implicit_cast<uint64_t>(self_image_infos->terminationFlags),
              proctype_image_infos.terminationFlags);

    TEST_STRING(
        process_reader, self_image_infos, proctype_image_infos, dyldVersion);
    TEST_STRING(
        process_reader, self_image_infos, proctype_image_infos, errorMessage);
  }
  if (proctype_image_infos.version >= 6) {
    EXPECT_EQ(
        reinterpret_cast<uint64_t>(self_image_infos->coreSymbolicationShmPage),
        proctype_image_infos.coreSymbolicationShmPage);
  }
  if (proctype_image_infos.version >= 7) {
    EXPECT_EQ(implicit_cast<uint64_t>(self_image_infos->systemOrderFlag),
              proctype_image_infos.systemOrderFlag);
  }
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
  if (proctype_image_infos.version >= 8) {
    EXPECT_EQ(implicit_cast<uint64_t>(self_image_infos->uuidArrayCount),
              proctype_image_infos.uuidArrayCount);
  }
  if (proctype_image_infos.version >= 9) {
    EXPECT_EQ(
        reinterpret_cast<uint64_t>(self_image_infos->dyldAllImageInfosAddress),
        proctype_image_infos.dyldAllImageInfosAddress);
  }
  if (proctype_image_infos.version >= 10) {
    EXPECT_EQ(implicit_cast<uint64_t>(self_image_infos->initialImageCount),
              proctype_image_infos.initialImageCount);
  }
  if (proctype_image_infos.version >= 11) {
    EXPECT_EQ(implicit_cast<uint64_t>(self_image_infos->errorKind),
              proctype_image_infos.errorKind);
    EXPECT_EQ(
        reinterpret_cast<uint64_t>(self_image_infos->errorClientOfDylibPath),
        proctype_image_infos.errorClientOfDylibPath);
    EXPECT_EQ(
        reinterpret_cast<uint64_t>(self_image_infos->errorTargetDylibPath),
        proctype_image_infos.errorTargetDylibPath);
    EXPECT_EQ(reinterpret_cast<uint64_t>(self_image_infos->errorSymbol),
              proctype_image_infos.errorSymbol);

    TEST_STRING(process_reader,
                self_image_infos,
                proctype_image_infos,
                errorClientOfDylibPath);
    TEST_STRING(process_reader,
                self_image_infos,
                proctype_image_infos,
                errorTargetDylibPath);
    TEST_STRING(
        process_reader, self_image_infos, proctype_image_infos, errorSymbol);
  }
  if (proctype_image_infos.version >= 12) {
    EXPECT_EQ(implicit_cast<uint64_t>(self_image_infos->sharedCacheSlide),
              proctype_image_infos.sharedCacheSlide);
  }
#endif
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_9
  if (proctype_image_infos.version >= 13) {
    EXPECT_EQ(0,
              memcmp(self_image_infos->sharedCacheUUID,
                     proctype_image_infos.sharedCacheUUID,
                     sizeof(self_image_infos->sharedCacheUUID)));
  }
#endif
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_12
  if (proctype_image_infos.version >= 15) {
    EXPECT_EQ(self_image_infos->infoArrayChangeTimestamp,
              proctype_image_infos.infoArrayChangeTimestamp);
    EXPECT_EQ(self_image_infos->sharedCacheBaseAddress,
              proctype_image_infos.sharedCacheBaseAddress);
    EXPECT_EQ(reinterpret_cast<uint64_t>(self_image_infos->dyldPath),
              proctype_image_infos.dyldPath);
    for (size_t index = 0;
         index < arraysize(self_image_infos->notifyPorts);
         ++index) {
      EXPECT_EQ(self_image_infos->notifyPorts[index],
                proctype_image_infos.notifyPorts[index]) << "index " << index;
    }

    TEST_STRING(
        process_reader, self_image_infos, proctype_image_infos, dyldPath);
  }
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_12
  // As dyld_all_image_infos has evolved over time, new fields were added to the
  // reserved region. process_types::dyld_all_image_infos declares a recent
  // version of the structure, but an older SDK may declare an older version
  // whose |reserved| member appears at a different (smaller) offset than the
  // process_types version. It’s difficult to compare the reserved fields in
  // these older SDKs, so only do it where the declarations match.
  if (proctype_image_infos.version >= 14) {
    for (size_t index = 0;
         index < arraysize(proctype_image_infos.reserved);
         ++index) {
      EXPECT_EQ(implicit_cast<uint64_t>(self_image_infos->reserved[index]),
                proctype_image_infos.reserved[index])
          << "index " << index;
    }
#if defined(ARCH_CPU_64_BITS)
    EXPECT_EQ(self_image_infos->reserved[4], proctype_image_infos.reserved_4);
    EXPECT_EQ(self_image_infos->reserved[5], proctype_image_infos.reserved_5);
    EXPECT_EQ(self_image_infos->reserved[6], proctype_image_infos.reserved_6);
    EXPECT_EQ(self_image_infos->reserved[7], proctype_image_infos.reserved_7);
    EXPECT_EQ(self_image_infos->reserved[8], proctype_image_infos.reserved_8);
#endif
  }
#endif

  if (proctype_image_infos.version >= 1) {
    std::vector<process_types::dyld_image_info> proctype_image_info_vector(
        proctype_image_infos.infoArrayCount);
    ASSERT_TRUE(process_types::dyld_image_info::ReadArrayInto(
        &process_reader,
        proctype_image_infos.infoArray,
        proctype_image_info_vector.size(),
        &proctype_image_info_vector[0]));

    for (size_t index = 0;
         index < proctype_image_infos.infoArrayCount;
         ++index) {
      const dyld_image_info* self_image_info =
          &self_image_infos->infoArray[index];
      const process_types::dyld_image_info& proctype_image_info =
          proctype_image_info_vector[index];

      EXPECT_EQ(reinterpret_cast<uint64_t>(self_image_info->imageLoadAddress),
                proctype_image_info.imageLoadAddress)
          << "index " << index;
      EXPECT_EQ(reinterpret_cast<uint64_t>(self_image_info->imageFilePath),
                proctype_image_info.imageFilePath)
          << "index " << index;
      EXPECT_EQ(implicit_cast<uint64_t>(self_image_info->imageFileModDate),
                proctype_image_info.imageFileModDate)
          << "index " << index;

      TEST_STRING(
          process_reader, self_image_info, proctype_image_info, imageFilePath);
    }
  }

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
  if (proctype_image_infos.version >= 8) {
    std::vector<process_types::dyld_uuid_info> proctype_uuid_info_vector(
        proctype_image_infos.uuidArrayCount);
    ASSERT_TRUE(process_types::dyld_uuid_info::ReadArrayInto(
        &process_reader,
        proctype_image_infos.uuidArray,
        proctype_uuid_info_vector.size(),
        &proctype_uuid_info_vector[0]));

    for (size_t index = 0;
         index < proctype_image_infos.uuidArrayCount;
         ++index) {
      const dyld_uuid_info* self_uuid_info =
          &self_image_infos->uuidArray[index];
      const process_types::dyld_uuid_info& proctype_uuid_info =
          proctype_uuid_info_vector[index];

      EXPECT_EQ(reinterpret_cast<uint64_t>(self_uuid_info->imageLoadAddress),
                proctype_uuid_info.imageLoadAddress)
          << "index " << index;
      EXPECT_EQ(0,
                memcmp(self_uuid_info->imageUUID,
                       proctype_uuid_info.imageUUID,
                       sizeof(self_uuid_info->imageUUID)))
          << "index " << index;
    }
  }
#endif
}

}  // namespace
}  // namespace test
}  // namespace crashpad
