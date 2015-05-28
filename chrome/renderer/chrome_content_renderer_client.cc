// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/localized_error.h"
#include "chrome/common/pepper_permission_util.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/benchmarking_extension.h"
#include "chrome/renderer/chrome_render_frame_observer.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/chrome_render_view_observer.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/renderer/external_extension.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/media/chrome_key_systems.h"
#include "chrome/renderer/net/net_error_helper.h"
#include "chrome/renderer/net_benchmarking_extension.h"
#include "chrome/renderer/page_load_histograms.h"
#include "chrome/renderer/pepper/pepper_helper.h"
#include "chrome/renderer/plugins/chrome_plugin_placeholder.h"
#include "chrome/renderer/plugins/plugin_preroller.h"
#include "chrome/renderer/plugins/plugin_uma.h"
#include "chrome/renderer/plugins/shadow_dom_plugin_placeholder.h"
#include "chrome/renderer/prefetch_helper.h"
#include "chrome/renderer/prerender/prerender_dispatcher.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/prerender/prerender_media_load_deferrer.h"
#include "chrome/renderer/prerender/prerenderer_client.h"
#include "chrome/renderer/safe_browsing/malware_dom_details.h"
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#include "chrome/renderer/searchbox/search_bouncer.h"
#include "chrome/renderer/searchbox/searchbox.h"
#include "chrome/renderer/searchbox/searchbox_extension.h"
#include "chrome/renderer/tts_dispatcher.h"
#include "chrome/renderer/worker_content_settings_client_proxy.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/nacl/renderer/ppb_nacl_private.h"
#include "components/nacl/renderer/ppb_nacl_private_impl.h"
#include "components/network_hints/renderer/prescient_networking_dispatcher.h"
#include "components/password_manager/content/renderer/credential_manager_client.h"
#include "components/pdf/renderer/ppb_pdf_impl.h"
#include "components/plugins/renderer/mobile_youtube_plugin.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "components/web_cache/renderer/web_cache_render_process_observer.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/plugin_instance_throttler.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "extensions/common/constants.h"
#include "ipc/ipc_sync_channel.h"
#include "net/base/net_errors.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebPluginPlaceholder.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if !defined(DISABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#include "components/nacl/renderer/nacl_helper.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "chrome/common/extensions/extension_process_policy.h"
#include "chrome/renderer/extensions/chrome_extensions_dispatcher_delegate.h"
#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"
#include "chrome/renderer/extensions/renderer_permissions_policy_delegate.h"
#include "chrome/renderer/extensions/resource_request_policy.h"
#include "chrome/renderer/media/cast_ipc_dispatcher.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/renderer/extensions_render_frame_observer.h"
#include "extensions/renderer/guest_view/extensions_guest_view_container.h"
#include "extensions/renderer/guest_view/guest_view_container.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container.h"
#include "extensions/renderer/script_context.h"
#endif

#if defined(ENABLE_IPC_FUZZER)
#include "chrome/common/external_ipc_dumper.h"
#endif

#if defined(ENABLE_PRINTING)
#include "chrome/renderer/printing/chrome_print_web_view_helper_delegate.h"
#include "components/printing/renderer/print_web_view_helper.h"
#endif

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/renderer/pepper/chrome_pdf_print_client.h"
#endif

#if defined(ENABLE_SPELLCHECK)
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "chrome/renderer/spellchecker/spellcheck_provider.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "chrome/renderer/media/webrtc_logging_message_filter.h"
#endif

#if defined(OS_WIN)
#include "chrome_elf/blacklist/blacklist.h"
#endif

using autofill::AutofillAgent;
using autofill::PasswordAutofillAgent;
using autofill::PasswordGenerationAgent;
using base::ASCIIToUTF16;
using base::UserMetricsAction;
using content::PluginInstanceThrottler;
using content::RenderFrame;
using content::RenderThread;
using content::WebPluginInfo;
using extensions::Extension;
using blink::WebCache;
using blink::WebConsoleMessage;
using blink::WebDataSource;
using blink::WebDocument;
using blink::WebPlugin;
using blink::WebPluginParams;
using blink::WebSecurityOrigin;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;
using blink::WebVector;

namespace {

ChromeContentRendererClient* g_current_client;

// Whitelist PPAPI for Android Runtime for Chromium. (See crbug.com/383937)
#if defined(ENABLE_PLUGINS)
const char* const kPredefinedAllowedCameraDeviceOrigins[] = {
  "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",
  "4EB74897CB187C7633357C2FE832E0AD6A44883A"
};

const char* const kPredefinedAllowedCompositorOrigins[] = {
  "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",
  "4EB74897CB187C7633357C2FE832E0AD6A44883A"
};
#endif

void AppendParams(const std::vector<base::string16>& additional_names,
                  const std::vector<base::string16>& additional_values,
                  WebVector<WebString>* existing_names,
                  WebVector<WebString>* existing_values) {
  DCHECK(additional_names.size() == additional_values.size());
  DCHECK(existing_names->size() == existing_values->size());

  size_t existing_size = existing_names->size();
  size_t total_size = existing_size + additional_names.size();

  WebVector<WebString> names(total_size);
  WebVector<WebString> values(total_size);

  for (size_t i = 0; i < existing_size; ++i) {
    names[i] = (*existing_names)[i];
    values[i] = (*existing_values)[i];
  }

  for (size_t i = 0; i < additional_names.size(); ++i) {
    names[existing_size + i] = additional_names[i];
    values[existing_size + i] = additional_values[i];
  }

  existing_names->swap(names);
  existing_values->swap(values);
}

#if defined(ENABLE_SPELLCHECK)
class SpellCheckReplacer : public content::RenderViewVisitor {
 public:
  explicit SpellCheckReplacer(SpellCheck* spellcheck)
      : spellcheck_(spellcheck) {}
  bool Visit(content::RenderView* render_view) override;

 private:
  SpellCheck* spellcheck_;  // New shared spellcheck for all views. Weak Ptr.
  DISALLOW_COPY_AND_ASSIGN(SpellCheckReplacer);
};

bool SpellCheckReplacer::Visit(content::RenderView* render_view) {
  SpellCheckProvider* provider = SpellCheckProvider::Get(render_view);
  DCHECK(provider);
  provider->set_spellcheck(spellcheck_);
  return true;
}
#endif

// For certain sandboxed Pepper plugins, use the JavaScript Content Settings.
bool ShouldUseJavaScriptSettingForPlugin(const WebPluginInfo& plugin) {
  if (plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS) {
    return false;
  }

#if !defined(DISABLE_NACL)
  // Treat Native Client invocations like JavaScript.
  if (plugin.name == ASCIIToUTF16(nacl::kNaClPluginName))
    return true;
#endif

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)
  // Treat CDM invocations like JavaScript.
  if (plugin.name == ASCIIToUTF16(kWidevineCdmDisplayName)) {
    DCHECK(plugin.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS);
    return true;
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)

  return false;
}

#if defined(ENABLE_EXTENSIONS)
void IsGuestViewApiAvailableToScriptContext(
    bool* api_is_available,
    extensions::ScriptContext* context) {
  if (context->GetAvailability("guestViewInternal").is_available()) {
    *api_is_available = true;
  }
}
#endif

#if defined(ENABLE_PLUGINS)
// Presence of the poster param within plugin object tags.
// These numeric values are used in UMA logs; do not change them.
enum PosterParamPresence {
  POSTER_PRESENCE_NO_PARAM_PPS_DISABLED = 0,
  POSTER_PRESENCE_NO_PARAM_PPS_ENABLED = 1,
  POSTER_PRESENCE_PARAM_EXISTS_PPS_DISABLED = 2,
  POSTER_PRESENCE_PARAM_EXISTS_PPS_ENABLED = 3,
  POSTER_PRESENCE_NUM_ITEMS
};

