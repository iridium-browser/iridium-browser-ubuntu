// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pepper_flash.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/secure_origin_whitelist.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/common_resources.h"
#include "components/dom_distiller/core/url_constants.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "extensions/common/constants.h"
#include "gpu/config/gpu_info.h"
#include "net/http/http_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#elif defined(OS_MACOSX)
#include "components/nacl/common/nacl_sandbox_type_mac.h"
#endif

#if !defined(DISABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#include "components/nacl/common/nacl_process_type.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "chrome/common/pepper_flash.h"
#include "content/public/common/pepper_plugin_info.h"
#include "flapper_version.h"  // In SHARED_INTERMEDIATE_DIR.
#include "ppapi/shared_impl/ppapi_permissions.h"
#endif

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS) && \
    !defined(WIDEVINE_CDM_IS_COMPONENT)
#include "chrome/common/widevine_cdm_constants.h"
#endif

namespace {

#if defined(ENABLE_PLUGINS)
const char kPDFPluginExtension[] = "pdf";
const char kPDFPluginDescription[] = "Portable Document Format";
const char kPDFPluginOutOfProcessMimeType[] =
    "application/x-google-chrome-pdf";
const uint32 kPDFPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                     ppapi::PERMISSION_DEV;

content::PepperPluginInfo::GetInterfaceFunc g_pdf_get_interface;
content::PepperPluginInfo::PPP_InitializeModuleFunc g_pdf_initialize_module;
content::PepperPluginInfo::PPP_ShutdownModuleFunc g_pdf_shutdown_module;

#if defined(ENABLE_REMOTING)

content::PepperPluginInfo::GetInterfaceFunc g_remoting_get_interface;
content::PepperPluginInfo::PPP_InitializeModuleFunc
    g_remoting_initialize_module;
content::PepperPluginInfo::PPP_ShutdownModuleFunc g_remoting_shutdown_module;

#if defined(GOOGLE_CHROME_BUILD)
const char kRemotingViewerPluginName[] = "Chrome Remote Desktop Viewer";
#else
const char kRemotingViewerPluginName[] = "Chromoting Viewer";
#endif  // defined(GOOGLE_CHROME_BUILD)
const char kRemotingViewerPluginDescription[] =
    "This plugin allows you to securely access other computers that have been "
    "shared with you. To use this plugin you must first install the "
    "<a href=\"https://chrome.google.com/remotedesktop\">"
    "Chrome Remote Desktop</a> webapp.";
// Use a consistent MIME-type regardless of branding.
const char kRemotingViewerPluginMimeType[] =
    "application/vnd.chromium.remoting-viewer";
const char kRemotingViewerPluginMimeExtension[] = "";
const char kRemotingViewerPluginMimeDescription[] = "";
const uint32 kRemotingViewerPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                                ppapi::PERMISSION_DEV;
#endif  // defined(ENABLE_REMOTING)

#if !defined(DISABLE_NACL)
content::PepperPluginInfo::GetInterfaceFunc g_nacl_get_interface;
content::PepperPluginInfo::PPP_InitializeModuleFunc g_nacl_initialize_module;
content::PepperPluginInfo::PPP_ShutdownModuleFunc g_nacl_shutdown_module;
#endif

// Appends the known built-in plugins to the given vector. Some built-in
// plugins are "internal" which means they are compiled into the Chrome binary,
// and some are extra shared libraries distributed with the browser (these are
// not marked internal, aside from being automatically registered, they're just
// regular plugins).
void ComputeBuiltInPlugins(std::vector<content::PepperPluginInfo>* plugins) {
  content::PepperPluginInfo pdf_info;
  pdf_info.is_internal = true;
  pdf_info.is_out_of_process = true;
  pdf_info.name = ChromeContentClient::kPDFPluginName;
  pdf_info.description = kPDFPluginDescription;
  pdf_info.path = base::FilePath::FromUTF8Unsafe(
      ChromeContentClient::kPDFPluginPath);
  content::WebPluginMimeType pdf_mime_type(
      kPDFPluginOutOfProcessMimeType,
      kPDFPluginExtension,
      kPDFPluginDescription);
  pdf_info.mime_types.push_back(pdf_mime_type);
  pdf_info.internal_entry_points.get_interface = g_pdf_get_interface;
  pdf_info.internal_entry_points.initialize_module = g_pdf_initialize_module;
  pdf_info.internal_entry_points.shutdown_module = g_pdf_shutdown_module;
  pdf_info.permissions = kPDFPluginPermissions;
  plugins->push_back(pdf_info);

  base::FilePath path;

#if !defined(DISABLE_NACL)
  // Handle Native Client just like the PDF plugin. This means that it is
  // enabled by default for the non-portable case.  This allows apps installed
  // from the Chrome Web Store to use NaCl even if the command line switch
  // isn't set.  For other uses of NaCl we check for the command line switch.
  if (PathService::Get(chrome::FILE_NACL_PLUGIN, &path)) {
    content::PepperPluginInfo nacl;
    // The nacl plugin is now built into the Chromium binary.
    nacl.is_internal = true;
    nacl.path = path;
    nacl.name = nacl::kNaClPluginName;
    content::WebPluginMimeType nacl_mime_type(nacl::kNaClPluginMimeType,
                                              nacl::kNaClPluginExtension,
                                              nacl::kNaClPluginDescription);
    nacl.mime_types.push_back(nacl_mime_type);
    content::WebPluginMimeType pnacl_mime_type(nacl::kPnaclPluginMimeType,
                                               nacl::kPnaclPluginExtension,
                                               nacl::kPnaclPluginDescription);
    nacl.mime_types.push_back(pnacl_mime_type);
    nacl.internal_entry_points.get_interface = g_nacl_get_interface;
    nacl.internal_entry_points.initialize_module = g_nacl_initialize_module;
    nacl.internal_entry_points.shutdown_module = g_nacl_shutdown_module;
    nacl.permissions = ppapi::PERMISSION_PRIVATE | ppapi::PERMISSION_DEV;
    plugins->push_back(nacl);
  }
#endif  // !defined(DISABLE_NACL)

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS) && \
    !defined(WIDEVINE_CDM_IS_COMPONENT)
  static bool skip_widevine_cdm_file_check = false;
  if (PathService::Get(chrome::FILE_WIDEVINE_CDM_ADAPTER, &path)) {
    if (skip_widevine_cdm_file_check || base::PathExists(path)) {
      content::PepperPluginInfo widevine_cdm;
      widevine_cdm.is_out_of_process = true;
      widevine_cdm.path = path;
      widevine_cdm.name = kWidevineCdmDisplayName;
      widevine_cdm.description = kWidevineCdmDescription +
                                 std::string(" (version: ") +
                                 WIDEVINE_CDM_VERSION_STRING + ")";
      widevine_cdm.version = WIDEVINE_CDM_VERSION_STRING;
      content::WebPluginMimeType widevine_cdm_mime_type(
          kWidevineCdmPluginMimeType,
          kWidevineCdmPluginExtension,
          kWidevineCdmPluginMimeTypeDescription);

      // Add the supported codecs as if they came from the component manifest.
      std::vector<std::string> codecs;
      codecs.push_back(kCdmSupportedCodecVorbis);
      codecs.push_back(kCdmSupportedCodecVp8);
      codecs.push_back(kCdmSupportedCodecVp9);
#if defined(USE_PROPRIETARY_CODECS)
      codecs.push_back(kCdmSupportedCodecAac);
      codecs.push_back(kCdmSupportedCodecAvc1);
#endif  // defined(USE_PROPRIETARY_CODECS)
      std::string codec_string =
          JoinString(codecs, kCdmSupportedCodecsValueDelimiter);
      widevine_cdm_mime_type.additional_param_names.push_back(
          base::ASCIIToUTF16(kCdmSupportedCodecsParamName));
      widevine_cdm_mime_type.additional_param_values.push_back(
          base::ASCIIToUTF16(codec_string));

      widevine_cdm.mime_types.push_back(widevine_cdm_mime_type);
      widevine_cdm.permissions = kWidevineCdmPluginPermissions;
      plugins->push_back(widevine_cdm);

      skip_widevine_cdm_file_check = true;
    }
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS) &&
        // !defined(WIDEVINE_CDM_IS_COMPONENT)

