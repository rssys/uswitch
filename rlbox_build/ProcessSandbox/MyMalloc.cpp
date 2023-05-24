#include "MyMalloc.h"
#include "myHelpers.h"

MyMalloc::MyMalloc(void* p_baseaddr, uintptr_t sizeInBytes, bool init)
  : baseaddr((struct Slot*)p_baseaddr), endaddr((uintptr_t)p_baseaddr + sizeInBytes), sbrkEnd(p_baseaddr)
{
  if(baseaddr == nullptr) ERROR("called MyMalloc with NULL\n")
}

static thread_local void* curMyMalloc;

static void* mySbrk(ssize_t size);

extern "C" {
#define USE_DL_PREFIX
#define USE_LOCKS 1
#define MORECORE mySbrk
#define MORECORE_CANNOT_TRIM
#define HAVE_MMAP 0
#define HAVE_MREMAP 0
#include "dlmalloc.c"
}

void* mySbrk(ssize_t size) {
  MyMalloc* myMalloc = (MyMalloc*) curMyMalloc;
  return myMalloc->mySbrk(size);
}

void* MyMalloc::mySbrk(ssize_t size) {
  if(size == 0) {
    return (void*) ((uintptr_t)sbrkEnd + 1);
  } else if(size < 0) {
    return (void*) MFAIL;
  } else {
    if(((uintptr_t)sbrkEnd+size) > endaddr) return (void*) MFAIL;
    else {
      void* oldsbrkEnd = sbrkEnd;
      sbrkEnd = (void*) ((uintptr_t)oldsbrkEnd + size);
      return oldsbrkEnd;
    }
  }
}

void* MyMalloc::malloc(uintptr_t sizeInBytes) {
  void* oldMyMalloc = curMyMalloc;
  curMyMalloc = this;
  auto ret = dlmalloc(sizeInBytes);
  curMyMalloc = oldMyMalloc;
  return ret;
}

void MyMalloc::free(void* ptr) {
  void* oldMyMalloc = curMyMalloc;
  curMyMalloc = this;
  dlfree(ptr);
  curMyMalloc = oldMyMalloc;
}

void* MyMalloc::realloc(void* oldmem, uintptr_t bytes) {
  void* oldMyMalloc = curMyMalloc;
  curMyMalloc = this;
  auto ret = dlrealloc(oldmem, bytes);
  curMyMalloc = oldMyMalloc;
  return ret;
}

bool MyMalloc::isValid(void* ptr) {
  return (uintptr_t)ptr >= (uintptr_t)baseaddr && (uintptr_t)ptr <= endaddr;
}
