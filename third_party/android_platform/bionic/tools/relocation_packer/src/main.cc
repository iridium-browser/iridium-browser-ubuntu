// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tool to pack and unpack relative relocations in a shared library.
//
// Invoke with -v to trace actions taken when packing or unpacking.
// Invoke with -p to pad removed relocations with R_*_NONE.  Suppresses
// shrinking of .rel.dyn.
// See PrintUsage() below for full usage details.
//
// NOTE: Breaks with libelf 0.152, which is buggy.  libelf 0.158 works.

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>

#include "debug.h"
#include "elf_file.h"
#include "elf_traits.h"
#include "libelf.h"

#include "nativehelper/ScopedFd.h"

static void PrintUsage(const char* argv0) {
  std::string temporary = argv0;
  const size_t last_slash = temporary.find_last_of("/");
  if (last_slash != temporary.npos) {
    temporary.erase(0, last_slash + 1);
  }
  const char* basename = temporary.c_str();

  printf(
      "Usage: %s [-u] [-v] [-p] file\n\n"
      "Pack or unpack relative relocations in a shared library.\n\n"
      "  -u, --unpack   unpack previously packed relative relocations\n"
      "  -v, --verbose  trace object file modifications (for debugging)\n"
      "  -p, --pad      do not shrink relocations, but pad (for debugging)\n\n",
      basename);

  printf(
      "Debug sections are not handled, so packing should not be used on\n"
      "shared libraries compiled for debugging or otherwise unstripped.\n");
}

int main(int argc, char* argv[]) {
  bool is_unpacking = false;
  bool is_verbose = false;
  bool is_padding = false;

  static const option options[] = {
    {"unpack", 0, 0, 'u'}, {"verbose", 0, 0, 'v'}, {"pad", 0, 0, 'p'},
    {"help", 0, 0, 'h'}, {NULL, 0, 0, 0}
  };
  bool has_options = true;
  while (has_options) {
    int c = getopt_long(argc, argv, "uvph", options, NULL);
    switch (c) {
      case 'u':
        is_unpacking = true;
        break;
      case 'v':
        is_verbose = true;
        break;
      case 'p':
        is_padding = true;
        break;
      case 'h':
        PrintUsage(argv[0]);
        return 0;
      case '?':
        LOG(INFO) << "Try '" << argv[0] << " --help' for more information.";
        return 1;
      case -1:
        has_options = false;
        break;
      default:
        NOTREACHED();
        return 1;
    }
  }
  if (optind != argc - 1) {
    LOG(INFO) << "Try '" << argv[0] << " --help' for more information.";
    return 1;
  }

  if (elf_version(EV_CURRENT) == EV_NONE) {
    LOG(WARNING) << "Elf Library is out of date!";
  }

  const char* file = argv[argc - 1];
  ScopedFd fd(open(file, O_RDWR));
  if (fd.get() == -1) {
    LOG(ERROR) << file << ": " << strerror(errno);
    return 1;
  }

  if (is_verbose)
    relocation_packer::Logger::SetVerbose(1);

  // We need to detect elf class in order to create
  // correct implementation
  uint8_t e_ident[EI_NIDENT];
  if (TEMP_FAILURE_RETRY(read(fd.get(), e_ident, EI_NIDENT) != EI_NIDENT)) {
    LOG(ERROR) << file << ": failed to read elf header:" << strerror(errno);
    return 1;
  }

  if (TEMP_FAILURE_RETRY(lseek(fd.get(), 0, SEEK_SET)) != 0) {
    LOG(ERROR) << file << ": lseek to 0 failed:" << strerror(errno);
    return 1;
  }

  bool status = false;

  if (e_ident[EI_CLASS] == ELFCLASS32) {
    relocation_packer::ElfFile<ELF32_traits> elf_file(fd.get());
    elf_file.SetPadding(is_padding);

    if (is_unpacking) {
      status = elf_file.UnpackRelocations();
    } else {
      status = elf_file.PackRelocations();
    }
  } else if (e_ident[EI_CLASS] == ELFCLASS64) {
    relocation_packer::ElfFile<ELF64_traits> elf_file(fd.get());
    elf_file.SetPadding(is_padding);

    if (is_unpacking) {
      status = elf_file.UnpackRelocations();
    } else {
      status = elf_file.PackRelocations();
    }
  } else {
    LOG(ERROR) << file << ": unknown ELFCLASS: " << e_ident[EI_CLASS];
    return 1;
  }

  if (!status) {
    LOG(ERROR) << file << ": failed to pack/unpack file";
    return 1;
  }

  return 0;
}