  // The Remoting Viewer plugin is built-in.
#if defined(ENABLE_REMOTING)
  content::PepperPluginInfo info;
  info.is_internal = true;
  info.is_out_of_process = true;
  info.name = kRemotingViewerPluginName;
  info.description = kRemotingViewerPluginDescription;
  info.path = base::FilePath::FromUTF8Unsafe(
      ChromeContentClient::kRemotingViewerPluginPath);
  content::WebPluginMimeType remoting_mime_type(
      kRemotingViewerPluginMimeType,
      kRemotingViewerPluginMimeExtension,
      kRemotingViewerPluginMimeDescription);
  info.mime_types.push_back(remoting_mime_type);
  info.internal_entry_points.get_interface = g_remoting_get_interface;
  info.internal_entry_points.initialize_module = g_remoting_initialize_module;
  info.internal_entry_points.shutdown_module = g_remoting_shutdown_module;
  info.permissions = kRemotingViewerPluginPermissions;

  plugins->push_back(info);
#endif
}

content::PepperPluginInfo CreatePepperFlashInfo(const base::FilePath& path,
                                                const std::string& version) {
  content::PepperPluginInfo plugin;

  plugin.is_out_of_process = true;
  plugin.name = content::kFlashPluginName;
  plugin.path = path;
  plugin.permissions = chrome::kPepperFlashPermissions;

  std::vector<std::string> flash_version_numbers;
  base::SplitString(version, '.', &flash_version_numbers);
  if (flash_version_numbers.size() < 1)
    flash_version_numbers.push_back("11");
  // |SplitString()| puts in an empty string given an empty string. :(
  else if (flash_version_numbers[0].empty())
    flash_version_numbers[0] = "11";
  if (flash_version_numbers.size() < 2)
    flash_version_numbers.push_back("2");
  if (flash_version_numbers.size() < 3)
    flash_version_numbers.push_back("999");
  if (flash_version_numbers.size() < 4)
    flash_version_numbers.push_back("999");
  // E.g., "Shockwave Flash 10.2 r154":
  plugin.description = plugin.name + " " + flash_version_numbers[0] + "." +
      flash_version_numbers[1] + " r" + flash_version_numbers[2];
  plugin.version = JoinString(flash_version_numbers, '.');
  content::WebPluginMimeType swf_mime_type(content::kFlashPluginSwfMimeType,
                                           content::kFlashPluginSwfExtension,
                                           content::kFlashPluginSwfDescription);
  plugin.mime_types.push_back(swf_mime_type);
  content::WebPluginMimeType spl_mime_type(content::kFlashPluginSplMimeType,
                                           content::kFlashPluginSplExtension,
                                           content::kFlashPluginSplDescription);
  plugin.mime_types.push_back(spl_mime_type);

  return plugin;
}

