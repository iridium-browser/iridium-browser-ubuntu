#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a static catalog manifest to be loaded at runtime. This includes
embedded service manifests for every supported service, as well as information
indicating how to start each service."""

import argparse
import json
import os.path
import sys

eater_relative = "../../../../../tools/json_comment_eater"
eater_relative = os.path.join(os.path.abspath(__file__), eater_relative)
sys.path.insert(0, os.path.normpath(eater_relative))
try:
  import json_comment_eater
finally:
  sys.path.pop(0)


def ParseJSONFile(filename):
  with open(filename) as json_file:
    try:
      return json.loads(json_comment_eater.Nom(json_file.read()))
    except ValueError as e:
      print "%s is not a valid JSON document" % filename
      raise e


def ParseManifest(filename):
  manifest = ParseJSONFile(filename)
  if "name" not in manifest:
    raise Exception("Manifest %s missing \"name\" key." % filename)
  return manifest["name"], manifest


def AddServiceEntryToCatalog(services, name, entry):
  if name in services:
    raise Exception("Duplicate service entry for %s" % name)
  services[name] = entry


def main():
  parser = argparse.ArgumentParser(
      description="Generates a Service Manager catalog manifest.")
  parser.add_argument("--output")
  parser.add_argument("--packages-dir")
  parser.add_argument("--pretty", action="store_true")
  parser.add_argument("--embedded-services", nargs="+",
                      dest="embedded_services", default=[])
  parser.add_argument("--standalone-services", nargs="+",
                      dest="standalone_services", default=[])
  parser.add_argument("--include-catalogs", nargs="+", dest="included_catalogs",
                      default=[])
  parser.add_argument("--override-service-executables", nargs="+",
                      dest="executable_override_specs", default=[])
  args, _ = parser.parse_known_args()

  if args.output is None or args.packages_dir is None:
    raise Exception("--output and --packages-dir required")

  services = {}
  for subcatalog_path in args.included_catalogs:
    subcatalog = ParseJSONFile(subcatalog_path)
    for name, entry in subcatalog["services"].iteritems():
      AddServiceEntryToCatalog(services, name, entry)

  executable_overrides = {}
  for override_spec in args.executable_override_specs:
    service_name, exe_path = override_spec.split(":", 1)
    executable_overrides[service_name] = exe_path

  for name in args.embedded_services:
    manifest_path = os.path.join(args.packages_dir, name, "manifest.json")
    service_name, manifest = ParseManifest(manifest_path)
    entry = { "embedded": True, "manifest": manifest }
    AddServiceEntryToCatalog(services, service_name, entry)

  for name in args.standalone_services:
    manifest_path = os.path.join(args.packages_dir, name, "manifest.json")
    service_name, manifest = ParseManifest(manifest_path)
    entry = { "embedded": False, "manifest": ParseJSONFile(manifest_path) }
    if name in executable_overrides:
      entry["executable"] = executable_overrides[name]
    AddServiceEntryToCatalog(services, service_name, entry)

  catalog = { "services": services }
  with open(args.output, 'w') as output_file:
    json.dump(catalog, output_file, indent=2 if args.pretty else -1)

  return 0

if __name__ == "__main__":
  sys.exit(main())
