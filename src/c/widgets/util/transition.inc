// Cogito transition system — per-frame value animators with easing.
// Each transition interpolates from→to over a duration, calling a Yis callback each frame.
// Transitions are driven from the main loop's draw cycle.

#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

typedef struct CogitoTransition {
  double from;
  double to;
  double duration_sec;
  int ease;
  double start_time;
  YisFn *on_update;   // called each frame with (value)
  YisFn *on_complete;  // called once when finished (optional)
  bool finished;
  struct CogitoTransition *next;
} CogitoTransition;

static CogitoTransition *cogito_transition_head = NULL;
static int cogito_transition_count = 0;

static int cogito_transition_start(double from, double to, int duration_ms,
                                    int ease, YisFn *on_update,
                                    YisFn *on_complete) {
  CogitoTransition *t =
      (CogitoTransition *)calloc(1, sizeof(CogitoTransition));
  t->from = from;
  t->to = to;
  t->duration_sec = duration_ms / 1000.0;
  t->ease = ease;
  t->start_time = cogito_now();
  t->on_update = on_update;
  t->on_complete = on_complete;
  t->finished = false;

  if (on_update)
    yis_retain_val(YV_FN(on_update));
  if (on_complete)
    yis_retain_val(YV_FN(on_complete));

  // prepend
  t->next = cogito_transition_head;
  cogito_transition_head = t;
  cogito_transition_count++;

  // keep animation loop alive
  double end_time = t->start_time + t->duration_sec;
  if (end_time > cogito_anim_end_time)
    cogito_anim_end_time = end_time + 0.05;

  return cogito_transition_count; // return ID (not strictly needed)
}

// Tick all transitions. Returns number still running.
static int cogito_transition_tick(void) {
  if (!cogito_transition_head)
    return 0;

  double now = cogito_now();
  int running = 0;

  CogitoTransition **pp = &cogito_transition_head;
  while (*pp) {
    CogitoTransition *t = *pp;
    double elapsed = now - t->start_time;
    double raw_t = (t->duration_sec > 0.0)
                       ? (elapsed / t->duration_sec)
                       : 1.0;

    if (raw_t >= 1.0) {
      raw_t = 1.0;
      t->finished = true;
    }

    double eased = cogito_ease(raw_t, t->ease);
    double value = t->from + (t->to - t->from) * eased;

    if (t->on_update) {
      YisVal args[1] = {YV_FLOAT(value)};
      cogito_invoke(t->on_update, 1, args);
    }

    if (t->finished) {
      if (t->on_complete) {
        cogito_invoke(t->on_complete, 0, NULL);
        yis_release_val(YV_FN(t->on_complete));
      }
      if (t->on_update)
        yis_release_val(YV_FN(t->on_update));
      *pp = t->next;
      free(t);
      cogito_transition_count--;
    } else {
      running++;
      pp = &(*pp)->next;
    }
  }

  return running;
}

static void cogito_transition_cancel_all(void) {
  CogitoTransition *t = cogito_transition_head;
  while (t) {
    CogitoTransition *next = t->next;
    if (t->on_update)
      yis_release_val(YV_FN(t->on_update));
    if (t->on_complete)
      yis_release_val(YV_FN(t->on_complete));
    free(t);
    t = next;
  }
  cogito_transition_head = NULL;
  cogito_transition_count = 0;
}