void AddPepperFlashFromCommandLine(
    std::vector<content::PepperPluginInfo>* plugins) {
  const base::CommandLine::StringType flash_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kPpapiFlashPath);
  if (flash_path.empty())
    return;

  // Also get the version from the command-line. Should be something like 11.2
  // or 11.2.123.45.
  std::string flash_version =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPpapiFlashVersion);

  plugins->push_back(
      CreatePepperFlashInfo(base::FilePath(flash_path), flash_version));
}

bool GetBundledPepperFlash(content::PepperPluginInfo* plugin) {
#if defined(FLAPPER_AVAILABLE)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Ignore bundled Pepper Flash if there is Pepper Flash specified from the
  // command-line.
  if (command_line->HasSwitch(switches::kPpapiFlashPath))
    return false;

  bool force_disable =
      command_line->HasSwitch(switches::kDisableBundledPpapiFlash);
  if (force_disable)
    return false;

  base::FilePath flash_path;
  if (!PathService::Get(chrome::FILE_PEPPER_FLASH_PLUGIN, &flash_path))
    return false;

  *plugin = CreatePepperFlashInfo(flash_path, FLAPPER_VERSION_STRING);
  return true;
#else
  return false;
#endif  // FLAPPER_AVAILABLE
}

#if defined(OS_WIN)
const char kPepperFlashDLLBaseName[] =
#if defined(ARCH_CPU_X86)
    "pepflashplayer32_";
#elif defined(ARCH_CPU_X86_64)
    "pepflashplayer64_";
