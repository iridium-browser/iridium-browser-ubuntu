// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/render_frame_host_factory.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Appends a description of the structure of the frame tree to |result|.
void AppendTreeNodeState(FrameTreeNode* node, std::string* result) {
  result->append(
      base::Int64ToString(node->current_frame_host()->GetRoutingID()));
  if (!node->current_frame_host()->IsRenderFrameLive())
    result->append("*");  // Asterisk next to dead frames.

  if (!node->frame_name().empty()) {
    result->append(" '");
    result->append(node->frame_name());
    result->append("'");
  }
  result->append(": [");
  const char* separator = "";
  for (size_t i = 0; i < node->child_count(); i++) {
    result->append(separator);
    AppendTreeNodeState(node->child_at(i), result);
    separator = ", ";
  }
  result->append("]");
}

// Logs calls to WebContentsObserver along with the state of the frame tree,
// for later use in EXPECT_EQ().
class TreeWalkingWebContentsLogger : public WebContentsObserver {
 public:
  explicit TreeWalkingWebContentsLogger(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  ~TreeWalkingWebContentsLogger() override {
    EXPECT_EQ("", log_) << "Activity logged that was not expected";
  }

  // Gets and resets the log, which is a string of what happened.
  std::string GetLog() {
    std::string result = log_;
    log_.clear();
    return result;
  }

  // content::WebContentsObserver implementation.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    LogWhatHappened("RenderFrameCreated", render_frame_host);
  }

  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override {
    if (old_host)
      LogWhatHappened("RenderFrameHostChanged(old)", old_host);
    LogWhatHappened("RenderFrameHostChanged(new)", new_host);
  }

  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    LogWhatHappened("RenderFrameDeleted", render_frame_host);
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    LogWhatHappened("RenderProcessGone");
  }

 private:
  void LogWhatHappened(const std::string& event_name) {
    if (!log_.empty()) {
      log_.append("\n");
    }
    log_.append(event_name + " -> ");
    AppendTreeNodeState(
        static_cast<WebContentsImpl*>(web_contents())->GetFrameTree()->root(),
        &log_);
  }

  void LogWhatHappened(const std::string& event_name, RenderFrameHost* rfh) {
    LogWhatHappened(
        base::StringPrintf("%s(%d)", event_name.c_str(), rfh->GetRoutingID()));
  }

  std::string log_;

  DISALLOW_COPY_AND_ASSIGN(TreeWalkingWebContentsLogger);
};

}  // namespace

class FrameTreeTest : public RenderViewHostImplTestHarness {
 protected:
  // Prints a FrameTree, for easy assertions of the tree hierarchy.
  std::string GetTreeState(FrameTree* frame_tree) {
    std::string result;
    AppendTreeNodeState(frame_tree->root(), &result);
    return result;
  }
};

