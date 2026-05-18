#ifndef SRVROS_SCHEDULER_H
#define SRVROS_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

struct fpu_state;

typedef void (*scheduler_thread_fn)(void *arg);
typedef bool (*scheduler_wait_condition_fn)(void *arg);

struct scheduler_wait_queue {
    uint64_t waiters;
};

void scheduler_init(void);
bool scheduler_spawn(const char *name, scheduler_thread_fn entry, void *arg);
void scheduler_yield(void);
void scheduler_exit_current(void) __attribute__((noreturn));
void scheduler_preempt_tick(void);
void scheduler_kill_user_threads(void *process, void *except_user_thread_context);
bool scheduler_wait(struct scheduler_wait_queue *queue,
    scheduler_wait_condition_fn condition,
    void *arg);
bool scheduler_wait_timeout(struct scheduler_wait_queue *queue,
    scheduler_wait_condition_fn condition,
    void *arg,
    uint64_t deadline_ticks);
void scheduler_wake_all(struct scheduler_wait_queue *queue);
uint64_t scheduler_wake_count(struct scheduler_wait_queue *queue, uint64_t max_count);
void scheduler_set_user_context(void *process, uint64_t address_space, uint64_t kernel_stack_top);
void scheduler_set_user_context_fpu(void *process,
    uint64_t address_space,
    uint64_t kernel_stack_top,
    struct fpu_state *user_fpu);
void scheduler_clear_user_context(void);
void *scheduler_current_user_context(void);
void scheduler_set_user_thread_context(void *context);
void *scheduler_current_user_thread_context(void);
uint64_t scheduler_thread_count(void);
uint64_t scheduler_preemptions(void);
uint64_t scheduler_demo_counter(void);
bool scheduler_start_demo_worker(void);

#endif
