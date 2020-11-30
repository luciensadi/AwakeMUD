/* ************************************************************************
 *   File: comm.c                                        Part of CircleMUD *
 *  Usage: Communication, socket handling, main(), central game loop       *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *                                                                         *
 *  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 ************************************************************************ */

#define __COMM_CC__
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <new>
#include <time.h>
#include <iostream>
using namespace std;

#if defined(WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#include <winsock.h>
#include <io.h>
#include <direct.h>
#include <mmsystem.h>
typedef SOCKET  socket_t;

#define chdir(x) _chdir(x)
#define random() rand()
#define srandom(x) srand(x)
#define close(x) _close(x)
#define write(x, y, z) _write(x, y, z)
#define read(x, y, z) _read(x, y, z)
#else
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netdb.h>
#endif

#include "telnet.h"
#include "awake.h"
#include "structs.h"
#include "utils.h"
#include "constants.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "newdb.h"
#include "house.h"
#include "memory.h"
#include "screen.h"
#include "newshop.h"
#include "quest.h"
#include "newmatrix.h"
#include "limits.h"
#include "protocol.h"
#include "perfmon.h"


const unsigned perfmon::kPulsePerSecond = PASSES_PER_SEC;

/* externs */
extern int restrict;
/* extern FILE *player_fl; */
extern int DFLT_PORT;
extern char *DFLT_DIR;
extern int MAX_PLAYERS;
extern int MAX_DESCRIPTORS_AVAILABLE;

extern struct time_info_data time_info; /* In db.c */
extern char help[];

/* local globals */
struct descriptor_data *descriptor_list = NULL; /* master desc list */
struct txt_block *bufpool = 0;  /* pool of large output buffers */
int buf_largecount = 0;         /* # of large buffers which exist */
int buf_overflows = 0;          /* # of overflows of output */
int buf_switches = 0;           /* # of switches from small to large buf */
int circle_shutdown = 0;        /* clean shutdown */
static int exit_code = SUCCESS;
int circle_reboot = 0;          /* reboot the game after a shutdown */
int no_specials = 0;            /* Suppress ass. of special routines */
int max_players = 0;            /* max descriptors available */
int tics = 0;                   /* for extern checkpointing */
int scheck = 0;                 /* for syntax checking mode */
extern int nameserver_is_slow;  /* see config.c */
struct timeval zero_time;       // zero-valued time structure, used to be
// called 'null_time' but that is kinda
// innacurate (=

static bool fCopyOver;
int  mother_desc;
int port;

int ViolencePulse = PULSE_VIOLENCE;

/* functions in this file */
int get_from_q(struct txt_q * queue, char *dest, int *aliased);
void init_game(int port);
void signal_setup(void);
void game_loop(int mother_desc);
int init_socket(int port);
int new_descriptor(int s);
int get_max_players(void);
int process_output(struct descriptor_data * t);
int process_input(struct descriptor_data * t);
void close_socket(struct descriptor_data * d);
struct timeval timediff(struct timeval * a, struct timeval * b);
void flush_queues(struct descriptor_data * d);
void nonblock(int s);
int perform_subst(struct descriptor_data * t, char *orig, char *subst);
int perform_alias(struct descriptor_data * d, char *orig);
void record_usage(void);
void make_prompt(struct descriptor_data * point);
void check_idle_passwords(void);
void init_descriptor (struct descriptor_data *newd, int desc);
char *colorize(struct descriptor_data *d, const char *str, bool skip_check = FALSE);
void send_keepalives();
void msdp_update();

/* extern fcnts */
extern void DBInit();
extern void DBFinalize();
void boot_world(void);
void zone_update(void);
void spec_update(void);
void point_update(void);        /* In limits.c */
void mobile_activity(void);
void string_add(struct descriptor_data * d, char *str);
void perform_violence(void);
void misc_update(void);
void decide_combat_pool(void);
int isbanned(char *hostname);
void matrix_violence(void);
void matrix_update(void);
void another_minute(void);
void weather_change(void);
void process_regeneration(int half_hour);
void MonorailProcess();
void EscalatorProcess();
void ElevatorProcess();
void taxi_leaves(void);
void johnson_update();
void check_idling(void);
void free_shop(struct shop_data *shop);
void free_quest(struct quest_data *quest);
void randomize_shop_prices(void);
void process_autonav(void);
void update_buildrepair(void);
void process_boost(void);
class memoryClass *Mem = new memoryClass();
void show_string(struct descriptor_data * d, char *input);

#ifdef USE_DEBUG_CANARIES
void check_memory_canaries();
#endif

#if (defined(WIN32) && !defined(__CYGWIN__))
void gettimeofday(struct timeval *t, struct timezone *dummy)
{
  DWORD millisec = GetTickCount();
  
  t->tv_sec = (int) (millisec / 1000);
  t->tv_usec = (millisec % 1000) * 1000;
}
#endif

/* *********************************************************************
 *  main game loop and related stuff                                    *
 ********************************************************************* */
#if defined(UNITTEST)
int game_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{  
  int pos = 1;
  char *dir;
  port = DFLT_PORT;
  dir = DFLT_DIR;
  while ((pos < argc) && (*(argv[pos]) == '-')) {
    switch (*(argv[pos] + 1)) {
      case 'd':
        if (*(argv[pos] + 2))
          dir = argv[pos] + 2;
        else if (++pos < argc)
          dir = argv[pos];
        else {
          log("Directory arg expected after option -d.");
          exit(1);
        }
        break;
      case 'c':
        scheck = 1;
        log("Syntax check mode enabled.");
        break;
      case 'r':
        restrict = 1;
        log("Restricting game -- no new players allowed.");
        break;
      case 'o':
        fCopyOver = TRUE;
        mother_desc = atoi(argv[pos]+2);
        break;
      case 's':
        no_specials = 1;
        log("Suppressing assignment of special routines.");
        break;
      default:
        log_vfprintf("SYSERR: Unknown option -%c in argument string.", *(argv[pos] + 1));
        break;
    }
    pos++;
  }
  
  if (pos < argc) {
    if (!isdigit(*argv[pos])) {
      fprintf(stderr, "Usage: %s [-c] [-m] [-q] [-r] [-s] [-d pathname] [port #]\n", argv[0]);
      exit(1);
    } else if ((port = atoi(argv[pos])) <= 1024) {
      fprintf(stderr, "Illegal port number. Port must be higher than 1024.\n");
      exit(1);
    }
  }
  
  if (chdir(dir) < 0) {
    perror("Fatal error changing to data directory");
    exit(1);
  }
  
  log_vfprintf("Using %s as data directory.", dir);
  
  if (scheck) {
    boot_world();
    log("Done.");
    exit(0);
  } else {
    log_vfprintf("Running game on port %d.", port);
    init_game(port);
  }
  
  return 0;
}
/* Reload players after a copyover */

void copyover_recover()
{
  struct descriptor_data *d;
  FILE *fp;
  char host[1024];
  char copyover_get[1024];
  bool fOld;
  char name[MAX_INPUT_LENGTH];
  int desc;
  
  log ("Copyover recovery initiated");
  
  fp = fopen (COPYOVER_FILE, "r");
  
  if (!fp) // there are some descriptors open which will hang forever then?
    
  {
    perror ("copyover_recover:fopen");
    log ("Copyover file not found. Exiting.\n\r");
    exit (1);
  }
  
  unlink (COPYOVER_FILE); // In case something crashes - doesn't prevent reading
  
  for (;;) {
    fOld = TRUE;
    fscanf (fp, "%d %s %s %s\n", &desc, name, host, copyover_get);
    if (desc == -1)
      break;
    
    /* Write something, and check if it goes error-free */
    if (write_to_descriptor (desc, "\n\rRestoring from copyover...\n\r") < 0) {
      close (desc); /* nope */
      continue;
    }
    
    /* create a new descriptor */
    CREATE(d, struct descriptor_data, 1);
    memset ((char *) d, 0, sizeof (struct descriptor_data));
    init_descriptor (d,desc); /* set up various stuff */
    
    strcpy(d->host, host);
    d->next = descriptor_list;
    descriptor_list = d;
    
    // Restore KaVir protocol data.
    CopyoverSet(d, copyover_get);
    
    
    d->connected = CON_CLOSE;
    
    /* Now, find the pfile */
    
    /* Why would you create a new character, then immediately discard it and load a new one from the DB? */
    //CREATE(d->character, struct char_data, 1);
    //clear_char(d->character);
    //CREATE(d->character->player_specials, struct player_special_data, 1);
    //d->character->desc = d;
    
    if ((d->character = playerDB.LoadChar(name, FALSE))) {
      d->character->desc = d;
      if (!PLR_FLAGGED(d->character, PLR_DELETED))
        PLR_FLAGS(d->character).RemoveBits(PLR_WRITING, PLR_MAILING, ENDBIT);
      else
        fOld = FALSE;
    } else
      fOld = FALSE;
    
    if (!fOld) /* Player file not found?! */
    {
      write_to_descriptor (desc, "\n\rSomehow, your character was lost in the copyover. Sorry.\n\r");
      close_socket (d);
    } else /* ok! */
    {
      long load_room;
      write_to_descriptor (desc, "\n\rCopyover recovery complete.\n\r");
      d->connected = CON_PLAYING;
      reset_char(d->character);
      d->character->next = character_list;
      character_list = d->character;
      if (real_room(GET_LAST_IN(d->character)) > 0)
        load_room = real_room(GET_LAST_IN(d->character));
      else
        load_room = real_room(GET_LOADROOM(d->character));
      char_to_room(d->character, &world[load_room]);
      //look_at_room(d->character, 0);
    }
  }
  fclose (fp);
  
  // Force all player characters to look now that everyone's properly loaded.
  struct char_data *plr = character_list;
  while (plr) {
    if (!IS_NPC(plr) && !PRF_FLAGGED(plr, PRF_SCREENREADER))
      look_at_room(plr, 0);
    plr = plr->next;
  }
}


/* Init sockets, run game, and cleanup sockets */
void init_game(int port)
{
  srandom(time(0));
  
  max_players = get_max_players();
  if (!fCopyOver) /* If copyover mother_desc is already set up */
  {
    log ("Opening mother connection.");
    mother_desc = init_socket (port);
  }
  
  DBInit();
  
  log("Signal trapping.");
  signal_setup();
  
  if (fCopyOver) /* reload players */
    copyover_recover();
  
  log("Entering game loop.");
  game_loop(mother_desc);
  
  log("Saving all players.");
  House_save_all();
  
  log("Closing all sockets.");
  while (descriptor_list)
    close_socket(descriptor_list);
  
  close(mother_desc);
  
  DBFinalize();
  
  if (circle_reboot) {
    log("Rebooting.");
    exit(52);                   /* what's so great about HHGTTG, anyhow? */
  }
  log("Normal termination of game.");
}

/*
 * init_socket sets up the mother descriptor - creates the socket, sets
 * its options up, binds it, and listens.
 */
int init_socket(int port)
{
  int s, opt;
  struct sockaddr_in sa;
  
  /*
   * Should the first argument to socket() be AF_INET or PF_INET?  I don't
   * know, take your pick.  PF_INET seems to be more widely adopted, and
   * Comer (_Internetworking with TCP/IP_) even makes a point to say that
   * people erroneously use AF_INET with socket() when they should be using
   * PF_INET.  However, the man pages of some systems indicate that AF_INET
   * is correct; some such as ConvexOS even say that you can use either one.
   * All implementations I've seen define AF_INET and PF_INET to be the same
   * number anyway, so ths point is (hopefully) moot.
   */
  
  if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Create socket");
    exit(1);
  }
