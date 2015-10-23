/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrProcessor.h"
#include "GrContext.h"
#include "GrCoordTransform.h"
#include "GrGeometryProcessor.h"
#include "GrInvariantOutput.h"
#include "GrMemoryPool.h"
#include "GrXferProcessor.h"
#include "SkSpinlock.h"
#include "gl/GrGLFragmentProcessor.h"

#if SK_ALLOW_STATIC_GLOBAL_INITIALIZERS

class GrFragmentProcessor;
class GrGeometryProcessor;

/*
 * Originally these were both in the processor unit test header, but then it seemed to cause linker
 * problems on android.
 */
template<>
SkTArray<GrProcessorTestFactory<GrFragmentProcessor>*, true>*
GrProcessorTestFactory<GrFragmentProcessor>::GetFactories() {
    static SkTArray<GrProcessorTestFactory<GrFragmentProcessor>*, true> gFactories;
    return &gFactories;
}

template<>
SkTArray<GrProcessorTestFactory<GrXPFactory>*, true>*
GrProcessorTestFactory<GrXPFactory>::GetFactories() {
    static SkTArray<GrProcessorTestFactory<GrXPFactory>*, true> gFactories;
    return &gFactories;
}

template<>
SkTArray<GrProcessorTestFactory<GrGeometryProcessor>*, true>*
GrProcessorTestFactory<GrGeometryProcessor>::GetFactories() {
    static SkTArray<GrProcessorTestFactory<GrGeometryProcessor>*, true> gFactories;
    return &gFactories;
}

/*
 * To ensure we always have successful static initialization, before creating from the factories
 * we verify the count is as expected.  If a new factory is added, then these numbers must be
 * manually adjusted.
 */
static const int kFPFactoryCount = 37;
static const int kGPFactoryCount = 14;
static const int kXPFactoryCount = 5;

template<>
void GrProcessorTestFactory<GrFragmentProcessor>::VerifyFactoryCount() {
    if (kFPFactoryCount != GetFactories()->count()) {
        SkFAIL("Wrong number of fragment processor factories!");
    }
}

template<>
void GrProcessorTestFactory<GrGeometryProcessor>::VerifyFactoryCount() {
    if (kGPFactoryCount != GetFactories()->count()) {
        SkFAIL("Wrong number of geometry processor factories!");
    }
}

template<>
void GrProcessorTestFactory<GrXPFactory>::VerifyFactoryCount() {
    if (kXPFactoryCount != GetFactories()->count()) {
        SkFAIL("Wrong number of xp factory factories!");
    }
}

#endif


// We use a global pool protected by a mutex(spinlock). Chrome may use the same GrContext on
// different threads. The GrContext is not used concurrently on different threads and there is a
// memory barrier between accesses of a context on different threads. Also, there may be multiple
// GrContexts and those contexts may be in use concurrently on different threads.
namespace {
SK_DECLARE_STATIC_SPINLOCK(gProcessorSpinlock);
class MemoryPoolAccessor {
public:
    MemoryPoolAccessor() { gProcessorSpinlock.acquire(); }

    ~MemoryPoolAccessor() { gProcessorSpinlock.release(); }

    GrMemoryPool* pool() const {
        static GrMemoryPool gPool(4096, 4096);
        return &gPool;
    }
};
}

int32_t GrProcessor::gCurrProcessorClassID = GrProcessor::kIllegalProcessorClassID;

///////////////////////////////////////////////////////////////////////////////

GrProcessor::~GrProcessor() {}

void GrProcessor::addTextureAccess(const GrTextureAccess* access) {
    fTextureAccesses.push_back(access);
    this->addGpuResource(access->getProgramTexture());
}

void* GrProcessor::operator new(size_t size) {
    return MemoryPoolAccessor().pool()->allocate(size);
}

void GrProcessor::operator delete(void* target) {
    return MemoryPoolAccessor().pool()->release(target);
}