const char kPluginPowerSaverPosterParamPresenceHistogram[] =
    "Plugin.PowerSaver.PosterParamPresence";

void RecordPosterParamPresence(PosterParamPresence presence) {
  UMA_HISTOGRAM_ENUMERATION(kPluginPowerSaverPosterParamPresenceHistogram,
                            presence, POSTER_PRESENCE_NUM_ITEMS);
}

void TrackPosterParamPresence(const blink::WebPluginParams& params,
                              bool power_saver_enabled) {
  DCHECK_EQ(params.attributeNames.size(), params.attributeValues.size());

  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    if (params.attributeNames[i].utf8() == "poster") {
      if (power_saver_enabled)
        RecordPosterParamPresence(POSTER_PRESENCE_PARAM_EXISTS_PPS_ENABLED);
      else
        RecordPosterParamPresence(POSTER_PRESENCE_PARAM_EXISTS_PPS_DISABLED);

      return;
    }
  }

  if (power_saver_enabled)
    RecordPosterParamPresence(POSTER_PRESENCE_NO_PARAM_PPS_ENABLED);
  else
    RecordPosterParamPresence(POSTER_PRESENCE_NO_PARAM_PPS_DISABLED);
}

std::string GetPluginInstancePosterAttribute(
    const blink::WebPluginParams& params) {
  DCHECK_EQ(params.attributeNames.size(), params.attributeValues.size());

  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    if (params.attributeNames[i].utf8() == "poster" &&
        !params.attributeValues[i].isEmpty()) {
      return params.attributeValues[i].utf8();
    }
  }
  return std::string();
}
#endif

#if defined(ENABLE_EXTENSIONS)
bool IsStandaloneExtensionProcess() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      extensions::switches::kExtensionProcess);
}
#endif
}  // namespace

ChromeContentRendererClient::ChromeContentRendererClient() {
  g_current_client = this;

#if defined(ENABLE_EXTENSIONS)
  extensions::ExtensionsClient::Set(
      extensions::ChromeExtensionsClient::GetInstance());
  extensions::ExtensionsRendererClient::Set(
      ChromeExtensionsRendererClient::GetInstance());
#endif
#if defined(ENABLE_PLUGINS)
  for (size_t i = 0; i < arraysize(kPredefinedAllowedCameraDeviceOrigins); ++i)
    allowed_camera_device_origins_.insert(
        kPredefinedAllowedCameraDeviceOrigins[i]);
  for (size_t i = 0; i < arraysize(kPredefinedAllowedCompositorOrigins); ++i)
    allowed_compositor_origins_.insert(kPredefinedAllowedCompositorOrigins[i]);
#endif
}

ChromeContentRendererClient::~ChromeContentRendererClient() {
  g_current_client = NULL;
}

