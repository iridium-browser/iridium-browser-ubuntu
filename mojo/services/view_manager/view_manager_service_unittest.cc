// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/view_manager/client_connection.h"
#include "mojo/services/view_manager/connection_manager.h"
#include "mojo/services/view_manager/connection_manager_delegate.h"
#include "mojo/services/view_manager/display_manager.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/server_view.h"
#include "mojo/services/view_manager/test_change_tracker.h"
#include "mojo/services/view_manager/view_manager_service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/types.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/util.h"
#include "third_party/mojo_services/src/view_manager/public/interfaces/view_manager.mojom.h"
#include "third_party/mojo_services/src/window_manager/public/interfaces/window_manager.mojom.h"
#include "third_party/mojo_services/src/window_manager/public/interfaces/window_manager_internal.mojom.h"
#include "ui/gfx/geometry/rect.h"

using mojo::Array;
using mojo::ERROR_CODE_NONE;
using mojo::InterfaceRequest;
using mojo::ServiceProvider;
using mojo::ServiceProviderPtr;
using mojo::String;
using mojo::ViewDataPtr;

namespace view_manager {
namespace {

// -----------------------------------------------------------------------------

// ViewManagerClient implementation that logs all calls to a TestChangeTracker.
// TODO(sky): refactor so both this and ViewManagerServiceAppTest share code.
class TestViewManagerClient : public mojo::ViewManagerClient {
 public:
  TestViewManagerClient() {}
  ~TestViewManagerClient() override {}

  TestChangeTracker* tracker() { return &tracker_; }

 private:
  // ViewManagerClient:
  void OnEmbed(uint16_t connection_id,
               const String& embedder_url,
               ViewDataPtr root,
               mojo::ViewManagerServicePtr view_manager_service,
               InterfaceRequest<ServiceProvider> services,
               ServiceProviderPtr exposed_services,
               mojo::ScopedMessagePipeHandle window_manager_pipe) override {
    tracker_.OnEmbed(connection_id, embedder_url, root.Pass());
  }
  void OnEmbeddedAppDisconnected(uint32_t view) override {
    tracker_.OnEmbeddedAppDisconnected(view);
  }
  void OnViewBoundsChanged(uint32_t view,
                           mojo::RectPtr old_bounds,
                           mojo::RectPtr new_bounds) override {
    tracker_.OnViewBoundsChanged(view, old_bounds.Pass(), new_bounds.Pass());
  }
  void OnViewViewportMetricsChanged(
      mojo::ViewportMetricsPtr old_metrics,
      mojo::ViewportMetricsPtr new_metrics) override {
    tracker_.OnViewViewportMetricsChanged(old_metrics.Pass(),
                                          new_metrics.Pass());
  }
  void OnViewHierarchyChanged(uint32_t view,
                              uint32_t new_parent,
                              uint32_t old_parent,
                              Array<ViewDataPtr> views) override {
    tracker_.OnViewHierarchyChanged(view, new_parent, old_parent, views.Pass());
  }
  void OnViewReordered(uint32_t view_id,
                       uint32_t relative_view_id,
                       mojo::OrderDirection direction) override {
    tracker_.OnViewReordered(view_id, relative_view_id, direction);
  }
  void OnViewDeleted(uint32_t view) override { tracker_.OnViewDeleted(view); }
  void OnViewVisibilityChanged(uint32_t view, bool visible) override {
    tracker_.OnViewVisibilityChanged(view, visible);
  }
  void OnViewDrawnStateChanged(uint32_t view, bool drawn) override {
    tracker_.OnViewDrawnStateChanged(view, drawn);
  }
  void OnViewSharedPropertyChanged(uint32_t view,
                                   const String& name,
                                   Array<uint8_t> new_data) override {
    tracker_.OnViewSharedPropertyChanged(view, name, new_data.Pass());
  }
  void OnViewInputEvent(uint32_t view,
                        mojo::EventPtr event,
                        const mojo::Callback<void()>& callback) override {
    tracker_.OnViewInputEvent(view, event.Pass());
  }
  void OnPerformAction(uint32_t view_id,
                       const String& name,
                       const mojo::Callback<void(bool)>& callback) override {}

  TestChangeTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(TestViewManagerClient);
};

// -----------------------------------------------------------------------------

// ClientConnection implementation that vends TestViewManagerClient.
class TestClientConnection : public ClientConnection {
 public:
  explicit TestClientConnection(scoped_ptr<ViewManagerServiceImpl> service_impl)
      : ClientConnection(service_impl.Pass(), &client_) {}
  ~TestClientConnection() override {}

  TestViewManagerClient* client() { return &client_; }

 private:
  TestViewManagerClient client_;

  DISALLOW_COPY_AND_ASSIGN(TestClientConnection);
};

// -----------------------------------------------------------------------------

// Empty implementation of ConnectionManagerDelegate.
class TestConnectionManagerDelegate : public ConnectionManagerDelegate {
 public:
  TestConnectionManagerDelegate() : last_connection_(nullptr) {}
  ~TestConnectionManagerDelegate() override {}

  TestViewManagerClient* last_client() {
    return last_connection_ ? last_connection_->client() : nullptr;
  }

  TestClientConnection* last_connection() { return last_connection_; }

 private:
  // ConnectionManagerDelegate:
  void OnLostConnectionToWindowManager() override {}

  ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
      mojo::ConnectionSpecificId creator_id,
      const std::string& creator_url,
      const std::string& url,
      const ViewId& root_id) override {
    scoped_ptr<ViewManagerServiceImpl> service(new ViewManagerServiceImpl(
        connection_manager, creator_id, creator_url, url, root_id));
    last_connection_ = new TestClientConnection(service.Pass());
    return last_connection_;
  }
  ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
      mojo::ConnectionSpecificId creator_id,
      const std::string& creator_url,
      const ViewId& root_id,
      mojo::ViewManagerClientPtr client) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  TestClientConnection* last_connection_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectionManagerDelegate);
};

// -----------------------------------------------------------------------------

// Empty implementation of DisplayManager.
class TestDisplayManager : public DisplayManager {
 public:
  TestDisplayManager() {}
  ~TestDisplayManager() override {}

  // DisplayManager:
  void Init(ConnectionManager* connection_manager) override {}
  void SchedulePaint(const ServerView* view, const gfx::Rect& bounds) override {
  }
  void SetViewportSize(const gfx::Size& size) override {}
  const mojo::ViewportMetrics& GetViewportMetrics() override {
    return display_metrices_;
  }

 private:
  mojo::ViewportMetrics display_metrices_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayManager);
};

// -----------------------------------------------------------------------------

// Empty implementation of WindowManagerInternal.
class TestWindowManagerInternal : public mojo::WindowManagerInternal {
 public:
  TestWindowManagerInternal() {}
  ~TestWindowManagerInternal() override {}

  // WindowManagerInternal:
  void CreateWindowManagerForViewManagerClient(
      uint16_t connection_id,
      mojo::ScopedMessagePipeHandle window_manager_pipe) override {}
  void SetViewManagerClient(mojo::ScopedMessagePipeHandle) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindowManagerInternal);
};

}  // namespace

// -----------------------------------------------------------------------------

class ViewManagerServiceTest : public testing::Test {
 public:
  ViewManagerServiceTest() : wm_client_(nullptr) {}
  ~ViewManagerServiceTest() override {}

  // ViewManagerServiceImpl for the window manager.
  ViewManagerServiceImpl* wm_connection() {
    return connection_manager_->GetConnection(1);
  }

  TestViewManagerClient* last_view_manager_client() {
    return delegate_.last_client();
  }

  TestClientConnection* last_client_connection() {
    return delegate_.last_connection();
  }

  ConnectionManager* connection_manager() { return connection_manager_.get(); }

