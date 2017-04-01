// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mhtml_generation_manager.h"

#include <map>
#include <queue>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/bad_message.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mhtml_generation_params.h"
#include "net/base/mime_util.h"

namespace content {

// The class and all of its members live on the UI thread.  Only static methods
// are executed on other threads.
class MHTMLGenerationManager::Job : public RenderProcessHostObserver {
 public:
  Job(int job_id,
      WebContents* web_contents,
      const MHTMLGenerationParams& params,
      const GenerateMHTMLCallback& callback);
  ~Job() override;

  int id() const { return job_id_; }
  void set_browser_file(base::File file) { browser_file_ = std::move(file); }
  base::TimeTicks creation_time() const { return creation_time_; }

  const GenerateMHTMLCallback& callback() const { return callback_; }

  // Indicates whether we expect a message from the |sender| at this time.
  // We expect only one message per frame - therefore calling this method
  // will always clear |frame_tree_node_id_of_busy_frame_|.
  bool IsMessageFromFrameExpected(RenderFrameHostImpl* sender);

  // Handler for FrameHostMsg_SerializeAsMHTMLResponse (a notification from the
  // renderer that the MHTML generation for previous frame has finished).
  // Returns MhtmlSaveStatus::SUCCESS or a specific error status.
  MhtmlSaveStatus OnSerializeAsMHTMLResponse(
      const std::set<std::string>& digests_of_uris_of_serialized_resources);

  // Sends IPC to the renderer, asking for MHTML generation of the next frame.
  // Returns MhtmlSaveStatus::SUCCESS or a specific error status.
  MhtmlSaveStatus SendToNextRenderFrame();

  // Indicates if more calls to SendToNextRenderFrame are needed.
  bool IsDone() const {
    bool waiting_for_response_from_renderer =
        frame_tree_node_id_of_busy_frame_ !=
        FrameTreeNode::kFrameTreeNodeInvalidId;
    bool no_more_requests_to_send = pending_frame_tree_node_ids_.empty();
    return !waiting_for_response_from_renderer && no_more_requests_to_send;
  }

  // Close the file on the file thread and respond back on the UI thread with
  // file size.
  void CloseFile(base::Callback<void(int64_t file_size)> callback);

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  void MarkAsFinished();

  void ReportRendererMainThreadTime(base::TimeDelta renderer_main_thread_time);

 private:
  static int64_t CloseFileOnFileThread(base::File file);
  void AddFrame(RenderFrameHost* render_frame_host);

  // Creates a new map with values (content ids) the same as in
  // |frame_tree_node_to_content_id_| map, but with the keys translated from
  // frame_tree_node_id into a |site_instance|-specific routing_id.
  std::map<int, std::string> CreateFrameRoutingIdToContentId(
      SiteInstance* site_instance);

  // Id used to map renderer responses to jobs.
  // See also MHTMLGenerationManager::id_to_job_ map.
  const int job_id_;

  // Time tracking for performance metrics reporting.
  const base::TimeTicks creation_time_;
  base::TimeTicks wait_on_renderer_start_time_;
  base::TimeDelta all_renderers_wait_time_;
  base::TimeDelta all_renderers_main_thread_time_;
  base::TimeDelta longest_renderer_main_thread_time_;

  // User-configurable parameters. Includes the file location, binary encoding
  // choices, and whether to skip storing resources marked
  // Cache-Control: no-store.
  MHTMLGenerationParams params_;

  // The IDs of frames that still need to be processed.
  std::queue<int> pending_frame_tree_node_ids_;

  // Identifies a frame to which we've sent FrameMsg_SerializeAsMHTML but for
  // which we didn't yet process FrameHostMsg_SerializeAsMHTMLResponse via
  // OnSerializeAsMHTMLResponse.
  int frame_tree_node_id_of_busy_frame_;

  // The handle to the file the MHTML is saved to for the browser process.
  base::File browser_file_;

  // Map from frames to content ids (see WebFrameSerializer::generateMHTMLParts
  // for more details about what "content ids" are and how they are used).
  std::map<int, std::string> frame_tree_node_to_content_id_;

