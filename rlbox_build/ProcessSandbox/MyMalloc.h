#pragma once

#include <stdint.h>
#include <sys/types.h>

// An allocator that "allocates" regions from a prespecified block of memory
// This implementation is a fairly simple "greedy" allocator;
//   if it's important for performance we should drop in a more sophisticated one
class MyMalloc {
  public:
    // baseaddr: start address of the region from which to return memory chunks
    // sizeInBytes: size of the region pointed to by baseaddr
    // init: if TRUE, initialize region-resident data structures (most of the time you want this)
    //   if FALSE, assume that another MyMalloc object is already operating on the
    //     same region; this MyMalloc will effectively "attach" to the region as well,
    //     and the two MyMalloc objects can cooperatively allocate chunks from the
    //     region.  User is responsible for ensuring that only one MyMalloc object is
    //     "active" (e.g. executing malloc() or free()) at once.
    MyMalloc(void* baseaddr, uintptr_t sizeInBytes, bool init);

    // returns NULL on failure
    void* malloc(uintptr_t sizeInBytes);

    // Must pass ptr previously returned from our malloc()
    void free(void* ptr);

    void* realloc(void* oldmem, uintptr_t bytes);

    // Does pointer point to somewhere inside the region we're managing?
    bool isValid(void* ptr);

    void* mySbrk(ssize_t size);

  protected:
    struct Slot {
      // TRUE if this slot is in-use, or FALSE if it is free
      bool inUse;

      // Pointer to previous Slot. For the first Slot, this will be NULL.
      Slot* previousSlot;

      // Pointer to next Slot. For the last Slot, this will be NULL.
      Slot* nextSlot;

      // Actual data
      // TODO: How to guarantee this is aligned properly
      uint8_t data[0];
    };

    Slot* const baseaddr;
    const uintptr_t endaddr;
    // The state variable for emulated sbrk. Indicates how far we have "sbrk"ed.
    void* sbrkEnd;

    uintptr_t sizeOfSlot(struct Slot* slot);

  private:
    // we work in chunks of this many bytes
    // allocations are rounded up to the next multiple of this
    static const uintptr_t granularity = 8;

};
