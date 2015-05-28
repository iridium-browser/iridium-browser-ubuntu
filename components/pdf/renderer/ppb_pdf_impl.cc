// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/ppb_pdf_impl.h"

#include "base/files/scoped_file.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local.h"
#include "build/build_config.h"
#include "components/pdf/common/pdf_messages.h"
#include "components/pdf/renderer/pdf_resource_util.h"
#include "content/public/common/child_process_sandbox_support_linux.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "gin/public/isolate_holder.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/icu/source/i18n/unicode/usearch.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace pdf {
namespace {

// --single-process model may fail in CHECK(!g_print_client) if there exist
// more than two RenderThreads, so here we use TLS for g_print_client.
// See http://crbug.com/457580.
base::LazyInstance<base::ThreadLocalPointer<PPB_PDF_Impl::PrintClient> >::Leaky
    g_print_client_tls = LAZY_INSTANCE_INITIALIZER;

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
class PrivateFontFile : public ppapi::Resource {
 public:
  PrivateFontFile(PP_Instance instance, int fd)
      : Resource(ppapi::OBJECT_IS_IMPL, instance), fd_(fd) {}

  bool GetFontTable(uint32_t table, void* output, uint32_t* output_length) {
    size_t temp_size = static_cast<size_t>(*output_length);
    bool rv = content::GetFontTable(fd_.get(),
                                    table,
                                    0 /* offset */,
                                    static_cast<uint8_t*>(output),
                                    &temp_size);
    *output_length = base::checked_cast<uint32_t>(temp_size);
    return rv;
  }

 protected:
  virtual ~PrivateFontFile() {}

 private:
  base::ScopedFD fd_;
};
#endif

PP_Var GetLocalizedString(PP_Instance instance_id,
                          PP_ResourceString string_id) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return PP_MakeUndefined();

  std::string rv = GetStringResource(string_id);
  return ppapi::StringVar::StringToPPVar(rv);
}

PP_Resource GetFontFileWithFallback(
    PP_Instance instance_id,
    const PP_BrowserFont_Trusted_Description* description,
    PP_PrivateFontCharset charset) {
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // Validate the instance before using it below.
  if (!content::PepperPluginInstance::Get(instance_id))
    return 0;

  scoped_refptr<ppapi::StringVar> face_name(
      ppapi::StringVar::FromPPVar(description->face));
  if (!face_name.get())
    return 0;

  int fd = content::MatchFontWithFallback(
      face_name->value().c_str(),
      description->weight >= PP_BROWSERFONT_TRUSTED_WEIGHT_BOLD,
      description->italic,
      charset,
      description->family);
  if (fd == -1)
    return 0;

  scoped_refptr<PrivateFontFile> font(new PrivateFontFile(instance_id, fd));

  return font->GetReference();
#else
  // For trusted PPAPI plugins, this is only needed in Linux since font loading
  // on Windows and Mac works through the renderer sandbox.
  return 0;
#endif
}

bool GetFontTableForPrivateFontFile(PP_Resource font_file,
                                    uint32_t table,
                                    void* output,
                                    uint32_t* output_length) {
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  ppapi::Resource* resource =
      ppapi::PpapiGlobals::Get()->GetResourceTracker()->GetResource(font_file);
  if (!resource)
    return false;

  PrivateFontFile* font = static_cast<PrivateFontFile*>(resource);
  return font->GetFontTable(table, output, output_length);
#else
  return false;
#endif
}

