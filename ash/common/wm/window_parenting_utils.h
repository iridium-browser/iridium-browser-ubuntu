// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_WINDOW_PARENTING_UTILS_H_
#define ASH_COMMON_WM_WINDOW_PARENTING_UTILS_H_

namespace ash {

class WmWindow;

namespace wm {

// Changes the parent of a |child| and all its transient children that are
// themselves children of |old_parent| to |new_parent|.
void ReparentChildWithTransientChildren(WmWindow* child,
                                        WmWindow* old_parent,
                                        WmWindow* new_parent);

// Changes the parent of all transient children of a |child| to |new_parent|.
// Does not change parent of the transient children that are not themselves
// children of |old_parent|.
void ReparentTransientChildrenOfChild(WmWindow* child,
                                      WmWindow* old_parent,
                                      WmWindow* new_parent);

}  // namespace wm
}  // namespace ash

#endif  // ASH_COMMON_WM_WINDOW_PARENTING_UTILS_H_
