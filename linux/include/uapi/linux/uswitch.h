/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_USWITCH_H
#define _UAPI_LINUX_USWITCH_H

#define USWITCH_MAX_CID	(~0U)

#define USWITCH_CLONE_FD_FIELD		0x3
#define USWITCH_CLONE_FD_COPY		0x0
#define USWITCH_CLONE_FD_SHARE		0x1
#define USWITCH_CLONE_FD_NEW		0x2
#define USWITCH_CLONE_FS_FIELD		0x4
#define USWITCH_CLONE_FS_COPY		0x0
#define USWITCH_CLONE_FS_SHARE		0x4

#define USWITCH_CNTL_SWITCH				0x0
#define USWITCH_CNTL_ENABLE_DEFERRED_SWITCH		0x1
#define USWITCH_CNTL_DISABLE_DEFERRED_SWITCH		0x2
#define	USWITCH_CNTL_GET_CID				0x3
#define USWITCH_CNTL_DESTROY_CONTEXT			0x4
#define USWITCH_CNTL_DUP_FILE				0x5
#define USWITCH_CNTL_SWITCH_PAGE_TABLE			0x6
#define USWITCH_CNTL_DUP_FILE2				0x7

#define USWITCH_INIT_FLAGS_FIELDS			0x3
#define USWITCH_ISOLATE_NAMESPACES			0x1
#define USWITCH_ISOLATE_CREDENTIALS			0x2

#define USWITCH_SIGNAL_STACK_USE_THREAD		0x0
#define USWITCH_SIGNAL_STACK_USE_SHARED		0x1
#define USWITCH_SIGNAL_STACK_USE_RSP		0x2

#define USWITCH_FORK_DISABLE			0x0
#define USWITCH_FORK_CURRENT			0x1
#define USWITCH_FORK_CURRENT_AND_ANOTHER	0x2

struct uswitch_data {
	int shared_descriptor;
	int next_descriptor;
	int seccomp_descriptor;
	int next_seccomp_descriptor;
	unsigned long ss_sp;
	size_t ss_size;
	unsigned int ss_flags;
	unsigned int ss_control;
	int block_signals;
	int next_block_signals;
	int fork_descriptor;
	int fork_control;
};

#endif /* _UAPI_LINUX_USWITCH_H */