#else
#error Unsupported Windows CPU architecture.
#endif  // defined(ARCH_CPU_X86)
#endif  // defined(OS_WIN)

bool GetSystemPepperFlash(content::PepperPluginInfo* plugin) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
#if defined(FLAPPER_AVAILABLE)
  // If flapper is available, only try system plugin if
  // --disable-bundled-ppapi-flash is specified.
  if (!command_line->HasSwitch(switches::kDisableBundledPpapiFlash))
    return false;
#endif  // defined(FLAPPER_AVAILABLE)

  // Do not try and find System Pepper Flash if there is a specific path on
  // the commmand-line.
  if (command_line->HasSwitch(switches::kPpapiFlashPath))
    return false;

  base::FilePath flash_path;
  if (!PathService::Get(chrome::DIR_PEPPER_FLASH_SYSTEM_PLUGIN, &flash_path))
    return false;

  if (!base::PathExists(flash_path))
    return false;

  base::FilePath manifest_path(flash_path.AppendASCII("manifest.json"));

  std::string manifest_data;
  if (!base::ReadFileToString(manifest_path, &manifest_data))
    return false;
  scoped_ptr<base::Value> manifest_value(
      base::JSONReader::Read(manifest_data, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!manifest_value.get())
    return false;
  base::DictionaryValue* manifest = NULL;
  if (!manifest_value->GetAsDictionary(&manifest))
    return false;

  Version version;
  if (!chrome::CheckPepperFlashManifest(*manifest, &version))
    return false;

#if defined(OS_WIN)
  // PepperFlash DLLs on Windows look like basename_v_x_y_z.dll.
  std::string filename(kPepperFlashDLLBaseName);
  filename.append(version.GetString());
  base::ReplaceChars(filename, ".", "_", &filename);
  filename.append(".dll");

  base::FilePath path(flash_path.Append(base::ASCIIToUTF16(filename)));
#else
  // PepperFlash on OS X is called PepperFlashPlayer.plugin
  base::FilePath path(flash_path.Append(chrome::kPepperFlashPluginFilename));
#endif

  if (!base::PathExists(path))
    return false;

  *plugin = CreatePepperFlashInfo(path, version.GetString());
  return true;
}
#endif  //  defined(ENABLE_PLUGINS)

std::string GetProduct() {
  chrome::VersionInfo version_info;
  return version_info.ProductNameAndVersionForUserAgent();
}

}  // namespace

std::string GetUserAgent() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserAgent)) {
    std::string ua = command_line->GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(ua))
      return ua;
    LOG(WARNING) << "Ignored invalid value for flag --" << switches::kUserAgent;
  }

  std::string product = GetProduct();
#if defined(OS_ANDROID)
  if (command_line->HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";
#endif
  return content::BuildUserAgentFromProduct(product);
}


#if defined(ENABLE_REMOTING)

void ChromeContentClient::SetRemotingEntryFunctions(
    content::PepperPluginInfo::GetInterfaceFunc get_interface,
    content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
    content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module) {
  g_remoting_get_interface = get_interface;
  g_remoting_initialize_module = initialize_module;
  g_remoting_shutdown_module = shutdown_module;
}
#endif

#if !defined(DISABLE_NACL)
void ChromeContentClient::SetNaClEntryFunctions(
    content::PepperPluginInfo::GetInterfaceFunc get_interface,
    content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
    content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module) {
  g_nacl_get_interface = get_interface;
  g_nacl_initialize_module = initialize_module;
  g_nacl_shutdown_module = shutdown_module;
}
#endif

#if defined(ENABLE_PLUGINS)
void ChromeContentClient::SetPDFEntryFunctions(
    content::PepperPluginInfo::GetInterfaceFunc get_interface,
    content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
    content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module) {
  g_pdf_get_interface = get_interface;
  g_pdf_initialize_module = initialize_module;
  g_pdf_shutdown_module = shutdown_module;
}
#endif

void ChromeContentClient::SetActiveURL(const GURL& url) {
  base::debug::SetCrashKeyValue(crash_keys::kActiveURL,
                                url.possibly_invalid_spec());
}

