/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_NACL_NACL_STARTUP_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_NACL_NACL_STARTUP_H_ 1

#include "native_client/src/include/elf32.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The true entry point for untrusted code is called with the normal C ABI,
 * taking one argument.  This is a pointer to stack space containing these
 * words:
 *      [0]             cleanup function pointer (always NULL in actual startup)
 *      [1]             envc, count of envp[] pointers
 *      [2]             argc, count of argv[] pointers
 *      [3]             argv[0..argc] pointers, argv[argc] being NULL
 *      [3+argc+1]      envp[0..envc] pointers, envp[envc] being NULL
 *      [3+argc+envc+2] auxv[] pairs
 */

enum NaClStartupInfoIndex {
  NACL_STARTUP_FINI,  /* Cleanup function pointer for dynamic linking.  */
  NACL_STARTUP_ENVC,  /* Count of envp[] pointers.  */
  NACL_STARTUP_ARGC,  /* Count of argv[] pointers.  */
  NACL_STARTUP_ARGV   /* argv[0] pointer.  */
};

typedef void (*nacl_startup_fini_func_t)(void);

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
  #define pointerType uint64_t
#else
  #define pointerType uint32_t
#endif

/*
 * Return the dynamic linker finalizer function.
 */
static inline __attribute__((unused))
nacl_startup_fini_func_t nacl_startup_fini(const pointerType info[]) {
  uintptr_t val = info[NACL_STARTUP_FINI];
  return (void (*)(void)) val;
}

/*
 * Return the count of argument strings.
 */
static inline __attribute__((unused))
int nacl_startup_argc(const pointerType info[]) {
  return info[NACL_STARTUP_ARGC];
}

/*
 * Return the vector of argument strings.
 */
static inline __attribute__((unused))
char **nacl_startup_argv(const pointerType info[]) {
  return (char **) &info[NACL_STARTUP_ARGV];
}

/*
 * Return the count of environment strings.
 */
static inline __attribute__((unused))
int nacl_startup_envc(const pointerType info[]) {
  return info[NACL_STARTUP_ENVC];
}

/*
 * Return the vector of environment strings.
 */
static inline __attribute__((unused))
char **nacl_startup_envp(const pointerType info[]) {
  return &nacl_startup_argv(info)[nacl_startup_argc(info) + 1];
}

/*
 * Return the vector of auxiliary data items.
 */
static inline __attribute__((unused))
Elf32_auxv_t_corr *nacl_startup_auxv(const pointerType info[]) {
  char **envend = &nacl_startup_envp(info)[nacl_startup_envc(info) + 1];
  return (Elf32_auxv_t_corr *) envend;
}

/*
 * The main entry point.
 */
extern void _start(pointerType info[]);

#ifdef __cplusplus
}
#endif

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_NACL_NACL_STARTUP_H_ */
