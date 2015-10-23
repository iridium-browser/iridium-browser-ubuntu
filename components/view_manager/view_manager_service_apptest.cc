// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/view_manager/ids.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "components/view_manager/test_change_tracker.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"

using mojo::ApplicationConnection;
using mojo::ApplicationDelegate;
using mojo::Array;
using mojo::Callback;
using mojo::ConnectionSpecificId;
using mojo::ERROR_CODE_NONE;
using mojo::ErrorCode;
using mojo::EventPtr;
using mojo::Id;
using mojo::InterfaceRequest;
using mojo::ORDER_DIRECTION_ABOVE;
using mojo::ORDER_DIRECTION_BELOW;
using mojo::OrderDirection;
using mojo::RectPtr;
using mojo::ServiceProvider;
using mojo::ServiceProviderPtr;
using mojo::String;
using mojo::ViewDataPtr;
using mojo::ViewManagerClient;
using mojo::ViewManagerService;
using mojo::ViewportMetricsPtr;

namespace view_manager {

// Creates an id used for transport from the specified parameters.
Id BuildViewId(ConnectionSpecificId connection_id,
               ConnectionSpecificId view_id) {
  return (connection_id << 16) | view_id;
}

// Callback function from ViewManagerService functions. ------------------------

void BoolResultCallback(base::RunLoop* run_loop,
                        bool* result_cache,
                        bool result) {
  *result_cache = result;
  run_loop->Quit();
}

void ErrorCodeResultCallback(base::RunLoop* run_loop,
                             ErrorCode* result_cache,
                             ErrorCode result) {
  *result_cache = result;
  run_loop->Quit();
}

void ViewTreeResultCallback(base::RunLoop* run_loop,
                            std::vector<TestView>* views,
                            Array<ViewDataPtr> results) {
  ViewDatasToTestViews(results, views);
  run_loop->Quit();
}

// -----------------------------------------------------------------------------

bool EmbedUrl(mojo::ApplicationImpl* app,
              ViewManagerService* vm,
              const String& url,
              Id root_id) {
  bool result = false;
  base::RunLoop run_loop;
  {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(url);
    scoped_ptr<ApplicationConnection> connection =
        app->ConnectToApplication(request.Pass());
    mojo::ViewManagerClientPtr client;
    connection->ConnectToService(&client);
    vm->Embed(root_id, client.Pass(),
              base::Bind(&BoolResultCallback, &run_loop, &result));
  }
  run_loop.Run();
  return result;
}

bool EmbedAllowingReembed(mojo::ApplicationImpl* app,
                          ViewManagerService* vm,
                          const String& url,
                          Id root_id) {
  bool result = false;
  base::RunLoop run_loop;
  {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(url);
    vm->EmbedAllowingReembed(
        root_id, request.Pass(),
        base::Bind(&BoolResultCallback, &run_loop, &result));
  }
  run_loop.Run();
  return result;
}

bool Embed(ViewManagerService* vm,
           Id root_id,
           mojo::ViewManagerClientPtr client) {
  bool result = false;
  base::RunLoop run_loop;
  {
    vm->Embed(root_id, client.Pass(),
              base::Bind(&BoolResultCallback, &run_loop, &result));
  }
  run_loop.Run();
  return result;
}

ErrorCode CreateViewWithErrorCode(ViewManagerService* vm, Id view_id) {
  ErrorCode result = ERROR_CODE_NONE;
  base::RunLoop run_loop;
  vm->CreateView(view_id,
                 base::Bind(&ErrorCodeResultCallback, &run_loop, &result));
  run_loop.Run();
  return result;
}

bool AddView(ViewManagerService* vm, Id parent, Id child) {
  bool result = false;
  base::RunLoop run_loop;
  vm->AddView(parent, child,
              base::Bind(&BoolResultCallback, &run_loop, &result));
  run_loop.Run();
  return result;
}

bool RemoveViewFromParent(ViewManagerService* vm, Id view_id) {
  bool result = false;
  base::RunLoop run_loop;
  vm->RemoveViewFromParent(view_id,
                           base::Bind(&BoolResultCallback, &run_loop, &result));
  run_loop.Run();
  return result;
}

bool ReorderView(ViewManagerService* vm,
                 Id view_id,
                 Id relative_view_id,
                 OrderDirection direction) {
  bool result = false;
  base::RunLoop run_loop;
  vm->ReorderView(view_id, relative_view_id, direction,
                  base::Bind(&BoolResultCallback, &run_loop, &result));
  run_loop.Run();
  return result;
}

void GetViewTree(ViewManagerService* vm,
                 Id view_id,
                 std::vector<TestView>* views) {
  base::RunLoop run_loop;
  vm->GetViewTree(view_id,
                  base::Bind(&ViewTreeResultCallback, &run_loop, views));
  run_loop.Run();
}

bool DeleteView(ViewManagerService* vm, Id view_id) {
  base::RunLoop run_loop;
  bool result = false;
  vm->DeleteView(view_id, base::Bind(&BoolResultCallback, &run_loop, &result));
  run_loop.Run();
  return result;
}

bool SetViewBounds(ViewManagerService* vm,
                   Id view_id,
                   int x,
                   int y,
                   int w,
                   int h) {
  base::RunLoop run_loop;
  bool result = false;
  RectPtr rect(mojo::Rect::New());
  rect->x = x;
  rect->y = y;
  rect->width = w;
  rect->height = h;
  vm->SetViewBounds(view_id, rect.Pass(),
                    base::Bind(&BoolResultCallback, &run_loop, &result));
  run_loop.Run();
  return result;
}

bool SetViewVisibility(ViewManagerService* vm, Id view_id, bool visible) {
  base::RunLoop run_loop;
  bool result = false;
  vm->SetViewVisibility(view_id, visible,
                        base::Bind(&BoolResultCallback, &run_loop, &result));
  run_loop.Run();
  return result;
}

bool SetViewProperty(ViewManagerService* vm,
                     Id view_id,
                     const std::string& name,
                     const std::vector<uint8_t>* data) {
  base::RunLoop run_loop;
  bool result = false;
  Array<uint8_t> mojo_data;
  if (data)
    mojo_data = Array<uint8_t>::From(*data);
  vm->SetViewProperty(view_id, name, mojo_data.Pass(),
                      base::Bind(&BoolResultCallback, &run_loop, &result));
  run_loop.Run();
  return result;
}

// Utility functions -----------------------------------------------------------

// Waits for all messages to be received by |vm|. This is done by attempting to
// create a bogus view. When we get the response we know all messages have been
// processed.
bool WaitForAllMessages(ViewManagerService* vm) {
  ErrorCode result = ERROR_CODE_NONE;
  base::RunLoop run_loop;
  vm->CreateView(ViewIdToTransportId(InvalidViewId()),
                 base::Bind(&ErrorCodeResultCallback, &run_loop, &result));
  run_loop.Run();
  return result != ERROR_CODE_NONE;
}

bool HasClonedView(const std::vector<TestView>& views) {
  for (size_t i = 0; i < views.size(); ++i)
    if (views[i].view_id == ViewIdToTransportId(ClonedViewId()))
      return true;
  return false;
}

const Id kNullParentId = 0;
std::string IdToString(Id id) {
  return (id == kNullParentId)
             ? "null"
             : base::StringPrintf("%d,%d", mojo::HiWord(id), mojo::LoWord(id));
}

std::string ViewParentToString(Id view, Id parent) {
  return base::StringPrintf("view=%s parent=%s", IdToString(view).c_str(),
                            IdToString(parent).c_str());
}

// -----------------------------------------------------------------------------

// A ViewManagerClient implementation that logs all changes to a tracker.
class ViewManagerClientImpl : public mojo::ViewManagerClient,
                              public TestChangeTracker::Delegate {
 public:
  explicit ViewManagerClientImpl(mojo::ApplicationImpl* app)
      : binding_(this), app_(app), connection_id_(0), root_view_id_(0) {
    tracker_.set_delegate(this);
  }

  void Bind(mojo::InterfaceRequest<mojo::ViewManagerClient> request) {
    binding_.Bind(request.Pass());
  }

  mojo::ViewManagerService* service() { return service_.get(); }
  TestChangeTracker* tracker() { return &tracker_; }

  // Runs a nested MessageLoop until |count| changes (calls to
  // ViewManagerClient functions) have been received.
  void WaitForChangeCount(size_t count) {
    if (count == tracker_.changes()->size())
      return;

    ASSERT_TRUE(wait_state_.get() == nullptr);
    wait_state_.reset(new WaitState);
    wait_state_->change_count = count;
    wait_state_->run_loop.Run();
    wait_state_.reset();
  }

  // Runs a nested MessageLoop until OnEmbed() has been encountered.
  void WaitForOnEmbed() {
    if (service_)
      return;
    embed_run_loop_.reset(new base::RunLoop);
    embed_run_loop_->Run();
    embed_run_loop_.reset();
  }

  bool WaitForIncomingMethodCall() {
    return binding_.WaitForIncomingMethodCall();
  }

  Id CreateView(ConnectionSpecificId view_id) {
    ErrorCode result = ERROR_CODE_NONE;
    base::RunLoop run_loop;
    Id id = BuildViewId(connection_id_, view_id);
    service()->CreateView(
        id, base::Bind(&ErrorCodeResultCallback, &run_loop, &result));
    run_loop.Run();
    return result == ERROR_CODE_NONE ? id : 0;
  }

  void set_root_view(Id root_view_id) { root_view_id_ = root_view_id; }

 private:
  // Used when running a nested MessageLoop.
  struct WaitState {
    WaitState() : change_count(0) {}

    // Number of changes waiting for.
    size_t change_count;
    base::RunLoop run_loop;
  };

  // TestChangeTracker::Delegate:
  void OnChangeAdded() override {
    if (wait_state_.get() &&
        wait_state_->change_count == tracker_.changes()->size()) {
      wait_state_->run_loop.Quit();
    }
  }

  // ViewManagerClient:
  void OnEmbed(ConnectionSpecificId connection_id,
               ViewDataPtr root,
               mojo::ViewManagerServicePtr view_manager_service,
               mojo::Id focused_view_id) override {
    // TODO(sky): add coverage of |focused_view_id|.
    service_ = view_manager_service.Pass();
    connection_id_ = connection_id;
    tracker()->OnEmbed(connection_id, root.Pass());
    if (embed_run_loop_)
      embed_run_loop_->Quit();
  }
  void OnEmbedForDescendant(
      uint32_t view,
      mojo::URLRequestPtr request,
      const OnEmbedForDescendantCallback& callback) override {
    tracker()->OnEmbedForDescendant(view);
    mojo::ViewManagerClientPtr client;
    scoped_ptr<ApplicationConnection> connection =
        app_->ConnectToApplication(request.Pass());
    connection->ConnectToService(&client);
    callback.Run(client.Pass());
  }
  void OnEmbeddedAppDisconnected(Id view_id) override {
    tracker()->OnEmbeddedAppDisconnected(view_id);
  }
  void OnUnembed() override { tracker()->OnUnembed(); }
  void OnViewBoundsChanged(Id view_id,
                           RectPtr old_bounds,
                           RectPtr new_bounds) override {
    // The bounds of the root may change during startup on Android at random
    // times. As this doesn't matter, and shouldn't impact test exepctations,
    // it is ignored.
    if (view_id == root_view_id_)
        return;
    tracker()->OnViewBoundsChanged(view_id, old_bounds.Pass(),
                                   new_bounds.Pass());
  }
  void OnViewViewportMetricsChanged(ViewportMetricsPtr old_metrics,
                                    ViewportMetricsPtr new_metrics) override {
    // Don't track the metrics as they are available at an indeterministic time
    // on Android.
  }
  void OnViewHierarchyChanged(Id view,
                              Id new_parent,
                              Id old_parent,
                              Array<ViewDataPtr> views) override {
    tracker()->OnViewHierarchyChanged(view, new_parent, old_parent,
                                      views.Pass());
  }
  void OnViewReordered(Id view_id,
                       Id relative_view_id,
                       OrderDirection direction) override {
    tracker()->OnViewReordered(view_id, relative_view_id, direction);
  }
  void OnViewDeleted(Id view) override { tracker()->OnViewDeleted(view); }
  void OnViewVisibilityChanged(uint32_t view, bool visible) override {
    tracker()->OnViewVisibilityChanged(view, visible);
  }
  void OnViewDrawnStateChanged(uint32_t view, bool drawn) override {
    tracker()->OnViewDrawnStateChanged(view, drawn);
  }
  void OnViewInputEvent(Id view_id,
                        EventPtr event,
                        const Callback<void()>& callback) override {
    tracker()->OnViewInputEvent(view_id, event.Pass());
    callback.Run();
  }
  void OnViewSharedPropertyChanged(uint32_t view,
                                   const String& name,
                                   Array<uint8_t> new_data) override {
    tracker_.OnViewSharedPropertyChanged(view, name, new_data.Pass());
  }
  // TODO(sky): add testing coverage.
  void OnViewFocused(uint32_t focused_view_id) override {}

  TestChangeTracker tracker_;

  mojo::ViewManagerServicePtr service_;

  // If non-null we're waiting for OnEmbed() using this RunLoop.
  scoped_ptr<base::RunLoop> embed_run_loop_;

  // If non-null we're waiting for a certain number of change notifications to
  // be encountered.
  scoped_ptr<WaitState> wait_state_;

  mojo::Binding<ViewManagerClient> binding_;
  mojo::ApplicationImpl* app_;
  Id connection_id_;
  Id root_view_id_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerClientImpl);
};

