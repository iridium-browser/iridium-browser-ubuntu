// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/plugin/pnacl_translate_thread.h"

#include <iterator>
#include <sstream>

#include "components/nacl/renderer/plugin/plugin.h"
#include "components/nacl/renderer/plugin/plugin_error.h"
#include "components/nacl/renderer/plugin/pnacl_resources.h"
#include "components/nacl/renderer/plugin/srpc_params.h"
#include "components/nacl/renderer/plugin/temporary_file.h"
#include "components/nacl/renderer/plugin/utility.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/cpp/var.h"

namespace plugin {
namespace {

template <typename Val>
std::string MakeCommandLineArg(const char* key, const Val val) {
  std::stringstream ss;
  ss << key << val;
  return ss.str();
}

void GetLlcCommandLine(std::vector<char>* split_args,
                       size_t obj_files_size,
                       int32_t opt_level,
                       bool is_debug,
                       const std::string& architecture_attributes) {
  typedef std::vector<std::string> Args;
  Args args;

  // TODO(dschuff): This CL override is ugly. Change llc to default to
  // using the number of modules specified in the first param, and
  // ignore multiple uses of -split-module
  args.push_back(MakeCommandLineArg("-split-module=", obj_files_size));
  args.push_back(MakeCommandLineArg("-O", opt_level));
  if (is_debug)
    args.push_back("-bitcode-format=llvm");
  if (!architecture_attributes.empty())
    args.push_back("-mattr=" + architecture_attributes);

  for (const std::string& arg : args) {
    std::copy(arg.begin(), arg.end(), std::back_inserter(*split_args));
    split_args->push_back('\x00');
  }
}

void GetSubzeroCommandLine(std::vector<char>* split_args,
                           int32_t opt_level,
                           bool is_debug,
                           const std::string& architecture_attributes) {
  typedef std::vector<std::string> Args;
  Args args;

  args.push_back(MakeCommandLineArg("-O", opt_level));
  DCHECK(!is_debug);
  // TODO(stichnot): enable this once the mattr flag formatting is
  // compatible: https://code.google.com/p/nativeclient/issues/detail?id=4132
  // if (!architecture_attributes.empty())
  //   args.push_back("-mattr=" + architecture_attributes);

  for (const std::string& arg : args) {
    std::copy(arg.begin(), arg.end(), std::back_inserter(*split_args));
    split_args->push_back('\x00');
  }
}

}  // namespace

PnaclTranslateThread::PnaclTranslateThread()
    : compiler_subprocess_active_(false),
      ld_subprocess_active_(false),
      subprocesses_aborted_(false),
      done_(false),
      compile_time_(0),
      obj_files_(NULL),
      num_threads_(0),
      nexe_file_(NULL),
      coordinator_error_info_(NULL),
      resources_(NULL),
      coordinator_(NULL),
      plugin_(NULL) {
  NaClXMutexCtor(&subprocess_mu_);
  NaClXMutexCtor(&cond_mu_);
  NaClXCondVarCtor(&buffer_cond_);
}

void PnaclTranslateThread::RunTranslate(
    const pp::CompletionCallback& finish_callback,
    const std::vector<TempFile*>* obj_files,
    int num_threads,
    TempFile* nexe_file,
    nacl::DescWrapper* invalid_desc_wrapper,
    ErrorInfo* error_info,
    PnaclResources* resources,
    PP_PNaClOptions* pnacl_options,
    const std::string& architecture_attributes,
    PnaclCoordinator* coordinator,
    Plugin* plugin) {
  PLUGIN_PRINTF(("PnaclStreamingTranslateThread::RunTranslate)\n"));
  obj_files_ = obj_files;
  num_threads_ = num_threads;
  nexe_file_ = nexe_file;
  invalid_desc_wrapper_ = invalid_desc_wrapper;
  coordinator_error_info_ = error_info;
  resources_ = resources;
  pnacl_options_ = pnacl_options;
  architecture_attributes_ = architecture_attributes;
  coordinator_ = coordinator;
  plugin_ = plugin;

  // Invoke llc followed by ld off the main thread.  This allows use of
  // blocking RPCs that would otherwise block the JavaScript main thread.
  report_translate_finished_ = finish_callback;
  translate_thread_.reset(new NaClThread);
  if (translate_thread_ == NULL) {
    TranslateFailed(PP_NACL_ERROR_PNACL_THREAD_CREATE,
                    "could not allocate thread struct.");
    return;
  }
  const int32_t kArbitraryStackSize = 128 * 1024;
  if (!NaClThreadCreateJoinable(translate_thread_.get(),
                                DoTranslateThread,
                                this,
                                kArbitraryStackSize)) {
    TranslateFailed(PP_NACL_ERROR_PNACL_THREAD_CREATE,
                    "could not create thread.");
    translate_thread_.reset(NULL);
  }
}

// Called from main thread to send bytes to the translator.
void PnaclTranslateThread::PutBytes(const void* bytes, int32_t count) {
  CHECK(bytes != NULL);
  NaClXMutexLock(&cond_mu_);
  data_buffers_.push_back(std::vector<char>());
  data_buffers_.back().insert(data_buffers_.back().end(),
                              static_cast<const char*>(bytes),
                              static_cast<const char*>(bytes) + count);
  NaClXCondVarSignal(&buffer_cond_);
  NaClXMutexUnlock(&cond_mu_);
}

void PnaclTranslateThread::EndStream() {
  NaClXMutexLock(&cond_mu_);
  done_ = true;
  NaClXCondVarSignal(&buffer_cond_);
  NaClXMutexUnlock(&cond_mu_);
}

void WINAPI PnaclTranslateThread::DoTranslateThread(void* arg) {
  PnaclTranslateThread* translator =
      reinterpret_cast<PnaclTranslateThread*>(arg);
  translator->DoTranslate();
}

void PnaclTranslateThread::DoTranslate() {
  ErrorInfo error_info;
  SrpcParams params;
  std::vector<nacl::DescWrapper*> compile_out_files;
  size_t i;
  for (i = 0; i < obj_files_->size(); i++)
    compile_out_files.push_back((*obj_files_)[i]->write_wrapper());
  for (; i < PnaclCoordinator::kMaxTranslatorObjectFiles; i++)
    compile_out_files.push_back(invalid_desc_wrapper_);

  PLUGIN_PRINTF(
      ("DoTranslate using subzero: %d\n", pnacl_options_->use_subzero));

  pp::Core* core = pp::Module::Get()->core();
  int64_t compiler_load_start_time = NaClGetTimeOfDayMicroseconds();
  PnaclResources::ResourceType compiler_type = pnacl_options_->use_subzero
                                                   ? PnaclResources::SUBZERO
                                                   : PnaclResources::LLC;
  // On success, ownership of file_info is transferred.
  PP_NaClFileInfo file_info = resources_->TakeFileInfo(compiler_type);
  const std::string& url = resources_->GetUrl(compiler_type);
  NaClSubprocess* compiler_subprocess =
      plugin_->LoadHelperNaClModule(url, file_info, &error_info);
  if (compiler_subprocess == NULL) {
    if (file_info.handle != PP_kInvalidFileHandle)
      CloseFileHandle(file_info.handle);
    TranslateFailed(PP_NACL_ERROR_PNACL_LLC_SETUP,
                    "Compile process could not be created: " +
                    error_info.message());
    return;
  }
  int64_t compiler_load_time_total =
      NaClGetTimeOfDayMicroseconds() - compiler_load_start_time;
  GetNaClInterface()->LogTranslateTime("NaCl.Perf.PNaClLoadTime.LoadCompiler",
                                       compiler_load_time_total);
  GetNaClInterface()->LogTranslateTime(
      pnacl_options_->use_subzero
          ? "NaCl.Perf.PNaClLoadTime.LoadCompiler.Subzero"
          : "NaCl.Perf.PNaClLoadTime.LoadCompiler.LLC",
      compiler_load_time_total);

  {
    nacl::MutexLocker ml(&subprocess_mu_);
    // If we received a call to AbortSubprocesses() before we had a chance to
    // set compiler_subprocess_, shut down and clean up the subprocess started
    // here.
    if (subprocesses_aborted_) {
      compiler_subprocess->service_runtime()->Shutdown();
      delete compiler_subprocess;
      return;
    }
    compiler_subprocess_.reset(compiler_subprocess);
    compiler_subprocess = NULL;
    compiler_subprocess_active_ = true;
  }

  int64_t do_compile_start_time = NaClGetTimeOfDayMicroseconds();
  bool init_success;

  std::vector<char> split_args;
  if (pnacl_options_->use_subzero) {
    GetSubzeroCommandLine(&split_args, pnacl_options_->opt_level,
                          pnacl_options_->is_debug, architecture_attributes_);
  } else {
    GetLlcCommandLine(&split_args, obj_files_->size(),
                      pnacl_options_->opt_level, pnacl_options_->is_debug,
                      architecture_attributes_);
  }
  init_success = compiler_subprocess_->InvokeSrpcMethod(
      "StreamInitWithSplit", "ihhhhhhhhhhhhhhhhC", &params, num_threads_,
      compile_out_files[0]->desc(), compile_out_files[1]->desc(),
      compile_out_files[2]->desc(), compile_out_files[3]->desc(),
      compile_out_files[4]->desc(), compile_out_files[5]->desc(),
      compile_out_files[6]->desc(), compile_out_files[7]->desc(),
      compile_out_files[8]->desc(), compile_out_files[9]->desc(),
      compile_out_files[10]->desc(), compile_out_files[11]->desc(),
      compile_out_files[12]->desc(), compile_out_files[13]->desc(),
      compile_out_files[14]->desc(), compile_out_files[15]->desc(),
      &split_args[0], split_args.size());
  if (!init_success) {
    if (compiler_subprocess_->srpc_client()->GetLastError() ==
        NACL_SRPC_RESULT_APP_ERROR) {
      // The error message is only present if the error was returned from llc
      TranslateFailed(PP_NACL_ERROR_PNACL_LLC_INTERNAL,
                      std::string("Stream init failed: ") +
                      std::string(params.outs()[0]->arrays.str));
    } else {
      TranslateFailed(PP_NACL_ERROR_PNACL_LLC_INTERNAL,
                      "Stream init internal error");
    }
    return;
  }
  PLUGIN_PRINTF(("PnaclCoordinator: StreamInit successful\n"));

  // llc process is started.
  while(!done_ || data_buffers_.size() > 0) {
    NaClXMutexLock(&cond_mu_);
    while(!done_ && data_buffers_.size() == 0) {
      NaClXCondVarWait(&buffer_cond_, &cond_mu_);
    }
    PLUGIN_PRINTF(("PnaclTranslateThread awake (done=%d, size=%" NACL_PRIuS
                   ")\n",
                   done_, data_buffers_.size()));
    if (data_buffers_.size() > 0) {
      std::vector<char> data;
      data.swap(data_buffers_.front());
      data_buffers_.pop_front();
      NaClXMutexUnlock(&cond_mu_);
      PLUGIN_PRINTF(("StreamChunk\n"));
      if (!compiler_subprocess_->InvokeSrpcMethod("StreamChunk", "C", &params,
                                                  &data[0], data.size())) {
        if (compiler_subprocess_->srpc_client()->GetLastError() !=
            NACL_SRPC_RESULT_APP_ERROR) {
          // If the error was reported by the translator, then we fall through
          // and call StreamEnd, which returns a string describing the error,
          // which we can then send to the Javascript console. Otherwise just
          // fail here, since the translator has probably crashed or asserted.
          TranslateFailed(PP_NACL_ERROR_PNACL_LLC_INTERNAL,
                          "Compile stream chunk failed. "
                          "The PNaCl translator has probably crashed.");
          return;
        }
        break;
      } else {
        PLUGIN_PRINTF(("StreamChunk Successful\n"));
        core->CallOnMainThread(
            0,
            coordinator_->GetCompileProgressCallback(data.size()),
            PP_OK);
      }
    } else {
      NaClXMutexUnlock(&cond_mu_);
    }
  }
  PLUGIN_PRINTF(("PnaclTranslateThread done with chunks\n"));
  // Finish llc.
  if (!compiler_subprocess_->InvokeSrpcMethod("StreamEnd", std::string(),
                                              &params)) {
    PLUGIN_PRINTF(("PnaclTranslateThread StreamEnd failed\n"));
    if (compiler_subprocess_->srpc_client()->GetLastError() ==
        NACL_SRPC_RESULT_APP_ERROR) {
      // The error string is only present if the error was sent back from llc.
      TranslateFailed(PP_NACL_ERROR_PNACL_LLC_INTERNAL,
                      params.outs()[3]->arrays.str);
    } else {
      TranslateFailed(PP_NACL_ERROR_PNACL_LLC_INTERNAL,
                      "Compile StreamEnd internal error");
    }
    return;
  }
  compile_time_ = NaClGetTimeOfDayMicroseconds() - do_compile_start_time;
  GetNaClInterface()->LogTranslateTime("NaCl.Perf.PNaClLoadTime.CompileTime",
                                       compile_time_);
  GetNaClInterface()->LogTranslateTime(
      pnacl_options_->use_subzero
          ? "NaCl.Perf.PNaClLoadTime.CompileTime.Subzero"
          : "NaCl.Perf.PNaClLoadTime.CompileTime.LLC",
      compile_time_);

  // Shut down the llc subprocess.
  NaClXMutexLock(&subprocess_mu_);
  compiler_subprocess_active_ = false;
  compiler_subprocess_.reset(NULL);
  NaClXMutexUnlock(&subprocess_mu_);

  if(!RunLdSubprocess()) {
    return;
  }
  core->CallOnMainThread(0, report_translate_finished_, PP_OK);
}

bool PnaclTranslateThread::RunLdSubprocess() {
  ErrorInfo error_info;
  SrpcParams params;

  std::vector<nacl::DescWrapper*> ld_in_files;
  size_t i;
  for (i = 0; i < obj_files_->size(); i++) {
    // Reset object file for reading first.
    if (!(*obj_files_)[i]->Reset()) {
      TranslateFailed(PP_NACL_ERROR_PNACL_LD_SETUP,
                      "Link process could not reset object file");
      return false;
    }
    ld_in_files.push_back((*obj_files_)[i]->read_wrapper());
  }
  for (; i < PnaclCoordinator::kMaxTranslatorObjectFiles; i++)
    ld_in_files.push_back(invalid_desc_wrapper_);

  nacl::DescWrapper* ld_out_file = nexe_file_->write_wrapper();
  int64_t ld_start_time = NaClGetTimeOfDayMicroseconds();
  PP_NaClFileInfo ld_file_info = resources_->TakeFileInfo(PnaclResources::LD);
  // On success, ownership of ld_file_info is transferred.
  nacl::scoped_ptr<NaClSubprocess> ld_subprocess(plugin_->LoadHelperNaClModule(
      resources_->GetUrl(PnaclResources::LD), ld_file_info, &error_info));
  if (ld_subprocess.get() == NULL) {
    if (ld_file_info.handle != PP_kInvalidFileHandle)
      CloseFileHandle(ld_file_info.handle);
    TranslateFailed(PP_NACL_ERROR_PNACL_LD_SETUP,
                    "Link process could not be created: " +
                    error_info.message());
    return false;
  }
  GetNaClInterface()->LogTranslateTime(
      "NaCl.Perf.PNaClLoadTime.LoadLinker",
      NaClGetTimeOfDayMicroseconds() - ld_start_time);
  {
    nacl::MutexLocker ml(&subprocess_mu_);
    // If we received a call to AbortSubprocesses() before we had a chance to
    // set ld_subprocess_, shut down and clean up the subprocess started here.
    if (subprocesses_aborted_) {
      ld_subprocess->service_runtime()->Shutdown();
      return false;
    }
    DCHECK(ld_subprocess_.get() == NULL);
    ld_subprocess_.swap(ld_subprocess);
    ld_subprocess_active_ = true;
  }

  int64_t link_start_time = NaClGetTimeOfDayMicroseconds();
  // Run LD.
  bool success = ld_subprocess_->InvokeSrpcMethod(
      "RunWithSplit",
      "ihhhhhhhhhhhhhhhhh",
      &params,
      static_cast<int>(obj_files_->size()),
      ld_in_files[0]->desc(),
      ld_in_files[1]->desc(),
      ld_in_files[2]->desc(),
      ld_in_files[3]->desc(),
      ld_in_files[4]->desc(),
      ld_in_files[5]->desc(),
      ld_in_files[6]->desc(),
      ld_in_files[7]->desc(),
      ld_in_files[8]->desc(),
      ld_in_files[9]->desc(),
      ld_in_files[10]->desc(),
      ld_in_files[11]->desc(),
      ld_in_files[12]->desc(),
      ld_in_files[13]->desc(),
      ld_in_files[14]->desc(),
      ld_in_files[15]->desc(),
      ld_out_file->desc());
  if (!success) {
    TranslateFailed(PP_NACL_ERROR_PNACL_LD_INTERNAL,
                    "link failed.");
    return false;
  }
  GetNaClInterface()->LogTranslateTime(
      "NaCl.Perf.PNaClLoadTime.LinkTime",
      NaClGetTimeOfDayMicroseconds() - link_start_time);
  PLUGIN_PRINTF(("PnaclCoordinator: link (translator=%p) succeeded\n",
                 this));
  // Shut down the ld subprocess.
  NaClXMutexLock(&subprocess_mu_);
  ld_subprocess_active_ = false;
  ld_subprocess_.reset(NULL);
  NaClXMutexUnlock(&subprocess_mu_);
  return true;
}

void PnaclTranslateThread::TranslateFailed(
    PP_NaClError err_code,
    const std::string& error_string) {
  PLUGIN_PRINTF(("PnaclTranslateThread::TranslateFailed (error_string='%s')\n",
                 error_string.c_str()));
  pp::Core* core = pp::Module::Get()->core();
  if (coordinator_error_info_->message().empty()) {
    // Only use our message if one hasn't already been set by the coordinator
    // (e.g. pexe load failed).
    coordinator_error_info_->SetReport(err_code,
                                       std::string("PnaclCoordinator: ") +
                                       error_string);
  }
  core->CallOnMainThread(0, report_translate_finished_, PP_ERROR_FAILED);
}

void PnaclTranslateThread::AbortSubprocesses() {
  PLUGIN_PRINTF(("PnaclTranslateThread::AbortSubprocesses\n"));
  NaClXMutexLock(&subprocess_mu_);
  if (compiler_subprocess_ != NULL && compiler_subprocess_active_) {
    compiler_subprocess_->service_runtime()->Shutdown();
    compiler_subprocess_active_ = false;
  }
  if (ld_subprocess_ != NULL && ld_subprocess_active_) {
    ld_subprocess_->service_runtime()->Shutdown();
    ld_subprocess_active_ = false;
  }
  subprocesses_aborted_ = true;
  NaClXMutexUnlock(&subprocess_mu_);
  nacl::MutexLocker ml(&cond_mu_);
  done_ = true;
  // Free all buffered bitcode chunks
  data_buffers_.clear();
  NaClXCondVarSignal(&buffer_cond_);
}

PnaclTranslateThread::~PnaclTranslateThread() {
  PLUGIN_PRINTF(("~PnaclTranslateThread (translate_thread=%p)\n", this));
  AbortSubprocesses();
  if (translate_thread_ != NULL)
    NaClThreadJoin(translate_thread_.get());
  PLUGIN_PRINTF(("~PnaclTranslateThread joined\n"));
  NaClCondVarDtor(&buffer_cond_);
  NaClMutexDtor(&cond_mu_);
  NaClMutexDtor(&subprocess_mu_);
}

} // namespace plugin
