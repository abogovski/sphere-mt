#include "allocator.h"
#include "allocator_error.h"
#include "allocator_pointer.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>

void AllocatorNode::setUsage(bool flag) {
  head = flag ? head | flg_mask : head & ~flg_mask;
}

void AllocatorNode::setLength(size_t length) {
  if (length & flg_mask) {
    throw AllocError(AllocErrorType::Internal, "length overlaps flag");
  }
  head = (head & flg_mask) | length;
}

bool AllocatorNode::usage() { return !!(head & flg_mask); }

size_t AllocatorNode::length() { return head & ~flg_mask; }

AllocatorNode *AllocatorNode::next(int step) {
  assert(step > 0);
  if (step > 1) {
    return this->next(1)->next(step - 1);
  }

  return (AllocatorNode *)(&head + head + 1);
}

/////////////////////////////////////////////////////////////////////////////////

Allocator::Allocator(void *base, size_t size)
    : base(base), first_node((AllocatorNode *)base), last_node(first_node),
      ptr_first(
          (AllocatorNode **)((char *)base + size - size % sizeof(size_t))),
      ptr_last(ptr_first) {
  assert(sizeof(AllocatorNode) == sizeof(size_t));
  assert(sizeof(AllocatorNode *) == sizeof(size_t));
  assert(sizeof(size_t) == 8);
  if (base == nullptr) {
    throw AllocError(AllocErrorType::NoMemory, "nullptr base");
  }
  if (size < 3 * sizeof(size_t)) {
    throw AllocError(AllocErrorType::NoMemory,
                     "too small size of base memory chunk");
  }

  first_node->setUsage(false);
  first_node->setLength(size / sizeof(size_t));
}

Pointer Allocator::alloc(size_t N) {
  if (!N) {
    return Pointer();
  }
  
  AllocatorNode **ptr = place_ptr();
  *ptr = force_find_free_node(N);
  alloc_node(*ptr, N);
  return Pointer(ptr);
}

void Allocator::realloc(Pointer &p, size_t N) {
  if (p.inner_ptr == nullptr) { // if nullptr
    p = alloc(N);
    return;
  }

  AllocatorNode *node = *(p.inner_ptr);

  if (node->length() * sizeof(size_t) >= N) { // if shrink
    alloc_node(node, N);
    return;
  }

  // grow
  if (node != last_node) { // try expand
    AllocatorNode *next = node->next();
    if (!next->usage()) {
      size_t avail_size =
          (node->length() + next->length() + 1) * sizeof(size_t);
      if (avail_size >= N) {
        realloc_node(node, N);
        return;
      }
    }
  }

  AllocatorNode *dst = force_find_free_node(N);
  memcpy(dst + 1, node + 1, N);

  dst->setUsage(true);
  free_node(node);
  alloc_node(dst, N);

  *(p.inner_ptr) = dst;
}

void Allocator::free(Pointer &p) {
  if (p.inner_ptr != nullptr) { // if nullptr
    free_node(*(p.inner_ptr));
    *p.inner_ptr = nullptr;
    p.inner_ptr = nullptr;
    squeze_ptrs();
  }
}

void Allocator::defrag() {
  AllocatorNode *dst = first_node;
  AllocatorNode *src = first_node;

  while (src <= last_node) {
    AllocatorNode *src_next = src->next();
    if (src->usage()) {
      memmove(dst, src, src->length() * sizeof(size_t));
      *std::find(ptr_first, ptr_last, src) = dst;
      dst = dst->next();
    }
    src = src_next;
  }

  static_assert(sizeof(*ptr_first) == sizeof(size_t), "");
  static_assert(sizeof(*dst) == sizeof(size_t), "");

  if ((AllocatorNode *)ptr_first - dst > 0) {
    dst->next()->setLength((AllocatorNode *)ptr_first - dst - 1);
    dst->next()->setUsage(false);
  }
}

void Allocator::squeze_ptrs() {
  size_t extend_by = 0;
  while (ptr_first != ptr_last && *ptr_first == nullptr) {
    ++ptr_first;
    ++extend_by;
  }
  if (last_node->usage() && extend_by > 0) {
    last_node = last_node->next();
    last_node->setUsage(false);
    last_node->setLength(extend_by);
  } else {
    last_node->setLength(last_node->length() + extend_by);
  }
}

AllocatorNode **Allocator::place_ptr() {
  AllocatorNode **ptr = ptr_last;
  while (--ptr >= ptr_first) {
    if (*ptr == nullptr) {
      return ptr;
    }
  }

  if (last_node->usage()) {
    throw AllocError(AllocErrorType::NoMemory, "ptr placement failed");
  }

  if (last_node->length() > 0){
    last_node->setLength(last_node->length() - 1);
  }

  --ptr_first;
  *ptr_first = nullptr;
  return ptr_first;
}

AllocatorNode *Allocator::find_free_node(size_t N) {
  AllocatorNode *node = first_node;
  for (;; node = node->next()) {
    if (!node->usage() && (node->length() * sizeof(size_t) >= N)) {
      return node;
    }
    if (node == last_node) {
      return nullptr;
    }
  }
}

AllocatorNode *Allocator::force_find_free_node(size_t N) {
  AllocatorNode *found = find_free_node(N);

  if (found == nullptr) {
    found = find_free_node(N);
    if (found == nullptr) {
      throw AllocError(AllocErrorType::NoMemory, "no large enough free nodes");
    }
  }

  return found;
}

void Allocator::alloc_node(AllocatorNode *node, size_t N) {
  size_t off = (N + sizeof(size_t) - 1) / sizeof(size_t);
  AllocatorNode *tail = node + off + 1;
  size_t tail_len = node->length() - off;
  if (tail_len > 0) {
    tail->setUsage(false);
    tail->setLength(tail_len - 1);
    if (node == last_node) {
      last_node = tail;
    } else if (!tail->next()->usage()) {
      tail->setLength((tail_len - 1) + (tail->next()->length() + 1));
    }
  }

  node->setUsage(true);
  node->setLength(off);
}

void Allocator::realloc_node(AllocatorNode *node, size_t N) {
  size_t off = (N + sizeof(size_t) - 1) / sizeof(size_t);
  AllocatorNode *tail = node + off + 1;
  size_t tail_len = node->length() + node->next()->length() - off;
  bool next_was_last = (node->next() == last_node);
  if (tail_len > 0) {
    tail->setUsage(false);
    tail->setLength(tail_len - 1);
    if (next_was_last) {
      last_node = tail;
    }
  }

  node->setUsage(true);
  node->setLength(off);
}

void Allocator::free_node(AllocatorNode *node) {
  node->setUsage(false);
  if (node != first_node) {
    AllocatorNode *prev = first_node;
    while (prev->next() != node)
      prev = prev->next();
    if (!prev->usage()) {
      prev->setLength(prev->length() + node->length() + 1);
      if (node == last_node) {
        last_node = prev;
      }
      node = prev;
    }

    if (node != last_node && !node->next()->usage()) {
      if (node->next() == last_node) {
        last_node = node;
      }
      node->setLength(node->length() + node->next()->length() + 1);
    }
  }
}