void ChromeContentRendererClient::RenderThreadStarted() {
  RenderThread* thread = RenderThread::Get();

  chrome_observer_.reset(new ChromeRenderProcessObserver());
  web_cache_observer_.reset(new web_cache::WebCacheRenderProcessObserver());

#if defined(ENABLE_EXTENSIONS)
  extension_dispatcher_delegate_.reset(
      new ChromeExtensionsDispatcherDelegate());
  // ChromeRenderViewTest::SetUp() creates its own ExtensionDispatcher and
  // injects it using SetExtensionDispatcher(). Don't overwrite it.
  if (!extension_dispatcher_) {
    extension_dispatcher_.reset(
        new extensions::Dispatcher(extension_dispatcher_delegate_.get()));
  }
  permissions_policy_delegate_.reset(
      new extensions::RendererPermissionsPolicyDelegate(
          extension_dispatcher_.get()));
#endif

  prescient_networking_dispatcher_.reset(
      new network_hints::PrescientNetworkingDispatcher());
#if defined(ENABLE_SPELLCHECK)
  // ChromeRenderViewTest::SetUp() creates a Spellcheck and injects it using
  // SetSpellcheck(). Don't overwrite it.
  if (!spellcheck_) {
    spellcheck_.reset(new SpellCheck());
    thread->AddObserver(spellcheck_.get());
  }
#endif
  visited_link_slave_.reset(new visitedlink::VisitedLinkSlave());
#if defined(FULL_SAFE_BROWSING)
  phishing_classifier_.reset(safe_browsing::PhishingClassifierFilter::Create());
#endif
  prerender_dispatcher_.reset(new prerender::PrerenderDispatcher());
#if defined(ENABLE_WEBRTC)
  webrtc_logging_message_filter_ = new WebRtcLoggingMessageFilter(
      RenderThread::Get()->GetIOMessageLoopProxy());
#endif
  search_bouncer_.reset(new SearchBouncer());

  thread->AddObserver(chrome_observer_.get());
  thread->AddObserver(web_cache_observer_.get());
#if defined(ENABLE_EXTENSIONS)
  thread->AddObserver(extension_dispatcher_.get());
#endif
#if defined(FULL_SAFE_BROWSING)
  thread->AddObserver(phishing_classifier_.get());
#endif
  thread->AddObserver(visited_link_slave_.get());
  thread->AddObserver(prerender_dispatcher_.get());
  thread->AddObserver(search_bouncer_.get());

#if defined(ENABLE_WEBRTC)
  thread->AddFilter(webrtc_logging_message_filter_.get());
#endif
#if defined(ENABLE_EXTENSIONS)
  thread->AddFilter(new CastIPCDispatcher(
      RenderThread::Get()->GetIOMessageLoopProxy()));
#endif

  thread->RegisterExtension(extensions_v8::ExternalExtension::Get());
  thread->RegisterExtension(extensions_v8::LoadTimesExtension::Get());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableBenchmarking))
    thread->RegisterExtension(extensions_v8::BenchmarkingExtension::Get());
  if (command_line->HasSwitch(switches::kEnableNetBenchmarking))
    thread->RegisterExtension(extensions_v8::NetBenchmarkingExtension::Get());
  if (command_line->HasSwitch(switches::kInstantProcess))
    thread->RegisterExtension(extensions_v8::SearchBoxExtension::Get());

  // chrome:, chrome-search:, chrome-devtools:, and chrome-distiller: pages
  // should not be accessible by normal content, and should also be unable to
  // script anything but themselves (to help limit the damage that a corrupt
  // page could cause).
  WebString chrome_ui_scheme(ASCIIToUTF16(content::kChromeUIScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(chrome_ui_scheme);

  WebString chrome_search_scheme(ASCIIToUTF16(chrome::kChromeSearchScheme));
  // The Instant process can only display the content but not read it.  Other
  // processes can't display it or read it.
  if (!command_line->HasSwitch(switches::kInstantProcess))
    WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(chrome_search_scheme);

  WebString dev_tools_scheme(ASCIIToUTF16(content::kChromeDevToolsScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(dev_tools_scheme);

  WebString dom_distiller_scheme(
      ASCIIToUTF16(dom_distiller::kDomDistillerScheme));
  // TODO(nyquist): Add test to ensure this happens when the flag is set.
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(dom_distiller_scheme);

#if defined(OS_CHROMEOS)
  WebString external_file_scheme(ASCIIToUTF16(content::kExternalFileScheme));
  WebSecurityPolicy::registerURLSchemeAsLocal(external_file_scheme);
#endif

#if defined(ENABLE_IPC_FUZZER)
  if (command_line->HasSwitch(switches::kIpcDumpDirectory)) {
    base::FilePath dump_directory =
        command_line->GetSwitchValuePath(switches::kIpcDumpDirectory);
    IPC::ChannelProxy::OutgoingMessageFilter* filter =
        LoadExternalIPCDumper(dump_directory);
    thread->GetChannel()->set_outgoing_message_filter(filter);
  }
#endif

  // chrome: and chrome-search: pages should not be accessible by bookmarklets
  // or javascript: URLs typed in the omnibox.
  WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs(
      chrome_ui_scheme);
  WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs(
      chrome_search_scheme);

  // chrome:, chrome-search:, chrome-extension:, and chrome-extension-resource:
  // resources shouldn't trigger insecure content warnings.
  WebSecurityPolicy::registerURLSchemeAsSecure(chrome_ui_scheme);
  WebSecurityPolicy::registerURLSchemeAsSecure(chrome_search_scheme);

  WebString extension_scheme(ASCIIToUTF16(extensions::kExtensionScheme));
  WebSecurityPolicy::registerURLSchemeAsSecure(extension_scheme);

  WebString extension_resource_scheme(
      ASCIIToUTF16(extensions::kExtensionResourceScheme));
  WebSecurityPolicy::registerURLSchemeAsSecure(extension_resource_scheme);

  // chrome:, chrome-extension:, chrome-extension-resource: resources should be
  // allowed to receive CORS requests.
  WebSecurityPolicy::registerURLSchemeAsCORSEnabled(chrome_ui_scheme);
  WebSecurityPolicy::registerURLSchemeAsCORSEnabled(extension_scheme);
  WebSecurityPolicy::registerURLSchemeAsCORSEnabled(extension_resource_scheme);

  // chrome-extension: resources should bypass Content Security Policy checks
  // when included in protected resources.
  WebSecurityPolicy::registerURLSchemeAsBypassingContentSecurityPolicy(
      extension_scheme);
  WebSecurityPolicy::registerURLSchemeAsBypassingContentSecurityPolicy(
      extension_resource_scheme);

#if defined(OS_WIN)
  // Report if the renderer process has been patched by chrome_elf.
  // TODO(csharp): Remove once the renderer is no longer getting
  // patched this way.
  if (blacklist::IsBlacklistInitialized())
    UMA_HISTOGRAM_BOOLEAN("Blacklist.PatchedInRenderer", true);
#endif
#if defined(ENABLE_PRINT_PREVIEW)
  pdf_print_client_.reset(new ChromePDFPrintClient());
  pdf::PPB_PDF_Impl::SetPrintClient(pdf_print_client_.get());
#endif
}

void ChromeContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  new ChromeRenderFrameObserver(render_frame);

  bool should_whitelist_for_content_settings =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInstantProcess);
  extensions::Dispatcher* ext_dispatcher = NULL;
#if defined(ENABLE_EXTENSIONS)
  ext_dispatcher = extension_dispatcher_.get();
#endif
  ContentSettingsObserver* content_settings = new ContentSettingsObserver(
      render_frame, ext_dispatcher, should_whitelist_for_content_settings);
  if (chrome_observer_.get()) {
    content_settings->SetContentSettingRules(
        chrome_observer_->content_setting_rules());
  }

#if defined(ENABLE_EXTENSIONS)
  new extensions::ExtensionsRenderFrameObserver(render_frame);
  new extensions::ExtensionFrameHelper(render_frame, ext_dispatcher);
#endif

#if defined(ENABLE_PLUGINS)
  new PepperHelper(render_frame);
#endif

#if !defined(DISABLE_NACL)
  new nacl::NaClHelper(render_frame);
#endif

  // TODO(jam): when the frame tree moves into content and parent() works at
  // RenderFrame construction, simplify this by just checking parent().
  if (render_frame->GetRenderView()->GetMainRenderFrame() != render_frame) {
    // Avoid any race conditions from having the browser tell subframes that
    // they're prerendering.
    if (prerender::PrerenderHelper::IsPrerendering(
            render_frame->GetRenderView()->GetMainRenderFrame())) {
      new prerender::PrerenderHelper(render_frame);
    }
  }

  if (render_frame->GetRenderView()->GetMainRenderFrame() == render_frame) {
    // Only attach NetErrorHelper to the main frame, since only the main frame
    // should get error pages.
    // PrefetchHelper is also needed only for main frames.
    new NetErrorHelper(render_frame);
    new prefetch::PrefetchHelper(render_frame);
  }

  PasswordGenerationAgent* password_generation_agent =
      new PasswordGenerationAgent(render_frame);
  PasswordAutofillAgent* password_autofill_agent =
      new PasswordAutofillAgent(render_frame);
  new AutofillAgent(render_frame, password_autofill_agent,
                    password_generation_agent);
}

void ChromeContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {

#if defined(ENABLE_EXTENSIONS)
  new extensions::ExtensionHelper(render_view, extension_dispatcher_.get());
  extension_dispatcher_->OnRenderViewCreated(render_view);
#endif
  new PageLoadHistograms(render_view);
#if defined(ENABLE_PRINTING)
  new printing::PrintWebViewHelper(
      render_view,
      scoped_ptr<printing::PrintWebViewHelper::Delegate>(
          new ChromePrintWebViewHelperDelegate()));
#endif
#if defined(ENABLE_SPELLCHECK)
  new SpellCheckProvider(render_view, spellcheck_.get());
#endif
  new prerender::PrerendererClient(render_view);
#if defined(FULL_SAFE_BROWSING)
  safe_browsing::MalwareDOMDetails::Create(render_view);
#endif

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInstantProcess))
    new SearchBox(render_view);

  new ChromeRenderViewObserver(render_view, web_cache_observer_.get());

  new password_manager::CredentialManagerClient(render_view);
}


SkBitmap* ChromeContentRendererClient::GetSadPluginBitmap() {
  return const_cast<SkBitmap*>(ResourceBundle::GetSharedInstance().
      GetImageNamed(IDR_SAD_PLUGIN).ToSkBitmap());
}

SkBitmap* ChromeContentRendererClient::GetSadWebViewBitmap() {
  return const_cast<SkBitmap*>(ResourceBundle::GetSharedInstance().
      GetImageNamed(IDR_SAD_WEBVIEW).ToSkBitmap());
}

#if defined(ENABLE_EXTENSIONS)
const Extension* ChromeContentRendererClient::GetExtensionByOrigin(
    const WebSecurityOrigin& origin) const {
  if (!EqualsASCII(origin.protocol(), extensions::kExtensionScheme))
    return NULL;

  const std::string extension_id = origin.host().utf8().data();
  return extension_dispatcher_->extensions()->GetByID(extension_id);
}
#endif

scoped_ptr<blink::WebPluginPlaceholder>
ChromeContentRendererClient::CreatePluginPlaceholder(
    content::RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& orig_params) {
  return CreateShadowDOMPlaceholderForPluginInfo(
      render_frame, frame, orig_params);
}

