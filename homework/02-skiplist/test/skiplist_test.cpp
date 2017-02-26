#include "gtest/gtest.h"
#include <fstream>
#include <skiplist/skiplist.h>
#include <string>

using namespace std;

TEST(SkipListTest, Empty) {
  SkipList<int, string, 8> sk;
  ASSERT_EQ(nullptr, sk.Get(100));
  ASSERT_EQ(sk.cend(), sk.cbegin()) << "Begin iterator fails";
  ASSERT_EQ(sk.cend(), sk.cfind(10)) << "Find iterator fails";
}

TEST(SkipListTest, SimplePut) {
  SkipList<int, string, 8> sk;
  sk.gvdump("0initial.dot");
  system("dot -Tpng 0initial.dot -o0initial.png");

  string test_str("test");
  std::string *pOld = sk.Put(10, test_str);
  sk.gvdump("1put.dot");
  system("dot -Tpng 1put.dot -o1put.png");
  assert(nullptr == pOld);
  // ASSERT_EQ(nullptr, pOld);

  pOld = sk[10];
  sk.gvdump("2idx.dot");
  system("dot -Tpng 2idx.dot -o2idx.png");
  assert(nullptr != pOld);
  assert(string("test") == *pOld);
  // ASSERT_NE(nullptr, pOld)         << "Value found";
  // ASSERT_EQ(string("test"), *pOld) << "Value is correct";

  pOld = sk.Get(10);
  sk.gvdump("3get.dot");
  system("dot -Tpng 3get.dot -o3get.png");
  assert(nullptr != pOld);
  assert(string("test") == *pOld);
  // ASSERT_NE(nullptr, pOld)         << "Value found";
  // ASSERT_EQ(string("test"), *pOld) << "Value is correct";

  Iterator<int, std::string> it = sk.cbegin();
  assert(sk.cend() != it);
  assert(10 == it.key());
  assert(string("test") == it.value());
  assert(string("test") == *it);
  // ASSERT_EQ(string("test"), *it)        << "Iterator value is correct";
  // ASSERT_NE(sk.cend(), it)              << "Iterator is not empty";
  // ASSERT_EQ(10, it.key())               << "Iterator key is correct";
  // ASSERT_EQ(string("test"), it.value()) << "Iterator value is correct";
  // ASSERT_EQ(string("test"), *it)        << "Iterator value is correct";
}
