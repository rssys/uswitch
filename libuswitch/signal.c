#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>
#include <asm/signal.h>
#include <asm/sigcontext.h>
#include <asm/ucontext.h>
#include <asm/siginfo.h>

int get_fpstate_size(uintptr_t ptr, uint32_t *size, uint64_t *xfeatures) {
    struct _xstate *xstate = (struct _xstate *)ptr;
    struct _fpstate *fpstate = (struct _fpstate *)ptr;
    if (fpstate->sw_reserved.magic1 == FP_XSTATE_MAGIC1) {
        *size = fpstate->sw_reserved.extended_size;
        *xfeatures = xstate->xstate_hdr.xfeatures;
        return 1;
    } else {
        *size = sizeof(struct _fpstate);
        return 0;
    }
}

int sanitize_fpstate(uintptr_t ptr, size_t size) {
    struct _xstate *xstate = (struct _xstate *)ptr;
    struct _fpstate *fpstate = (struct _fpstate *)ptr;
    if (fpstate->sw_reserved.magic1 != FP_XSTATE_MAGIC1) {
        return 0;
    }
    uint32_t s = fpstate->sw_reserved.extended_size;
    if (s != size) {
        return 0;
    }
    uintptr_t pmagic2 = ptr + s - FP_XSTATE_MAGIC2_SIZE;
    if (*(uint32_t *)pmagic2 != FP_XSTATE_MAGIC2) {
        return 0;
    }
    uint64_t xfeatures = xstate->xstate_hdr.xfeatures;
    if (!(xfeatures & (1lu << 9))) {
        return 0;
    }
    return 1;
}