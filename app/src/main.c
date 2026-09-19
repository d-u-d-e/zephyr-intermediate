#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 2048

#define MAINTENANCE_PRIORITY 4
#define EVENT_PERIOD_MS 250
#define MAINTENANCE_LOAD_US 45000

#define DEADLINE_MS 30

/* Set to 1 after proving that ready time causes the deadline miss. */
#ifndef FIX_WORKER_PRIORITY
#define FIX_WORKER_PRIORITY 1
#endif

#if FIX_WORKER_PRIORITY
#define CONTROL_PRIORITY 3
#else
#define CONTROL_PRIORITY 7
#endif

struct control_event {
  uint32_t seq;
  uint32_t ready_ms;
};

K_MSGQ_DEFINE(control_queue, sizeof(struct control_event), 4, 4);
K_SEM_DEFINE(maintenance_start, 0, 1);

/* ================================================================== */
/*  Timer expiry: creates one control event                           */
/* ================================================================== */

static void event_timer_expiry(struct k_timer *timer) {
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

  sys_trace_named_event("event_ready", event.seq,
                        k_msgq_num_used_get(&control_queue));

  /* Both threads become ready when the timer interrupt returns. */
  k_sem_give(&maintenance_start);
}

K_TIMER_DEFINE(event_timer, event_timer_expiry, NULL);

/* ================================================================== */
/*  Control thread                                                   */
/* ================================================================== */

static void control_fn(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  uint32_t deadline_misses = 0;

  while (true) {
    struct control_event event;
    int ret = k_msgq_get(&control_queue, &event, K_FOREVER);

    if (ret != 0) {
      LOG_ERR("[CONTROL] receive failed: %d", ret);
      continue;
    }

    uint32_t latency = k_uptime_get_32() - event.ready_ms;

    sys_trace_named_event("event_done", event.seq, latency);

    if (latency > DEADLINE_MS) {
      LOG_WRN_RATELIMIT("missed deadline, seq=%u, latency=%u, count=%u",
                        event.seq, latency, deadline_misses);
      deadline_misses++;
    } else {
      LOG_INF("[CONTROL] event_done seq=%u, latency=%u", event.seq, latency);
    }
  }
}

/* ================================================================== */
/*  Background maintenance thread                                    */
/* ================================================================== */

static void maintenance_fn(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  while (true) {
    k_sem_take(&maintenance_start, K_FOREVER);

    /* This work is important, but it has no short deadline. */
    k_busy_wait(MAINTENANCE_LOAD_US);
  }
}

K_THREAD_DEFINE(control, STACK_SIZE, control_fn, NULL, NULL, NULL,
                CONTROL_PRIORITY, 0, 0);

K_THREAD_DEFINE(maintenance, STACK_SIZE, maintenance_fn, NULL, NULL, NULL,
                MAINTENANCE_PRIORITY, 0, 0);

int main(void) {
  LOG_INF("=== L6 Homework: Runtime Investigation ===");
  LOG_INF("Control work must start within 10 ms");
  LOG_INF("Inspect, measure, trace, explain, and correct the delay");

  k_timer_start(&event_timer, K_MSEC(500), K_MSEC(EVENT_PERIOD_MS));

  return 0;
}