#if defined(SO_SNDBUF)
  opt = LARGE_BUFSIZE + GARBAGE_SPACE;
  if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *) &opt, sizeof(opt)) < 0) {
    perror("setsockopt SNDBUF");
    exit(1);
  }
#endif
  
#if defined(SO_REUSEADDR)
  opt = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
    perror("setsockopt REUSEADDR");
    exit(1);
  }
#endif
  
#if defined(SO_REUSEPORT)
  opt = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (char *) &opt, sizeof(opt)) < 0) {
    perror("setsockopt REUSEPORT");
    exit(1);
  }
#endif
  
#if defined(SO_LINGER)
  {
    struct linger ld;
    
    ld.l_onoff = 0;
    ld.l_linger = 0;
    if (setsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&ld, sizeof(ld)) < 0) {
      perror("setsockopt LINGER");
      exit(1);
    }
  }
#endif
  
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  
  if (bind(s, (struct sockaddr *) & sa, sizeof(sa)) < 0) {
    perror("bind");
    close(s);
    exit(1);
  }
  nonblock(s);
  listen(s, 5);
  return s;
}

int get_max_players(void)
{
  int max_descs = 0;
  const char *method;
  
  /*
   * First, we'll try using getrlimit/setrlimit.  This will probably work
   * on most systems.
   */
#if defined (RLIMIT_NOFILE) || defined (RLIMIT_OFILE)
#if !defined(RLIMIT_NOFILE)
#define RLIMIT_NOFILE RLIMIT_OFILE
#endif
  
  {
    struct rlimit limit;
    
    method = "rlimit";
    if (getrlimit(RLIMIT_NOFILE, &limit) < 0)
    {
      perror("calling getrlimit");
      exit(1);
    }
    if (limit.rlim_max == RLIM_INFINITY)
      max_descs = MAX_PLAYERS + NUM_RESERVED_DESCS;
    else
      max_descs = MIN((unsigned)MAX_PLAYERS + NUM_RESERVED_DESCS, (unsigned)limit.rlim_max);
    
    limit.rlim_cur = max_descs;
    setrlimit(RLIMIT_NOFILE, &limit);
  }
#elif defined (OPEN_MAX) || defined(FOPEN_MAX)
#if !defined(OPEN_MAX)
#define OPEN_MAX FOPEN_MAX
#endif
  method = "OPEN_MAX";
  max_descs = OPEN_MAX;         /* Uh oh.. rlimit didn't work, but we have
                                 * OPEN_MAX */
#else
  /*
   * Okay, you don't have getrlimit() and you don't have OPEN_MAX.  Time to
   * use the POSIX sysconf() function.  (See Stevens' _Advanced Programming
   * in the UNIX Environment_).
   */
  method = "POSIX sysconf";
  errno = 0;
  if ((max_descs = sysconf(_SC_OPEN_MAX)) < 0) {
    if (errno == 0)
      max_descs = MAX_PLAYERS + NUM_RESERVED_DESCS;
    else {
      perror("Error calling sysconf");
      exit(1);
    }
  }
#endif
  
  /* now calculate max _players_ based on max descs */
  max_descs = MIN(MAX_PLAYERS, max_descs - NUM_RESERVED_DESCS);
  
  if (max_descs <= 0) {
    log_vfprintf("Non-positive max player limit!  (Set at %d using %s).",
        max_descs, method);
    
    exit(1);
  }
  
  log_vfprintf("Setting player limit to %d using %s.", max_descs, method);
  
  return max_descs;
}

void alarm_handler(int signal) {
  log("alarm_handler: Alarm signal has been triggered. Killing the MUD to break out of a suspected infinite loop.\r\n");
  raise(SIGSEGV);
}

/*
 * game_loop contains the main loop which drives the entire MUD.  It
 * cycles once every 0.10 seconds and is responsible for accepting new
 * new connections, polling existing connections for input, dequeueing
 * output and sending it out to players, and calling "heartbeat" functions
 * such as mobile_activity().
 */
void game_loop(int mother_desc)
{
  fd_set input_set, output_set, exc_set;
#ifndef GOTTAGOFAST
  struct timeval last_time, now, timespent, timeout, opt_time;
#else
  struct timeval last_time, opt_time;
#endif
  char comm[MAX_INPUT_LENGTH];
  struct descriptor_data *d, *next_d;
  int pulse = 0, maxdesc, aliased;
  int i;
  
  /* initialize various time values */
  zero_time.tv_sec = 0;
  zero_time.tv_usec = 0;
  opt_time.tv_usec = OPT_USEC;
  opt_time.tv_sec = 0;
  gettimeofday(&last_time, (struct timezone *) 0);
  PERF_prof_reset();
  
  // Register our alarm handler.
  signal(SIGALRM, alarm_handler);
  
  /* The Main Loop.  The Big Cheese.  The Top Dog.  The Head Honcho.  The.. */
  while (!circle_shutdown) {
    
    /* Reset the timer on our alarm. If this amount of time in seconds elapses without this being called again, invoke alarm_handler() and die. */
    alarm(10);
    
    /* Sleep if we don't have any connections */
    if (descriptor_list == NULL) {
      // Clear our alarm handler.
      signal(SIGALRM, SIG_IGN);
      
      FD_ZERO(&input_set);
      FD_SET(mother_desc, &input_set);
      if ((select(mother_desc + 1, &input_set, (fd_set *) 0, (fd_set *) 0, NULL) < 0)) {
        if (errno == EINTR) {
          // Suppressed this log since we get woken up regularly by alarm while idle.
          // log("Waking up to process signal.");
        } else
          perror("Select coma");
      } // else log("New connection.  Waking up.");
      gettimeofday(&last_time, (struct timezone *) 0);
      
      // Re-register our alarm handler.
      signal(SIGALRM, alarm_handler);
    }

    /* Set up the input, output, and exception sets for select(). */
    FD_ZERO(&input_set);
    FD_ZERO(&output_set);
    FD_ZERO(&exc_set);
    FD_SET(mother_desc, &input_set);
    maxdesc = mother_desc;
    for (d = descriptor_list; d; d = d->next) {
      if (d->descriptor > maxdesc)
        maxdesc = d->descriptor;
      FD_SET(d->descriptor, &input_set);
      FD_SET(d->descriptor, &output_set);
      FD_SET(d->descriptor, &exc_set);
    }
    
    /*
     * At this point, the original Diku code set up a signal mask to avoid
     * block all signals from being delivered.  I believe this was done in
     * order to prevent the MUD from dying with an "interrupted system call"
     * error in the event that a signal be received while the MUD is dormant.
     * However, I think it is easier to check for an EINTR error return from
     * this select() call rather than to block and unblock signals.
     */
#ifndef GOTTAGOFAST
    do {
      errno = 0;                /* clear error condition */
      
      /* figure out for how long we have to sleep */
      gettimeofday(&now, (struct timezone *) 0);
      timespent = timediff(&now, &last_time);

      {
        long int total_usec = 1000000 * timespent.tv_sec + timespent.tv_usec;
        double usage_pcnt = 100 * ((double)total_usec / OPT_USEC);
        PERF_log_pulse(usage_pcnt);

        if (usage_pcnt >= 100)
        {
          char buf[MAX_STRING_LENGTH];
          snprintf(buf, sizeof(buf), "Pulse usage %f >= 100%%. Trace info:", usage_pcnt);
          log(buf);
          PERF_prof_repr_pulse(buf, sizeof(buf));
          log(buf);
        }
      }

      /* just in case, re-calculate after PERF logging to figure out for how long we have to sleep */
      gettimeofday(&now, (struct timezone *) 0);
      timespent = timediff(&now, &last_time);
      timeout = timediff(&opt_time, &timespent);
      
      /* sleep until the next 0.1 second mark */
      if (select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &timeout) < 0)
        if (errno != EINTR) {
          perror("Select sleep");
          exit(1);
        }
      
      // Reset our alarm timer.
      alarm(10);
    } while (errno);
#endif
    
    /* record the time for the next pass */
    gettimeofday(&last_time, (struct timezone *) 0);
    PERF_prof_reset();
    PERF_PROF_SCOPE( pr_main_loop_, "main loop");
    
    // I added this for Linux, because &zero_time gets modified to become
    // time not slept.  This could possibly change the amount of time the
    // mud decides to sit on select(), not a good thing being that it should
    // return immediately (zero_time = 0).
    zero_time.tv_sec = 0;
    zero_time.tv_usec = 0;
    /* poll (without blocking) for new input, output, and exceptions */
    if (select(maxdesc + 1, &input_set, &output_set, &exc_set, &zero_time)<0) {
      perror("Select poll");
      return;
    }

    {
      PERF_PROF_SCOPE( pr_new_conn_, "new conns");
      /* New connection waiting for us? */
      if (FD_ISSET(mother_desc, &input_set))
        new_descriptor(mother_desc);
    }
    
    /* kick out the freaky folks in the exception set */
    {
      PERF_PROF_SCOPE( pr_exc_conn, "exc conns")
      for (d = descriptor_list; d; d = next_d) {
        next_d = d->next;
        if (FD_ISSET(d->descriptor, &exc_set)) {
          FD_CLR(d->descriptor, &input_set);
          FD_CLR(d->descriptor, &output_set);
          close_socket(d);
        }
      }
    }
    
    /* process descriptors with input pending */
    {
      PERF_PROF_SCOPE( pr_inp_conn_, "input conns")
      for (d = descriptor_list; d; d = next_d) {
        next_d = d->next;
        if (FD_ISSET(d->descriptor, &input_set))
          if (process_input(d) < 0)
            close_socket(d);
      }
    }
    
    /* process commands we just read from process_input */
    {
      PERF_PROF_SCOPE( pr_commands_, "process commands");
      for (d = descriptor_list; d; d = next_d) {
        next_d = d->next;
        
        if ((--(d->wait) <= 0) && get_from_q(&d->input, comm, &aliased)) {
          if (d->character) {
            d->character->char_specials.timer = 0;
            if (d->original)
              d->original->char_specials.timer = 0;
            if (!d->connected && GET_WAS_IN(d->character)) {
              if (d->character->in_room)
                char_from_room(d->character);
              char_to_room(d->character, GET_WAS_IN(d->character));
              GET_WAS_IN(d->character) = NULL;
              act("$n has returned.", TRUE, d->character, 0, 0, TO_ROOM);
            }
          }
          d->wait = 1;
          d->prompt_mode = 1;
          
          if (d->str)                     /* writing boards, mail, etc.   */
            string_add(d, comm);
          else if (d->showstr_point)      /* reading something w/ pager   */
            show_string(d, comm);
          else if (d->connected != CON_PLAYING)   /* in menus, etc.       */
            nanny(d, comm);
          else {                          /* else: we're playing normally */
            if (aliased) /* to prevent recursive aliases */
              d->prompt_mode = 0;
            else {
              if (perform_alias(d, comm)) /* run it through aliasing system */
                get_from_q(&d->input, comm, &aliased);
            }
            command_interpreter(d->character, comm, GET_CHAR_NAME(d->character)); /* send it to interpreter */
          }
        }
      }
    }
    
    /* send queued output out to the operating system (ultimately to user) */
    {
      PERF_PROF_SCOPE(pr_send_output, "send output");
      for (d = descriptor_list; d; d = next_d) {
        next_d = d->next;
        if (FD_ISSET(d->descriptor, &output_set) && *(d->output)) {
          if (process_output(d) < 0)
            close_socket(d);
          else
            d->prompt_mode = 1;
        }
      }
    }
    
    /* kick out folks in the CON_CLOSE state */
    {
      PERF_PROF_SCOPE(pr_con_close, "handle CON_CLOSE");
      for (d = descriptor_list; d; d = next_d) {
        next_d = d->next;
        if (STATE(d) == CON_CLOSE)
          close_socket(d);
      }
    }
    
    /* give each descriptor an appropriate prompt */
    {
      PERF_PROF_SCOPE(pr_send_prompt_, "send prompts");
      for (d = descriptor_list; d; d = d->next) {
        /* KaVir's protocol snippet. The last sent data was OOB, so do NOT draw the prompt */
        if (d->pProtocol->WriteOOB)
          continue;
        
        if (d->prompt_mode) {
          make_prompt(d);
          d->prompt_mode = 0;
        }
      }
    }
    
    /* handle heartbeat stuff */
    /* Note: pulse now changes every 0.10 seconds  */
    
    pulse++;
    
    if (!(pulse % PULSE_ZONE)) {
      zone_update();
      phone_check();
      process_autonav();
      process_boost();
    }
    
    if (!(pulse % PULSE_SPECIAL)) {
      spec_update();
    }
    
    if (!(pulse % PULSE_MONORAIL)) {
      MonorailProcess();
    }
    
    if (!(pulse % PULSE_MOBILE)) {
      mobile_activity();
    }
    
    if (!(pulse % ViolencePulse)) {
      decide_combat_pool();
      perform_violence();
      matrix_violence();
    }
    
    // Every MUD minute
    if (!(pulse % (SECS_PER_MUD_MINUTE * PASSES_PER_SEC))) {
      update_buildrepair();
      another_minute();
      misc_update();
      matrix_update();
#ifdef USE_DEBUG_CANARIES
      check_memory_canaries();
#endif
    }
    
    // Every 5 MUD minutes
    if (!(pulse % (5 * SECS_PER_MUD_MINUTE * PASSES_PER_SEC))) {
      if (!(pulse % (SECS_PER_MUD_HOUR * PASSES_PER_SEC)))
        process_regeneration(1);
      else
        process_regeneration(0);
      taxi_leaves();
    }
    
    // Every 59 MUD minutes
    if (!(pulse % (59 * SECS_PER_MUD_MINUTE * PASSES_PER_SEC)))
      save_vehicles();
    
    // Every MUD hour
    if (!(pulse % (SECS_PER_MUD_HOUR * PASSES_PER_SEC))) {
      point_update();
      weather_change();
      if (time_info.hours == 17) {
        for (i = 0; i <= top_of_world; i++) {
          if (ROOM_FLAGGED(&world[i], ROOM_STREETLIGHTS)) {
            send_to_room("A streetlight hums faintly, flickers, and turns on.\r\n", &world[i]);
          }
        }
      }
      if (time_info.hours == 7) {
        for (i = 0; i <= top_of_world; i++) {
          if (ROOM_FLAGGED(&world[i], ROOM_STREETLIGHTS)) {
            send_to_room("A streetlight flickers and goes out.\r\n", &world[i]);
          }
        }
      }
    }
    
    // Every 70 MUD minutes
    if (!(pulse % (70 * SECS_PER_MUD_MINUTE * PASSES_PER_SEC)))
      House_save_all();
    
    // Every MUD day
    if (!(pulse % (24 * SECS_PER_MUD_HOUR * PASSES_PER_SEC)))
      randomize_shop_prices();
    
    // Every IRL second
    if (!(pulse % PASSES_PER_SEC)) {
      EscalatorProcess();
      ElevatorProcess();
      msdp_update();
    }
    
    // Every 10 IRL seconds
    if (!(pulse % (10 * PASSES_PER_SEC))) {
      check_idle_passwords();
    }
    
    // Every IRL minute
    if (!(pulse % (60 * PASSES_PER_SEC))) {
      check_idling();
      send_keepalives();
      // johnson_update();
    }
    
    // Every IRL day
    if (!(pulse % (60 * PASSES_PER_SEC * 60 * 24))) {
      /* Check if the MySQL connection is active, and if not, recreate it. */
#ifdef DEBUG
      unsigned long oldthread = mysql_thread_id(mysql);
#endif
      {
        PERF_PROF_SCOPE( pr_mysql_ping_, "mysql_ping");
        mysql_ping(mysql);
      }
#ifdef DEBUG
      unsigned long newthread = mysql_thread_id(mysql);
      if (oldthread != newthread) {
        log("MySQL connection was recreated by ping.");
      }
#endif
      /* End MySQL keepalive ping section. */
    }
    
    tics++;                     /* tics since last checkpoint signal */
  }
}


