#ifndef __SKIPLIST_H
#define __SKIPLIST_H
#include "iterator.h"
#include "node.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <functional>

/**
 * Skiplist interface
 */
template <class Key, class Value, size_t MAXHEIGHT, class Less = std::less<Key>>
class SkipList {
private:
  DataNode<Key, Value> *pHead;
  DataNode<Key, Value> *pTail;

  IndexNode<Key, Value> *pTailIdx;
  IndexNode<Key, Value> *aHeadIdx[MAXHEIGHT];

public:
  /**
   * Creates new empty skiplist
   */
  SkipList() {
    pHead = new DataNode<Key, Value>(nullptr, nullptr);
    pTail = new DataNode<Key, Value>(nullptr, nullptr);
    pHead->pNext = pTail;

    Node<Key, Value> *below = pHead;
    pTailIdx = new IndexNode<Key, Value>(pTail, pTail);
    for (size_t i = 0; i < MAXHEIGHT; i++) {
      aHeadIdx[i] = new IndexNode<Key, Value>(below, pHead);
      aHeadIdx[i]->pNext = pTailIdx;
      below = aHeadIdx[i];
    }
  }

  /**
   * Disable copy constructor
   */
  SkipList(const SkipList &that) = delete;

  /**
   * Destructor
   */
  virtual ~SkipList() {
    // Idx cleanup
    for (size_t i = 0; i < MAXHEIGHT; i++) {
      for (auto pIdx = aHeadIdx[i]; pIdx != pTailIdx; pIdx = delIdx(pIdx)) {
      }
    }
    delIdx(pTailIdx);

    // Data cleanup
    for (auto pData = pHead; pData != pTail; pData = delData(pData)) {
    }
    delData(pTail);
  }

  /**
   * Assign new value for the key. If a such key already has
   * association then old value returns, otherwise nullptr
   *
   * @param key key to be assigned with value
   * @param value to be added
   * @return old value for the given key or nullptr
   */
  virtual Value *Put(const Key &key, Value &value) {
    Path pp;
    if (search(key, pp)) {
      auto data_node = pp.pData->pNext;
      auto old_value = data_node->pValue;
      pp.pData->pValue = &value;
      return old_value;
    }
    put_new(pp, new DataNode<Key, Value>(&key, &value));
    return nullptr;
  };

  /**
   * Put value only if there is no association with key in
   * the list and returns nullptr
   *
   * If there is an established association with the key already
   * method doesn't nothing and returns existing value
   *
   * @param key key to be assigned with value
   * @param value to be added
   * @return existing value for the given key or nullptr
   */
  virtual Value *PutIfAbsent(const Key &key, Value &value) {
    Path pp;
    if (search(key, pp)) {
      auto data_node = pp.pData->pNext;
      return data_node->pValue;
    }
    put_new(pp, new DataNode<Key, Value>(&key, &value));
    return nullptr;
  };

  /**
   * Returns value assigned for the given key or nullptr
   * if there is no established association with the given key
   *
   * @param key to find
   * @return value associated with given key or nullptr
   */
  virtual Value *Get(const Key &key) const {
    Path pp;
    volatile bool found = search(key, pp);
    return found ? pp.pData->pNext->pValue : nullptr;
  };

  /**
   * Remove given key from the skiplist and returns value
   * it has or nullptr in case if key wasn't associated with
   * any value
   *
   * @param key to be added
   * @return value for the removed key or nullptr
   */
  virtual Value *Delete(const Key &key) {
    Path pp;
    if (search(key, pp)) {
      for (int i = 0; i <= pp.match_at; ++i) {
        pp.aIdx[i]->pNext = delIdx(pp.aIdx[i]->pNext);
      }

      auto data = pp.pData->pNext;
      auto old_value = data->pValue;
      pp.pData->pNext = delData(data);

      return old_value;
    }

    return nullptr;
  };

  /**
   * Same as Get
   */
  virtual Value *operator[](const Key &key) const { return Get(key); };

  /**
   * Return iterator onto very first key in the skiplist
   */
  virtual Iterator<Key, Value> cbegin() const {
    return Iterator<Key, Value>(pHead->pNext);
  };

  /**
   * Returns iterator to the first key that is greater or equals to
   * the given key
   */
  virtual Iterator<Key, Value> cfind(const Key &min) const {
    Path pp;
    search(min, pp);
    return Iterator<Key, Value>(pp.pData->pNext);
  };

  /**string
   * Returns iterator on the skiplist tail
   */
  virtual Iterator<Key, Value> cend() const {
    return Iterator<Key, Value>(pTail);
  };

  virtual void gvdump_datanode(std::ofstream &of,
                               DataNode<Key, Value> *pData) const {
    of << "\"" << (void *)pData << "_";
    if (pData->pKey != nullptr) {
      of << *pData->pKey;
    } else {
      of << "null";
    }
    of << "\"";
  }

