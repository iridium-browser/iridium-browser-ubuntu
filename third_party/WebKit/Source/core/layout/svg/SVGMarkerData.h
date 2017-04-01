/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGMarkerData_h
#define SVGMarkerData_h

#include "platform/graphics/Path.h"
#include "wtf/Allocator.h"
#include "wtf/MathExtras.h"

namespace blink {

enum SVGMarkerType { StartMarker, MidMarker, EndMarker };

struct MarkerPosition {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  MarkerPosition(SVGMarkerType useType,
                 const FloatPoint& useOrigin,
                 float useAngle)
      : type(useType), origin(useOrigin), angle(useAngle) {}

  SVGMarkerType type;
  FloatPoint origin;
  float angle;
};

class LayoutSVGResourceMarker;

class SVGMarkerData {
  STACK_ALLOCATED();

 public:
  SVGMarkerData(Vector<MarkerPosition>& positions, bool autoStartReverse)
      : m_positions(positions),
        m_elementIndex(0),
        m_autoStartReverse(autoStartReverse) {}

  static void updateFromPathElement(void* info, const PathElement* element) {
    static_cast<SVGMarkerData*>(info)->updateFromPathElement(*element);
  }

  void pathIsDone() {
    m_positions.push_back(
        MarkerPosition(EndMarker, m_origin, currentAngle(EndMarker)));
  }

  static inline LayoutSVGResourceMarker* markerForType(
      const SVGMarkerType& type,
      LayoutSVGResourceMarker* markerStart,
      LayoutSVGResourceMarker* markerMid,
      LayoutSVGResourceMarker* markerEnd) {
    switch (type) {
      case StartMarker:
        return markerStart;
      case MidMarker:
        return markerMid;
      case EndMarker:
        return markerEnd;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
  }

 private:
  float currentAngle(SVGMarkerType type) const {
    // For details of this calculation, see:
    // http://www.w3.org/TR/SVG/single-page.html#painting-MarkerElement
    FloatPoint inSlope(m_inslopePoints[1] - m_inslopePoints[0]);
    FloatPoint outSlope(m_outslopePoints[1] - m_outslopePoints[0]);

    double inAngle = rad2deg(inSlope.slopeAngleRadians());
    double outAngle = rad2deg(outSlope.slopeAngleRadians());

    switch (type) {
      case StartMarker:
        if (m_autoStartReverse)
          outAngle += 180;
        return clampTo<float>(outAngle);
      case MidMarker:
        // WK193015: Prevent bugs due to angles being non-continuous.
        if (fabs(inAngle - outAngle) > 180)
          inAngle += 360;
        return clampTo<float>((inAngle + outAngle) / 2);
      case EndMarker:
        return clampTo<float>(inAngle);
    }

    ASSERT_NOT_REACHED();
    return 0;
  }

  void updateOutslope(const PathElement& element) {
    m_outslopePoints[0] = m_origin;
    FloatPoint point = element.type == PathElementCloseSubpath
                           ? m_subpathStart
                           : element.points[0];
    m_outslopePoints[1] = point;
  }

  void updateFromPathElement(const PathElement& element) {
    // First update the outslope for the previous element.
    updateOutslope(element);

    // Record the marker for the previous element.
    if (m_elementIndex > 0) {
      SVGMarkerType markerType = m_elementIndex == 1 ? StartMarker : MidMarker;
      m_positions.push_back(
          MarkerPosition(markerType, m_origin, currentAngle(markerType)));
    }

    // Update our marker data for this element.
    updateMarkerDataForPathElement(element);
    ++m_elementIndex;
  }

  void updateMarkerDataForPathElement(const PathElement& element) {
    const FloatPoint* points = element.points;

    switch (element.type) {
      case PathElementAddQuadCurveToPoint:
        m_inslopePoints[0] = points[0];
        m_inslopePoints[1] = points[1];
        m_origin = points[1];
        break;
      case PathElementAddCurveToPoint:
        m_inslopePoints[0] = points[1];
        m_inslopePoints[1] = points[2];
        m_origin = points[2];
        break;
      case PathElementMoveToPoint:
        m_subpathStart = points[0];
      case PathElementAddLineToPoint:
        updateInslope(points[0]);
        m_origin = points[0];
        break;
      case PathElementCloseSubpath:
        updateInslope(m_subpathStart);
        m_origin = m_subpathStart;
        m_subpathStart = FloatPoint();
    }
  }

  void updateInslope(const FloatPoint& point) {
    m_inslopePoints[0] = m_origin;
    m_inslopePoints[1] = point;
  }

  Vector<MarkerPosition>& m_positions;
  unsigned m_elementIndex;
  FloatPoint m_origin;
  FloatPoint m_subpathStart;
  FloatPoint m_inslopePoints[2];
  FloatPoint m_outslopePoints[2];
  bool m_autoStartReverse;
};

}  // namespace blink

#endif  // SVGMarkerData_h
