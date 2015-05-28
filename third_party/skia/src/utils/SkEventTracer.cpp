/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkEventTracer.h"
#include "SkOnce.h"

class SkDefaultEventTracer: public SkEventTracer {
    virtual SkEventTracer::Handle
        addTraceEvent(char phase,
                      const uint8_t* categoryEnabledFlag,
                      const char* name,
                      uint64_t id,
                      int numArgs,
                      const char** argNames,
                      const uint8_t* argTypes,
                      const uint64_t* argValues,
                      uint8_t flags) override { return 0; }

    virtual void
        updateTraceEventDuration(const uint8_t* categoryEnabledFlag,
                                 const char* name,
                                 SkEventTracer::Handle handle) override {};

    const uint8_t* getCategoryGroupEnabled(const char* name) override {
        static uint8_t no = 0;
        return &no;
    };
    virtual const char* getCategoryGroupName(
      const uint8_t* categoryEnabledFlag) override {
        static const char* dummy = "dummy";
        return dummy;
    };
};

SkEventTracer* SkEventTracer::gInstance;

static void cleanup_tracer() {
    // calling SetInstance will delete the existing instance.
    SkEventTracer::SetInstance(NULL);
}

static void intialize_default_tracer(SkEventTracer* current_instance) {
    if (NULL == current_instance) {
        SkEventTracer::SetInstance(SkNEW(SkDefaultEventTracer));
    }
    atexit(cleanup_tracer);
}


SK_DECLARE_STATIC_ONCE(once);
SkEventTracer* SkEventTracer::GetInstance() {
    SkOnce(&once, intialize_default_tracer, SkEventTracer::gInstance);
    SkASSERT(SkEventTracer::gInstance);
    return SkEventTracer::gInstance;
}
