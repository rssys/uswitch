#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/syscalls.h>
#include <linux/fdtable.h>
#include <linux/sched.h>
#include <linux/uswitch.h>
#include <linux/seccomp.h>
#include <linux/idr.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/nsproxy.h>
#include <linux/fs_struct.h>
#include <linux/init_task.h>
#include <uapi/linux/uswitch.h>

static int do_uswitch_init(struct uswitch_data **data, int flags);
static int do_uswitch_clone(int flags);
static int do_uswitch_switch(bool *need_switch_seccomp);
static int do_uswitch_destroy(int cid);
static int do_uswitch_dup_file(int cid, unsigned int fd);
static void switch_page_table(void);
static struct files_struct *get_task_files(struct task_struct *tsk);
static struct fs_struct *get_task_fs(struct task_struct *tsk);
static struct nsproxy *get_task_nsproxy(struct task_struct *tsk);
static struct uswitch_context_struct *get_uswitch_ctx(struct uswitch_contexts_table_struct *table, int cid);
static void put_uswitch_ctx(struct uswitch_context_struct *ctx);
static void put_fs(struct fs_struct *fs);
static int validate_init_flags(int flags);

SYSCALL_DEFINE2(uswitch_init, struct uswitch_data **, data, int, flags)
{
	int res = validate_init_flags(flags);
	if (res < 0)
		return res;
	return do_uswitch_init(data, flags);
}

SYSCALL_DEFINE1(uswitch_clone, int, flags)
{
	return do_uswitch_clone(flags);
}

SYSCALL_DEFINE3(uswitch_cntl, int, cmd, long, arg1, long, arg2)
{
	struct task_struct *tsk = get_current();
	struct uswitch_contexts_struct *ctxs = tsk->uswitch_contexts;
	if (!ctxs)
		return -EINVAL;
	if (cmd == USWITCH_CNTL_SWITCH)
		return do_uswitch_switch(NULL);
	else if (cmd == USWITCH_CNTL_ENABLE_DEFERRED_SWITCH) {
		set_task_syscall_work(tsk, USWITCH);
		return 0;
	} else if (cmd == USWITCH_CNTL_DISABLE_DEFERRED_SWITCH) {
		clear_task_syscall_work(tsk, USWITCH);
		return 0;
	} else if (cmd == USWITCH_CNTL_GET_CID)
		return put_user(ctxs->user_data, (struct uswitch_data **)arg1);
	else if (cmd == USWITCH_CNTL_DESTROY_CONTEXT)
		return do_uswitch_destroy((int)arg1);
	else if (cmd == USWITCH_CNTL_DUP_FILE)
		return do_uswitch_dup_file((int)arg1, (unsigned int)arg2);
	else if (cmd == USWITCH_CNTL_SWITCH_PAGE_TABLE) {
		// for comparing performance with lwC
		switch_page_table();
		return do_uswitch_switch(NULL);
	}
	return -EINVAL;
}

struct files_struct *get_task_files(struct task_struct *tsk)
{
	struct files_struct *files;
	task_lock(tsk);
	files = tsk->files;
	if (!files) {
		task_unlock(tsk);
		return NULL;
	}
	atomic_inc(&tsk->files->count);
	task_unlock(tsk);
	return files;
}

struct fs_struct *get_task_fs(struct task_struct *tsk)
{
	struct fs_struct *fs;
	task_lock(tsk);
	fs = tsk->fs;
	if (!fs) {
		task_unlock(tsk);
		return NULL;
	}
	spin_lock(&fs->lock);
	++fs->users;
	spin_unlock(&fs->lock);
	task_unlock(tsk);
	return fs;
}

struct nsproxy *get_task_nsproxy(struct task_struct *tsk)
{
	struct nsproxy *ns;
	task_lock(tsk);
	ns = tsk->nsproxy;
	if (!ns) {
		task_unlock(tsk);
		return NULL;
	}
	get_nsproxy(ns);
	task_unlock(tsk);
	return ns;
}

struct uswitch_context_struct *get_uswitch_ctx(struct uswitch_contexts_table_struct *table, int cid)
{
	struct uswitch_context_struct *ctx = (struct uswitch_context_struct *)idr_find(&table->ctx_idr, cid);
	if (!ctx)
		return NULL;
	refcount_inc(&ctx->refs);
	return ctx;
}

void put_uswitch_ctx(struct uswitch_context_struct *ctx)
{
	if (ctx && refcount_dec_and_test(&ctx->refs)) {
		if (ctx->files)
			put_files_struct(ctx->files);
		if (ctx->seccomp.filter)
			__seccomp_filter_release(ctx->seccomp.filter);
			//__put_seccomp_filter(ctx->seccomp.filter);
		if (ctx->fs)
			put_fs(ctx->fs);
		if (ctx->nsproxy)
			put_nsproxy(ctx->nsproxy);
		if (ctx->cred)
			put_cred(ctx->cred);
		if (ctx->real_cred)
			put_cred(ctx->real_cred);
		kfree(ctx);
	}
}

void put_fs(struct fs_struct *fs) {
	int kill;
	spin_lock(&fs->lock);
	kill = !--fs->users;
	spin_unlock(&fs->lock);
	if (kill)
		free_fs_struct(fs);
}

