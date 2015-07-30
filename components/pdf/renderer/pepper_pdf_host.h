// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PEPPER_PDF_HOST_H_
#define COMPONENTS_PDF_RENDERER_PEPPER_PDF_HOST_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/serialized_structs.h"

struct PP_ImageDataDesc;
struct PP_Size;
class SkBitmap;

namespace content {
class RendererPpapiHost;
}

namespace ppapi {
class HostResource;
}

namespace ppapi {
namespace host {
struct HostMessageContext;
}
}

namespace pdf {

class PepperPDFHost : public ppapi::host::ResourceHost {
 public:
  class PrintClient {
   public:
    virtual ~PrintClient() {}

    // Returns whether printing is enabled for the plugin instance identified by
    // |instance_id|.
    virtual bool IsPrintingEnabled(PP_Instance instance_id) = 0;

    // Invokes the "Print" command for the plugin instance identified by
    // |instance_id|. Returns whether the "Print" command was issued or not.
    virtual bool Print(PP_Instance instance_id) = 0;
  };

  PepperPDFHost(content::RendererPpapiHost* host,
                PP_Instance instance,
                PP_Resource resource);
  ~PepperPDFHost() override;

  // Invokes the "Print" command for the given instance as if the user right
  // clicked on it and selected "Print". Returns if the "Print" command was
  // issued or not.
  static bool InvokePrintingForInstance(PP_Instance instance);

  // The caller retains the ownership of |print_client|. The client is
  // allowed to be set only once, and when set, the client must outlive the
  // PPB_PDF_Impl instance.
  static void SetPrintClient(PrintClient* print_client);

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

 private:
  int32_t OnHostMsgGetLocalizedString(ppapi::host::HostMessageContext* context,
                                      PP_ResourceString string_id);
  int32_t OnHostMsgDidStartLoading(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgDidStopLoading(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgSetContentRestriction(
      ppapi::host::HostMessageContext* context,
      int restrictions);
  int32_t OnHostMsgUserMetricsRecordAction(
      ppapi::host::HostMessageContext* context,
      const std::string& action);
  int32_t OnHostMsgHasUnsupportedFeature(
      ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgPrint(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgSaveAs(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgSetSelectedText(ppapi::host::HostMessageContext* context,
                                   const base::string16& selected_text);
  int32_t OnHostMsgSetLinkUnderCursor(ppapi::host::HostMessageContext* context,
                                      const std::string& url);

  content::RendererPpapiHost* host_;

  DISALLOW_COPY_AND_ASSIGN(PepperPDFHost);
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PEPPER_PDF_HOST_H_
