/**
* @file dg_event.hpp
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
* Ported to AwakeMUD CE by Fizban. The struct carries a dg_ prefix here
* because `event` is too generic a tag to claim in this codebase; every
* function keeps the name tbaMUD gave it.
*/
#ifndef _dg_event_hpp_
#define _dg_event_hpp_

/** Every function handled by the event system has this shape. */
#define EVENTFUNC(name) long (name)(void *event_obj)

/** An event. Events sit on the queue and fire when their turn comes up. */
struct dg_event {
  EVENTFUNC(*func);        /**< called when this event comes up      */
  void *event_obj;         /**< passed to func when func is called   */
  struct q_element *q_el;  /**< where this event sits in the queue   */

  dg_event() : func(NULL), event_obj(NULL), q_el(NULL) {}
};

/** Buckets in each queue. More buckets, cheaper enqueues. */
#define NUM_EVENT_QUEUES    10

/** The priority queue. */
struct dg_queue {
  struct q_element *head[NUM_EVENT_QUEUES]; /**< front of each bucket */
  struct q_element *tail[NUM_EVENT_QUEUES]; /**< rear of each bucket  */

  dg_queue()
  {
    for (int i = 0; i < NUM_EVENT_QUEUES; i++)
      head[i] = tail[i] = NULL;
  }
};

/** Queued elements. */
struct q_element {
  void *data;  /**< the event to be handled       */
  long key;    /**< when it should be handled     */
  struct q_element *prev, *next;

  q_element() : data(NULL), key(0), prev(NULL), next(NULL) {}
};

/* events */
void event_init(void);
struct dg_event *event_create(EVENTFUNC(*func), void *event_obj, long when);
void event_cancel(struct dg_event *event);
void event_process(void);
long event_time(struct dg_event *event);
void event_free_all(void);
void cleanup_event_obj(struct dg_event *event);
int  event_is_queued(struct dg_event *event);

/* queues */
struct dg_queue *queue_init(void);
struct q_element *queue_enq(struct dg_queue *q, void *data, long key);
void queue_deq(struct dg_queue *q, struct q_element *qe);
void *queue_head(struct dg_queue *q);
long queue_key(struct dg_queue *q);
long queue_elmt_key(struct q_element *qe);
void queue_free(struct dg_queue *q);

/** The event system's own pulse counter, stepped once per event_process(). */
extern long dg_pulse;

#endif /* _dg_event_hpp_ */