static int copy_uswitch_thread(struct task_struct *tsk) {
	struct uswitch_contexts_struct *ctxs = NULL, *current_ctxs = NULL;
	struct uswitch_contexts_table_struct *public_table = NULL;
	struct uswitch_context_struct *ctx = NULL;
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *vma;
	struct page *data_page;
	struct uswitch_data *kernel_data;
	struct uswitch_data *user_data;
	int ret = 0;
	int current_cid;
	unsigned long user_addr;
	unsigned long pfn;

	current_ctxs = current->uswitch_contexts;
	data_page = alloc_page(GFP_USER);
	pfn = page_to_pfn(data_page);
	ctxs = kcalloc(1, sizeof(struct uswitch_contexts_struct), GFP_KERNEL);
	if (!data_page || !ctxs) {
		ret = -ENOMEM;
		goto fail_malloc;
	}
	kernel_data = page_address(data_page);
	public_table = current_ctxs->public_table;
	current_cid = current_ctxs->current_cid;
	ctxs->public_table = public_table;
	ctxs->current_cid = current_cid;
	ctxs->data_page = data_page;
	ctxs->kernel_data = kernel_data;
	memset(kernel_data, 0, PAGE_SIZE);
	kernel_data->shared_descriptor = 0;
	kernel_data->next_descriptor = -1;
	kernel_data->seccomp_descriptor = -1;
	kernel_data->next_seccomp_descriptor = -2;
	kernel_data->ss_control = USWITCH_SIGNAL_STACK_USE_THREAD;
	kernel_data->block_signals = 0;
	kernel_data->next_block_signals = -1;
	kernel_data->fork_descriptor = 0;
	kernel_data->fork_control = USWITCH_FORK_DISABLE;
	refcount_inc(&public_table->refs);

	rcu_read_lock();
	ctx = (struct uswitch_context_struct *)idr_find(&public_table->ctx_idr, current_cid);
	if (!ctx) {
		ret = -EINVAL;
		rcu_read_unlock();
		goto fail_set_ref;
	}
	rcu_read_unlock();

	down_write(&mm->mmap_lock);
	user_addr = get_unmapped_area(NULL, 0, PAGE_SIZE, 0, 0);
	if (IS_ERR_VALUE(user_addr)) {
		goto fail_get_unmap;
	}
	user_data = (struct uswitch_data *)user_addr;
	ctxs->user_data = user_data;

	vma = vm_area_alloc(mm);
	if (!vma) {
		ret = -ENOMEM;
		goto fail_get_unmap;
	}
	vma->vm_pgoff = user_addr >> PAGE_SHIFT;
	vma->vm_start = user_addr;
	vma->vm_end = user_addr + PAGE_SIZE;
	vma->vm_flags = calc_vm_prot_bits(PROT_READ | PROT_WRITE, 0) | calc_vm_flag_bits(MAP_SHARED) |
			mm->def_flags | VM_MAYREAD | VM_MAYWRITE | VM_MAYSHARE | VM_SHARED | VM_DONTCOPY | VM_PFNMAP;
	if (!arch_validate_flags(vma->vm_flags)) {
		ret = -ENOMEM;
		goto fail_vma_alloc;
	}
	vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	
	if (remap_pfn_range(vma, user_addr, pfn, PAGE_SIZE, vma->vm_page_prot)) {
		ret = -ENOMEM;
		goto fail_vma;
	}
	if (insert_vm_struct(mm, vma)) {
		ret = -ENOMEM;
		goto fail_vma;
	}
	up_write(&mm->mmap_lock);

	tsk->uswitch_contexts = ctxs;

	return 0;
fail_vma:
fail_vma_alloc:
	vm_area_free(vma);
fail_get_unmap:
	up_write(&mm->mmap_lock);
fail_set_ref:
	refcount_dec(&public_table->refs);
fail_malloc:
	if(data_page)
		__free_page(data_page);
	kfree(ctxs);
	return ret;
}

static struct uswitch_context_struct *copy_uswitch_ctx(struct uswitch_contexts_table_struct *dst_table,
	struct uswitch_contexts_table_struct *src_table, int cid, int *ret)
{
	int res;
	struct uswitch_context_struct *src_ctx;
	struct uswitch_context_struct *dst_ctx = NULL;
	struct files_struct *files;
	struct seccomp seccomp;
	struct fs_struct *fs;
	struct nsproxy *nsproxy;
	const struct cred *cred;
	const struct cred *real_cred;
	src_ctx = get_uswitch_ctx(src_table, cid);
	if (!src_ctx) {
		*ret = -EINVAL;
		return NULL;
	}
	dst_ctx = kcalloc(1, sizeof(struct uswitch_context_struct), GFP_KERNEL);
	if (!dst_ctx) {
		*ret = -ENOMEM;
		goto fail_malloc;
	}

	res = idr_alloc(&dst_table->ctx_idr, (void *)dst_ctx, cid, cid + 1, GFP_KERNEL);
	if (res < 0) {
		*ret = res;
		goto fail_idr_alloc;
	}

