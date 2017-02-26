#ifndef __ITERATOR_H
#define __ITERATOR_H
#include "node.h"
#include <cassert>
#include <exception>

/**
 * Skiplist const iterator
 */
template <class Key, class Value> class Iterator {
private:
  Node<Key, Value> *pCurrent;

public:
  Iterator(Node<Key, Value> *p) : pCurrent(p) {}
  virtual ~Iterator() {}

  virtual const Key &key() const {
    assert(pCurrent != nullptr);
    return pCurrent->key();
  };

  virtual const Value &value() const {
    assert(pCurrent != nullptr);
    return pCurrent->value();
  };

  virtual const Value &operator*() {
    assert(pCurrent != nullptr);
    return pCurrent->value();
  };

  virtual const Value &operator->() {
    assert(pCurrent != nullptr);
    return pCurrent->value();
  };

  virtual bool operator==(const Iterator &it) const {
    typedef DataNode<Key, Value> *data_ptr_t;
    assert(dynamic_cast<data_ptr_t>(pCurrent) != nullptr);
    assert(dynamic_cast<data_ptr_t>(it.pCurrent) != nullptr);
    return pCurrent == it.pCurrent;
  };

  virtual bool operator!=(const Iterator &it) const { return !operator==(it); };

  virtual Iterator &operator=(const Iterator &it) {
    pCurrent = it.pCurrent;
    return *this;
  };

  virtual Iterator &operator++() {
    pCurrent = &pCurrent->next();
    return *this;
  };

  virtual Iterator operator++(int) {
    Iterator it(pCurrent);
    pCurrent = &pCurrent->next();
    return it;
  };
};

#endif // __ITERATOR_H
