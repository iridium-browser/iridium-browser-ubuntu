// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_H_

#include <stdint.h>

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/lib/associated_interface_ptr_state.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// Represents the client side of an associated interface. It is similar to
// InterfacePtr, except that it doesn't own a message pipe handle.
template <typename Interface>
class AssociatedInterfacePtr {
 public:
  // Constructs an unbound AssociatedInterfacePtr.
  AssociatedInterfacePtr() {}
  AssociatedInterfacePtr(decltype(nullptr)) {}

  AssociatedInterfacePtr(AssociatedInterfacePtr&& other) {
    internal_state_.Swap(&other.internal_state_);
  }

  AssociatedInterfacePtr& operator=(AssociatedInterfacePtr&& other) {
    reset();
    internal_state_.Swap(&other.internal_state_);
    return *this;
  }

  // Assigning nullptr to this class causes it to closes the associated
  // interface (if any) and returns the pointer to the unbound state.
  AssociatedInterfacePtr& operator=(decltype(nullptr)) {
    reset();
    return *this;
  }

  ~AssociatedInterfacePtr() {}

  // Sets up this object as the client side of an associated interface.
  // Calling with an invalid |info| has the same effect as reset(). In this
  // case, the AssociatedInterfacePtr is not considered as bound.
  //
  // |runner| must belong to the same thread. It will be used to dispatch all
  // callbacks and connection error notification. It is useful when you attach
  // multiple task runners to a single thread for the purposes of task
  // scheduling.
  //
  // NOTE: Please see the comments of
  // AssociatedGroup.CreateAssociatedInterface() about when you can use this
  // object to make calls.
  void Bind(AssociatedInterfacePtrInfo<Interface> info,
            scoped_refptr<base::SingleThreadTaskRunner> runner =
                base::ThreadTaskRunnerHandle::Get()) {
    reset();

    bool is_local = info.handle().is_local();

    DCHECK(is_local) << "The AssociatedInterfacePtrInfo is supposed to be used "
                        "at the other side of the message pipe.";

    if (info.is_valid() && is_local)
      internal_state_.Bind(std::move(info), std::move(runner));
  }

  bool is_bound() const { return internal_state_.is_bound(); }

  Interface* get() const { return internal_state_.instance(); }

  // Functions like a pointer to Interface. Must already be bound.
  Interface* operator->() const { return get(); }
  Interface& operator*() const { return *get(); }

  // Returns the version number of the interface that the remote side supports.
  uint32_t version() const { return internal_state_.version(); }

  // Returns the internal interface ID of this associated interface.
  uint32_t interface_id() const { return internal_state_.interface_id(); }

  // Queries the max version that the remote side supports. On completion, the
  // result will be returned as the input of |callback|. The version number of
  // this object will also be updated.
  void QueryVersion(const base::Callback<void(uint32_t)>& callback) {
    internal_state_.QueryVersion(callback);
  }

  // If the remote side doesn't support the specified version, it will close the
  // associated interface asynchronously. This does nothing if it's already
  // known that the remote side supports the specified version, i.e., if
  // |version <= this->version()|.
  //
  // After calling RequireVersion() with a version not supported by the remote
  // side, all subsequent calls to interface methods will be ignored.
  void RequireVersion(uint32_t version) {
    internal_state_.RequireVersion(version);
  }

  // Sends a message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  void FlushForTesting() { internal_state_.FlushForTesting(); }

  // Closes the associated interface (if any) and returns the pointer to the
  // unbound state.
  void reset() {
    State doomed;
    internal_state_.Swap(&doomed);
  }

  // Similar to the method above, but also specifies a disconnect reason.
  void ResetWithReason(uint32_t custom_reason, const std::string& description) {
    if (internal_state_.is_bound())
      internal_state_.SendDisconnectReason(custom_reason, description);
    reset();
  }

  // Indicates whether an error has been encountered. If true, method calls made
  // on this interface will be dropped (and may already have been dropped).
  bool encountered_error() const { return internal_state_.encountered_error(); }

  // Registers a handler to receive error notifications.
  //
  // This method may only be called after the AssociatedInterfacePtr has been
  // bound.
  void set_connection_error_handler(const base::Closure& error_handler) {
    internal_state_.set_connection_error_handler(error_handler);
  }

  void set_connection_error_with_reason_handler(
      const ConnectionErrorWithReasonCallback& error_handler) {
    internal_state_.set_connection_error_with_reason_handler(error_handler);
  }

  // Unbinds and returns the associated interface pointer information which
  // could be used to setup an AssociatedInterfacePtr again. This method may be
  // used to move the proxy to a different thread.
  //
  // It is an error to call PassInterface() while there are pending responses.
  // TODO: fix this restriction, it's not always obvious when there is a
  // pending response.
  AssociatedInterfacePtrInfo<Interface> PassInterface() {
    DCHECK(!internal_state_.has_pending_callbacks());
    State state;
    internal_state_.Swap(&state);

    return state.PassInterface();
  }

  // Returns the associated group that this object belongs to. Returns null if
  // the object is not bound.
  AssociatedGroup* associated_group() {
    return internal_state_.associated_group();
  }

  // DO NOT USE. Exposed only for internal use and for testing.
  internal::AssociatedInterfacePtrState<Interface>* internal_state() {
    return &internal_state_;
  }