// -----------------------------------------------------------------------------

// InterfaceFactory for vending ViewManagerClientImpls.
class ViewManagerClientFactory
    : public mojo::InterfaceFactory<ViewManagerClient> {
 public:
  explicit ViewManagerClientFactory(mojo::ApplicationImpl* app) : app_(app) {}
  ~ViewManagerClientFactory() override {}

  // Runs a nested MessageLoop until a new instance has been created.
  scoped_ptr<ViewManagerClientImpl> WaitForInstance() {
    if (!client_impl_.get()) {
      DCHECK(!run_loop_.get());
      run_loop_.reset(new base::RunLoop);
      run_loop_->Run();
      run_loop_.reset();
    }
    return client_impl_.Pass();
  }

 private:
  // InterfaceFactory<ViewManagerClient>:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<ViewManagerClient> request) override {
    client_impl_.reset(new ViewManagerClientImpl(app_));
    client_impl_->Bind(request.Pass());
    if (run_loop_.get())
      run_loop_->Quit();
  }

  mojo::ApplicationImpl* app_;
  scoped_ptr<ViewManagerClientImpl> client_impl_;
  scoped_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerClientFactory);
};

class ViewManagerServiceAppTest : public mojo::test::ApplicationTestBase,
                                  public ApplicationDelegate {
 public:
  ViewManagerServiceAppTest()
      : connection_id_1_(0), connection_id_2_(0), root_view_id_(0) {}
  ~ViewManagerServiceAppTest() override {}

 protected:
  enum class EmbedType {
    ALLOW_REEMBED,
    NO_REEMBED,
  };

  // Returns the changes from the various connections.
  std::vector<Change>* changes1() { return vm_client1_->tracker()->changes(); }
  std::vector<Change>* changes2() { return vm_client2_->tracker()->changes(); }
  std::vector<Change>* changes3() { return vm_client3_->tracker()->changes(); }

  // Various connections. |vm1()|, being the first connection, has special
  // permissions (it's treated as the window manager).
  ViewManagerService* vm1() { return vm_client1_->service(); }
  ViewManagerService* vm2() { return vm_client2_->service(); }
  ViewManagerService* vm3() { return vm_client3_->service(); }

  ViewManagerClientImpl* vm_client1() { return vm_client1_.get(); }
  ViewManagerClientImpl* vm_client2() { return vm_client2_.get(); }
  ViewManagerClientImpl* vm_client3() { return vm_client3_.get(); }

  Id root_view_id() const { return root_view_id_; }

  int connection_id_1() const { return connection_id_1_; }
  int connection_id_2() const { return connection_id_2_; }

  void EstablishSecondConnectionWithRoot(Id root_id) {
    ASSERT_TRUE(vm_client2_.get() == nullptr);
    vm_client2_ = EstablishConnectionViaEmbed(
        vm1(), root_id, EmbedType::NO_REEMBED, &connection_id_2_);
    ASSERT_GT(connection_id_2_, 0);
    ASSERT_TRUE(vm_client2_.get() != nullptr);
    vm_client2_->set_root_view(root_view_id_);
  }

  void EstablishSecondConnection(bool create_initial_view) {
    Id view_1_1 = 0;
    if (create_initial_view) {
      view_1_1 = vm_client1()->CreateView(1);
      ASSERT_TRUE(view_1_1);
    }
    ASSERT_NO_FATAL_FAILURE(
        EstablishSecondConnectionWithRoot(BuildViewId(connection_id_1(), 1)));

    if (create_initial_view) {
      EXPECT_EQ("[" + ViewParentToString(view_1_1, kNullParentId) + "]",
                ChangeViewDescription(*changes2()));
    }
  }

  void EstablishThirdConnection(ViewManagerService* owner, Id root_id) {
    ASSERT_TRUE(vm_client3_.get() == nullptr);
    vm_client3_ = EstablishConnectionViaEmbed(owner, root_id,
                                              EmbedType::NO_REEMBED, nullptr);
    ASSERT_TRUE(vm_client3_.get() != nullptr);
    vm_client3_->set_root_view(root_view_id_);
  }

  // Establishes a new connection by way of Embed() on the specified
  // ViewManagerService.
  scoped_ptr<ViewManagerClientImpl> EstablishConnectionViaEmbed(
      ViewManagerService* owner,
      Id root_id,
      EmbedType embed_type,
      int* connection_id) {
    if (embed_type == EmbedType::NO_REEMBED &&
        !EmbedUrl(application_impl(), owner, application_impl()->url(),
                  root_id)) {
      ADD_FAILURE() << "Embed() failed";
      return nullptr;
    } else if (embed_type == EmbedType::ALLOW_REEMBED &&
               !EmbedAllowingReembed(application_impl(), owner,
                                     application_impl()->url(), root_id)) {
      ADD_FAILURE() << "Embed() failed";
      return nullptr;
    }
    scoped_ptr<ViewManagerClientImpl> client =
        client_factory_->WaitForInstance();
    if (!client.get()) {
      ADD_FAILURE() << "WaitForInstance failed";
      return nullptr;
    }
    client->WaitForOnEmbed();

    EXPECT_EQ("OnEmbed",
              SingleChangeToDescription(*client->tracker()->changes()));
    if (connection_id)
      *connection_id = (*client->tracker()->changes())[0].connection_id;
    return client.Pass();
  }

  // ApplicationTestBase:
  ApplicationDelegate* GetApplicationDelegate() override { return this; }
  void SetUp() override {
    ApplicationTestBase::SetUp();
    client_factory_.reset(new ViewManagerClientFactory(application_impl()));
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:view_manager");
    scoped_ptr<ApplicationConnection> vm_connection =
        application_impl()->ConnectToApplication(request.Pass());
    vm_connection->AddService(client_factory_.get());
    vm_connection->ConnectToService(&view_manager_root_);
    vm_client1_ = client_factory_->WaitForInstance();
    ASSERT_TRUE(vm_client1_);
    // Next we should get an embed call on the "window manager" client.
    vm_client1_->WaitForIncomingMethodCall();
    ASSERT_EQ(1u, changes1()->size());
    EXPECT_EQ(CHANGE_TYPE_EMBED, (*changes1())[0].type);
    // All these tests assume 1 for the client id. The only real assertion here
    // is the client id is not zero, but adding this as rest of code here
    // assumes 1.
    ASSERT_GT((*changes1())[0].connection_id, 0);
    connection_id_1_ = (*changes1())[0].connection_id;
    ASSERT_FALSE((*changes1())[0].views.empty());
    root_view_id_ = (*changes1())[0].views[0].view_id;
    vm_client1_->set_root_view(root_view_id_);
    changes1()->clear();
  }

  // ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService(client_factory_.get());
    return true;
  }

  scoped_ptr<ViewManagerClientImpl> vm_client1_;
  scoped_ptr<ViewManagerClientImpl> vm_client2_;
  scoped_ptr<ViewManagerClientImpl> vm_client3_;

  mojo::ViewManagerRootPtr view_manager_root_;

 private:
  scoped_ptr<ViewManagerClientFactory> client_factory_;
  int connection_id_1_;
  int connection_id_2_;
  Id root_view_id_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewManagerServiceAppTest);
};

