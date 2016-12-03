// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_message_filter.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/prerender_messages.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace prerender {

namespace {

class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static ShutdownNotifierFactory* GetInstance() {
    return base::Singleton<ShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "PrerenderMessageFilter") {
    DependsOn(PrerenderLinkManagerFactory::GetInstance());
  }
  ~ShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(ShutdownNotifierFactory);
};

}  // namespace

PrerenderMessageFilter::PrerenderMessageFilter(int render_process_id,
                                               Profile* profile)
    : BrowserMessageFilter(PrerenderMsgStart),
      render_process_id_(render_process_id),
      prerender_link_manager_(
          PrerenderLinkManagerFactory::GetForProfile(profile)) {
  shutdown_notifier_ =
      ShutdownNotifierFactory::GetInstance()->Get(profile)->Subscribe(
          base::Bind(&PrerenderMessageFilter::ShutdownOnUIThread,
                     base::Unretained(this)));
}

PrerenderMessageFilter::~PrerenderMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

// static
void PrerenderMessageFilter::EnsureShutdownNotifierFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

bool PrerenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrerenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(PrerenderHostMsg_AddLinkRelPrerender, OnAddPrerender)
    IPC_MESSAGE_HANDLER(
        PrerenderHostMsg_CancelLinkRelPrerender, OnCancelPrerender)
    IPC_MESSAGE_HANDLER(
        PrerenderHostMsg_AbandonLinkRelPrerender, OnAbandonPrerender)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PrerenderMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, content::BrowserThread::ID* thread) {
  if (message.type() == PrerenderHostMsg_AddLinkRelPrerender::ID ||
      message.type() == PrerenderHostMsg_CancelLinkRelPrerender::ID ||
      message.type() == PrerenderHostMsg_AbandonLinkRelPrerender::ID) {
    *thread = BrowserThread::UI;
  }
}

void PrerenderMessageFilter::OnChannelClosing() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrerenderMessageFilter::OnChannelClosingInUIThread, this));
}

void PrerenderMessageFilter::OnDestruct() const {
  // |shutdown_notifier_| needs to be destroyed on the UI thread.
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

void PrerenderMessageFilter::OnAddPrerender(
    int prerender_id,
    const PrerenderAttributes& attributes,
    const content::Referrer& referrer,
    const gfx::Size& size,
    int render_view_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!prerender_link_manager_)
    return;
  prerender_link_manager_->OnAddPrerender(
      render_process_id_, prerender_id,
      attributes.url, attributes.rel_types, referrer,
      size, render_view_route_id);
}

void PrerenderMessageFilter::OnCancelPrerender(
    int prerender_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!prerender_link_manager_)
    return;
  prerender_link_manager_->OnCancelPrerender(render_process_id_, prerender_id);
}

void PrerenderMessageFilter::OnAbandonPrerender(
    int prerender_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!prerender_link_manager_)
    return;
  prerender_link_manager_->OnAbandonPrerender(render_process_id_, prerender_id);
}

void PrerenderMessageFilter::ShutdownOnUIThread() {
  prerender_link_manager_ = nullptr;
  shutdown_notifier_.reset();
}

void PrerenderMessageFilter::OnChannelClosingInUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!prerender_link_manager_)
    return;
  prerender_link_manager_->OnChannelClosing(render_process_id_);
}

}  // namespace prerender