/* ******************************************************************
 *  general utility stuff (for local use)                            *
 ****************************************************************** */

/*
 *  new code to calculate time differences, which works on systems
 *  for which tv_usec is unsigned (and thus comparisons for something
 *  being < 0 fail).  Based on code submitted by ss@sirocco.cup.hp.com.
 */

/*
 * code to return the time difference between a and b (a-b).
 * always returns a nonnegative value (floors at 0).
 */
struct timeval timediff(struct timeval * a, struct timeval * b)
{
  struct timeval rslt;
  
  if (a->tv_sec < b->tv_sec)
    return zero_time;
  else if (a->tv_sec == b->tv_sec)
  {
    if (a->tv_usec < b->tv_usec)
      return zero_time;
    else {
      rslt.tv_sec = 0;
      rslt.tv_usec = a->tv_usec - b->tv_usec;
      return rslt;
    }
  } else
  { /* a->tv_sec > b->tv_sec */
    rslt.tv_sec = a->tv_sec - b->tv_sec;
    if (a->tv_usec < b->tv_usec) {
      rslt.tv_usec = a->tv_usec + 1000000 - b->tv_usec;
      rslt.tv_sec--;
    } else
      rslt.tv_usec = a->tv_usec - b->tv_usec;
    return rslt;
  }
}

void record_usage(void)
{
  int sockets_connected = 0, sockets_playing = 0;
  struct descriptor_data *d;
  
  for (d = descriptor_list; d; d = d->next) {
    sockets_connected++;
    if (!d->connected)
      sockets_playing++;
  }
  
  log_vfprintf("usage: %-3d sockets connected, %-3d sockets playing",
      sockets_connected, sockets_playing);
  
#ifdef RUSAGE
  
  {
    struct rusage ru;
    
    getrusage(0, &ru);
    log("rusage: user time: %ld sec, system time: %ld sec, max res size: %ld",
        ru.ru_utime.tv_sec, ru.ru_stime.tv_sec, ru.ru_maxrss);
  }
#endif
  
}

/*
 * Turn off echoing (specific to telnet client)
 */
void echo_off(struct descriptor_data *d)
{
  ProtocolNoEcho( d, true );
  
  /*
  char off_string[] =
  {
    (char) IAC,
    (char) WILL,
    (char) TELOPT_ECHO,
    (char) 0,
  };
  
  SEND_TO_Q(off_string, d);
   */
}

/*
 * Turn on echoing (specific to telnet client)
 */
void echo_on(struct descriptor_data *d)
{
  ProtocolNoEcho( d, false );
  
  /*
  char on_string[] =
  {
    (char) IAC,
    (char) WONT,
    (char) TELOPT_ECHO,
    (char) TELOPT_NAOFFD,
    (char) TELOPT_NAOCRD,
    (char) 0,
  };
  
  SEND_TO_Q(on_string, d);
   */
}

// Sends a keepalive pulse to the given descriptor.
void keepalive(struct descriptor_data *d) {
  char keepalive[] = {
    (char) IAC,
    (char) NOP,
    (char) 0
  };
  
  SEND_TO_Q(keepalive, d);
}

// Sends keepalives to everyone who's enabled them.
void send_keepalives() {
  PERF_PROF_SCOPE(pr_, __func__);
  for (struct descriptor_data *d = descriptor_list; d; d = d->next)
    if (d->character && PRF_FLAGGED(d->character, PRF_KEEPALIVE))
      keepalive(d);
}

