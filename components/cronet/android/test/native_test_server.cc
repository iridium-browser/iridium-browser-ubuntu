// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_test_server.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/cronet/android/cronet_url_request_context_adapter.h"
#include "components/cronet/android/url_request_context_adapter.h"
#include "jni/NativeTestServer_jni.h"
#include "net/base/host_port_pair.h"
#include "net/base/url_util.h"
#include "net/dns/host_resolver_impl.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace cronet {

namespace {

const char kEchoBodyPath[] = "/echo_body";
const char kEchoHeaderPath[] = "/echo_header";
const char kEchoAllHeadersPath[] = "/echo_all_headers";
const char kEchoMethodPath[] = "/echo_method";
const char kRedirectToEchoBodyPath[] = "/redirect_to_echo_body";
const char kFakeSdchDomain[] = "fake.sdch.domain";
// Path that advertises the dictionary passed in query params if client
// supports Sdch encoding. E.g. /sdch/index?q=LeQxM80O will make the server
// responds with "Get-Dictionary: /sdch/dict/LeQxM80O".
const char kSdchPath[] = "/sdch/index";
// Path that returns encoded response if client has the right dictionary.
const char kSdchTestPath[] = "/sdch/test";
// Path where dictionaries are stored.
const char kSdchDictPath[] = "/sdch/dict/";

net::test_server::EmbeddedTestServer* g_test_server = nullptr;

class CustomHttpResponse : public net::test_server::HttpResponse {
 public:
  CustomHttpResponse(const std::string& headers, const std::string& contents)
      : headers_(headers), contents_(contents) {}

  std::string ToResponseString() const override {
    return headers_ + "\r\n" + contents_;
  }

  void AddHeader(const std::string& key_value_pair) {
    headers_.append(base::StringPrintf("%s\r\n", key_value_pair.c_str()));
  }

