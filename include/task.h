#ifndef TASK_H
#define TASK_H
#include <sys/wait.h>
#include <stdint.h>
#include <liburing.h>
extern void context_switch(void **old_rsp, void *new_rsp);

#ifndef STACK_CAPACITY
#define STACK_CAPACITY (16 * 4096)
#endif

#define IO_URING_QUEUE_DEPTH 256

typedef enum {
    STATUS_BLOCKED,
    STATUS_RUNNABLE,
    STATUS_DEAD
} Status;

typedef struct {
    void *rsp;
    void *stack_base;
    int32_t io_result;
    Status status;
    int valgrind_stack_id;
} Task;

typedef struct {
    Task **items;
    size_t count;
    size_t capacity;
    size_t head;
    size_t tail;
} Tasks;

void task_finish();
void task_init(void (*f)(void*), void *arg);
void runtime_init();
void runtime_stop();
bool runtime_check();
void runtime_run();
void task_yield();
ssize_t async_read_file(int fd, void *buf, size_t len, uint64_t offset);
ssize_t async_read_byte(char *c);
int async_timeout(double ms);
int async_prep_waitid(int pid, siginfo_t *infop);
int async_submit_waitids(size_t alive);
#endif //TASK_H