void make_prompt(struct descriptor_data * d)
{
  char *prompt;
  
  if (d->str) {
    if (D_PRF_FLAGGED(d, PRF_SCREENREADER)) {
      write_to_descriptor(d->descriptor, "Compose mode, write the at symbol on new line to quit. ");
    } else {
      write_to_descriptor(d->descriptor, "Compose mode, write @ on new line to quit]: ");
    }
  }
  else if (d->showstr_point)
    write_to_descriptor(d->descriptor, " Press [return] to continue, [q] to quit ");
  else if (D_PRF_FLAGGED(d, PRF_NOPROMPT)) {
    // Anything below this line won't render for noprompters.
    return;
  }
  else if (STATE(d) == CON_POCKETSEC)
    write_to_descriptor(d->descriptor, " > ");
  
  else if (!d->connected)
  {
    struct char_data *ch;
    
    if (d->original) {
      prompt = GET_PROMPT(d->original);
      ch = d->original;
    } else {
      prompt = GET_PROMPT(d->character);
      ch = d->character;
    }
    if (!prompt || !*prompt)
      write_to_descriptor(d->descriptor, "> ");
    else if (!strchr(prompt, '@')) {
      prompt = colorize(d, prompt);
      write_to_descriptor(d->descriptor, prompt);
    } else {
      char temp[MAX_INPUT_LENGTH], str[11];
      int i = 0, j, physical;
      
      for (; *prompt; prompt++) {
        if (*prompt == '@' && *(prompt+1)) {
          prompt++;
          switch(*prompt) {
            case 'a':       // astral pool
              sprintf(str, "%d", GET_ASTRAL(d->character));
              break;
            case 'A':
              if (GET_EQ(d->character, WEAR_WIELD)) {
                
                if (IS_GUN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD), 3)))
                  switch (GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD), 11)) {
                    case MODE_SS:
                      sprintf(str, "SS");
                      break;
                    case MODE_SA:
                      sprintf(str, "SA");
                      break;
                    case MODE_BF:
                      sprintf(str, "BF");
                      break;
                    case MODE_FA:
                      sprintf(str, "FA(%d)", GET_OBJ_TIMER(GET_EQ(d->character, WEAR_WIELD)));
                      break;
                    default:
                      sprintf(str, "NA");
                  }
                else strcpy(str, "ML");
                
              } else
                strcpy(str, "NA");
              break;
            case 'b':       // ballistic
              sprintf(str, "%d", GET_BALLISTIC(d->character));
              break;
            case 'c':       // combat pool
              sprintf(str, "%d", GET_COMBAT(d->character));
              break;
            case 'C':       // persona condition
              if (ch->persona)
                sprintf(str, "%d", ch->persona->condition);
              else
                sprintf(str, "NA");
              break;
            case 'd':       // defense pool
              sprintf(str, "%d", GET_DEFENSE(d->character));
              break;
            case 'D':
              sprintf(str, "%d", GET_BODY(d->character));
              break;
            case 'e':
              if (ch->persona)
                sprintf(str, "%d", ch->persona->decker->active);
              else
                sprintf(str, "0");
              break;
            case 'E':
              if (ch->persona && ch->persona->decker->deck)
                sprintf(str, "%d", GET_OBJ_VAL(ch->persona->decker->deck, 2));
              else
                sprintf(str, "0");
              break;
            case 'f':
              sprintf(str, "%d", GET_CASTING(d->character));
              break;
            case 'F':
              sprintf(str, "%d", GET_DRAIN(d->character));
              break;
            case 'g':       // current ammo
              if (GET_EQ(d->character, WEAR_WIELD) &&
                  IS_GUN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD), 3)))
                if (GET_EQ(d->character, WEAR_WIELD)->contains) {
                  sprintf(str, "%d", MIN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD), 5),
                                         GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD)->contains, 9)));
                } else
                  sprintf(str, "0");
                else
                  sprintf(str, "0");
              break;
            case 'G':       // max ammo
              if (GET_EQ(d->character, WEAR_WIELD) &&
                  IS_GUN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD), 3)))
                sprintf(str, "%d", GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD), 5));
              else
                sprintf(str, "0");
              break;
            case 'h':       // hacking pool
              if (ch->persona)
                sprintf(str, "%d", GET_REM_HACKING(d->character));
              else
                sprintf(str, "%d", GET_HACKING(d->character));
              break;
            case 'H':
              sprintf(str, "%d%cM", (time_info.hours % 12 == 0 ? 12 : time_info.hours % 12), (time_info.hours >= 12 ? 'P' : 'A'));
              break;
            case 'i':       // impact
              sprintf(str, "%d", GET_IMPACT(d->character));
              break;
            case 'k':       // karma
              sprintf(str, "%0.2f", ((float)GET_KARMA(ch) / 100));
              break;
            case 'l':       // current weight
              sprintf(str, "%.2f", IS_CARRYING_W(d->character));
              break;
            case 'L':       // max weight
              sprintf(str, "%d", CAN_CARRY_W(d->character));
              break;
            case 'm':       // current mental
              physical = (int)(GET_MENTAL(d->character) / 100);
              for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content)
                if (GET_OBJ_VAL(bio, 0) == BIO_DAMAGECOMPENSATOR)
                  physical = MIN(10, physical + GET_OBJ_VAL(bio, 1));
                else if (GET_OBJ_VAL(bio, 0) == BIO_PAINEDITOR && GET_OBJ_VAL(bio, 3)) {
                  physical = 10;
                  break;
                }
              sprintf(str, "%d", physical);
              break;
            case 'M':       // max mental
              sprintf(str, "%d", (int)(GET_MAX_MENTAL(d->character) / 100));
              break;
            case 'n':       // nuyen
              sprintf(str, "%ld", GET_NUYEN(d->character));
              break;
            case 'o':       // offense pool
              sprintf(str, "%d", GET_OFFENSE(d->character));
              break;
            case 'p':       // current physical
              physical = (int)(GET_PHYSICAL(d->character) / 100);
              for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content)
                if (GET_OBJ_VAL(bio, 0) == BIO_DAMAGECOMPENSATOR)
                  physical = MIN(10, physical + GET_OBJ_VAL(bio, 1));
                else if (GET_OBJ_VAL(bio, 0) == BIO_PAINEDITOR && GET_OBJ_VAL(bio, 3)) {
                  physical = 10;
                  break;
                }
              sprintf(str, "%d", physical);
              break;
            case 'P':       // max physical
              sprintf(str, "%d", (int)(GET_MAX_PHYSICAL(d->character) / 100));
              break;
            case 'r':
              if (ch->persona && ch->persona->decker->deck)
                sprintf(str, "%d", GET_OBJ_VAL(ch->persona->decker->deck, 3) - GET_OBJ_VAL(ch->persona->decker->deck, 5));
              else
                sprintf(str, "0");
              break;
            case 'R':
              if (ch->persona && ch->persona->decker && ch->persona->decker->deck)
                sprintf(str, "%d", GET_OBJ_VAL(ch->persona->decker->deck, 3));
              else
                sprintf(str, "0");
              break;
            case 's':       // current ammo
              if (GET_EQ(d->character, WEAR_HOLD) &&
                  IS_GUN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_HOLD), 3)))
                if (GET_EQ(d->character, WEAR_HOLD)->contains) {
                  sprintf(str, "%d", MIN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_HOLD), 5),
                                         GET_OBJ_VAL(GET_EQ(d->character, WEAR_HOLD)->contains, 9)));
                } else
                  sprintf(str, "0");
                else
                  sprintf(str, "0");
              break;
            case 'S':       // max ammo
              if (GET_EQ(d->character, WEAR_HOLD) &&
                  IS_GUN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_HOLD), 3)))
                sprintf(str, "%d", GET_OBJ_VAL(GET_EQ(d->character, WEAR_HOLD), 5));
              else
                sprintf(str, "0");
              break;
            case 't':       // magic pool
              sprintf(str, "%d", GET_MAGIC(d->character));
              break;
            case 'T':
              sprintf(str, "%d", GET_SUSTAINED_NUM(d->character));
              break;
            case 'u':
              sprintf(str, "%d", GET_SDEFENSE(d->character));
              break;
            case 'U':
              sprintf(str, "%d", GET_REFLECT(d->character));
              break;
            case 'w':
              sprintf(str, "%d", GET_INVIS_LEV(d->character));
              break;
            case 'W':
              sprintf(str, "%d", GET_INCOG_LEV(d->character));
              break;
            case 'z':
              if (GET_REAL_LEVEL(d->character) >= LVL_BUILDER)
                sprintf(str, "%d", d->character->player_specials->saved.zonenum);
              else
                strcpy(str, "@z");
              break;
            case 'v':
              if (GET_REAL_LEVEL(d->character) >= LVL_BUILDER)
                sprintf(str, "%ld", d->character->in_room->number);
              else
                strcpy(str, "@v");
              break;
            case '@':
              strcpy(str, "@");
              break;
            case '!':
              strcpy(str, "\r\n");
              break;
            default:
              sprintf(str, "@%c", *prompt);
              break;
          }
          for (j = 0; str[j]; j++, i++)
            temp[i] = str[j];
        } else {
          temp[i] = *prompt;
          i++;
        }
      }
      temp[i] = '\0';
      int size = strlen(temp);
      const char *final_str = ProtocolOutput(d, temp, &size);
      write_to_descriptor(d->descriptor, final_str);
    }
  }
}

void write_to_q(const char *txt, struct txt_q * queue, int aliased)
{
  struct txt_block *temp = new txt_block;
  temp->text = new char[strlen(txt) + 1];
  strcpy(temp->text, txt);
  temp->aliased = aliased;
  
  /* queue empty? */
  if (!queue->head)
  {
    temp->next = NULL;
    queue->head = queue->tail = temp;
  } else
  {
    queue->tail->next = temp;
    queue->tail = temp;
    temp->next = NULL;
  }
}

int get_from_q(struct txt_q * queue, char *dest, int *aliased)
{
  struct txt_block *tmp;
  
  /* queue empty? */
  if (!queue->head)
    return 0;
  
  tmp = queue->head;
  strcpy(dest, queue->head->text);
  *aliased = queue->head->aliased;
  queue->head = queue->head->next;
  
  if (tmp) {
    DELETE_AND_NULL_ARRAY(tmp->text);
    DELETE_AND_NULL(tmp);
  }
  
  return 1;
}

/* Empty the queues before closing connection */
void flush_queues(struct descriptor_data * d)
{
  int dummy;
  
  if (d->large_outbuf)
  {
    d->large_outbuf->next = bufpool;
    bufpool = d->large_outbuf;
  }
  while (get_from_q(&d->input, buf2, &dummy))
    ;
}

/* Add a new string to a player's output queue */
void write_to_output(const char *unmodified_txt, struct descriptor_data *t)
{
  int size = strlen(unmodified_txt);
  
  // Process text per KaVir's protocol snippet.
  const char *txt = ProtocolOutput(t, unmodified_txt, &size);
  if (t->pProtocol->WriteOOB > 0)
    --t->pProtocol->WriteOOB;
  
  // txt = colorize(t, (char *)txt);
  
  /* if we're in the overflow state already, ignore this new output */
  if (t->bufptr < 0)
    return;
  
  /* if we have enough space, just write to buffer and that's it! */
  if (t->bufspace >= size)
  {
    strcpy(t->output + t->bufptr, txt);
    t->bufspace -= size;
    t->bufptr += size;
    return;
  }
  
  /*
   * If we're already using the large buffer, or if even the large buffer
   * is too small to handle this new text, chuck the text and switch to the
   * overflow state.
   */
  if (t->large_outbuf || ((size + strlen(t->output)) > LARGE_BUFSIZE))
  {
    t->bufptr = -1;
    buf_overflows++;
    return;
  }
  
  buf_switches++;
  
  /* if the pool has a buffer in it, grab it */
  if (bufpool != NULL)
  {
    t->large_outbuf = bufpool;
    bufpool = bufpool->next;
  } else
  {                      /* else create a new one */
    t->large_outbuf = new txt_block;
    t->large_outbuf->text = new char[LARGE_BUFSIZE];
    buf_largecount++;
  }
  
  strcpy(t->large_outbuf->text, t->output);     /* copy to big buffer */
  t->output = t->large_outbuf->text;    /* make big buffer primary */
  strcat(t->output, txt);       /* now add new text */
  
  /* calculate how much space is left in the buffer */
  t->bufspace = LARGE_BUFSIZE - 1 - strlen(t->output);
  
  /* set the pointer for the next write */
  t->bufptr = strlen(t->output);
}

/* ******************************************************************
 *  socket handling                                                  *
 ****************************************************************** */
void init_descriptor (struct descriptor_data *newd, int desc)
{
  static int last_desc = 0;  /* last descriptor number */
  
  newd->descriptor = desc;
  newd->connected = CON_GET_NAME;
  newd->idle_tics = 0;
  newd->wait = 1;
  newd->output = newd->small_outbuf;
  newd->bufspace = SMALL_BUFSIZE - 1;
  newd->next = descriptor_list;
  newd->login_time = time (0);
  
  // Initialize the protocol data when a new descriptor structure is allocated.
  newd->pProtocol = ProtocolCreate();
  
  if (++last_desc == 10000)
    last_desc = 1;
  newd->desc_num = last_desc;
}

