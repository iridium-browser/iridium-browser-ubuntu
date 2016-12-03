// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_SHELF_TEST_API_H_
#define ASH_TEST_SHELF_TEST_API_H_

#include "ash/common/shelf/shelf.h"
#include "base/macros.h"

namespace ash {
namespace test {

// Use the api in this class to access private members of Shelf.
class ShelfTestAPI {
 public:
  explicit ShelfTestAPI(Shelf* shelf) : shelf_(shelf) {}
  ~ShelfTestAPI() {}

  WmShelf* wm_shelf() { return shelf_->wm_shelf_; }

  ShelfView* shelf_view() { return shelf_->shelf_view_; }

 private:
  Shelf* shelf_;

  DISALLOW_COPY_AND_ASSIGN(ShelfTestAPI);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_SHELF_TEST_API_H_