  // MIME multipart boundary to use in the MHTML doc.
  std::string mhtml_boundary_marker_;

  // Digests of URIs of already generated MHTML parts.
  std::set<std::string> digests_of_already_serialized_uris_;
  std::string salt_;

  // The callback to call once generation is complete.
  const GenerateMHTMLCallback callback_;

  // Whether the job is finished (set to true only for the short duration of
  // time between MHTMLGenerationManager::JobFinished is called and the job is
  // destroyed by MHTMLGenerationManager::OnFileClosed).
  bool is_finished_;

  // RAII helper for registering this Job as a RenderProcessHost observer.
  ScopedObserver<RenderProcessHost, MHTMLGenerationManager::Job>
      observed_renderer_process_host_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

MHTMLGenerationManager::Job::Job(int job_id,
                                 WebContents* web_contents,
                                 const MHTMLGenerationParams& params,
                                 const GenerateMHTMLCallback& callback)
    : job_id_(job_id),
      creation_time_(base::TimeTicks::Now()),
      params_(params),
      frame_tree_node_id_of_busy_frame_(FrameTreeNode::kFrameTreeNodeInvalidId),
      mhtml_boundary_marker_(net::GenerateMimeMultipartBoundary()),
      salt_(base::GenerateGUID()),
      callback_(callback),
      is_finished_(false),
      observed_renderer_process_host_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents->ForEachFrame(base::Bind(
      &MHTMLGenerationManager::Job::AddFrame,
      base::Unretained(this)));  // Safe because ForEachFrame is synchronous.

  // Main frame needs to be processed first.
  DCHECK(!pending_frame_tree_node_ids_.empty());
  DCHECK(FrameTreeNode::GloballyFindByID(pending_frame_tree_node_ids_.front())
             ->parent() == nullptr);
}

MHTMLGenerationManager::Job::~Job() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

std::map<int, std::string>
MHTMLGenerationManager::Job::CreateFrameRoutingIdToContentId(
    SiteInstance* site_instance) {
  std::map<int, std::string> result;
  for (const auto& it : frame_tree_node_to_content_id_) {
    int ftn_id = it.first;
    const std::string& content_id = it.second;

    FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(ftn_id);
    if (!ftn)
      continue;

    int routing_id =
        ftn->render_manager()->GetRoutingIdForSiteInstance(site_instance);
    if (routing_id == MSG_ROUTING_NONE)
      continue;

    result[routing_id] = content_id;
  }
  return result;
}

MhtmlSaveStatus MHTMLGenerationManager::Job::SendToNextRenderFrame() {
  DCHECK(browser_file_.IsValid());
  DCHECK(!pending_frame_tree_node_ids_.empty());

  FrameMsg_SerializeAsMHTML_Params ipc_params;
  ipc_params.job_id = job_id_;
  ipc_params.mhtml_boundary_marker = mhtml_boundary_marker_;
  ipc_params.mhtml_binary_encoding = params_.use_binary_encoding;
  ipc_params.mhtml_cache_control_policy = params_.cache_control_policy;
  ipc_params.mhtml_popup_overlay_removal = params_.remove_popup_overlay;

  int frame_tree_node_id = pending_frame_tree_node_ids_.front();
  pending_frame_tree_node_ids_.pop();
  ipc_params.is_last_frame = pending_frame_tree_node_ids_.empty();

  FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!ftn)  // The contents went away.
    return MhtmlSaveStatus::FRAME_NO_LONGER_EXISTS;
  RenderFrameHost* rfh = ftn->current_frame_host();

  // Get notified if the target of the IPC message dies between responding.
  observed_renderer_process_host_.RemoveAll();
  observed_renderer_process_host_.Add(rfh->GetProcess());

  // Tell the renderer to skip (= deduplicate) already covered MHTML parts.
  ipc_params.salt = salt_;
  ipc_params.digests_of_uris_to_skip = digests_of_already_serialized_uris_;

