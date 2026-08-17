#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(l1task1, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

#define PRIO_COOP (-1)
#define PRIO_HIGH 3
#define PRIO_MED 5
#define PRIO_LOW 7

void coop_fn(void *p1, void *p2, void *p3) {
  LOG_INF("[COOP] starting - will run 5 steps without yielding");

  int step = 0;
  for (int i = 0; i < 5; i++) {
    k_busy_wait(40000);
    step++;
    LOG_INF("[COOP] step %d/5 - still holding CPU", step);
  }

  k_yield();
}

void high_fn(void *p1, void *p2, void *p3) {
  while (true) {
    LOG_INF("T_HIGH running");
    k_msleep(100);
  }
}

void med_fn(void *p1, void *p2, void *p3) {
  while (true) {
    LOG_INF("T_MED running");
    k_msleep(200);
  }
}

void low_fn(void *p1, void *p2, void *p3) {
  while (true) {
    LOG_INF("T_LOW running");
    k_msleep(300);
  }
}

K_THREAD_DEFINE(t_coop, STACK_SIZE, coop_fn, NULL, NULL, NULL, PRIO_COOP, 0, 0);
K_THREAD_DEFINE(t_high, STACK_SIZE, high_fn, NULL, NULL, NULL, PRIO_HIGH, 0, 0);
K_THREAD_DEFINE(t_med, STACK_SIZE, med_fn, NULL, NULL, NULL, PRIO_MED, 0, 0);
K_THREAD_DEFINE(t_low, STACK_SIZE, low_fn, NULL, NULL, NULL, PRIO_LOW, 0, 0);

int main(void) { return 0; }
