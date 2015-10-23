// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_renderer_host.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/browser_side_navigation_test_utils.h"
#include "content/test/content_browser_sanity_checker.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_frame_host_factory.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_view_host_factory.h"
#include "content/test/test_web_contents.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/test/aura_test_helper.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/wm/core/default_activation_client.h"
#endif

namespace content {

// RenderFrameHostTester ------------------------------------------------------

// static
RenderFrameHostTester* RenderFrameHostTester::For(RenderFrameHost* host) {
  return static_cast<TestRenderFrameHost*>(host);
}

// static
RenderFrameHost* RenderFrameHostTester::GetPendingForController(
    NavigationController* controller) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      controller->GetWebContents());
  return web_contents->GetRenderManagerForTesting()->pending_frame_host();
}

// static
bool RenderFrameHostTester::IsRenderFrameHostSwappedOut(RenderFrameHost* rfh) {
  return static_cast<RenderFrameHostImpl*>(rfh)->is_swapped_out();
}

// RenderViewHostTester -------------------------------------------------------

// static
RenderViewHostTester* RenderViewHostTester::For(RenderViewHost* host) {
  return static_cast<TestRenderViewHost*>(host);
}

// static
bool RenderViewHostTester::TestOnMessageReceived(RenderViewHost* rvh,
                                                 const IPC::Message& msg) {
  return static_cast<RenderViewHostImpl*>(rvh)->OnMessageReceived(msg);
}

// static
bool RenderViewHostTester::HasTouchEventHandler(RenderViewHost* rvh) {
  RenderWidgetHostImpl* host_impl = RenderWidgetHostImpl::From(rvh);
  return host_impl->has_touch_handler();
}


// RenderViewHostTestEnabler --------------------------------------------------

RenderViewHostTestEnabler::RenderViewHostTestEnabler()
    : rph_factory_(new MockRenderProcessHostFactory()),
      rvh_factory_(new TestRenderViewHostFactory(rph_factory_.get())),
      rfh_factory_(new TestRenderFrameHostFactory()) {}

RenderViewHostTestEnabler::~RenderViewHostTestEnabler() {
}


// RenderViewHostTestHarness --------------------------------------------------

RenderViewHostTestHarness::RenderViewHostTestHarness()
    : thread_bundle_options_(TestBrowserThreadBundle::DEFAULT) {}

RenderViewHostTestHarness::~RenderViewHostTestHarness() {
}

NavigationController& RenderViewHostTestHarness::controller() {
  return web_contents()->GetController();
}

WebContents* RenderViewHostTestHarness::web_contents() {
  return contents_.get();
}

RenderViewHost* RenderViewHostTestHarness::rvh() {
  RenderViewHost* result = web_contents()->GetRenderViewHost();
  CHECK_EQ(result, web_contents()->GetMainFrame()->GetRenderViewHost());
  return result;
}

RenderViewHost* RenderViewHostTestHarness::pending_rvh() {
  return pending_main_rfh() ? pending_main_rfh()->GetRenderViewHost() : NULL;
}

RenderViewHost* RenderViewHostTestHarness::active_rvh() {
  return pending_rvh() ? pending_rvh() : rvh();
}

RenderFrameHost* RenderViewHostTestHarness::main_rfh() {
  return web_contents()->GetMainFrame();
}

RenderFrameHost* RenderViewHostTestHarness::pending_main_rfh() {
  return WebContentsTester::For(web_contents())->GetPendingMainFrame();
}

BrowserContext* RenderViewHostTestHarness::browser_context() {
  return browser_context_.get();
}

MockRenderProcessHost* RenderViewHostTestHarness::process() {
  return static_cast<MockRenderProcessHost*>(active_rvh()->GetProcess());
}

void RenderViewHostTestHarness::DeleteContents() {
  SetContents(NULL);
}

void RenderViewHostTestHarness::SetContents(WebContents* contents) {
  contents_.reset(contents);
}