	rwlock_init(&dst_ctx->lock);
	refcount_set(&dst_ctx->refs, 1);

	read_lock(&src_ctx->lock);
	files = src_ctx->files;
	if (files)
		atomic_inc(&files->count);

	seccomp = src_ctx->seccomp;
	if (seccomp.filter)
		__get_seccomp_filter_users(seccomp.filter);

	fs = src_ctx->fs;
	if (fs) {
		spin_lock(&fs->lock);
		++fs->users;
		spin_unlock(&fs->lock);
	}

	nsproxy = src_ctx->nsproxy;
	if (nsproxy)
		get_nsproxy(nsproxy);

	cred = src_ctx->cred;
	if (cred)
		get_cred(cred);
	real_cred = src_ctx->real_cred;
	if (real_cred)
		get_cred(real_cred);
	read_unlock(&src_ctx->lock);

	if (files)
		dst_ctx->files = dup_fd(files, NR_OPEN_MAX, ret);
	if (!dst_ctx->files)
		goto fail_fd;
	put_files_struct(files);
	files = NULL;
	dst_ctx->fs = copy_fs_struct(fs);
	if (!dst_ctx->fs) {
		*ret = -ENOMEM;
		goto fail_fs;
	}
	put_fs(fs);
	fs = NULL;

	dst_ctx->seccomp = seccomp;
	dst_ctx->nsproxy = nsproxy;
	dst_ctx->cred = cred;
	dst_ctx->real_cred = real_cred;
	*ret = 0;
	put_uswitch_ctx(src_ctx);
	return dst_ctx;

fail_fs:
	if (dst_ctx->files)
		put_files_struct(dst_ctx->files);
fail_fd:
	if (fs)
		put_fs(fs);
	if (files)
		put_files_struct(files);
	if (seccomp.filter)
		__seccomp_filter_release(seccomp.filter);
	if (nsproxy)
		put_nsproxy(nsproxy);
	if (cred)
		put_cred(cred);
	if (real_cred)
		put_cred(real_cred);
	idr_remove(&dst_table->ctx_idr, cid);
fail_idr_alloc:
	kfree(dst_ctx);
fail_malloc:
	put_uswitch_ctx(src_ctx);
	return NULL;
}

static int copy_uswitch_fork(struct task_struct *tsk, int fork_control) {
	struct uswitch_contexts_struct *ctxs = NULL, *current_ctxs = NULL;
	struct uswitch_contexts_table_struct *ptab = NULL, *public_table = NULL;
	struct uswitch_context_struct *ctx = NULL;
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *vma;
	struct page *data_page;
	struct uswitch_data *kernel_data;
	struct uswitch_data *user_data;
	int ret = 0;
	int current_cid, fork_descriptor;
	unsigned long user_addr;
	unsigned long pfn;
	int cid;

	current_ctxs = current->uswitch_contexts;
	data_page = alloc_page(GFP_USER);
	pfn = page_to_pfn(data_page);
	ctxs = kcalloc(1, sizeof(struct uswitch_contexts_struct), GFP_KERNEL);
	ptab = kcalloc(1, sizeof(struct uswitch_contexts_table_struct), GFP_KERNEL);
	if (!data_page || !ctxs || !ptab) {
		ret = -ENOMEM;
		goto fail_malloc;
	}
	spin_lock_init(&ptab->lock);
	idr_init(&ptab->ctx_idr);
	refcount_set(&ptab->refs, 1);

	kernel_data = page_address(data_page);
	public_table = current_ctxs->public_table;
	ptab->flags = public_table->flags;
	current_cid = current_ctxs->current_cid;
	fork_descriptor = current_ctxs->kernel_data->fork_descriptor;

	ctxs->public_table = ptab;
	ctxs->current_cid = current_cid;
	ctxs->data_page = data_page;
	ctxs->kernel_data = kernel_data;
	memset(kernel_data, 0, PAGE_SIZE);
	kernel_data->shared_descriptor = 0;
	kernel_data->next_descriptor = -1;
	kernel_data->seccomp_descriptor = -1;
	kernel_data->next_seccomp_descriptor = -2;
	kernel_data->ss_control = USWITCH_SIGNAL_STACK_USE_THREAD;
	kernel_data->block_signals = 0;
	kernel_data->next_block_signals = -1;
	kernel_data->fork_descriptor = 0;
	kernel_data->fork_control = USWITCH_FORK_DISABLE;

	ctx = copy_uswitch_ctx(ptab, public_table, current_cid, &ret);
	if (!ctx)
		goto fail_copy_ctxs;

	if (fork_control == USWITCH_FORK_CURRENT_AND_ANOTHER && current_cid != fork_descriptor) {
		ctx = copy_uswitch_ctx(ptab, public_table, fork_descriptor, &ret);
		if (!ctx)
			goto fail_copy_ctxs;
	}

	down_write(&mm->mmap_lock);
	user_addr = get_unmapped_area(NULL, 0, PAGE_SIZE, 0, 0);
	if (IS_ERR_VALUE(user_addr)) {
		goto fail_get_unmap;
	}
	user_data = (struct uswitch_data *)user_addr;
	ctxs->user_data = user_data;

	vma = vm_area_alloc(mm);
	if (!vma) {
		ret = -ENOMEM;
		goto fail_get_unmap;
	}
	vma->vm_pgoff = user_addr >> PAGE_SHIFT;
	vma->vm_start = user_addr;
	vma->vm_end = user_addr + PAGE_SIZE;
	vma->vm_flags = calc_vm_prot_bits(PROT_READ | PROT_WRITE, 0) | calc_vm_flag_bits(MAP_SHARED) |
			mm->def_flags | VM_MAYREAD | VM_MAYWRITE | VM_MAYSHARE | VM_SHARED | VM_DONTCOPY | VM_PFNMAP;
	if (!arch_validate_flags(vma->vm_flags)) {
		ret = -ENOMEM;
		goto fail_vma_alloc;
	}
	vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	
	if (remap_pfn_range(vma, user_addr, pfn, PAGE_SIZE, vma->vm_page_prot)) {
		ret = -ENOMEM;
		goto fail_vma;
	}
	if (insert_vm_struct(mm, vma)) {
		ret = -ENOMEM;
		goto fail_vma;
	}
	up_write(&mm->mmap_lock);

	tsk->uswitch_contexts = ctxs;

	return 0;
fail_vma:
fail_vma_alloc:
	vm_area_free(vma);
fail_get_unmap:
	up_write(&mm->mmap_lock);
fail_copy_ctxs:
	idr_for_each_entry(&ptab->ctx_idr, ctx, cid) {
		put_uswitch_ctx(ctx);
	}
	idr_destroy(&ptab->ctx_idr);
fail_malloc:
	if(data_page)
		__free_page(data_page);
	kfree(ctxs);
	kfree(ptab);
	return ret;
}