// Exercise tree manipulation routines.
//  - Add a series of nodes and verify tree structure.
//  - Remove a series of nodes and verify tree structure.
//
// TODO(nick): http://crbug.com/444722 Disabled temporarily because of a bad
// interaction with the WebContentsObserverConsistencyChecker -- calling
// AddFrame directly causes the RFH to not be announced. We either need to
// rewrite this test, or be consistent in the layer at which we announce render
// frame creation.
TEST_F(FrameTreeTest, DISABLED_Shape) {
  // Use the FrameTree of the WebContents so that it has all the delegates it
  // needs.  We may want to consider a test version of this.
  FrameTree* frame_tree = contents()->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();

  std::string no_children_node("no children node");
  std::string deep_subtree("node with deep subtree");
  int process_id = root->current_frame_host()->GetProcess()->GetID();

  ASSERT_EQ("1: []", GetTreeState(frame_tree));

  // Simulate attaching a series of frames to build the frame tree.
  frame_tree->AddFrame(root, process_id, 14, std::string(),
                       SandboxFlags::NONE);
  frame_tree->AddFrame(root, process_id, 15, std::string(),
                       SandboxFlags::NONE);
  frame_tree->AddFrame(root, process_id, 16, std::string(),
                       SandboxFlags::NONE);

  frame_tree->AddFrame(root->child_at(0), process_id, 244, std::string(),
                       SandboxFlags::NONE);
  frame_tree->AddFrame(root->child_at(1), process_id, 255, no_children_node,
                       SandboxFlags::NONE);
  frame_tree->AddFrame(root->child_at(0), process_id, 245, std::string(),
                       SandboxFlags::NONE);

  ASSERT_EQ("1: [14: [244: [], 245: []], "
                "15: [255 'no children node': []], "
                "16: []]",
            GetTreeState(frame_tree));

  FrameTreeNode* child_16 = root->child_at(2);
  frame_tree->AddFrame(child_16, process_id, 264, std::string(),
                       SandboxFlags::NONE);
  frame_tree->AddFrame(child_16, process_id, 265, std::string(),
                       SandboxFlags::NONE);
  frame_tree->AddFrame(child_16, process_id, 266, std::string(),
                       SandboxFlags::NONE);
  frame_tree->AddFrame(child_16, process_id, 267, deep_subtree,
                       SandboxFlags::NONE);
  frame_tree->AddFrame(child_16, process_id, 268, std::string(),
                       SandboxFlags::NONE);

  FrameTreeNode* child_267 = child_16->child_at(3);
  frame_tree->AddFrame(child_267, process_id, 365, std::string(),
                       SandboxFlags::NONE);
  frame_tree->AddFrame(child_267->child_at(0), process_id, 455, std::string(),
                       SandboxFlags::NONE);
  frame_tree->AddFrame(child_267->child_at(0)->child_at(0), process_id, 555,
                       std::string(), SandboxFlags::NONE);
  frame_tree->AddFrame(child_267->child_at(0)->child_at(0)->child_at(0),
                       process_id, 655, std::string(), SandboxFlags::NONE);

  // Now that's it's fully built, verify the tree structure is as expected.
  ASSERT_EQ("1: [14: [244: [], 245: []], "
                "15: [255 'no children node': []], "
                "16: [264: [], 265: [], 266: [], "
                     "267 'node with deep subtree': "
                         "[365: [455: [555: [655: []]]]], 268: []]]",
            GetTreeState(frame_tree));

  FrameTreeNode* child_555 = child_267->child_at(0)->child_at(0)->child_at(0);
  frame_tree->RemoveFrame(child_555);
  ASSERT_EQ("1: [14: [244: [], 245: []], "
                "15: [255 'no children node': []], "
                "16: [264: [], 265: [], 266: [], "
                     "267 'node with deep subtree': "
                         "[365: [455: []]], 268: []]]",
            GetTreeState(frame_tree));

  frame_tree->RemoveFrame(child_16->child_at(1));
  ASSERT_EQ("1: [14: [244: [], 245: []], "
                "15: [255 'no children node': []], "
                "16: [264: [], 266: [], "
                     "267 'node with deep subtree': "
                         "[365: [455: []]], 268: []]]",
            GetTreeState(frame_tree));

  frame_tree->RemoveFrame(root->child_at(1));
  ASSERT_EQ("1: [14: [244: [], 245: []], "
                "16: [264: [], 266: [], "
                     "267 'node with deep subtree': "
                         "[365: [455: []]], 268: []]]",
            GetTreeState(frame_tree));
}

