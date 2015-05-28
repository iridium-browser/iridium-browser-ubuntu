/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkFunction_DEFINED
#define SkFunction_DEFINED

// TODO: document

#include "SkTypes.h"
#include "SkTLogic.h"

template <typename> class SkFunction;

template <typename R, typename... Args>
class SkFunction<R(Args...)> : SkNoncopyable {
public:
    SkFunction(R (*fn)(Args...)) : fVTable(GetFunctionPointerVTable()) {
        // We've been passed a function pointer.  We'll just store it.
        fFunction = reinterpret_cast<void*>(fn);
    }

    template <typename Fn>
    SkFunction(Fn fn, SK_WHEN_C((sizeof(Fn) > sizeof(void*)), void*) = nullptr)
            : fVTable(GetOutlineVTable<Fn>()) {
        // We've got a functor larger than a pointer.  We've go to copy it onto the heap.
        fFunction = SkNEW_ARGS(Fn, (Forward(fn)));
    }

    template <typename Fn>
    SkFunction(Fn fn, SK_WHEN_C((sizeof(Fn) <= sizeof(void*)), void*) = nullptr)
            : fVTable(GetInlineVTable<Fn>()) {
        // We've got a functor that fits in a pointer.  We copy it right inline.
        fFunction = NULL;  // Quiets a (spurious) warning that fFunction might be uninitialized.
        SkNEW_PLACEMENT_ARGS(&fFunction, Fn, (Forward(fn)));
    }

    ~SkFunction() { fVTable.fCleanUp(fFunction); }

    R operator()(Args... args) { return fVTable.fCall(fFunction, Forward(args)...); }

private:
    // ~= std::forward.  This moves its argument if possible, falling back to a copy if not.
    template <typename T> static T&& Forward(T& v) { return (T&&)v; }

    struct VTable {
        R (*fCall)(void*, Args...);
        void (*fCleanUp)(void*);
    };

    // Used when fFunction is a function pointer of type R(*)(Args...).
    static const VTable& GetFunctionPointerVTable() {
        static const VTable vtable = {
            [](void* fn, Args... args) {
                return reinterpret_cast<R(*)(Args...)>(fn)(Forward(args)...);
            },
            [](void*) { /* Nothing to clean up for function pointers. */ }
        };
        return vtable;
    }

    // Used when fFunction is a pointer to a functor of type Fn on the heap (we own it).
    template <typename Fn>
    static const VTable& GetOutlineVTable() {
        static const VTable vtable = {
            [](void* fn, Args... args) { return (*static_cast<Fn*>(fn))(Forward(args)...); },
            [](void* fn) { SkDELETE(static_cast<Fn*>(fn)); },
        };
        return vtable;
    }

    // Used when fFunction _is_ a functor of type Fn, not a pointer to the functor.
    template <typename Fn>
    static const VTable& GetInlineVTable() {
        static const VTable vtable = {
            [](void* fn, Args... args) {
                union { void** p; Fn* f; } pun = { &fn };
                return (*pun.f)(Forward(args)...);
            },
            [](void* fn) {
                union { void** p; Fn* f; } pun = { &fn };
                (*pun.f).~Fn();
                (void)(pun.f);   // Otherwise, when ~Fn() is trivial, MSVC complains pun is unused.
            }
        };
        return vtable;
    }


    void* fFunction;        // A function pointer, a pointer to a functor, or an inlined functor.
    const VTable& fVTable;  // How to call, delete (and one day copy, move) fFunction.
};

// TODO:
//   - is it worth moving fCall out of the VTable into SkFunction itself to avoid the indirection?
//   - make SkFunction copyable

#endif//SkFunction_DEFINED