// Verifies two clients/connections get different ids.
TEST_F(ViewManagerServiceAppTest, TwoClientsGetDifferentConnectionIds) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  ASSERT_EQ(1u, changes2()->size());
  ASSERT_NE(connection_id_1(), connection_id_2());
}

// Verifies when Embed() is invoked any child views are removed.
TEST_F(ViewManagerServiceAppTest, ViewsRemovedWhenEmbedding) {
  // Two views 1 and 2. 2 is parented to 1.
  Id view_1_1 = vm_client1()->CreateView(1);
  ASSERT_TRUE(view_1_1);
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));

  Id view_1_2 = vm_client1()->CreateView(2);
  ASSERT_TRUE(view_1_2);
  ASSERT_TRUE(AddView(vm1(), view_1_1, view_1_2));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));
  ASSERT_EQ(1u, changes2()->size());
  ASSERT_EQ(1u, (*changes2())[0].views.size());
  EXPECT_EQ("[" + ViewParentToString(view_1_1, kNullParentId) + "]",
            ChangeViewDescription(*changes2()));

  // Embed() removed view 2.
  {
    std::vector<TestView> views;
    GetViewTree(vm1(), view_1_2, &views);
    EXPECT_EQ(ViewParentToString(view_1_2, kNullParentId),
              SingleViewDescription(views));
  }

  // vm2 should not see view 2.
  {
    std::vector<TestView> views;
    GetViewTree(vm2(), view_1_1, &views);
    EXPECT_EQ(ViewParentToString(view_1_1, kNullParentId),
              SingleViewDescription(views));
  }
  {
    std::vector<TestView> views;
    GetViewTree(vm2(), view_1_2, &views);
    EXPECT_TRUE(views.empty());
  }

  // Views 3 and 4 in connection 2.
  Id view_2_3 = vm_client2()->CreateView(3);
  Id view_2_4 = vm_client2()->CreateView(4);
  ASSERT_TRUE(view_2_3);
  ASSERT_TRUE(view_2_4);
  ASSERT_TRUE(AddView(vm2(), view_2_3, view_2_4));

  // Connection 3 rooted at 2.
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(vm2(), view_2_3));

  // View 4 should no longer have a parent.
  {
    std::vector<TestView> views;
    GetViewTree(vm2(), view_2_3, &views);
    EXPECT_EQ(ViewParentToString(view_2_3, kNullParentId),
              SingleViewDescription(views));

    views.clear();
    GetViewTree(vm2(), view_2_4, &views);
    EXPECT_EQ(ViewParentToString(view_2_4, kNullParentId),
              SingleViewDescription(views));
  }

  // And view 4 should not be visible to connection 3.
  {
    std::vector<TestView> views;
    GetViewTree(vm3(), view_2_3, &views);
    EXPECT_EQ(ViewParentToString(view_2_3, kNullParentId),
              SingleViewDescription(views));
  }
}

// Verifies once Embed() has been invoked the parent connection can't see any
// children.
TEST_F(ViewManagerServiceAppTest, CantAccessChildrenOfEmbeddedView) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  Id view_2_2 = vm_client2()->CreateView(2);
  ASSERT_TRUE(view_2_2);
  ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));

  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(vm2(), view_2_2));

  Id view_3_3 = vm_client3()->CreateView(3);
  ASSERT_TRUE(view_3_3);
  ASSERT_TRUE(AddView(vm3(), view_2_2, view_3_3));

  // Even though 3 is a child of 2 connection 2 can't see 3 as it's from a
  // different connection.
  {
    std::vector<TestView> views;
    GetViewTree(vm2(), view_2_2, &views);
    EXPECT_EQ(ViewParentToString(view_2_2, view_1_1),
              SingleViewDescription(views));
  }

  // Connection 2 shouldn't be able to get view 3 at all.
  {
    std::vector<TestView> views;
    GetViewTree(vm2(), view_3_3, &views);
    EXPECT_TRUE(views.empty());
  }

  // Connection 1 should be able to see it all (its the root).
  {
    std::vector<TestView> views;
    GetViewTree(vm1(), view_1_1, &views);
    ASSERT_EQ(3u, views.size());
    EXPECT_EQ(ViewParentToString(view_1_1, kNullParentId), views[0].ToString());
    EXPECT_EQ(ViewParentToString(view_2_2, view_1_1), views[1].ToString());
    EXPECT_EQ(ViewParentToString(view_3_3, view_2_2), views[2].ToString());
  }
}