  virtual void gvdump(std::string fname) const {
    using std::endl;
    std::ofstream of(fname);
    of << "digraph SkipList {" << endl;
    for (size_t i = 0; i < MAXHEIGHT; ++i) {
      for (auto pIdx = aHeadIdx[i]; pIdx != pTailIdx; pIdx = pIdx->pNext) {
        of << "  \"" << (void *)pIdx << "\"->\"" << (void *)pIdx->pNext << "\""
           << endl;
        of << "  \"" << (void *)pIdx << "\"->\"" << (void *)pIdx->pDown << "\""
           << endl;
        of << "  \"" << (void *)pIdx << "\"->\"" << (void *)pIdx->pRoot << "\""
           << endl;
      }
      of << "  { rank=same; ";
      for (auto pIdx = aHeadIdx[i]; pIdx != pTailIdx; pIdx = pIdx->pNext) {
        of << "\"" << (void *)pIdx << "\" ";
      }
      of << "  }" << endl << endl;
    }

    for (auto pData = pHead; pData != pTail; pData = pData->pNext) {
      of << "  ";
      gvdump_datanode(of, pData);
      of << "->";
      gvdump_datanode(of, pData->pNext);
      of << endl;
      of << "  \"" << (void *)pData << "\"->\"" << (void *)pData->pNext << "\""
         << endl;
    }

    of << "  { rank=same; ";
    for (auto pData = pHead; pData != pTail; pData = pData->pNext) {
      gvdump_datanode(of, pData);
      of << " ";
    }
    of << "  }" << endl;
    of << "  pTailIdx_" << (void *)pTailIdx << endl;
    of << "  pTail_" << (void *)pTail << endl;
    of << "}" << endl;
  };

private:
  struct Path {
    IndexNode<Key, Value> *aIdx[MAXHEIGHT];
    int match_at;
    DataNode<Key, Value> *pData;

    void reset() {
      std::fill_n(aIdx, MAXHEIGHT, nullptr);
      match_at = -1;
      pData = nullptr;
    }
  };

  bool search(const Key &key, Path &prev_path) const {
    prev_path.reset();

    // iterate over index nodes
    bool found = false;
    const Key *curKey = nullptr;
    IndexNode<Key, Value> *prevIdx = nullptr;
    IndexNode<Key, Value> *curIdx = aHeadIdx[MAXHEIGHT - 1];

    for (int i = MAXHEIGHT - 1; i >= 0;) {
      // move forward
      do {
        prevIdx = curIdx;
        curIdx = curIdx->pNext;
        if (curIdx == pTailIdx) {
          break;
        }
        curKey = curIdx->pRoot->pKey;
      } while (Less()(*curKey, key));

      // if not found yet, check for exact match
      if (!found && curIdx != pTailIdx && !Less()(key, *curKey)) {
        prev_path.match_at = i;
        found = true;
      }

      // move down
      prev_path.aIdx[i--] = prevIdx;
      if (i >= 0) {
        curIdx = dynamic_cast<IndexNode<Key, Value> *>(prevIdx->pDown);
        assert(curIdx != nullptr);
      }
    }

    // iterate over data nodes
    DataNode<Key, Value> *prev = nullptr;
    DataNode<Key, Value> *cur =
        dynamic_cast<DataNode<Key, Value> *>(prevIdx->pDown);
    assert(cur != nullptr);
    assert(cur == prevIdx->pRoot);

    do {
      prev = cur;
      cur = cur->pNext;
      if (cur == pTail) {
        break;
      }
      curKey = cur->pKey;
    } while (Less()(*curKey, key));

    prev_path.pData = prev;
    return found || (cur != pTail && !Less()(key, *curKey));
  }

  void put_new(const Path &prev_path, DataNode<Key, Value> *pData) {
    assert(prev_path.match_at == -1);

    pData->pNext = prev_path.pData->pNext;
    prev_path.pData->pNext = pData;

    Node<Key, Value> *below = pData;
    for (size_t i = 0; i < MAXHEIGHT && flip(); ++i) {
      auto pIdx = new IndexNode<Key, Value>(below, pData);
      pIdx->pNext = prev_path.aIdx[i]->pNext;
      prev_path.aIdx[i]->pNext = pIdx;
      below = pIdx;
    }
  }

  bool flip() const { return rand() & 1; }

  IndexNode<Key, Value> *delIdx(IndexNode<Key, Value> *pIdx) const {
    IndexNode<Key, Value> *pNextIdx = pIdx->pNext;
    pIdx->pDown = nullptr;
    pIdx->pRoot = nullptr;
    pIdx->pNext = nullptr;
    delete pIdx;

    return pNextIdx;
  }

  DataNode<Key, Value> *delData(DataNode<Key, Value> *pData) const {
    DataNode<Key, Value> *pNextData = pData->pNext;
    pData->pKey = nullptr;
    pData->pValue = nullptr;
    pData->pNext = nullptr;
    delete pData;

    return pNextData;
  }
};
#endif // __SKIPLIST_H
