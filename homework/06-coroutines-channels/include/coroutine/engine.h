#ifndef COROUTINE_ENGINE_H
#define COROUTINE_ENGINE_H

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <setjmp.h>
#include <stdexcept>
#include <tuple>

namespace Coroutine {

/**
 * # Entry point of coroutine library
 * Allows to run coroutine and schedule its execution. Not threadsafe
 */
class Engine final {
private:
    /**
     * A single coroutine instance which could be scheduled for execution
     * should be allocated on heap
     */
    struct context;
    struct channel;

    typedef struct context {
        // coroutine stack start address
        char* Low = nullptr;

        // coroutine stack end address
        char* Hight = nullptr;

        // coroutine stack copy buffer
        std::tuple<char*, uint32_t> Stack = std::make_tuple(nullptr, 0);

        // Saved coroutine context (registers)
        jmp_buf Environment;

        // Coroutine that has started this one. Once current routine is done, control must
        // be passed back to caller
        struct context* caller = nullptr;

        // Coroutine got control from the current one. Whenever current routine
        // continues self exectution it must transfers control to callee if any
        struct context* callee = nullptr;

        // To include routine in the different lists, such as "running"
        struct context* prev = nullptr;
        struct context* next = nullptr;

        // which channel this context awaits, if blocked, otherwise nullptr
        struct channel* awaiting = nullptr;
    } context;

    typedef struct channel {
        typedef struct write_task {
            context* owner;
            const char* src;
            size_t size;
        } write_task;

        typedef struct read_task {
            context* owner;
            char* dst;
            size_t size;
        } read_task;

        char* buf;
        size_t start;
        size_t filled;
        size_t size;

        Engine* engine;

        std::queue<write_task> write_tasks;
        std::queue<read_task> read_tasks;

        void create(Engine* eng, size_t buf_size) {
            buf = new char[buf_size];
            start = 0;
            filled = 0;
            size = buf_size;
            engine = eng;
        }

        void destroy() {
            if (write_tasks.size() > 0 || read_tasks.size() > 0) {
                throw std::runtime_error("attempt to close channel with pending tasks (blocked coroutines)");
            }

            delete[] buf;
        }

        void write(context* active_coroutine, const char* src, size_t src_size) {
            write_task wt = { active_coroutine, src, src_size };
            write_tasks.push(wt);
            active_coroutine->awaiting = this;

            while (active_coroutine != write_tasks.front().owner) {
                engine->sched(write_tasks.front().owner);
                if (read_tasks.size() > 0) {
                    engine->sched(read_tasks.front().owner);
                }
            }

            while (write_tasks.size() > 0 && active_coroutine == write_tasks.front().owner) {
                do_write_task();
                if (read_tasks.size() > 0) {
                    engine->sched(read_tasks.front().owner);
                }
            }
        }

        void read(context* active_coroutine, char* dst, size_t dst_size) {
            read_task rt = { active_coroutine, dst, dst_size };
            read_tasks.push(rt);
            active_coroutine->awaiting = this;

            while (active_coroutine != read_tasks.front().owner) {
                engine->sched(read_tasks.front().owner);
                if (write_tasks.size() > 0) {
                    engine->sched(write_tasks.front().owner);
                }
            }

            while (read_tasks.size() > 0 && active_coroutine == read_tasks.front().owner) {
                do_read_task();
                if (write_tasks.size() > 0) {
                    engine->sched(write_tasks.front().owner);
                }
            }
        }

    private:
        bool do_write_task() // returns true, if some tasks had progress (including removal of empty write task)
        {
            if (write_tasks.size() == 0) {
                return true;
            }

            bool written = false;
            write_task& wt = write_tasks.front();
            while (wt.size) {
                size_t avail_size = size - filled;
                if (!avail_size) {
                    break;
                }

                size_t transaction_size = avail_size < wt.size ? avail_size : wt.size;
                if (start + transaction_size > size) {
                    memcpy(buf + start, wt.src, size - start);
                    memcpy(buf, wt.src + (size - start), transaction_size - (size - start));
                } else {
                    memcpy(buf + start, wt.src, transaction_size);
                }
                written = true;
                wt.src += transaction_size;
                wt.size -= transaction_size;
                filled += transaction_size;
            }

            if (!wt.size) {
                wt.owner->awaiting = nullptr;
                write_tasks.pop();
            }

            return written;
        }

        bool do_read_task() {
            if (read_tasks.size() == 0) {
                return false;
            }

            bool was_read = false;
            read_task& rt = read_tasks.front();
            while (rt.size) {
                if (filled == 0) {
                    break;
                }
                size_t transaction_size = filled < rt.size ? filled : rt.size;
                if (start + transaction_size > size) {
                    memcpy(rt.dst, buf + start, size - start);
                    memcpy(rt.dst + (size - start), buf, transaction_size - (size - start));
                    start = transaction_size - (size - start);
                } else {
                    memcpy(rt.dst, buf + start, transaction_size);
                    start += transaction_size;
                }
                rt.dst += transaction_size;
                rt.size -= transaction_size;
            }

            if (!rt.size) {
                rt.owner->awaiting = nullptr;
                read_tasks.pop();
            }
        }
    } channel;
    std::map<long, channel> channels;

    /**
     * Where coroutines stack begins
     */
    char* StackBottom;

    /**const int&
     * Current coroutine
     */
    context* cur_routine;