int copy_uswitch(unsigned long clone_flags, struct task_struct *tsk)
{
	int fork_control;
	if (!current->uswitch_contexts)
		return 0;
	if ((clone_flags & CLONE_FILES) && (clone_flags & CLONE_THREAD) && !(clone_flags & CLONE_VFORK) &&
		(clone_flags & CLONE_VM ) && (clone_flags & CLONE_FS) && !(clone_flags & CLONE_NEWNS) &&
		!(clone_flags & CLONE_NEWIPC) && !(clone_flags & CLONE_NEWNET) && !(clone_flags & CLONE_NEWCGROUP) &&
		!(clone_flags & CLONE_NEWPID) && !(clone_flags & CLONE_NEWUSER) && !(clone_flags & CLONE_NEWUTS) &&
		!(clone_flags & CLONE_INTO_CGROUP))
		return copy_uswitch_thread(tsk);
	fork_control = current->uswitch_contexts->kernel_data->fork_control;
	if (fork_control != USWITCH_FORK_DISABLE &&
		!(clone_flags & CLONE_FILES) && !(clone_flags & CLONE_THREAD) && !(clone_flags & CLONE_VFORK) &&
		!(clone_flags & CLONE_VM ) && !(clone_flags & CLONE_FS) && !(clone_flags & CLONE_NEWNS) &&
		!(clone_flags & CLONE_NEWIPC) && !(clone_flags & CLONE_NEWNET) && !(clone_flags & CLONE_NEWCGROUP) &&
		!(clone_flags & CLONE_NEWPID) && !(clone_flags & CLONE_NEWUSER) && !(clone_flags & CLONE_NEWUTS) &&
		!(clone_flags & CLONE_INTO_CGROUP))
		return copy_uswitch_fork(tsk, fork_control);
	return 0;
}

void exit_uswitch(struct task_struct *tsk)
{
	struct uswitch_contexts_struct *ctxs;
	struct uswitch_context_struct *ctx;
	int id;

	task_lock(tsk);
	ctxs = tsk->uswitch_contexts;
	if (!ctxs) {
		task_unlock(tsk);
		return;
	}
	tsk->uswitch_contexts = NULL;
	task_unlock(tsk);

	if (ctxs->saved_seccomp.filter)
		__seccomp_filter_release(ctxs->saved_seccomp.filter);
		//__put_seccomp_filter(ctxs->saved_seccomp.filter);

	if (refcount_dec_and_test(&ctxs->public_table->refs)) {
		idr_for_each_entry(&ctxs->public_table->ctx_idr, ctx, id) {
			put_uswitch_ctx(ctx);
		}
		idr_destroy(&ctxs->public_table->ctx_idr);
		kfree(ctxs->public_table);
	}
	vm_munmap((unsigned long)ctxs->user_data, PAGE_SIZE);
	if(ctxs->data_page)
		__free_page(ctxs->data_page);
	kfree(ctxs);
}

int uswitch_enter_syscall(unsigned long *work, bool *need_switch_seccomp)
{
	int res = do_uswitch_switch(need_switch_seccomp) == -EBUSY ? -1 : 0;
	*work = READ_ONCE(current_thread_info()->syscall_work);
	return res;
}

