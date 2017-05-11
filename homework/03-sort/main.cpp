#include "algo.h"
#include "config.h"

#include <cassert>

#include <iostream>
#include <string>

using std::sort;
using std::ostringstream;
using std::unique_ptr;

enum { BUF_LEN = 16 };

int main(int argc, char* argv[]) {
    assert(argc == 3);
    extsort<long>(argv[1], argv[2], BUF_LEN, 4);
    return 0;
}
