#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import in_generator
import name_utilities


class CSSProperties(in_generator.Writer):
    defaults = {
        'alias_for': None,
        'descriptor_only': None,
        'runtime_flag': None,
        'longhands': '',
        'interpolable': False,
        'inherited': False,
        'independent': False,
        'font': False,
        'svg': False,
        'name_for_methods': None,
        'use_handlers_for': None,
        'getter': None,
        'setter': None,
        'initial': None,
        'type_name': None,
        'converter': None,
        'custom_all': False,
        'custom_initial': False,
        'custom_inherit': False,
        'custom_value': False,
        'builder_skip': False,
        'direction_aware': False,
        # Typed OM annotations.
        'typedom_types': [],
        'keywords': [],
        'supports_percentage': False,
        'supports_multiple': False,
    }

    valid_values = {
        'interpolable': (True, False),
        'inherited': (True, False),
        'independent': (True, False),
        'font': (True, False),
        'svg': (True, False),
        'custom_all': (True, False),
        'custom_initial': (True, False),
        'custom_inherit': (True, False),
        'custom_value': (True, False),
        'builder_skip': (True, False),
        'direction_aware': (True, False),
    }

    def __init__(self, file_paths):
        in_generator.Writer.__init__(self, file_paths)

        properties = self.in_file.name_dictionaries
        self._descriptors = [property for property in properties if property['descriptor_only']]

        self._aliases = [property for property in properties if property['alias_for']]
        properties = [property for property in properties if not property['alias_for']]

        # 0: CSSPropertyInvalid
        # 1: CSSPropertyApplyAtRule
        # 2: CSSPropertyVariable
        self._first_enum_value = 3

        # StylePropertyMetadata additionally assumes there are under 1024 properties.
        assert self._first_enum_value + len(properties) < 512, 'Property aliasing expects there are under 512 properties.'

        for offset, property in enumerate(properties):
            property['property_id'] = name_utilities.enum_for_css_property(property['name'])
            property['upper_camel_name'] = name_utilities.camel_case(property['name'])
            property['lower_camel_name'] = name_utilities.lower_first(property['upper_camel_name'])
            property['enum_value'] = self._first_enum_value + offset
            property['is_internal'] = property['name'].startswith('-internal-')

        self._properties_including_aliases = properties
        self._properties = {property['property_id']: property for property in properties}

        # The generated code will only work with at most one alias per property
        assert len({property['alias_for'] for property in self._aliases}) == len(self._aliases)

        for property in self._aliases:
            property['property_id'] = name_utilities.enum_for_css_property_alias(property['name'])
            aliased_property = self._properties[name_utilities.enum_for_css_property(property['alias_for'])]
            property['enum_value'] = aliased_property['enum_value'] + 512
        self._properties_including_aliases += self._aliases
