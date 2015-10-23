// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/blink_url_request_type_converters.h"

#include "base/strings/string_util.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/mojo/src/mojo/public/cpp/system/data_pipe.h"

namespace mojo {
namespace {

// Ripped from web_url_loader_impl.cc.
class HeaderFlattener : public blink::WebHTTPHeaderVisitor {
 public:
  HeaderFlattener() : has_accept_header_(false) {}

  void visitHeader(const blink::WebString& name,
                   const blink::WebString& value) override {
    // Headers are latin1.
    const std::string& name_latin1 = name.latin1();
    const std::string& value_latin1 = value.latin1();

    if (base::LowerCaseEqualsASCII(name_latin1, "accept"))
      has_accept_header_ = true;

    HttpHeaderPtr header = HttpHeader::New();
    header->name = name_latin1;
    header->value = value_latin1;
    buffer_.push_back(header.Pass());
  }

  Array<HttpHeaderPtr> GetBuffer() {
    // In some cases, WebKit doesn't add an Accept header, but not having the
    // header confuses some web servers.  See bug 808613.
    if (!has_accept_header_) {
      HttpHeaderPtr header = HttpHeader::New();
      header->name = "Accept";
      header->value = "*/*";
      buffer_.push_back(header.Pass());
      has_accept_header_ = true;
    }
    return buffer_.Pass();
  }

 private:
  Array<HttpHeaderPtr> buffer_;
  bool has_accept_header_;
};

void AddRequestBody(URLRequest* url_request,
                    const blink::WebURLRequest& request) {
  if (request.httpBody().isNull())
    return;

  uint32_t i = 0;
  blink::WebHTTPBody::Element element;
  while (request.httpBody().elementAt(i++, element)) {
    switch (element.type) {
      case blink::WebHTTPBody::Element::TypeData:
        if (!element.data.isEmpty()) {
          // WebKit sometimes gives up empty data to append. These aren't
          // necessary so we just optimize those out here.
          uint32_t num_bytes = static_cast<uint32_t>(element.data.size());
          MojoCreateDataPipeOptions options;
          options.struct_size = sizeof(MojoCreateDataPipeOptions);
          options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
          options.element_num_bytes = 1;
          options.capacity_num_bytes = num_bytes;
          DataPipe data_pipe(options);
          url_request->body.push_back(
              data_pipe.consumer_handle.Pass());
          WriteDataRaw(data_pipe.producer_handle.get(),
                       element.data.data(),
                       &num_bytes,
                       MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
        }
        break;
      case blink::WebHTTPBody::Element::TypeFile:
      case blink::WebHTTPBody::Element::TypeFileSystemURL:
      case blink::WebHTTPBody::Element::TypeBlob:
        // TODO(mpcomplete): handle these.
        NOTIMPLEMENTED();
        break;
      default:
        NOTREACHED();
    }
  }
}

} // namespace

URLRequestPtr TypeConverter<URLRequestPtr, blink::WebURLRequest>::Convert(
    const blink::WebURLRequest& request) {
  URLRequestPtr url_request(URLRequest::New());
  url_request->url = request.url().string().utf8();
  url_request->method = request.httpMethod().utf8();

  HeaderFlattener flattener;
  request.visitHTTPHeaderFields(&flattener);
  url_request->headers = flattener.GetBuffer().Pass();

  AddRequestBody(url_request.get(), request);

  return url_request.Pass();
}

}  // namespace mojo