    /**
     * List of routines ready to be scheduled. Note that suspended routine ends up here as well
     */
    context* running;

    context* idle;

protected:
    /**
     * Save stack of the current coroutine in the given context
     */
    void Store(context& ctx);

    /**
     * Restore stack of the given context and pass control to coroutinne
     */
    void Restore(context& ctx);

    /**
     * Suspend current coroutine execution and execute given context
     */
    //void Enter(context& ctx);

public:
    Engine()
        : StackBottom(0)
        , cur_routine(nullptr)
        , running(nullptr)
        , idle(nullptr) {}
    Engine(Engine&&) = delete;
    Engine(const Engine&) = delete;

    /**
     * Gives up current routine execution and let engine to schedule other one. It is not defined when
     * routine will get execution back, for example if there are no other coroutines then executing could
     * be trasferred back immediately (yield turns to be noop).
     *
     * Also there are no guarantee what coroutine will get execution, it could be caller of the current one or
     * any other which is ready to run
     */
    void yield();

    /**
     * Suspend current routine and transfers control to the given one, resumes its execution from the point
     * when it has been suspended previously.
     *
     * If routine to pass execution to is not specified runtime will try to transfer execution back to caller
     * of the current routine, if there is no caller then this method has same semantics as yield
     */
    void sched(void* routine);

    /**
     * Entry point into the engine. Prepare all internal mechanics and starts given function which is
     * considered as main.
     *
     * Once control returns back to caller of start all coroutines are done execution, in other words,
     * this function doesn't return control until all coroutines are done.
     *
     * @param pointer to the main coroutine
     * @param arguments to be passed to the main coroutine
     */
    template <typename... Ta>
    void start(void (*main)(Ta...), Ta&&... args) {
        if (idle != nullptr) {
            throw std::runtime_error("start called inside coroutines");
        }

        // save idle context
        idle = new context();
        if (setjmp(idle->Environment) != 0) {
            delete idle;
            return; // idle context = return from start()
        }

        // To acquire stack begin, create variable on stack and remember its address
        char StackStartsHere;
        this->StackBottom = &StackStartsHere;

        // Start routine execution
        void* pc = run(main, std::forward<Ta>(args)...);
        if (pc != nullptr) {
            // cur_routine = (context*) pc;
            sched(pc);
        }

        // Shutdown runtime
        this->StackBottom = 0;
    }

    /**
     * Register new coroutine. Couroutine won't receive control until scheduled explicitely or implicitly.
     * On error, function returns nullptr
     */
    template <typename... Ta>
    void* run(void (*func)(Ta...), Ta&&... args) {
        if (this->StackBottom == 0) {
            // Engine wasn't initialized yet
            return nullptr;
        }

        // New coroutine context that carries around all information enough to call function
        context* pc = new context();
        pc->caller = cur_routine;

        // Store current state right here, i.e just before enter new coroutine, later, once it gets scheduled
        // execution starts here. Note that we have to acquire stack of the current function call to ensure
        // that function parameters will be passed along
        // LONGJMP RETURNS HERE!
        if (setjmp(pc->Environment) > 0) {
            // here: only after longjmp
            // Created routine got control in order to start execution. Note that all variables, such as
            // context pointer, arguments and a pointer to the function comes from restored stack

            // invoke routine
            func(std::forward<Ta>(args)...);

            // Routine complete its execution, time to delete it.
            context* next = pc->caller;
            if (pc->prev != nullptr) {
                pc->prev->next = pc->next;
            }

            if (pc->next != nullptr) {
                pc->next->prev = pc->prev;
            }

            if (pc->caller != nullptr) {
                pc->caller->callee = nullptr;
            }

            if (running == cur_routine) {
                running = running->next;
            }

            if (next == nullptr) {
                next = running;
            }

            // current coroutine finished, and the pointer is not relevant now
            cur_routine = nullptr;

            pc->prev = pc->next = nullptr;
            delete std::get<0>(pc->Stack);
            delete pc;

            std::cout << "complete: " << pc << ", next: " << next << std::endl;
            sched(next);

            // sched should never return (longjmp to next or idle instead)
            assert(0);
        }

        // setjmp remembers position from which routine could starts execution, but to make it correctly
        // it is neccessary to save arguments, pointer to body function, pointer to context, e.t.c - i.e
        // save stack.
        Store(*pc);

        // Add routine as running list
        // add to the beginning of the double-linked list
        pc->next = running;
        running = pc;
        if (pc->next != nullptr) {
            pc->next->prev = pc;
        }

        return pc;
    }

    bool cexists(long id) {
        return channels.find(id) != channels.end();
    }

    void cnew(long id, size_t buf_size) {
        auto found = channels.find(id);
        if (found != channels.end()) {
            throw std::runtime_error("requested channel id is in use");
        }
        channel ch;
        ch.create(this, buf_size);
        channels[id] = ch;
    }

    void cclose(long id) {
        if (channels.find(id) != channels.end()) {
            channels[id].destroy();
            channels.erase(id);
        }
    }

    void cwrite(long id, const char* src, size_t src_size) {
        channels[id].write(cur_routine, src, src_size);
    }

    void cread(long id, char* dst, size_t dst_size) {
        channels[id].read(cur_routine, dst, dst_size);
    }
};

} // namespace Coroutine

#endif // COROUTINE_ENGINE_H