bool ChromeContentRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const WebPluginParams& params,
    WebPlugin** plugin) {
  std::string orig_mime_type = params.mimeType.utf8();
#if defined(ENABLE_EXTENSIONS)
  if (orig_mime_type == content::kBrowserPluginMimeType) {
    bool guest_view_api_available = false;
    extension_dispatcher_->script_context_set().ForEach(
        render_frame->GetRenderView(),
        base::Bind(&IsGuestViewApiAvailableToScriptContext,
                   &guest_view_api_available));
    if (guest_view_api_available)
      return false;
  }
#endif

  ChromeViewHostMsg_GetPluginInfo_Output output;
#if defined(ENABLE_PLUGINS)
  render_frame->Send(new ChromeViewHostMsg_GetPluginInfo(
      render_frame->GetRoutingID(), GURL(params.url),
      frame->top()->document().url(), orig_mime_type, &output));

#else
  output.status.value = ChromeViewHostMsg_GetPluginInfo_Status::kNotFound;
#endif
  *plugin = CreatePlugin(render_frame, frame, params, output);
  return true;
}

WebPlugin* ChromeContentRendererClient::CreatePluginReplacement(
    content::RenderFrame* render_frame,
    const base::FilePath& plugin_path) {
  ChromePluginPlaceholder* placeholder =
      ChromePluginPlaceholder::CreateErrorPlugin(render_frame, plugin_path);
  return placeholder->plugin();
}

void ChromeContentRendererClient::DeferMediaLoad(
    content::RenderFrame* render_frame,
    const base::Closure& closure) {
#if defined(OS_ANDROID)
  // Chromium for Android doesn't support prerender yet.
  closure.Run();
  return;
#else
  if (!prerender::PrerenderHelper::IsPrerendering(render_frame)) {
    closure.Run();
    return;
  }

  // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
  new prerender::PrerenderMediaLoadDeferrer(render_frame, closure);
#endif
}

WebPlugin* ChromeContentRendererClient::CreatePlugin(
    content::RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const WebPluginParams& original_params,
    const ChromeViewHostMsg_GetPluginInfo_Output& output) {
  const ChromeViewHostMsg_GetPluginInfo_Status& status = output.status;
  const WebPluginInfo& info = output.plugin;
  const std::string& actual_mime_type = output.actual_mime_type;
  const base::string16& group_name = output.group_name;
  const std::string& identifier = output.group_identifier;
  ChromeViewHostMsg_GetPluginInfo_Status::Value status_value = status.value;
  GURL url(original_params.url);
  std::string orig_mime_type = original_params.mimeType.utf8();
  ChromePluginPlaceholder* placeholder = NULL;

  // If the browser plugin is to be enabled, this should be handled by the
  // renderer, so the code won't reach here due to the early exit in
  // OverrideCreatePlugin.
  if (status_value == ChromeViewHostMsg_GetPluginInfo_Status::kNotFound ||
      orig_mime_type == content::kBrowserPluginMimeType) {
#if defined(OS_ANDROID)
    if (plugins::MobileYouTubePlugin::IsYouTubeURL(url, orig_mime_type)) {
      base::StringPiece template_html(
          ResourceBundle::GetSharedInstance().GetRawDataResource(
              IDR_MOBILE_YOUTUBE_PLUGIN_HTML));
      return (new plugins::MobileYouTubePlugin(
                  render_frame,
                  frame,
                  original_params,
                  template_html,
                  GURL(ChromePluginPlaceholder::kPluginPlaceholderDataURL)))
          ->plugin();
    }
#endif
    PluginUMAReporter::GetInstance()->ReportPluginMissing(orig_mime_type, url);
    placeholder = ChromePluginPlaceholder::CreateMissingPlugin(
        render_frame, frame, original_params);
  } else {
    // TODO(bauerb): This should be in content/.
    WebPluginParams params(original_params);
    for (size_t i = 0; i < info.mime_types.size(); ++i) {
      if (info.mime_types[i].mime_type == actual_mime_type) {
        AppendParams(info.mime_types[i].additional_param_names,
                     info.mime_types[i].additional_param_values,
                     &params.attributeNames, &params.attributeValues);
        break;
      }
    }
    if (params.mimeType.isNull() && (actual_mime_type.size() > 0)) {
      // Webkit might say that mime type is null while we already know the
      // actual mime type via ChromeViewHostMsg_GetPluginInfo. In that case
      // we should use what we know since WebpluginDelegateProxy does some
      // specific initializations based on this information.
      params.mimeType = WebString::fromUTF8(actual_mime_type.c_str());
    }

    ContentSettingsObserver* observer =
        ContentSettingsObserver::Get(render_frame);

    const ContentSettingsType content_type =
        ShouldUseJavaScriptSettingForPlugin(info)
            ? CONTENT_SETTINGS_TYPE_JAVASCRIPT
            : CONTENT_SETTINGS_TYPE_PLUGINS;

    if ((status_value ==
             ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized ||
         status_value == ChromeViewHostMsg_GetPluginInfo_Status::kBlocked) &&
        observer->IsPluginTemporarilyAllowed(identifier)) {
      status_value = ChromeViewHostMsg_GetPluginInfo_Status::kAllowed;
    }

#if defined(OS_WIN)
    // In Windows we need to check if we can load NPAPI plugins.
    // For example, if the render view is in the Ash desktop, we should not.
    // If user is on ALLOW or DETECT setting, loading needs to be blocked here.
    if ((status_value == ChromeViewHostMsg_GetPluginInfo_Status::kAllowed ||
         status_value ==
             ChromeViewHostMsg_GetPluginInfo_Status::kPlayImportantContent) &&
        info.type == content::WebPluginInfo::PLUGIN_TYPE_NPAPI) {
        if (observer->AreNPAPIPluginsBlocked())
          status_value =
              ChromeViewHostMsg_GetPluginInfo_Status::kNPAPINotSupported;
    }
#endif

    auto create_blocked_plugin =
        [&render_frame, &frame, &params, &info, &identifier, &group_name](
            int template_id, const base::string16& message) {
          return ChromePluginPlaceholder::CreateBlockedPlugin(
              render_frame, frame, params, info, identifier, group_name,
              template_id, message, PlaceholderPosterInfo());
        };
    switch (status_value) {
      case ChromeViewHostMsg_GetPluginInfo_Status::kNotFound: {
        NOTREACHED();
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kAllowed:
      case ChromeViewHostMsg_GetPluginInfo_Status::kPlayImportantContent: {
#if !defined(DISABLE_NACL) && defined(ENABLE_EXTENSIONS)
        const bool is_nacl_plugin =
            info.name == ASCIIToUTF16(nacl::kNaClPluginName);
        const bool is_nacl_mime_type =
            actual_mime_type == nacl::kNaClPluginMimeType;
        const bool is_pnacl_mime_type =
            actual_mime_type == nacl::kPnaclPluginMimeType;
        if (is_nacl_plugin || is_nacl_mime_type || is_pnacl_mime_type) {
          bool is_nacl_unrestricted = false;
          if (is_nacl_mime_type) {
            is_nacl_unrestricted =
                base::CommandLine::ForCurrentProcess()->HasSwitch(
                    switches::kEnableNaCl);
          } else if (is_pnacl_mime_type) {
            is_nacl_unrestricted = true;
          }
          GURL manifest_url;
          GURL app_url;
          if (is_nacl_mime_type || is_pnacl_mime_type) {
            // Normal NaCl/PNaCl embed. The app URL is the page URL.
            manifest_url = url;
            app_url = frame->top()->document().url();
          } else {
            // NaCl is being invoked as a content handler. Look up the NaCl
            // module using the MIME type. The app URL is the manifest URL.
            manifest_url = GetNaClContentHandlerURL(actual_mime_type, info);
            app_url = manifest_url;
          }
          const Extension* extension =
              g_current_client->extension_dispatcher_->extensions()->
                  GetExtensionOrAppByURL(manifest_url);
          if (!IsNaClAllowed(manifest_url,
                             app_url,
                             is_nacl_unrestricted,
                             extension,
                             &params)) {
            WebString error_message;
            if (is_nacl_mime_type) {
              error_message =
                  "Only unpacked extensions and apps installed from the Chrome "
                  "Web Store can load NaCl modules without enabling Native "
                  "Client in about:flags.";
            } else if (is_pnacl_mime_type) {
              error_message =
                  "Portable Native Client must not be disabled in about:flags.";
            }
            frame->addMessageToConsole(
                WebConsoleMessage(WebConsoleMessage::LevelError,
                                  error_message));
            placeholder = create_blocked_plugin(
                IDR_BLOCKED_PLUGIN_HTML,
  #if defined(OS_CHROMEOS)
                l10n_util::GetStringUTF16(IDS_NACL_PLUGIN_BLOCKED));
  #else
                l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, group_name));
  #endif
            break;
          }
        }
#endif  // !defined(DISABLE_NACL) && defined(ENABLE_EXTENSIONS)

#if defined(ENABLE_PLUGINS)
        // Delay loading plugins if prerendering.
        // TODO(mmenke):  In the case of prerendering, feed into
        //                ChromeContentRendererClient::CreatePlugin instead, to
        //                reduce the chance of future regressions.
        bool is_prerendering =
            prerender::PrerenderHelper::IsPrerendering(render_frame);

        // TODO(tommycli): Plugin Power Saver is disabled on prerendered pages.
        // This is because the placeholder does not feed back into
        // ChromeContentRendererClient::CreatePlugin. Because of this, it does
        // not handle the preroll to UI overlay placeholder flow correctly.
        //
        // Background tab plugin deferral is disabled for the same reason.
        //
        // https://crbug.com/471427
        bool power_saver_enabled =
            !is_prerendering &&
            status_value ==
                ChromeViewHostMsg_GetPluginInfo_Status::kPlayImportantContent;

        if (info.name == ASCIIToUTF16(content::kFlashPluginName))
          TrackPosterParamPresence(params, power_saver_enabled);

        PlaceholderPosterInfo poster_info;
        if (power_saver_enabled) {
          poster_info.poster_attribute =
              GetPluginInstancePosterAttribute(params);
          poster_info.base_url = frame->document().url();
        }

        if (is_prerendering || !poster_info.poster_attribute.empty()) {
          placeholder = ChromePluginPlaceholder::CreateBlockedPlugin(
              render_frame, frame, params, info, identifier, group_name,
              poster_info.poster_attribute.empty() ? IDR_BLOCKED_PLUGIN_HTML
                                                   : IDR_PLUGIN_POSTER_HTML,
              l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, group_name),
              poster_info);
          placeholder->set_blocked_for_prerendering(is_prerendering);
          placeholder->set_power_saver_enabled(power_saver_enabled);
          placeholder->set_allow_loading(true);
          break;
        }

        scoped_ptr<content::PluginInstanceThrottler> throttler;
        if (power_saver_enabled) {
          throttler = PluginInstanceThrottler::Create();
          // PluginPreroller manages its own lifetime.
          new PluginPreroller(
              render_frame, frame, params, info, identifier, group_name,
              l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, group_name),
              throttler.get());
        }

        return render_frame->CreatePlugin(frame, info, params,
                                          throttler.Pass());
#else   // !defined(ENABLE_PLUGINS)
        return render_frame->CreatePlugin(frame, info, params, nullptr);
#endif  // defined(ENABLE_PLUGINS)
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kNPAPINotSupported: {
        RenderThread::Get()->RecordAction(
            UserMetricsAction("Plugin_NPAPINotSupported"));
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_SUPPORTED_METRO));
        render_frame->Send(new ChromeViewHostMsg_NPAPINotSupported(
            render_frame->GetRoutingID(), identifier));
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kDisabled: {
        PluginUMAReporter::GetInstance()->ReportPluginDisabled(orig_mime_type,
                                                               url);
        placeholder = create_blocked_plugin(
            IDR_DISABLED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_DISABLED, group_name));
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedBlocked: {
#if defined(ENABLE_PLUGIN_INSTALLATION)
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED, group_name));
        placeholder->set_allow_loading(true);
        render_frame->Send(new ChromeViewHostMsg_BlockedOutdatedPlugin(
            render_frame->GetRoutingID(), placeholder->CreateRoutingId(),
            identifier));