void SearchString(PP_Instance instance,
                  const unsigned short* input_string,
                  const unsigned short* input_term,
                  bool case_sensitive,
                  PP_PrivateFindResult** results,
                  int* count) {
  const base::char16* string =
      reinterpret_cast<const base::char16*>(input_string);
  const base::char16* term = reinterpret_cast<const base::char16*>(input_term);

  UErrorCode status = U_ZERO_ERROR;
  UStringSearch* searcher =
      usearch_open(term,
                   -1,
                   string,
                   -1,
                   content::RenderThread::Get()->GetLocale().c_str(),
                   0,
                   &status);
  DCHECK(status == U_ZERO_ERROR || status == U_USING_FALLBACK_WARNING ||
         status == U_USING_DEFAULT_WARNING);
  UCollationStrength strength = case_sensitive ? UCOL_TERTIARY : UCOL_PRIMARY;

  UCollator* collator = usearch_getCollator(searcher);
  if (ucol_getStrength(collator) != strength) {
    ucol_setStrength(collator, strength);
    usearch_reset(searcher);
  }

  status = U_ZERO_ERROR;
  int match_start = usearch_first(searcher, &status);
  DCHECK(status == U_ZERO_ERROR);

  std::vector<PP_PrivateFindResult> pp_results;
  while (match_start != USEARCH_DONE) {
    size_t matched_length = usearch_getMatchedLength(searcher);
    PP_PrivateFindResult result;
    result.start_index = match_start;
    result.length = matched_length;
    pp_results.push_back(result);
    match_start = usearch_next(searcher, &status);
    DCHECK(status == U_ZERO_ERROR);
  }

  *count = pp_results.size();
  if (*count) {
    *results = reinterpret_cast<PP_PrivateFindResult*>(
        malloc(*count * sizeof(PP_PrivateFindResult)));
    memcpy(*results, &pp_results[0], *count * sizeof(PP_PrivateFindResult));
  } else {
    *results = NULL;
  }

  usearch_close(searcher);
}

void DidStartLoading(PP_Instance instance_id) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;
  instance->GetRenderView()->DidStartLoading();
}

void DidStopLoading(PP_Instance instance_id) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;
  instance->GetRenderView()->DidStopLoading();
}

void SetContentRestriction(PP_Instance instance_id, int restrictions) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;
  instance->GetRenderView()->Send(new PDFHostMsg_PDFUpdateContentRestrictions(
      instance->GetRenderView()->GetRoutingID(), restrictions));
}

void HistogramPDFPageCount(PP_Instance instance, int count) {
  UMA_HISTOGRAM_COUNTS_10000("PDF.PageCount", count);
}

void UserMetricsRecordAction(PP_Instance instance, PP_Var action) {
  scoped_refptr<ppapi::StringVar> action_str(
      ppapi::StringVar::FromPPVar(action));
  if (action_str.get())
    content::RenderThread::Get()->RecordComputedAction(action_str->value());
}

void HasUnsupportedFeature(PP_Instance instance_id) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;

  // Only want to show an info bar if the pdf is the whole tab.
  if (!instance->IsFullPagePlugin())
    return;

  blink::WebView* view =
      instance->GetContainer()->element().document().frame()->view();
  content::RenderView* render_view = content::RenderView::FromWebView(view);
  render_view->Send(
      new PDFHostMsg_PDFHasUnsupportedFeature(render_view->GetRoutingID()));
}

void SaveAs(PP_Instance instance_id) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;
  GURL url = instance->GetPluginURL();

  content::RenderView* render_view = instance->GetRenderView();
  blink::WebLocalFrame* frame =
      render_view->GetWebView()->mainFrame()->toWebLocalFrame();
  content::Referrer referrer = content::Referrer::SanitizeForRequest(
      url, content::Referrer(frame->document().url(),
                             frame->document().referrerPolicy()));
  render_view->Send(
      new PDFHostMsg_PDFSaveURLAs(render_view->GetRoutingID(), url, referrer));
}

void Print(PP_Instance instance) {
  PPB_PDF_Impl::InvokePrintingForInstance(instance);
}

PP_Bool IsFeatureEnabled(PP_Instance instance, PP_PDFFeature feature) {
  switch (feature) {
    case PP_PDFFEATURE_HIDPI:
      return PP_TRUE;
    case PP_PDFFEATURE_PRINTING:
      return (g_print_client_tls.Pointer()->Get() &&
              g_print_client_tls.Pointer()->Get()->IsPrintingEnabled(instance))
                 ? PP_TRUE
                 : PP_FALSE;
  }
  return PP_FALSE;
}

