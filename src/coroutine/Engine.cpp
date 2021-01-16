#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char stack_pos;
    char *top = &stack_pos;


    ctx.High = std::max(top, ctx.High);
    ctx.Low = std::min(top, ctx.Low);
    size_t size = ctx.High - ctx.Low;

    auto &buf = std::get<0>(ctx.Stack);
    auto &new_size = std::get<1>(ctx.Stack);
    if (size > new_size) {
        delete [] buf;
        buf = new char[size];
        new_size = size;
    }
    memcpy(buf, ctx.Low, size);
}

void Engine::Restore(context &ctx) {
    char stack_pos;
    if (&stack_pos >= ctx.Low && &stack_pos <= ctx.High) {
        Restore(ctx);
    }

    memcpy(ctx.Low, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    context *todo = alive;
    if (todo == cur_routine && todo != nullptr) {
        todo = todo->next;
    }

    if (todo == nullptr) {
        return;
    }

    sched(todo);
}

void Engine::sched(void *routine) {
    context *ctx = (context*) routine;

    if (cur_routine != nullptr) {
        if (setjmp(cur_routine->Environment) > 0) {
            return;
        }
        Store(*cur_routine);
    }

    cur_routine = ctx;
    Restore(*cur_routine);
}

} // namespace Coroutine
} // namespace Afina