 private:
  std::string headers_;
  std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(CustomHttpResponse);
};

scoped_ptr<CustomHttpResponse> ConstructResponseBasedOnFile(
    const base::FilePath& file_path) {
  std::string file_contents;
  bool read_file = base::ReadFileToString(file_path, &file_contents);
  DCHECK(read_file);
  base::FilePath headers_path(
      file_path.AddExtension(FILE_PATH_LITERAL("mock-http-headers")));
  std::string headers_contents;
  bool read_headers = base::ReadFileToString(headers_path, &headers_contents);
  DCHECK(read_headers);
  scoped_ptr<CustomHttpResponse> http_response(
      new CustomHttpResponse(headers_contents, file_contents));
  return http_response.Pass();
}

scoped_ptr<net::test_server::HttpResponse> NativeTestServerRequestHandler(
    const net::test_server::HttpRequest& request) {
  DCHECK(g_test_server);
  scoped_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_content_type("text/plain");

  if (request.relative_url == kEchoBodyPath) {
    if (request.has_content) {
      response->set_content(request.content);
    } else {
      response->set_content("Request has no body. :(");
    }
    return response.Pass();
  }

  if (base::StartsWith(request.relative_url, kEchoHeaderPath,
                       base::CompareCase::SENSITIVE)) {
    GURL url = g_test_server->GetURL(request.relative_url);
    auto it = request.headers.find(url.query());
    if (it != request.headers.end()) {
      response->set_content(it->second);
    } else {
      response->set_content("Header not found. :(");
    }
    return response.Pass();
  }

  if (request.relative_url == kEchoAllHeadersPath) {
    response->set_content(request.all_headers);
    return response.Pass();
  }

  if (request.relative_url == kEchoMethodPath) {
    response->set_content(request.method_string);
    return response.Pass();
  }

  if (request.relative_url == kRedirectToEchoBodyPath) {
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->AddCustomHeader("Location", kEchoBodyPath);
    return response.Pass();
  }

  // Unhandled requests result in the Embedded test server sending a 404.
  return scoped_ptr<net::test_server::BasicHttpResponse>();
}

scoped_ptr<net::test_server::HttpResponse> SdchRequestHandler(
    const net::test_server::HttpRequest& request) {
  DCHECK(g_test_server);
  base::FilePath dir_path;
  bool get_data_dir = base::android::GetDataDirectory(&dir_path);
  DCHECK(get_data_dir);
  dir_path = dir_path.Append(FILE_PATH_LITERAL("test"));

  if (base::StartsWith(request.relative_url, kSdchPath,
                       base::CompareCase::SENSITIVE)) {
    base::FilePath file_path = dir_path.Append("sdch/index");
    scoped_ptr<CustomHttpResponse> response =
        ConstructResponseBasedOnFile(file_path).Pass();
    // Check for query params to see which dictionary to advertise.
    // For instance, ?q=dictionaryA will make the server advertise dictionaryA.
    GURL url = g_test_server->GetURL(request.relative_url);
    std::string dictionary;
    if (!net::GetValueForKeyInQuery(url, "q", &dictionary)) {
      CHECK(false) << "dictionary is not found in query params of "
                   << request.relative_url;
    }
    auto accept_encoding_header = request.headers.find("Accept-Encoding");
    if (accept_encoding_header != request.headers.end()) {
      if (accept_encoding_header->second.find("sdch") != std::string::npos)
        response->AddHeader(base::StringPrintf(
            "Get-Dictionary: %s%s", kSdchDictPath, dictionary.c_str()));
    }
    return response.Pass();
  }

  if (base::StartsWith(request.relative_url, kSdchTestPath,
                       base::CompareCase::SENSITIVE)) {
    auto avail_dictionary_header = request.headers.find("Avail-Dictionary");
    if (avail_dictionary_header != request.headers.end()) {
      base::FilePath file_path = dir_path.Append(
          "sdch/" + avail_dictionary_header->second + "_encoded");
      return ConstructResponseBasedOnFile(file_path).Pass();
    }
    scoped_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse());
    response->set_content_type("text/plain");
    response->set_content("Sdch is not used.\n");
    return response.Pass();
  }

  // Unhandled requests result in the Embedded test server sending a 404.
  return scoped_ptr<net::test_server::BasicHttpResponse>();
}

void RegisterHostResolverProcHelper(
    net::URLRequestContext* url_request_context) {
  net::HostResolverImpl* resolver =
      static_cast<net::HostResolverImpl*>(url_request_context->host_resolver());
  scoped_refptr<net::RuleBasedHostResolverProc> proc =
      new net::RuleBasedHostResolverProc(NULL);
  proc->AddRule(kFakeSdchDomain, "127.0.0.1");
  resolver->set_proc_params_for_test(
      net::HostResolverImpl::ProcTaskParams(proc.get(), 1u));
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NativeTestServer_onHostResolverProcRegistered(env);
}

void RegisterHostResolverProcOnNetworkThread(
    CronetURLRequestContextAdapter* context_adapter) {
  RegisterHostResolverProcHelper(context_adapter->GetURLRequestContext());
}

// TODO(xunjieli): Delete this once legacy API is removed.
void RegisterHostResolverProcOnNetworkThreadLegacyAPI(
    URLRequestContextAdapter* context_adapter) {
  RegisterHostResolverProcHelper(context_adapter->GetURLRequestContext());
}

}  // namespace

jboolean StartNativeTestServer(JNIEnv* env,
                               jclass jcaller,
                               jstring jtest_files_root) {
  // Shouldn't happen.
  if (g_test_server)
    return false;
  g_test_server = new net::test_server::EmbeddedTestServer();
  g_test_server->RegisterRequestHandler(
      base::Bind(&NativeTestServerRequestHandler));
  g_test_server->RegisterRequestHandler(base::Bind(&SdchRequestHandler));
  base::FilePath test_files_root(
      base::android::ConvertJavaStringToUTF8(env, jtest_files_root));

  // Add a third handler for paths that NativeTestServerRequestHandler does not
  // handle.
  g_test_server->ServeFilesFromDirectory(test_files_root);
  return g_test_server->InitializeAndWaitUntilReady();
}

