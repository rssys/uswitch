#include <sys/mman.h>
#include "syscall.h"

static void dummy(void) { }
weak_alias(dummy, __vm_wait);

extern int (*munmap_hook)(void *addr, size_t length);

int __munmap(void *start, size_t len)
{
	__vm_wait();
	/* return syscall(SYS_munmap, start, len); */
	return munmap_hook(start, len);
}

weak_alias(__munmap, munmap);
