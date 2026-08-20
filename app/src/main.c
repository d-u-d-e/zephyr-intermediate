#include "zephyr/toolchain.h"
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

/* Statistics */
static int total_events;
static int total_processed;

static void sensor_handler(struct k_work *work) {
  ARG_UNUSED(work);
  total_processed++;
  LOG_INF("[HANDLER] processed a new event, total so far: %d, tick=%u",
          total_processed, k_uptime_get_32());
}

K_WORK_DELAYABLE_DEFINE(work, sensor_handler);

static void sensor_sim_fn(void *p1, void *p2, void *p3) {

  // a burst of 5 events
  for (int j = 0; j < 5; j++) {
    total_events++;
    LOG_INF("[SENSOR] event %d  tick=%u", total_events, k_uptime_get_32());
    int ret = k_work_reschedule(&work, K_MSEC(30));
    if (ret < 0) {
      LOG_ERR("submit failed: %d", ret);
    }
    k_msleep(20);
  }

  LOG_INF("[SENSOR] all events produced");
}

K_THREAD_DEFINE(sensor_thread, STACK_SIZE, sensor_sim_fn, NULL, NULL, NULL, 5,
                0, 0);

int main(void) {

  /* Wait long enough for all events to complete */
  k_msleep(200);

  return 0;
}