  ipc_params.destination_file = IPC::GetPlatformFileForTransit(
      browser_file_.GetPlatformFile(), false);  // |close_source_handle|.
  ipc_params.frame_routing_id_to_content_id =
      CreateFrameRoutingIdToContentId(rfh->GetSiteInstance());

  // Send the IPC asking the renderer to serialize the frame.
  DCHECK_EQ(FrameTreeNode::kFrameTreeNodeInvalidId,
            frame_tree_node_id_of_busy_frame_);
  frame_tree_node_id_of_busy_frame_ = frame_tree_node_id;
  rfh->Send(new FrameMsg_SerializeAsMHTML(rfh->GetRoutingID(), ipc_params));
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("page-serialization", "WaitingOnRenderer",
                                    this, "frame tree node id",
                                    frame_tree_node_id);
  DCHECK(wait_on_renderer_start_time_.is_null());
  wait_on_renderer_start_time_ = base::TimeTicks::Now();
  return MhtmlSaveStatus::SUCCESS;
}

void MHTMLGenerationManager::Job::RenderProcessExited(
    RenderProcessHost* host,
    base::TerminationStatus status,
    int exit_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MHTMLGenerationManager::GetInstance()->RenderProcessExited(this);
}

void MHTMLGenerationManager::Job::MarkAsFinished() {
  DCHECK(!is_finished_);
  if (is_finished_)
    return;

  is_finished_ = true;
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("page-serialization", "JobFinished",
                                      this);

  // End of job timing reports.
  if (!wait_on_renderer_start_time_.is_null()) {
    base::TimeDelta renderer_wait_time =
        base::TimeTicks::Now() - wait_on_renderer_start_time_;
    UMA_HISTOGRAM_TIMES(
        "PageSerialization.MhtmlGeneration.BrowserWaitForRendererTime."
        "SingleFrame",
        renderer_wait_time);
    all_renderers_wait_time_ += renderer_wait_time;
  }
  if (!all_renderers_wait_time_.is_zero()) {
    UMA_HISTOGRAM_TIMES(
        "PageSerialization.MhtmlGeneration.BrowserWaitForRendererTime."
        "FrameTree",
        all_renderers_wait_time_);
  }
  if (!all_renderers_main_thread_time_.is_zero()) {
    UMA_HISTOGRAM_TIMES(
        "PageSerialization.MhtmlGeneration.RendererMainThreadTime.FrameTree",
        all_renderers_main_thread_time_);
  }
  if (!longest_renderer_main_thread_time_.is_zero()) {
    UMA_HISTOGRAM_TIMES(
        "PageSerialization.MhtmlGeneration.RendererMainThreadTime.SlowestFrame",
        longest_renderer_main_thread_time_);
  }

  // Stopping RenderProcessExited notifications is needed to avoid calling
  // JobFinished twice.  See also https://crbug.com/612098.
  observed_renderer_process_host_.RemoveAll();
}

void MHTMLGenerationManager::Job::ReportRendererMainThreadTime(
    base::TimeDelta renderer_main_thread_time) {
  DCHECK(renderer_main_thread_time > base::TimeDelta());
  if (renderer_main_thread_time > base::TimeDelta())
    all_renderers_main_thread_time_ += renderer_main_thread_time;
  if (renderer_main_thread_time > longest_renderer_main_thread_time_)
    longest_renderer_main_thread_time_ = renderer_main_thread_time;
}

void MHTMLGenerationManager::Job::AddFrame(RenderFrameHost* render_frame_host) {
  auto* rfhi = static_cast<RenderFrameHostImpl*>(render_frame_host);
  int frame_tree_node_id = rfhi->frame_tree_node()->frame_tree_node_id();
  pending_frame_tree_node_ids_.push(frame_tree_node_id);

  std::string guid = base::GenerateGUID();
  std::string content_id = base::StringPrintf("<frame-%d-%s@mhtml.blink>",
                                              frame_tree_node_id, guid.c_str());
  frame_tree_node_to_content_id_[frame_tree_node_id] = content_id;
}