int do_uswitch_init(struct uswitch_data **pdata, int flags)
{
	struct task_struct *tsk = get_current();
	struct uswitch_contexts_struct *ctxs = NULL;
	struct uswitch_contexts_table_struct *public_table = NULL;
	struct uswitch_context_struct *ctx = NULL;
	struct mm_struct *mm = tsk->mm;
	struct vm_area_struct *vma;
	struct page *data_page;
	struct uswitch_data *kernel_data;
	struct uswitch_data *user_data;
	int ret = 0;
	int new_cid;
	unsigned long user_addr;
	unsigned long pfn;
	if (tsk->uswitch_contexts)
		return -EINVAL;
	data_page = alloc_page(GFP_USER);
	pfn = page_to_pfn(data_page);
	ctxs = kcalloc(1, sizeof(struct uswitch_contexts_struct), GFP_KERNEL);
	public_table = kcalloc(1, sizeof(struct uswitch_contexts_table_struct), GFP_KERNEL);
	ctx = kcalloc(1, sizeof(struct uswitch_context_struct), GFP_KERNEL);
	if (!data_page || !ctxs || !public_table || !ctx) {
		ret = -ENOMEM;
		goto fail_malloc;
	}
	kernel_data = page_address(data_page);
	ctxs->public_table = public_table;
	ctxs->current_cid = 0;
	ctxs->data_page = data_page;
	ctxs->kernel_data = kernel_data;
	refcount_set(&ctx->refs, 1);
	refcount_set(&public_table->refs, 1);
	rwlock_init(&ctx->lock);
	spin_lock_init(&public_table->lock);
	idr_init(&public_table->ctx_idr);
	public_table->flags = flags;
	new_cid = idr_alloc(&public_table->ctx_idr, ctx, 0, USWITCH_MAX_CID, GFP_KERNEL);
	if (new_cid < 0) {
		ret = -ENOMEM;
		goto fail_idr_alloc;
	}

	down_write(&mm->mmap_lock);
	user_addr = get_unmapped_area(NULL, 0, PAGE_SIZE, 0, 0);
	if (IS_ERR_VALUE(user_addr)) {
		ret = -ENOMEM;
		goto fail_get_unmap; 
	}
	user_data = (struct uswitch_data *)user_addr;
	ctxs->user_data = user_data;
	vma = vm_area_alloc(mm);
	if (!vma) {
		ret = -ENOMEM;
		goto fail_get_unmap;
	}
	vma->vm_pgoff = user_addr >> PAGE_SHIFT;
	vma->vm_start = user_addr;
	vma->vm_end = user_addr + PAGE_SIZE;
	vma->vm_flags = calc_vm_prot_bits(PROT_READ | PROT_WRITE, 0) | calc_vm_flag_bits(MAP_SHARED) |
			mm->def_flags | VM_MAYREAD | VM_MAYWRITE | VM_MAYSHARE | VM_SHARED | VM_DONTCOPY | VM_PFNMAP;
	if (!arch_validate_flags(vma->vm_flags)) {
		ret = -ENOMEM;
		goto fail_vma_alloc;
	}
	vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);

	if (remap_pfn_range(vma, user_addr, pfn, PAGE_SIZE, vma->vm_page_prot)) {
		ret = -ENOMEM;
		goto fail_vma;
	}

	if (insert_vm_struct(mm, vma)) {
		ret = -ENOMEM;
		goto fail_vma;
	}

	up_write(&mm->mmap_lock);

	spin_lock_irq(&current->sighand->seccomplock);
	ctx->seccomp = tsk->seccomp;
	if (ctx->seccomp.filter)
		get_seccomp_filter(tsk);
		//__get_seccomp_filter(tsk->seccomp.filter);
	spin_unlock_irq(&tsk->sighand->seccomplock);

	if (flags & USWITCH_ISOLATE_CREDENTIALS) {
		rcu_read_lock();
		ctx->cred = rcu_dereference(tsk->cred);
		ctx->real_cred = rcu_dereference(tsk->real_cred);
		get_cred(ctx->cred);
		get_cred(ctx->real_cred);
		rcu_read_unlock();
	} else {
		ctx->cred = NULL;
		ctx->real_cred = NULL;
	}

	task_lock(tsk);
	ctx->files = tsk->files;
	atomic_inc(&tsk->files->count);
	ctx->fs = tsk->fs;
	spin_lock(&ctx->fs->lock);
	++ctx->fs->users;
	spin_unlock(&ctx->fs->lock);
	if (flags & USWITCH_ISOLATE_NAMESPACES) {
		get_nsproxy(tsk->nsproxy);
		ctx->nsproxy = tsk->nsproxy;
	} else
		ctx->nsproxy = NULL;
	tsk->uswitch_contexts = ctxs;
	task_unlock(tsk);

	set_task_syscall_work(tsk, USWITCH);
	memset(kernel_data, 0, PAGE_SIZE);
	kernel_data->shared_descriptor = 0;
	kernel_data->next_descriptor = -1;
	kernel_data->seccomp_descriptor = -1;
	kernel_data->next_seccomp_descriptor = -2;
	kernel_data->ss_control = USWITCH_SIGNAL_STACK_USE_THREAD;
	kernel_data->block_signals = 0;
	kernel_data->next_block_signals = -1;
	kernel_data->fork_control = USWITCH_FORK_DISABLE;

	if (put_user(user_data, pdata))
		return -EFAULT;
	return new_cid;
