#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

#define PRIO_LOW 7
#define PRIO_MED 5
#define PRIO_HIGH 3
#define PRIO_COOP -1


void t_low_fn(void *p1, void *p2, void *p3)
{
    const char *name = k_thread_name_get(k_current_get());
    while (1) {
        LOG_INF("%s running", name);
        k_msleep(300);
    }
}

void t_med_fn(void *p1, void *p2, void *p3)
{
    const char *name = k_thread_name_get(k_current_get());
    while (1) {
        LOG_INF("%s running", name);
        k_msleep(200);
    }
}

void t_high_fn(void *p1, void *p2, void *p3)
{
    const char *name = k_thread_name_get(k_current_get());
    while (1) {
        LOG_INF("%s running", name);
        k_msleep(100);
    }
}

void t_coop_fn(void *p1, void *p2, void *p3) 
{
    const char *name = k_thread_name_get(k_current_get());

    while (1) {
        for (int i = 0; i < 5; i++) {
           LOG_INF("%s iteration %d", name, i); 
        }
        // k_yield(); // this never let lower priority task to run...
        k_msleep(50);
    }
}

K_THREAD_DEFINE(T_LOW, STACK_SIZE, t_low_fn, NULL, NULL, NULL, PRIO_LOW, 0, 0);
K_THREAD_DEFINE(T_MED, STACK_SIZE, t_med_fn, NULL, NULL, NULL, PRIO_MED, 0, 0);
K_THREAD_DEFINE(T_HIGH, STACK_SIZE, t_high_fn, NULL, NULL, NULL, PRIO_HIGH, 0, 0);
K_THREAD_DEFINE(T_COOP, STACK_SIZE, t_coop_fn, NULL, NULL, NULL, PRIO_COOP, 0, 1000);



int main(void)
{
    return 0;
}