void MHTMLGenerationManager::Job::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observed_renderer_process_host_.Remove(host);
}

void MHTMLGenerationManager::Job::CloseFile(
    base::Callback<void(int64_t)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!browser_file_.IsValid()) {
    callback.Run(-1);
    return;
  }

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::Job::CloseFileOnFileThread,
                 base::Passed(std::move(browser_file_))),
      callback);
}

bool MHTMLGenerationManager::Job::IsMessageFromFrameExpected(
    RenderFrameHostImpl* sender) {
  int sender_id = sender->frame_tree_node()->frame_tree_node_id();
  if (sender_id != frame_tree_node_id_of_busy_frame_)
    return false;

  // We only expect one message per frame - let's make sure subsequent messages
  // from the same |sender| will be rejected.
  frame_tree_node_id_of_busy_frame_ = FrameTreeNode::kFrameTreeNodeInvalidId;

  return true;
}

MhtmlSaveStatus MHTMLGenerationManager::Job::OnSerializeAsMHTMLResponse(
    const std::set<std::string>& digests_of_uris_of_serialized_resources) {
  DCHECK(!wait_on_renderer_start_time_.is_null());
  base::TimeDelta renderer_wait_time =
      base::TimeTicks::Now() - wait_on_renderer_start_time_;
  UMA_HISTOGRAM_TIMES(
      "PageSerialization.MhtmlGeneration.BrowserWaitForRendererTime."
      "SingleFrame",
      renderer_wait_time);
  all_renderers_wait_time_ += renderer_wait_time;
  wait_on_renderer_start_time_ = base::TimeTicks();

  // Renderer should be deduping resources with the same uris.
  DCHECK_EQ(0u, base::STLSetIntersection<std::set<std::string>>(
                    digests_of_already_serialized_uris_,
                    digests_of_uris_of_serialized_resources).size());
  digests_of_already_serialized_uris_.insert(
      digests_of_uris_of_serialized_resources.begin(),
      digests_of_uris_of_serialized_resources.end());

  // Report success if all frames have been processed.
  if (pending_frame_tree_node_ids_.empty())
    return MhtmlSaveStatus::SUCCESS;

  return SendToNextRenderFrame();
}

// static
int64_t MHTMLGenerationManager::Job::CloseFileOnFileThread(base::File file) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(file.IsValid());
  int64_t file_size = file.GetLength();
  file.Close();
  return file_size;
}

MHTMLGenerationManager* MHTMLGenerationManager::GetInstance() {
  return base::Singleton<MHTMLGenerationManager>::get();
}

MHTMLGenerationManager::MHTMLGenerationManager() : next_job_id_(0) {}

MHTMLGenerationManager::~MHTMLGenerationManager() {
}

void MHTMLGenerationManager::SaveMHTML(WebContents* web_contents,
                                       const MHTMLGenerationParams& params,
                                       const GenerateMHTMLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Job* job = NewJob(web_contents, params, callback);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
      "page-serialization", "SavingMhtmlJob", job, "url",
      web_contents->GetLastCommittedURL().possibly_invalid_spec(),
      "file", params.file_path.AsUTF8Unsafe());

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MHTMLGenerationManager::CreateFile, params.file_path),
      base::Bind(&MHTMLGenerationManager::OnFileAvailable,
                 base::Unretained(this),  // Safe b/c |this| is a singleton.
                 job->id()));
}