  TestViewManagerClient* wm_client() { return wm_client_; }

 protected:
  // testing::Test:
  void SetUp() override {
    connection_manager_.reset(new ConnectionManager(
        &delegate_, scoped_ptr<DisplayManager>(new TestDisplayManager),
        &wm_internal_));
    scoped_ptr<ViewManagerServiceImpl> service(new ViewManagerServiceImpl(
        connection_manager_.get(), kInvalidConnectionId, std::string(),
        std::string("mojo:window_manager"), RootViewId()));
    scoped_ptr<TestClientConnection> client_connection(
        new TestClientConnection(service.Pass()));
    wm_client_ = client_connection->client();
    ASSERT_TRUE(wm_client_ != nullptr);
    connection_manager_->SetWindowManagerClientConnection(
        client_connection.Pass());
    ASSERT_TRUE(wm_connection() != nullptr);
    ASSERT_TRUE(wm_connection()->root() != nullptr);
  }

 private:
  // TestViewManagerClient that is used for the WM connection.
  TestViewManagerClient* wm_client_;

  TestWindowManagerInternal wm_internal_;
  TestConnectionManagerDelegate delegate_;
  scoped_ptr<ConnectionManager> connection_manager_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerServiceTest);
};

namespace {

const ServerView* GetFirstCloned(const ServerView* view) {
  for (const ServerView* child : view->GetChildren()) {
    if (child->id() == ClonedViewId())
      return child;
  }
  return nullptr;
}

// Provides common setup for animation tests. Creates the following views:
// 0,1 (the root, provided by view manager)
//   1,1 the second connection is embedded here (view owned by wm_connection()).
//     2,1 bounds=1,2 11x22
//       2,2 bounds=2,3 6x7
//         2,3 bounds=3,4 6x7
// CloneAndAnimate() is invoked for 2,2.
void SetUpAnimate1(ViewManagerServiceTest* test, ViewId* embed_view_id) {
  *embed_view_id = ViewId(test->wm_connection()->id(), 1);
  EXPECT_EQ(ERROR_CODE_NONE, test->wm_connection()->CreateView(*embed_view_id));
  EXPECT_TRUE(test->wm_connection()->SetViewVisibility(*embed_view_id, true));
  EXPECT_TRUE(test->wm_connection()->AddView(*(test->wm_connection()->root()),
                                             *embed_view_id));
  test->wm_connection()->EmbedUrl(std::string(), *embed_view_id, nullptr,
                                  nullptr);
  ViewManagerServiceImpl* connection1 =
      test->connection_manager()->GetConnectionWithRoot(*embed_view_id);
  ASSERT_TRUE(connection1 != nullptr);
  ASSERT_NE(connection1, test->wm_connection());

  const ViewId child1(connection1->id(), 1);
  EXPECT_EQ(ERROR_CODE_NONE, connection1->CreateView(child1));
  const ViewId child2(connection1->id(), 2);
  EXPECT_EQ(ERROR_CODE_NONE, connection1->CreateView(child2));
  const ViewId child3(connection1->id(), 3);
  EXPECT_EQ(ERROR_CODE_NONE, connection1->CreateView(child3));

  ServerView* v1 = connection1->GetView(child1);
  v1->SetVisible(true);
  v1->SetBounds(gfx::Rect(1, 2, 11, 22));
  ServerView* v2 = connection1->GetView(child2);
  v2->SetVisible(true);
  v2->SetBounds(gfx::Rect(2, 3, 6, 7));
  ServerView* v3 = connection1->GetView(child3);
  v3->SetVisible(true);
  v3->SetBounds(gfx::Rect(3, 4, 6, 7));

  EXPECT_TRUE(connection1->AddView(*embed_view_id, child1));
  EXPECT_TRUE(connection1->AddView(child1, child2));
  EXPECT_TRUE(connection1->AddView(child2, child3));

  TestViewManagerClient* connection1_client = test->last_view_manager_client();
  connection1_client->tracker()->changes()->clear();
  test->wm_client()->tracker()->changes()->clear();
  EXPECT_TRUE(test->connection_manager()->CloneAndAnimate(child2));
  EXPECT_TRUE(connection1_client->tracker()->changes()->empty());
  EXPECT_TRUE(test->wm_client()->tracker()->changes()->empty());

  // We cloned v2. The cloned view ends up as a sibling of it.
  const ServerView* cloned_view = GetFirstCloned(connection1->GetView(child1));
  ASSERT_TRUE(cloned_view);
  // |cloned_view| should have one and only one cloned child (corresponds to
  // |child3|).
  ASSERT_EQ(1u, cloned_view->GetChildren().size());
  EXPECT_TRUE(cloned_view->GetChildren()[0]->id() == ClonedViewId());

  // Cloned views should match the bounds of the view they were cloned from.
  EXPECT_EQ(v2->bounds(), cloned_view->bounds());
  EXPECT_EQ(v3->bounds(), cloned_view->GetChildren()[0]->bounds());

  // Cloned views are owned by the ConnectionManager and shouldn't be returned
  // from ViewManagerServiceImpl::GetView.
  EXPECT_TRUE(connection1->GetView(ClonedViewId()) == nullptr);
  EXPECT_TRUE(test->wm_connection()->GetView(ClonedViewId()) == nullptr);
}

}  // namespace

