/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_USWITCH_H
#define _LINUX_USWITCH_H
#include <linux/idr.h>
#include <linux/seccomp.h>
#include <linux/refcount.h>
#include <linux/spinlock.h>
#include <linux/rwlock.h>
#include <uapi/linux/uswitch.h>

struct files_struct;
struct fs_struct;
struct nsproxy;
struct task_struct;
struct uswitch_context_struct {
	refcount_t refs;
	rwlock_t lock;
	struct files_struct *files;
	struct seccomp seccomp;
	struct fs_struct *fs;
	struct nsproxy *nsproxy;
	const struct cred *cred;
	const struct cred *real_cred;
};

struct uswitch_contexts_table_struct {
	refcount_t refs;
	spinlock_t lock;
	int flags;
	struct idr ctx_idr;
};

struct uswitch_contexts_struct
{
	struct uswitch_contexts_table_struct *public_table;
	int current_cid;
	int seccomp_cid;
	struct page *data_page;
	struct uswitch_data *kernel_data;
	struct uswitch_data *user_data;
	struct seccomp saved_seccomp;

	unsigned long ss_sp;
	size_t ss_size;
	unsigned int ss_flags;
	unsigned int ss_control;
};

int copy_uswitch(unsigned long clone_flags, struct task_struct *tsk);
void exit_uswitch(struct task_struct *tsk);
int uswitch_enter_syscall(unsigned long *work, bool *need_switch_seccomp);

#endif /* _LINUX_USWITCH_H */
