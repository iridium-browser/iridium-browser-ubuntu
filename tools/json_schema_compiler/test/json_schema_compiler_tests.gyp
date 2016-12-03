# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //tools/json_schema_compiler:test
      'target_name': 'json_schema_compiler_tests',
      'type': 'static_library',
      'variables': {
        'chromium_code': 1,
        'schema_files': [
          'additional_properties.json',
          'any.json',
          'arrays.json',
          'callbacks.json',
          'choices.json',
          'crossref.json',
          'enums.json',
          'functions_as_parameters.json',
          'functions_on_types.json',
          'idl_basics.idl',
          'idl_other_namespace.idl',
          'idl_other_namespace_sub_namespace.idl',
          'idl_object_types.idl',
          'objects.json',
          'objects_movable.idl',
          'objects_movable_json.json',
          'simple_api.json',
          'error_generation.json'
        ],
        'cc_dir': 'tools/json_schema_compiler/test',
        'root_namespace': 'test::api::%(namespace)s',
      },
      'inputs': [
        '<@(schema_files)',
      ],
      'sources': [
        '<@(schema_files)',
        'test_util.cc',
        'test_util.h',
      ],
      'includes': ['../../../build/json_schema_compile.gypi'],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      'target_name': 'feature_compiler_tests',
      'type': 'static_library',
      'variables': {
        'feature_class': 'APIFeature',
        'provider_class': 'CompilerTestFeatureProvider',
        'out_dir': 'tools/json_schema_compiler/test',
        'out_base_filename': 'features_compiler_test',
        'in_files': [
          'tools/json_schema_compiler/test/features_test.json',
          'tools/json_schema_compiler/test/features_test2.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../json_features.gypi'],
    },
  ],
}