#else
        NOTREACHED();
#endif
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedDisallowed: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED, group_name));
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_NOT_AUTHORIZED, group_name));
        placeholder->set_allow_loading(true);
        if (info.type != content::WebPluginInfo::PLUGIN_TYPE_NPAPI) {
          render_frame->Send(new ChromeViewHostMsg_BlockedUnauthorizedPlugin(
              render_frame->GetRoutingID(),
              group_name,
              identifier));
        }
        observer->DidBlockContentType(content_type, group_name);
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kBlocked: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, group_name));
        placeholder->set_allow_loading(true);
        RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Blocked"));
        observer->DidBlockContentType(content_type, group_name);
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kBlockedByPolicy: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, group_name));
        placeholder->set_allow_loading(false);
        RenderThread::Get()->RecordAction(
            UserMetricsAction("Plugin_BlockedByPolicy"));
        observer->DidBlockContentType(content_type, group_name);
        break;
      }
    }
  }
  placeholder->SetStatus(status);
  return placeholder->plugin();
}

// For NaCl content handling plugins, the NaCl manifest is stored in an
// additonal 'nacl' param associated with the MIME type.
//  static
GURL ChromeContentRendererClient::GetNaClContentHandlerURL(
    const std::string& actual_mime_type,
    const content::WebPluginInfo& plugin) {
  // Look for the manifest URL among the MIME type's additonal parameters.
  const char kNaClPluginManifestAttribute[] = "nacl";
  base::string16 nacl_attr = ASCIIToUTF16(kNaClPluginManifestAttribute);
  for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
    if (plugin.mime_types[i].mime_type == actual_mime_type) {
      const content::WebPluginMimeType& content_type = plugin.mime_types[i];
      for (size_t i = 0; i < content_type.additional_param_names.size(); ++i) {
        if (content_type.additional_param_names[i] == nacl_attr)
          return GURL(content_type.additional_param_values[i]);
      }
      break;
    }
  }
  return GURL();
}

