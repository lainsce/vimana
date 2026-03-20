// Cogito async task system — thread pool + main-thread dispatch queue.
// Tasks run on a background thread (from a small pool). When done, result
// callbacks are dispatched on the main (UI) thread during the next frame.

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

// ---- Work queue item ----
typedef struct CogitoTaskItem {
  YisFn* work;     // background work function: () -> any
  YisFn* done;     // main-thread callback: (result) -> void
  YisVal result;    // filled by worker thread
  struct CogitoTaskItem* next;
} CogitoTaskItem;

// ---- Thread-safe FIFO ----
static CogitoTaskItem* cogito_task_pending_head = NULL;
static CogitoTaskItem* cogito_task_pending_tail = NULL;
static pthread_mutex_t cogito_task_pending_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cogito_task_pending_cond = PTHREAD_COND_INITIALIZER;

// Completed items to dispatch on main thread
static CogitoTaskItem* cogito_task_done_head = NULL;
static CogitoTaskItem* cogito_task_done_tail = NULL;
static pthread_mutex_t cogito_task_done_mtx = PTHREAD_MUTEX_INITIALIZER;

static bool cogito_task_pool_running = false;

#define COGITO_TASK_POOL_SIZE 4
static pthread_t cogito_task_threads[COGITO_TASK_POOL_SIZE];

// ---- Worker thread ----
static void* cogito_task_worker(void* arg) {
  (void)arg;
  while (1) {
    pthread_mutex_lock(&cogito_task_pending_mtx);
    while (!cogito_task_pending_head && cogito_task_pool_running) {
      pthread_cond_wait(&cogito_task_pending_cond, &cogito_task_pending_mtx);
    }
    if (!cogito_task_pool_running && !cogito_task_pending_head) {
      pthread_mutex_unlock(&cogito_task_pending_mtx);
      break;
    }
    CogitoTaskItem* item = cogito_task_pending_head;
    if (item) {
      cogito_task_pending_head = item->next;
      if (!cogito_task_pending_head) cogito_task_pending_tail = NULL;
    }
    pthread_mutex_unlock(&cogito_task_pending_mtx);

    if (!item) continue;

    // Execute work on background thread
    if (item->work) {
      item->result = item->work->fn(item->work->env, 0, NULL);
      yis_retain_val(item->result);
    } else {
      item->result = YV_NULLV;
    }

    // Enqueue result for main thread
    item->next = NULL;
    pthread_mutex_lock(&cogito_task_done_mtx);
    if (cogito_task_done_tail) {
      cogito_task_done_tail->next = item;
    } else {
      cogito_task_done_head = item;
    }
    cogito_task_done_tail = item;
    pthread_mutex_unlock(&cogito_task_done_mtx);

    // Wake SDL event loop so the idle wait doesn't block dispatch
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = SDL_EVENT_USER;
    SDL_PushEvent(&ev);
  }
  return NULL;
}

// ---- Public API ----

static void cogito_task_pool_init(void) {
  if (cogito_task_pool_running) return;
  cogito_task_pool_running = true;
  for (int i = 0; i < COGITO_TASK_POOL_SIZE; i++) {
    pthread_create(&cogito_task_threads[i], NULL, cogito_task_worker, NULL);
  }
}

// Submit a task: work() runs on background thread, done(result) runs on main thread.
static void cogito_task_run(YisFn* work, YisFn* done) {
  cogito_task_pool_init();
  CogitoTaskItem* item = (CogitoTaskItem*)calloc(1, sizeof(CogitoTaskItem));
  item->work = work;
  item->done = done;
  if (work) yis_retain_val(YV_FN(work));
  if (done) yis_retain_val(YV_FN(done));

  pthread_mutex_lock(&cogito_task_pending_mtx);
  if (cogito_task_pending_tail) {
    cogito_task_pending_tail->next = item;
  } else {
    cogito_task_pending_head = item;
  }
  cogito_task_pending_tail = item;
  pthread_cond_signal(&cogito_task_pending_cond);
  pthread_mutex_unlock(&cogito_task_pending_mtx);
}

// Drain completed tasks on the main thread. Called once per frame.
static void cogito_task_dispatch_done(void) {
  CogitoTaskItem* batch = NULL;
  pthread_mutex_lock(&cogito_task_done_mtx);
  batch = cogito_task_done_head;
  cogito_task_done_head = NULL;
  cogito_task_done_tail = NULL;
  pthread_mutex_unlock(&cogito_task_done_mtx);

  while (batch) {
    CogitoTaskItem* item = batch;
    batch = batch->next;

    if (item->done) {
      YisVal args[1] = { item->result };
      cogito_invoke(item->done, 1, args);
      yis_release_val(YV_FN(item->done));
    }
    if (item->work) yis_release_val(YV_FN(item->work));
    yis_release_val(item->result);
    free(item);
  }
}

static void cogito_task_pool_shutdown(void) {
  if (!cogito_task_pool_running) return;
  pthread_mutex_lock(&cogito_task_pending_mtx);
  cogito_task_pool_running = false;
  pthread_cond_broadcast(&cogito_task_pending_cond);
  pthread_mutex_unlock(&cogito_task_pending_mtx);
  for (int i = 0; i < COGITO_TASK_POOL_SIZE; i++) {
    pthread_join(cogito_task_threads[i], NULL);
  }
}
