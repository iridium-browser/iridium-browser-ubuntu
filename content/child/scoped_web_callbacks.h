// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SCOPED_WEB_CALLBACKS_H_
#define CONTENT_CHILD_SCOPED_WEB_CALLBACKS_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/move.h"
#include "third_party/WebKit/public/platform/WebCallbacks.h"

// A ScopedWebCallbacks is a move-only scoper which helps manage the lifetime of
// a blink::WebCallbacks object. This is particularly useful when you're
// simultaneously dealing with the following two conditions:
//
//   1. Your WebCallbacks implementation requires either onSuccess or onError to
//      be called before it's destroyed. This is the case with
//      CallbackPromiseAdapter for example, because its underlying
//      ScriptPromiseResolver must be resolved or rejected before destruction.
//
//   2. You are passing ownership of the WebCallbacks to code which may
//      silenty drop it. A common way for this to happen is to bind the
//      WebCallbacks as an argument to a base::Callback which gets destroyed
//      before it can run.
//
// While it's possible to individually track the lifetime of pending
// WebCallbacks, this becomes cumbersome when dealing with many different
// callbacks types. ScopedWebCallbacks provides a generic and relatively
// lightweight solution to this problem.
//
// Example usage:
//
//   using FooCallbacks = blink::WebCallbacks<const Foo&, const FooError&>;
//
//   void RespondWithSuccess(ScopedWebCallbacks<FooCallbacks> callbacks) {
//     callbacks.PassCallbacks()->onSuccess(Foo("everything is great"));
//   }
//
//   void OnCallbacksDropped(scoped_ptr<FooCallbacks> callbacks) {
//     // Ownership of the FooCallbacks is passed to this function if
//     // ScopedWebCallbacks::PassCallbacks isn't called before the
//     // ScopedWebCallbacks is destroyed.
//     callbacks->onError(FooError("everything is terrible"));
//   }
//
//   // Blink client implementation
//   void FooClientImpl::doMagic(FooCallbacks* callbacks) {
//     auto scoped_callbacks = make_scoped_web_callbacks(
//         callbacks, base::Bind(&OnCallbacksDropped));
//
//     // Call to some lower-level service which may never run the callback we
//     // give it.
//     foo_service_->DoMagic(base::Bind(&RespondWithSuccess,
//                                      base::Passed(&scoped_callbacks)));
//   }
//
// If the bound RespondWithSuccess callback actually runs, PassCallbacks() will
// reliquish ownership of the WebCallbacks object to a temporary scoped_ptr
// which will be destroyed immediately after onSuccess is called.
//
// If the bound RespondWithSuccess callback is instead destroyed first,
// the ScopedWebCallbacks destructor will invoke OnCallbacksDropped, executing
// our desired default behavior before deleting the WebCallbacks.
template <typename CallbacksType>
class ScopedWebCallbacks {
  MOVE_ONLY_TYPE_FOR_CPP_03(ScopedWebCallbacks, RValue);

 public:
  using DestructionCallback =
      base::Callback<void(scoped_ptr<CallbacksType> callbacks)>;

  ScopedWebCallbacks(scoped_ptr<CallbacksType> callbacks,
                     const DestructionCallback& destruction_callback)
      : callbacks_(callbacks.Pass()),
        destruction_callback_(destruction_callback) {}

  ~ScopedWebCallbacks() {
    if (callbacks_)
      destruction_callback_.Run(callbacks_.Pass());
  }

  ScopedWebCallbacks(RValue other) { *this = other; }

  ScopedWebCallbacks& operator=(RValue other) {
    callbacks_ = other.object->callbacks_.Pass();
    destruction_callback_ = other.object->destruction_callback_;
    return *this;
  }

  scoped_ptr<CallbacksType> PassCallbacks() { return callbacks_.Pass(); }

 private:
  scoped_ptr<CallbacksType> callbacks_;
  DestructionCallback destruction_callback_;
};

template <typename CallbacksType>
ScopedWebCallbacks<CallbacksType> make_scoped_web_callbacks(
    CallbacksType* callbacks,
    const typename ScopedWebCallbacks<CallbacksType>::DestructionCallback&
        destruction_callback) {
  return ScopedWebCallbacks<CallbacksType>(make_scoped_ptr(callbacks),
                                           destruction_callback);
}

#endif  // CONTENT_CHILD_SCOPED_WEB_CALLBACKS_H_
