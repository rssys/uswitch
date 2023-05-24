/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <sys/resource.h>
#include <string.h>
#include <sys/mman.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"

size_t g_prereserved_sandbox_size = 0;

void* g_manual_preserved_sandbox_memory = 0;
size_t g_manual_preserved_sandbox_size = 0;

void NaClCreateManualPrereservedSandboxMemory(void) {
  if (g_manual_preserved_sandbox_memory == 0) {
    int flags = MAP_NORESERVE | MAP_PRIVATE | MAP_ANONYMOUS;
    size_t size = ((unsigned int) 1) << 30;
    void* addr = mmap(0, size, PROT_NONE, flags, -1, 0);

    if (addr == MAP_FAILED || addr == 0) {
      abort();
    }

    g_manual_preserved_sandbox_memory = addr;
    g_manual_preserved_sandbox_size = size;
  }
}

int NaClFindManualPrereservedSandboxMemory(void **p, size_t num_bytes) {
  NaClLog(2, "NaClFindManualPrereservedSandboxMemory(, %#.8"NACL_PRIxPTR")\n",
          num_bytes);

  if (g_manual_preserved_sandbox_memory != 0) {
    if (num_bytes != g_manual_preserved_sandbox_size) {
      abort();
    }
    *p = g_manual_preserved_sandbox_memory;
    // mark it as used
    g_manual_preserved_sandbox_memory = 0;
     g_manual_preserved_sandbox_size = 0;
    return 1;
  }

  *p = 0;
  return 0;
}

/*
 * Find sandbox memory prereserved by the nacl_helper in chrome. The
 * nacl_helper, if present, reserves the bottom 1G of the address space
 * for use by Native Client.
 */
int NaClFindPrereservedSandboxMemory(void **p, size_t num_bytes) {
  NaClLog(2, "NaClFindPrereservedSandboxMemory(, %#.8"NACL_PRIxPTR")\n",
          num_bytes);

  *p = 0;
  return num_bytes == g_prereserved_sandbox_size;
}

/*
 * If we have a soft limit for RLIMIT_AS lower than the hard limit,
 * increase it to include enough for the untrusted address space.
 */
void NaClAddrSpaceBeforeAlloc(size_t untrusted_size) {
  struct rlimit lim;

  UNREFERENCED_PARAMETER(untrusted_size);

  if (getrlimit(RLIMIT_AS, &lim) < 0) {
    NaClLog(LOG_WARNING, "NaClAddrSpaceBeforeAlloc: getrlimit failed: %s",
            strerror(errno));
    return;
  }

  /*
   * We don't just increase it by untrusted_size, because we need a
   * contiguous piece of that size, and that may stretch the overall
   * address space we need to occupy well beyond the sum of the old
   * limit and untrusted_size.
   */
  if (lim.rlim_cur < lim.rlim_max) {
    NaClLog(LOG_INFO, "NaClAddrSpaceBeforeAlloc: "
            "raising RLIMIT_AS from %#lx to hard limit %#lx\n",
            (unsigned long) lim.rlim_cur,
            (unsigned long) lim.rlim_max);
    lim.rlim_cur = lim.rlim_max;
    if (setrlimit(RLIMIT_AS, &lim) < 0) {
      NaClLog(LOG_WARNING, "NaClAddrSpaceBeforeAlloc: setrlimit failed: %s",
              strerror(errno));
    }
  }
}