bool GrProcessor::hasSameTextureAccesses(const GrProcessor& that) const {
    if (this->numTextures() != that.numTextures()) {
        return false;
    }
    for (int i = 0; i < this->numTextures(); ++i) {
        if (this->textureAccess(i) != that.textureAccess(i)) {
            return false;
        }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool GrFragmentProcessor::isEqual(const GrFragmentProcessor& that,
                                  bool ignoreCoordTransforms) const {
    if (this->classID() != that.classID() ||
        !this->hasSameTextureAccesses(that)) {
        return false;
    }
    if (ignoreCoordTransforms) {
        if (this->numTransforms() != that.numTransforms()) {
            return false;
        }
    } else if (!this->hasSameTransforms(that)) {
        return false;
    }
    if (!this->onIsEqual(that)) {
        return false;
    }
    if (this->numChildProcessors() != that.numChildProcessors()) {
        return false;
    }
    for (int i = 0; i < this->numChildProcessors(); ++i) {
        if (!this->childProcessor(i).isEqual(that.childProcessor(i), ignoreCoordTransforms)) {
            return false;
        }
    }
    return true;
}

GrGLFragmentProcessor* GrFragmentProcessor::createGLInstance() const {
    GrGLFragmentProcessor* glFragProc = this->onCreateGLInstance();
    glFragProc->fChildProcessors.push_back_n(fChildProcessors.count());
    for (int i = 0; i < fChildProcessors.count(); ++i) {
        glFragProc->fChildProcessors[i] = fChildProcessors[i].processor()->createGLInstance();
    }
    return glFragProc;
}

void GrFragmentProcessor::addTextureAccess(const GrTextureAccess* textureAccess) {
    // Can't add texture accesses after registering any children since their texture accesses have
    // already been bubbled up into our fTextureAccesses array
    SkASSERT(fChildProcessors.empty());

    INHERITED::addTextureAccess(textureAccess);
    fNumTexturesExclChildren++;
}

void GrFragmentProcessor::addCoordTransform(const GrCoordTransform* transform) {
    // Can't add transforms after registering any children since their transforms have already been
    // bubbled up into our fCoordTransforms array
    SkASSERT(fChildProcessors.empty());

    fCoordTransforms.push_back(transform);
    fUsesLocalCoords = fUsesLocalCoords || transform->sourceCoords() == kLocal_GrCoordSet;
    SkDEBUGCODE(transform->setInProcessor();)
    fNumTransformsExclChildren++;
}

int GrFragmentProcessor::registerChildProcessor(const GrFragmentProcessor* child) {
    // Append the child's transforms to our transforms array and the child's textures array to our
    // textures array
    if (!child->fCoordTransforms.empty()) {
        fCoordTransforms.push_back_n(child->fCoordTransforms.count(),
                                     child->fCoordTransforms.begin());
    }
    if (!child->fTextureAccesses.empty()) {
        fTextureAccesses.push_back_n(child->fTextureAccesses.count(),
                                     child->fTextureAccesses.begin());
    }

    int index = fChildProcessors.count();
    fChildProcessors.push_back(GrFragmentStage(child));

    if (child->willReadFragmentPosition()) {
        this->setWillReadFragmentPosition();
    }

    return index;
}

bool GrFragmentProcessor::hasSameTransforms(const GrFragmentProcessor& that) const {
    if (this->numTransforms() != that.numTransforms()) {
        return false;
    }
    int count = this->numTransforms();
    for (int i = 0; i < count; ++i) {
        if (this->coordTransform(i) != that.coordTransform(i)) {
            return false;
        }
    }
    return true;
}

void GrFragmentProcessor::computeInvariantOutput(GrInvariantOutput* inout) const {
    this->onComputeInvariantOutput(inout);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// Initial static variable from GrXPFactory
int32_t GrXPFactory::gCurrXPFClassID =
        GrXPFactory::kIllegalXPFClassID;

///////////////////////////////////////////////////////////////////////////////////////////////////

// GrProcessorDataManager lives in the same pool
void* GrProcessorDataManager::operator new(size_t size) {
    return MemoryPoolAccessor().pool()->allocate(size);
}

void GrProcessorDataManager::operator delete(void* target) {
    return MemoryPoolAccessor().pool()->release(target);
}