int new_descriptor(int s)
{
  int desc, sockets_connected = 0;
  unsigned long addr;
  int i;
  struct descriptor_data *newd;
  struct sockaddr_in peer;
  struct hostent *from;
  extern char *GREETINGS;
  
  /* accept the new connection */
  i = sizeof(peer);
#if defined(__CYGWIN__) || defined(WIN32)
  
  if ((desc = accept(s, (struct sockaddr *) &peer, &i)) < 0) {
    perror("accept");
    return -1;
  }
#else
    
  if ((desc = accept(s, (struct sockaddr *) &peer, (unsigned *) &i)) < 0)
  {
    perror("accept");
    return -1;
  }
#endif
  
  /* keep it from blocking */
  nonblock(desc);
  
  /* make sure we have room for it */
  for (newd = descriptor_list; newd; newd = newd->next)
    sockets_connected++;
  
  if (sockets_connected >= max_players)
  {
    write_to_descriptor(desc, "Sorry, Awakened Worlds is full right now... try again later!  :-)\r\n");
    close(desc);
    return 0;
  }
  /* create a new descriptor */
  newd = new descriptor_data;
  memset((char *) newd, 0, sizeof(struct descriptor_data));
  
  /* find the sitename */
  if (nameserver_is_slow || !(from = gethostbyaddr((char *) &peer.sin_addr,
                                                   sizeof(peer.sin_addr), AF_INET)))
  {
    if (!nameserver_is_slow)
      perror("gethostbyaddr");
    addr = ntohl(peer.sin_addr.s_addr);
    sprintf(newd->host, "%03u.%03u.%03u.%03u", (int) ((addr & 0xFF000000) >> 24),
            (int) ((addr & 0x00FF0000) >> 16), (int) ((addr & 0x0000FF00) >> 8),
            (int) ((addr & 0x000000FF)));
  } else
  {
    strncpy(newd->host, from->h_name, HOST_LENGTH);
    *(newd->host + HOST_LENGTH) = '\0';
  }
  
  /* determine if the site is banned */
  if (isbanned(newd->host) == BAN_ALL)
  {
    close(desc);
    sprintf(buf2, "Connection attempt denied from [%s]", newd->host);
    mudlog(buf2, NULL, LOG_BANLOG, TRUE);
    DELETE_AND_NULL(newd);
    return 0;
  }
  
  init_descriptor(newd, desc);
  
  /* prepend to list */
  descriptor_list = newd;
  
  // Once the descriptor has been fully created and added to any lists, it's time to negotiate:
  ProtocolNegotiate(newd);
  
  int size = strlen(GREETINGS);
  SEND_TO_Q(ProtocolOutput(newd, GREETINGS, &size), newd);
  return 0;
}
  
int process_output(struct descriptor_data *t) {
  static char i[LARGE_BUFSIZE + GARBAGE_SPACE];
  static int result;
  
  /* If the descriptor is NULL, just return */
  if ( !t )
    return 0;
  
  /* we may need this \r\n for later -- see below */
  strcpy(i, "\r\n");
  
  /* now, append the 'real' output */
  strcpy(i + 2, t->output);
  
  /* if we're in the overflow state, notify the user */
  if (t->bufptr < 0)
    strcat(i, "**OVERFLOW**");
  
  /* add the extra CRLF if the person isn't in compact mode */
  if (!t->connected && t->character)
    strcat(i + 2, "\r\n");
  
  /*
   * now, send the output.  If this is an 'interruption', use the prepended
   * CRLF, otherwise send the straight output sans CRLF.
   */
  if (!t->prompt_mode)          /* && !t->connected) */
    result = write_to_descriptor(t->descriptor, i);
  else
    result = write_to_descriptor(t->descriptor, i + 2);
  
  /* handle snooping: prepend "% " and send to snooper */
  if (t->snoop_by)
  {
    SEND_TO_Q("% ", t->snoop_by);
    SEND_TO_Q(t->output, t->snoop_by);
    SEND_TO_Q("%%", t->snoop_by);
  }
  /*
   * if we were using a large buffer, put the large buffer on the buffer pool
   * and switch back to the small one
   */
  if (t->large_outbuf)
  {
    t->large_outbuf->next = bufpool;
    bufpool = t->large_outbuf;
    t->large_outbuf = NULL;
    t->output = t->small_outbuf;
  }
  /* reset total bufspace back to that of a small buffer */
  t->bufspace = SMALL_BUFSIZE - 1;
  t->bufptr = 0;
  *(t->output) = '\0';
  
  return result;
}

int write_to_descriptor(int desc, const char *txt) {
  int total, bytes_written;
  
  total = strlen(txt);
  
  do {
    if ((bytes_written = write(desc, txt, total)) < 0) {
#ifdef EWOULDBLOCK
      if (errno == EWOULDBLOCK)
        errno = EAGAIN;
#endif
      
      if (errno == EAGAIN)
        log("process_output: socket write would block, about to close");
      else
        perror("Write to socket");
      return -1;
    } else {
      txt += bytes_written;
      total -= bytes_written;
    }
  } while (total > 0);
  
  return 0;
}

/*
 * ASSUMPTION: There will be no newlines in the raw input buffer when this
 * function is called.  We must maintain that before returning.
 */
int process_input(struct descriptor_data *t) {
  int buf_length, bytes_read, space_left, failed_subst;
  char *ptr, *read_point, *write_point, *nl_pos = NULL;
  char tmp[MAX_INPUT_LENGTH + 8];
  
  // Initialize our temporary buffer, which is used by KaVir protocol code for reading out their crap.
  static char temporary_buffer[MAX_PROTOCOL_BUFFER];
  temporary_buffer[0] = '\0';
    
  /* End of KaVir protocol kludge. */
  
  /* first, find the point where we left off reading data */
  buf_length = strlen(t->inbuf);
  read_point = t->inbuf + buf_length;
  space_left = MAX_RAW_INPUT_LENGTH - buf_length - 1;
    
  do {
    if (space_left <= 0) {
      if (t->character)
        extract_char(t->character);
      log("process_input: about to close connection: input overflow");
      return -1;
    }
    
    if ((bytes_read = read(t->descriptor, &temporary_buffer, MAX_PROTOCOL_BUFFER - 1)) < 0) {
      
#ifdef EWOULDBLOCK
      if (errno == EWOULDBLOCK)
        errno = EAGAIN;
#endif
      
      if (errno != EAGAIN) {
        perror("process_input: about to lose connection");
        return -1;              /* some error condition was encountered on
                                 * read */
      } else
        return 0;               /* the read would have blocked: just means no
                                 * data there */
    } else if (bytes_read == 0) {
      // log("EOF on socket read (connection broken by peer)");
      return -1;
    }
    /* at this point, we know we got some data from the read */
    
    temporary_buffer[bytes_read] = '\0';  /* terminate the string */
    
    // Push the data for processing through KaVir's protocol code. Resize bytes_read to account for the stripped control chars.
    ProtocolInput(t, temporary_buffer, bytes_read, t->inbuf);
#ifdef DEBUG_PROTOCOL
    log_vfprintf("Parsed '%s' to '%s', changing length from %d to %lu.",
                 temporary_buffer, t->inbuf + buf_length, bytes_read, strlen(t->inbuf + buf_length));
#endif
    bytes_read = strlen(t->inbuf + buf_length);
    
    // We parsed out all the data-- must have been pure control codes. Break out.
    if (bytes_read == 0)
      return 0;
    
    /* search for a newline in the data we just read */
    for (ptr = read_point; *ptr && !nl_pos; ptr++)
      if (ISNEWL(*ptr))
        nl_pos = ptr;
    
    read_point += bytes_read;
    space_left -= bytes_read;
    
    /*
     * on some systems such as AIX, POSIX-standard nonblocking I/O is broken,
     * causing the MUD to hang when it encounters input not terminated by a
     * newline.  This was causing hangs at the Password: prompt, for example.
     * I attempt to compensate by always returning after the _first_ read, instead
     * of looping forever until a read returns -1.  This simulates non-blocking
     * I/O because the result is we never call read unless we know from select()
     * that data is ready (process_input is only called if select indicates that
     * this descriptor is in the read set).  JE 2/23/95.
     */
#ifdef SOMETHING_THAT_DOESNT_EXIST_JUST_SO_XCODE_WONT_CHOKE_ON_SEEING_TWO_END_BRACKETS_WHILE_NOT_UNDERSTANDING_THE_IFDEFS_THAT_SHIELD_THEM
    {
#endif
      
#if !defined(POSIX_NONBLOCK_BROKEN)
  // note: if you define the above, you'll have to figure out protocol handling yourself-- see below.
    
  } while (nl_pos == NULL);
#else
  
  } while (0);

  if (nl_pos == NULL)
    return 0;
#endif

  /*
   * okay, at this point we have at least one newline in the string; now we
   * can copy the formatted data to a new array for further processing.
   */

  read_point = t->inbuf;

  while (nl_pos != NULL) {
    write_point = tmp;
    space_left = MAX_INPUT_LENGTH - 1;
    
    for (ptr = read_point; (space_left > 0) && (ptr < nl_pos); ptr++) {
      if (*ptr == '\b') {       /* handle backspacing */
        if (write_point > tmp) {
          if (*(--write_point) == '$') {
            write_point--;
            space_left += 2;
          } else
            space_left++;
        }
      } else if (isprint(*ptr)) {
        if ((*(write_point++) = *ptr) == '$') { /* copy one character */
          *(write_point++) = '$';
          space_left -= 2;
        } else
          space_left--;
      }
    }
    *write_point = '\0';
    
    if ((space_left <= 0) && (ptr < nl_pos)) {
      char buffer[MAX_INPUT_LENGTH + 64];
      
      sprintf(buffer, "Line too long.  Truncated to:\r\n%s\r\n", tmp);
      if (write_to_descriptor(t->descriptor, buffer) < 0)
        return -1;
    }
    if (t->snoop_by) {
      SEND_TO_Q("% ", t->snoop_by);
      SEND_TO_Q(tmp, t->snoop_by);
      SEND_TO_Q("\r\n", t->snoop_by);
    }
    failed_subst = 0;
    
    if (*tmp == '!' && t->connected != CON_CNFPASSWD )
      strcpy(tmp, t->last_input);
    else if (*tmp == '|' && t->connected != CON_CNFPASSWD ) {
      if (!(failed_subst = perform_subst(t, t->last_input, tmp)))
        strcpy(t->last_input, tmp);
    } else
      strcpy(t->last_input, tmp);
    
    if (!failed_subst)
      write_to_q(tmp, &t->input, 0);
    
    /* find the end of this line */
    while (ISNEWL(*nl_pos))
      nl_pos++;
    
    /* see if there's another newline in the input buffer */
    read_point = ptr = nl_pos;
    for (nl_pos = NULL; *ptr && !nl_pos; ptr++)
      if (ISNEWL(*ptr))
        nl_pos = ptr;
  }

  /* now move the rest of the buffer up to the beginning for the next pass */
  write_point = t->inbuf;
  while (*read_point)
    *(write_point++) = *(read_point++);
  *write_point = '\0';

  return 1;
}

/*
 * perform substitution for the '^..^' csh-esque syntax
 * orig is the orig string (i.e. the one being modified.
 * subst contains the substition string, i.e. "^telm^tell"
 */
int perform_subst(struct descriptor_data *t, char *orig, char *subst)
{
  char New[MAX_INPUT_LENGTH + 5];
  
  char *first, *second, *strpos;
  
  /*
   * first is the position of the beginning of the first string (the one
   * to be replaced
   */
  first = subst + 1;
  
  /* now find the second '^' */
  if (!(second = strchr(first, '^')))
  {
    SEND_TO_Q("Invalid substitution.\r\n", t);
    return 1;
  }
  
  /* terminate "first" at the position of the '^' and make 'second' point
   * to the beginning of the second string */
  *(second++) = '\0';
  
  /* now, see if the contents of the first string appear in the original */
  if (!(strpos = strstr(orig, first)))
  {
    SEND_TO_Q("Invalid substitution.\r\n", t);
    return 1;
  }
  
  /* now, we construct the new string for output. */
  
  /* first, everything in the original, up to the string to be replaced */
  strncpy(New, orig, (strpos - orig));
  New[(strpos - orig)] = '\0';
  
  /* now, the replacement string */
  strncat(New, second, (MAX_INPUT_LENGTH - strlen(New) - 1));
  
  /* now, if there's anything left in the original after the string to
   * replaced, copy that too. */
  if (((strpos - orig) + strlen(first)) < strlen(orig))
    strncat(New, strpos + strlen(first), (MAX_INPUT_LENGTH - strlen(New) - 1));
  
  /* terminate the string in case of an overflow from strncat */
  New[MAX_INPUT_LENGTH-1] = '\0';
  strcpy(subst, New);
  
  return 0;
}