// Ensure frames can be found by frame_tree_node_id, routing ID, or name.
TEST_F(FrameTreeTest, FindFrames) {
  // Add a few child frames to the main frame.
  FrameTree* frame_tree = contents()->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();
  main_test_rfh()->OnCreateChildFrame(22, "child0", SandboxFlags::NONE);
  main_test_rfh()->OnCreateChildFrame(23, "child1", SandboxFlags::NONE);
  main_test_rfh()->OnCreateChildFrame(24, std::string(), SandboxFlags::NONE);
  FrameTreeNode* child0 = root->child_at(0);
  FrameTreeNode* child1 = root->child_at(1);
  FrameTreeNode* child2 = root->child_at(2);

  // Add one grandchild frame.
  child1->current_frame_host()->OnCreateChildFrame(33, "grandchild",
                                                   SandboxFlags::NONE);
  FrameTreeNode* grandchild = child1->child_at(0);

  // Ensure they can be found by FTN id.
  EXPECT_EQ(root, frame_tree->FindByID(root->frame_tree_node_id()));
  EXPECT_EQ(child0, frame_tree->FindByID(child0->frame_tree_node_id()));
  EXPECT_EQ(child1, frame_tree->FindByID(child1->frame_tree_node_id()));
  EXPECT_EQ(child2, frame_tree->FindByID(child2->frame_tree_node_id()));
  EXPECT_EQ(grandchild, frame_tree->FindByID(grandchild->frame_tree_node_id()));
  EXPECT_EQ(nullptr, frame_tree->FindByID(-1));

  // Ensure they can be found by routing id.
  int process_id = main_test_rfh()->GetProcess()->GetID();
  EXPECT_EQ(root, frame_tree->FindByRoutingID(process_id,
                                              main_test_rfh()->GetRoutingID()));
  EXPECT_EQ(child0, frame_tree->FindByRoutingID(process_id, 22));
  EXPECT_EQ(child1, frame_tree->FindByRoutingID(process_id, 23));
  EXPECT_EQ(child2, frame_tree->FindByRoutingID(process_id, 24));
  EXPECT_EQ(grandchild, frame_tree->FindByRoutingID(process_id, 33));
  EXPECT_EQ(nullptr, frame_tree->FindByRoutingID(process_id, 37));

  // Ensure they can be found by name, if they have one.
  EXPECT_EQ(root, frame_tree->FindByName(std::string()));
  EXPECT_EQ(child0, frame_tree->FindByName("child0"));
  EXPECT_EQ(child1, frame_tree->FindByName("child1"));
  EXPECT_EQ(grandchild, frame_tree->FindByName("grandchild"));
  EXPECT_EQ(nullptr, frame_tree->FindByName("no such frame"));
}

// Check that PreviousSibling() is retrieved correctly.
TEST_F(FrameTreeTest, PreviousSibling) {
  // Add a few child frames to the main frame.
  FrameTree* frame_tree = contents()->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();
  main_test_rfh()->OnCreateChildFrame(22, "child0", SandboxFlags::NONE);
  main_test_rfh()->OnCreateChildFrame(23, "child1", SandboxFlags::NONE);
  main_test_rfh()->OnCreateChildFrame(24, "child2", SandboxFlags::NONE);
  FrameTreeNode* child0 = root->child_at(0);
  FrameTreeNode* child1 = root->child_at(1);
  FrameTreeNode* child2 = root->child_at(2);

  // Add one grandchild frame.
  child1->current_frame_host()->OnCreateChildFrame(33, "grandchild",
                                                   SandboxFlags::NONE);
  FrameTreeNode* grandchild = child1->child_at(0);

  EXPECT_EQ(nullptr, root->PreviousSibling());
  EXPECT_EQ(nullptr, child0->PreviousSibling());
  EXPECT_EQ(child0, child1->PreviousSibling());
  EXPECT_EQ(child1, child2->PreviousSibling());
  EXPECT_EQ(nullptr, grandchild->PreviousSibling());
}

// Do some simple manipulations of the frame tree, making sure that
// WebContentsObservers see a consistent view of the tree as we go.
TEST_F(FrameTreeTest, ObserverWalksTreeDuringFrameCreation) {
  TreeWalkingWebContentsLogger activity(contents());
  contents()->NavigateAndCommit(GURL("http://www.google.com"));
  EXPECT_EQ("", activity.GetLog());

  FrameTree* frame_tree = contents()->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();

  // Simulate attaching a series of frames to build the frame tree.
  main_test_rfh()->OnCreateChildFrame(14, std::string(), SandboxFlags::NONE);
  EXPECT_EQ(
      "RenderFrameHostChanged(new)(14) -> 1: []\n"
      "RenderFrameCreated(14) -> 1: [14: []]",
      activity.GetLog());
  main_test_rfh()->OnCreateChildFrame(18, std::string(), SandboxFlags::NONE);
  EXPECT_EQ(
      "RenderFrameHostChanged(new)(18) -> 1: [14: []]\n"
      "RenderFrameCreated(18) -> 1: [14: [], 18: []]",
      activity.GetLog());
  frame_tree->RemoveFrame(root->child_at(0));
  EXPECT_EQ("RenderFrameDeleted(14) -> 1: [18: []]", activity.GetLog());
  frame_tree->RemoveFrame(root->child_at(0));
  EXPECT_EQ("RenderFrameDeleted(18) -> 1: []", activity.GetLog());
}

