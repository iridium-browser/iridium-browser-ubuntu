// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_TESTS_TEST_WINDOW_TREE_H_
#define SERVICES_UI_PUBLIC_CPP_TESTS_TEST_WINDOW_TREE_H_

#include <stdint.h>

#include <set>

#include "base/macros.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace ui {

// Testing WindowTree implementation.
class TestWindowTree : public mojom::WindowTree {
 public:
  TestWindowTree();
  ~TestWindowTree() override;

  // Returns the most recent change_id supplied to one of the WindowTree
  // functions. Returns false if one of the WindowTree functions has not been
  // invoked since the last GetAndClearChangeId().
  bool GetAndClearChangeId(uint32_t* change_id);

  uint32_t window_id() const { return window_id_; }

  bool WasEventAcked(uint32_t event_id) const;

 private:
  // mojom::WindowTree:
  void NewWindow(uint32_t change_id,
                 uint32_t window_id,
                 const base::Optional<
                     std::unordered_map<std::string, std::vector<uint8_t>>>&
                     properties) override;
  void NewTopLevelWindow(
      uint32_t change_id,
      uint32_t window_id,
      const std::unordered_map<std::string, std::vector<uint8_t>>& properties)
      override;
  void DeleteWindow(uint32_t change_id, uint32_t window_id) override;
  void SetWindowBounds(uint32_t change_id,
                       uint32_t window_id,
                       const gfx::Rect& bounds) override;
  void SetClientArea(uint32_t window_id,
                     const gfx::Insets& insets,
                     const base::Optional<std::vector<gfx::Rect>>&
                         additional_client_areas) override;
  void SetHitTestMask(uint32_t window_id,
                      const base::Optional<gfx::Rect>& mask) override;
  void SetCanAcceptDrops(uint32_t window_id, bool accepts_drags) override;
  void SetWindowVisibility(uint32_t change_id,
                           uint32_t window_id,
                           bool visible) override;
  void SetWindowProperty(
      uint32_t change_id,
      uint32_t window_id,
      const std::string& name,
      const base::Optional<std::vector<uint8_t>>& value) override;
  void SetWindowOpacity(uint32_t change_id,
                        uint32_t window_id,
                        float opacity) override;
  void AttachCompositorFrameSink(
      uint32_t window_id,
      mojo::InterfaceRequest<cc::mojom::MojoCompositorFrameSink> surface,
      cc::mojom::MojoCompositorFrameSinkClientPtr client) override;
  void AddWindow(uint32_t change_id, uint32_t parent, uint32_t child) override;
  void RemoveWindowFromParent(uint32_t change_id, uint32_t window_id) override;
  void AddTransientWindow(uint32_t change_id,
                          uint32_t window_id,
                          uint32_t transient_window_id) override;
  void RemoveTransientWindowFromParent(uint32_t change_id,
                                       uint32_t window_id) override;
  void SetModal(uint32_t change_id, uint32_t window_id) override;
  void ReorderWindow(uint32_t change_id,
                     uint32_t window_id,
                     uint32_t relative_window_id,
                     mojom::OrderDirection direction) override;
  void GetWindowTree(uint32_t window_id,
                     const GetWindowTreeCallback& callback) override;
  void SetCapture(uint32_t change_id, uint32_t window_id) override;
  void ReleaseCapture(uint32_t change_id, uint32_t window_id) override;
  void StartPointerWatcher(bool want_moves) override;
  void StopPointerWatcher() override;
  void Embed(uint32_t window_id,
             mojom::WindowTreeClientPtr client,
             uint32_t flags,
             const EmbedCallback& callback) override;
  void SetFocus(uint32_t change_id, uint32_t window_id) override;
  void SetCanFocus(uint32_t window_id, bool can_focus) override;
  void SetCanAcceptEvents(uint32_t window_id, bool can_accept_events) override;
  void SetPredefinedCursor(uint32_t change_id,
                           uint32_t window_id,
                           ui::mojom::Cursor cursor_id) override;
  void SetWindowTextInputState(uint32_t window_id,
                               mojo::TextInputStatePtr state) override;
  void SetImeVisibility(uint32_t window_id,
                        bool visible,
                        mojo::TextInputStatePtr state) override;
  void OnWindowInputEventAck(uint32_t event_id,
                             ui::mojom::EventResult result) override;
  void DeactivateWindow(uint32_t window_id) override;
  void GetWindowManagerClient(
      mojo::AssociatedInterfaceRequest<mojom::WindowManagerClient> internal)
      override;
  void GetCursorLocationMemory(const GetCursorLocationMemoryCallback& callback)
      override;
  void PerformDragDrop(
      uint32_t change_id,
      uint32_t source_window_id,
      const std::unordered_map<std::string, std::vector<uint8_t>>& drag_data,
      uint32_t drag_operation) override;
  void CancelDragDrop(uint32_t window_id) override;
  void PerformWindowMove(uint32_t change_id,
                         uint32_t window_id,
                         mojom::MoveLoopSource source,
                         const gfx::Point& cursor_location) override;
  void CancelWindowMove(uint32_t window_id) override;

  bool got_change_;
  uint32_t change_id_;
  std::set<uint32_t> acked_events_;
  uint32_t window_id_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTree);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_TESTS_TEST_WINDOW_TREE_H_