// Verifies once Embed() has been invoked the parent can't mutate the children.
TEST_F(ViewManagerServiceAppTest, CantModifyChildrenOfEmbeddedView) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  Id view_2_2 = vm_client2()->CreateView(2);
  ASSERT_TRUE(view_2_2);
  ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));

  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(vm2(), view_2_2));

  Id view_2_3 = vm_client2()->CreateView(3);
  ASSERT_TRUE(view_2_3);
  // Connection 2 shouldn't be able to add anything to the view anymore.
  ASSERT_FALSE(AddView(vm2(), view_2_2, view_2_3));

  // Create view 3 in connection 3 and add it to view 3.
  Id view_3_3 = vm_client3()->CreateView(3);
  ASSERT_TRUE(view_3_3);
  ASSERT_TRUE(AddView(vm3(), view_2_2, view_3_3));

  // Connection 2 shouldn't be able to remove view 3.
  ASSERT_FALSE(RemoveViewFromParent(vm2(), view_3_3));
}

// Verifies client gets a valid id.
TEST_F(ViewManagerServiceAppTest, CreateView) {
  Id view_1_1 = vm_client1()->CreateView(1);
  ASSERT_TRUE(view_1_1);
  EXPECT_TRUE(changes1()->empty());

  // Can't create a view with the same id.
  ASSERT_EQ(mojo::ERROR_CODE_VALUE_IN_USE,
            CreateViewWithErrorCode(vm1(), view_1_1));
  EXPECT_TRUE(changes1()->empty());

  // Can't create a view with a bogus connection id.
  EXPECT_EQ(
      mojo::ERROR_CODE_ILLEGAL_ARGUMENT,
      CreateViewWithErrorCode(vm1(), BuildViewId(connection_id_1() + 1, 1)));
  EXPECT_TRUE(changes1()->empty());
}

// Verifies AddView fails when view is already in position.
TEST_F(ViewManagerServiceAppTest, AddViewWithNoChange) {
  Id view_1_2 = vm_client1()->CreateView(2);
  Id view_1_3 = vm_client1()->CreateView(3);
  ASSERT_TRUE(view_1_2);
  ASSERT_TRUE(view_1_3);

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Make 3 a child of 2.
  ASSERT_TRUE(AddView(vm1(), view_1_2, view_1_3));

  // Try again, this should fail.
  EXPECT_FALSE(AddView(vm1(), view_1_2, view_1_3));
}

// Verifies AddView fails when view is already in position.
TEST_F(ViewManagerServiceAppTest, AddAncestorFails) {
  Id view_1_2 = vm_client1()->CreateView(2);
  Id view_1_3 = vm_client1()->CreateView(3);
  ASSERT_TRUE(view_1_2);
  ASSERT_TRUE(view_1_3);

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  // Make 3 a child of 2.
  ASSERT_TRUE(AddView(vm1(), view_1_2, view_1_3));

  // Try to make 2 a child of 3, this should fail since 2 is an ancestor of 3.
  EXPECT_FALSE(AddView(vm1(), view_1_3, view_1_2));
}

// Verifies adding to root sends right notifications.
TEST_F(ViewManagerServiceAppTest, AddToRoot) {
  Id view_1_21 = vm_client1()->CreateView(21);
  Id view_1_3 = vm_client1()->CreateView(3);
  ASSERT_TRUE(view_1_21);
  ASSERT_TRUE(view_1_3);

  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  changes2()->clear();

  // Make 3 a child of 21.
  ASSERT_TRUE(AddView(vm1(), view_1_21, view_1_3));

  // Make 21 a child of 1.
  ASSERT_TRUE(AddView(vm1(), view_1_1, view_1_21));

  // Connection 2 should not be told anything (because the view is from a
  // different connection). Create a view to ensure we got a response from
  // the server.
  ASSERT_TRUE(vm_client2()->CreateView(100));
  EXPECT_TRUE(changes2()->empty());
}

// Verifies HierarchyChanged is correctly sent for various adds/removes.
TEST_F(ViewManagerServiceAppTest, ViewHierarchyChangedViews) {
  // 1,2->1,11.
  Id view_1_2 = vm_client1()->CreateView(2);
  ASSERT_TRUE(view_1_2);
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_2, true));
  Id view_1_11 = vm_client1()->CreateView(11);
  ASSERT_TRUE(view_1_11);
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_11, true));
  ASSERT_TRUE(AddView(vm1(), view_1_2, view_1_11));

  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_1, true));

  ASSERT_TRUE(WaitForAllMessages(vm2()));
  changes2()->clear();

  // 1,1->1,2->1,11
  {
    // Client 2 should not get anything (1,2 is from another connection).
    ASSERT_TRUE(AddView(vm1(), view_1_1, view_1_2));
    ASSERT_TRUE(WaitForAllMessages(vm2()));
    EXPECT_TRUE(changes2()->empty());
  }

  // 0,1->1,1->1,2->1,11.
  {
    // Client 2 is now connected to the root, so it should have gotten a drawn
    // notification.
    ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
    vm_client2_->WaitForChangeCount(1u);
    EXPECT_EQ("DrawnStateChanged view=" + IdToString(view_1_1) + " drawn=true",
              SingleChangeToDescription(*changes2()));
  }

  // 1,1->1,2->1,11.
  {
    // Client 2 is no longer connected to the root, should get drawn state
    // changed.
    changes2()->clear();
    ASSERT_TRUE(RemoveViewFromParent(vm1(), view_1_1));
    vm_client2_->WaitForChangeCount(1);
    EXPECT_EQ("DrawnStateChanged view=" + IdToString(view_1_1) + " drawn=false",
              SingleChangeToDescription(*changes2()));
  }

  // 1,1->1,2->1,11->1,111.
  Id view_1_111 = vm_client1()->CreateView(111);
  ASSERT_TRUE(view_1_111);
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_111, true));
  {
    changes2()->clear();
    ASSERT_TRUE(AddView(vm1(), view_1_11, view_1_111));
    ASSERT_TRUE(WaitForAllMessages(vm2()));
    EXPECT_TRUE(changes2()->empty());
  }

  // 0,1->1,1->1,2->1,11->1,111
  {
    changes2()->clear();
    ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
    vm_client2_->WaitForChangeCount(1);
    EXPECT_EQ("DrawnStateChanged view=" + IdToString(view_1_1) + " drawn=true",
              SingleChangeToDescription(*changes2()));
  }
}

TEST_F(ViewManagerServiceAppTest, ViewHierarchyChangedAddingKnownToUnknown) {
  // Create the following structure: root -> 1 -> 11 and 2->21 (2 has no
  // parent).
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id view_1_1 = BuildViewId(connection_id_1(), 1);

  Id view_2_11 = vm_client2()->CreateView(11);
  Id view_2_2 = vm_client2()->CreateView(2);
  Id view_2_21 = vm_client2()->CreateView(21);
  ASSERT_TRUE(view_2_11);
  ASSERT_TRUE(view_2_2);
  ASSERT_TRUE(view_2_21);

  // Set up the hierarchy.
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
  ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_11));
  ASSERT_TRUE(AddView(vm2(), view_2_2, view_2_21));

  // Remove 11, should result in a hierarchy change for the root.
  {
    changes1()->clear();
    ASSERT_TRUE(RemoveViewFromParent(vm2(), view_2_11));

    vm_client1_->WaitForChangeCount(1);
    EXPECT_EQ("HierarchyChanged view=" + IdToString(view_2_11) +
                  " new_parent=null old_parent=" + IdToString(view_1_1),
              SingleChangeToDescription(*changes1()));
  }

  // Add 2 to 1.
  {
    changes1()->clear();
    ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));
    vm_client1_->WaitForChangeCount(1);
    EXPECT_EQ("HierarchyChanged view=" + IdToString(view_2_2) + " new_parent=" +
                  IdToString(view_1_1) + " old_parent=null",
              SingleChangeToDescription(*changes1()));
    EXPECT_EQ("[" + ViewParentToString(view_2_2, view_1_1) + "],[" +
                  ViewParentToString(view_2_21, view_2_2) + "]",
              ChangeViewDescription(*changes1()));
  }
}

