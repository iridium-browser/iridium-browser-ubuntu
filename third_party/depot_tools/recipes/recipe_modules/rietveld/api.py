# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import urlparse

from recipe_engine import recipe_api


class RietveldApi(recipe_api.RecipeApi):
  def calculate_issue_root(self, extra_patch_project_roots=None):
    """Returns path where a patch should be applied to based on "patch_project".

    YOU SHOULD NOT USE THIS METHOD. Put this into gclient's config as
    patch_projects instead, and with luck you won't need to use
    calculate_patch_root from gclient api.
    TODO(tandrii): remove this method completely. See http://crbug.com/605563.

    Maps Rietveld's "patch_project" to a path of directories relative to
    api.gclient.c.solutions[0].name which describe where to place the patch.

    Args:
      extra_patch_project_roots: Dict mapping project names to relative roots.

    Returns:
      Relative path or empty string if patch_project is not set or path for a
      given is unknown.
    """
    # Property 'patch_project' is set by Rietveld, 'project' is set by git-try
    # when TRYSERVER_PROJECT is present in codereview.settings.
    patch_project = (self.m.properties.get('patch_project') or
                     self.m.properties.get('project'))

    # Please avoid adding projects into this hard-coded list unless your project
    # CLs are being run by multiple recipes. Instead pass patch_project_roots to
    # ensure_checkout.
    patch_project_roots = {
      'angle/angle': ['third_party', 'angle'],
      'blink': ['third_party', 'WebKit'],
      'v8': ['v8'],
      'luci-py': ['luci'],
      'recipes-py': ['recipes-py'],
    }

    # Make sure to update common projects (above) with extra projects (and not
    # vice versa, so that recipes can override default values if needed.
    if extra_patch_project_roots:
      patch_project_roots.update(extra_patch_project_roots)

    path_parts = patch_project_roots.get(patch_project)
    return self.m.path.join(*path_parts) if path_parts else ''

  def apply_issue(self, *root_pieces, **kwargs):
    """Call apply_issue from depot_tools.

    Args:
      root_pieces (strings): location where to run apply_issue, relative to the
        checkout root.
      authentication (string or None): authentication scheme to use. Can be None
        or 'oauth2'. See also apply_issue.py --help (-E and --no-auth options.)
    """
    # TODO(pgervais): replace *root_pieces by a single Path object.
    authentication = kwargs.get('authentication', None)
    rietveld_url = self.m.properties['rietveld']
    issue_number = self.m.properties['issue']

    if authentication == 'oauth2':
      try:
        build_path = self.m.path['build']
      except KeyError:
        raise self.m.step.StepFailure(
          'build path is not defined. This is typical for LUCI builds. '
          'LUCI does not support rietveld.apply_issue; use bot_update instead')
      step_result = self.m.python(
        'apply_issue',
        self.package_repo_resource('apply_issue.py'), [
          '-r', self.m.path['checkout'].join(*root_pieces),
          '-i', issue_number,
          '-p', self.m.properties['patchset'],
          '-s', rietveld_url,
          '-E', build_path.join('site_config', '.rietveld_client_email'),
          '-k', build_path.join('site_config', '.rietveld_secret_key'),
        ],
      )
    else:
      step_result = self.m.python(
        'apply_issue',
        self.package_repo_resource('apply_issue.py'), [
          '-r', self.m.path['checkout'].join(*root_pieces),
          '-i', issue_number,
          '-p', self.m.properties['patchset'],
          '-s', rietveld_url,
          '--no-auth'],
        )
    step_result.presentation.links['Applied issue %s' % issue_number] = (
      urlparse.urljoin(rietveld_url, str(issue_number)))

