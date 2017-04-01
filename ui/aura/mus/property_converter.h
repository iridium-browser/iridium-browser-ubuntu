// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_PROPERTY_CONVERTER_H_
#define UI_AURA_MUS_PROPERTY_CONVERTER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

namespace gfx {
class Rect;
class Size;
}

namespace aura {

// PropertyConverter is used to convert Window properties for transport to the
// mus window server and back. Any time a property changes from one side it is
// mapped to the other using this class. Not all Window properties need to map
// to server properties, and similarly not all transport properties need map to
// Window properties.
class AURA_EXPORT PropertyConverter {
 public:
  // All primitive values are stored using this type.
  using PrimitiveType = int64_t;

  PropertyConverter();
  ~PropertyConverter();

  // Maps a property on the Window to a property pushed to the server. Return
  // true if the property should be sent to the server, false if the property
  // is only used locally.
  bool ConvertPropertyForTransport(
      Window* window,
      const void* key,
      std::string* transport_name,
      std::unique_ptr<std::vector<uint8_t>>* transport_value);

  // Returns the transport name for a Window property.
  std::string GetTransportNameForPropertyKey(const void* key);

  // Applies a value from the server to |window|. |transport_name| is the
  // name of the property and |transport_data| the value. |transport_data| may
  // be null.
  void SetPropertyFromTransportValue(
      Window* window,
      const std::string& transport_name,
      const std::vector<uint8_t>* transport_data);

  // Returns the value for a particular transport value. All primitives are
  // serialized as a PrimitiveType, so this function may be used for any
  // primitive. Returns true on success and sets |value| accordingly. A return
  // value of false indicates the value isn't known or the property type isn't
  // primitive.
  bool GetPropertyValueFromTransportValue(
      const std::string& transport_name,
      const std::vector<uint8_t>& transport_data,
      PrimitiveType* value);

  // Register a property to support conversion between mus and aura.
  template<typename T>
  void RegisterProperty(const WindowProperty<T>* property,
                        const char* transport_name) {
    PrimitiveProperty primitive_property;
    primitive_property.property_name = property->name;
    primitive_property.transport_name = transport_name;
    primitive_property.default_value = property->default_value;
    primitive_properties_[property] = primitive_property;
  }

  // Specializations for properties to pointer types supporting mojo conversion.
  void RegisterProperty(const WindowProperty<gfx::ImageSkia*>* property,
                        const char* transport_name);
  void RegisterProperty(const WindowProperty<gfx::Rect*>* property,
                        const char* transport_name);
  void RegisterProperty(const WindowProperty<gfx::Size*>* property,
                        const char* transport_name);
  void RegisterProperty(const WindowProperty<std::string*>* property,
                        const char* transport_name);
  void RegisterProperty(const WindowProperty<base::string16*>* property,
                        const char* transport_name);

 private:
  // Contains data needed to store and convert primitive-type properties.
  struct PrimitiveProperty {
    // The aura::WindowProperty::name used for storage.
    const char* property_name = nullptr;
    // The mus property name used for transport.
    const char* transport_name = nullptr;
    // The aura::WindowProperty::default_value stored using PrimitiveType.
    PrimitiveType default_value = 0;
  };

  // A map of aura::WindowProperty<T> to PrimitiveProperty structs.
  // This supports the internal codepaths for primitive types, eg. T=bool.
  std::map<const void*, PrimitiveProperty> primitive_properties_;

  // Maps of aura::WindowProperty<T> to their mus property names.
  // This supports types that can be serialized for Mojo, eg. T=std::string*.
  std::map<const WindowProperty<gfx::ImageSkia*>*, const char*>
      image_properties_;
  std::map<const WindowProperty<gfx::Rect*>*, const char*> rect_properties_;
  std::map<const WindowProperty<gfx::Size*>*, const char*> size_properties_;
  std::map<const WindowProperty<std::string*>*, const char*> string_properties_;
  std::map<const WindowProperty<base::string16*>*, const char*>
      string16_properties_;

  DISALLOW_COPY_AND_ASSIGN(PropertyConverter);
};

}  // namespace aura

#endif  // UI_AURA_MUS_PROPERTY_CONVERTER_H_
