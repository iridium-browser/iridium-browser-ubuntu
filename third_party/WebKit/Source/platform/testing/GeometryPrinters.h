// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryPrinters_h
#define GeometryPrinters_h

#include "platform/geometry/FloatRoundedRect.h"
#include <iosfwd>

namespace blink {

class FloatBox;
class FloatPoint;
class FloatQuad;
class FloatRect;
class FloatSize;
class LayoutRect;

// GTest print support for geometry classes.
//
// To avoid ODR violations, these should also be declared in the respective
// headers defining these types. This is required because otherwise a template
// instantiation may be instantiated differently, depending on whether this
// declaration is found.
//
// As a result, it is not necessary to include this file in tests in order to
// use these printers. If, however, you get a link error about these symbols,
// you need to make sure the blink_platform_test_support target is linked in
// your unit test binary.
void PrintTo(const FloatBox&, std::ostream*);
void PrintTo(const FloatPoint&, std::ostream*);
void PrintTo(const FloatQuad&, std::ostream*);
void PrintTo(const FloatRect&, std::ostream*);
void PrintTo(const FloatRoundedRect&, std::ostream*);
void PrintTo(const FloatRoundedRect::Radii&, std::ostream*);
void PrintTo(const FloatSize&, std::ostream*);
void PrintTo(const LayoutRect&, std::ostream*);

} // namespace blink

#endif // GeometryPrinters_h
