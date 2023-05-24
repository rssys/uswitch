#include "MyMalloc.h"
#include "myHelpers.h"
#include <stdlib.h>  // NULL

MyMalloc::MyMalloc(void* baseaddr, uintptr_t sizeInBytes, bool init)
  : baseaddr((struct Slot*)baseaddr), endaddr((uintptr_t)baseaddr + sizeInBytes)
{
  if(baseaddr == NULL) ERROR("called MyMalloc with NULL\n")
  if(init) {
    this->baseaddr->inUse = false;
    this->baseaddr->previousSlot = NULL;
    this->baseaddr->nextSlot = NULL;
  }
}

void* MyMalloc::malloc(uintptr_t sizeInBytes) {
  // round up to next multiple of granularity
  sizeInBytes = (((sizeInBytes-1)/granularity)+1)*granularity;

  Slot* slot = baseaddr;
  uintptr_t size;
  while(slot != NULL) {
    size = sizeOfSlot(slot);
    if(!slot->inUse && size >= sizeInBytes) break;
    slot = slot->nextSlot;
  }
  if(slot == NULL) return NULL;  // not enough room
  slot->inUse = true;
  if(size >= sizeInBytes + sizeof(Slot) + granularity) {
    // enough room to make a new Slot with the leftover space
    Slot* oldNextSlot = slot->nextSlot;
    Slot* newNextSlot = (Slot*) ((uintptr_t)&slot->data + sizeInBytes);
    slot->nextSlot = newNextSlot;
    newNextSlot->inUse = false;
    newNextSlot->previousSlot = slot;
    newNextSlot->nextSlot = oldNextSlot;
    if(oldNextSlot) oldNextSlot->previousSlot = newNextSlot;
  } else {
    // just give this Slot all the space
    // no need to adjust anyone's nextSlot or previousSlot
  }
  return &slot->data;
}

bool MyMalloc::isValid(void* ptr) {
  return (uintptr_t)ptr >= (uintptr_t)baseaddr && (uintptr_t)ptr <= endaddr;
}

void MyMalloc::free(void* ptr) {
  if(!isValid(ptr)) ERROR("MyMalloc::free: invalid ptr %p, compare to baseaddr %p and endaddr %p\n", ptr, baseaddr, endaddr)
  Slot* slot = (Slot*) ((uintptr_t)ptr-sizeof(Slot));
    // assume 'slot' is a valid Slot, i.e. assume 'ptr' was previously returned
    //   from malloc() and thus is the ->data field of a valid Slot

  Slot* previousSlot = slot->previousSlot;
  Slot* nextSlot = slot->nextSlot;
  if(previousSlot == NULL || previousSlot->inUse) {
    // don't actually delete the Slot structure, just mark it free
    slot->inUse = false;
  } else {
    // give all this slot's space, plus the space taken up by its Slot structure,
    //   to previous Slot
    previousSlot->nextSlot = nextSlot;
    if(nextSlot) nextSlot->previousSlot = previousSlot;
    slot = previousSlot;
  }
  // now check if the slot we're in can be combined with nextSlot
  if(nextSlot != NULL && !nextSlot->inUse) {
    // combine this slot and next slot
    slot->nextSlot = nextSlot->nextSlot;
    nextSlot->previousSlot = slot->previousSlot;
  }
}

uintptr_t MyMalloc::sizeOfSlot(struct MyMalloc::Slot* slot) {
  Slot* nextSlot = slot->nextSlot;
  if(nextSlot) {
    return ((uintptr_t)nextSlot - (uintptr_t)slot->data);
  } else {
    return (endaddr - (uintptr_t)slot->data);
  }
}