void MHTMLGenerationManager::OnSerializeAsMHTMLResponse(
    RenderFrameHostImpl* sender,
    int job_id,
    MhtmlSaveStatus save_status,
    const std::set<std::string>& digests_of_uris_of_serialized_resources,
    base::TimeDelta renderer_main_thread_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Job* job = FindJob(job_id);
  if (!job || !job->IsMessageFromFrameExpected(sender)) {
    NOTREACHED();
    ReceivedBadMessage(sender->GetProcess(),
                       bad_message::DWNLD_INVALID_SERIALIZE_AS_MHTML_RESPONSE);
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("page-serialization", "WaitingOnRenderer",
                                  job);
  job->ReportRendererMainThreadTime(renderer_main_thread_time);

  if (save_status == MhtmlSaveStatus::SUCCESS) {
    save_status = job->OnSerializeAsMHTMLResponse(
        digests_of_uris_of_serialized_resources);
  }

  if (save_status != MhtmlSaveStatus::SUCCESS) {
    JobFinished(job, save_status);
    return;
  }

  if (job->IsDone())
    JobFinished(job, MhtmlSaveStatus::SUCCESS);
}

// static
base::File MHTMLGenerationManager::CreateFile(const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  // SECURITY NOTE: A file descriptor to the file created below will be passed
  // to multiple renderer processes which (in out-of-process iframes mode) can
  // act on behalf of separate web principals.  Therefore it is important to
  // only allow writing to the file and forbid reading from the file (as this
  // would allow reading content generated by other renderers / other web
  // principals).
  uint32_t file_flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;

  base::File browser_file(file_path, file_flags);
  if (!browser_file.IsValid()) {
    LOG(ERROR) << "Failed to create file to save MHTML at: " <<
        file_path.value();
  }
  return browser_file;
}

void MHTMLGenerationManager::OnFileAvailable(int job_id,
                                             base::File browser_file) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Job* job = FindJob(job_id);
  DCHECK(job);

  if (!browser_file.IsValid()) {
    LOG(ERROR) << "Failed to create file";
    JobFinished(job, MhtmlSaveStatus::FILE_CREATION_ERROR);
    return;
  }

  job->set_browser_file(std::move(browser_file));

  MhtmlSaveStatus save_status = job->SendToNextRenderFrame();
  if (save_status != MhtmlSaveStatus::SUCCESS) {
    JobFinished(job, save_status);
  }
}

void MHTMLGenerationManager::JobFinished(Job* job,
                                         MhtmlSaveStatus save_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(job);

  job->MarkAsFinished();
  job->CloseFile(
      base::Bind(&MHTMLGenerationManager::OnFileClosed,
                 base::Unretained(this),  // Safe b/c |this| is a singleton.
                 job->id(), save_status));
}

void MHTMLGenerationManager::OnFileClosed(int job_id,
                                          MhtmlSaveStatus save_status,
                                          int64_t file_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Checks if an error happened while closing the file.
  if (save_status == MhtmlSaveStatus::SUCCESS && file_size < 0)
    save_status = MhtmlSaveStatus::FILE_CLOSING_ERROR;

  Job* job = FindJob(job_id);
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "page-serialization", "SavingMhtmlJob", job, "job save status",
      GetMhtmlSaveStatusLabel(save_status), "file size", file_size);
  UMA_HISTOGRAM_TIMES("PageSerialization.MhtmlGeneration.FullPageSavingTime",
                      base::TimeTicks::Now() - job->creation_time());
  UMA_HISTOGRAM_ENUMERATION("PageSerialization.MhtmlGeneration.FinalSaveStatus",
                            static_cast<int>(save_status),
                            static_cast<int>(MhtmlSaveStatus::LAST));
  job->callback().Run(save_status == MhtmlSaveStatus::SUCCESS ? file_size : -1);
  id_to_job_.erase(job_id);
}

MHTMLGenerationManager::Job* MHTMLGenerationManager::NewJob(
    WebContents* web_contents,
    const MHTMLGenerationParams& params,
    const GenerateMHTMLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Job* job = new Job(++next_job_id_, web_contents, params, callback);
  id_to_job_[job->id()] = base::WrapUnique(job);
  return job;
}

MHTMLGenerationManager::Job* MHTMLGenerationManager::FindJob(int job_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter = id_to_job_.find(job_id);
  if (iter == id_to_job_.end()) {
    NOTREACHED();
    return nullptr;
  }
  return iter->second.get();
}

void MHTMLGenerationManager::RenderProcessExited(Job* job) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(job);
  JobFinished(job, MhtmlSaveStatus::RENDER_PROCESS_EXITED);
}

}  // namespace content