fail_vma:
fail_vma_alloc:
	vm_area_free(vma);
fail_get_unmap:
	up_write(&mm->mmap_lock);
fail_idr_alloc:
	idr_destroy(&public_table->ctx_idr);
fail_malloc:
	if(data_page)
		__free_page(data_page);
	kfree(ctxs);
	kfree(public_table);
	kfree(ctx);
	return ret;
}

int do_uswitch_clone(int flags)
{
	struct task_struct *tsk = get_current();
	struct uswitch_contexts_struct *ctxs = tsk->uswitch_contexts;
	struct uswitch_context_struct *ctx = NULL;
	struct files_struct *files = NULL;
	struct fs_struct *fs = NULL;
	int ret;
	int new_cid;
	int fd_flag = flags & USWITCH_CLONE_FD_FIELD;
	int fd_share = fd_flag == USWITCH_CLONE_FD_SHARE;
	int fd_copy = fd_flag == USWITCH_CLONE_FD_COPY;
	int fd_new = fd_flag == USWITCH_CLONE_FD_NEW;
	if (!ctxs)
		return -EINVAL;
	if (!(fd_share || fd_copy || fd_new))
		return -EINVAL;
	ctx = kcalloc(1, sizeof(struct uswitch_context_struct), GFP_KERNEL);
	if (!ctx) {
		ret = -ENOMEM;
		goto fail_malloc;
	}
	rwlock_init(&ctx->lock);
	refcount_set(&ctx->refs, 1);

	if (fd_share || fd_copy) {
		files = get_task_files(tsk);
		if (fd_share)
			ctx->files = files;
		else {
			ctx->files = dup_fd(files, NR_OPEN_MAX, &ret);
			put_files_struct(files);
			if (!ctx->files)
				goto fail_fd;
		}
	} else {
		ctx->files = dup_fd(&init_files, NR_OPEN_MAX, &ret);
		if (!ctx->files)
			goto fail_fd;
	}

	spin_lock_irq(&current->sighand->seccomplock);
	if (tsk->seccomp.filter)
		get_seccomp_filter(tsk);
		//__get_seccomp_filter(tsk->seccomp.filter);
	ctx->seccomp = tsk->seccomp;
	spin_unlock_irq(&current->sighand->seccomplock);

	fs = get_task_fs(tsk);
	if ((flags & USWITCH_CLONE_FS_FIELD) == USWITCH_CLONE_FS_SHARE)
		ctx->fs = fs;
	else {
		ctx->fs = copy_fs_struct(fs);
		put_fs(fs);
		if (!ctx->fs)
			goto fail_idr_alloc;
	}

	if (ctxs->public_table->flags & USWITCH_ISOLATE_NAMESPACES)
		ctx->nsproxy = get_task_nsproxy(tsk);
	else
		ctx->nsproxy = NULL;

	if (ctxs->public_table->flags & USWITCH_ISOLATE_CREDENTIALS) {
		rcu_read_lock();
		ctx->cred = rcu_dereference(tsk->cred);
		ctx->real_cred = rcu_dereference(tsk->real_cred);
		get_cred(ctx->cred);
		get_cred(ctx->real_cred);
		rcu_read_unlock();
	} else {
		ctx->cred = NULL;
		ctx->real_cred = NULL;
	}

	idr_preload(GFP_KERNEL);
	spin_lock(&ctxs->public_table->lock);
	new_cid = idr_alloc(&ctxs->public_table->ctx_idr, ctx, 0, USWITCH_MAX_CID, GFP_ATOMIC);
	spin_unlock(&ctxs->public_table->lock);
	idr_preload_end();

	if (new_cid < 0) {
		ret = -ENOMEM;
		goto fail_idr_alloc;
	}
	return new_cid;
fail_idr_alloc:
	if (ctx->seccomp.filter)
		__seccomp_filter_release(ctx->seccomp.filter);
		//__put_seccomp_filter(ctx->seccomp.filter);
	if (ctx->files)
		put_files_struct(ctx->files);
	if (ctx->fs)
		put_fs(ctx->fs);
	if (ctx->nsproxy)
		put_nsproxy(ctx->nsproxy);
	if (ctx->cred)
		put_cred(ctx->cred);
	if (ctx->real_cred)
		put_cred(ctx->real_cred);
fail_fd:
	kfree(ctx);
fail_malloc:
	return ret;
}

