/*
 * Lecture 5 - homework - watchdog
 *
 * Goal: Study reliability under pressure
 * Task:
 * - Build a producer/consumer pipeline (from Lecture 4 homework)
 * - Add a task watchdog
 * - Simulate a stuck consumer (long k_sleep in consumer thread)
 * - Observe: queue fills, watchdog fires, callback triggered
 * - Add a health-check thread that monitors queue fill level and logs a warning at 75% capacity
 *
 * ================================================================
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/task_wdt/task_wdt.h>


LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

#define STACK_SIZE    1024
#define SENSOR_MS     100    /* sensor fires every 100ms */
#define NB_EVENT      20
#define SIMULATE_STUCK_EVENT_NB 10
#define SIMULATE_STUCK_DURATION 1000
#define WATCHDOG_TIMEOUT (SIMULATE_STUCK_DURATION-100)
#define LOG_WAIT_MAX_TIME 5000 
#define SENSOR_QUEUE_DEPTH 5



// define data and callback
struct sensor_data {
    int32_t data;
    uint32_t timeStamp_ms;
};

// define message queue
K_MSGQ_DEFINE(sensor_queue, sizeof(struct sensor_data), SENSOR_QUEUE_DEPTH, 4);

// define ZBUS elements
static void display_listener_cb(const struct zbus_channel *chan);

ZBUS_LISTENER_DEFINE(display_lis, display_listener_cb);
ZBUS_SUBSCRIBER_DEFINE(logger_sub, 4);

ZBUS_CHAN_DEFINE(
    sensor_chan,
    struct sensor_data,
    NULL, NULL,
    ZBUS_OBSERVERS(display_lis, logger_sub),
    ZBUS_MSG_INIT(.data = 0, .timeStamp_ms = 0)
);

// define timer
struct data_monitoring {
    uint32_t data_droped;
    uint32_t data_recieved;
};


K_SEM_DEFINE(sem_health_thread, 0, 1);

void sensor_ISR_handler(struct k_timer *dummy)
{
    struct data_monitoring* data_cnt = (struct data_monitoring*) dummy->user_data;

    if (data_cnt->data_recieved < NB_EVENT) 
    {
        struct sensor_data new_data;
        
        new_data.data = data_cnt->data_recieved + 1000;
        new_data.timeStamp_ms = k_uptime_get_32();

        int ret = k_msgq_put(&sensor_queue, &new_data, K_NO_WAIT);
        if (ret)
        {
            data_cnt->data_droped++;
        }
        data_cnt->data_recieved++;
        k_sem_give(&sem_health_thread);
    }
}

K_TIMER_DEFINE(sensor_ISR, sensor_ISR_handler, NULL);

// callback watchdog
static void watchdog_sensor_read_cb(int channel_id, void *user_data)
{
    k_tid_t stuck_thread = (k_tid_t)user_data;
    const char *name = k_thread_name_get(stuck_thread);

    LOG_ERR("[watchdog] Watchdog callback for thread: %s", name);
}

// define thread sensor read:
// - reading message queue from ISR, 
// - sending data for logging and displaying over zbus
static void sensor_read_thread_fn(void* p1, void* p2, void* p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    int simulate_stuck_cnt = 0;

    // init watchdog channel
    int wd_chan = task_wdt_add(WATCHDOG_TIMEOUT, watchdog_sensor_read_cb, (void *)k_current_get());

    while (1)
    {
        // feed watchdog
        task_wdt_feed(wd_chan); 

        // read incoming message
        struct sensor_data new_data;
        
        int ret = k_msgq_get(&sensor_queue, &new_data, K_MSEC(500));
        if (ret != 0) {
            LOG_ERR("[Sensor read] receive failed: %d", ret);
            break;
        } 
        else {
            LOG_INF("[Sensor read] Message in Queue : %lu", k_msgq_num_used_get(&sensor_queue));
        }

        // write to zbus
        ret = zbus_chan_pub(&sensor_chan, &new_data, K_MSEC(100));
        if (ret != 0) {
            LOG_WRN("[Sensor read] publish failed: %d", ret);
        }

        // force wait to slow down the reading 
        if (++simulate_stuck_cnt == SIMULATE_STUCK_EVENT_NB)
        {
            LOG_INF("[Sensor read] simulate stuck");
            k_msleep(SIMULATE_STUCK_DURATION);
            
        }
    }
}

// health thread, triggered by ISR when new data put to queue
static void health_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (1)
    {
        k_sem_take(&sem_health_thread, K_FOREVER);
        uint32_t used = k_msgq_num_used_get(&sensor_queue);
        LOG_INF("[HEALTH] sensor_queue=%lu/%u", used, SENSOR_QUEUE_DEPTH);

        if (((float) used) / ((float) SENSOR_QUEUE_DEPTH) >= 0.75) 
        {
            LOG_WRN("[HEALTH] queue has %u free slots", SENSOR_QUEUE_DEPTH - used);
        }
    }
}

// define displayer and logger
static void display_listener_cb(const struct zbus_channel *chan)
{
    const struct sensor_data* data = (const struct sensor_data *) zbus_chan_const_msg(chan);
    LOG_INF("Listener: %dx", data->data);
}

static void logger_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    k_thread_name_set(k_current_get(), "logger");

    const struct zbus_channel *chan;
    int received = 0;

    while (1) 
    {

        int ret = zbus_sub_wait(&logger_sub, &chan, K_MSEC(LOG_WAIT_MAX_TIME));
        if (ret != 0) {
            LOG_ERR("Sensor does not deliver data...");
            break;    
        }

        struct sensor_data msg;
        ret = zbus_chan_read(chan, &msg, K_NO_WAIT);
        if (ret != 0) {
            LOG_WRN("Logger read failed ret=%d", ret);
            continue;
        }

        LOG_INF("LOGGER thread=%s data=%u time=%d [ms], elapse time= %dms, event_count = %d",
                k_thread_name_get(k_current_get()),
                msg.data,
                msg.timeStamp_ms,
                (int)(k_uptime_get() - msg.timeStamp_ms),
                received++);
    }

    LOG_INF("LOGGER received=%d msg in total", received);
}


K_THREAD_DEFINE(sensor_thread, STACK_SIZE, sensor_read_thread_fn,
                NULL, NULL, NULL, 6, 0, 0);

K_THREAD_DEFINE(health_thread, STACK_SIZE, health_fn,
                NULL, NULL, NULL, 6, 0, 0);

K_THREAD_DEFINE(logger_thread, STACK_SIZE, logger_thread_fn,
                NULL, NULL, NULL, 6, 0, 0);


                


int main(void)
{
    LOG_INF("=== L4 Homework ===");

    task_wdt_init(NULL); // init watchdog driver


    struct data_monitoring data_cnt = {.data_droped=0, .data_recieved=0};

    k_timer_user_data_set(&sensor_ISR, &data_cnt);
    k_timer_start(&sensor_ISR, K_MSEC(SENSOR_MS), K_MSEC(SENSOR_MS));

    int ret = k_thread_join(logger_thread, K_FOREVER);
    if (ret == 0) 
    {
        LOG_INF("Data dropped form ISR: %d on %d", data_cnt.data_droped, data_cnt.data_recieved);
    }

    return 0;
}

