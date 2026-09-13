#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <liburing.h>
#include <unistd.h>
#include <valgrind/valgrind.h>
#include "task.h"
#include "logger.h"

#define STB_Q_IMPLEMENTATION
#include "macros.h"


static void *main_rsp = NULL;
static Task *current_task = NULL;
static Tasks ready_queue = {0};
static struct io_uring ring;
static bool runtime_running = 0;

void task_yield() {
    context_switch(&current_task->rsp, main_rsp);
}


void task_finish() {
    current_task->status = STATUS_DEAD;
    VALGRIND_STACK_DEREGISTER(current_task->valgrind_stack_id);
    task_yield();
    assert("Task survived yield after marked dead");
}

void task_init(void (*f)(void*), void *arg) {
    void *stack_base = malloc(STACK_CAPACITY);

    void **rsp = (void**)((char*)stack_base + STACK_CAPACITY);
    *(--rsp) = task_finish;
    *(--rsp) = f;
    *(--rsp) = arg;
    *(--rsp) = 0;
    *(--rsp) = 0;  
    *(--rsp) = 0;  
    *(--rsp) = 0;  
    *(--rsp) = 0;  
    *(--rsp) = 0;

    Task *new_task = malloc(sizeof(Task));
    assert(new_task != NULL && "Out of RAM");

    new_task->rsp = rsp;
    new_task->stack_base = stack_base;
    new_task->status = STATUS_RUNNABLE;
    new_task->io_result = 0;
    new_task->valgrind_stack_id = VALGRIND_STACK_REGISTER(stack_base, (char*)stack_base + STACK_CAPACITY);
    q_push(&ready_queue, new_task);
}

void runtime_clear() {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (sqe) {
        io_uring_prep_cancel(sqe, NULL, IORING_ASYNC_CANCEL_ALL | IORING_ASYNC_CANCEL_ANY);
        io_uring_submit(&ring);        
    }
    struct io_uring_cqe *cqe;
    while (io_uring_peek_cqe(&ring, &cqe) == 0) {
        Task *req = (Task*)io_uring_cqe_get_data(cqe);
        if (req) {
            free(req->stack_base);
            free(req);
        }
        io_uring_cqe_seen(&ring, cqe);
    }
    io_uring_queue_exit(&ring);
}



void runtime_run() {
    struct io_uring_cqe *cqe;
    int ret;
    while (runtime_running) {
        if (!q_empty(&ready_queue)) {
            q_pop(&ready_queue, current_task);
            context_switch(&main_rsp, current_task->rsp);

            if (current_task->status == STATUS_DEAD) {
                free(current_task->stack_base);
                free(current_task);
                current_task = NULL;
            }
            continue;
        }
        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            if (ret == -EINTR) continue;
            break;
        }

        Task *ready_task = (Task*)io_uring_cqe_get_data(cqe);
        if (ready_task != NULL) {
            ready_task->io_result = cqe->res;     // Yielded task will return on next instruction after yield
            ready_task->status = STATUS_RUNNABLE; // and there you want to check cqe->res through task->io_result
            q_push(&ready_queue, ready_task);
        }
        io_uring_cqe_seen(&ring, cqe);
    }
    runtime_clear();
    free(ready_queue.items);
}

bool runtime_check() {
    return runtime_running;
}


void runtime_init() {
    current_task = NULL;
    main_rsp = NULL;
    runtime_running = 1;

    int ret = io_uring_queue_init(256, &ring, 0);
    
    if (ret < 0) {
        LOG_ERROR("Couldn't init io_uring: %s", strerror(-ret));
        exit(1);
    }

}

void runtime_stop() {
    runtime_running = 0;
}

ssize_t async_read_file(int fd, void *buf, size_t len, uint64_t offset) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) return -1;

    io_uring_prep_read(sqe, fd, buf, len, offset);
    io_uring_sqe_set_data(sqe, current_task);
    io_uring_submit(&ring);

    current_task->status = STATUS_BLOCKED;
    task_yield();
    
    if (current_task->io_result < 0) {
        errno = -current_task->io_result;
        return -1;
    }
    return current_task->io_result;
}


ssize_t async_read_byte(char *c) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) return -1;

    io_uring_prep_read(sqe, STDIN_FILENO, c, 1, 0);
    io_uring_sqe_set_data(sqe, current_task);
    io_uring_submit(&ring);

    current_task->status = STATUS_BLOCKED;
    task_yield();

    if (current_task->io_result < 0) {
        errno = -current_task->io_result;
        return -1;
    }
    return current_task->io_result;
}

int async_timeout(double ms) { // Careful
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) return -1;
    struct __kernel_timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = (long long)(ms * 100000); // [BUG]: have no idea why the fuck ns = 10^5ms
    io_uring_prep_timeout(sqe, &ts, 0, 0); // based on animation tests, frames updating ~10times rare if ns = 10^6ms
    io_uring_sqe_set_data(sqe, current_task);
    io_uring_submit(&ring);
    
    current_task->status = STATUS_BLOCKED;
    task_yield();
    
    return 0;
}

int async_prep_waitid(int pid, siginfo_t *infop) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) return -1;
    memset(infop, 0, sizeof(siginfo_t));
    io_uring_prep_waitid(sqe, P_PID, (id_t)pid, infop, WEXITED | WUNTRACED, 0);
    io_uring_sqe_set_data(sqe, current_task);
    return 0;
}

int async_submit_waitids(size_t alive) {
    io_uring_submit(&ring);

    current_task->status = STATUS_BLOCKED;
    while (alive) {
        task_yield();
        if (current_task->io_result < 0) {
            errno = -current_task->io_result;
            return -1;
        }
        alive--;
    }
    return 0;
}

#undef STB_Q_IMPLEMENTATION