TEST_F(ViewManagerServiceAppTest, ReorderView) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  Id view_2_1 = vm_client2()->CreateView(1);
  Id view_2_2 = vm_client2()->CreateView(2);
  Id view_2_3 = vm_client2()->CreateView(3);
  Id view_1_4 = vm_client1()->CreateView(4);  // Peer to 1,1
  Id view_1_5 = vm_client1()->CreateView(5);  // Peer to 1,1
  Id view_2_6 = vm_client2()->CreateView(6);  // Child of 1,2.
  Id view_2_7 = vm_client2()->CreateView(7);  // Unparented.
  Id view_2_8 = vm_client2()->CreateView(8);  // Unparented.
  ASSERT_TRUE(view_2_1);
  ASSERT_TRUE(view_2_2);
  ASSERT_TRUE(view_2_3);
  ASSERT_TRUE(view_1_4);
  ASSERT_TRUE(view_1_5);
  ASSERT_TRUE(view_2_6);
  ASSERT_TRUE(view_2_7);
  ASSERT_TRUE(view_2_8);

  ASSERT_TRUE(AddView(vm2(), view_2_1, view_2_2));
  ASSERT_TRUE(AddView(vm2(), view_2_2, view_2_6));
  ASSERT_TRUE(AddView(vm2(), view_2_1, view_2_3));
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_4));
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_5));
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_2_1));

  {
    changes1()->clear();
    ASSERT_TRUE(ReorderView(vm2(), view_2_2, view_2_3, ORDER_DIRECTION_ABOVE));

    vm_client1_->WaitForChangeCount(1);
    EXPECT_EQ("Reordered view=" + IdToString(view_2_2) + " relative=" +
                  IdToString(view_2_3) + " direction=above",
              SingleChangeToDescription(*changes1()));
  }

  {
    changes1()->clear();
    ASSERT_TRUE(ReorderView(vm2(), view_2_2, view_2_3, ORDER_DIRECTION_BELOW));

    vm_client1_->WaitForChangeCount(1);
    EXPECT_EQ("Reordered view=" + IdToString(view_2_2) + " relative=" +
                  IdToString(view_2_3) + " direction=below",
              SingleChangeToDescription(*changes1()));
  }

  // view2 is already below view3.
  EXPECT_FALSE(ReorderView(vm2(), view_2_2, view_2_3, ORDER_DIRECTION_BELOW));

  // view4 & 5 are unknown to connection2_.
  EXPECT_FALSE(ReorderView(vm2(), view_1_4, view_1_5, ORDER_DIRECTION_ABOVE));

  // view6 & view3 have different parents.
  EXPECT_FALSE(ReorderView(vm1(), view_2_3, view_2_6, ORDER_DIRECTION_ABOVE));

  // Non-existent view-ids
  EXPECT_FALSE(ReorderView(vm1(), BuildViewId(connection_id_1(), 27),
                           BuildViewId(connection_id_1(), 28),
                           ORDER_DIRECTION_ABOVE));

  // view7 & view8 are un-parented.
  EXPECT_FALSE(ReorderView(vm1(), view_2_7, view_2_8, ORDER_DIRECTION_ABOVE));
}

// Verifies DeleteView works.
TEST_F(ViewManagerServiceAppTest, DeleteView) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  Id view_2_2 = vm_client2()->CreateView(2);
  ASSERT_TRUE(view_2_2);

  // Make 2 a child of 1.
  {
    changes1()->clear();
    ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));
    vm_client1_->WaitForChangeCount(1);
    EXPECT_EQ("HierarchyChanged view=" + IdToString(view_2_2) + " new_parent=" +
                  IdToString(view_1_1) + " old_parent=null",
              SingleChangeToDescription(*changes1()));
  }

  // Delete 2.
  {
    changes1()->clear();
    changes2()->clear();
    ASSERT_TRUE(DeleteView(vm2(), view_2_2));
    EXPECT_TRUE(changes2()->empty());

    vm_client1_->WaitForChangeCount(1);
    EXPECT_EQ("ViewDeleted view=" + IdToString(view_2_2),
              SingleChangeToDescription(*changes1()));
  }
}

// Verifies DeleteView isn't allowed from a separate connection.
TEST_F(ViewManagerServiceAppTest, DeleteViewFromAnotherConnectionDisallowed) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  EXPECT_FALSE(DeleteView(vm2(), BuildViewId(connection_id_1(), 1)));
}

// Verifies if a view was deleted and then reused that other clients are
// properly notified.
TEST_F(ViewManagerServiceAppTest, ReuseDeletedViewId) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  Id view_2_2 = vm_client2()->CreateView(2);
  ASSERT_TRUE(view_2_2);

  // Add 2 to 1.
  {
    changes1()->clear();
    ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));
    vm_client1_->WaitForChangeCount(1);
    EXPECT_EQ("HierarchyChanged view=" + IdToString(view_2_2) + " new_parent=" +
                  IdToString(view_1_1) + " old_parent=null",
              SingleChangeToDescription(*changes1()));
    EXPECT_EQ("[" + ViewParentToString(view_2_2, view_1_1) + "]",
              ChangeViewDescription(*changes1()));
  }

  // Delete 2.
  {
    changes1()->clear();
    ASSERT_TRUE(DeleteView(vm2(), view_2_2));

    vm_client1_->WaitForChangeCount(1);
    EXPECT_EQ("ViewDeleted view=" + IdToString(view_2_2),
              SingleChangeToDescription(*changes1()));
  }

  // Create 2 again, and add it back to 1. Should get the same notification.
  view_2_2 = vm_client2()->CreateView(2);
  ASSERT_TRUE(view_2_2);
  {
    changes1()->clear();
    ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));

    vm_client1_->WaitForChangeCount(1);
    EXPECT_EQ("HierarchyChanged view=" + IdToString(view_2_2) + " new_parent=" +
                  IdToString(view_1_1) + " old_parent=null",
              SingleChangeToDescription(*changes1()));
    EXPECT_EQ("[" + ViewParentToString(view_2_2, view_1_1) + "]",
              ChangeViewDescription(*changes1()));
  }
}

// Assertions for GetViewTree.
TEST_F(ViewManagerServiceAppTest, GetViewTree) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id view_1_1 = BuildViewId(connection_id_1(), 1);

  // Create 11 in first connection and make it a child of 1.
  Id view_1_11 = vm_client1()->CreateView(11);
  ASSERT_TRUE(view_1_11);
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
  ASSERT_TRUE(AddView(vm1(), view_1_1, view_1_11));

  // Create two views in second connection, 2 and 3, both children of 1.
  Id view_2_2 = vm_client2()->CreateView(2);
  Id view_2_3 = vm_client2()->CreateView(3);
  ASSERT_TRUE(view_2_2);
  ASSERT_TRUE(view_2_3);
  ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));
  ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_3));

  // Verifies GetViewTree() on the root. The root connection sees all.
  {
    std::vector<TestView> views;
    GetViewTree(vm1(), root_view_id(), &views);
    ASSERT_EQ(5u, views.size());
    EXPECT_EQ(ViewParentToString(root_view_id(), kNullParentId),
              views[0].ToString());
    EXPECT_EQ(ViewParentToString(view_1_1, root_view_id()),
              views[1].ToString());
    EXPECT_EQ(ViewParentToString(view_1_11, view_1_1), views[2].ToString());
    EXPECT_EQ(ViewParentToString(view_2_2, view_1_1), views[3].ToString());
    EXPECT_EQ(ViewParentToString(view_2_3, view_1_1), views[4].ToString());
  }

  // Verifies GetViewTree() on the view 1,1 from vm2(). vm2() sees 1,1 as 1,1
  // is vm2()'s root and vm2() sees all the views it created.
  {
    std::vector<TestView> views;
    GetViewTree(vm2(), view_1_1, &views);
    ASSERT_EQ(3u, views.size());
    EXPECT_EQ(ViewParentToString(view_1_1, kNullParentId), views[0].ToString());
    EXPECT_EQ(ViewParentToString(view_2_2, view_1_1), views[1].ToString());
    EXPECT_EQ(ViewParentToString(view_2_3, view_1_1), views[2].ToString());
  }

  // Connection 2 shouldn't be able to get the root tree.
  {
    std::vector<TestView> views;
    GetViewTree(vm2(), root_view_id(), &views);
    ASSERT_EQ(0u, views.size());
  }
}