int do_uswitch_switch(bool *need_switch_seccomp)
{
	struct task_struct *tsk = get_current();
	struct uswitch_contexts_struct *ctxs = tsk->uswitch_contexts;
	struct uswitch_context_struct *current_ctx, *next_ctx, *seccomp_ctx = NULL;
	int current_cid, next_cid, next_next_cid, seccomp_cid, next_seccomp_cid;
	int next_block_signals;

	struct files_struct *current_files = NULL;
	struct fs_struct *current_fs = NULL;
	struct seccomp current_seccomp = {.filter = NULL};
	struct nsproxy *current_ns = NULL;
	const struct cred *current_cred_ = NULL;
	const struct cred *current_real_cred_ = NULL;
	struct seccomp temp_seccomp = {.filter = NULL};
	bool need_write_back = false;

	if (need_switch_seccomp)
		*need_switch_seccomp = false;
	if (!ctxs)
		return -EINVAL;
	current_cid = READ_ONCE(ctxs->current_cid);
	next_cid = READ_ONCE(ctxs->kernel_data->shared_descriptor);
	next_next_cid = READ_ONCE(ctxs->kernel_data->next_descriptor);
	seccomp_cid = READ_ONCE(ctxs->kernel_data->seccomp_descriptor);
	next_seccomp_cid = READ_ONCE(ctxs->kernel_data->next_seccomp_descriptor);
	next_block_signals = READ_ONCE(ctxs->kernel_data->next_block_signals);
	if (current_cid == next_cid) {
		if (next_next_cid != -1) {
			WRITE_ONCE(ctxs->kernel_data->shared_descriptor, next_next_cid);
			WRITE_ONCE(ctxs->kernel_data->next_descriptor, -1);
		}
		if (seccomp_cid != -1 && seccomp_cid != current_cid && need_switch_seccomp) {
			rcu_read_lock();
			seccomp_ctx = get_uswitch_ctx(ctxs->public_table, seccomp_cid);
			rcu_read_unlock();
			if (!seccomp_ctx)
				return -EBUSY;
			read_lock(&seccomp_ctx->lock);
			current_seccomp = seccomp_ctx->seccomp;
			if (current_seccomp.filter)
				__get_seccomp_filter(current_seccomp.filter);
			read_unlock(&seccomp_ctx->lock);
			spin_lock_irq(&tsk->sighand->seccomplock);
			ctxs->saved_seccomp = tsk->seccomp;
			tsk->seccomp = current_seccomp;
			*need_switch_seccomp = true;
			if (tsk->seccomp.mode != SECCOMP_MODE_DISABLED)
				set_task_syscall_work(tsk, SECCOMP);
			else
				clear_task_syscall_work(tsk, SECCOMP);
			spin_unlock_irq(&tsk->sighand->seccomplock);
			put_uswitch_ctx(seccomp_ctx);
		}
		if (next_seccomp_cid != -2) {
			WRITE_ONCE(ctxs->kernel_data->seccomp_descriptor, next_seccomp_cid);
			WRITE_ONCE(ctxs->kernel_data->next_seccomp_descriptor, -2);
		}
		if (next_block_signals != -1) {
			WRITE_ONCE(ctxs->kernel_data->block_signals, next_block_signals);
			WRITE_ONCE(ctxs->kernel_data->next_block_signals, -1);
		}
		return current_cid;
	}

	rcu_read_lock();
	current_ctx = get_uswitch_ctx(ctxs->public_table, current_cid);
	next_ctx = get_uswitch_ctx(ctxs->public_table, next_cid);
	if (seccomp_cid != -1 && seccomp_cid != next_cid && need_switch_seccomp)
		seccomp_ctx = get_uswitch_ctx(ctxs->public_table, seccomp_cid);
	rcu_read_unlock();
	//printk("%px %px %d %d %px %px\n", ctxs->kernel_data, ctxs->user_data, current_cid, next_cid, current_ctx, next_ctx);

	if (!next_ctx) {
		put_uswitch_ctx(current_ctx);
		put_uswitch_ctx(seccomp_ctx);
		return -EINVAL;
	}

	read_lock(&next_ctx->lock);
	current_files = next_ctx->files;
	if (current_files)
		atomic_inc(&current_files->count);
	current_fs = next_ctx->fs;
	if (current_fs) {
		spin_lock(&current_fs->lock);
		++current_fs->users;
		spin_unlock(&current_fs->lock);
	}
	current_seccomp = next_ctx->seccomp;
	if (current_seccomp.filter)
		__get_seccomp_filter_users(current_seccomp.filter);
	current_ns = next_ctx->nsproxy;
	if (current_ns)
		get_nsproxy(current_ns);
	current_cred_ = next_ctx->cred;
	if (current_cred_)
		get_cred(current_cred_);
	current_real_cred_ = next_ctx->real_cred;
	if (current_real_cred_)
		get_cred(current_real_cred_);
	read_unlock(&next_ctx->lock);

	if (seccomp_ctx) {
		read_lock(&seccomp_ctx->lock);
		temp_seccomp = seccomp_ctx->seccomp;
		if (current_seccomp.filter)
			__get_seccomp_filter_users(current_seccomp.filter);
		read_unlock(&seccomp_ctx->lock);
	}

	task_lock(tsk);
	swap(current_files, tsk->files);
	swap(current_fs, tsk->fs);
	if (ctxs->public_table->flags & USWITCH_ISOLATE_NAMESPACES)
		swap(current_ns, tsk->nsproxy);
	task_unlock(tsk);

	spin_lock_irq(&tsk->sighand->seccomplock);
	if (seccomp_ctx) {
		ctxs->saved_seccomp = current_seccomp;
		current_seccomp = tsk->seccomp;
		tsk->seccomp = temp_seccomp;
		*need_switch_seccomp = true;
	} else {
		swap(current_seccomp, tsk->seccomp);
	}
	if (tsk->seccomp.mode != SECCOMP_MODE_DISABLED)
		set_task_syscall_work(tsk, SECCOMP);
	else
		clear_task_syscall_work(tsk, SECCOMP);
	spin_unlock_irq(&tsk->sighand->seccomplock);

	if (ctxs->public_table->flags & USWITCH_ISOLATE_CREDENTIALS) {
		current_cred_ = rcu_replace_pointer(tsk->cred, current_cred_, 1);
		current_real_cred_ = rcu_replace_pointer(tsk->real_cred, current_real_cred_, 1);
	}

	if (current_ctx) {
		read_lock(&current_ctx->lock);
		need_write_back = !(
			current_ctx->files == current_files &&
			current_ctx->fs == current_fs &&
			current_ctx->seccomp.mode == current_seccomp.mode &&
			current_ctx->seccomp.filter == current_seccomp.filter &&
			current_ctx->nsproxy == current_ns &&
			current_ctx->cred == current_cred_ &&
			current_ctx->real_cred == current_real_cred_);
		read_unlock(&current_ctx->lock);
		if (need_write_back) {
			write_lock(&current_ctx->lock);
			swap(current_files, current_ctx->files);
			swap(current_fs, current_ctx->fs);
			swap(current_seccomp, current_ctx->seccomp);
			swap(current_ns, current_ctx->nsproxy);
			swap(current_cred_, current_ctx->cred);
			swap(current_real_cred_, current_ctx->real_cred);
			write_unlock(&current_ctx->lock);
		}
	}

	if (current_files)
		put_files_struct(current_files);
	if (current_fs)
		put_fs(current_fs);
	if (current_seccomp.filter)
		__seccomp_filter_release(current_seccomp.filter);
	if (current_ns)
		put_nsproxy(current_ns);
	if (current_cred_)
		put_cred(current_cred_);
	if (current_real_cred_)
		put_cred(current_real_cred_);

	if (next_next_cid != -1) {
		WRITE_ONCE(ctxs->kernel_data->shared_descriptor, next_next_cid);
		WRITE_ONCE(ctxs->kernel_data->next_descriptor, -1);
	}
	if (next_seccomp_cid != -2) {
		WRITE_ONCE(ctxs->kernel_data->seccomp_descriptor, next_seccomp_cid);
		WRITE_ONCE(ctxs->kernel_data->next_seccomp_descriptor, -2);
	}
	if (next_block_signals != -1) {
		WRITE_ONCE(ctxs->kernel_data->block_signals, next_block_signals);
		WRITE_ONCE(ctxs->kernel_data->next_block_signals, -1);
	}
	ctxs->current_cid = next_cid;
	// set user counts

	if (current_ctx)
		put_uswitch_ctx(current_ctx);
	put_uswitch_ctx(next_ctx);
	put_uswitch_ctx(seccomp_ctx);
	return next_cid;
}