#if !defined(DISABLE_NACL)
//  static
bool ChromeContentRendererClient::IsNaClAllowed(
    const GURL& manifest_url,
    const GURL& app_url,
    bool is_nacl_unrestricted,
    const Extension* extension,
    WebPluginParams* params) {
  // Temporarily allow these whitelisted apps and WebUIs to use NaCl.
  std::string app_url_host = app_url.host();
  std::string manifest_url_path = manifest_url.path();

  bool is_whitelisted_web_ui =
      app_url.spec() == chrome::kChromeUIAppListStartPageURL;

  bool is_photo_app =
      // Whitelisted apps must be served over https.
      app_url.SchemeIs("https") &&
      manifest_url.SchemeIs("https") &&
      (EndsWith(app_url_host, "plus.google.com", false) ||
       EndsWith(app_url_host, "plus.sandbox.google.com", false)) &&
      manifest_url.DomainIs("ssl.gstatic.com") &&
      (manifest_url_path.find("s2/oz/nacl/") == 1 ||
       manifest_url_path.find("photos/nacl/") == 1);

  std::string manifest_fs_host;
  if (manifest_url.SchemeIsFileSystem() && manifest_url.inner_url()) {
    manifest_fs_host = manifest_url.inner_url()->host();
  }
  bool is_hangouts_app =
      // Whitelisted apps must be served over secure scheme.
      app_url.SchemeIs("https") &&
      manifest_url.SchemeIsSecure() &&
      manifest_url.SchemeIsFileSystem() &&
      (EndsWith(app_url_host, "talkgadget.google.com", false) ||
       EndsWith(app_url_host, "plus.google.com", false) ||
       EndsWith(app_url_host, "plus.sandbox.google.com", false)) &&
      // The manifest must be loaded from the host's FileSystem.
      (manifest_fs_host == app_url_host);

  bool is_whitelisted_app = is_photo_app || is_hangouts_app;

  bool is_extension_from_webstore = false;
  bool is_invoked_by_hosted_app = false;
  bool is_extension_unrestricted = false;
#if defined(ENABLE_EXTENSIONS)
  is_extension_from_webstore = extension && extension->from_webstore();

  is_invoked_by_hosted_app = extension &&
      extension->is_hosted_app() &&
      extension->web_extent().MatchesURL(app_url);

  // Allow built-in extensions and extensions under development.
  is_extension_unrestricted = extension &&
      (extension->location() == extensions::Manifest::COMPONENT ||
       extensions::Manifest::IsUnpackedLocation(extension->location()));
#endif  // defined(ENABLE_EXTENSIONS)

  bool is_invoked_by_extension = app_url.SchemeIs("chrome-extension");

  // The NaCl PDF viewer is always allowed and can use 'Dev' interfaces.
  bool is_nacl_pdf_viewer =
      (is_extension_from_webstore &&
       manifest_url.SchemeIs("chrome-extension") &&
       manifest_url.host() == "acadkphlmlegjaadjagenfimbpphcgnh");

  // Allow Chrome Web Store extensions, built-in extensions and extensions
  // under development if the invocation comes from a URL with an extension
  // scheme. Also allow invocations if they are from whitelisted URLs or
  // if --enable-nacl is set.
  bool is_nacl_allowed = is_nacl_unrestricted ||
                         is_whitelisted_web_ui ||
                         is_whitelisted_app ||
                         is_nacl_pdf_viewer ||
                         is_invoked_by_hosted_app ||
                         (is_invoked_by_extension &&
                             (is_extension_from_webstore ||
                                 is_extension_unrestricted));
  if (is_nacl_allowed) {
    bool app_can_use_dev_interfaces = is_nacl_pdf_viewer;
    // Make sure that PPAPI 'dev' interfaces aren't available for production
    // apps unless they're whitelisted.
    WebString dev_attribute = WebString::fromUTF8("@dev");
    if ((!is_whitelisted_app && !is_extension_from_webstore) ||
        app_can_use_dev_interfaces) {
      // Add the special '@dev' attribute.
      std::vector<base::string16> param_names;
      std::vector<base::string16> param_values;
      param_names.push_back(dev_attribute);
      param_values.push_back(WebString());
      AppendParams(
          param_names,
          param_values,
          &params->attributeNames,
          &params->attributeValues);
    } else {
      // If the params somehow contain '@dev', remove it.
      size_t attribute_count = params->attributeNames.size();
      for (size_t i = 0; i < attribute_count; ++i) {
        if (params->attributeNames[i].equals(dev_attribute))
          params->attributeNames[i] = WebString();
      }
    }
  }
  return is_nacl_allowed;
}
#endif  // defined(DISABLE_NACL)

bool ChromeContentRendererClient::HasErrorPage(int http_status_code,
                                               std::string* error_domain) {
  // Use an internal error page, if we have one for the status code.
  if (!LocalizedError::HasStrings(LocalizedError::kHttpErrorDomain,
                                  http_status_code)) {
    return false;
  }

  *error_domain = LocalizedError::kHttpErrorDomain;
  return true;
}

bool ChromeContentRendererClient::ShouldSuppressErrorPage(
    content::RenderFrame* render_frame,
    const GURL& url) {
  // Unit tests for ChromeContentRendererClient pass a NULL RenderFrame here.
  // Unfortunately it's very difficult to construct a mock RenderView, so skip
  // this functionality in this case.
  if (render_frame) {
    content::RenderView* render_view = render_frame->GetRenderView();
    content::RenderFrame* main_render_frame = render_view->GetMainRenderFrame();
    blink::WebFrame* web_frame = render_frame->GetWebFrame();
    NetErrorHelper* net_error_helper = NetErrorHelper::Get(main_render_frame);
    if (net_error_helper->ShouldSuppressErrorPage(web_frame, url))
      return true;
  }
  // Do not flash an error page if the Instant new tab page fails to load.
  return search_bouncer_.get() && search_bouncer_->IsNewTabPage(url);
}

void ChromeContentRendererClient::GetNavigationErrorStrings(
    content::RenderView* render_view,
    blink::WebFrame* frame,
    const blink::WebURLRequest& failed_request,
    const blink::WebURLError& error,
    std::string* error_html,
    base::string16* error_description) {
  const GURL failed_url = error.unreachableURL;
  const Extension* extension = NULL;

#if defined(ENABLE_EXTENSIONS)
  if (failed_url.is_valid() &&
      !failed_url.SchemeIs(extensions::kExtensionScheme)) {
    extension = extension_dispatcher_->extensions()->GetExtensionOrAppByURL(
        failed_url);
  }
#endif

  bool is_post = EqualsASCII(failed_request.httpMethod(), "POST");

  if (error_html) {
    bool extension_but_not_bookmark_app = false;
#if defined(ENABLE_EXTENSIONS)
    extension_but_not_bookmark_app = extension && !extension->from_bookmark();
#endif
    // Use a local error page.
    if (extension_but_not_bookmark_app) {
#if defined(ENABLE_EXTENSIONS)
      // TODO(erikkay): Should we use a different template for different
      // error messages?
      int resource_id = IDR_ERROR_APP_HTML;
      const base::StringPiece template_html(
          ResourceBundle::GetSharedInstance().GetRawDataResource(
              resource_id));
      if (template_html.empty()) {
        NOTREACHED() << "unable to load template. ID: " << resource_id;
      } else {
        base::DictionaryValue error_strings;
        const std::string locale = RenderThread::Get()->GetLocale();
        LocalizedError::GetAppErrorStrings(failed_url, extension, locale,
                                           &error_strings);
        // "t" is the id of the template's root node.
        *error_html = webui::GetTemplatesHtml(template_html, &error_strings,
                                              "t");
      }
#endif
    } else {
      // TODO(ellyjones): change GetNavigationErrorStrings to take a RenderFrame
      // instead of a RenderView, then pass that in.
      // This is safe for now because we only install the NetErrorHelper on the
      // main render frame anyway; see the TODO(ellyjones) in
      // RenderFrameCreated.
      content::RenderFrame* main_render_frame =
          render_view->GetMainRenderFrame();
      NetErrorHelper* helper = NetErrorHelper::Get(main_render_frame);
      helper->GetErrorHTML(frame, error, is_post, error_html);
    }
  }

  if (error_description) {
    if (!extension)
      *error_description = LocalizedError::GetErrorDetails(error, is_post);
  }
}

bool ChromeContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
#if defined(ENABLE_EXTENSIONS)
  return !IsStandaloneExtensionProcess();
#else
  return true;
#endif
}