TEST_F(ViewManagerServiceAppTest, SetViewBounds) {
  Id view_1_1 = vm_client1()->CreateView(1);
  ASSERT_TRUE(view_1_1);
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  changes2()->clear();
  ASSERT_TRUE(SetViewBounds(vm1(), view_1_1, 0, 0, 100, 100));

  vm_client2_->WaitForChangeCount(1);
  EXPECT_EQ("BoundsChanged view=" + IdToString(view_1_1) +
                " old_bounds=0,0 0x0 new_bounds=0,0 100x100",
            SingleChangeToDescription(*changes2()));

  // Should not be possible to change the bounds of a view created by another
  // connection.
  ASSERT_FALSE(SetViewBounds(vm2(), view_1_1, 0, 0, 0, 0));
}

// Verify AddView fails when trying to manipulate views in other roots.
TEST_F(ViewManagerServiceAppTest, CantMoveViewsFromOtherRoot) {
  // Create 1 and 2 in the first connection.
  Id view_1_1 = vm_client1()->CreateView(1);
  Id view_1_2 = vm_client1()->CreateView(2);
  ASSERT_TRUE(view_1_1);
  ASSERT_TRUE(view_1_2);

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Try to move 2 to be a child of 1 from connection 2. This should fail as 2
  // should not be able to access 1.
  ASSERT_FALSE(AddView(vm2(), view_1_1, view_1_2));

  // Try to reparent 1 to the root. A connection is not allowed to reparent its
  // roots.
  ASSERT_FALSE(AddView(vm2(), root_view_id(), view_1_1));
}

// Verify RemoveViewFromParent fails for views that are descendants of the
// roots.
TEST_F(ViewManagerServiceAppTest, CantRemoveViewsInOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  Id view_1_1 = vm_client1()->CreateView(1);
  Id view_1_2 = vm_client1()->CreateView(2);
  ASSERT_TRUE(view_1_1);
  ASSERT_TRUE(view_1_2);

  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_2));

  // Establish the second connection and give it the root 1.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Connection 2 should not be able to remove view 2 or 1 from its parent.
  ASSERT_FALSE(RemoveViewFromParent(vm2(), view_1_2));
  ASSERT_FALSE(RemoveViewFromParent(vm2(), view_1_1));

  // Create views 10 and 11 in 2.
  Id view_2_10 = vm_client2()->CreateView(10);
  Id view_2_11 = vm_client2()->CreateView(11);
  ASSERT_TRUE(view_2_10);
  ASSERT_TRUE(view_2_11);

  // Parent 11 to 10.
  ASSERT_TRUE(AddView(vm2(), view_2_10, view_2_11));
  // Remove 11 from 10.
  ASSERT_TRUE(RemoveViewFromParent(vm2(), view_2_11));

  // Verify nothing was actually removed.
  {
    std::vector<TestView> views;
    GetViewTree(vm1(), root_view_id(), &views);
    ASSERT_EQ(3u, views.size());
    EXPECT_EQ(ViewParentToString(root_view_id(), kNullParentId),
              views[0].ToString());
    EXPECT_EQ(ViewParentToString(view_1_1, root_view_id()),
              views[1].ToString());
    EXPECT_EQ(ViewParentToString(view_1_2, root_view_id()),
              views[2].ToString());
  }
}

// Verify GetViewTree fails for views that are not descendants of the roots.
TEST_F(ViewManagerServiceAppTest, CantGetViewTreeOfOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  Id view_1_1 = vm_client1()->CreateView(1);
  Id view_1_2 = vm_client1()->CreateView(2);
  ASSERT_TRUE(view_1_1);
  ASSERT_TRUE(view_1_2);

  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_2));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  std::vector<TestView> views;

  // Should get nothing for the root.
  GetViewTree(vm2(), root_view_id(), &views);
  ASSERT_TRUE(views.empty());

  // Should get nothing for view 2.
  GetViewTree(vm2(), view_1_2, &views);
  ASSERT_TRUE(views.empty());

  // Should get view 1 if asked for.
  GetViewTree(vm2(), view_1_1, &views);
  ASSERT_EQ(1u, views.size());
  EXPECT_EQ(ViewParentToString(view_1_1, kNullParentId), views[0].ToString());
}

TEST_F(ViewManagerServiceAppTest, EmbedWithSameViewId) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  changes2()->clear();

  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(vm1(), view_1_1));

  // Connection2 should have been told of the unembed and delete.
  {
    vm_client2_->WaitForChangeCount(2);
    EXPECT_EQ("OnUnembed", ChangesToDescription1(*changes2())[0]);
    EXPECT_EQ("ViewDeleted view=" + IdToString(view_1_1),
              ChangesToDescription1(*changes2())[1]);
  }

  // Connection2 has no root. Verify it can't see view 1,1 anymore.
  {
    std::vector<TestView> views;
    GetViewTree(vm2(), view_1_1, &views);
    EXPECT_TRUE(views.empty());
  }
}

TEST_F(ViewManagerServiceAppTest, EmbedWithSameViewId2) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  changes2()->clear();

  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(vm1(), view_1_1));

  // Connection2 should have been told about the unembed and delete.
  vm_client2_->WaitForChangeCount(2);
  changes2()->clear();

  // Create a view in the third connection and parent it to the root.
  Id view_3_1 = vm_client3()->CreateView(1);
  ASSERT_TRUE(view_3_1);
  ASSERT_TRUE(AddView(vm3(), view_1_1, view_3_1));

  // Connection 1 should have been told about the add (it owns the view).
  {
    vm_client1_->WaitForChangeCount(1);
    EXPECT_EQ("HierarchyChanged view=" + IdToString(view_3_1) + " new_parent=" +
                  IdToString(view_1_1) + " old_parent=null",
              SingleChangeToDescription(*changes1()));
  }

  // Embed 1,1 again.
  {
    changes3()->clear();

    // We should get a new connection for the new embedding.
    scoped_ptr<ViewManagerClientImpl> connection4(EstablishConnectionViaEmbed(
        vm1(), view_1_1, EmbedType::NO_REEMBED, nullptr));
    ASSERT_TRUE(connection4.get());
    EXPECT_EQ("[" + ViewParentToString(view_1_1, kNullParentId) + "]",
              ChangeViewDescription(*connection4->tracker()->changes()));

    // And 3 should get an unembed and delete.
    vm_client3_->WaitForChangeCount(2);
    EXPECT_EQ("OnUnembed", ChangesToDescription1(*changes3())[0]);
    EXPECT_EQ("ViewDeleted view=" + IdToString(view_1_1),
              ChangesToDescription1(*changes3())[1]);
  }

  // vm3() has no root. Verify it can't see view 1,1 anymore.
  {
    std::vector<TestView> views;
    GetViewTree(vm3(), view_1_1, &views);
    EXPECT_TRUE(views.empty());
  }

  // Verify 3,1 is no longer parented to 1,1. We have to do this from 1,1 as
  // vm3() can no longer see 1,1.
  {
    std::vector<TestView> views;
    GetViewTree(vm1(), view_1_1, &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ(ViewParentToString(view_1_1, kNullParentId), views[0].ToString());
  }

  // Verify vm3() can still see the view it created 3,1.
  {
    std::vector<TestView> views;
    GetViewTree(vm3(), view_3_1, &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ(ViewParentToString(view_3_1, kNullParentId), views[0].ToString());
  }
}

// Assertions for SetViewVisibility.
TEST_F(ViewManagerServiceAppTest, SetViewVisibility) {
  // Create 1 and 2 in the first connection and parent both to the root.
  Id view_1_1 = vm_client1()->CreateView(1);
  Id view_1_2 = vm_client1()->CreateView(2);
  ASSERT_TRUE(view_1_1);
  ASSERT_TRUE(view_1_2);

  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
  {
    std::vector<TestView> views;
    GetViewTree(vm1(), root_view_id(), &views);
    ASSERT_EQ(2u, views.size());
    EXPECT_EQ(ViewParentToString(root_view_id(), kNullParentId) +
                  " visible=true drawn=true",
              views[0].ToString2());
    EXPECT_EQ(ViewParentToString(view_1_1, root_view_id()) +
                  " visible=false drawn=false",
              views[1].ToString2());
  }

  // Show all the views.
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_1, true));
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_2, true));
  {
    std::vector<TestView> views;
    GetViewTree(vm1(), root_view_id(), &views);
    ASSERT_EQ(2u, views.size());
    EXPECT_EQ(ViewParentToString(root_view_id(), kNullParentId) +
                  " visible=true drawn=true",
              views[0].ToString2());
    EXPECT_EQ(ViewParentToString(view_1_1, root_view_id()) +
                  " visible=true drawn=true",
              views[1].ToString2());
  }

  // Hide 1.
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_1, false));
  {
    std::vector<TestView> views;
    GetViewTree(vm1(), view_1_1, &views);
    ASSERT_EQ(1u, views.size());
    EXPECT_EQ(ViewParentToString(view_1_1, root_view_id()) +
                  " visible=false drawn=false",
              views[0].ToString2());
  }

  // Attach 2 to 1.
  ASSERT_TRUE(AddView(vm1(), view_1_1, view_1_2));
  {
    std::vector<TestView> views;
    GetViewTree(vm1(), view_1_1, &views);
    ASSERT_EQ(2u, views.size());
    EXPECT_EQ(ViewParentToString(view_1_1, root_view_id()) +
                  " visible=false drawn=false",
              views[0].ToString2());
    EXPECT_EQ(
        ViewParentToString(view_1_2, view_1_1) + " visible=true drawn=false",
        views[1].ToString2());
  }

  // Show 1.
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_1, true));
  {
    std::vector<TestView> views;
    GetViewTree(vm1(), view_1_1, &views);
    ASSERT_EQ(2u, views.size());
    EXPECT_EQ(ViewParentToString(view_1_1, root_view_id()) +
                  " visible=true drawn=true",
              views[0].ToString2());
    EXPECT_EQ(
        ViewParentToString(view_1_2, view_1_1) + " visible=true drawn=true",
        views[1].ToString2());
  }
}

