#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/tracing/tracing.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_INF);

#define STACK_SIZE            2048
#define CONTROL_PRIORITY         4
#define MAINTENANCE_PRIORITY     7
#define EVENT_PERIOD_MS        250
#define MAINTENANCE_LOAD_US  45000
/* Define a response-time guarantee. */
#define CONTROL_DEADLINE_MS 10      

/* 
*  Homework task: Inspect thread priorities and states with the kernel shell:
*       with native_sim use connect to kernel using the pseudo UART /dev/pts/2 -> printed out at start of the message
*       then inspect with:
*           > kernel thread list
*  Homework task: Capture CTF and decode it with Babeltrace
*        copy tracing metadata: TSDL schema describing every Zephyr CTF event type and its fields
*           > cp ../deps/zephyr/subsys/tracing/ctf/tsdl/metadata trace_data/
*        Capture CTF events into channel0_0: binary stream recorded at runtime
*           >  ./build/zephyr/zephyr.exe -trace-file=trace_data/channel0_0
*        use babektrace:
*           > babeltrace trace_data/      
*/

struct control_event {
    uint32_t seq;
    uint32_t ready_ms;
};

K_MSGQ_DEFINE(control_queue, sizeof(struct control_event), 4, 4);
K_SEM_DEFINE(maintenance_start, 0, 1);

/* ================================================================== */
/*  Timer expiry: creates one control event                           */
/* ================================================================== */

static void event_timer_expiry(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    static uint32_t seq;
    struct control_event event = {
        .seq = seq++,
        .ready_ms = k_uptime_get_32(),
    };

    /* Timer expiry runs in interrupt context, so never wait here. */
    int ret = k_msgq_put(&control_queue, &event, K_NO_WAIT);

    if (ret != 0) {
        return;
    }

    /* Both threads become ready when the timer interrupt returns. */
    k_sem_give(&maintenance_start);

    /* Add an application trace event for this sequence. */
    sys_trace_named_event("Event_ready", event.seq, event.ready_ms);
}

K_TIMER_DEFINE(event_timer, event_timer_expiry, NULL);

/* ================================================================== */
/*  Control thread                                                   */
/* ================================================================== */

static void control_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    uint32_t deadlineMiss_cnt = 0;

    while (true) {
        struct control_event event;
        int ret = k_msgq_get(&control_queue, &event, K_FOREVER);

        if (ret != 0) {
            LOG_ERR("[CONTROL] receive failed: %d", ret);
            continue;
        }

        LOG_INF("[CONTROL] processed seq=%u", event.seq);

        /* Measure latency and count every deadline miss. */
        uint32_t latency_ms = k_uptime_get_32() - event.ready_ms;
        if (latency_ms > CONTROL_DEADLINE_MS) 
        {
            deadlineMiss_cnt++;
            /* Rate-limit repeated warning messages. */
            LOG_WRN_RATELIMIT("deadline_miss count=%u seq=%u latency_ms=%u", deadlineMiss_cnt, event.seq, latency_ms);
        }
        
        /* Add an application trace event for completion. */
        sys_trace_named_event("Event_done", event.seq, latency_ms);
    }
}

/* ================================================================== */
/*  Background maintenance thread                                    */
/* ================================================================== */

static void maintenance_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (true) {
        k_sem_take(&maintenance_start, K_FOREVER);

        /* This work is important, but it has no short deadline. */
        k_busy_wait(MAINTENANCE_LOAD_US);
    }
}

K_THREAD_DEFINE(control, STACK_SIZE, control_fn,
                NULL, NULL, NULL, CONTROL_PRIORITY, 0, 0);

K_THREAD_DEFINE(maintenance, STACK_SIZE, maintenance_fn,
                NULL, NULL, NULL, MAINTENANCE_PRIORITY, 0, 0);

int main(void)
{
    LOG_INF("=== L6 Homework: Runtime Investigation ===");
    LOG_INF("Control work must start within 10 ms");
    LOG_INF("Inspect, measure, trace, explain, and correct the delay");

    k_timer_start(&event_timer, K_MSEC(500), K_MSEC(EVENT_PERIOD_MS));

    return 0;
}
