#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pretty-prints the histograms.xml file, alphabetizing tags, wrapping text
at 80 chars, enforcing standard attribute ordering, and standardizing
indentation.

This is quite a bit more complicated than just calling tree.toprettyxml();
we need additional customization, like special attribute ordering in tags
and wrapping text nodes, so we implement our own full custom XML pretty-printer.
"""

from __future__ import with_statement

import logging
import os
import shutil
import sys
import xml.dom.minidom

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import diff_util
import presubmit_util

import print_style

# Tags whose children we want to alphabetize. The key is the parent tag name,
# and the value is a pair of the tag name of the children we want to sort,
# and a key function that maps each child node to the desired sort key.
ALPHABETIZATION_RULES = {
  'histograms': ('histogram', lambda n: n.attributes['name'].value.lower()),
  'enums': ('enum', lambda n: n.attributes['name'].value.lower()),
  'enum': ('int', lambda n: int(n.attributes['value'].value)),
  'histogram_suffixes_list': (
      'histogram_suffixes', lambda n: n.attributes['name'].value.lower()),
  'histogram_suffixes': ('affected-histogram',
                         lambda n: n.attributes['name'].value.lower()),
}


class Error(Exception):
  pass


def unsafeAppendChild(parent, child):
  """Append child to parent's list of children, ignoring the possibility that it
  is already in another node's childNodes list.  Requires that the previous
  parent of child is discarded (to avoid non-tree DOM graphs).
  This can provide a significant speedup as O(n^2) operations are removed (in
  particular, each child insertion avoids the need to traverse the old parent's
  entire list of children)."""
  child.parentNode = None
  parent.appendChild(child)
  child.parentNode = parent


def TransformByAlphabetizing(node):
  """Transform the given XML by alphabetizing specific node types according to
  the rules in ALPHABETIZATION_RULES.

  Args:
    node: The minidom node to transform.

  Returns:
    The minidom node, with children appropriately alphabetized. Note that the
    transformation is done in-place, i.e. the original minidom tree is modified
    directly.
  """
  if node.nodeType != xml.dom.minidom.Node.ELEMENT_NODE:
    for c in node.childNodes:
      TransformByAlphabetizing(c)
    return node

  # Element node with a tag name that we alphabetize the children of?
  if node.tagName in ALPHABETIZATION_RULES:
    # Put subnodes in a list of node,key pairs to allow for custom sorting.
    subtag, key_function = ALPHABETIZATION_RULES[node.tagName]
    subnodes = []
    sort_key = -1
    pending_node_indices = []
    for c in node.childNodes:
      if (c.nodeType == xml.dom.minidom.Node.ELEMENT_NODE and
          c.tagName == subtag):
        sort_key = key_function(c)
        # Replace sort keys for delayed nodes.
        for idx in pending_node_indices:
          subnodes[idx][1] = sort_key
        pending_node_indices = []
      else:
        # Subnodes that we don't want to rearrange use the next node's key,
        # so they stay in the same relative position.
        # Therefore we delay setting key until the next node is found.
        pending_node_indices.append(len(subnodes))

      subnodes.append( [c, sort_key] )

    # Use last sort key for trailing unknown nodes.
    for idx in pending_node_indices:
      subnodes[idx][1] = sort_key

    # Sort the subnode list.
    subnodes.sort(key=lambda pair: pair[1])

    # Re-add the subnodes, transforming each recursively.
    while node.firstChild:
      node.removeChild(node.firstChild)
    for (c, _) in subnodes:
      unsafeAppendChild(node, TransformByAlphabetizing(c))
    return node

  # Recursively handle other element nodes and other node types.
  for c in node.childNodes:
    TransformByAlphabetizing(c)
  return node


def PrettyPrint(raw_xml):
  """Pretty-print the given XML.

  Args:
    raw_xml: The contents of the histograms XML file, as a string.

  Returns:
    The pretty-printed version.
  """
  tree = xml.dom.minidom.parseString(raw_xml)
  tree = TransformByAlphabetizing(tree)
  return print_style.GetPrintStyle().PrettyPrintNode(tree)


def main():
  presubmit_util.DoPresubmitMain(sys.argv, 'histograms.xml',
                                 'histograms.before.pretty-print.xml',
                                 'pretty_print.py', PrettyPrint)

if __name__ == '__main__':
  main()