int do_uswitch_destroy(int cid)
{
	struct task_struct *tsk = get_current();
	struct uswitch_contexts_struct *ctxs = tsk->uswitch_contexts;
	struct uswitch_context_struct *ctx;
	int ret;
	if (!ctxs) 
		return -EINVAL;
	if (ctxs->current_cid == cid)
		return -EBUSY;
	spin_lock(&ctxs->public_table->lock);
	ctx = (struct uswitch_context_struct *)idr_remove(&ctxs->public_table->ctx_idr, cid);
	if (!ctx) {
		ret = -EINVAL;
		goto cleanup;
	}
	spin_unlock(&ctxs->public_table->lock);
	put_uswitch_ctx(ctx);
	return 0;
cleanup:
	spin_unlock(&ctxs->public_table->lock);
	return ret;
}

int do_uswitch_dup_file(int cid, unsigned int fd)
{
	struct task_struct *tsk = get_current();
	struct uswitch_contexts_struct *ctxs = tsk->uswitch_contexts;
	struct uswitch_context_struct *ctx;
	struct file *file;
	int ret;
	if (!ctxs) 
		return -EINVAL;
	rcu_read_lock();
	ctx = (struct uswitch_context_struct *)idr_find(&ctxs->public_table->ctx_idr, cid);
	if (!ctx) {
		rcu_read_unlock();
		return -EINVAL;
	}
	file = fget_files(ctx->files, fd);
	rcu_read_unlock();
	if (!file)
		return -EBADF;
	ret = get_unused_fd_flags(0);
	if (ret >= 0)
		fd_install(ret, file);
	else
		fput(file);
	return ret;
}

void switch_page_table(void)
{
	asm (
		"mov %cr3, %rax\n"
		"btsq $63, %rax\n"
		"mov %rax, %cr3\n"
	);
}


int validate_init_flags(int flags)
{
	if (flags & ~USWITCH_INIT_FLAGS_FIELDS)
		return -EINVAL;
	if ((flags & USWITCH_ISOLATE_NAMESPACES) && !(flags & USWITCH_ISOLATE_CREDENTIALS))
		return -EINVAL;
	return 0;
}
