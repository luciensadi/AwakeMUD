/**
* @file dg_event.cpp
* A small event system, here so that trigger scripts can use the `wait`
* command to pause mid-script and resume later.
*
* Part of the core tbaMUD source code distribution, which is a derivative
* of, and continuation of, CircleMUD.
*
* Original DG Scripts authors:
* $Author: Mark A. Heilpern/egreen/Welcor $
* $Date: 2004/10/11 12:07:00$
* $Revision: 1.0.14 $
*
* Ported to AwakeMUD CE by Fizban.
*/

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "comm.hpp"
#include "dg_event.hpp"
#include "dg_scripts.hpp"

/** The queue every event lands on. */
static struct dg_queue *event_q = NULL;

/** Stepped once per event_process(), which the heartbeat calls every pulse. */
long dg_pulse = 0;

/** Create the main event queue. */
void event_init(void)
{
  event_q = queue_init();
}

/** Create an event and enqueue it.
 * @param func fires when the event comes up, and is handed event_obj.
 * @param event_obj optional payload for func to cast; may be NULL.
 * @param when pulses from now until the event fires.
 */
struct dg_event *event_create(EVENTFUNC(*func), void *event_obj, long when)
{
  struct dg_event *new_event;

  if (when < 1) /* make sure it's in the future */
    when = 1;

  new_event = new dg_event;
  new_event->func = func;
  new_event->event_obj = event_obj;
  new_event->q_el = queue_enq(event_q, new_event, when + dg_pulse);

  return new_event;
}

/** Dequeue an event and free it. */
void event_cancel(struct dg_event *event)
{
  if (!event) {
    log("SYSERR: Attempted to cancel a NULL event");
    return;
  }

  if (!event->q_el) {
    log("SYSERR: Attempted to cancel a non-NULL unqueued event, freeing anyway");
  } else {
    queue_deq(event_q, event->q_el);
  }

  if (event->event_obj)
    cleanup_event_obj(event);

  delete event;
}

/** Free an event's payload. */
void cleanup_event_obj(struct dg_event *event)
{
  /* Wait events own a plain wait_event_data; nothing else hangs off it. */
  delete static_cast<struct wait_event_data *>(event->event_obj);
  event->event_obj = NULL;
}

/** Fire every event whose time has come, re-enqueueing the ones that ask for
 * it. The heartbeat calls this once per pulse.
 */
void event_process(void)
{
  struct dg_event *the_event;
  long new_time;

  dg_pulse++;

  while (dg_pulse >= queue_key(event_q)) {
    if (!(the_event = (struct dg_event *) queue_head(event_q))) {
      log("SYSERR: Attempt to get a NULL event");
      return;
    }

    /* Null q_el so anything called beneath event_process can tell it is
     * running under the event function itself. */
    the_event->q_el = NULL;

    /* Call the event function, and re-enqueue if it asked for more time. */
    if ((new_time = (the_event->func)(the_event->event_obj)) > 0) {
      the_event->q_el = queue_enq(event_q, the_event, new_time + dg_pulse);
    } else {
      /* The event function has already dealt with event_obj. */
      delete the_event;
    }
  }
}

/** How many pulses from now this event fires. */
long event_time(struct dg_event *event)
{
  return queue_elmt_key(event->q_el) - dg_pulse;
}

/** Free every event on the queue. */
void event_free_all(void)
{
  if (event_q != NULL) {
    queue_free(event_q);
    event_q = NULL;
  }
}

/** Whether this event is still sitting on the queue. */
int event_is_queued(struct dg_event *event)
{
  return event->q_el ? 1 : 0;
}

/* ************************************************************************
*  Generic priority queue.                                                *
************************************************************************ */

/** Create an empty priority queue. */
struct dg_queue *queue_init(void)
{
  return new dg_queue;
}

/** Add data to a priority queue.
 * @param key when the element should be activated; also picks its bucket.
 */
struct q_element *queue_enq(struct dg_queue *q, void *data, long key)
{
  struct q_element *qe, *i;
  int bucket;

  qe = new q_element;
  qe->data = data;
  qe->key = key;

  bucket = key % NUM_EVENT_QUEUES;   /* which bucket does this go in */

  if (!q->head[bucket]) { /* bucket is empty */
    q->head[bucket] = qe;
    q->tail[bucket] = qe;
  } else {
    for (i = q->tail[bucket]; i; i = i->prev) {
      if (i->key < key) { /* found insertion point */
        if (i == q->tail[bucket]) {
          q->tail[bucket] = qe;
        } else {
          qe->next = i->next;
          i->next->prev = qe;
        }

        qe->prev = i;
        i->next = qe;
        break;
      }
    }

    if (i == NULL) { /* insertion point is the front of the list */
      qe->next = q->head[bucket];
      q->head[bucket] = qe;
      qe->next->prev = qe;
    }
  }

  return qe;
}

/** Remove queue element qe from priority queue q, and free qe. */
void queue_deq(struct dg_queue *q, struct q_element *qe)
{
  int i;

  assert(qe);

  i = qe->key % NUM_EVENT_QUEUES;

  if (qe->prev == NULL)
    q->head[i] = qe->next;
  else
    qe->prev->next = qe->next;

  if (qe->next == NULL)
    q->tail[i] = qe->prev;
  else
    qe->next->prev = qe->prev;

  delete qe;
}

/** Remove and return the data of the first element of q. */
void *queue_head(struct dg_queue *q)
{
  void *dg_data;
  int i;

  i = dg_pulse % NUM_EVENT_QUEUES;

  if (!q->head[i])
    return NULL;

  dg_data = q->head[i]->data;
  queue_deq(q, q->head[i]);
  return dg_data;
}

/** The key of the current head element, or LONG_MAX if there isn't one. */
long queue_key(struct dg_queue *q)
{
  int i;

  i = dg_pulse % NUM_EVENT_QUEUES;

  if (q->head[i])
    return q->head[i]->key;

  return LONG_MAX;
}

/** The key of queue element qe. */
long queue_elmt_key(struct q_element *qe)
{
  return qe->key;
}

/** Free q and everything on it. */
void queue_free(struct dg_queue *q)
{
  int i;
  struct q_element *qe, *next_qe;
  struct dg_event *event;

  for (i = 0; i < NUM_EVENT_QUEUES; i++) {
    for (qe = q->head[i]; qe; qe = next_qe) {
      next_qe = qe->next;
      if ((event = (struct dg_event *) qe->data) != NULL) {
        if (event->event_obj)
          cleanup_event_obj(event);

        delete event;
      }
      delete qe;
    }
  }

  delete q;
}
