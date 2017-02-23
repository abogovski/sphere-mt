#include "allocator_pointer.h"
#include "allocator.h"
#include "allocator_error.h"

Pointer::Pointer() : inner_ptr(nullptr) {}

void *Pointer::get() const {
  if (inner_ptr == nullptr)
    return nullptr;

  if (*inner_ptr == nullptr || !(*inner_ptr)->usage()) {
    throw AllocError(AllocErrorType::InvalidOperation,
                     "possibly it's ptr.get() after free(ptr)");
  }
  return *inner_ptr + 1;
}

Pointer::Pointer(AllocatorNode **ptr) : inner_ptr(ptr) {}