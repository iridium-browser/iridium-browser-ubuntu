// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/android/system/core_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/android/scoped_java_ref.h"
#include "jni/CoreImpl_jni.h"
#include "mojo/public/c/system/core.h"

namespace mojo {
namespace android {

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static jlong GetTimeTicksNow(JNIEnv* env,
                             const JavaParamRef<jobject>& jcaller) {
  return MojoGetTimeTicksNow();
}

static jint WaitMany(JNIEnv* env,
                     const JavaParamRef<jobject>& jcaller,
                     const JavaParamRef<jobject>& buffer,
                     jlong deadline) {
  // |buffer| contains, in this order
  // input: The array of N handles (MojoHandle, 4 bytes each)
  // input: The array of N signals (MojoHandleSignals, 4 bytes each)
  // space for output: The array of N handle states (MojoHandleSignalsState, 8
  //                   bytes each)
  // space for output: The result index (uint32_t, 4 bytes)
  uint8_t* buffer_start =
      static_cast<uint8_t*>(env->GetDirectBufferAddress(buffer));
  DCHECK(buffer_start);
  DCHECK_EQ(reinterpret_cast<uintptr_t>(buffer_start) % 8, 0u);
  // Each handle of the input array contributes 4 (MojoHandle) + 4
  // (MojoHandleSignals) + 8 (MojoHandleSignalsState) = 16 bytes to the size of
  // the buffer.
  const size_t size_per_handle = 16;
  const size_t buffer_size = env->GetDirectBufferCapacity(buffer);
  DCHECK_EQ((buffer_size - 4) % size_per_handle, 0u);

  const size_t nb_handles = (buffer_size - 4) / size_per_handle;
  const MojoHandle* handle_start =
      reinterpret_cast<const MojoHandle*>(buffer_start);
  const MojoHandleSignals* signals_start =
      reinterpret_cast<const MojoHandleSignals*>(buffer_start + 4 * nb_handles);
  MojoHandleSignalsState* states_start =
      reinterpret_cast<MojoHandleSignalsState*>(buffer_start + 8 * nb_handles);
  uint32_t* result_index =
      reinterpret_cast<uint32_t*>(buffer_start + 16 * nb_handles);
  *result_index = static_cast<uint32_t>(-1);
  return MojoWaitMany(handle_start, signals_start, nb_handles, deadline,
                      result_index, states_start);
}

static ScopedJavaLocalRef<jobject> CreateMessagePipe(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& options_buffer) {
  const MojoCreateMessagePipeOptions* options = NULL;
  if (options_buffer) {
    const void* buffer_start = env->GetDirectBufferAddress(options_buffer);
    DCHECK(buffer_start);
    DCHECK_EQ(reinterpret_cast<const uintptr_t>(buffer_start) % 8, 0u);
    const size_t buffer_size = env->GetDirectBufferCapacity(options_buffer);
    DCHECK_EQ(buffer_size, sizeof(MojoCreateMessagePipeOptions));
    options = static_cast<const MojoCreateMessagePipeOptions*>(buffer_start);
    DCHECK_EQ(options->struct_size, buffer_size);
  }
  MojoHandle handle1;
  MojoHandle handle2;
  MojoResult result = MojoCreateMessagePipe(options, &handle1, &handle2);
  return Java_CoreImpl_newNativeCreationResult(env, result, handle1, handle2);
}

static ScopedJavaLocalRef<jobject> CreateDataPipe(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& options_buffer) {
  const MojoCreateDataPipeOptions* options = NULL;
  if (options_buffer) {
    const void* buffer_start = env->GetDirectBufferAddress(options_buffer);
    DCHECK(buffer_start);
    DCHECK_EQ(reinterpret_cast<const uintptr_t>(buffer_start) % 8, 0u);
    const size_t buffer_size = env->GetDirectBufferCapacity(options_buffer);
    DCHECK_EQ(buffer_size, sizeof(MojoCreateDataPipeOptions));
    options = static_cast<const MojoCreateDataPipeOptions*>(buffer_start);
    DCHECK_EQ(options->struct_size, buffer_size);
  }
  MojoHandle handle1;
  MojoHandle handle2;
  MojoResult result = MojoCreateDataPipe(options, &handle1, &handle2);
  return Java_CoreImpl_newNativeCreationResult(env, result, handle1, handle2);
}

static ScopedJavaLocalRef<jobject> CreateSharedBuffer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& options_buffer,
    jlong num_bytes) {
  const MojoCreateSharedBufferOptions* options = 0;
  if (options_buffer) {
    const void* buffer_start = env->GetDirectBufferAddress(options_buffer);
    DCHECK(buffer_start);
    DCHECK_EQ(reinterpret_cast<const uintptr_t>(buffer_start) % 8, 0u);
    const size_t buffer_size = env->GetDirectBufferCapacity(options_buffer);
    DCHECK_EQ(buffer_size, sizeof(MojoCreateSharedBufferOptions));
    options = static_cast<const MojoCreateSharedBufferOptions*>(buffer_start);
    DCHECK_EQ(options->struct_size, buffer_size);
  }
  MojoHandle handle;
  MojoResult result = MojoCreateSharedBuffer(options, num_bytes, &handle);
  return Java_CoreImpl_newResultAndInteger(env, result, handle);
}

static jint Close(JNIEnv* env,
                  const JavaParamRef<jobject>& jcaller,
                  jint mojo_handle) {
  return MojoClose(mojo_handle);
}

static jint Wait(JNIEnv* env,
                 const JavaParamRef<jobject>& jcaller,
                 const JavaParamRef<jobject>& buffer,
                 jint mojo_handle,
                 jint signals,
                 jlong deadline) {
  // Buffer contains space for the MojoHandleSignalsState
  void* buffer_start = env->GetDirectBufferAddress(buffer);
  DCHECK(buffer_start);
  DCHECK_EQ(reinterpret_cast<const uintptr_t>(buffer_start) % 8, 0u);
  DCHECK_EQ(sizeof(struct MojoHandleSignalsState),
            static_cast<size_t>(env->GetDirectBufferCapacity(buffer)));
  struct MojoHandleSignalsState* signals_state =
      static_cast<struct MojoHandleSignalsState*>(buffer_start);
  return MojoWait(mojo_handle, signals, deadline, signals_state);
}

static jint WriteMessage(JNIEnv* env,
                         const JavaParamRef<jobject>& jcaller,
                         jint mojo_handle,
                         const JavaParamRef<jobject>& bytes,
                         jint num_bytes,
                         const JavaParamRef<jobject>& handles_buffer,
                         jint flags) {
  const void* buffer_start = 0;
  uint32_t buffer_size = 0;
  if (bytes) {
    buffer_start = env->GetDirectBufferAddress(bytes);
    DCHECK(buffer_start);
    DCHECK(env->GetDirectBufferCapacity(bytes) >= num_bytes);
    buffer_size = num_bytes;
  }
  const MojoHandle* handles = 0;
  uint32_t num_handles = 0;
  if (handles_buffer) {
    handles =
        static_cast<MojoHandle*>(env->GetDirectBufferAddress(handles_buffer));
    num_handles = env->GetDirectBufferCapacity(handles_buffer) / 4;
  }
  // Java code will handle invalidating handles if the write succeeded.
  return MojoWriteMessage(
      mojo_handle, buffer_start, buffer_size, handles, num_handles, flags);
}

static ScopedJavaLocalRef<jobject> ReadMessage(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint mojo_handle,
    const JavaParamRef<jobject>& bytes,
    const JavaParamRef<jobject>& handles_buffer,
    jint flags) {
  void* buffer_start = 0;
  uint32_t buffer_size = 0;
  if (bytes) {
    buffer_start = env->GetDirectBufferAddress(bytes);
    DCHECK(buffer_start);
    buffer_size = env->GetDirectBufferCapacity(bytes);
  }
  MojoHandle* handles = 0;
  uint32_t num_handles = 0;
  if (handles_buffer) {
    handles =
        static_cast<MojoHandle*>(env->GetDirectBufferAddress(handles_buffer));
    num_handles = env->GetDirectBufferCapacity(handles_buffer) / 4;
  }
  MojoResult result = MojoReadMessage(
      mojo_handle, buffer_start, &buffer_size, handles, &num_handles, flags);
  // Jave code will handle taking ownership of any received handle.
  return Java_CoreImpl_newReadMessageResult(env, result, buffer_size,
                                            num_handles);
}

static ScopedJavaLocalRef<jobject> ReadData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint mojo_handle,
    const JavaParamRef<jobject>& elements,
    jint elements_capacity,
    jint flags) {
  void* buffer_start = 0;
  uint32_t buffer_size = elements_capacity;
  if (elements) {
    buffer_start = env->GetDirectBufferAddress(elements);
    DCHECK(buffer_start);
    DCHECK(elements_capacity <= env->GetDirectBufferCapacity(elements));
  }
  MojoResult result =
      MojoReadData(mojo_handle, buffer_start, &buffer_size, flags);
  return Java_CoreImpl_newResultAndInteger(
      env, result, (result == MOJO_RESULT_OK) ? buffer_size : 0);
}