bool ChromeContentRendererClient::AllowPopup() {
#if defined(ENABLE_EXTENSIONS)
  extensions::ScriptContext* current_context =
      extension_dispatcher_->script_context_set().GetCurrent();
  if (!current_context || !current_context->extension())
    return false;
  // See http://crbug.com/117446 for the subtlety of this check.
  switch (current_context->context_type()) {
    case extensions::Feature::UNSPECIFIED_CONTEXT:
    case extensions::Feature::WEB_PAGE_CONTEXT:
    case extensions::Feature::UNBLESSED_EXTENSION_CONTEXT:
    case extensions::Feature::WEBUI_CONTEXT:
      return false;
    case extensions::Feature::BLESSED_EXTENSION_CONTEXT:
    case extensions::Feature::CONTENT_SCRIPT_CONTEXT:
      return true;
    case extensions::Feature::BLESSED_WEB_PAGE_CONTEXT:
      return !current_context->web_frame()->parent();
  }
  NOTREACHED();
#endif
  return false;
}

bool ChromeContentRendererClient::ShouldFork(blink::WebFrame* frame,
                                             const GURL& url,
                                             const std::string& http_method,
                                             bool is_initial_navigation,
                                             bool is_server_redirect,
                                             bool* send_referrer) {
  DCHECK(!frame->parent());

  // If this is the Instant process, fork all navigations originating from the
  // renderer.  The destination page will then be bucketed back to this Instant
  // process if it is an Instant url, or to another process if not.  Conversely,
  // fork if this is a non-Instant process navigating to an Instant url, so that
  // such navigations can also be bucketed into an Instant renderer.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInstantProcess) ||
      (search_bouncer_.get() && search_bouncer_->ShouldFork(url))) {
    *send_referrer = true;
    return true;
  }

  // For now, we skip the rest for POST submissions.  This is because
  // http://crbug.com/101395 is more likely to cause compatibility issues
  // with hosted apps and extensions than WebUI pages.  We will remove this
  // check when cross-process POST submissions are supported.
  if (http_method != "GET")
    return false;

  // If |url| matches one of the prerendered URLs, stop this navigation and try
  // to swap in the prerendered page on the browser process. If the prerendered
  // page no longer exists by the time the OpenURL IPC is handled, a normal
  // navigation is attempted.
  if (prerender_dispatcher_.get() &&
      prerender_dispatcher_->IsPrerenderURL(url)) {
    *send_referrer = true;
    return true;
  }

#if defined(ENABLE_EXTENSIONS)
  const extensions::ExtensionSet* extensions =
      extension_dispatcher_->extensions();

  // Determine if the new URL is an extension (excluding bookmark apps).
  const Extension* new_url_extension = extensions::GetNonBookmarkAppExtension(
      *extensions, url);
  bool is_extension_url = !!new_url_extension;

  // If the navigation would cross an app extent boundary, we also need
  // to defer to the browser to ensure process isolation.  This is not necessary
  // for server redirects, which will be transferred to a new process by the
  // browser process when they are ready to commit.  It is necessary for client
  // redirects, which won't be transferred in the same way.
  if (!is_server_redirect &&
      CrossesExtensionExtents(frame, url, *extensions, is_extension_url,
          is_initial_navigation)) {
    // Include the referrer in this case since we're going from a hosted web
    // page. (the packaged case is handled previously by the extension
    // navigation test)
    *send_referrer = true;

    const Extension* extension =
        extension_dispatcher_->extensions()->GetExtensionOrAppByURL(url);
    if (extension && extension->is_app()) {
      extensions::RecordAppLaunchType(
          extension_misc::APP_LAUNCH_CONTENT_NAVIGATION, extension->GetType());
    }
    return true;
  }

  // If this is a reload, check whether it has the wrong process type.  We
  // should send it to the browser if it's an extension URL (e.g., hosted app)
  // in a normal process, or if it's a process for an extension that has been
  // uninstalled.
  if (frame->top()->document().url() == url) {
    if (is_extension_url != IsStandaloneExtensionProcess())
      return true;
  }
#endif  // defined(ENABLE_EXTENSIONS)

  return false;
}

#if defined(ENABLE_EXTENSIONS)
bool ChromeContentRendererClient::ShouldForwardToGuestContainer(
    const IPC::Message& msg) {
  return extensions::GuestViewContainer::HandlesMessage(msg);
}
#endif

bool ChromeContentRendererClient::WillSendRequest(
    blink::WebFrame* frame,
    ui::PageTransition transition_type,
    const GURL& url,
    const GURL& first_party_for_cookies,
    GURL* new_url) {
  // Check whether the request should be allowed. If not allowed, we reset the
  // URL to something invalid to prevent the request and cause an error.
#if defined(ENABLE_EXTENSIONS)
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      !extensions::ResourceRequestPolicy::CanRequestResource(
          url,
          frame,
          transition_type,
          extension_dispatcher_->extensions())) {
    *new_url = GURL(chrome::kExtensionInvalidRequestURL);
    return true;
  }

  if (url.SchemeIs(extensions::kExtensionResourceScheme) &&
      !extensions::ResourceRequestPolicy::CanRequestExtensionResourceScheme(
          url,
          frame)) {
    *new_url = GURL(chrome::kExtensionResourceInvalidRequestURL);
    return true;
  }
#endif

  const content::RenderView* render_view =
      content::RenderView::FromWebView(frame->view());
  SearchBox* search_box = SearchBox::Get(render_view);
  if (search_box && url.SchemeIs(chrome::kChromeSearchScheme)) {
    SearchBox::ImageSourceType type = SearchBox::NONE;
    if (url.host() == chrome::kChromeUIFaviconHost)
      type = SearchBox::FAVICON;
    else if (url.host() == chrome::kChromeUILargeIconHost)
      type = SearchBox::LARGE_ICON;
    else if (url.host() == chrome::kChromeUIFallbackIconHost)
      type = SearchBox::FALLBACK_ICON;
    else if (url.host() == chrome::kChromeUIThumbnailHost)
      type = SearchBox::THUMB;
    if (type != SearchBox::NONE)
      return search_box->GenerateImageURLFromTransientURL(url, type, new_url);
  }

  return false;
}

unsigned long long ChromeContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return visited_link_slave_->ComputeURLFingerprint(canonical_url, length);
}

bool ChromeContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return visited_link_slave_->IsVisited(link_hash);
}

blink::WebPrescientNetworking*
ChromeContentRendererClient::GetPrescientNetworking() {
  return prescient_networking_dispatcher_.get();
}

bool ChromeContentRendererClient::ShouldOverridePageVisibilityState(
    const content::RenderFrame* render_frame,
    blink::WebPageVisibilityState* override_state) {
  if (!prerender::PrerenderHelper::IsPrerendering(render_frame))
    return false;

  *override_state = blink::WebPageVisibilityStatePrerender;
  return true;
}

#if defined(ENABLE_EXTENSIONS)
void ChromeContentRendererClient::SetExtensionDispatcherForTest(
    extensions::Dispatcher* extension_dispatcher) {
  extension_dispatcher_.reset(extension_dispatcher);
  permissions_policy_delegate_.reset(
      new extensions::RendererPermissionsPolicyDelegate(
          extension_dispatcher_.get()));
}

extensions::Dispatcher*
ChromeContentRendererClient::GetExtensionDispatcherForTest() {
  return extension_dispatcher_.get();
}

