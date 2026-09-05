#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#define STACK_SIZE 2048
#define SENSOR_COUNT 10
#define SENSOR_PERIOD_MS 100

LOG_MODULE_REGISTER(l4task1);

struct sensor_data {
  int32_t sample_value;
  uint32_t timestamp_ms;
  uint8_t seq;
};

static void display_listener_cb(const struct zbus_channel *chan);

// Add one listener for fast display updates
ZBUS_LISTENER_DEFINE(display_lis, display_listener_cb);

// Add one subscriber for slower logging
ZBUS_MSG_SUBSCRIBER_DEFINE(logger_sub);

ZBUS_CHAN_DEFINE(sensor_chan, struct sensor_data, NULL, NULL,
                 ZBUS_OBSERVERS(display_lis, logger_sub),
                 ZBUS_MSG_INIT(.sample_value = 0, .timestamp_ms = 0, .seq = 0));

/* ================================================================== */
/*  Listener - display                                                */
/* ================================================================== */

static void display_listener_cb(const struct zbus_channel *chan) {
  const struct sensor_data *msg =
      (const struct sensor_data *)zbus_chan_const_msg(chan);

  LOG_INF("[DISPLAY-LIS] thread=%s seq=%u temp=%d mC",
          k_thread_name_get(k_current_get()), msg->seq, msg->sample_value);
}

/* ================================================================== */
/* Sensor simulator                                                   */
/* ================================================================== */

static void sensor_thread_fn(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  k_thread_name_set(k_current_get(), "sensor");

  for (int i = 0; i < SENSOR_COUNT; i++) {
    struct sensor_data data = {
        .sample_value = 24000 + (i * 350),
        .timestamp_ms = k_uptime_get_32(),
        .seq = (uint8_t)i,
    };

    LOG_INF("[SENSOR] publish seq=%u temp=%d mC", data.seq, data.sample_value);

    int ret = zbus_chan_pub(&sensor_chan, &data, K_MSEC(100));
    if (ret != 0) {
      LOG_WRN("[SENSOR] publish failed ret=%d", ret);
    }

    k_msleep(SENSOR_PERIOD_MS);
  }

  LOG_INF("[SENSOR] done");
}

/* ================================================================== */
/*  Message subscriber - logger                                       */
/* ================================================================== */

static void logger_thread_fn(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  k_thread_name_set(k_current_get(), "logger");

  const struct zbus_channel *chan;
  int received = 0;

  while (received < SENSOR_COUNT) {
    struct sensor_data msg;
    int ret = zbus_sub_wait_msg(&logger_sub, &chan, &msg, K_MSEC(1500));
    if (ret != 0) {
      LOG_WRN("[LOGGER-MSG] timeout ret=%d", ret);
      break;
    }

    received++;

    LOG_INF("[LOGGER-MSG] thread=%s seq=%u temp=%d latency=%ums",
            k_thread_name_get(k_current_get()), msg.seq, msg.sample_value,
            k_uptime_get_32() - msg.timestamp_ms);
    k_msleep(350);
  }
  LOG_INF("[LOGGER-MSG] done received=%d", received);
}

/* ================================================================== */
/*  Threads                                                           */
/* ================================================================== */

K_THREAD_DEFINE(sensor_thread, STACK_SIZE, sensor_thread_fn, NULL, NULL, NULL,
                5, 0, 0);

K_THREAD_DEFINE(logger_thread, STACK_SIZE, logger_thread_fn, NULL, NULL, NULL,
                6, 0, 0);

/* ================================================================== */
/*  Main                                                              */
/* ================================================================== */

int main(void) {
  LOG_INF("=== L4 Task 1: Zbus Pub-Sub ===");
  LOG_INF("sensor publishes every %dms", SENSOR_PERIOD_MS);
  LOG_INF("display listener runs in publisher context");
  LOG_INF("logger uses message subscriber copies");
  return 0;
}