static ScopedJavaLocalRef<jobject> BeginReadData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint mojo_handle,
    jint num_bytes,
    jint flags) {
  void const* buffer = 0;
  uint32_t buffer_size = num_bytes;
  MojoResult result =
      MojoBeginReadData(mojo_handle, &buffer, &buffer_size, flags);
  jobject byte_buffer = 0;
  if (result == MOJO_RESULT_OK) {
    byte_buffer =
        env->NewDirectByteBuffer(const_cast<void*>(buffer), buffer_size);
  }
  return Java_CoreImpl_newResultAndBuffer(env, result, byte_buffer);
}

static jint EndReadData(JNIEnv* env,
                        const JavaParamRef<jobject>& jcaller,
                        jint mojo_handle,
                        jint num_bytes_read) {
  return MojoEndReadData(mojo_handle, num_bytes_read);
}

static ScopedJavaLocalRef<jobject> WriteData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint mojo_handle,
    const JavaParamRef<jobject>& elements,
    jint limit,
    jint flags) {
  void* buffer_start = env->GetDirectBufferAddress(elements);
  DCHECK(buffer_start);
  DCHECK(limit <= env->GetDirectBufferCapacity(elements));
  uint32_t buffer_size = limit;
  MojoResult result =
      MojoWriteData(mojo_handle, buffer_start, &buffer_size, flags);
  return Java_CoreImpl_newResultAndInteger(
      env, result, (result == MOJO_RESULT_OK) ? buffer_size : 0);
}

