// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cronet_url_request_adapter.h"

#include <limits>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/cronet/android/cronet_url_request_context_adapter.h"
#include "jni/CronetUrlRequest_jni.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"

using base::android::ConvertUTF8ToJavaString;

namespace cronet {

// Explicitly register static JNI functions.
bool CronetUrlRequestAdapterRegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong CreateRequestAdapter(JNIEnv* env,
                                  jobject jurl_request,
                                  jlong jurl_request_context_adapter,
                                  jstring jurl_string,
                                  jint jpriority) {
  CronetURLRequestContextAdapter* context_adapter =
      reinterpret_cast<CronetURLRequestContextAdapter*>(
          jurl_request_context_adapter);
  DCHECK(context_adapter);

  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl_string));

  VLOG(1) << "New chromium network request_adapter: "
          << url.possibly_invalid_spec();

  CronetURLRequestAdapter* adapter =
      new CronetURLRequestAdapter(context_adapter, env, jurl_request, url,
                                  static_cast<net::RequestPriority>(jpriority));

  return reinterpret_cast<jlong>(adapter);
}

// net::WrappedIOBuffer subclass for a buffer owned by a Java ByteBuffer. Keeps
// the ByteBuffer alive until destroyed. Uses WrappedIOBuffer because data() is
// owned by the embedder.
class CronetURLRequestAdapter::IOBufferWithByteBuffer
    : public net::WrappedIOBuffer {
 public:
  // Creates a buffer wrapping the Java ByteBuffer |jbyte_buffer|. |data| points
  // to the memory backed by the ByteBuffer, and position is the location to
  // start writing.
  IOBufferWithByteBuffer(
      JNIEnv* env,
      jobject jbyte_buffer,
      void* data,
      int position)
      : net::WrappedIOBuffer(static_cast<char*>(data) + position),
        initial_position_(position) {
    DCHECK(data);
    DCHECK_EQ(env->GetDirectBufferAddress(jbyte_buffer), data);
    byte_buffer_.Reset(env, jbyte_buffer);
  }

  int initial_position() const { return initial_position_; }

  jobject byte_buffer() const { return byte_buffer_.obj(); }

 private:
  ~IOBufferWithByteBuffer() override {}

  base::android::ScopedJavaGlobalRef<jobject> byte_buffer_;

  const int initial_position_;
};

CronetURLRequestAdapter::CronetURLRequestAdapter(
    CronetURLRequestContextAdapter* context,
    JNIEnv* env,
    jobject jurl_request,
    const GURL& url,
    net::RequestPriority priority)
    : context_(context),
      initial_url_(url),
      initial_priority_(priority),
      initial_method_("GET"),
      load_flags_(context->default_load_flags()) {
  DCHECK(!context_->IsOnNetworkThread());
  owner_.Reset(env, jurl_request);
}

CronetURLRequestAdapter::~CronetURLRequestAdapter() {
  DCHECK(context_->IsOnNetworkThread());
}

jboolean CronetURLRequestAdapter::SetHttpMethod(JNIEnv* env,
                                                jobject jcaller,
                                                jstring jmethod) {
  DCHECK(!context_->IsOnNetworkThread());
  std::string method(base::android::ConvertJavaStringToUTF8(env, jmethod));
  // Http method is a token, just as header name.
  if (!net::HttpUtil::IsValidHeaderName(method))
    return JNI_FALSE;
  initial_method_ = method;
  return JNI_TRUE;
}

jboolean CronetURLRequestAdapter::AddRequestHeader(JNIEnv* env,
                                                   jobject jcaller,
                                                   jstring jname,
                                                   jstring jvalue) {
  DCHECK(!context_->IsOnNetworkThread());
  std::string name(base::android::ConvertJavaStringToUTF8(env, jname));
  std::string value(base::android::ConvertJavaStringToUTF8(env, jvalue));
  if (!net::HttpUtil::IsValidHeaderName(name) ||
      !net::HttpUtil::IsValidHeaderValue(value)) {
    return JNI_FALSE;
  }
  initial_request_headers_.SetHeader(name, value);
  return JNI_TRUE;
}

void CronetURLRequestAdapter::DisableCache(JNIEnv* env, jobject jcaller) {
  DCHECK(!context_->IsOnNetworkThread());
  load_flags_ |= net::LOAD_DISABLE_CACHE;
}

void CronetURLRequestAdapter::SetUpload(
    scoped_ptr<net::UploadDataStream> upload) {
  DCHECK(!context_->IsOnNetworkThread());
  DCHECK(!upload_);
  upload_ = upload.Pass();
}

