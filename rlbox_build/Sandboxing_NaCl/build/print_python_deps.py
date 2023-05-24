#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints all non-system dependencies for the given module.

The primary use-case for this script is to genererate the list of python modules
required for .isolate files.
"""

import argparse
import imp
import os
import pipes
import sys

# Don't use any helper modules, or else they will end up in the results.


_SRC_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))


def _ComputePythonDependencies():
  """Gets the paths of imported non-system python modules.

  A path is assumed to be a "system" import if it is outside of chromium's
  src/. The paths will be relative to the current directory.
  """
  module_paths = (m.__file__ for m in sys.modules.values()
                  if m and hasattr(m, '__file__'))

  src_paths = set()
  for path in module_paths:
    if path == __file__:
      continue
    path = os.path.abspath(path)
    if not path.startswith(_SRC_ROOT):
      continue

    if (path.endswith('.pyc')
        or (path.endswith('c') and not os.path.splitext(path)[1])):
      path = path[:-1]
    src_paths.add(path)

  return src_paths


def _NormalizeCommandLine(options):
  """Returns a string that when run from SRC_ROOT replicates the command."""
  args = ['build/print_python_deps.py']
  root = os.path.relpath(options.root, _SRC_ROOT)
  if root != '.':
    args.extend(('--root', root))
  if options.output:
    args.extend(('--output', os.path.relpath(options.output, _SRC_ROOT)))
  for whitelist in sorted(options.whitelists):
    args.extend(('--whitelist', os.path.relpath(whitelist, _SRC_ROOT)))
  args.append(os.path.relpath(options.module, _SRC_ROOT))
  return ' '.join(pipes.quote(x) for x in args)


def _FindPythonInDirectory(directory):
  """Returns an iterable of all non-test python files in the given directory."""
  files = []
  for root, _dirnames, filenames in os.walk(directory):
    for filename in filenames:
      if filename.endswith('.py') and not filename.endswith('_test.py'):
        yield os.path.join(root, filename)


def main():
  parser = argparse.ArgumentParser(
      description='Prints all non-system dependencies for the given module.')
  parser.add_argument('module',
                      help='The python module to analyze.')
  parser.add_argument('--root', default='.',
                      help='Directory to make paths relative to.')
  parser.add_argument('--output',
                      help='Write output to a file rather than stdout.')
  parser.add_argument('--whitelist', default=[], action='append',
                      dest='whitelists',
                      help='Recursively include all non-test python files '
                      'within this directory. May be specified multiple times.')
  options = parser.parse_args()
  # Replace the path entry for print_python_deps.py with the one for the given
  # module.
  sys.path[0] = os.path.dirname(options.module)
  imp.load_source('NAME', options.module)

  paths_set = _ComputePythonDependencies()
  for path in options.whitelists:
    paths_set.update(os.path.abspath(p) for p in _FindPythonInDirectory(path))

  paths = [os.path.relpath(p, options.root) for p in paths_set]

  normalized_cmdline = _NormalizeCommandLine(options)
  out = open(options.output, 'w') if options.output else sys.stdout
  with out:
    out.write('# Generated by running:\n')
    out.write('#   %s\n' % normalized_cmdline)
    for path in sorted(paths):
      out.write(path + '\n')


if __name__ == '__main__':
  sys.exit(main())
