#include <coroutine/engine.h>

#include <cassert>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Coroutine {

void Engine::Store(context& ctx) {
    char stackStart;

    ctx.Low = ctx.Hight = this->StackBottom;
    if (&stackStart > ctx.Low) {
        ctx.Hight = &stackStart;
    } else {
        ctx.Low = &stackStart;
    }

    int size = ctx.Hight - ctx.Low;
    if (std::get<1>(ctx.Stack) < size) {
        delete std::get<0>(ctx.Stack);
        std::get<0>(ctx.Stack) = new char[size];
        std::get<1>(ctx.Stack) = size;
    }

    memcpy(std::get<0>(ctx.Stack), ctx.Low, size);
}

void Engine::Restore(context& ctx) {
    char stackStart;
    char* stackAddr = &stackStart;

    if (ctx.Low <= stackAddr && stackAddr <= ctx.Hight) {
        Restore(ctx);
    }

    memcpy(ctx.Low, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    sched(nullptr);
}

void Engine::sched(void* routine_) {
    // pass control to the another coroutine
    // routine = nullptr means yield()

    context* routine = (context*)routine_;

    bool all_routines_blocked = (running != nullptr);
    for (context* p = running; p != nullptr; p = p->next) {
        if (p->awaiting == nullptr) {
            all_routines_blocked = false;
            break;
        }
    }
    if (all_routines_blocked) {
        throw std::runtime_error("deadlock"); // catches only the simplest cases of deadlock
    }

    // save context, if called from running coroutine
    if (cur_routine != nullptr) {
        if (setjmp(cur_routine->Environment) != 0) {
            return;
        }
        Store(*cur_routine);
    }

    // if all coroutines finished execution
    if (routine == nullptr && cur_routine == nullptr) {
        longjmp(idle->Environment, 1);
    }

    if (routine == nullptr && cur_routine != nullptr) {
        // invoke the caller of current coroutine
        if (cur_routine->caller != nullptr) {
            routine = cur_routine->caller;
        }
        // invoke ANY coroutine another than cur_routine
        // if no coroutines remain, pass control back to cur_routine
        else {
            for (context* p = running; p != nullptr; p = p->next) {
                if (p != cur_routine && p->awaiting == nullptr) { // find any ready routine != cur_routine
                    routine = p;
                    break;
                }
            }

            // if deadlock
            if (routine == nullptr && cur_routine->awaiting != nullptr) {
                assert(0); // checked for deadlock before
            }

            // if only cur_routine remains
            if (routine == nullptr) {
                routine = cur_routine;
            }
        }
    }

    if (routine->callee != nullptr && routine->callee == cur_routine) {
        routine->callee = routine->callee->caller = nullptr;
    }

    while (routine->callee != nullptr) {
        routine = routine->callee;
    }

    // set new caller
    // recall: caller is a coroutine which invoked "routine" via yield() or sched()
    routine->caller = cur_routine;

    cur_routine = routine;
    Restore(*routine);
}

} // namespace Coroutine