void CronetURLRequestAdapter::Start(JNIEnv* env, jobject jcaller) {
  DCHECK(!context_->IsOnNetworkThread());
  context_->PostTaskToNetworkThread(
      FROM_HERE, base::Bind(&CronetURLRequestAdapter::StartOnNetworkThread,
                            base::Unretained(this)));
}

void CronetURLRequestAdapter::GetStatus(JNIEnv* env,
                                        jobject jcaller,
                                        jobject jstatus_listener) const {
  DCHECK(!context_->IsOnNetworkThread());
  base::android::ScopedJavaGlobalRef<jobject> status_listener_ref;
  status_listener_ref.Reset(env, jstatus_listener);
  context_->PostTaskToNetworkThread(
      FROM_HERE, base::Bind(&CronetURLRequestAdapter::GetStatusOnNetworkThread,
                            base::Unretained(this), status_listener_ref));
}

void CronetURLRequestAdapter::FollowDeferredRedirect(JNIEnv* env,
                                                     jobject jcaller) {
  DCHECK(!context_->IsOnNetworkThread());
  context_->PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(
          &CronetURLRequestAdapter::FollowDeferredRedirectOnNetworkThread,
          base::Unretained(this)));
}

jboolean CronetURLRequestAdapter::ReadData(
    JNIEnv* env, jobject jcaller, jobject jbyte_buffer, jint jposition,
    jint jcapacity) {
  DCHECK(!context_->IsOnNetworkThread());
  DCHECK_LT(jposition, jcapacity);

  void* data = env->GetDirectBufferAddress(jbyte_buffer);
  if (!data)
    return JNI_FALSE;

  scoped_refptr<IOBufferWithByteBuffer> read_buffer(
      new IOBufferWithByteBuffer(env, jbyte_buffer, data, jposition));

  int remaining_capacity = jcapacity - jposition;

  context_->PostTaskToNetworkThread(
      FROM_HERE, base::Bind(&CronetURLRequestAdapter::ReadDataOnNetworkThread,
                            base::Unretained(this),
                            read_buffer,
                            remaining_capacity));
  return JNI_TRUE;
}

void CronetURLRequestAdapter::Destroy(JNIEnv* env, jobject jcaller) {
  // Destroy could be called from any thread, including network thread (if
  // posting task to executor throws an exception), but is posted, so |this|
  // is valid until calling task is complete. Destroy() is always called from
  // within a synchronized java block that guarantees no future posts to the
  // network thread with the adapter pointer.
  context_->PostTaskToNetworkThread(
      FROM_HERE, base::Bind(&CronetURLRequestAdapter::DestroyOnNetworkThread,
                            base::Unretained(this)));
}

void CronetURLRequestAdapter::PopulateResponseHeaders(JNIEnv* env,
                                                      jobject jurl_request,
                                                      jobject jheaders_list) {
  DCHECK(context_->IsOnNetworkThread());
  const net::HttpResponseHeaders* headers = url_request_->response_headers();
  if (headers == nullptr)
    return;

  void* iter = nullptr;
  std::string header_name;
  std::string header_value;
  while (headers->EnumerateHeaderLines(&iter, &header_name, &header_value)) {
    base::android::ScopedJavaLocalRef<jstring> name =
        ConvertUTF8ToJavaString(env, header_name);
    base::android::ScopedJavaLocalRef<jstring> value =
        ConvertUTF8ToJavaString(env, header_value);
    Java_CronetUrlRequest_onAppendResponseHeader(
        env, jurl_request, jheaders_list, name.obj(), value.obj());
  }
}

base::android::ScopedJavaLocalRef<jstring>
CronetURLRequestAdapter::GetHttpStatusText(JNIEnv* env, jobject jcaller) const {
  DCHECK(context_->IsOnNetworkThread());
  const net::HttpResponseHeaders* headers = url_request_->response_headers();
  return ConvertUTF8ToJavaString(env, headers->GetStatusText());
}

base::android::ScopedJavaLocalRef<jstring>
CronetURLRequestAdapter::GetNegotiatedProtocol(JNIEnv* env,
                                               jobject jcaller) const {
  DCHECK(context_->IsOnNetworkThread());
  return ConvertUTF8ToJavaString(
      env, url_request_->response_info().npn_negotiated_protocol);
}

base::android::ScopedJavaLocalRef<jstring>
CronetURLRequestAdapter::GetProxyServer(JNIEnv* env,
                                        jobject jcaller) const {
  DCHECK(context_->IsOnNetworkThread());
  return ConvertUTF8ToJavaString(
      env, url_request_->response_info().proxy_server.ToString());
}

jboolean CronetURLRequestAdapter::GetWasCached(JNIEnv* env,
                                               jobject jcaller) const {
  DCHECK(context_->IsOnNetworkThread());
  return url_request_->response_info().was_cached;
}