void ChromeContentClient::SetGpuInfo(const gpu::GPUInfo& gpu_info) {
#if !defined(OS_ANDROID)
  base::debug::SetCrashKeyValue(crash_keys::kGPUVendorID,
      base::StringPrintf("0x%04x", gpu_info.gpu.vendor_id));
  base::debug::SetCrashKeyValue(crash_keys::kGPUDeviceID,
      base::StringPrintf("0x%04x", gpu_info.gpu.device_id));
#endif
  base::debug::SetCrashKeyValue(crash_keys::kGPUDriverVersion,
      gpu_info.driver_version);
  base::debug::SetCrashKeyValue(crash_keys::kGPUPixelShaderVersion,
      gpu_info.pixel_shader_version);
  base::debug::SetCrashKeyValue(crash_keys::kGPUVertexShaderVersion,
      gpu_info.vertex_shader_version);
#if defined(OS_MACOSX)
  base::debug::SetCrashKeyValue(crash_keys::kGPUGLVersion, gpu_info.gl_version);
#elif defined(OS_POSIX)
  base::debug::SetCrashKeyValue(crash_keys::kGPUVendor, gpu_info.gl_vendor);
  base::debug::SetCrashKeyValue(crash_keys::kGPURenderer, gpu_info.gl_renderer);
#endif
}

void ChromeContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
#if defined(ENABLE_PLUGINS)
  ComputeBuiltInPlugins(plugins);
  AddPepperFlashFromCommandLine(plugins);

  content::PepperPluginInfo plugin;
  if (GetBundledPepperFlash(&plugin))
    plugins->push_back(plugin);
  if (GetSystemPepperFlash(&plugin))
    plugins->push_back(plugin);
#endif
}

void ChromeContentClient::AddAdditionalSchemes(
    std::vector<std::string>* standard_schemes,
    std::vector<std::string>* savable_schemes) {
  standard_schemes->push_back(extensions::kExtensionScheme);
  savable_schemes->push_back(extensions::kExtensionScheme);
  standard_schemes->push_back(chrome::kChromeNativeScheme);
  standard_schemes->push_back(extensions::kExtensionResourceScheme);
  savable_schemes->push_back(extensions::kExtensionResourceScheme);
  standard_schemes->push_back(chrome::kChromeSearchScheme);
  savable_schemes->push_back(chrome::kChromeSearchScheme);
  standard_schemes->push_back(dom_distiller::kDomDistillerScheme);
  savable_schemes->push_back(dom_distiller::kDomDistillerScheme);
#if defined(OS_CHROMEOS)
  standard_schemes->push_back(chrome::kCrosScheme);
#endif
}

std::string ChromeContentClient::GetProduct() const {
  return ::GetProduct();
}

std::string ChromeContentClient::GetUserAgent() const {
  return ::GetUserAgent();
}

base::string16 ChromeContentClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ChromeContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedStaticMemory* ChromeContentClient::GetDataResourceBytes(
    int resource_id) const {
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id);
}

gfx::Image& ChromeContentClient::GetNativeImageNamed(int resource_id) const {
  return ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
}

std::string ChromeContentClient::GetProcessTypeNameInEnglish(int type) {
#if !defined(DISABLE_NACL)
  switch (type) {
    case PROCESS_TYPE_NACL_LOADER:
      return "Native Client module";
    case PROCESS_TYPE_NACL_BROKER:
      return "Native Client broker";
  }
#endif

  NOTREACHED() << "Unknown child process type!";
  return "Unknown";
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
bool ChromeContentClient::GetSandboxProfileForSandboxType(
    int sandbox_type,
    int* sandbox_profile_resource_id) const {
  DCHECK(sandbox_profile_resource_id);
  if (sandbox_type == NACL_SANDBOX_TYPE_NACL_LOADER) {
    *sandbox_profile_resource_id = IDR_NACL_SANDBOX_PROFILE;
    return true;
  }
  return false;
}
#endif

void ChromeContentClient::AddSecureSchemesAndOrigins(
    std::set<std::string>* schemes,
    std::set<GURL>* origins) {
  schemes->insert(content::kChromeUIScheme);
  schemes->insert(extensions::kExtensionScheme);
  schemes->insert(extensions::kExtensionResourceScheme);
  GetSecureOriginWhitelist(origins);
}