// Verifies ViewManagerService::GetViewTree() doesn't return cloned views.
TEST_F(ViewManagerServiceTest, ConnectionsCantSeeClonedViews) {
  ViewId embed_view_id;
  EXPECT_NO_FATAL_FAILURE(SetUpAnimate1(this, &embed_view_id));

  ViewManagerServiceImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_view_id);

  const ViewId child1(connection1->id(), 1);
  const ViewId child2(connection1->id(), 2);
  const ViewId child3(connection1->id(), 3);

  // Verify the root doesn't see any cloned views.
  std::vector<const ServerView*> views(
      wm_connection()->GetViewTree(*wm_connection()->root()));
  ASSERT_EQ(5u, views.size());
  ASSERT_TRUE(views[0]->id() == *wm_connection()->root());
  ASSERT_TRUE(views[1]->id() == embed_view_id);
  ASSERT_TRUE(views[2]->id() == child1);
  ASSERT_TRUE(views[3]->id() == child2);
  ASSERT_TRUE(views[4]->id() == child3);

  // Verify connection1 doesn't see any cloned views.
  std::vector<const ServerView*> v1_views(
      connection1->GetViewTree(embed_view_id));
  ASSERT_EQ(4u, v1_views.size());
  ASSERT_TRUE(v1_views[0]->id() == embed_view_id);
  ASSERT_TRUE(v1_views[1]->id() == child1);
  ASSERT_TRUE(v1_views[2]->id() == child2);
  ASSERT_TRUE(v1_views[3]->id() == child3);
}

TEST_F(ViewManagerServiceTest, ClonedViewsPromotedOnConnectionClose) {
  ViewId embed_view_id;
  EXPECT_NO_FATAL_FAILURE(SetUpAnimate1(this, &embed_view_id));

  // Destroy connection1, which should force the cloned view to become a child
  // of where it was embedded (the embedded view still exists).
  connection_manager()->OnConnectionError(last_client_connection());

  ServerView* embed_view = wm_connection()->GetView(embed_view_id);
  ASSERT_TRUE(embed_view != nullptr);
  const ServerView* cloned_view = GetFirstCloned(embed_view);
  ASSERT_TRUE(cloned_view);
  ASSERT_EQ(1u, cloned_view->GetChildren().size());
  EXPECT_TRUE(cloned_view->GetChildren()[0]->id() == ClonedViewId());

  // Because the cloned view changed parents its bounds should have changed.
  EXPECT_EQ(gfx::Rect(3, 5, 6, 7), cloned_view->bounds());
  // The bounds of the cloned child should not have changed though.
  EXPECT_EQ(gfx::Rect(3, 4, 6, 7), cloned_view->GetChildren()[0]->bounds());
}