void free_editing_structs(descriptor_data *d, int state)
{
  if (d->edit_obj) {
    if (d->connected == CON_PART_CREATE || d->connected == CON_AMMO_CREATE || d->connected == CON_SPELL_CREATE
        || (d->connected >= CON_PRO_CREATE && d->connected <= CON_DECK_CREATE))
      extract_obj(d->edit_obj);
    else
      Mem->DeleteObject(d->edit_obj);
    d->edit_obj = NULL;
  }
  
  if (d->edit_room) {
    Mem->DeleteRoom(d->edit_room);
    d->edit_room = NULL;
  }
  
  if (d->edit_mob) {
    Mem->DeleteCh(d->edit_mob);
    d->edit_mob = NULL;
  }
  
  if (d->edit_quest) {
    free_quest(d->edit_quest);
    DELETE_AND_NULL(d->edit_quest);
  }
  
  if (d->edit_shop) {
    free_shop(d->edit_shop);
    DELETE_AND_NULL(d->edit_shop);
  }
  
  if (d->edit_zon) {
    DELETE_ARRAY_IF_EXTANT(d->edit_zon->name);
    DELETE_AND_NULL_ARRAY(d->edit_zon);
  }
  
  DELETE_IF_EXTANT(d->edit_cmd);
  
  if (d->edit_veh) {
    Mem->DeleteVehicle(d->edit_veh);
    d->edit_veh = NULL;
  }
  if (d->edit_host) {
    Mem->DeleteHost(d->edit_host);
    d->edit_host = NULL;
  }
  if (d->edit_icon) {
    Mem->DeleteIcon(d->edit_icon);
    d->edit_icon = NULL;
  }
  if (d->edit_pgroup) {
    delete d->edit_pgroup;
    d->edit_pgroup = NULL;
  }
}

void close_socket(struct descriptor_data *d)
{
  struct descriptor_data *temp;
  spell_data *one, *next;
  char buf[128];
  
  /* Forget snooping */
  if (d->snooping)
    d->snooping->snoop_by = NULL;
  
  if (d->snoop_by)
  {
    SEND_TO_Q("Your victim is no longer among us.\r\n", d->snoop_by);
    d->snoop_by->snooping = NULL;
  }
  if (d->character)
  {
    /* added to Free up temporary editing constructs */
    if (d->connected == CON_PART_CREATE || d->connected == CON_SPELL_CREATE || d->connected == CON_PLAYING || (d->connected >= CON_SPELL_CREATE &&
                                                                                                               d->connected <= CON_BCUSTOMIZE)) {
      if (d->connected == CON_VEHCUST)
        d->edit_veh = NULL;
      if (d->connected == CON_POCKETSEC) {
        d->edit_obj = NULL;
        d->edit_mob = NULL;
      }
      playerDB.SaveChar(d->character);
      act("$n has lost $s link.", TRUE, d->character, 0, 0, TO_ROOM);
      sprintf(buf, "Closing link to: %s. (%s)", GET_CHAR_NAME(d->character), connected_types[d->connected]);
      mudlog(buf, d->character, LOG_CONNLOG, TRUE);
      if (d->character->persona) {
        extract_icon(d->character->persona);
        d->character->persona = NULL;
        PLR_FLAGS(d->character).RemoveBit(PLR_MATRIX);
      }
      free_editing_structs(d, STATE(d));
      d->character->desc = NULL;
    } else {
      sprintf(buf, "Losing player: %s.",
              GET_CHAR_NAME(d->character) ? GET_CHAR_NAME(d->character) : "<null>");
      mudlog(buf, d->character, LOG_CONNLOG, TRUE);
      // we do this because objects can be given to characters in chargen
      for (int i = 0; i < NUM_WEARS; i++)
        if (GET_EQ(d->character, i))
          extract_obj(GET_EQ(d->character, i));
      while (d->character->carrying)
        extract_obj(d->character->carrying);
      if (d->character->spells)
        for (one = d->character->spells; one; one = next) {
          next = one->next;
          DELETE_ARRAY_IF_EXTANT(one->name);
          DELETE_AND_NULL(one);
        }
      d->character->spells = NULL;
      Mem->DeleteCh(d->character);
    }
  }
  
  // Free the protocol data when a socket is closed.
  ProtocolDestroy( d->pProtocol );
  
  close(d->descriptor);
  flush_queues(d);
  
  /* JE 2/22/95 -- part of my enending quest to make switch stable */
  if (d->original && d->original->desc)
    d->original->desc = NULL;
  
  REMOVE_FROM_LIST(d, descriptor_list, next);
  
  DELETE_ARRAY_IF_EXTANT(d->showstr_head);
  
  // Clean up message history lists.
  delete_message_history(d);
  
  delete d;
}

void check_idle_passwords(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct descriptor_data *d;
  
  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) != CON_PASSWORD && STATE(d) != CON_GET_NAME)
      continue;
    
    if (++d->idle_tics >= 12 ) {
      /* 120 seconds RL, or 2 minutes */
      if (STATE(d) == CON_PASSWORD)
        echo_on(d);
      
      SEND_TO_Q("\r\nTimed out... goodbye.\r\n", d);
      STATE(d) = CON_CLOSE;
    }
  }
}


/*
 * I tried to universally convert Circle over to POSIX compliance, but
 * alas, some systems are still straggling behind and don't have all the
 * appropriate defines.  In particular, NeXT 2.x defines O_NDELAY but not
 * O_NONBLOCK.  Krusty old NeXT machines!  (Thanks to Michael Jones for
 * this and various other NeXT fixes.)
 */
#ifndef O_NONBLOCK
#define O_NONBLOCK O_NDELAY
#endif

#if defined(WIN32) && !defined(__CYGWIN__)
void nonblock(socket_t s)
{
  unsigned long val = 1;
  ioctlsocket(s, FIONBIO, &val);
}
#else

void nonblock(int s)
{
  int flags;
  
  flags = fcntl(s, F_GETFL, 0);
  flags |= O_NONBLOCK;
  if (fcntl(s, F_SETFL, flags) < 0) {
    perror("Fatal error executing nonblock (comm.c)");
    exit(1);
  }
}
#endif

void shutdown(int code)
{
  circle_shutdown = true;
  exit_code = code;
}

/* ******************************************************************
 *  signal-handling functions (formerly signals.c)                   *
 ****************************************************************** */


void checkpointing(int Empty)
{
  if (!tics) {
    log("SYSERR: CHECKPOINT shutdown: tics not updated");
    abort();
  } else
    tics = 0;
}



void unrestrict_game(int Empty)
{
  extern struct ban_list_element *ban_list;
  
  mudlog("Received SIGUSR2 - completely unrestricting game (emergency)",
         NULL, LOG_SYSLOG, TRUE);
  ban_list = NULL;
  restrict = 0;
}

void free_up_memory(int Empty)
{
  mudlog("Warning: Received signal, Freeing up Memory", NULL, LOG_SYSLOG, TRUE);
  if (!Mem->ClearStacks()) {
    cerr << "SYSERR: Unable to free enough memory, shutting down...\n";
    House_save_all();
    exit(0);
  }
}

void hupsig(int Empty)
{
  mudlog("Received SIGHUP.  Shutting down...", NULL, LOG_SYSLOG, TRUE);
  House_save_all();
  exit(0);
}

void intsig(int Empty)
{
  mudlog("Received SIGINT.  Shutting down...", NULL, LOG_SYSLOG, TRUE);
  House_save_all();
  exit(0);
}

void termsig(int Empty)
{
  mudlog("Received SIGTERM.  Shutting down...", NULL, LOG_SYSLOG, TRUE);
  House_save_all();
  exit(0);
}



/*
 * This is an implementation of signal() using sigaction() for portability.
 * (sigaction() is POSIX; signal() is not.)  Taken from Stevens' _Advanced
 * Programming in the UNIX Environment_.  We are specifying that all system
 * calls _not_ be automatically restarted for uniformity, because BSD systems
 * do not restart select(), even if SA_RESTART is used.
 *
 * Note that NeXT 2.x is not POSIX and does not have sigaction; therefore,
 * I just define it to be the old signal.  If your system doesn't have
 * sigaction either, you can use the same fix.
 *
 * SunOS Release 4.0.2 (sun386) needs this too, according to Tim Aldric.
 */


#if defined(NeXT) || defined(sun386) || (defined(WIN32) && !defined(__CYGWIN__))
#define my_signal(signo, func) signal(signo, func)
#else
sigfunc *my_signal(int signo, sigfunc * func)
{
  struct sigaction act, oact;
  
  act.sa_handler = func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
#ifdef SA_INTERRUPT
  
  act.sa_flags |= SA_INTERRUPT; /* SunOS */
#endif
  
  if (sigaction(signo, &act, &oact) < 0)
    return SIG_ERR;
  
  return oact.sa_handler;
}
#endif /* NeXT */

void signal_setup(void)
{
  my_signal(SIGINT, hupsig);
  my_signal(SIGTERM, hupsig);
#if !defined(WIN32) || defined(__CYGWIN__)
  
  my_signal(SIGPIPE, SIG_IGN);
  my_signal(SIGALRM, SIG_IGN);
#endif
}



/* ****************************************************************
 *       Public routines for system-to-player-communication        *
 *******************************************************************/

int is_color (char c)
{
  int i = 0;
  
  switch (LOWER(c)) {
    case 'b':
    case 'c':
    case 'g':
    case 'l':
    case 'm':
    case 'r':
    case 'w':
    case 'y':
      i = 2;
      break;
    default:
      break;
  }
  if (i && c < 'a')
    i--;
  
  return i;
}

