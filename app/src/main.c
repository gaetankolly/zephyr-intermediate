/*
 * Lecture 3 - Homework Starter Code
 *
 * GOAL: Convert a polling loop to an event-driven workqueue architecture.
 *
 * The starter code works but is INEFFICIENT.
 * polling_thread wakes every 10ms to check a flag.
 * sensor_sim fires every 100ms - that's 10 wasted wake-ups per event.
 *
 *
 * ================================================================
 * TASKS
 * ================================================================
 *
 * TASK 1 (starter - already works, just run it):
 *   Run the starter. Count wake-ups vs real events in the log.
 *   Expected: ~10 wake-ups per sensor event. Confirm this.
 *
 * TASK 2 (implement):
 *   Replace polling_thread with a k_work handler.
 *   sensor_sim should call k_work_submit() instead of setting a flag.
 *   The handler should do what polling_thread currently does.
 *
 *   Steps:
 *   - Define a work item with K_WORK_DEFINE
 *   - Write the handler function
 *   - In sensor_sim: call k_work_submit() (remove k_sem_give + flag)
 *   - Remove the polling_thread entirely
 *
 * TASK 3 (verify):
 *   Add k_uptime_get_32() to your handler's LOG_INF.
 *   Confirm handler runs only when sensor_sim fires (every ~100ms).
 *   No unnecessary wake-ups.
 *
 * BONUS (debounce):
 *   Change sensor_sim to fire 5 events within 20ms (not 1 per 100ms).
 *   Use k_work_reschedule with 30ms delay so only ONE handler
 *   call occurs after the burst - not 5.
 *   Log the reschedule timestamps to confirm the burst collapses.
 *
 * ================================================================
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

#define STACK_SIZE    1024
#define SENSOR_MS     100    /* sensor fires every 100ms */
#define POLL_MS       10     /* polling consumer checks every 10ms */
#define EVENT_COUNT   10     /* total sensor events to produce */

/* ================================================================
 * STARTER CODE -- inefficient polling version
 * Run this first, then replace with workqueue in Task 2.
 * ================================================================ */

/* Statistics */
static int total_events;
static int total_wakeups;
static int total_processed;

/* ------------------------------------------------------------------ */
/*  polling_thread - checks flag every 10ms                          */
/*                                                                     */
/*  TASK 2: Replace this entire function + thread with a k_work       */
/*  handler. The handler body is the same as what's inside the        */
/*  if (sensor_flag) block below.                                      */
/* ------------------------------------------------------------------ */
static void sensor_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    total_processed++;

    /*
        * This is the "real work". In Task 2 this goes into
        * the k_work handler body.
        */
    LOG_INF("[CONSUMER] processed event %d  wakeups_so_far=%d  tick=%u",
            total_processed, total_wakeups,
            k_uptime_get_32());
}

K_WORK_DEFINE(sensor_work, sensor_work_handler);


/* ------------------------------------------------------------------ */
/*  sensor_sim - fires EVENT_COUNT events, 100ms apart               */
/* ------------------------------------------------------------------ */

static void sensor_sim_fn(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < EVENT_COUNT; i++) {
        k_msleep(SENSOR_MS);

        total_events++;
        LOG_INF("[SENSOR] event %d  tick=%u", i, k_uptime_get_32());


        int ret = k_work_submit(&sensor_work);
        if (ret < 0) { LOG_ERR("submit failed: %d", ret); }


        /*
         * BONUS: Replace the single k_msleep(SENSOR_MS) above with
         * a burst of 5 rapid events, then use k_work_reschedule in
         * the handler to collapse them to one execution.
         */
    }

    LOG_INF("[SENSOR] all events produced");
}


/* ------------------------------------------------------------------ */
/*  Threads                                                             */
/*                                                                     */
/*  TASK 2: Remove the polling_thread define. Add a K_WORK_DEFINE     */
/*  for your handler here instead.                                     */
/* ------------------------------------------------------------------ */

K_THREAD_DEFINE(sensor_thread,  STACK_SIZE, sensor_sim_fn, NULL, NULL, NULL, 5, 0, 0);

/* ================================================================
 * TASK 2 PLACEHOLDER - implement your solution here
 *
 * Uncomment and fill in:
 *
 * static void sensor_handler(struct k_work *work)
 * {
 *     ARG_UNUSED(work);
 *     total_processed++;
 *     LOG_INF("[HANDLER] processed event %d  tick=%u",
 *             total_processed, k_uptime_get_32());
 * }
 *
 * K_WORK_DEFINE(sensor_work, sensor_handler);
 *
 * BONUS PLACEHOLDER - for debounce:
 *
 * K_WORK_DELAYABLE_DEFINE(debounce_work, sensor_handler);
 * In sensor_sim: k_work_reschedule(&debounce_work, K_MSEC(30));
 * ================================================================ */

int main(void)
{
    LOG_INF("=== L3 Homework: Polling to Workqueue ===");
    LOG_INF("Starter: polling every %dms, sensor fires every %dms",
            POLL_MS, SENSOR_MS);
    LOG_INF("Expected wasted wakeups: ~%d per event",
            (SENSOR_MS / POLL_MS) - 1);
    LOG_INF("Run this, count wakeups, then convert to workqueue.");

    /* Wait long enough for all events to complete */
    k_msleep((EVENT_COUNT + 2) * SENSOR_MS + 500);

    return 0;
}