PP_Resource GetResourceImageForScale(PP_Instance instance_id,
                                     PP_ResourceImage image_id,
                                     float scale) {
  // Validate the instance.
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return 0;

  gfx::ImageSkia* res_image_skia = GetImageResource(image_id);

  if (!res_image_skia)
    return 0;

  return instance->CreateImage(res_image_skia, scale);
}

PP_Resource GetResourceImage(PP_Instance instance_id,
                             PP_ResourceImage image_id) {
  return GetResourceImageForScale(instance_id, image_id, 1.0f);
}

PP_Var ModalPromptForPassword(PP_Instance instance_id, PP_Var message) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return PP_MakeUndefined();

  std::string actual_value;
  scoped_refptr<ppapi::StringVar> message_string(
      ppapi::StringVar::FromPPVar(message));

  IPC::SyncMessage* msg = new PDFHostMsg_PDFModalPromptForPassword(
      instance->GetRenderView()->GetRoutingID(),
      message_string->value(),
      &actual_value);
  msg->EnableMessagePumping();
  instance->GetRenderView()->Send(msg);

  return ppapi::StringVar::StringToPPVar(actual_value);
}

PP_Bool IsOutOfProcess(PP_Instance instance_id) {
  return PP_FALSE;
}

// This function is intended for both in-process and out-of-process pdf.
void SetSelectedText(PP_Instance instance_id, const char* selected_text) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;

  base::string16 selection_text;
  base::UTF8ToUTF16(selected_text, strlen(selected_text), &selection_text);
  instance->SetSelectedText(selection_text);
}

void SetLinkUnderCursor(PP_Instance instance_id, const char* url) {
  content::PepperPluginInstance* instance =
      content::PepperPluginInstance::Get(instance_id);
  if (!instance)
    return;
  instance->SetLinkUnderCursor(url);
}

void GetV8ExternalSnapshotData(PP_Instance instance_id,
                               const char** natives_data_out,
                               int* natives_size_out,
                               const char** snapshot_data_out,
                               int* snapshot_size_out) {
  gin::IsolateHolder::GetV8ExternalSnapshotData(natives_data_out,
      natives_size_out, snapshot_data_out, snapshot_size_out);
}

const PPB_PDF ppb_pdf = {                      //
    &GetLocalizedString,                       //
    &GetResourceImage,                         //
    &GetFontFileWithFallback,                  //
    &GetFontTableForPrivateFontFile,           //
    &SearchString,                             //
    &DidStartLoading,                          //
    &DidStopLoading,                           //
    &SetContentRestriction,                    //
    &HistogramPDFPageCount,                    //
    &UserMetricsRecordAction,                  //
    &HasUnsupportedFeature,                    //
    &SaveAs,                                   //
    &Print,                                    //
    &IsFeatureEnabled,                         //
    &GetResourceImageForScale,                 //
    &ModalPromptForPassword,                   //
    &IsOutOfProcess,                           //
    &SetSelectedText,                          //
    &SetLinkUnderCursor,                       //
    &GetV8ExternalSnapshotData,                //
};

}  // namespace

// static
const PPB_PDF* PPB_PDF_Impl::GetInterface() {
  return &ppb_pdf;
}

// static
bool PPB_PDF_Impl::InvokePrintingForInstance(PP_Instance instance_id) {
  return g_print_client_tls.Pointer()->Get()
             ? g_print_client_tls.Pointer()->Get()->Print(instance_id)
             : false;
}

void PPB_PDF_Impl::SetPrintClient(PPB_PDF_Impl::PrintClient* client) {
  CHECK(!g_print_client_tls.Pointer()->Get())
      << "There should only be a single PrintClient for one RenderThread.";
  g_print_client_tls.Pointer()->Set(client);
}

}  // namespace pdf
