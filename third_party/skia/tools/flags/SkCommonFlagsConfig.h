/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SK_COMMON_FLAGS_CONFIG_H
#define SK_COMMON_FLAGS_CONFIG_H

#include "SkCommandLineFlags.h"

#if SK_SUPPORT_GPU
#include "GrContextFactory.h"
#endif

DECLARE_string(config);

#if SK_SUPPORT_GPU
class SkCommandLineConfigGpu;
#endif

// SkCommandLineConfig represents a Skia rendering configuration string.
// The string has following form:
// tag:
//   [via-]*backend
// where 'backend' consists of chars excluding hyphen
// and each 'via' consists of chars excluding hyphen.
class SkCommandLineConfig {
  public:
    SkCommandLineConfig(const SkString& tag, const SkString& backend,
                        const SkTArray<SkString>& viaParts);
    virtual ~SkCommandLineConfig();
#if SK_SUPPORT_GPU
    virtual const SkCommandLineConfigGpu* asConfigGpu() const { return nullptr; }
#endif
    const SkString& getTag() const { return fTag; }
    const SkString& getBackend() const { return fBackend; }
    const SkTArray<SkString>& getViaParts() const { return fViaParts; }
  private:
    SkString fTag;
    SkString fBackend;
    SkTArray<SkString> fViaParts;
};

#if SK_SUPPORT_GPU
// SkCommandLineConfigGpu is a SkCommandLineConfig that extracts information out of the backend
// part of the tag. It is constructed tags that have:
// * backends of form "gpu(option=value,option2=value,...)"
// * backends that represent a shorthand of above (such as "msaa16" representing "gpu(samples=16)")
class SkCommandLineConfigGpu : public SkCommandLineConfig {
  public:
    typedef sk_gpu_test::GrContextFactory::ContextType ContextType;
    typedef sk_gpu_test::GrContextFactory::ContextOptions ContextOptions;
    SkCommandLineConfigGpu(const SkString& tag, const SkTArray<SkString>& viaParts,
                           ContextType contextType, bool useNVPR, bool useInstanced, bool useDIText,
                           int samples, SkColorType colorType, sk_sp<SkColorSpace> colorSpace);
    const SkCommandLineConfigGpu* asConfigGpu() const override { return this; }
    ContextType getContextType() const { return fContextType; }
    ContextOptions getContextOptions() const { return fContextOptions; }
    bool getUseNVPR() const { return fContextOptions & ContextOptions::kEnableNVPR; }
    bool getUseInstanced() const { return fContextOptions & ContextOptions::kUseInstanced; }
    bool getUseDIText() const { return fUseDIText; }
    int getSamples() const { return fSamples; }
    SkColorType getColorType() const { return fColorType; }
    SkColorSpace* getColorSpace() const { return fColorSpace.get(); }

  private:
    ContextType fContextType;
    ContextOptions fContextOptions;
    bool fUseDIText;
    int fSamples;
    SkColorType fColorType;
    sk_sp<SkColorSpace> fColorSpace;
};
#endif

typedef SkTArray<std::unique_ptr<SkCommandLineConfig>, true> SkCommandLineConfigArray;
void ParseConfigs(const SkCommandLineFlags::StringArray& configList,
                  SkCommandLineConfigArray* outResult);

#endif
