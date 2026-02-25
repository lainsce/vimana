typedef enum {
  COGITO_TIMER_CB_ERGO = 0,
  COGITO_TIMER_CB_C
} CogitoTimerCallbackKind;

typedef void (*CogitoTimerUserFreeFn)(void* user);

typedef struct CogitoTimer {
  uint64_t id;
  double due_time;
  double interval_s;
  bool repeat;
  bool cancelled;
  bool running;
  CogitoNode* owner;
  CogitoTimerCallbackKind cb_kind;
  union {
    ErgoFn* ergo_fn;
    struct {
      cogito_timer_fn fn;
      void* user;
      CogitoTimerUserFreeFn user_free;
    } c;
  } cb;
  struct CogitoTimer* next;
} CogitoTimer;

static CogitoTimer* cogito_timers = NULL;
static uint64_t cogito_timer_next_id = 1;

static double cogito_timer_now(void) {
  return cogito_now();
}

static double cogito_timer_interval_seconds(uint32_t delay_ms, bool repeat) {
  double secs = (double)delay_ms / 1000.0;
  if (repeat && secs <= 0.0) secs = 0.001;
  if (secs < 0.0) secs = 0.0;
  return secs;
}

static uint64_t cogito_timer_take_id(void) {
  uint64_t id = cogito_timer_next_id++;
  if (id == 0) id = cogito_timer_next_id++;
  return id;
}

static void cogito_timer_release_callback(CogitoTimer* timer) {
  if (!timer) return;
  if (timer->cb_kind == COGITO_TIMER_CB_ERGO && timer->cb.ergo_fn) {
    ergo_release_val(EV_FN(timer->cb.ergo_fn));
    timer->cb.ergo_fn = NULL;
  } else if (timer->cb_kind == COGITO_TIMER_CB_C &&
             timer->cb.c.user_free && timer->cb.c.user) {
    timer->cb.c.user_free(timer->cb.c.user);
    timer->cb.c.user = NULL;
    timer->cb.c.user_free = NULL;
  }
}

static void cogito_timer_free(CogitoTimer* timer) {
  if (!timer) return;
  cogito_timer_release_callback(timer);
  free(timer);
}

static void cogito_timer_collect(void) {
  CogitoTimer** slot = &cogito_timers;
  while (*slot) {
    CogitoTimer* timer = *slot;
    if (timer->cancelled && !timer->running) {
      *slot = timer->next;
      timer->next = NULL;
      cogito_timer_free(timer);
      continue;
    }
    slot = &timer->next;
  }
}

static uint64_t cogito_timer_schedule_common(uint32_t delay_ms, bool repeat,
                                             CogitoTimerCallbackKind cb_kind,
                                             ErgoFn* ergo_fn, cogito_timer_fn c_fn,
                                             void* c_user,
                                             CogitoTimerUserFreeFn c_user_free,
                                             CogitoNode* owner) {
  if (cb_kind == COGITO_TIMER_CB_ERGO && !ergo_fn) return 0;
  if (cb_kind == COGITO_TIMER_CB_C && !c_fn) return 0;

  CogitoTimer* timer = (CogitoTimer*)calloc(1, sizeof(*timer));
  if (!timer) return 0;

  timer->id = cogito_timer_take_id();
  timer->repeat = repeat;
  timer->interval_s = cogito_timer_interval_seconds(delay_ms, repeat);
  timer->due_time = cogito_timer_now() + timer->interval_s;
  timer->owner = owner;
  timer->cb_kind = cb_kind;
  if (cb_kind == COGITO_TIMER_CB_ERGO) {
    timer->cb.ergo_fn = ergo_fn;
    ergo_retain_val(EV_FN(ergo_fn));
  } else {
    timer->cb.c.fn = c_fn;
    timer->cb.c.user = c_user;
    timer->cb.c.user_free = c_user_free;
  }

  timer->next = cogito_timers;
  cogito_timers = timer;
  return timer->id;
}

static uint64_t cogito_timer_schedule_ergo(uint32_t delay_ms, bool repeat,
                                           ErgoFn* fn) {
  return cogito_timer_schedule_common(delay_ms, repeat, COGITO_TIMER_CB_ERGO, fn,
                                      NULL, NULL, NULL, NULL);
}

static uint64_t cogito_timer_schedule_c(uint32_t delay_ms, bool repeat,
                                        cogito_timer_fn fn, void* user) {
  return cogito_timer_schedule_common(delay_ms, repeat, COGITO_TIMER_CB_C, NULL,
                                      fn, user, NULL, NULL);
}

static uint64_t cogito_timer_schedule_c_ex(uint32_t delay_ms, bool repeat,
                                           cogito_timer_fn fn, void* user,
                                           CogitoTimerUserFreeFn user_free) {
  return cogito_timer_schedule_common(delay_ms, repeat, COGITO_TIMER_CB_C, NULL,
                                      fn, user, user_free, NULL);
}