// parses the color codes and translates the properly
char *colorize(struct descriptor_data *d, const char *str, bool skip_check)
{
  // Big ol' buffer so that even if the entire string is color codes, we can still store it all plus a terminal \0. -LS
  static char buffer[MAX_STRING_LENGTH * 8 + 1];
  //char *temp = &buffer[0];
  //const char *color;
  
  // KaVir's protocol snippet handles this, so...
  strcpy(buffer, str);
  return &buffer[0];
  /*
  
  if (!str || !*str) {
    // Declare our own error buf so we don't clobber anyone's strings.
    char colorize_error_buf[200];
    sprintf(colorize_error_buf, "SYSERR: Received empty string to colorize() for descriptor %d (orig %s, char %s).",
            d->descriptor, d->original ? GET_NAME(d->original) : "(null)", d->character ? GET_CHAR_NAME(d->character) : "(null)");
    mudlog(colorize_error_buf, NULL, LOG_SYSLOG, TRUE);
    strcpy(buffer, "(null)");
    return buffer;
  }
  
  if (!D_PRF_FLAGGED(d, PRF_NOCOLOR) && (skip_check || d->character)) // ok but why do we care if they have a character?
  {
    while(*str) {
      if (*str == '^') {
        // first, advance the pointer, then point color to the color
        switch (*++str) {
          case 'l':
            color = KBLK;
            break; // black
          case 'r':
            color = KRED;
            break; // red
          case 'g':
            color = KGRN;
            break; // green
          case 'y':
            color = KYEL;
            break; // yellow
          case 'b':
            color = KBLU;
            break; // blue
          case 'm':
            color = KMAG;
            break; // magenta
          case 'n':
            color = KNRM;
            break; // normal
          case 'c':
            color = KCYN;
            break; // cyan
          case 'w':
            color = KWHT;
            break; // white
          case 'L':
            color = B_BLK;
            break; // bold black
          case 'R':
            color = B_RED;
            break; // bold red
          case 'G':
            color = B_GREEN;
            break; // bold green
          case 'Y':
            color = B_YELLOW;
            break; // bold yellow
          case 'B':
            color = B_BLUE;
            break; // bold blue
          case 'M':
            color = B_MAGENTA;
            break; // bold magenta
          case 'N':
            color = CGLOB;
            break;
          case 'C':
            color = B_CYAN;
            break; // bold cyan
          case 'W':
            color = B_WHITE;
            break; // bold white
          case '^':
            color = "^";
            break;
          default:
            color = NULL;
            break;
        }
        // somehow I get the feeling there is a better way to do this
        if (color) {
          while (*color)
            *temp++ = *color++;
          ++str;
        } else {
          *temp++ = '^';
          *temp++ = *str++;
        }
      } else
        *temp++ = *str++;
      // in the case where it is nothing, we just copy and advance
    }
    // now cap it off with the off-color set of ansi-escape sequences
    // as a preventive measure
    color = KNRM;
    while (*color)
      *temp++ = *color++;
  } else
  {
    while(*str) {
      if (*str == '^') {
        // first, advance the pointer, then point color to the color
        switch (*++str) {
          case 'l':
          case 'r':
          case 'g':
          case 'y':
          case 'b':
          case 'm':
          case 'n':
          case 'c':
          case 'w':
          case 'L':
          case 'R':
          case 'G':
          case 'Y':
          case 'B':
          case 'N':
          case 'M':
          case 'C':
          case 'W':
            color = "";
            break;
          case '^':
            color = "^";
            break;
          default:
            color = NULL;
            break;
        }
        // first we check to see if it was nothing, ie none of the sequences
        if (!color) {
          // and if it is not, we copy ^ first, then the value of *str
          // and advance again
          *temp++ = '^';
          *temp++ = *str++;
        } else if (*color == '\0')
          // we check to see if this is a case of a color sequence
          str++;
        // and if it is, we advance the pointer to eat it up
        // and the last case is if it is a ^, in which case we just copy one
        // ^ into it, as we do here
        else if(*color != '\0') {
          str++;
          while (*color)
            *temp++ = *color++;
        } else
          *temp++ = *str++;
      } else {
        *temp++ = *str++;
      }
    }
  }
  // and terminate it with NULL properly
  *temp = '\0';
  
  return &buffer[0];
  */
}

void send_to_char(struct char_data * ch, const char * const messg, ...)
{
  if (!ch->desc || !messg)
    return;
  
  char internal_buffer[MAX_STRING_LENGTH];
  
  va_list argptr;
  va_start(argptr, messg);
  vsprintf(internal_buffer, messg, argptr);
  va_end(argptr);
  SEND_TO_Q(internal_buffer, ch->desc);
}

void send_to_char(const char *messg, struct char_data *ch)
{
  if (ch->desc && messg)
    SEND_TO_Q(messg, ch->desc);
}

void send_to_icon(struct matrix_icon * icon, const char * const messg, ...)
{
  if (!icon || !icon->decker || !icon->decker->ch || !icon->decker->ch->desc || !messg)
    return;
  
  char internal_buffer[MAX_STRING_LENGTH];
  
  va_list argptr;
  va_start(argptr, messg);
  vsprintf(internal_buffer, messg, argptr);
  va_end(argptr);
  SEND_TO_Q(internal_buffer, icon->decker->ch->desc);
  if (icon->decker->hitcher && icon->decker->hitcher->desc)
    SEND_TO_Q(internal_buffer, icon->decker->hitcher->desc);
}


void send_to_all(const char *messg)
{
  struct descriptor_data *i;
  
  if (messg)
    for (i = descriptor_list;i; i = i->next)
      if (!i->connected) {
        SEND_TO_Q(messg, i);
      }
}

void send_to_outdoor(const char *messg)
{
  struct descriptor_data *i;
  
  if (!messg || !*messg)
    return;
  
  for (i = descriptor_list; i; i = i->next)
    if (!i->connected && i->character && AWAKE(i->character) &&
        !(PLR_FLAGGED(i->character, PLR_WRITING) ||
          PLR_FLAGGED(i->character, PLR_EDITING) ||
          PLR_FLAGGED(i->character, PLR_MAILING) ||
          PLR_FLAGGED(i->character, PLR_MATRIX) ||
          PLR_FLAGGED(i->character, PLR_REMOTE)) &&
        OUTSIDE(i->character)) {
      SEND_TO_Q(messg, i);
    }
}

void send_to_driver(char *messg, struct veh_data *veh)
{
  struct char_data *i;
  
  if (messg)
    for (i = veh->people; i; i = i->next_in_veh)
      if (AFF_FLAGGED(i, AFF_PILOT) && i->desc)
        SEND_TO_Q(messg, i->desc);
}

void send_to_host(vnum_t room, const char *messg, struct matrix_icon *icon, bool needsee)
{
  struct matrix_icon *i;
  if (messg)
    for (i = matrix[room].icons; i; i = i->next_in_host)
      if (icon != i && i->decker)
        if (!needsee || (needsee && has_spotted(i, icon)))
          send_to_icon(i, messg);
}
void send_to_veh(const char *messg, struct veh_data *veh, struct char_data *ch, bool torig, ...)
{
  struct char_data *i;
  
  if (messg)
  {
    for (i = veh->people; i; i = i->next_in_veh)
      if (i != ch && i->desc) {
        if (!(!torig && AFF_FLAGGED(i, AFF_RIG)))
          SEND_TO_Q(messg, i->desc);
      }
    if (torig && veh->rigger && veh->rigger->desc)
      SEND_TO_Q(messg, veh->rigger->desc);
  }
}

void send_to_veh(const char *messg, struct veh_data *veh, struct char_data *ch, struct char_data *cha, bool torig)
{
  struct char_data *i;
  
  if (messg)
  {
    for (i = veh->people; i; i = i->next_in_veh)
      if (i != ch && i != cha && i->desc) {
        if (!(!torig && AFF_FLAGGED(i, AFF_RIG)))
          SEND_TO_Q(messg, i->desc);
      }
    if (torig && veh->rigger && veh->rigger->desc)
      SEND_TO_Q(messg, veh->rigger->desc);
  }
}

void send_to_room(const char *messg, struct room_data *room)
{
  struct char_data *i;
  struct veh_data *v;
  if (messg && room) {
    for (i = room->people; i; i = i->next_in_room)
      if (i->desc)
        if (!(PLR_FLAGGED(i, PLR_REMOTE) || PLR_FLAGGED(i, PLR_MATRIX)) && AWAKE(i))
          SEND_TO_Q(messg, i->desc);
    for (v = room->vehicles; v; v = v->next_veh)
      if (v->people)
        send_to_veh(messg, v, NULL, TRUE);
    
  }
}

const char *ACTNULL = "<NULL>";

#define CHECK_NULL(pointer, expression)  if ((pointer) == NULL) i = ACTNULL; else i = (expression);

// Uses NEW-- make sure you delete!
char* strip_ending_punctuation_new(const char* orig) {
  int len = strlen(orig);
  char* stripped = new char[len];
  
  strcpy(stripped, orig);
  
  char* c = stripped + len - 1;
  
  if (*c == '.' || *c == '!' || *c == '?')
    *c = '\0';
  
  return stripped;
}

const char *get_voice_perceived_by(struct char_data *speaker, struct char_data *listener) {
  static char voice_buf[150];
  struct remem *mem = NULL;
  
  if (IS_NPC(speaker))
    return GET_NAME(speaker);
  else {
    if (IS_SENATOR(listener)) {
      sprintf(voice_buf, "%s(%s)", speaker->player.physical_text.room_desc, GET_CHAR_NAME(speaker));
      return voice_buf;
    } else if ((mem = found_mem(GET_MEMORY(listener), speaker))) {
      sprintf(voice_buf, "%s(%s)", speaker->player.physical_text.room_desc, CAP(mem->mem));
      return voice_buf;
    } else {
      return speaker->player.physical_text.room_desc;
    }
  }
}

/* higher-level communication: the act() function */
// now returns the composed line, in case you need to capture it for some reason
const char *perform_act(const char *orig, struct char_data * ch, struct obj_data * obj,
                 void *vict_obj, struct char_data * to)
{
  extern char *make_desc(char_data *ch, char_data *i, char *buf, int act, bool dont_capitalize_a_an);
  const char *i = NULL;
  char *buf;
  struct char_data *vict;
  static char lbuf[MAX_STRING_LENGTH];
  struct remem *mem = NULL;
  char temp[MAX_STRING_LENGTH];
  buf = lbuf;
  vict = (struct char_data *) vict_obj;
  
  for (;;)
  {
    if (*orig == '$') {
      switch (*(++orig)) {
        case 'a':
          CHECK_NULL(obj, SANA(obj));
          break;
        case 'A':
          CHECK_NULL(vict_obj, SANA((struct obj_data *) vict_obj));
          break;
        case 'e':
          if (CAN_SEE(to, ch))
            i = HSSH(ch);
          else
            i = "it";
          break;
        case 'E':
          if (vict_obj) {
            if (CAN_SEE(to, vict))
              i = HSSH(vict);
            else
              i = "it";
          }
          break;
        case 'F':
          CHECK_NULL(vict_obj, fname((char *) vict_obj));
          break;
        case 'm':
          if (CAN_SEE(to, ch))
            i = HMHR(ch);
          else
            i = "them";
          break;
        case 'M':
          if (vict_obj) {
            if (CAN_SEE(to, vict))
              i = HMHR(vict);
            else
              i = "them";
          }
          break;
        case 'n':
          if (to == ch)
            i = "you";
          else if (CAN_SEE(to, ch)) {
            if (IS_SENATOR(to) && !IS_NPC(ch))
              i = GET_CHAR_NAME(ch);
            else
              i = make_desc(to, ch, buf, TRUE, TRUE);
          } else {
            if (IS_SENATOR(ch))
              i = "an invisible staff member";
            else
              i = "someone";
          }
          break;
        case 'N':
          if (to == vict)
            i = "you";
          else if (CAN_SEE(to, vict)) {
            if (IS_SENATOR(to) && !IS_NPC(vict))
              i = GET_CHAR_NAME(vict);
            else
              i = make_desc(to, vict, buf, TRUE, TRUE);
          } else {
            if (IS_SENATOR(vict))
              i = "an invisible staff member";
            else
              i = "someone";
          }
          break;
        case 'o':
          CHECK_NULL(obj, OBJN(obj, to));
          break;
        case 'O':
          CHECK_NULL(vict_obj, OBJN((struct obj_data *) vict_obj, to));
          break;
        case 'p':
          CHECK_NULL(obj, OBJS(obj, to));
          break;
        case 'P':
          CHECK_NULL(vict_obj, OBJS((struct obj_data *) vict_obj, to));
          break;
        case 's':
          if (CAN_SEE(to, ch))
            i = HSHR(ch);
          else
            i = "their";
          break;
        case 'S':
          if (vict_obj) {
            if (CAN_SEE(to, vict))
              i = HSHR(vict);
            else
              i = "their";
          }
          break;
        case 'T':
          CHECK_NULL(vict_obj, (char *) vict_obj);
          break;
        case 'v':
          i = get_voice_perceived_by(ch, to);
          break;
        case 'z':
          // You always know if it's you.
          if (to == ch) {
            i = "you";
          }
          
          // Staff ignore visibility checks.
          else if (IS_SENATOR(to)) {
            if (!IS_NPC(ch)) {
              i = GET_CHAR_NAME(ch);
            } else {
              i = make_desc(to, ch, buf, TRUE, TRUE);
            }
          }
          
          // If they're visible, it's simple.
          else if (CAN_SEE(to, ch)) {
            i = make_desc(to, ch, buf, TRUE, TRUE);
          }
          
          // If we've gotten here, the speaker is an invisible player or staff member.
          else {
            // Since $z is only used for speech, and since NPCs can't be remembered, we display their name when NPCs speak.
            if (IS_NPC(ch))
              i = GET_NAME(ch);
            else {
              if (IS_SENATOR(ch)) {
                i = "an invisible staff member";
              } else {
                // Easy case: If it's a modulator, use that voice.
                bool masked = FALSE;
                for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content) {
                  if (GET_OBJ_VAL(obj, 0) == CYB_VOICEMOD && GET_OBJ_VAL(obj, 3)) {
                    masked = TRUE;
                    break;
                  }
                }
                if (masked) {
                  sprintf(temp, "a masked voice");
                } else {
                  // Voice is new and must be deleted.
                  char* voice = strip_ending_punctuation_new(ch->player.physical_text.room_desc);
                  
                  if ((mem = found_mem(GET_MEMORY(to), ch)))
                    sprintf(temp, "%s(%s)", voice, CAP(mem->mem));
                  else
                    sprintf(temp, "%s", voice);
                  
                  i = temp;
                  
                  // Voice deleted here.
                  delete voice;
                }
              }
            }
          }
          break;
        case '$':
        default:
          //        i = ((char *)orig--);
          i = "$";
          break;
      }
      while ((*buf = *(i++)))
        buf++;
      orig++;
    } else if (!(*(buf++) = *(orig++)))
      break;
  }
  
  *(--buf) = '\r';
  *(++buf) = '\n';
  *(++buf) = '\0';
  
  SEND_TO_Q(CAP(lbuf), to->desc);
  return lbuf;
}

