// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_WINDOW_TREE_CLIENT_H_
#define SERVICES_UI_PUBLIC_CPP_WINDOW_TREE_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ui/common/types.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_manager_delegate.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace display {
class Display;
}

namespace gfx {
class Insets;
}

namespace service_manager {
class Connector;
}

namespace ui {
class InFlightChange;
class WindowTreeClientDelegate;
class WindowTreeClientPrivate;
class WindowTreeClientObserver;
enum class ChangeType;

// Manages the connection with mus.
//
// WindowTreeClient is owned by the creator. Generally when the delegate gets
// one of OnEmbedRootDestroyed() or OnLostConnection() it should delete the
// WindowTreeClient.
//
// When WindowTreeClient is deleted all windows are deleted (and observers
// notified).
class WindowTreeClient : public mojom::WindowTreeClient,
                         public mojom::WindowManager,
                         public WindowManagerClient {
 public:
  explicit WindowTreeClient(
      WindowTreeClientDelegate* delegate,
      WindowManagerDelegate* window_manager_delegate = nullptr,
      mojom::WindowTreeClientRequest request = nullptr);
  ~WindowTreeClient() override;

  // Establishes the connection by way of the WindowTreeFactory.
  void ConnectViaWindowTreeFactory(service_manager::Connector* connector);

  // Establishes the connection by way of WindowManagerWindowTreeFactory.
  void ConnectAsWindowManager(service_manager::Connector* connector);

  // Wait for OnEmbed(), returning when done.
  void WaitForEmbed();

  bool connected() const { return tree_ != nullptr; }
  ClientSpecificId client_id() const { return client_id_; }

  // API exposed to the window implementations that pushes local changes to the
  // service.
  void DestroyWindow(Window* window);

  // These methods take TransportIds. For windows owned by the current client,
  // the client id high word can be zero. In all cases, the TransportId 0x1
  // refers to the root window.
  void AddChild(Window* parent, Id child_id);
  void RemoveChild(Window* parent, Id child_id);

  void AddTransientWindow(Window* window, Id transient_window_id);
  void RemoveTransientWindowFromParent(Window* window);

  void SetModal(Window* window);

  void Reorder(Window* window,
               Id relative_window_id,
               mojom::OrderDirection direction);

  // Returns true if the specified window was created by this client.
  bool WasCreatedByThisClient(const Window* window) const;

  void SetBounds(Window* window,
                 const gfx::Rect& old_bounds,
                 const gfx::Rect& bounds);
  void SetCapture(Window* window);
  void ReleaseCapture(Window* window);
  void SetClientArea(Id window_id,
                     const gfx::Insets& client_area,
                     const std::vector<gfx::Rect>& additional_client_areas);
  void SetHitTestMask(Id window_id, const gfx::Rect& mask);
  void ClearHitTestMask(Id window_id);
  void SetFocus(Window* window);
  void SetCanFocus(Id window_id, bool can_focus);
  void SetCanAcceptDrops(Id window_id, bool can_accept_drops);
  void SetCanAcceptEvents(Id window_id, bool can_accept_events);
  void SetPredefinedCursor(Id window_id, ui::mojom::Cursor cursor_id);
  void SetVisible(Window* window, bool visible);
  void SetOpacity(Window* window, float opacity);
  void SetProperty(Window* window,
                   const std::string& name,
                   const base::Optional<std::vector<uint8_t>>& data);
  void SetWindowTextInputState(Id window_id, mojo::TextInputStatePtr state);
  void SetImeVisibility(Id window_id,
                        bool visible,
                        mojo::TextInputStatePtr state);

  void Embed(Id window_id,
             mojom::WindowTreeClientPtr client,
             uint32_t flags,
             const mojom::WindowTree::EmbedCallback& callback);

  void RequestClose(Window* window);

  void AttachCompositorFrameSink(
      Id window_id,
      cc::mojom::MojoCompositorFrameSinkRequest compositor_frame_sink,
      cc::mojom::MojoCompositorFrameSinkClientPtr client);

  // Sets the input capture to |window| without notifying the server.
  void LocalSetCapture(Window* window);
  // Sets focus to |window| without notifying the server.
  void LocalSetFocus(Window* window);

  // Start/stop tracking windows. While tracked, they can be retrieved via
  // WindowTreeClient::GetWindowById.
  void AddWindow(Window* window);

  bool IsRoot(Window* window) const { return roots_.count(window) > 0; }

  void OnWindowDestroying(Window* window);

  // Called after the window's observers have been notified of destruction (as
  // the last step of ~Window).
  void OnWindowDestroyed(Window* window);

  Window* GetWindowByServerId(Id id);

  // Returns the root of this connection.
  const std::set<Window*>& GetRoots();

  // Returns the Window with input capture; null if no window has requested
  // input capture, or if another app has capture.
  Window* GetCaptureWindow();

  // Returns the focused window; null if focus is not yet known or another app
  // is focused.
  Window* GetFocusedWindow();

  // Sets focus to null. This does nothing if focus is currently null.
  void ClearFocus();

  // Returns the current location of the mouse on screen. Note: this method may
  // race the asynchronous initialization; but in that case we return (0, 0).
  gfx::Point GetCursorScreenPoint();

  // See description in window_tree.mojom. When an existing pointer watcher is
  // updated or cleared then any future events from the server for that watcher
  // will be ignored.
  void StartPointerWatcher(bool want_moves);
  void StopPointerWatcher();

  void PerformDragDrop(
      Window* window,
      const std::map<std::string, std::vector<uint8_t>>& drag_data,
      int drag_operation,
      const gfx::Point& cursor_location,
      const SkBitmap& bitmap,
      const base::Callback<void(bool, uint32_t)>& callback);

  // Cancels a in progress drag drop. (If no drag is in progress, does
  // nothing.)
  void CancelDragDrop(Window* window);

  // Performs a window move. |callback| will be asynchronously called with the
  // whether the move loop completed successfully.
  void PerformWindowMove(Window* window,
                         ui::mojom::MoveLoopSource source,
                         const gfx::Point& cursor_location,
                         const base::Callback<void(bool)>& callback);

  // Cancels a in progress window move. (If no window is currently being moved,
  // does nothing.)
  void CancelWindowMove(Window* window);

  // Creates and returns a new Window (which is owned by the window server).
  // Windows are initially hidden, use SetVisible(true) to show.
  Window* NewWindow() { return NewWindow(nullptr); }
  Window* NewWindow(
      const std::map<std::string, std::vector<uint8_t>>* properties);
  Window* NewTopLevelWindow(
      const std::map<std::string, std::vector<uint8_t>>* properties);

  void AddObserver(WindowTreeClientObserver* observer);
  void RemoveObserver(WindowTreeClientObserver* observer);

#if !defined(NDEBUG)
  std::string GetDebugWindowHierarchy() const;
  void BuildDebugInfo(const std::string& depth,
                      Window* window,
                      std::string* result) const;
#endif

 private:
  friend class WindowTreeClientPrivate;

  struct CurrentDragState;

  enum class NewWindowType {
    CHILD,
    TOP_LEVEL,
  };

  using IdToWindowMap = std::map<Id, Window*>;

  // TODO(sky): this assumes change_ids never wrap, which is a bad assumption.
  using InFlightMap = std::map<uint32_t, std::unique_ptr<InFlightChange>>;

  // Returns the oldest InFlightChange that matches |change|.
  InFlightChange* GetOldestInFlightChangeMatching(const InFlightChange& change);

  // See InFlightChange for details on how InFlightChanges are used.
  uint32_t ScheduleInFlightChange(std::unique_ptr<InFlightChange> change);

  // Returns true if there is an InFlightChange that matches |change|. If there
  // is an existing change SetRevertValueFrom() is invoked on it. Returns false
  // if there is no InFlightChange matching |change|.
  // See InFlightChange for details on how InFlightChanges are used.
  bool ApplyServerChangeToExistingInFlightChange(const InFlightChange& change);

  static Id server_id(const Window* window) { return window->server_id(); }

  void BuildWindowTree(const std::vector<mojom::WindowDataPtr>& windows,
                       Window* initial_parent);

  Window* NewWindowImpl(NewWindowType type,
                        const Window::SharedProperties* properties);

  // Sets the mojom::WindowTree implementation.
  void SetWindowTree(mojom::WindowTreePtr window_tree_ptr);

  // Called when the mojom::WindowTree connection is lost, deletes this.
  void OnConnectionLost();

  // OnEmbed() calls into this. Exposed as a separate function for testing.
  void OnEmbedImpl(mojom::WindowTree* window_tree,
                   ClientSpecificId client_id,
                   mojom::WindowDataPtr root_data,
                   int64_t display_id,
                   Id focused_window_id,
                   bool drawn);

  // Called by WmNewDisplayAdded().
  void WmNewDisplayAddedImpl(const display::Display& display,
                             mojom::WindowDataPtr root_data,
                             bool parent_drawn);

  void OnReceivedCursorLocationMemory(mojo::ScopedSharedBufferHandle handle);

  // Callback passed from WmPerformMoveLoop().
  void OnWmMoveLoopCompleted(uint32_t change_id, bool completed);

  // Overridden from WindowTreeClient:
  void OnEmbed(ClientSpecificId client_id,
               mojom::WindowDataPtr root,
               mojom::WindowTreePtr tree,
               int64_t display_id,
               Id focused_window_id,
               bool drawn) override;
  void OnEmbeddedAppDisconnected(Id window_id) override;
  void OnUnembed(Id window_id) override;
  void OnCaptureChanged(Id new_capture_window_id,
                        Id old_capture_window_id) override;
  void OnTopLevelCreated(uint32_t change_id,
                         mojom::WindowDataPtr data,
                         int64_t display_id,
                         bool drawn) override;
  void OnWindowBoundsChanged(Id window_id,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnClientAreaChanged(
      uint32_t window_id,
      const gfx::Insets& new_client_area,
      const std::vector<gfx::Rect>& new_additional_client_areas) override;
  void OnTransientWindowAdded(uint32_t window_id,
                              uint32_t transient_window_id) override;
  void OnTransientWindowRemoved(uint32_t window_id,
                                uint32_t transient_window_id) override;
  void OnWindowHierarchyChanged(
      Id window_id,
      Id old_parent_id,
      Id new_parent_id,
      std::vector<mojom::WindowDataPtr> windows) override;
  void OnWindowReordered(Id window_id,
                         Id relative_window_id,
                         mojom::OrderDirection direction) override;
  void OnWindowDeleted(Id window_id) override;
  void OnWindowVisibilityChanged(Id window_id, bool visible) override;
  void OnWindowOpacityChanged(Id window_id,
                              float old_opacity,
                              float new_opacity) override;
  void OnWindowParentDrawnStateChanged(Id window_id, bool drawn) override;
  void OnWindowSharedPropertyChanged(
      Id window_id,
      const std::string& name,
      const base::Optional<std::vector<uint8_t>>& new_data) override;
  void OnWindowInputEvent(uint32_t event_id,
                          Id window_id,
                          std::unique_ptr<ui::Event> event,
                          bool matches_pointer_watcher) override;
  void OnPointerEventObserved(std::unique_ptr<ui::Event> event,
                              uint32_t window_id) override;
  void OnWindowFocused(Id focused_window_id) override;
  void OnWindowPredefinedCursorChanged(Id window_id,
                                       mojom::Cursor cursor) override;
  void OnWindowSurfaceChanged(Id window_id,
                              const cc::SurfaceInfo& surface_info) override;
  void OnDragDropStart(
      const std::unordered_map<std::string, std::vector<uint8_t>>& mime_data)
      override;
  void OnDragEnter(Id window_id,
                   uint32_t event_flags,
                   const gfx::Point& position,
                   uint32_t effect_bitmask,
                   const OnDragEnterCallback& callback) override;
  void OnDragOver(Id window_id,
                  uint32_t event_flags,
                  const gfx::Point& position,
                  uint32_t effect_bitmask,
                  const OnDragOverCallback& callback) override;
  void OnDragLeave(Id window_id) override;
  void OnCompleteDrop(Id window_id,
                      uint32_t event_flags,
                      const gfx::Point& position,
                      uint32_t effect_bitmask,
                      const OnCompleteDropCallback& callback) override;
  void OnPerformDragDropCompleted(uint32_t window,
                                  bool success,
                                  uint32_t action_taken) override;
  void OnDragDropDone() override;
  void OnChangeCompleted(uint32_t change_id, bool success) override;
  void RequestClose(uint32_t window_id) override;
  void GetWindowManager(
      mojo::AssociatedInterfaceRequest<WindowManager> internal) override;

  // Overridden from WindowManager:
  void OnConnect(ClientSpecificId client_id) override;
  void WmNewDisplayAdded(const display::Display& display,
                         mojom::WindowDataPtr root_data,
                         bool parent_drawn) override;
  void WmDisplayRemoved(int64_t display_id) override;
  void WmDisplayModified(const display::Display& display) override;
  void WmSetBounds(uint32_t change_id,
                   Id window_id,
                   const gfx::Rect& transit_bounds) override;
  void WmSetProperty(
      uint32_t change_id,
      Id window_id,
      const std::string& name,
      const base::Optional<std::vector<uint8_t>>& transit_data) override;
  void WmCreateTopLevelWindow(
      uint32_t change_id,
      ClientSpecificId requesting_client_id,
      const std::unordered_map<std::string, std::vector<uint8_t>>&
          transport_properties) override;
  void WmClientJankinessChanged(ClientSpecificId client_id,
                                bool janky) override;
  void WmPerformMoveLoop(uint32_t change_id,
                         Id window_id,
                         mojom::MoveLoopSource source,
                         const gfx::Point& cursor_location) override;
  void WmCancelMoveLoop(uint32_t window_id) override;
  void WmDeactivateWindow(uint32_t window_id) override;
  void OnAccelerator(uint32_t ack_id,
                     uint32_t accelerator_id,
                     std::unique_ptr<ui::Event> event) override;

  // Overridden from WindowManagerClient:
  void SetFrameDecorationValues(
      mojom::FrameDecorationValuesPtr values) override;
  void SetNonClientCursor(Window* window, ui::mojom::Cursor cursor_id) override;
  void AddAccelerators(std::vector<mojom::AcceleratorPtr> accelerators,
                       const base::Callback<void(bool)>& callback) override;
  void RemoveAccelerator(uint32_t id) override;
  void AddActivationParent(Window* window) override;
  void RemoveActivationParent(Window* window) override;
  void ActivateNextWindow() override;
  void SetUnderlaySurfaceOffsetAndExtendedHitArea(
      Window* window,
      const gfx::Vector2d& offset,
      const gfx::Insets& hit_area) override;

  // The one int in |cursor_location_mapping_|. When we read from this
  // location, we must always read from it atomically.
  base::subtle::Atomic32* cursor_location_memory() {
    return reinterpret_cast<base::subtle::Atomic32*>(
        cursor_location_mapping_.get());
  }

  // This is set once and only once when we get OnEmbed(). It gives the unique
  // id for this client.
  ClientSpecificId client_id_;

  // Id assigned to the next window created.
  ClientSpecificId next_window_id_;

  // Id used for the next change id supplied to the server.
  uint32_t next_change_id_;
  InFlightMap in_flight_map_;

  WindowTreeClientDelegate* delegate_;

  WindowManagerDelegate* window_manager_delegate_;

  std::set<Window*> roots_;

  IdToWindowMap windows_;
  std::map<ClientSpecificId, std::set<Window*>> embedded_windows_;

  Window* capture_window_;

  Window* focused_window_;

  mojo::Binding<mojom::WindowTreeClient> binding_;
  mojom::WindowTreePtr tree_ptr_;
  // Typically this is the value contained in |tree_ptr_|, but tests may
  // directly set this.
  mojom::WindowTree* tree_;

  // Set to true if OnEmbed() was received.
  bool is_from_embed_ = false;

  bool in_destructor_;

  // A mapping to shared memory that is one 32 bit integer long. The window
  // server uses this to let us synchronously read the cursor location.
  mojo::ScopedSharedBufferMapping cursor_location_mapping_;

  base::ObserverList<WindowTreeClientObserver> observers_;

  std::unique_ptr<mojo::AssociatedBinding<mojom::WindowManager>>
      window_manager_internal_;
  mojom::WindowManagerClientAssociatedPtr window_manager_internal_client_;

  bool has_pointer_watcher_ = false;

  // The current change id for the client.
  uint32_t current_move_loop_change_ = 0u;

  // Callback executed when a move loop initiated by PerformWindowMove() is
  // completed.
  base::Callback<void(bool)> on_current_move_finished_;

  // The current change id for the window manager.gg
  uint32_t current_wm_move_loop_change_ = 0u;
  Id current_wm_move_loop_window_id_ = 0u;

  // State related to being the initiator of a drag started with
  // PerformDragDrop().
  std::unique_ptr<CurrentDragState> current_drag_state_;

  // The mus server sends the mime drag data once per connection; we cache this
  // and are responsible for sending it to all of our windows.

  std::map<std::string, std::vector<uint8_t>> mime_drag_data_;

  // A set of window ids for windows that we received an OnDragEnter() message
  // for. We maintain this set so we know who to send OnDragFinish() messages
  // at the end of the drag.
  std::set<Id> drag_entered_windows_;

  base::WeakPtrFactory<WindowTreeClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClient);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_WINDOW_TREE_CLIENT_H_