// Make sure that WebContentsObservers see a consistent view of the tree after
// recovery from a render process crash.
TEST_F(FrameTreeTest, ObserverWalksTreeAfterCrash) {
  TreeWalkingWebContentsLogger activity(contents());
  contents()->NavigateAndCommit(GURL("http://www.google.com"));
  EXPECT_EQ("", activity.GetLog());

  main_test_rfh()->OnCreateChildFrame(22, std::string(), SandboxFlags::NONE);
  EXPECT_EQ(
      "RenderFrameHostChanged(new)(22) -> 1: []\n"
      "RenderFrameCreated(22) -> 1: [22: []]",
      activity.GetLog());
  main_test_rfh()->OnCreateChildFrame(23, std::string(), SandboxFlags::NONE);
  EXPECT_EQ(
      "RenderFrameHostChanged(new)(23) -> 1: [22: []]\n"
      "RenderFrameCreated(23) -> 1: [22: [], 23: []]",
      activity.GetLog());

  // Crash the renderer
  main_test_rfh()->GetProcess()->SimulateCrash();
  EXPECT_EQ(
      "RenderFrameDeleted(23) -> 1: [22: [], 23*: []]\n"
      "RenderFrameDeleted(22) -> 1: [22*: [], 23*: []]\n"
      "RenderFrameDeleted(1) -> 1: []\n"  // TODO(nick): Should be "1*:"
      "RenderProcessGone -> 1*: []",
      activity.GetLog());
}

// Ensure that frames are not added to the tree, if the process passed in
// is different than the process of the parent node.
TEST_F(FrameTreeTest, FailAddFrameWithWrongProcessId) {
  contents()->NavigateAndCommit(GURL("http://www.google.com"));
  FrameTree* frame_tree = contents()->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();
  int process_id = root->current_frame_host()->GetProcess()->GetID();

  ASSERT_EQ("1: []", GetTreeState(frame_tree));

  // Simulate attaching a frame from mismatched process id.
  ASSERT_FALSE(frame_tree->AddFrame(root, process_id + 1, 1, std::string(),
                                    SandboxFlags::NONE));
  ASSERT_EQ("1: []", GetTreeState(frame_tree));
}

// Ensure that frames removed while a process has crashed are not preserved in
// the global map of id->frame.
TEST_F(FrameTreeTest, ProcessCrashClearsGlobalMap) {
  // Add a couple child frames to the main frame.
  FrameTreeNode* root = contents()->GetFrameTree()->root();

  main_test_rfh()->OnCreateChildFrame(22, std::string(), SandboxFlags::NONE);
  main_test_rfh()->OnCreateChildFrame(23, std::string(), SandboxFlags::NONE);

  // Add one grandchild frame.
  RenderFrameHostImpl* child1_rfh = root->child_at(0)->current_frame_host();
  child1_rfh->OnCreateChildFrame(33, std::string(), SandboxFlags::NONE);

  // Ensure they can be found by id.
  int id1 = root->child_at(0)->frame_tree_node_id();
  int id2 = root->child_at(1)->frame_tree_node_id();
  int id3 = root->child_at(0)->child_at(0)->frame_tree_node_id();
  EXPECT_TRUE(FrameTreeNode::GloballyFindByID(id1));
  EXPECT_TRUE(FrameTreeNode::GloballyFindByID(id2));
  EXPECT_TRUE(FrameTreeNode::GloballyFindByID(id3));

  // Crash the renderer.
  main_test_rfh()->GetProcess()->SimulateCrash();

  // Ensure they cannot be found by id after the process has crashed.
  EXPECT_FALSE(FrameTreeNode::GloballyFindByID(id1));
  EXPECT_FALSE(FrameTreeNode::GloballyFindByID(id2));
  EXPECT_FALSE(FrameTreeNode::GloballyFindByID(id3));
}

}  // namespace content