TEST_F(ViewManagerServiceTest, ClonedViewsPromotedOnHide) {
  ViewId embed_view_id;
  EXPECT_NO_FATAL_FAILURE(SetUpAnimate1(this, &embed_view_id));

  ViewManagerServiceImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_view_id);

  // Hide the parent of the cloned view, which should force the cloned view to
  // become a sibling of the parent.
  const ServerView* view_to_hide =
      connection1->GetView(ViewId(connection1->id(), 1));
  ASSERT_TRUE(connection1->SetViewVisibility(view_to_hide->id(), false));

  const ServerView* cloned_view = GetFirstCloned(view_to_hide->parent());
  ASSERT_TRUE(cloned_view);
  ASSERT_EQ(1u, cloned_view->GetChildren().size());
  EXPECT_TRUE(cloned_view->GetChildren()[0]->id() == ClonedViewId());
  EXPECT_EQ(2u, cloned_view->parent()->GetChildren().size());
  EXPECT_TRUE(cloned_view->parent()->GetChildren()[1] == cloned_view);
}

// Clone and animate on a tree with more depth. Basically that of
// SetUpAnimate1() but cloning 2,1.
TEST_F(ViewManagerServiceTest, CloneAndAnimateLargerDepth) {
  const ViewId embed_view_id(wm_connection()->id(), 1);
  EXPECT_EQ(ERROR_CODE_NONE, wm_connection()->CreateView(embed_view_id));
  EXPECT_TRUE(wm_connection()->SetViewVisibility(embed_view_id, true));
  EXPECT_TRUE(
      wm_connection()->AddView(*(wm_connection()->root()), embed_view_id));
  wm_connection()->EmbedUrl(std::string(), embed_view_id, nullptr, nullptr);
  ViewManagerServiceImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_view_id);
  ASSERT_TRUE(connection1 != nullptr);
  ASSERT_NE(connection1, wm_connection());

  const ViewId child1(connection1->id(), 1);
  EXPECT_EQ(ERROR_CODE_NONE, connection1->CreateView(child1));
  const ViewId child2(connection1->id(), 2);
  EXPECT_EQ(ERROR_CODE_NONE, connection1->CreateView(child2));
  const ViewId child3(connection1->id(), 3);
  EXPECT_EQ(ERROR_CODE_NONE, connection1->CreateView(child3));

  ServerView* v1 = connection1->GetView(child1);
  v1->SetVisible(true);
  connection1->GetView(child2)->SetVisible(true);
  connection1->GetView(child3)->SetVisible(true);

  EXPECT_TRUE(connection1->AddView(embed_view_id, child1));
  EXPECT_TRUE(connection1->AddView(child1, child2));
  EXPECT_TRUE(connection1->AddView(child2, child3));

  TestViewManagerClient* connection1_client = last_view_manager_client();
  connection1_client->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();
  EXPECT_TRUE(connection_manager()->CloneAndAnimate(child1));
  EXPECT_TRUE(connection1_client->tracker()->changes()->empty());
  EXPECT_TRUE(wm_client()->tracker()->changes()->empty());

  // We cloned v1. The cloned view ends up as a sibling of it.
  const ServerView* cloned_view = GetFirstCloned(v1->parent());
  ASSERT_TRUE(cloned_view);
  // |cloned_view| should have a child and its child should have a child.
  ASSERT_EQ(1u, cloned_view->GetChildren().size());
  const ServerView* cloned_view_child = cloned_view->GetChildren()[0];
  EXPECT_EQ(1u, cloned_view_child->GetChildren().size());
  EXPECT_TRUE(cloned_view_child->id() == ClonedViewId());
}

}  // namespace view_manager