bool ChromeContentRendererClient::CrossesExtensionExtents(
    blink::WebFrame* frame,
    const GURL& new_url,
    const extensions::ExtensionSet& extensions,
    bool is_extension_url,
    bool is_initial_navigation) {
  GURL old_url(frame->top()->document().url());

  // If old_url is still empty and this is an initial navigation, then this is
  // a window.open operation.  We should look at the opener URL.
  if (is_initial_navigation && old_url.is_empty() && frame->opener()) {
    // If we're about to open a normal web page from a same-origin opener stuck
    // in an extension process, we want to keep it in process to allow the
    // opener to script it.
    WebDocument opener_document = frame->opener()->document();
    WebSecurityOrigin opener = frame->opener()->document().securityOrigin();
    bool opener_is_extension_url =
        !opener.isUnique() && extensions.GetExtensionOrAppByURL(
            opener_document.url()) != NULL;
    if (!is_extension_url &&
        !opener_is_extension_url &&
        IsStandaloneExtensionProcess() &&
        opener.canRequest(WebURL(new_url)))
      return false;

    // In all other cases, we want to compare against the top frame's URL (as
    // opposed to the opener frame's), since that's what determines the type of
    // process.  This allows iframes outside an app to open a popup in the app.
    old_url = frame->top()->opener()->top()->document().url();
  }

  // Only consider keeping non-app URLs in an app process if this window
  // has an opener (in which case it might be an OAuth popup that tries to
  // script an iframe within the app).
  bool should_consider_workaround = !!frame->opener();

  return extensions::CrossesExtensionProcessBoundary(
      extensions, old_url, new_url, should_consider_workaround);
}
#endif  // defined(ENABLE_EXTENSIONS)

#if defined(ENABLE_SPELLCHECK)
void ChromeContentRendererClient::SetSpellcheck(SpellCheck* spellcheck) {
  RenderThread* thread = RenderThread::Get();
  if (spellcheck_.get() && thread)
    thread->RemoveObserver(spellcheck_.get());
  spellcheck_.reset(spellcheck);
  SpellCheckReplacer replacer(spellcheck_.get());
  content::RenderView::ForEach(&replacer);
  if (thread)
    thread->AddObserver(spellcheck_.get());
}
#endif

// static
bool ChromeContentRendererClient::WasWebRequestUsedBySomeExtensions() {
#if defined(ENABLE_EXTENSIONS)
  return g_current_client->extension_dispatcher_
      ->WasWebRequestUsedBySomeExtensions();
#else
  return false;
#endif
}

const void* ChromeContentRendererClient::CreatePPAPIInterface(
    const std::string& interface_name) {
#if defined(ENABLE_PLUGINS)
#if !defined(DISABLE_NACL)
  if (interface_name == PPB_NACL_PRIVATE_INTERFACE)
    return nacl::GetNaClPrivateInterface();
#endif  // DISABLE_NACL
  if (interface_name == PPB_PDF_INTERFACE)
    return pdf::PPB_PDF_Impl::GetInterface();
#endif
  return NULL;
}

bool ChromeContentRendererClient::IsExternalPepperPlugin(
    const std::string& module_name) {
  // TODO(bbudge) remove this when the trusted NaCl plugin has been removed.
  // We must defer certain plugin events for NaCl instances since we switch
  // from the in-process to the out-of-process proxy after instantiating them.
  return module_name == "Native Client";
}

#if defined(ENABLE_PLUGINS) && defined(ENABLE_EXTENSIONS)
bool ChromeContentRendererClient::IsExtensionOrSharedModuleWhitelisted(
    const GURL& url, const std::set<std::string>& whitelist) {
  const extensions::ExtensionSet* extension_set =
      g_current_client->extension_dispatcher_->extensions();
  return chrome::IsExtensionOrSharedModuleWhitelisted(url, extension_set,
      whitelist);
}
#endif

blink::WebSpeechSynthesizer*
ChromeContentRendererClient::OverrideSpeechSynthesizer(
    blink::WebSpeechSynthesizerClient* client) {
  return new TtsDispatcher(client);
}

bool ChromeContentRendererClient::AllowPepperMediaStreamAPI(
    const GURL& url) {
#if !defined(OS_ANDROID)
  // Allow only the Hangouts app to use the MediaStream APIs. It's OK to check
  // the whitelist in the renderer, since we're only preventing access until
  // these APIs are public and stable.
  std::string url_host = url.host();
  if (url.SchemeIs("https") &&
      (EndsWith(url_host, "talkgadget.google.com", false) ||
       EndsWith(url_host, "plus.google.com", false) ||
       EndsWith(url_host, "plus.sandbox.google.com", false)) &&
      StartsWithASCII(url.path(), "/hangouts/", false)) {
    return true;
  }
  // Allow access for tests.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting)) {
    return true;
  }
#endif  // !defined(OS_ANDROID)
  return false;
}

void ChromeContentRendererClient::AddKeySystems(
    std::vector<media::KeySystemInfo>* key_systems) {
  AddChromeKeySystems(key_systems);
}

bool ChromeContentRendererClient::ShouldReportDetailedMessageForSource(
    const base::string16& source) const {
#if defined(ENABLE_EXTENSIONS)
  return extensions::IsSourceFromAnExtension(source);
#else
  return false;
#endif
}

bool ChromeContentRendererClient::ShouldEnableSiteIsolationPolicy() const {
  // SiteIsolationPolicy is off by default. We would like to activate cross-site
  // document blocking (for UMA data collection) for normal renderer processes
  // running a normal web page from the Internet. We only turn on
  // SiteIsolationPolicy for a renderer process that does not have the extension
  // flag on.
#if defined(ENABLE_EXTENSIONS)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(extensions::switches::kExtensionProcess);
#else
  return true;
#endif
}

blink::WebWorkerContentSettingsClientProxy*
ChromeContentRendererClient::CreateWorkerContentSettingsClientProxy(
    content::RenderFrame* render_frame,
    blink::WebFrame* frame) {
  return new WorkerContentSettingsClientProxy(render_frame, frame);
}

bool ChromeContentRendererClient::IsPluginAllowedToUseDevChannelAPIs() {
#if defined(ENABLE_PLUGINS)
  // Allow access for tests.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting)) {
    return true;
  }

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  // Allow dev channel APIs to be used on "Canary", "Dev", and "Unknown"
  // releases of Chrome. Permitting "Unknown" allows these APIs to be used on
  // Chromium builds as well.
  return channel <= chrome::VersionInfo::CHANNEL_DEV;
#else
  return false;
#endif
}

bool ChromeContentRendererClient::IsPluginAllowedToUseCameraDeviceAPI(
    const GURL& url) {
#if defined(ENABLE_PLUGINS) && defined(ENABLE_EXTENSIONS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting))
    return true;

  if (IsExtensionOrSharedModuleWhitelisted(url, allowed_camera_device_origins_))
    return true;
#endif

  return false;
}

bool ChromeContentRendererClient::IsPluginAllowedToUseCompositorAPI(
    const GURL& url) {
#if defined(ENABLE_PLUGINS) && defined(ENABLE_EXTENSIONS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting))
    return true;
  if (IsExtensionOrSharedModuleWhitelisted(url, allowed_compositor_origins_))
    return true;

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  return channel <= chrome::VersionInfo::CHANNEL_DEV;
#else
  return false;
#endif
}

content::BrowserPluginDelegate*
ChromeContentRendererClient::CreateBrowserPluginDelegate(
    content::RenderFrame* render_frame,
    const std::string& mime_type,
    const GURL& original_url) {
#if defined(ENABLE_EXTENSIONS)
  if (mime_type == content::kBrowserPluginMimeType) {
    return new extensions::ExtensionsGuestViewContainer(render_frame);
  } else {
    return new extensions::MimeHandlerViewContainer(
        render_frame, mime_type, original_url);
  }
#else
  return NULL;
#endif
}