WebContents* RenderViewHostTestHarness::CreateTestWebContents() {
  // Make sure we ran SetUp() already.
#if defined(OS_WIN)
  DCHECK(ole_initializer_ != NULL);
#endif
#if defined(USE_AURA)
  DCHECK(aura_test_helper_ != NULL);
#endif

  // This will be deleted when the WebContentsImpl goes away.
  SiteInstance* instance = SiteInstance::Create(browser_context_.get());
  instance->GetProcess()->Init();

  return TestWebContents::Create(browser_context_.get(), instance);
}

void RenderViewHostTestHarness::NavigateAndCommit(const GURL& url) {
  static_cast<TestWebContents*>(web_contents())->NavigateAndCommit(url);
}

void RenderViewHostTestHarness::Reload() {
  NavigationEntry* entry = controller().GetLastCommittedEntry();
  DCHECK(entry);
  controller().Reload(false);
  RenderFrameHostTester::For(main_rfh())
      ->SendNavigateWithTransition(entry->GetPageID(), entry->GetUniqueID(),
                                   false, entry->GetURL(),
                                   ui::PAGE_TRANSITION_RELOAD);
}

void RenderViewHostTestHarness::FailedReload() {
  NavigationEntry* entry = controller().GetLastCommittedEntry();
  DCHECK(entry);
  controller().Reload(false);
  RenderFrameHostTester::For(main_rfh())
      ->SendFailedNavigate(entry->GetPageID(), entry->GetUniqueID(), false,
                           entry->GetURL());
}

void RenderViewHostTestHarness::SetUp() {
  thread_bundle_.reset(new TestBrowserThreadBundle(thread_bundle_options_));

#if defined(OS_WIN)
  ole_initializer_.reset(new ui::ScopedOleInitializer());
#endif
#if defined(USE_AURA)
  // The ContextFactory must exist before any Compositors are created.
  bool enable_pixel_output = false;
  ui::ContextFactory* context_factory =
      ui::InitializeContextFactoryForTests(enable_pixel_output);

  aura_test_helper_.reset(
      new aura::test::AuraTestHelper(base::MessageLoopForUI::current()));
  aura_test_helper_->SetUp(context_factory);
  new wm::DefaultActivationClient(aura_test_helper_->root_window());
#endif

  sanity_checker_.reset(new ContentBrowserSanityChecker());

  DCHECK(!browser_context_);
  browser_context_.reset(CreateBrowserContext());

  SetContents(CreateTestWebContents());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    BrowserSideNavigationSetUp();
  }
}

void RenderViewHostTestHarness::TearDown() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    BrowserSideNavigationTearDown();
  }

  SetContents(NULL);
#if defined(USE_AURA)
  aura_test_helper_->TearDown();
  ui::TerminateContextFactoryForTests();
#endif
  // Make sure that we flush any messages related to WebContentsImpl destruction
  // before we destroy the browser context.
  base::RunLoop().RunUntilIdle();

#if defined(OS_WIN)
  ole_initializer_.reset();
#endif

  // Delete any RenderProcessHosts before the BrowserContext goes away.
  if (rvh_test_enabler_.rph_factory_)
    rvh_test_enabler_.rph_factory_.reset();

  // Release the browser context by posting itself on the end of the task
  // queue. This is preferable to immediate deletion because it will behave
  // properly if the |rph_factory_| reset above enqueued any tasks which
  // depend on |browser_context_|.
  BrowserThread::DeleteSoon(content::BrowserThread::UI,
                            FROM_HERE,
                            browser_context_.release());
  thread_bundle_.reset();
}

BrowserContext* RenderViewHostTestHarness::CreateBrowserContext() {
  return new TestBrowserContext();
}

void RenderViewHostTestHarness::SetRenderProcessHostFactory(
    RenderProcessHostFactory* factory) {
  rvh_test_enabler_.rvh_factory_->set_render_process_host_factory(factory);
}

}  // namespace content
