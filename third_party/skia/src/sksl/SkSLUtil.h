/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
 
#ifndef SKSL_UTIL
#define SKSL_UTIL

#include "stdlib.h"
#include "assert.h"
#include "SkOpts.h"
#include "SkRefCnt.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkTypes.h"
#include "GrContextOptions.h"
#include "GrShaderCaps.h"

namespace SkSL {

// Various sets of caps for use in tests
class ShaderCapsFactory {
public:
    static sk_sp<GrShaderCaps> Default() {
        sk_sp<GrShaderCaps> result = sk_make_sp<GrShaderCaps>(GrContextOptions());
        result->fVersionDeclString = "#version 400";
        result->fShaderDerivativeSupport = true;
        return result;
    }

    static sk_sp<GrShaderCaps> Version450Core() {
        sk_sp<GrShaderCaps> result = sk_make_sp<GrShaderCaps>(GrContextOptions());
        result->fVersionDeclString = "#version 450 core";
        return result;
    }

    static sk_sp<GrShaderCaps> Version110() {
        sk_sp<GrShaderCaps> result = sk_make_sp<GrShaderCaps>(GrContextOptions());
        result->fVersionDeclString = "#version 110";
        result->fGLSLGeneration = GrGLSLGeneration::k110_GrGLSLGeneration;
        return result;
    }

    static sk_sp<GrShaderCaps> UsesPrecisionModifiers() {
        sk_sp<GrShaderCaps> result = sk_make_sp<GrShaderCaps>(GrContextOptions());
        result->fVersionDeclString = "#version 400";
        result->fUsesPrecisionModifiers = true;
        return result;
    }

    static sk_sp<GrShaderCaps> CannotUseMinAndAbsTogether() {
        sk_sp<GrShaderCaps> result = sk_make_sp<GrShaderCaps>(GrContextOptions());
        result->fVersionDeclString = "#version 400";
        result->fCanUseMinAndAbsTogether = false;
        return result;
    }

    static sk_sp<GrShaderCaps> MustForceNegatedAtanParamToFloat() {
        sk_sp<GrShaderCaps> result = sk_make_sp<GrShaderCaps>(GrContextOptions());
        result->fVersionDeclString = "#version 400";
        result->fMustForceNegatedAtanParamToFloat = true;
        return result;
    }

    static sk_sp<GrShaderCaps> ShaderDerivativeExtensionString() {
        sk_sp<GrShaderCaps> result = sk_make_sp<GrShaderCaps>(GrContextOptions());
        result->fVersionDeclString = "#version 400";
        result->fShaderDerivativeSupport = true;
        result->fShaderDerivativeExtensionString = "GL_OES_standard_derivatives";
        return result;
    }

    static sk_sp<GrShaderCaps> FragCoordsOld() {
        sk_sp<GrShaderCaps> result = sk_make_sp<GrShaderCaps>(GrContextOptions());
        result->fVersionDeclString = "#version 110";
        result->fGLSLGeneration = GrGLSLGeneration::k110_GrGLSLGeneration;
        result->fFragCoordConventionsExtensionString = "GL_ARB_fragment_coord_conventions";
        return result;
    }

    static sk_sp<GrShaderCaps> FragCoordsNew() {
        sk_sp<GrShaderCaps> result = sk_make_sp<GrShaderCaps>(GrContextOptions());
        result->fVersionDeclString = "#version 400";
        result->fFragCoordConventionsExtensionString = "GL_ARB_fragment_coord_conventions";
        return result;
    }

    static sk_sp<GrShaderCaps> VariousCaps() {
        sk_sp<GrShaderCaps> result = sk_make_sp<GrShaderCaps>(GrContextOptions());
        result->fVersionDeclString = "#version 400";
        result->fExternalTextureSupport = true;
        result->fFBFetchSupport = false;
        result->fDropsTileOnZeroDivide = true;
        result->fTexelFetchSupport = true;
        result->fCanUseAnyFunctionInShader = false;
        return result;
    }
};

void write_data(const SkData& d, SkWStream& out);

SkString operator+(const SkString& s, const char* c);

SkString operator+(const char* c, const SkString& s);

SkString operator+(const SkString& s1, const SkString& s2);

bool operator==(const SkString& s1, const char* s2);

bool operator!=(const SkString& s1, const char* s2);

bool operator!=(const char* s1, const SkString& s2);

SkString to_string(double value);

SkString to_string(int32_t value);

SkString to_string(uint32_t value);

SkString to_string(int64_t value);

SkString to_string(uint64_t value);

#if _MSC_VER
#define NORETURN __declspec(noreturn)
#else
#define NORETURN __attribute__((__noreturn__))
#endif
int stoi(SkString s);

double stod(SkString s);

long stol(SkString s);

NORETURN void sksl_abort();

} // namespace

#define ASSERT(x) SkASSERT(x)
#define ASSERT_RESULT(x) SkAssertResult(x);

#ifdef SKIA
#define ABORT(...) { SkDebugf(__VA_ARGS__); sksl_abort(); }
#else
#define ABORT(...) { sksl_abort(); }
#endif

namespace std {
    template<> struct hash<SkString> {
        size_t operator()(const SkString& s) const {
            return SkOpts::hash_fn(s.c_str(), s.size(), 0);
        }
    };
}
#endif
