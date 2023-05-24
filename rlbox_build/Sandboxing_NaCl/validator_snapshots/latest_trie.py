# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library to find latest trie paths."""

import os


def LatestRagelTriePath(top_level_dir, bitness):
  """Computes the pathname of the latest ragel trie.

  Args:
    top_level_dir: the directory where the 32 bit and 64 bit
                   ragel trie directories live.
    bitness: an integer for the x86 architecture. Must be 32 or 64.
  Returns:
    path of the latest trie.
  Raises:
    AssertionError: if the bitness is invalid or no tries found.
  """
  if bitness not in (32, 64):
    raise AssertionError('invalid bitness: ', bitness)
  ragel_dirs = {32: 'ragel_trie_x86_32', 64: 'ragel_trie_x86_64'}
  tries = os.listdir(os.path.join(top_level_dir, ragel_dirs[bitness]))
  if not tries:
    raise AssertionError('no tries found: ', top_level_dir, bitness)
  # For now, we assume that all of the tries start with the date,
  # so we pick the maximum.
  return os.path.join(top_level_dir, ragel_dirs[bitness], max(tries))