// Assertions for SetViewVisibility sending notifications.
TEST_F(ViewManagerServiceAppTest, SetViewVisibilityNotifications) {
  // Create 1,1 and 1,2. 1,2 is made a child of 1,1 and 1,1 a child of the root.
  Id view_1_1 = vm_client1()->CreateView(1);
  ASSERT_TRUE(view_1_1);
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_1, true));
  Id view_1_2 = vm_client1()->CreateView(2);
  ASSERT_TRUE(view_1_2);
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_2, true));
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
  ASSERT_TRUE(AddView(vm1(), view_1_1, view_1_2));

  // Establish the second connection at 1,2.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnectionWithRoot(view_1_2));

  // Add 2,3 as a child of 1,2.
  Id view_2_3 = vm_client2()->CreateView(3);
  ASSERT_TRUE(view_2_3);
  ASSERT_TRUE(SetViewVisibility(vm2(), view_2_3, true));
  ASSERT_TRUE(AddView(vm2(), view_1_2, view_2_3));
  WaitForAllMessages(vm1());

  changes2()->clear();
  // Hide 1,2 from connection 1. Connection 2 should see this.
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_2, false));
  {
    vm_client2_->WaitForChangeCount(1);
    EXPECT_EQ(
        "VisibilityChanged view=" + IdToString(view_1_2) + " visible=false",
        SingleChangeToDescription(*changes2()));
  }

  changes1()->clear();
  // Show 1,2 from connection 2, connection 1 should be notified.
  ASSERT_TRUE(SetViewVisibility(vm2(), view_1_2, true));
  {
    vm_client1_->WaitForChangeCount(1);
    EXPECT_EQ(
        "VisibilityChanged view=" + IdToString(view_1_2) + " visible=true",
        SingleChangeToDescription(*changes1()));
  }

  changes2()->clear();
  // Hide 1,1, connection 2 should be told the draw state changed.
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_1, false));
  {
    vm_client2_->WaitForChangeCount(1);
    EXPECT_EQ("DrawnStateChanged view=" + IdToString(view_1_2) + " drawn=false",
              SingleChangeToDescription(*changes2()));
  }

  changes2()->clear();
  // Show 1,1 from connection 1. Connection 2 should see this.
  ASSERT_TRUE(SetViewVisibility(vm1(), view_1_1, true));
  {
    vm_client2_->WaitForChangeCount(1);
    EXPECT_EQ("DrawnStateChanged view=" + IdToString(view_1_2) + " drawn=true",
              SingleChangeToDescription(*changes2()));
  }

  // Change visibility of 2,3, connection 1 should see this.
  changes1()->clear();
  ASSERT_TRUE(SetViewVisibility(vm2(), view_2_3, false));
  {
    vm_client1_->WaitForChangeCount(1);
    EXPECT_EQ(
        "VisibilityChanged view=" + IdToString(view_2_3) + " visible=false",
        SingleChangeToDescription(*changes1()));
  }

  changes2()->clear();
  // Remove 1,1 from the root, connection 2 should see drawn state changed.
  ASSERT_TRUE(RemoveViewFromParent(vm1(), view_1_1));
  {
    vm_client2_->WaitForChangeCount(1);
    EXPECT_EQ("DrawnStateChanged view=" + IdToString(view_1_2) + " drawn=false",
              SingleChangeToDescription(*changes2()));
  }

  changes2()->clear();
  // Add 1,1 back to the root, connection 2 should see drawn state changed.
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
  {
    vm_client2_->WaitForChangeCount(1);
    EXPECT_EQ("DrawnStateChanged view=" + IdToString(view_1_2) + " drawn=true",
              SingleChangeToDescription(*changes2()));
  }
}

TEST_F(ViewManagerServiceAppTest, SetViewProperty) {
  Id view_1_1 = vm_client1()->CreateView(1);
  ASSERT_TRUE(view_1_1);

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));
  changes2()->clear();

  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
  {
    std::vector<TestView> views;
    GetViewTree(vm1(), root_view_id(), &views);
    ASSERT_EQ(2u, views.size());
    EXPECT_EQ(root_view_id(), views[0].view_id);
    EXPECT_EQ(view_1_1, views[1].view_id);
    ASSERT_EQ(0u, views[1].properties.size());
  }

  // Set properties on 1.
  changes2()->clear();
  std::vector<uint8_t> one(1, '1');
  ASSERT_TRUE(SetViewProperty(vm1(), view_1_1, "one", &one));
  {
    vm_client2_->WaitForChangeCount(1);
    EXPECT_EQ(
        "PropertyChanged view=" + IdToString(view_1_1) + " key=one value=1",
        SingleChangeToDescription(*changes2()));
  }

  // Test that our properties exist in the view tree
  {
    std::vector<TestView> views;
    GetViewTree(vm1(), view_1_1, &views);
    ASSERT_EQ(1u, views.size());
    ASSERT_EQ(1u, views[0].properties.size());
    EXPECT_EQ(one, views[0].properties["one"]);
  }

  changes2()->clear();
  // Set back to null.
  ASSERT_TRUE(SetViewProperty(vm1(), view_1_1, "one", NULL));
  {
    vm_client2_->WaitForChangeCount(1);
    EXPECT_EQ(
        "PropertyChanged view=" + IdToString(view_1_1) + " key=one value=NULL",
        SingleChangeToDescription(*changes2()));
  }
}

TEST_F(ViewManagerServiceAppTest, OnEmbeddedAppDisconnected) {
  // Create connection 2 and 3.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  Id view_2_2 = vm_client2()->CreateView(2);
  ASSERT_TRUE(view_2_2);
  ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));
  changes2()->clear();
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(vm2(), view_2_2));

  // Close connection 3. Connection 2 (which had previously embedded 3) should
  // be notified of this.
  vm_client3_.reset();
  vm_client2_->WaitForChangeCount(1);
  EXPECT_EQ("OnEmbeddedAppDisconnected view=" + IdToString(view_2_2),
            SingleChangeToDescription(*changes2()));
}

