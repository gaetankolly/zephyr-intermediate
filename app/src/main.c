/*
 * Lecture 4 - homework - zbus
 *
 * GOAL: Convert a polling loop to an event-driven workqueue architecture.
 *
 * Tasks:
 *  - Create one zbus channel for sensor data
 *  - Publish simulated sensor samples every 100 ms
 *  - Add one listener for fast display updates
 *  - Add one subscriber for slower logging
 *
 * ================================================================
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

#define STACK_SIZE    1024
#define SENSOR_MS     100    /* sensor fires every 100ms */
#define NB_EVENT      10

// define data and callback
struct sensor_data {
    int32_t data;
    uint32_t timeStamp_ms;
};

static void display_listener_cb(const struct zbus_channel *chan);

// define ZBUS elements
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
static int event_nb;

void sensor_ISR_handler(struct k_timer *dummy)
{
    int* evt_nb = (int*) dummy->user_data;

    if (*evt_nb < NB_EVENT) 
    {
        struct sensor_data new_data;
        
        new_data.data = *evt_nb + 1000;
        new_data.timeStamp_ms = k_uptime_get_32();
        zbus_chan_pub(&sensor_chan, &new_data, K_MSEC(100));
        (*evt_nb)++;
    }
}

K_TIMER_DEFINE(sensor_ISR, sensor_ISR_handler, NULL);


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

        int ret = zbus_sub_wait(&logger_sub, &chan, K_MSEC(2000));
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

K_THREAD_DEFINE(logger_thread, STACK_SIZE, logger_thread_fn,
                NULL, NULL, NULL, 6, 0, 0);


int main(void)
{
    LOG_INF("=== L4 Homework ===");
    
    event_nb = 0;
    k_timer_user_data_set(&sensor_ISR, &event_nb);
    k_timer_start(&sensor_ISR, K_MSEC(SENSOR_MS), K_MSEC(SENSOR_MS));

    return 0;
}