void RegisterHostResolverProc(JNIEnv* env,
                              jclass jcaller,
                              jlong jadapter,
                              jboolean jlegacy_api) {
  if (jlegacy_api == JNI_TRUE) {
    URLRequestContextAdapter* context_adapter =
        reinterpret_cast<URLRequestContextAdapter*>(jadapter);
    context_adapter->PostTaskToNetworkThread(
        FROM_HERE, base::Bind(&RegisterHostResolverProcOnNetworkThreadLegacyAPI,
                              base::Unretained(context_adapter)));
  } else {
    CronetURLRequestContextAdapter* context_adapter =
        reinterpret_cast<CronetURLRequestContextAdapter*>(jadapter);
    context_adapter->PostTaskToNetworkThread(
        FROM_HERE, base::Bind(&RegisterHostResolverProcOnNetworkThread,
                              base::Unretained(context_adapter)));
  }
}

void ShutdownNativeTestServer(JNIEnv* env, jclass jcaller) {
  if (!g_test_server)
    return;
  delete g_test_server;
  g_test_server = NULL;
}

jstring GetEchoBodyURL(JNIEnv* env, jclass jcaller) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(kEchoBodyPath);
  return base::android::ConvertUTF8ToJavaString(env, url.spec()).Release();
}

jstring GetEchoHeaderURL(JNIEnv* env, jclass jcaller, jstring jheader) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(kEchoHeaderPath);
  GURL::Replacements replacements;
  std::string header = base::android::ConvertJavaStringToUTF8(env, jheader);
  replacements.SetQueryStr(header.c_str());
  url = url.ReplaceComponents(replacements);
  return base::android::ConvertUTF8ToJavaString(env, url.spec()).Release();
}

jstring GetEchoAllHeadersURL(JNIEnv* env, jclass jcaller) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(kEchoAllHeadersPath);
  return base::android::ConvertUTF8ToJavaString(env, url.spec()).Release();
}

jstring GetEchoMethodURL(JNIEnv* env, jclass jcaller) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(kEchoMethodPath);
  return base::android::ConvertUTF8ToJavaString(env, url.spec()).Release();
}

jstring GetRedirectToEchoBody(JNIEnv* env, jclass jcaller) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(kRedirectToEchoBodyPath);
  return base::android::ConvertUTF8ToJavaString(env, url.spec()).Release();
}

jstring GetFileURL(JNIEnv* env, jclass jcaller, jstring jfile_path) {
  DCHECK(g_test_server);
  std::string file = base::android::ConvertJavaStringToUTF8(env, jfile_path);
  GURL url = g_test_server->GetURL(file);
  return base::android::ConvertUTF8ToJavaString(env, url.spec()).Release();
}

jstring GetSdchURL(JNIEnv* env, jclass jcaller) {
  DCHECK(g_test_server);
  std::string url(base::StringPrintf("http://%s:%d", kFakeSdchDomain,
                                     g_test_server->port()));
  return base::android::ConvertUTF8ToJavaString(env, url).Release();
}

jstring GetHostPort(JNIEnv* env, jclass jcaller) {
  DCHECK(g_test_server);
  std::string host_port =
      net::HostPortPair::FromURL(g_test_server->base_url()).ToString();
  return base::android::ConvertUTF8ToJavaString(env, host_port).Release();
}

jboolean IsDataReductionProxySupported(JNIEnv* env, jclass jcaller) {
#if defined(DATA_REDUCTION_PROXY_SUPPORT)
  return JNI_TRUE;
#else
  return JNI_FALSE;
#endif
}

bool RegisterNativeTestServer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace cronet