static ScopedJavaLocalRef<jobject> BeginWriteData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint mojo_handle,
    jint num_bytes,
    jint flags) {
  void* buffer = 0;
  uint32_t buffer_size = num_bytes;
  MojoResult result =
      MojoBeginWriteData(mojo_handle, &buffer, &buffer_size, flags);
  jobject byte_buffer = 0;
  if (result == MOJO_RESULT_OK) {
    byte_buffer = env->NewDirectByteBuffer(buffer, buffer_size);
  }
  return Java_CoreImpl_newResultAndBuffer(env, result, byte_buffer);
}

static jint EndWriteData(JNIEnv* env,
                         const JavaParamRef<jobject>& jcaller,
                         jint mojo_handle,
                         jint num_bytes_written) {
  return MojoEndWriteData(mojo_handle, num_bytes_written);
}

static ScopedJavaLocalRef<jobject> Duplicate(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint mojo_handle,
    const JavaParamRef<jobject>& options_buffer) {
  const MojoDuplicateBufferHandleOptions* options = 0;
  if (options_buffer) {
    const void* buffer_start = env->GetDirectBufferAddress(options_buffer);
    DCHECK(buffer_start);
    const size_t buffer_size = env->GetDirectBufferCapacity(options_buffer);
    DCHECK_EQ(buffer_size, sizeof(MojoDuplicateBufferHandleOptions));
    options =
        static_cast<const MojoDuplicateBufferHandleOptions*>(buffer_start);
    DCHECK_EQ(options->struct_size, buffer_size);
  }
  MojoHandle handle;
  MojoResult result = MojoDuplicateBufferHandle(mojo_handle, options, &handle);
  return Java_CoreImpl_newResultAndInteger(env, result, handle);
}

static ScopedJavaLocalRef<jobject> Map(JNIEnv* env,
                                       const JavaParamRef<jobject>& jcaller,
                                       jint mojo_handle,
                                       jlong offset,
                                       jlong num_bytes,
                                       jint flags) {
  void* buffer = 0;
  MojoResult result =
      MojoMapBuffer(mojo_handle, offset, num_bytes, &buffer, flags);
  jobject byte_buffer = 0;
  if (result == MOJO_RESULT_OK) {
    byte_buffer = env->NewDirectByteBuffer(buffer, num_bytes);
  }
  return Java_CoreImpl_newResultAndBuffer(env, result, byte_buffer);
}

static int Unmap(JNIEnv* env,
                 const JavaParamRef<jobject>& jcaller,
                 const JavaParamRef<jobject>& buffer) {
  void* buffer_start = env->GetDirectBufferAddress(buffer);
  DCHECK(buffer_start);
  return MojoUnmapBuffer(buffer_start);
}

static jint GetNativeBufferOffset(JNIEnv* env,
                                  const JavaParamRef<jobject>& jcaller,
                                  const JavaParamRef<jobject>& buffer,
                                  jint alignment) {
  jint offset =
      reinterpret_cast<uintptr_t>(env->GetDirectBufferAddress(buffer)) %
      alignment;
  if (offset == 0)
    return 0;
  return alignment - offset;
}

bool RegisterCoreImpl(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace mojo
