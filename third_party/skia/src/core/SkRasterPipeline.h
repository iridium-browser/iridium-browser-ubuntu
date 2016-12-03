/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkRasterPipeline_DEFINED
#define SkRasterPipeline_DEFINED

#include "SkNx.h"
#include "SkTArray.h"
#include "SkTypes.h"

/**
 * SkRasterPipeline provides a cheap way to chain together a pixel processing pipeline.
 *
 * It's particularly designed for situations where the potential pipeline is extremely
 * combinatoric: {N dst formats} x {M source formats} x {K mask formats} x {C transfer modes} ...
 * No one wants to write specialized routines for all those combinations, and if we did, we'd
 * end up bloating our code size dramatically.  SkRasterPipeline stages can be chained together
 * at runtime, so we can scale this problem linearly rather than combinatorically.
 *
 * Each stage is represented by a function conforming to a common interface, SkRasterPipeline::Fn,
 * and by an arbitrary context pointer.  Fn's arguments, and sometimes custom calling convention,
 * are designed to maximize the amount of data we can pass along the pipeline cheaply.
 * On many machines all arguments stay in registers the entire time.
 *
 * The meaning of the arguments to Fn are sometimes fixed...
 *    - The Stage* always represents the current stage, mainly providing access to ctx().
 *    - The size_t is always the destination x coordinate.  If you need y, put it in your context.
 *    - By the time the shader's done, the first four vectors should hold source red,
 *      green, blue, and alpha, up to 4 pixels' worth each.
 *
 * ...and sometimes flexible:
 *    - In the shader, the first four vectors can be used for anything, e.g. sample coordinates.
 *    - The last four vectors are scratch registers that can be used to communicate between
 *      stages; transfer modes use these to hold the original destination pixel components.
 *
 * On some platforms the last four vectors are slower to work with than the other arguments.
 *
 * When done mutating its arguments and/or context, a stage can either:
 *   1) call st->next() with its mutated arguments, chaining to the next stage of the pipeline; or
 *   2) return, indicating the pipeline is complete for these pixels.
 *
 * Some obvious stages that typically return are those that write a color to a destination pointer,
 * but any stage can short-circuit the rest of the pipeline by returning instead of calling next().
 *
 * TODO: explain EasyFn and SK_RASTER_STAGE
 */

class SkRasterPipeline {
public:
    struct Stage;
    using Fn = void(SK_VECTORCALL *)(Stage*, size_t, Sk4f,Sk4f,Sk4f,Sk4f,
                                                     Sk4f,Sk4f,Sk4f,Sk4f);
    using EasyFn = void(void*, size_t, Sk4f&, Sk4f&, Sk4f&, Sk4f&,
                                       Sk4f&, Sk4f&, Sk4f&, Sk4f&);

    struct Stage {
        template <typename T>
        T ctx() { return static_cast<T>(fCtx); }

        void SK_VECTORCALL next(size_t x, Sk4f v0, Sk4f v1, Sk4f v2, Sk4f v3,
                                          Sk4f v4, Sk4f v5, Sk4f v6, Sk4f v7) {
            // Stages are logically a pipeline, and physically are contiguous in an array.
            // To get to the next stage, we just increment our pointer to the next array element.
            fNext(this+1, x, v0,v1,v2,v3, v4,v5,v6,v7);
        }

        // It makes next() a good bit cheaper if we hold the next function to call here,
        // rather than logically simpler choice of the function implementing this stage.
        Fn fNext;
        void* fCtx;
    };


    SkRasterPipeline();

    // Run the pipeline constructed with append(), walking x through [x,x+n),
    // generally in 4 pixel steps, but sometimes 1 pixel at a time.
    void run(size_t x, size_t n);
    void run(size_t n) { this->run(0, n); }

    // Use this append() if your stage is sensitive to the number of pixels you're working with:
    //   - body will always be called for a full 4 pixels
    //   - tail will always be called for a single pixel
    // Typically this is only an essential distintion for stages that read or write memory.
    void append(Fn body, const void* body_ctx,
                Fn tail, const void* tail_ctx);

    // Most stages don't actually care if they're working on 4 or 1 pixel.
    void append(Fn fn, const void* ctx = nullptr) {
        this->append(fn, ctx, fn, ctx);
    }

    // Most 4 pixel or 1 pixel variants share the same context pointer.
    void append(Fn body, Fn tail, const void* ctx = nullptr) {
        this->append(body, ctx, tail, ctx);
    }


    // Versions of append that can be used with static EasyFns (see SK_RASTER_STAGE).
    template <EasyFn body, EasyFn tail>
    void append(const void* body_ctx, const void* tail_ctx) {
        this->append(Easy<body>, body_ctx,
                     Easy<tail>, tail_ctx);
    }

    template <EasyFn fn>
    void append(const void* ctx = nullptr) { this->append<fn, fn>(ctx, ctx); }

    template <EasyFn body, EasyFn tail>
    void append(const void* ctx = nullptr) { this->append<body, tail>(ctx, ctx); }


    // Append all stages to this pipeline.
    void extend(const SkRasterPipeline&);

private:
    using Stages = SkSTArray<10, Stage, /*MEM_COPY=*/true>;

    // This no-op default makes fBodyStart and fTailStart unconditionally safe to call,
    // and is always the last stage's fNext as a sort of safety net to make sure even a
    // buggy pipeline can't walk off its own end.
    static void SK_VECTORCALL JustReturn(Stage*, size_t, Sk4f,Sk4f,Sk4f,Sk4f,
                                                         Sk4f,Sk4f,Sk4f,Sk4f);

    template <EasyFn kernel>
    static void SK_VECTORCALL Easy(SkRasterPipeline::Stage* st, size_t x,
                                   Sk4f  r, Sk4f  g, Sk4f  b, Sk4f  a,
                                   Sk4f dr, Sk4f dg, Sk4f db, Sk4f da) {
        kernel(st->ctx<void*>(), x, r,g,b,a, dr,dg,db,da);
        st->next(x, r,g,b,a, dr,dg,db,da);
    }

    Stages fBody,
           fTail;
    Fn fBodyStart = &JustReturn,
       fTailStart = &JustReturn;
};

// These are always static, and we _really_ want them to inline.
// If you find yourself wanting a non-inline stage, write a SkRasterPipeline::Fn directly.
#define SK_RASTER_STAGE(name)                                       \
    static SK_ALWAYS_INLINE void name(void* ctx, size_t x,          \
                            Sk4f&  r, Sk4f&  g, Sk4f&  b, Sk4f&  a, \
                            Sk4f& dr, Sk4f& dg, Sk4f& db, Sk4f& da)

#endif//SkRasterPipeline_DEFINED