// TODO: stopped partway through editing this func, it is nonfunctional.
bool can_send_act_to_target(struct char_data *ch, bool hide_invisible, struct obj_data * obj, void *vict_obj, struct char_data *to, int type) {
#define SENDOK(ch) ((ch)->desc && AWAKE(ch) && !(PLR_FLAGGED((ch), PLR_WRITING) || PLR_FLAGGED((ch), PLR_EDITING) || PLR_FLAGGED((ch), PLR_MAILING) || PLR_FLAGGED((ch), PLR_CUSTOMIZE)) && (STATE(ch->desc) != CON_SPELL_CREATE))
  
  return SENDOK(to)
          && !(hide_invisible && ch && !CAN_SEE(to, ch))
          && (to != ch) && !(PLR_FLAGGED(to, PLR_REMOTE) || PLR_FLAGGED(to, PLR_MATRIX))
          && (type == TO_ROOM || type == TO_ROLLS || (to != vict_obj));
  
#undef SENDOK
}

/* Condition checking for perform_act(). Returns the string generated by p_a() provided that
    the type is TO_CHAR, TO_VICT, or TO_DECK. If no string is generated or type is other, returns NULL. */
const char *act(const char *str, int hide_invisible, struct char_data * ch,
         struct obj_data * obj, void *vict_obj, int type)
{
  struct char_data *to, *next;
  int sleep;
  
#define SENDOK(ch) ((ch)->desc && (AWAKE(ch) || sleep) && !(PLR_FLAGGED((ch), PLR_WRITING) || PLR_FLAGGED((ch), PLR_EDITING) || PLR_FLAGGED((ch), PLR_MAILING) || PLR_FLAGGED((ch), PLR_CUSTOMIZE)) && (STATE(ch->desc) != CON_SPELL_CREATE))
  
  if (!str || !*str)
    return NULL;
  
  /*
   * Warning: the following TO_SLEEP code is a hack.
   *
   * I wanted to be able to tell act to deliver a message regardless of sleep
   * without adding an additional argument.  TO_SLEEP is 128 (a single bit
   * high up).  It's ONLY legal to combine TO_SLEEP with one other TO_x
   * command.  It's not legal to combine TO_x's with each other otherwise.
   */
  
  /* check if TO_SLEEP is there, and remove it if it is. */
  if ((sleep = (type & TO_SLEEP)))
    type &= ~TO_SLEEP;
  
  if ( type == TO_ROLLS )
    sleep = 1;
  
  if (type == TO_CHAR || type == TO_CHAR_INCLUDE_RIGGER || type == TO_CHAR_FORCE)
  {
    if (ch && (TO_CHAR_FORCE
               || (SENDOK(ch) 
                   && !((!TO_CHAR_INCLUDE_RIGGER && PLR_FLAGGED(ch, PLR_REMOTE))
                        || PLR_FLAGGED(ch, PLR_MATRIX)))))
      return perform_act(str, ch, obj, vict_obj, ch);
    return NULL;
  }
  if (type == TO_VICT || type == TO_VICT_INCLUDE_RIGGER || type == TO_VICT_FORCE)
  {
    to = (struct char_data *) vict_obj;
    if (!to)
      return NULL;
      
    if (SENDOK(to) 
        && !(((!TO_VICT_INCLUDE_RIGGER && PLR_FLAGGED(to, PLR_REMOTE))
               || PLR_FLAGGED(to, PLR_MATRIX)) 
        && !sleep) 
        && !(hide_invisible && ch && !CAN_SEE(to, ch)))
      return perform_act(str, ch, obj, vict_obj, to);
    return NULL;
  }
  if (type == TO_DECK)
  {
    if ((to = (struct char_data *) vict_obj) && SENDOK(to) && PLR_FLAGGED(to, PLR_MATRIX))
      return perform_act(str, ch, obj, vict_obj, to);
    return NULL;
  }
  /* ASSUMPTION: at this point we know type must be TO_NOTVICT
   or TO_ROOM or TO_ROLLS */
  
  if (type == TO_VEH_ROOM)
    if (ch && ch->in_veh && !ch->in_veh->in_veh)
      to = ch->in_veh->in_room->people;
    else
      return NULL;
  else if (ch && ch->in_room)
    to = ch->in_room->people;
  else if (obj && obj->in_room)
    to = obj->in_room->people;
  else if (obj && obj->in_veh)
    to = obj->in_veh->people;
  else if (ch && ch->in_veh)
    to = ch->in_veh->people;
  else
  {
    mudlog("SYSERR: no valid target to act()!", NULL, LOG_SYSLOG, TRUE);
    sprintf(buf, "Invocation: act('%s', '%d', char_data, obj_data, vict_obj, '%d').", str, hide_invisible, type);
    if (ch) {
      sprintf(ENDOF(buf), "\r\nch: %s, in_room %s, in_veh %s",
              GET_CHAR_NAME(ch), ch->in_room ? GET_ROOM_NAME(ch->in_room) : "n/a",
              ch->in_veh ? GET_VEH_NAME(ch->in_veh) : "n/a");
    } else {
      strcat(buf, " ...No character.");
    }
    if (obj) {
      sprintf(ENDOF(buf), "\r\nobj: %s, in_room %s, in_veh %s",
              GET_OBJ_NAME(obj), obj->in_room ? GET_ROOM_NAME(obj->in_room) : "n/a",
              obj->in_veh ? GET_VEH_NAME(obj->in_veh) : "n/a");
    } else {
      strcat(buf, " ...No obj.");
    }
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  if (ch && IS_ASTRAL(ch) && !hide_invisible)
    hide_invisible = TRUE;
  
  if ( type == TO_ROLLS )
  {
    for (; to; to = to->next_in_room) {
      if (!PRF_FLAGGED(to, PRF_ROLLS))
        continue;
      if (SENDOK(to)
          && !(hide_invisible
               && ch
               && !CAN_SEE(to, ch)))
        perform_act(str, ch, obj, vict_obj, to);
    }
    return NULL;
  }
  
  for (; to; to = next)
  {
    if (to->in_veh)
      next = to->next_in_veh;
    else
      next = to->next_in_room;
    if (to == next) {
      sprintf(buf, "SYSERR: Encountered to=next infinite loop while looping over act string ''%s' for %s. Debug info: %s, type %d",
              str, 
              GET_CHAR_NAME(ch), 
              to->in_veh ? "in veh" : "in room",
              type);
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      return NULL;
    }
    if (can_send_act_to_target(ch, hide_invisible, obj, vict_obj, to, type))
      perform_act(str, ch, obj, vict_obj, to);
  }
  return NULL;
}
#undef SENDOK

#ifdef USE_DEBUG_CANARIES
void check_memory_canaries() {
  // Check every room in the world.
  for (int world_index = 0; world_index < top_of_world; world_index++) {
    // Check every direction that exists for this room.
    for (int direction_index = 0; direction_index <= DOWN; direction_index++) {
      if (world[world_index].dir_option[direction_index])
        assert(world[world_index].dir_option[direction_index]->canary == CANARY_VALUE);
    }
    // Check the room itself.
    assert(world[world_index].canary == CANARY_VALUE);
  }
  
  // Check every object in the game.
  for (int obj_index = 0; obj_index < top_of_objt; obj_index++) {
    assert(obj_proto[obj_index].canary == CANARY_VALUE);
  }
}
#endif

void msdp_update() {
  PERF_PROF_SCOPE(pr_, __func__);
  struct descriptor_data *d = NULL;
  struct char_data *ch = NULL;
  int PlayerCount = 0;
  
  for (d = descriptor_list; d; d = d->next) {
    if (!(ch = d->character))
      continue;
    
    ++PlayerCount;
    // General info
    MSDPSetString(d, eMSDP_CHARACTER_NAME, GET_CHAR_NAME(ch));
    MSDPSetString(d, eMSDP_SERVER_ID, "AwakeMUD CE");
    
    // Character info
    MSDPSetNumber(d, eMSDP_HEALTH, GET_PHYSICAL(ch));
    MSDPSetNumber(d, eMSDP_HEALTH_MAX, GET_MAX_PHYSICAL(ch));
    MSDPSetNumber(d, eMSDP_MANA, GET_MENTAL(ch));
    MSDPSetNumber(d, eMSDP_MANA_MAX, GET_MAX_MENTAL(ch));
    MSDPSetNumber(d, eMSDP_MONEY, GET_NUYEN(ch));
    
    // TODO: Many more things can be added here.
    
    MSDPUpdate(d); /* Flush all the dirty variables */
  }
  
  /* Ideally this should be called once at startup, and again whenever
   * someone leaves or joins the mud.  But this works, and it keeps the
   * snippet simple.  Optimise as you see fit.
   */
  MSSPSetPlayers( PlayerCount );
}