  // Allow AssociatedInterfacePtr<> to be used in boolean expressions, but not
  // implicitly convertible to a real bool (which is dangerous).
 private:
  // TODO(dcheng): Use an explicit conversion operator.
  typedef internal::AssociatedInterfacePtrState<Interface>
      AssociatedInterfacePtr::*Testable;

 public:
  operator Testable() const {
    return internal_state_.is_bound() ? &AssociatedInterfacePtr::internal_state_
                                      : nullptr;
  }

 private:
  // Forbid the == and != operators explicitly, otherwise AssociatedInterfacePtr
  // will be converted to Testable to do == or != comparison.
  template <typename T>
  bool operator==(const AssociatedInterfacePtr<T>& other) const = delete;
  template <typename T>
  bool operator!=(const AssociatedInterfacePtr<T>& other) const = delete;

  typedef internal::AssociatedInterfacePtrState<Interface> State;
  mutable State internal_state_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedInterfacePtr);
};

// Creates an associated interface. The output |ptr| should be used locally
// while the returned request should be passed through the message pipe endpoint
// referred to by |associated_group| to setup the corresponding associated
// interface implementation at the remote side.
//
// NOTE: |ptr| should NOT be used to make calls before the request is sent.
// Violating that will cause the message pipe to be closed. On the other hand,
// as soon as the request is sent, |ptr| is usable. There is no need to wait
// until the request is bound to an implementation at the remote side.
template <typename Interface>
AssociatedInterfaceRequest<Interface> MakeRequest(
    AssociatedInterfacePtr<Interface>* ptr,
    AssociatedGroup* group,
    scoped_refptr<base::SingleThreadTaskRunner> runner =
        base::ThreadTaskRunnerHandle::Get()) {
  AssociatedInterfaceRequest<Interface> request;
  AssociatedInterfacePtrInfo<Interface> ptr_info;
  group->CreateAssociatedInterface(AssociatedGroup::WILL_PASS_REQUEST,
                                   &ptr_info, &request);

  ptr->Bind(std::move(ptr_info), std::move(runner));
  return request;
}

// Creates an associated interface proxy in its own AssociatedGroup.
template <typename Interface>
AssociatedInterfaceRequest<Interface> MakeRequestForTesting(
    AssociatedInterfacePtr<Interface>* ptr,
    scoped_refptr<base::SingleThreadTaskRunner> runner =
        base::ThreadTaskRunnerHandle::Get()) {
  MessagePipe pipe;
  using internal::MultiplexRouter;
  scoped_refptr<MultiplexRouter> router0 = new MultiplexRouter(
      std::move(pipe.handle0), MultiplexRouter::MULTI_INTERFACE, true, runner);
  scoped_refptr<MultiplexRouter> router1 = new MultiplexRouter(
      std::move(pipe.handle1), MultiplexRouter::MULTI_INTERFACE, false, runner);

  AssociatedInterfacePtrInfo<Interface> ptr_info;
  AssociatedInterfaceRequest<Interface> request;
  router1->CreateAssociatedGroup()->CreateAssociatedInterface(
      AssociatedGroup::WILL_PASS_PTR, &ptr_info, &request);

  // Emulate passing |ptr_info| across a pipe.
  ScopedInterfaceEndpointHandle handle = ptr_info.PassHandle();
  DCHECK(!handle.is_local());
  ptr->Bind(AssociatedInterfacePtrInfo<Interface>(
                router0->CreateLocalEndpointHandle(handle.release()),
                ptr_info.version()),
            std::move(runner));

  return request;
}

// Like |GetProxy|, but the interface is never associated with any other
// interface. The returned request can be bound directly to the corresponding
// associated interface implementation, without first passing it through a
// message pipe endpoint.
//
// This function has two main uses:
//
//  * In testing, where the returned request is bound to e.g. a mock and there
//    are no other interfaces involved.
//
//  * When discarding messages sent on an interface, which can be done by
//    discarding the returned request.
template <typename Interface>
AssociatedInterfaceRequest<Interface> GetIsolatedProxy(
    AssociatedInterfacePtr<Interface>* ptr) {
  MessagePipe pipe;
  scoped_refptr<internal::MultiplexRouter> router0 =
      new internal::MultiplexRouter(std::move(pipe.handle0),
                                    internal::MultiplexRouter::MULTI_INTERFACE,
                                    false, base::ThreadTaskRunnerHandle::Get());
  scoped_refptr<internal::MultiplexRouter> router1 =
      new internal::MultiplexRouter(std::move(pipe.handle1),
                                    internal::MultiplexRouter::MULTI_INTERFACE,
                                    true, base::ThreadTaskRunnerHandle::Get());

  ScopedInterfaceEndpointHandle endpoint0, endpoint1;
  router0->CreateEndpointHandlePair(&endpoint0, &endpoint1);
  endpoint1 = router1->CreateLocalEndpointHandle(endpoint1.release());

  ptr->Bind(AssociatedInterfacePtrInfo<Interface>(std::move(endpoint0),
                                                  Interface::Version_));

  AssociatedInterfaceRequest<Interface> request;
  request.Bind(std::move(endpoint1));
  return request;
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_H_
