#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

#define THREAD_PRIO 7
#define ITERATION 100000 

static volatile uint32_t count = 0;
static K_MUTEX_DEFINE(counter_mutex);

static inline void inc_slowly() {
        k_mutex_lock(&counter_mutex, K_FOREVER);
        volatile int local_count = count;
        k_busy_wait(1);
        count = local_count +1;
        k_mutex_unlock(&counter_mutex);
}

volatile void t_thread_fn(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < ITERATION; i++) {
        inc_slowly();
    }

    return;
}




K_THREAD_DEFINE(T_1, STACK_SIZE, t_thread_fn, NULL, NULL, NULL, THREAD_PRIO, 0, 0);
K_THREAD_DEFINE(T_2, STACK_SIZE, t_thread_fn, NULL, NULL, NULL, THREAD_PRIO, 0, 0);

int main(void)
{
    int64_t time = k_uptime_get();

    k_thread_join(T_1, K_FOREVER);

    k_thread_join(T_2, K_FOREVER);

    LOG_INF("Elapse time: %lld ms", k_uptime_delta(&time));

    LOG_INF("Counter is: %lu", count);

    return 0;
}

