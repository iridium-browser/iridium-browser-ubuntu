# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import subprocess


def CheckChange(input, output):
    # There apparently is no way to wrap strings in blueprints, so ignore long
    # lines in them.
    long_line_sources = lambda x: input.FilterSourceFile(
            x, white_list=".*", black_list=['Android[.]bp'])

    results = []
    results += input.canned_checks.CheckDoNotSubmit(input, output)
    results += input.canned_checks.CheckChangeHasNoTabs(input, output)
    results += input.canned_checks.CheckLongLines(
            input, output, 80, source_file_filter=long_line_sources)
    results += input.canned_checks.CheckPatchFormatted(input, output)
    results += input.canned_checks.CheckGNFormatted(input, output)
    results += CheckAndroidBlueprint(input, output)
    return results


def CheckChangeOnUpload(input_api, output_api):
    return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return CheckChange(input_api, output_api)


def CheckAndroidBlueprint(input_api, output_api):
    # If no GN files were modified, bail out.
    build_file_filter = lambda x: input_api.FilterSourceFile(
          x,
          white_list=('.*BUILD[.]gn$', '.*[.]gni$', 'tools/gen_android_bp'))
    if not input_api.AffectedSourceFiles(build_file_filter):
        return []

    with open('Android.bp') as f:
        current_blueprint = f.read()
    new_blueprint = subprocess.check_output(
        ['tools/gen_android_bp', '--output', '/dev/stdout'])

    if current_blueprint != new_blueprint:
        return [
            output_api.PresubmitError(
                'Android.bp is out of date. Please run tools/gen_android_bp '
                'to update it.')
        ]
    return []
