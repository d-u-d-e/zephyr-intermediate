#include "zephyr/task_wdt/task_wdt.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define APP_LOG_LEVEL LOG_LEVEL_DBG
#define CONSUMER_STACK_SIZE 1024
#define PRODUCER_STACK_SIZE 1024

LOG_MODULE_REGISTER(l5task1, APP_LOG_LEVEL);

#define SAMPLE_COUNT 12
#define SENSOR_QUEUE_DEPTH 8
#define HEALTH_STACK_SIZE 1024

struct sensor_sample {
  uint32_t seq;
  int32_t value;
  uint32_t timestamp_ms;
};

struct processed_sample {
  uint32_t seq;
  int32_t value;
  uint32_t latency_ms;
};

K_MSGQ_DEFINE(sensor_queue, sizeof(struct sensor_sample), SENSOR_QUEUE_DEPTH,
              4);

static void producer_fn(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  for (uint32_t seq = 0; seq < SAMPLE_COUNT; seq++) {
    struct sensor_sample sample = {
        .seq = seq,
        .value = 100 + (int32_t)seq,
        .timestamp_ms = k_uptime_get_32(),
    };

    LOG_DBG("[PRODUCER] acquired seq=%u value=%d timestamp=%u", sample.seq,
            sample.value, sample.timestamp_ms);

    int ret = k_msgq_put(&sensor_queue, &sample, K_MSEC(100));

    if (ret != 0) {
      LOG_WRN("[PRODUCER] queue full, dropped seq=%u", seq);
    } else {
      LOG_DBG("[PRODUCER] queued seq=%u used=%u/%u", seq,
              k_msgq_num_used_get(&sensor_queue), SENSOR_QUEUE_DEPTH);
    }

    k_msleep(30);
  }

  LOG_INF("[PRODUCER] done");
}

void wdt_callback(int id, void *user_data) 
{
    LOG_ERR("WDT CALLBACK: watchdog fired!");
}

static void consumer_fn(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  task_wdt_init(NULL);
  int id = task_wdt_add(250, wdt_callback, NULL);

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    struct sensor_sample sample;

    int ret = k_msgq_get(&sensor_queue, &sample, K_FOREVER);

    if (ret != 0) {
      LOG_ERR("[CONSUMER] receive failed: %d", ret);
      break;
    }

    struct processed_sample result = {
        .seq = sample.seq,
        .value = sample.value * 2,
        .latency_ms = k_uptime_get_32() - sample.timestamp_ms,
    };

    if (i == SAMPLE_COUNT - 1) {
      // simulate a stack consumer for this item number
      k_msleep(500);
    }

    LOG_DBG("[CONSUMER] seq=%u input=%d output=%d", sample.seq, sample.value,
            result.value);

    k_msleep(150);
    task_wdt_feed(id);
  }

  task_wdt_delete(id);
  LOG_INF("[CONSUMER] done");
}

/* ================================================================== */
/*  Health monitor                                                    */
/* ================================================================== */

static void health_fn(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  for (int i = 0; i < 6; i++) {
    k_msleep(60);

    uint32_t used = k_msgq_num_used_get(&sensor_queue);

    if (4 * used >= 3 * SENSOR_QUEUE_DEPTH) {
      LOG_WRN("[HEALTH] queue has %u free slots: <= 25%%",
              SENSOR_QUEUE_DEPTH - used);
    }
  }

  LOG_INF("[HEALTH] done");
}

/* ================================================================== */
/*  Threads                                                           */
/* ================================================================== */

K_THREAD_DEFINE(producer_thread, PRODUCER_STACK_SIZE, producer_fn, NULL, NULL,
                NULL, 5, 0, 0);

K_THREAD_DEFINE(consumer_thread, CONSUMER_STACK_SIZE, consumer_fn, NULL, NULL,
                NULL, 5, 0, 0);

K_THREAD_DEFINE(health_thread, HEALTH_STACK_SIZE, health_fn, NULL, NULL, NULL,
                6, 0, 0);

/* ================================================================== */
/*  Main                                                              */
/* ================================================================== */

int main(void) { return 0; }