static uint64_t cogito_timer_schedule_c_owner(uint32_t delay_ms, bool repeat,
                                              cogito_timer_fn fn, void* user,
                                              CogitoNode* owner) {
  return cogito_timer_schedule_common(delay_ms, repeat, COGITO_TIMER_CB_C, NULL,
                                      fn, user, NULL, owner);
}

static uint64_t cogito_timer_schedule_c_owner_ex(uint32_t delay_ms, bool repeat,
                                                 cogito_timer_fn fn, void* user,
                                                 CogitoTimerUserFreeFn user_free,
                                                 CogitoNode* owner) {
  return cogito_timer_schedule_common(delay_ms, repeat, COGITO_TIMER_CB_C, NULL,
                                      fn, user, user_free, owner);
}

static bool cogito_timer_cancel_id(uint64_t timer_id) {
  if (timer_id == 0) return false;
  for (CogitoTimer* timer = cogito_timers; timer; timer = timer->next) {
    if (timer->id == timer_id && !timer->cancelled) {
      timer->cancelled = true;
      return true;
    }
  }
  return false;
}

static void cogito_timer_mark_all_cancelled(void) {
  for (CogitoTimer* timer = cogito_timers; timer; timer = timer->next) {
    timer->cancelled = true;
  }
}

static void cogito_timer_clear_all_internal(void) {
  cogito_timer_mark_all_cancelled();
  cogito_timer_collect();
}

static void cogito_timer_cancel_owner(CogitoNode* owner) {
  if (!owner) return;
  for (CogitoTimer* timer = cogito_timers; timer; timer = timer->next) {
    if (!timer->cancelled && timer->owner == owner) {
      timer->cancelled = true;
    }
  }
  cogito_timer_collect();
}

static void cogito_timer_cancel_owner_tree(CogitoNode* root) {
  if (!root) return;
  cogito_timer_cancel_owner(root);
  if (!root->children || root->len == 0) return;
  for (size_t i = 0; i < root->len; i++) {
    cogito_timer_cancel_owner_tree(root->children[i]);
  }
}

static bool cogito_timer_dispatch_due(void) {
  if (!cogito_timers) return false;
  bool fired = false;
  double now = cogito_timer_now();
  for (CogitoTimer* timer = cogito_timers; timer; timer = timer->next) {
    if (timer->cancelled) continue;
    if (now + 1e-9 < timer->due_time) continue;

    timer->running = true;
    if (timer->cb_kind == COGITO_TIMER_CB_ERGO) {
      if (timer->cb.ergo_fn) cogito_invoke(timer->cb.ergo_fn, 0, NULL);
    } else {
      if (timer->cb.c.fn) timer->cb.c.fn(timer->cb.c.user);
    }
    timer->running = false;
    fired = true;

    if (timer->cancelled) continue;
    if (timer->repeat) {
      double step = timer->interval_s;
      if (step <= 0.0) step = 0.001;
      timer->due_time += step;
      if (timer->due_time <= now) timer->due_time = now + step;
    } else {
      timer->cancelled = true;
    }
  }
  cogito_timer_collect();
  return fired;
}

static uint32_t cogito_timer_next_wait_ms(uint32_t fallback_ms) {
  cogito_timer_collect();
  if (!cogito_timers) return fallback_ms;

  double now = cogito_timer_now();
  double best_delta = -1.0;
  for (CogitoTimer* timer = cogito_timers; timer; timer = timer->next) {
    if (timer->cancelled) continue;
    double delta = timer->due_time - now;
    if (delta <= 0.0) return 0;
    if (best_delta < 0.0 || delta < best_delta) best_delta = delta;
  }
  if (best_delta < 0.0) return fallback_ms;

  uint32_t wait_ms = (uint32_t)ceil(best_delta * 1000.0);
  if (wait_ms > fallback_ms) wait_ms = fallback_ms;
  return wait_ms;
}

__attribute__((unused))
static ErgoVal cogito_timer_timeout(ErgoVal delayv, ErgoVal handlerv) {
  if (handlerv.tag != EVT_FN) ergo_trap("cogito.timer_timeout expects function");
  int64_t delay = ergo_as_int(delayv);
  if (delay < 0) delay = 0;
  uint64_t timer_id =
      cogito_timer_schedule_ergo((uint32_t)delay, false, handlerv.as.p);
  return EV_INT((int64_t)timer_id);
}

__attribute__((unused))
static ErgoVal cogito_timer_interval(ErgoVal delayv, ErgoVal handlerv) {
  if (handlerv.tag != EVT_FN) ergo_trap("cogito.timer_interval expects function");
  int64_t delay = ergo_as_int(delayv);
  if (delay < 0) delay = 0;
  uint64_t timer_id =
      cogito_timer_schedule_ergo((uint32_t)delay, true, handlerv.as.p);
  return EV_INT((int64_t)timer_id);
}

__attribute__((unused))
static ErgoVal cogito_timer_cancel(ErgoVal timer_id_v) {
  uint64_t timer_id = (uint64_t)ergo_as_int(timer_id_v);
  return EV_BOOL(cogito_timer_cancel_id(timer_id));
}

__attribute__((unused))
static void cogito_timer_cancel_all(void) { cogito_timer_clear_all_internal(); }
