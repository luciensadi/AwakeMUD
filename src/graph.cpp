/* ************************************************************************
 *   File: graph.c                                       Part of CircleMUD *
 *  Usage: various graph algorithms                                        *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *                                                                         *
 *  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 ************************************************************************ */

#include <stdio.h>
#include <stdlib.h>

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "constants.h"


/* Externals */

struct bfs_queue_struct
{
  vnum_t room;
  char dir;
  struct bfs_queue_struct *next;
};

static struct bfs_queue_struct *queue_head = 0, *queue_tail = 0;

/* Utility macros */
#define MARK(room) (world[room].room_flags.SetBit(ROOM_BFS_MARK))
#define UNMARK(room) (world[room].room_flags.RemoveBit(ROOM_BFS_MARK))
#define IS_MARKED(room) (world[room].room_flags.IsSet(ROOM_BFS_MARK))
#define TOROOM(x, y) (real_room(world[(x)].dir_option[(y)]->to_room->number))
#define IS_CLOSED(x, y) (IS_SET(world[(x)].dir_option[(y)]->exit_info, EX_CLOSED))

#define VALID_EDGE(x, y) (world[(x)].dir_option[(y)] && \
                          (TOROOM(x, y) != NOWHERE) &&  \
                          (!IS_CLOSED(x, y)) &&         \
                          (ROOM_FLAGGED(&world[x], ROOM_ROAD) || ROOM_FLAGGED(&world[x], ROOM_GARAGE)) && \
                          (!ROOM_FLAGGED(&world[x], ROOM_NOGRID)) && \
                          (!IS_MARKED(TOROOM(x, y))))

void bfs_enqueue(vnum_t room, char dir)
{
  struct bfs_queue_struct *curr;
  
  curr = new bfs_queue_struct;
  curr->room = room;
  curr->dir = dir;
  curr->next = 0;
  
  if (queue_tail) {
    queue_tail->next = curr;
    queue_tail = curr;
  } else
    queue_head = queue_tail = curr;
}


void bfs_dequeue(void)
{
  struct bfs_queue_struct *curr;
  
  curr = queue_head;
  
  if (!(queue_head = queue_head->next))
    queue_tail = 0;
  delete curr;
}


void bfs_clear_queue(void)
{
  while (queue_head)
    bfs_dequeue();
}


/* find_first_step: given a source room and a target room, find the first
 step on the shortest path from the source to the target.
 
 Intended usage: in mobile_activity, give a mob a dir to go if they're
 tracking another mob or a PC.  Or, a 'track' skill for PCs.
 */

int find_first_step(vnum_t src, vnum_t target)
{
  int curr_dir;
  vnum_t curr_room;
  
  if (src < 0 || src > top_of_world || target < 0 || target > top_of_world) {
    snprintf(buf3, sizeof(buf3), "Illegal value passed to find_first_step (graph.c). Expected constraints: 0 <= src %ld <= %ld; 0 <= target %ld <= %ld.",
            src, top_of_world, target, top_of_world);
    log(buf3);
    return BFS_ERROR;
  }
  if (src == target)
    return BFS_ALREADY_THERE;
  
  /* clear marks first */
  for (curr_room = 0; curr_room <= top_of_world; curr_room++)
    UNMARK(curr_room);
  
  MARK(src);
  
  /* first, enqueue the first steps, saving which direction we're going. */
  for (curr_dir = 0; curr_dir < NUM_OF_DIRS; curr_dir++)
    if (VALID_EDGE(src, curr_dir)) {
      MARK(TOROOM(src, curr_dir));
      bfs_enqueue(TOROOM(src, curr_dir), curr_dir);
    }
  /* now, do the classic BFS. */
  while (queue_head) {
    if (queue_head->room == target) {
      curr_dir = queue_head->dir;
      bfs_clear_queue();
      
      /* clear marks last */
      for (curr_room = 0; curr_room <= top_of_world; curr_room++)
        UNMARK(curr_room);
      
      return curr_dir;
    } else {
      for (curr_dir = 0; curr_dir < NUM_OF_DIRS; curr_dir++)
        if (VALID_EDGE(queue_head->room, curr_dir)) {
          MARK(TOROOM(queue_head->room, curr_dir));
          bfs_enqueue(TOROOM(queue_head->room, curr_dir), queue_head->dir);
        }
      bfs_dequeue();
    }
  }
  
  /* clear marks last */
  for (curr_room = 0; curr_room <= top_of_world; curr_room++)
    UNMARK(curr_room);
  
  return BFS_NO_PATH;
}