// Verifies when the parent of an Embed() is destroyed the embedded app gets
// a ViewDeleted (and doesn't trigger a DCHECK).
TEST_F(ViewManagerServiceAppTest, OnParentOfEmbedDisconnects) {
  // Create connection 2 and 3.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
  Id view_2_2 = vm_client2()->CreateView(2);
  Id view_2_3 = vm_client2()->CreateView(3);
  ASSERT_TRUE(view_2_2);
  ASSERT_TRUE(view_2_3);
  ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));
  ASSERT_TRUE(AddView(vm2(), view_2_2, view_2_3));
  changes2()->clear();
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(vm2(), view_2_3));
  changes3()->clear();

  // Close connection 2. Connection 3 should get a delete (for its root).
  vm_client2_.reset();
  vm_client3_->WaitForChangeCount(1);
  EXPECT_EQ("ViewDeleted view=" + IdToString(view_2_3),
            SingleChangeToDescription(*changes3()));
}

// Verifies ViewManagerServiceImpl doesn't incorrectly erase from its internal
// map when a view from another connection with the same view_id is removed.
TEST_F(ViewManagerServiceAppTest, DontCleanMapOnDestroy) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  ASSERT_TRUE(vm_client2()->CreateView(1));
  changes1()->clear();
  vm_client2_.reset();
  vm_client1_->WaitForChangeCount(1);
  EXPECT_EQ("OnEmbeddedAppDisconnected view=" + IdToString(view_1_1),
            SingleChangeToDescription(*changes1()));
  std::vector<TestView> views;
  GetViewTree(vm1(), view_1_1, &views);
  EXPECT_FALSE(views.empty());
}

TEST_F(ViewManagerServiceAppTest, CloneAndAnimate) {
  // Create connection 2 and 3.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
  Id view_2_2 = vm_client2()->CreateView(2);
  Id view_2_3 = vm_client2()->CreateView(3);
  ASSERT_TRUE(view_2_2);
  ASSERT_TRUE(view_2_3);
  ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));
  ASSERT_TRUE(AddView(vm2(), view_2_2, view_2_3));
  changes2()->clear();

  ASSERT_TRUE(WaitForAllMessages(vm1()));
  changes1()->clear();

  view_manager_root_->CloneAndAnimate(view_2_3);
  ASSERT_TRUE(WaitForAllMessages(vm1()));

  ASSERT_TRUE(WaitForAllMessages(vm1()));
  ASSERT_TRUE(WaitForAllMessages(vm2()));

  // No messages should have been received.
  EXPECT_TRUE(changes1()->empty());
  EXPECT_TRUE(changes2()->empty());

  // No one should be able to see the cloned tree.
  std::vector<TestView> views;
  GetViewTree(vm1(), view_1_1, &views);
  EXPECT_FALSE(HasClonedView(views));
  views.clear();

  GetViewTree(vm2(), view_1_1, &views);
  EXPECT_FALSE(HasClonedView(views));
}

// Verifies Embed() works when supplying a ViewManagerClient.
TEST_F(ViewManagerServiceAppTest, EmbedSupplyingViewManagerClient) {
  ASSERT_TRUE(vm_client1()->CreateView(1));

  ViewManagerClientImpl client2(application_impl());
  mojo::ViewManagerClientPtr client2_ptr;
  mojo::Binding<ViewManagerClient> client2_binding(&client2, &client2_ptr);
  ASSERT_TRUE(
      Embed(vm1(), BuildViewId(connection_id_1(), 1), client2_ptr.Pass()));
  client2.WaitForOnEmbed();
  EXPECT_EQ("OnEmbed",
            SingleChangeToDescription(*client2.tracker()->changes()));
}

TEST_F(ViewManagerServiceAppTest, OnWillEmbed) {
  // Create connections 2 and 3, marking 2 as an embed root.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  ASSERT_TRUE(AddView(vm1(), root_view_id(), view_1_1));
  Id view_2_2 = vm_client2()->CreateView(2);
  ASSERT_TRUE(view_2_2);
  ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(vm2(), view_2_2));
  Id view_3_3 = vm_client3()->CreateView(3);
  ASSERT_TRUE(view_3_3);
  ASSERT_TRUE(AddView(vm3(), view_2_2, view_3_3));
  vm2()->SetEmbedRoot();
  // Make sure the viewmanager processed the SetEmbedRoot() call.
  ASSERT_TRUE(WaitForAllMessages(vm2()));
  changes2()->clear();

  // Embed 4 into 3, connection 2 should get the OnWillEmbed.
  scoped_ptr<ViewManagerClientImpl> connection4(EstablishConnectionViaEmbed(
      vm3(), view_3_3, EmbedType::ALLOW_REEMBED, nullptr));
  ASSERT_TRUE(connection4.get());
  EXPECT_EQ("OnEmbedForDescendant view=" + IdToString(view_3_3),
            SingleChangeToDescription(*changes2()));

  // Mark 3 as an embed root.
  vm3()->SetEmbedRoot();
  // Make sure the viewmanager processed the SetEmbedRoot() call.
  ASSERT_TRUE(WaitForAllMessages(vm3()));
  changes2()->clear();
  changes3()->clear();

  // Embed 5 into 4. Only 3 should get the will embed.
  Id view_4_4 = connection4->CreateView(4);
  ASSERT_TRUE(view_4_4);
  ASSERT_TRUE(AddView(connection4->service(), view_3_3, view_4_4));

  // vm3() and vm2() should see view_4_4 as they are embed roots.
  ASSERT_TRUE(WaitForAllMessages(vm3()));
  EXPECT_EQ("HierarchyChanged view=" + IdToString(view_4_4) + " new_parent=" +
                IdToString(view_3_3) + " old_parent=null",
            SingleChangeToDescription(*changes3()));
  changes3()->clear();

  ASSERT_TRUE(WaitForAllMessages(vm2()));
  EXPECT_EQ("HierarchyChanged view=" + IdToString(view_4_4) + " new_parent=" +
                IdToString(view_3_3) + " old_parent=null",
            SingleChangeToDescription(*changes2()));
  changes2()->clear();

  scoped_ptr<ViewManagerClientImpl> connection5(EstablishConnectionViaEmbed(
      connection4->service(), view_4_4, EmbedType::ALLOW_REEMBED, nullptr));
  ASSERT_TRUE(connection5.get());
  EXPECT_EQ("OnEmbedForDescendant view=" + IdToString(view_4_4),
            SingleChangeToDescription(*changes3()));
  ASSERT_TRUE(changes2()->empty());
}

TEST_F(ViewManagerServiceAppTest, EmbedFailsFromOtherConnection) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  Id view_2_2 = vm_client2()->CreateView(2);
  ASSERT_TRUE(view_2_2);
  ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(vm2(), view_2_2));

  Id view_3_3 = vm_client3()->CreateView(3);
  ASSERT_TRUE(view_3_3);
  ASSERT_TRUE(AddView(vm3(), view_2_2, view_3_3));

  // 2 should not be able to embed in view_3_3 as view_3_3 was not created by
  // 2.
  EXPECT_FALSE(EmbedUrl(application_impl(), vm2(), application_impl()->url(),
                        view_3_3));
}

// Verifies Embed() from window manager on another connections view works.
TEST_F(ViewManagerServiceAppTest, EmbedFromOtherConnection) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  Id view_1_1 = BuildViewId(connection_id_1(), 1);
  Id view_2_2 = vm_client2()->CreateView(2);
  ASSERT_TRUE(view_2_2);
  ASSERT_TRUE(AddView(vm2(), view_1_1, view_2_2));

  changes2()->clear();

  // Establish a third connection in view_2_2.
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(vm1(), view_2_2));

  WaitForAllMessages(vm2());
  EXPECT_EQ(std::string(), SingleChangeToDescription(*changes2()));
}

// TODO(sky): need to better track changes to initial connection. For example,
// that SetBounsdViews/AddView and the like don't result in messages to the
// originating connection.

// TODO(sky): make sure coverage of what was
// ViewManagerTest.SecondEmbedRoot_InitService and
// ViewManagerTest.MultipleEmbedRootsBeforeWTHReady gets added to window manager
// tests.

}  // namespace view_manager
