#include "gtest/gtest.h"
#include <iostream>
#include <sstream>

#include <coroutine/engine.h>

void _calculator_add(int& result, int left, int right) {
    result = left + right;
}

TEST(CoroutineTest, SimpleStart) {
    Coroutine::Engine engine;

    int result;
    engine.start(_calculator_add, result, 1, 2);

    ASSERT_EQ(3, result);
}

void printa(Coroutine::Engine& pe, std::stringstream& out, void*& other) {
    out << "A1 ";
    std::cout << "A1 " << std::endl;
    pe.sched(other);

    out << "A2 ";
    std::cout << "A2 " << std::endl;
    pe.sched(other);

    out << "A3 ";
    std::cout << "A3 " << std::endl;
    pe.sched(other);
}

void printb(Coroutine::Engine& pe, std::stringstream& out, void*& other) {
    out << "B1 ";
    std::cout << "B1 " << std::endl;
    pe.sched(other);

    out << "B2 ";
    std::cout << "B2 " << std::endl;
    pe.sched(other);

    out << "B3 ";
    std::cout << "B3 " << std::endl;
}

std::stringstream out;
void *pa = nullptr, *pb = nullptr;
void _printer(Coroutine::Engine& pe, std::string& result) {
    // Create routines, note it doens't get control yet
    pa = pe.run(printa, pe, out, pb);
    pb = pe.run(printb, pe, out, pa);
    std::cout << "pa = " << pa << ", pb = " << pb << std::endl;

    // Pass control to first routine, it will ping pong
    // between printa/printb greedely then we will get
    // contol back
    pe.sched(pa);
    out << "END";
    std::cout << "END" << std::endl;

    // done
    result = out.str();
}

TEST(CoroutineTest, Printer) {
    Coroutine::Engine engine;

    std::string result;
    engine.start(_printer, engine, result);
    ASSERT_STREQ("A1 B1 A2 B2 A3 B3 END", result.c_str());
}

/*******************ChannelsTests*************/

TEST(CoroutineTest, ChannelsUtils) {
    Coroutine::Engine engine;
    engine.cnew(1, 16);
    ASSERT_EQ(true, engine.cexists(1));
    ASSERT_EQ(false, engine.cexists(2));
    engine.cclose(1);
    ASSERT_EQ(false, engine.cexists(1));
}

std::ostringstream chout;
void chread1(Coroutine::Engine& pe, long& chid, size_t& msgsize) {
    std::cout << "A1 " << std::endl;
    char* buf = new char[msgsize];
    pe.cread(chid, buf, msgsize);
    chout << buf;
    delete[] buf;
}

void chmain1(Coroutine::Engine& pe) {
    size_t len = strlen("hello") + 1;
    long chid = 1;
    pe.cnew(chid, len);
    pe.cwrite(chid, "hello", len);
    pe.run(chread1, pe, chid, len);
    pe.yield();
}

TEST(CouroutineTest, ChannelsDelayedRead) {
    Coroutine::Engine engine;
    engine.start(chmain1, engine);
    std::string result = chout.str();
    ASSERT_STREQ("hello", result.c_str());
}
