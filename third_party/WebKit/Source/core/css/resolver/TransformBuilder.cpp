/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/css/resolver/TransformBuilder.h"

#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/MatrixTransformOperation.h"
#include "platform/transforms/PerspectiveTransformOperation.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/SkewTransformOperation.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/transforms/TranslateTransformOperation.h"

namespace blink {

static Length convertToFloatLength(
    const CSSPrimitiveValue& primitiveValue,
    const CSSToLengthConversionData& conversionData) {
  return primitiveValue.convertToLength(conversionData);
}

static TransformOperation::OperationType getTransformOperationType(
    CSSValueID type) {
  switch (type) {
    default:
      NOTREACHED();
    case CSSValueScale:
      return TransformOperation::Scale;
    case CSSValueScaleX:
      return TransformOperation::ScaleX;
    case CSSValueScaleY:
      return TransformOperation::ScaleY;
    case CSSValueScaleZ:
      return TransformOperation::ScaleZ;
    case CSSValueScale3d:
      return TransformOperation::Scale3D;
    case CSSValueTranslate:
      return TransformOperation::Translate;
    case CSSValueTranslateX:
      return TransformOperation::TranslateX;
    case CSSValueTranslateY:
      return TransformOperation::TranslateY;
    case CSSValueTranslateZ:
      return TransformOperation::TranslateZ;
    case CSSValueTranslate3d:
      return TransformOperation::Translate3D;
    case CSSValueRotate:
      return TransformOperation::Rotate;
    case CSSValueRotateX:
      return TransformOperation::RotateX;
    case CSSValueRotateY:
      return TransformOperation::RotateY;
    case CSSValueRotateZ:
      return TransformOperation::RotateZ;
    case CSSValueRotate3d:
      return TransformOperation::Rotate3D;
    case CSSValueSkew:
      return TransformOperation::Skew;
    case CSSValueSkewX:
      return TransformOperation::SkewX;
    case CSSValueSkewY:
      return TransformOperation::SkewY;
    case CSSValueMatrix:
      return TransformOperation::Matrix;
    case CSSValueMatrix3d:
      return TransformOperation::Matrix3D;
    case CSSValuePerspective:
      return TransformOperation::Perspective;
  }
}

bool TransformBuilder::hasRelativeLengths(const CSSValueList& valueList) {
  for (auto& value : valueList) {
    const CSSFunctionValue* transformValue = toCSSFunctionValue(value.get());

    for (const CSSValue* item : *transformValue) {
      const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(*item);

      // TODO(hs1217.lee) : to prevent relative unit like calc(10px + 1em).
      // but when calc() not take parameter of ralative unit like calc(1px +1
      // px),
      // shoud be return false;
      if (primitiveValue.isCalculated()) {
        return true;
      }

      if (CSSPrimitiveValue::isRelativeUnit(
              primitiveValue.typeWithCalcResolved())) {
        return true;
      }
    }
  }
  return false;
}

TransformOperations TransformBuilder::createTransformOperations(
    const CSSValue& inValue,
    const CSSToLengthConversionData& conversionData) {
  TransformOperations operations;
  if (!inValue.isValueList()) {
    DCHECK_EQ(toCSSIdentifierValue(inValue).getValueID(), CSSValueNone);
    return operations;
  }

  float zoomFactor = conversionData.zoom();
  for (auto& value : toCSSValueList(inValue)) {
    const CSSFunctionValue* transformValue = toCSSFunctionValue(value.get());
    TransformOperation::OperationType transformType =
        getTransformOperationType(transformValue->functionType());

    const CSSPrimitiveValue& firstValue =
        toCSSPrimitiveValue(transformValue->item(0));

    switch (transformType) {
      case TransformOperation::Scale:
      case TransformOperation::ScaleX:
      case TransformOperation::ScaleY: {
        double sx = 1.0;
        double sy = 1.0;
        if (transformType == TransformOperation::ScaleY) {
          sy = firstValue.getDoubleValue();
        } else {
          sx = firstValue.getDoubleValue();
          if (transformType != TransformOperation::ScaleX) {
            if (transformValue->length() > 1) {
              const CSSPrimitiveValue& secondValue =
                  toCSSPrimitiveValue(transformValue->item(1));
              sy = secondValue.getDoubleValue();
            } else {
              sy = sx;
            }
          }
        }
        operations.operations().push_back(
            ScaleTransformOperation::create(sx, sy, 1.0, transformType));
        break;
      }
      case TransformOperation::ScaleZ:
      case TransformOperation::Scale3D: {
        double sx = 1.0;
        double sy = 1.0;
        double sz = 1.0;
        if (transformType == TransformOperation::ScaleZ) {
          sz = firstValue.getDoubleValue();
        } else {
          sx = firstValue.getDoubleValue();
          sy = toCSSPrimitiveValue(transformValue->item(1)).getDoubleValue();
          sz = toCSSPrimitiveValue(transformValue->item(2)).getDoubleValue();
        }
        operations.operations().push_back(
            ScaleTransformOperation::create(sx, sy, sz, transformType));
        break;
      }
      case TransformOperation::Translate:
      case TransformOperation::TranslateX:
      case TransformOperation::TranslateY: {
        Length tx = Length(0, Fixed);
        Length ty = Length(0, Fixed);
        if (transformType == TransformOperation::TranslateY)
          ty = convertToFloatLength(firstValue, conversionData);
        else {
          tx = convertToFloatLength(firstValue, conversionData);
          if (transformType != TransformOperation::TranslateX) {
            if (transformValue->length() > 1) {
              const CSSPrimitiveValue& secondValue =
                  toCSSPrimitiveValue(transformValue->item(1));
              ty = convertToFloatLength(secondValue, conversionData);
            }
          }
        }

        operations.operations().push_back(
            TranslateTransformOperation::create(tx, ty, 0, transformType));
        break;
      }
      case TransformOperation::TranslateZ:
      case TransformOperation::Translate3D: {
        Length tx = Length(0, Fixed);
        Length ty = Length(0, Fixed);
        double tz = 0;
        if (transformType == TransformOperation::TranslateZ) {
          tz = firstValue.computeLength<double>(conversionData);
        } else {
          tx = convertToFloatLength(firstValue, conversionData);
          ty = convertToFloatLength(
              toCSSPrimitiveValue(transformValue->item(1)), conversionData);
          tz = toCSSPrimitiveValue(transformValue->item(2))
                   .computeLength<double>(conversionData);
        }

        operations.operations().push_back(
            TranslateTransformOperation::create(tx, ty, tz, transformType));
        break;
      }
      case TransformOperation::RotateX:
      case TransformOperation::RotateY:
      case TransformOperation::RotateZ: {
        double angle = firstValue.computeDegrees();
        if (transformValue->length() == 1) {
          double x = transformType == TransformOperation::RotateX;
          double y = transformType == TransformOperation::RotateY;
          double z = transformType == TransformOperation::RotateZ;
          operations.operations().push_back(
              RotateTransformOperation::create(x, y, z, angle, transformType));
        } else {
          // For SVG 'transform' attributes we generate 3-argument rotate()
          // functions.
          DCHECK_EQ(transformValue->length(), 3u);
          const CSSPrimitiveValue& secondValue =
              toCSSPrimitiveValue(transformValue->item(1));
          const CSSPrimitiveValue& thirdValue =
              toCSSPrimitiveValue(transformValue->item(2));
          operations.operations().push_back(
              RotateAroundOriginTransformOperation::create(
                  angle, secondValue.computeLength<double>(conversionData),
                  thirdValue.computeLength<double>(conversionData)));
        }
        break;
      }
      case TransformOperation::Rotate3D: {
        const CSSPrimitiveValue& secondValue =
            toCSSPrimitiveValue(transformValue->item(1));
        const CSSPrimitiveValue& thirdValue =
            toCSSPrimitiveValue(transformValue->item(2));
        const CSSPrimitiveValue& fourthValue =
            toCSSPrimitiveValue(transformValue->item(3));
        double x = firstValue.getDoubleValue();
        double y = secondValue.getDoubleValue();
        double z = thirdValue.getDoubleValue();
        double angle = fourthValue.computeDegrees();
        operations.operations().push_back(
            RotateTransformOperation::create(x, y, z, angle, transformType));
        break;
      }
      case TransformOperation::Skew:
      case TransformOperation::SkewX:
      case TransformOperation::SkewY: {
        double angleX = 0;
        double angleY = 0;
        double angle = firstValue.computeDegrees();
        if (transformType == TransformOperation::SkewY)
          angleY = angle;
        else {
          angleX = angle;
          if (transformType == TransformOperation::Skew) {
            if (transformValue->length() > 1) {
              const CSSPrimitiveValue& secondValue =
                  toCSSPrimitiveValue(transformValue->item(1));
              angleY = secondValue.computeDegrees();
            }
          }
        }
        operations.operations().push_back(
            SkewTransformOperation::create(angleX, angleY, transformType));
        break;
      }
      case TransformOperation::Matrix: {
        double a = firstValue.getDoubleValue();
        double b =
            toCSSPrimitiveValue(transformValue->item(1)).getDoubleValue();
        double c =
            toCSSPrimitiveValue(transformValue->item(2)).getDoubleValue();
        double d =
            toCSSPrimitiveValue(transformValue->item(3)).getDoubleValue();
        double e =
            zoomFactor *
            toCSSPrimitiveValue(transformValue->item(4)).getDoubleValue();
        double f =
            zoomFactor *
            toCSSPrimitiveValue(transformValue->item(5)).getDoubleValue();
        operations.operations().push_back(
            MatrixTransformOperation::create(a, b, c, d, e, f));
        break;
      }
      case TransformOperation::Matrix3D: {
        TransformationMatrix matrix(
            toCSSPrimitiveValue(transformValue->item(0)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(1)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(2)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(3)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(4)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(5)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(6)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(7)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(8)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(9)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(10)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(11)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(12)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(13)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(14)).getDoubleValue(),
            toCSSPrimitiveValue(transformValue->item(15)).getDoubleValue());
        matrix.zoom(zoomFactor);
        operations.operations().push_back(
            Matrix3DTransformOperation::create(matrix));
        break;
      }
      case TransformOperation::Perspective: {
        double p = firstValue.computeLength<double>(conversionData);
        ASSERT(p >= 0);
        operations.operations().push_back(
            PerspectiveTransformOperation::create(p));
        break;
      }
      default:
        ASSERT_NOT_REACHED();
        break;
    }
  }
  return operations;
}

}  // namespace blink