// net::URLRequest::Delegate overrides (called on network thread).

void CronetURLRequestAdapter::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  DCHECK(context_->IsOnNetworkThread());
  DCHECK(request->status().is_success());
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onReceivedRedirect(
      env, owner_.obj(),
      ConvertUTF8ToJavaString(env, redirect_info.new_url.spec()).obj(),
      redirect_info.status_code);
  *defer_redirect = true;
}

void CronetURLRequestAdapter::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK(context_->IsOnNetworkThread());
  request->Cancel();
  int net_error = net::MapCertStatusToNetError(ssl_info.cert_status);
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onError(
      env, owner_.obj(), net_error,
      ConvertUTF8ToJavaString(env, net::ErrorToString(net_error)).obj());
}

void CronetURLRequestAdapter::OnResponseStarted(net::URLRequest* request) {
  DCHECK(context_->IsOnNetworkThread());
  if (MaybeReportError(request))
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onResponseStarted(env, owner_.obj(),
                                                  request->GetResponseCode());
}

void CronetURLRequestAdapter::OnReadCompleted(net::URLRequest* request,
                                              int bytes_read) {
  DCHECK(context_->IsOnNetworkThread());
  if (MaybeReportError(request))
    return;
  if (bytes_read != 0) {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_CronetUrlRequest_onReadCompleted(
        env, owner_.obj(), read_buffer_->byte_buffer(), bytes_read,
        read_buffer_->initial_position());
    // Free the read buffer. This lets the Java ByteBuffer be freed, if the
    // embedder releases it, too.
    read_buffer_ = nullptr;
  } else {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_CronetUrlRequest_onSucceeded(
        env, owner_.obj(), url_request_->GetTotalReceivedBytes());
  }
}

void CronetURLRequestAdapter::StartOnNetworkThread() {
  DCHECK(context_->IsOnNetworkThread());
  VLOG(1) << "Starting chromium request: "
          << initial_url_.possibly_invalid_spec().c_str()
          << " priority: " << RequestPriorityToString(initial_priority_);
  url_request_ = context_->GetURLRequestContext()->CreateRequest(
      initial_url_, net::DEFAULT_PRIORITY, this);
  url_request_->SetLoadFlags(load_flags_);
  url_request_->set_method(initial_method_);
  url_request_->SetExtraRequestHeaders(initial_request_headers_);
  url_request_->SetPriority(initial_priority_);
  if (upload_)
    url_request_->set_upload(upload_.Pass());
  url_request_->Start();
}

void CronetURLRequestAdapter::GetStatusOnNetworkThread(
    const base::android::ScopedJavaGlobalRef<jobject>& status_listener_ref)
    const {
  DCHECK(context_->IsOnNetworkThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onStatus(env, owner_.obj(),
                                         status_listener_ref.obj(),
                                         url_request_->GetLoadState().state);
}

void CronetURLRequestAdapter::FollowDeferredRedirectOnNetworkThread() {
  DCHECK(context_->IsOnNetworkThread());
  url_request_->FollowDeferredRedirect();
}

void CronetURLRequestAdapter::ReadDataOnNetworkThread(
    scoped_refptr<IOBufferWithByteBuffer> read_buffer,
    int buffer_size) {
  DCHECK(context_->IsOnNetworkThread());
  DCHECK(read_buffer);
  DCHECK(!read_buffer_);

  read_buffer_ = read_buffer;

  int bytes_read = 0;
  url_request_->Read(read_buffer_.get(), buffer_size, &bytes_read);
  // If IO is pending, wait for the URLRequest to call OnReadCompleted.
  if (url_request_->status().is_io_pending())
    return;

  OnReadCompleted(url_request_.get(), bytes_read);
}

void CronetURLRequestAdapter::DestroyOnNetworkThread() {
  DCHECK(context_->IsOnNetworkThread());
  delete this;
}

bool CronetURLRequestAdapter::MaybeReportError(net::URLRequest* request) const {
  DCHECK_NE(net::URLRequestStatus::IO_PENDING, url_request_->status().status());
  DCHECK_EQ(request, url_request_);
  if (url_request_->status().is_success())
    return false;
  int net_error = url_request_->status().error();
  VLOG(1) << "Error " << net::ErrorToString(net_error)
          << " on chromium request: " << initial_url_.possibly_invalid_spec();
  JNIEnv* env = base::android::AttachCurrentThread();
  cronet::Java_CronetUrlRequest_onError(
      env, owner_.obj(), net_error,
      ConvertUTF8ToJavaString(env, net::ErrorToString(net_error)).obj());
  return true;
}

}  // namespace cronet
