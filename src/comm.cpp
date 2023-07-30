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
#include <chrono>

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

#include "telnet.hpp"
#include "awake.hpp"
#include "structs.hpp"
#include "utils.hpp"
#include "constants.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "house.hpp"
#include "memory.hpp"
#include "screen.hpp"
#include "newshop.hpp"
#include "quest.hpp"
#include "newmatrix.hpp"
#include "limits.hpp"
#include "protocol.hpp"
#include "perfmon.hpp"
#include "config.hpp"
#include "ignore_system.hpp"
#include "dblist.hpp"


const unsigned perfmon::kPulsePerSecond = PASSES_PER_SEC;

/* externs */
extern int restrict_mud;
/* extern FILE *player_fl; */
extern int DFLT_PORT;
extern int MAX_PLAYERS;
extern int MAX_DESCRIPTORS_AVAILABLE;
extern bool _OVERRIDE_ALLOW_PLAYERS_TO_USE_ROLLS_;

extern struct time_info_data time_info; /* In db.c */
extern char help[];

#ifdef USE_PRIVATE_CE_WORLD
extern void do_secret_ticks(int pulse);
#endif

bool _GLOBALLY_BAN_OPENVPN_CONNETIONS_ = FALSE;

/* local globals */
int connection_rapidity_tracker_for_dos = 0;
struct descriptor_data *descriptor_list = NULL; /* master desc list */
bool global_descriptor_list_invalidated = FALSE;
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
int get_from_q(struct txt_q * queue, char *dest, size_t dest_size, int *aliased);
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
int make_prompt(struct descriptor_data * point);
void check_idle_passwords(void);
void init_descriptor (struct descriptor_data *newd, int desc);
char *colorize(struct descriptor_data *d, const char *str, bool skip_check = FALSE);
void send_keepalives();
void msdp_update();
void increase_congregation_bonus_pools();
void send_nuyen_rewards_to_pcs();

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
void process_vehicle_decay(void);
void update_buildrepair(void);
void process_boost(void);
class memoryClass *Mem = new memoryClass();
void show_string(struct descriptor_data * d, char *input);
extern void update_paydata_market();
extern void warn_about_apartment_deletion();
void process_wheres_my_car();
extern int calculate_distance_between_rooms(vnum_t start_room_vnum, vnum_t target_room_vnum, bool ignore_roads);
void set_descriptor_canaries(struct descriptor_data *newd);

extern int modify_target_rbuf_magical(struct char_data *ch, char *rbuf, int rbuf_len);

#ifdef USE_DEBUG_CANARIES
void check_memory_canaries();
#endif

int gettimeofday(struct timeval *t, struct timezone *dummy)
{
  t->tv_usec =   std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  t->tv_sec = (int) (t->tv_usec / 1000);
  return 0;
}

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
  const char *dir;
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
          exit(ERROR_INVALID_ARGUMENTS_TO_BINARY);
        }
        break;
      case 'c':
        scheck = 1;
        log("Syntax check mode enabled.");
        break;
      case 'r':
        restrict_mud = 1;
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
      exit(ERROR_INVALID_ARGUMENTS_TO_BINARY);
    } else if ((port = atoi(argv[pos])) <= 1024) {
      fprintf(stderr, "Illegal port number. Port must be higher than 1024.\n");
      exit(ERROR_INVALID_ARGUMENTS_TO_BINARY);
    }
  }

  if (chdir(dir) < 0) {
    perror("Fatal error changing to data directory");
    exit(ERROR_INVALID_DATA_DIRECTORY);
  }

  log_vfprintf("Using %s as data directory.", dir);

  if (scheck) {
    boot_world();
    log("Done.");
    exit(EXIT_CODE_ZERO_ALL_IS_WELL);
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
  char name[MAX_NAME_LENGTH + 1];
  int desc;

  log ("COPYOVERLOG: Copyover recovery initiated.");

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
    fscanf (fp, "%d %20s %1023s %1023s\n", &desc, name, host, copyover_get);
    if (desc == -1)
      break;

    log_vfprintf("COPYOVERLOG: Restoring %s (%s)...", name, host);

    /* Write something, and check if it goes error-free */
    if (write_to_descriptor (desc, "\n\rJust kidding. Restoring from copyover...\n\r") < 0) {
       log_vfprintf("COPYOVERLOG:  - Failed to write to %s (%s), closing their descriptor.", name, host);
      close (desc); /* nope */
      continue;
    }

    /* create a new descriptor */
    CREATE(d, struct descriptor_data, 1);
    memset ((char *) d, 0, sizeof (struct descriptor_data));
    init_descriptor (d,desc); /* set up various stuff */

    strlcpy(d->host, host, sizeof(d->host));
    d->next = descriptor_list;
    descriptor_list = d;

    // Restore KaVir protocol data.
    CopyoverSet(d, copyover_get);


    d->connected = CON_CLOSE;

    /* Now, find the pfile */

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
      log_vfprintf("COPYOVERLOG:  - Playerfile for %s (%s) was not found??", name, host);
      close_socket (d);
    } else /* ok! */
    {
      long load_room, last_room;
      write_to_descriptor (desc, "\n\rCopyover recovery complete. Welcome back!\n\r");
      d->connected = CON_PLAYING;
      reset_char(d->character);
      d->character->next = character_list;
      character_list = d->character;
      // Since we're coming from copyover if the char is in a vehicle and is the owner throw him at the front seat
      // otherwise throw him at the back. Because we don't save frontness and we'll assume the owner is driving
      // anyway.
      last_room = real_room(GET_LAST_IN(d->character)) ?  real_room(GET_LAST_IN(d->character))  : 0;
      if (d->character->player_specials->saved.last_veh && last_room) {
        for (struct veh_data *veh = world[last_room].vehicles; veh; veh = veh->next) {
          if (veh->idnum == d->character->player_specials->saved.last_veh) {
            if (veh->damage < VEH_DAM_THRESHOLD_DESTROYED) {
              if (veh->owner == GET_IDNUM(d->character))
                d->character->vfront = TRUE;
              else
                d->character->vfront = FALSE;

              char_to_veh(veh, d->character);
            }
            break;
          }
        }
      }
      else {
        if (real_room(GET_LAST_IN(d->character)) > 0)
          load_room = real_room(GET_LAST_IN(d->character));
        else
          load_room = real_room(GET_LOADROOM(d->character));
        char_to_room(d->character, &world[load_room]);
        //look_at_room(d->character, 0, 0);
      }
    }
  }
  fclose (fp);

  // Regenerate everyone's subscriber lists.
  log("COPYOVERLOG: Regenerating subscriber lists.");
  for (struct veh_data *veh = veh_list; veh; veh = veh->next) {
    if (!veh->sub || !veh->owner)
      continue;

    for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
      struct char_data *operative_character = (d->original && GET_IDNUM(d->original) == veh->owner ? d->original : d->character);
      
      if (operative_character && GET_IDNUM(operative_character) != veh->owner)
        continue;

      struct veh_data *f = NULL;
      for (f = operative_character->char_specials.subscribe; f; f = f->next_sub)
        if (f == veh)
          break;

      if (!f) {
        veh->next_sub = operative_character->char_specials.subscribe;

        // Doubly link it into the list.
        if (operative_character->char_specials.subscribe)
          operative_character->char_specials.subscribe->prev_sub = veh;

        operative_character->char_specials.subscribe = veh;
      }

      break;
    }
  }

  // Force all player characters to look now that everyone's properly loaded.
  log("COPYOVERLOG: Forcing look.");
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    if (d->character && !PRF_FLAGGED(d->character, PRF_SCREENREADER))
      look_at_room(d->character, 0, 0);
  }

  log("COPYOVERLOG: Copyover complete. Have a nice day!");
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
    exit(EXIT_CODE_REBOOTING);
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
    exit(ERROR_COULD_NOT_CREATE_SOCKET);
  }
#if defined(SO_SNDBUF)
  opt = LARGE_BUFSIZE + GARBAGE_SPACE;
  if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *) &opt, sizeof(opt)) < 0) {
    perror("setsockopt SNDBUF");
    exit(ERROR_COULD_NOT_SET_SOCKET_OPTIONS);
  }
#endif

#if defined(SO_REUSEADDR)
  opt = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
    perror("setsockopt REUSEADDR");
    exit(ERROR_COULD_NOT_REUSE_ADDRESS);
  }
#endif

// This allows to bind on the same port. Commenting it out as it shouldn't be here
// in the first place.
/*#if defined(SO_REUSEPORT)
  opt = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (char *) &opt, sizeof(opt)) < 0) {
    perror("setsockopt REUSEPORT");
    exit(ERROR_COULD_NOT_REUSE_PORT);
  }
#endif
*/
#if defined(SO_LINGER)
  {
    struct linger ld;

    ld.l_onoff = 0;
    ld.l_linger = 0;
    if (setsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&ld, sizeof(ld)) < 0) {
      perror("setsockopt LINGER");
      exit(ERROR_COULD_NOT_SET_LINGER);
    }
  }
#endif

  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);

  if (::bind(s, (struct sockaddr *) & sa, sizeof(sa)) < 0) {
    perror("bind");
    close(s);
    exit(ERROR_COULD_NOT_BIND_SOCKET);
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
      exit(ERROR_GETRLIMIT_FAILED);
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
      exit(ERROR_SYSCONF_FAILED);
    }
  }
#endif

  /* now calculate max _players_ based on max descs */
  max_descs = MIN(MAX_PLAYERS, max_descs - NUM_RESERVED_DESCS);

  if (max_descs <= 0) {
    log_vfprintf("Non-positive max player limit!  (Set at %d using %s).",
        max_descs, method);

    exit(ERROR_YOU_SET_AN_IMPOSSIBLE_PLAYER_LIMIT);
  }

  log_vfprintf("Setting player limit to %d using %s.", max_descs, method);

  return max_descs;
}

void alarm_handler(int signal) {
#ifdef __CYGWIN__
  log("IGNORING alarm handler due to Cygwin usage! The program may be stuck, feel free to ctrl-C out of it.");
#else
  log("alarm_handler: Alarm signal has been triggered. Killing the MUD to break out of a suspected infinite loop.\r\n");
  raise(SIGSEGV);
#endif
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
    alarm(SECONDS_TO_WAIT_FOR_HUNG_MUD_TO_RECOVER_BEFORE_KILLING_IT);

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

/*      Silenced this because it was spammy and didn't provide much value.
        if (usage_pcnt >= 100)
        {
          char buf[MAX_STRING_LENGTH];
          snprintf(buf, sizeof(buf), "Pulse usage %f >= 100%%. Trace info:", usage_pcnt);
          log(buf);
          PERF_prof_repr_pulse(buf, sizeof(buf));
          log(buf);
        }
*/
      }

      /* just in case, re-calculate after PERF logging to figure out for how long we have to sleep */
      gettimeofday(&now, (struct timezone *) 0);
      timespent = timediff(&now, &last_time);
      timeout = timediff(&opt_time, &timespent);

      /* sleep until the next 0.1 second mark */
      if (select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &timeout) < 0)
        if (errno != EINTR) {
          perror("Select sleep");
          exit(ERROR_COULD_NOT_RECOVER_FROM_SELECT_SLEEP);
        }

      // Reset our alarm timer.
      alarm(SECONDS_TO_WAIT_FOR_HUNG_MUD_TO_RECOVER_BEFORE_KILLING_IT);
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
      // Clear the descriptor list invalidator, just in case it was set in the above code blocks.
      // We only care about it in this block because some commands (ex: purge, dc) can alter the descriptor list
      // and invalidate the NEXT descriptor instead of just the current one.
      global_descriptor_list_invalidated = FALSE;

      PERF_PROF_SCOPE( pr_commands_, "process commands");
      for (d = descriptor_list; d; d = next_d) {
        next_d = d->next;

        if ((--(d->wait) <= 0) && get_from_q(&d->input, comm, sizeof(comm), &aliased)) {
          if (d->character) {
            d->character->char_specials.last_timer = d->character->char_specials.timer;
            d->character->char_specials.timer = 0;
            if (d->original) {
              d->original->char_specials.last_timer = d->original->char_specials.timer;
              d->original->char_specials.timer = 0;
            }
            if (d->connected == CON_PLAYING && GET_WAS_IN(d->character)) {
              if (GET_WAS_IN(d->character) == get_ch_in_room(d->character)) {
                mudlog("Debug: get_was_in == get_ch_in_room on return check, won't print.", d->character, LOG_SYSLOG, TRUE);
              } else {
                if (d->character->in_room)
                  char_from_room(d->character);
                char_to_room(d->character, GET_WAS_IN(d->character));
                act("$n has returned.", TRUE, d->character, 0, 0, TO_ROOM);
              }
              GET_WAS_IN(d->character) = NULL;
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
                get_from_q(&d->input, comm, sizeof(comm), &aliased);
            }
            command_interpreter(d->character, comm, GET_CHAR_NAME(d->character)); /* send it to interpreter */
          }
        }

        // If we've invalidated the descriptor list in the process of an interpreted command, bail out.
        if (global_descriptor_list_invalidated) {
          global_descriptor_list_invalidated = FALSE;
          log("Global descriptor list was invalidated- skipping remaining inputs for this cycle.");
          break;
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
      for (d = descriptor_list; d; d = next_d) {
        next_d = d->next;

        /* KaVir's protocol snippet. The last sent data was OOB, so do NOT draw the prompt */
        if (d->pProtocol->WriteOOB)
          continue;

        if (d->prompt_mode) {
          if (make_prompt(d)) {
            close_socket(d);
          } else {
            d->prompt_mode = 0;
          }
        }
      }
    }

    /* handle heartbeat stuff */
    /* Note: pulse now changes every 0.10 seconds  */

    pulse++;

#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
    // Every RL second.
    if (!(pulse % PASSES_PER_SEC)) {
      void verify_every_pointer_we_can_think_of();

      verify_every_pointer_we_can_think_of();
    }
#endif

    if (!(pulse % PULSE_ZONE)) {
      zone_update();
      phone_check();
      process_autonav();
      process_vehicle_decay();
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
#ifdef USE_PRIVATE_CE_WORLD
      do_secret_ticks(pulse);
#endif
    }

    // Every 5 MUD minutes
    if (!(pulse % (5 * SECS_PER_MUD_MINUTE * PASSES_PER_SEC))) {
      if (!(pulse % (SECS_PER_MUD_HOUR * PASSES_PER_SEC)))
        process_regeneration(1);
      else
        process_regeneration(0);
      taxi_leaves();
      process_wheres_my_car();
    }

    // Every 29 MUD minutes
    if (!(pulse % (29 * SECS_PER_MUD_MINUTE * PASSES_PER_SEC))) {
      // Run through vehicles and rectify their occupancy.
      for (struct veh_data *veh = veh_list; veh; veh = veh->next) {
        if (!veh->people)
          continue;

        struct char_data *last_tch = veh->people;
        for (struct char_data *tch = last_tch->next_in_veh; tch; tch = tch->next_in_veh) {
          if (tch->in_veh != veh) {
            mudlog("Warning: Character is in a vehicle's people list, but not in that vehicle. Rectifying.", tch, LOG_SYSLOG, TRUE);
            last_tch->next_in_veh = tch->next_in_veh;
          }
          last_tch = tch;
        }
      }
    }

    // Every 59 MUD minutes
    if (!(pulse % (59 * SECS_PER_MUD_MINUTE * PASSES_PER_SEC))) {
      save_vehicles(FALSE);
      update_paydata_market();
    }

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

      // Reset DOS tracker.
      connection_rapidity_tracker_for_dos = 0;
    }

    // Every 30 IRL seconds
    if (!(pulse % (SECONDS_BETWEEN_CONGREGATION_POOL_GAINS * PASSES_PER_SEC))) {
      increase_congregation_bonus_pools();
    }

    // Every IRL minute
    if (!(pulse % (60 * PASSES_PER_SEC))) {
      check_idling();
      send_keepalives();
      // johnson_update();
      process_boost();
    }

    // By default, every IRL hour, but configurable in config.h.
    if (!(pulse % (60 * PASSES_PER_SEC * IDLE_NUYEN_MINUTES_BETWEEN_AWARDS))) {
      send_nuyen_rewards_to_pcs();
    }

    // Every IRL day
    if (!(pulse % (60 * PASSES_PER_SEC * 60 * 24))) {
      warn_about_apartment_deletion();

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

#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
    // Every RL second, part II: Catching things AFTER pulse updates.
    if (!(pulse % PASSES_PER_SEC)) {
      void verify_every_pointer_we_can_think_of();

      verify_every_pointer_we_can_think_of();
    }
#endif

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

  if (write_to_descriptor(d->descriptor, keepalive) < 0) {
    mudlog("SYSERR: Failed to write keepalive data to descriptor.", d->character, LOG_SYSLOG, TRUE);
  }
}

// Sends keepalives to everyone who's enabled them.
void send_keepalives() {
  PERF_PROF_SCOPE(pr_, __func__);
  for (struct descriptor_data *d = descriptor_list; d; d = d->next)
    if (d->character && PRF_FLAGGED(d->character, PRF_KEEPALIVE)) {
      keepalive(d);

      // Set the OOB flag so that we don't re-draw the prompt.
      d->pProtocol->WriteOOB = TRUE;
    }
}

int make_prompt(struct descriptor_data * d)
{
  char *prompt;

  const char *data = NULL;

  if (d->str) {
    if (D_PRF_FLAGGED(d, PRF_SCREENREADER)) {
      data = "Compose mode, press return twice on empty line for a new paragraph, write the at symbol on empty line to quit]: ";
    } else {
      data = "Compose mode, press <return> twice on empty line for a new paragraph, write @ on empty line to quit]: ";
    }
  }
  else if (d->showstr_point)
    data = " Press [return] to continue, [q] to quit ";
  else if (D_PRF_FLAGGED(d, PRF_NOPROMPT)) {
    // Anything below this line won't render for noprompters.
    return 0;
  }
  else if (STATE(d) == CON_POCKETSEC)
    data = " > ";

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
      data = "> ";
    else if (!strchr(prompt, '@')) {
      data = colorize(d, prompt);
    } else {
      char temp[MAX_INPUT_LENGTH], str[20];
      int i = 0, j, physical;

      for (; *prompt; prompt++) {
        if (*prompt == '@' && *(prompt+1)) {
          prompt++;
          switch(*prompt) {
            case 'a':       // astral pool
              snprintf(str, sizeof(str), "%d", GET_ASTRAL(d->character));
              break;
            case 'A':
              if (GET_EQ(d->character, WEAR_WIELD)) {

                if (IS_GUN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD), 3)))
                  switch (GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD), 11)) {
                    case MODE_SS:
                      snprintf(str, sizeof(str), "SS");
                      break;
                    case MODE_SA:
                      snprintf(str, sizeof(str), "SA");
                      break;
                    case MODE_BF:
                      snprintf(str, sizeof(str), "BF");
                      break;
                    case MODE_FA:
                      snprintf(str, sizeof(str), "FA(%d)", GET_OBJ_TIMER(GET_EQ(d->character, WEAR_WIELD)));
                      break;
                    default:
                      snprintf(str, sizeof(str), "NA");
                  }
                else strlcpy(str, "ML", sizeof(str));

              } else
                strlcpy(str, "N/A", sizeof(str));
              break;
            case 'b':       // ballistic
              snprintf(str, sizeof(str), "%d", GET_BALLISTIC(d->character));
              break;
            case 'c':       // combat pool
              snprintf(str, sizeof(str), "%d", GET_COMBAT(d->character));
              break;
            case 'C':       // persona condition
              if (ch->persona)
                snprintf(str, sizeof(str), "%d", ch->persona->condition);
              else
                snprintf(str, sizeof(str), "NA");
              break;
            case 'd':       // defense pool
              snprintf(str, sizeof(str), "%d", GET_DEFENSE(d->character));
              break;
            case 'D':
              snprintf(str, sizeof(str), "%d", GET_BODY(d->character));
              break;
            case 'e':
              if (ch->persona)
                snprintf(str, sizeof(str), "%d", ch->persona->decker->active);
              else
                snprintf(str, sizeof(str), "0");
              break;
            case 'E':
              if (ch->persona && ch->persona->decker->deck)
                snprintf(str, sizeof(str), "%d", GET_OBJ_VAL(ch->persona->decker->deck, 2));
              else
                snprintf(str, sizeof(str), "0");
              break;
            case 'f':
              snprintf(str, sizeof(str), "%d", GET_CASTING(d->character));
              break;
            case 'F':
              snprintf(str, sizeof(str), "%d", GET_DRAIN(d->character));
              break;
            case 'g':       // current ammo
              if (GET_EQ(d->character, WEAR_WIELD) &&
                  IS_GUN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD), 3)))
                if (GET_EQ(d->character, WEAR_WIELD)->contains) {
                  snprintf(str, sizeof(str), "%d", MIN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD), 5),
                                         GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD)->contains, 9)));
                } else
                  snprintf(str, sizeof(str), "0");
                else
                  snprintf(str, sizeof(str), "0");
              break;
            case 'G':       // max ammo
              if (GET_EQ(d->character, WEAR_WIELD) &&
                  IS_GUN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD), 3)))
                snprintf(str, sizeof(str), "%d", GET_OBJ_VAL(GET_EQ(d->character, WEAR_WIELD), 5));
              else
                snprintf(str, sizeof(str), "0");
              break;
            case 'h':       // hacking pool
              if (ch->persona)
                snprintf(str, sizeof(str), "%d", GET_REM_HACKING(d->character));
              else
                snprintf(str, sizeof(str), "%d", GET_HACKING(d->character));
              break;
            case 'H':
              snprintf(str, sizeof(str), "%d%cM", (time_info.hours % 12 == 0 ? 12 : time_info.hours % 12), (time_info.hours >= 12 ? 'P' : 'A'));
              break;
            case 'i':       // impact
              snprintf(str, sizeof(str), "%d", GET_IMPACT(d->character));
              break;
            case 'j':
              snprintf(str, sizeof(str), "%d", modify_target(d->character));
              break;
            case 'k':       // karma
              snprintf(str, sizeof(str), "%0.2f", ((float)GET_KARMA(ch) / 100));
              break;
            case 'l':       // current weight
              snprintf(str, sizeof(str), "%.2f", IS_CARRYING_W(d->character));
              break;
            case 'L':       // max weight
              snprintf(str, sizeof(str), "%d", CAN_CARRY_W(d->character));
              break;
            case 'm':       // current mental
            case '*':       // current mental, but subtracted from 10 to give damage boxes taken instead
              physical = (int)(GET_MENTAL(ch) / 100);
              if (IS_JACKED_IN(ch)) {
                physical = 10;
              } else {
                for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
                  if (GET_BIOWARE_TYPE(bio) == BIO_PAINEDITOR && GET_BIOWARE_IS_ACTIVATED(bio)) {
                    physical = 10;
                    break;
                  }
                }
              }

              if (*prompt != 'm') {
                physical = 10 - physical;
              }

              snprintf(str, sizeof(str), "%d", physical);
              break;
            case 'M':       // max mental
              snprintf(str, sizeof(str), "%d", (int)(GET_MAX_MENTAL(d->character) / 100));
              break;
            case 'n':       // nuyen
              snprintf(str, sizeof(str), "%ld", GET_NUYEN(d->character));
              break;
            case 'o':       // offense pool
              snprintf(str, sizeof(str), "%d", GET_OFFENSE(d->character));
              break;
            case 'p':       // current physical
            case '&':       // current physical, but subtracted from 10 to give damage boxes taken instead
              physical = (int)(GET_PHYSICAL(ch) / 100);

              if (IS_JACKED_IN(ch)) {
                physical = 10;
              } else {
                for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
                  if (GET_BIOWARE_TYPE(bio) == BIO_PAINEDITOR && GET_BIOWARE_IS_ACTIVATED(bio)) {
                    physical = 10;
                    break;
                  }
                }
              }

              if (*prompt != 'p') {
                physical = 10 - physical;
              }

              snprintf(str, sizeof(str), "%d", physical);
              
              break;
            case 'P':       // max physical
              snprintf(str, sizeof(str), "%d", (int)(GET_MAX_PHYSICAL(d->character) / 100));
              break;
            case 'q': // Selected Domain, plus Sky if available
              {
                struct room_data *room = get_ch_in_room(d->character);
                if (room && SECT(room) != SPIRIT_FOREST && SECT(room) != SPIRIT_HEARTH && !ROOM_FLAGGED(room, ROOM_INDOORS)) {
                  snprintf(str, sizeof(str), "%s, Sky", spirit_name[GET_DOMAIN(ch)]);
                } else {
                  strlcpy(str, GET_DOMAIN(ch) == SPIRIT_HEARTH ? "Hearth" : spirit_name[GET_DOMAIN(ch)], sizeof(str));
                }
              }
              break;
            case 'r':
              if (ch->persona && ch->persona->decker->deck)
                snprintf(str, sizeof(str), "%d", GET_OBJ_VAL(ch->persona->decker->deck, 3) - GET_OBJ_VAL(ch->persona->decker->deck, 5));
              else
                snprintf(str, sizeof(str), "0");
              break;
            case 'R':
              if (ch->persona && ch->persona->decker && ch->persona->decker->deck)
                snprintf(str, sizeof(str), "%d", GET_OBJ_VAL(ch->persona->decker->deck, 3));
              else
                snprintf(str, sizeof(str), "0");
              break;
            case 's':       // current ammo
              if (GET_EQ(d->character, WEAR_HOLD) &&
                  IS_GUN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_HOLD), 3)))
                if (GET_EQ(d->character, WEAR_HOLD)->contains) {
                  snprintf(str, sizeof(str), "%d", MIN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_HOLD), 5),
                                         GET_OBJ_VAL(GET_EQ(d->character, WEAR_HOLD)->contains, 9)));
                } else
                  snprintf(str, sizeof(str), "0");
                else
                  snprintf(str, sizeof(str), "0");
              break;
            case 'S':       // max ammo
              if (GET_EQ(d->character, WEAR_HOLD) &&
                  IS_GUN(GET_OBJ_VAL(GET_EQ(d->character, WEAR_HOLD), 3)))
                snprintf(str, sizeof(str), "%d", GET_OBJ_VAL(GET_EQ(d->character, WEAR_HOLD), 5));
              else
                snprintf(str, sizeof(str), "0");
              break;
            case 't':       // magic pool
              snprintf(str, sizeof(str), "%d", GET_MAGIC(d->character));
              break;
            case 'T':
              snprintf(str, sizeof(str), "%d", GET_SUSTAINED_NUM(d->character));
              break;
            case 'u':
              snprintf(str, sizeof(str), "%d", GET_SDEFENSE(d->character));
              break;
            case 'U':
              snprintf(str, sizeof(str), "%d", GET_REFLECT(d->character));
              break;
            case 'v':
              {
                struct room_data *room = get_ch_in_room(d->character);

                if (!room) {
                  strlcpy(str, "@v", sizeof(str));
                } else if (GET_REAL_LEVEL(d->character) >= LVL_BUILDER || PLR_FLAGGED(d->character, PLR_PAID_FOR_VNUMS)) {
                  snprintf(str, sizeof(str), "%ld", room->number);
                } else if (GET_REAL_LEVEL(d->character) <= LVL_MORTAL) {
                  strlcpy(str, "^WHELP SYSPOINTS^n", sizeof(str));
                } else {
                  strlcpy(str, "@v", sizeof(str));
                }
              }
              break;
            case 'w':
              snprintf(str, sizeof(str), "%d", GET_INVIS_LEV(d->character));
              break;
            case 'W':
              snprintf(str, sizeof(str), "%d", GET_INCOG_LEV(d->character));
              break;
            case 'z':
              if (GET_REAL_LEVEL(d->character) >= LVL_BUILDER)
                snprintf(str, sizeof(str), "%d", d->character->player_specials->saved.zonenum);
              else
                strlcpy(str, "@z", sizeof(str));
              break;
            case 'Z':
              if (d->original) {
                // Projecting: Survival ticks left.
                snprintf(str, sizeof(str), "%d", (int) ceil(((float)GET_ESS(d->original)) / 100));
              } else {
                // Meatform: Your essence.
                snprintf(str, sizeof(str), "%.2f", ((float)GET_ESS(d->character)) / 100);
              }
              break;
            case '@':
              strlcpy(str, "@", sizeof(str));
              break;
            case '!':
              strlcpy(str, "\r\n", sizeof(str));
              break;
            default:
              snprintf(str, sizeof(str), "@%c", *prompt);
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
      const char *final_str = ProtocolOutput(d, temp, &size, DO_APPEND_GA);
      data = final_str;
    }
  }

  if (data) {
    // We let our caller handle the error here.
    if (write_to_descriptor(d->descriptor, data) < 0) {
      mudlog("Error writing prompt to descriptor, aborting.", d->character, LOG_SYSLOG, TRUE);
      return -1;
    }
  }

  return 0;
}

void write_to_q(const char *txt, struct txt_q * queue, int aliased)
{
  struct txt_block *temp = new txt_block;
  size_t text_size = strlen(txt) + 1;
  temp->text = new char[text_size];
  strlcpy(temp->text, txt, text_size);
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

int get_from_q(struct txt_q * queue, char *dest, size_t dest_size, int *aliased)
{
  struct txt_block *tmp;

  /* queue empty? */
  if (!queue->head)
    return 0;

  tmp = queue->head;
  strlcpy(dest, queue->head->text, dest_size);
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
  while (get_from_q(&d->input, buf2, sizeof(buf2), &dummy))
    ;
}

/* Add a new string to a player's output queue */
void write_to_output(const char *unmodified_txt, struct descriptor_data *t)
{
  int size = strlen(unmodified_txt);

  if (t == NULL)
    return;

  #ifdef PROTO_DEBUG
  log_vfprintf("Entering write_to_output with unmodified_txt ''%s'' with size %d.", unmodified_txt, size);
  #endif

  // Process text per KaVir's protocol snippet.
  const char *txt = ProtocolOutput(t, unmodified_txt, &size, DONT_APPEND_GA);
  if (t->pProtocol->WriteOOB > 0) {
    #ifdef PROTO_DEBUG
    log_vfprintf("Decrementing WriteOOB (%d to %d).", t->pProtocol->WriteOOB, --t->pProtocol->WriteOOB);
    #else
    --t->pProtocol->WriteOOB;
    #endif
  }

  // Now, re-calculate the size of the written content, since ProtocolOutput produces different content.
  size = strlen(txt);

  #ifdef PROTO_DEBUG
  log_vfprintf("Got cooked content ''%s'' with size=%d.", txt, size);
  #endif

  // txt = colorize(t, (char *)txt);

  /* if we're in the overflow state already, ignore this new output */
  if (t->bufptr < 0) {
    #ifdef PROTO_DEBUG
    log_vfprintf("Ignoring cooked content: t->bufptr = %d, so we're in the overflow state. Aborting.", t->bufptr);
    #endif
    return;
  }

  /* if we have enough space, just write to buffer and that's it! */
  if (t->bufspace > size)
  {
    #ifdef PROTO_DEBUG
    log_vfprintf("Have enough space in t->bufspace (%d >= %d). Writing cooked content into t->output, which is starting at ''%s''.", t->bufspace, size, txt, t->output);
    #endif
    strlcpy(t->output + t->bufptr, txt, t->bufspace);
    assert(t->small_outbuf_canary == 31337);
    assert(t->output_canary == 31337);
    t->bufspace -= size;
    t->bufptr += size;
    #ifdef PROTO_DEBUG
    log_vfprintf("After operations, t->output is now ''%s'', with bufspace=%d. Successfully completed run.", t->output, t->bufspace);
    #endif
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
    #ifdef PROTO_DEBUG
    log_vfprintf("Whoops, not enough space. Going to overflow state and discarding cooked content. t->large_outbuf=%d, total size = %d. t->bufptr becomes -1, and buf_overflows is now %d.", t->large_outbuf, size + strlen(t->output), buf_overflows);
    #endif
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

  strlcpy(t->large_outbuf->text, t->output, LARGE_BUFSIZE);     /* copy to big buffer */
  t->output = t->large_outbuf->text;    /* make big buffer primary */
  strlcat(t->output, txt, LARGE_BUFSIZE);       /* now add new text */
  assert(t->small_outbuf_canary == 31337);

  /* calculate how much space is left in the buffer */
  t->bufspace = LARGE_BUFSIZE - 1 - strlen(t->output);

  /* set the pointer for the next write */
  t->bufptr = strlen(t->output);

  #ifdef PROTO_DEBUG
  log_vfprintf("That was lengthy, I had to switch to a large buffer. t->output is now ''%s'' (len %d, which leaves %d space remaining out of %d).", t->output, t->bufptr, t->bufspace, LARGE_BUFSIZE);
  #endif
}

/* ******************************************************************
 *  socket handling                                                  *
 ****************************************************************** */

void set_descriptor_canaries(struct descriptor_data *newd) {
  newd->canary = 31337;
  newd->small_outbuf_canary = 31337;
  newd->inbuf_canary = 31337;
  newd->output_canary = 31337;
  newd->last_input_canary = 31337;
  newd->input_and_character_canary = 31337;
}

void init_descriptor (struct descriptor_data *newd, int desc)
{
  static int last_desc = 0;  /* last descriptor number */

  set_descriptor_canaries(newd);

  newd->descriptor = desc;
  newd->connected = CON_GET_NAME;
  newd->idle_ticks = 0;
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

  // Calculate the long address.
  addr = ntohl(peer.sin_addr.s_addr);

  // Block it if we're getting too many openings in a limited time window.
  if (connection_rapidity_tracker_for_dos++ >= DOS_DENIAL_THRESHOLD) {
    if (write_to_descriptor(desc, "Sorry, we're unable to service your request right now. Please try again in a few minutes.\r\n") >= 0)
      close(desc);
    log_vfprintf("DOSLOG: ERROR: Denied incoming request from long-format address %ld - DOS protection.", addr);
    return 0;
  }
  else if (connection_rapidity_tracker_for_dos >= DOS_SLOW_NS_THRESHOLD) {
    if (!nameserver_is_slow) {
      mudlog("^RDOSLOG: WARNING: Received too many connections in a short span of time. Engaging slow NS and skipping player count limit to mitigate potential DOS.^g", NULL, LOG_SYSLOG, TRUE);
      nameserver_is_slow = TRUE;
    }
  }
  else {
    /* make sure we have room for it */
    for (newd = descriptor_list; newd; newd = newd->next)
      sockets_connected++;

    if (sockets_connected >= max_players)
    {
      if (write_to_descriptor(desc, "Sorry, Awakened Worlds is full right now... try again later!  :-)\r\n") >= 0)
        close(desc);
      return 0;
    }
  }

  /* create a new descriptor */
  newd = new descriptor_data;
  memset((char *) newd, 0, sizeof(struct descriptor_data));
  // Set our canaries.
  set_descriptor_canaries(newd);

  /* find the sitename */
  time_t gethostbyaddr_timer = time(0);
  if (nameserver_is_slow || !(from = gethostbyaddr((char *) &peer.sin_addr,
                                                   sizeof(peer.sin_addr), AF_INET)))
  {
    if (!nameserver_is_slow)
      perror("gethostbyaddr");
    snprintf(newd->host, sizeof(newd->host), "%03u.%03u.%03u.%03u", (int) ((addr & 0xFF000000) >> 24),
            (int) ((addr & 0x00FF0000) >> 16), (int) ((addr & 0x0000FF00) >> 8),
            (int) ((addr & 0x000000FF)));
  } else
  {
    strlcpy(newd->host, from->h_name, HOST_LENGTH);
    *(newd->host + HOST_LENGTH) = '\0';

    time_t time_delta = time(0) - gethostbyaddr_timer;
    if (time_delta > THRESHOLD_IN_SECONDS_FOR_SLOWNS_AUTOMATIC_ACTIVATION) {
      snprintf(buf2, sizeof(buf2), "%03u.%03u.%03u.%03u", (int) ((addr & 0xFF000000) >> 24),
              (int) ((addr & 0x00FF0000) >> 16), (int) ((addr & 0x0000FF00) >> 8),
              (int) ((addr & 0x000000FF)));
      snprintf(buf, sizeof(buf), "^YThe resolution of host '%s' [%s] took too long at %ld second(s). Automatically engaging slow NS defense.^g", newd->host, buf2, time_delta);
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
      nameserver_is_slow = TRUE;
    }
  }

  /* determine if the site is banned */
  if (isbanned(newd->host) == BAN_ALL) {
#ifdef USE_PRIVATE_CE_WORLD
    write_to_descriptor(desc, "Sorry, this IP address has been banned. If you believe this to be in error, contact luciensadi@gmail.com.\r\n");
#else
    write_to_descriptor(desc, "Sorry, this IP address has been banned.\r\n");
#endif
    close(desc);
    snprintf(buf2, sizeof(buf2), "Connection attempt denied from banned site [%s]", newd->host);
    mudlog(buf2, NULL, LOG_BANLOG, TRUE);
    DELETE_AND_NULL(newd);
    return 0;
  }

  if (peer.sin_port == 1194 && _GLOBALLY_BAN_OPENVPN_CONNETIONS_) {
    close(desc);
    snprintf(buf2, sizeof(buf2), "Connection attempt denied from NON-BANNED site [%s] using VPN origin port 1194", newd->host);
    mudlog(buf2, NULL, LOG_BANLOG, TRUE);
    DELETE_AND_NULL(newd);
    return 0;
  }

  if (nameserver_is_slow) {
    log_vfprintf("DOSLOG: Connection from [%03u.%03u.%03u.%03u port %d%s] (slow nameserver mode).",
                 (int) ((addr & 0xFF000000) >> 24),
                 (int) ((addr & 0x00FF0000) >> 16),
                 (int) ((addr & 0x0000FF00) >> 8),
                 (int) ((addr & 0x000000FF)),
                 peer.sin_port,
                 peer.sin_port == 1194 ? "(OpenVPN?)" : ""
                );
  }
  else {
    log_vfprintf("DOSLOG: Connection from [%s (%03u.%03u.%03u.%03u) port %d%s].",
                 newd->host,
                 (int) ((addr & 0xFF000000) >> 24),
                 (int) ((addr & 0x00FF0000) >> 16),
                 (int) ((addr & 0x0000FF00) >> 8),
                 (int) ((addr & 0x000000FF)),
                 peer.sin_port,
                 peer.sin_port == 1194 ? "(OpenVPN?)" : ""
                );
  }

  init_descriptor(newd, desc);

  /* prepend to list */
  descriptor_list = newd;

  // Ensure all the canaries are functional before negotiating the protocol.
  assert(newd->small_outbuf_canary == 31337);
  assert(newd->output_canary == 31337);
  assert(newd->input_and_character_canary == 31337);
  assert(newd->inbuf_canary == 31337);

  // Once the descriptor has been fully created and added to any lists, it's time to negotiate:
  ProtocolNegotiate(newd);

  int size = strlen(GREETINGS);
  SEND_TO_Q(ProtocolOutput(newd, GREETINGS, &size, DO_APPEND_GA), newd);
  return 0;
}

int process_output(struct descriptor_data *t) {
  static char i[LARGE_BUFSIZE + GARBAGE_SPACE];
  static int result;

  /* If the descriptor is NULL, just return */
  if ( !t )
    return 0;

  memset(i, 0, sizeof(i));

  // Write out a newline, the contents of the output buffer, and the overflow warning if needed.
  // Add an extra CRLF if the person isn't in compact mode.
  snprintf(i, sizeof(i), "\r\n%s%s%s",
           t->output,
           t->bufptr < 0 ? "**OVERFLOW**" : "",            // <- overflow mode check: bufptr < 0
           !t->connected && t->character ? "\r\n" : "");   // <- 'compact mode' check

  /*
   * now, send the output.  If this is an 'interruption', use the prepended
   * CRLF, otherwise send the straight output sans CRLF.
   */
   // Result is saved and handed up.
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
  }
  /* reset total bufspace back to that of a small buffer */
  t->bufspace = SMALL_BUFSIZE - 1;
  t->bufptr = 0;
  t->output = t->small_outbuf;
  *(t->output) = '\0';

  return result;
}

int write_to_descriptor(int desc, const char *txt) {
  int total, bytes_written;

  assert(txt != NULL);

  total = strlen(txt);

  #ifdef PROTO_DEBUG
  log_vfprintf("Before conversion: '''%s'''", txt);
  // We're gonna do some real weird shit and deliberately convert all non-printing chars to printing ones.
  char the_bullshit_buf[total * 10];
  for (int txt_idx = 0; txt[txt_idx]; txt_idx++) {
    if ((txt[txt_idx] < ' ' || txt[txt_idx] > '~')) {
      if (txt[txt_idx] == 27) {
        strlcat(the_bullshit_buf, "@ESC", sizeof(the_bullshit_buf));
      } else {
        snprintf(ENDOF(the_bullshit_buf), sizeof(the_bullshit_buf), "@%d", (unsigned char) txt[txt_idx]);
      }
    } else {
      snprintf(ENDOF(the_bullshit_buf), sizeof(the_bullshit_buf), "%c", txt[txt_idx]);
    }
  }
  log_vfprintf("After conversion: '''%s'''", the_bullshit_buf);
  #endif

  do {
#ifdef __APPLE__
    if ((bytes_written = write(desc, txt, total)) < 0) {
#else
    if ((bytes_written = send(desc, txt, total, MSG_NOSIGNAL)) < 0) {
#endif
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
    ProtocolInput(t, temporary_buffer, bytes_read, t->inbuf, MAX_RAW_INPUT_LENGTH);
    assert(t->inbuf_canary == 31337);
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
    assert(t->inbuf_canary == 31337);

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

      snprintf(buffer, sizeof(buffer), "Line too long.  Truncated to:\r\n%s\r\n", tmp);

      // The calling function can handle this.
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
      strlcpy(tmp, t->last_input, sizeof(tmp));
    else
      strlcpy(t->last_input, tmp, MAX_INPUT_LENGTH);
    assert(t->last_input_canary == 31337);

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

  assert(t->inbuf_canary == 31337);

  return 1;
}

/*
 * perform substitution for the '^..^' csh-esque syntax
 * orig is the orig string (i.e. the one being modified.
 * subst contains the substition string, i.e. "^telm^tell"
 */
int perform_subst(struct descriptor_data *t, char *orig, char *subst, size_t subst_size)
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
  strlcpy(New, orig, (strpos - orig));
  New[(strpos - orig)] = '\0';

  /* now, the replacement string */
  strlcat(New, second, MAX_INPUT_LENGTH);

  /* now, if there's anything left in the original after the string to
   * replaced, copy that too. */
  if (((strpos - orig) + strlen(first)) < strlen(orig))
    strlcat(New, strpos + strlen(first), MAX_INPUT_LENGTH);

  /* terminate the string in case of an overflow from strncat  -- useless code now -LS */
  New[MAX_INPUT_LENGTH-1] = '\0';
  strlcpy(subst, New, subst_size);

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
  char buf[MAX_STRING_LENGTH];

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
    // Log our metrics.
    {
      time_t time_delta = time(0) - d->login_time;
      bool printed = FALSE;
      float per_hour_multiplier = 3600.0 / time_delta;

      strlcpy(buf, "Over ", sizeof(buf));

      if (time_delta >= SECS_PER_REAL_DAY) {
        int days = time_delta / SECS_PER_REAL_DAY;
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%d day%s", days, days != 1 ? "s" : "");
        time_delta %= SECS_PER_REAL_DAY;
        printed = TRUE;
      }

      if (time_delta >= SECS_PER_REAL_HOUR) {
        int hours = time_delta / SECS_PER_REAL_HOUR;
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%d hour%s", printed ? ", " : "", hours, hours != 1 ? "s" : "");
        time_delta %= SECS_PER_REAL_HOUR;
        printed = TRUE;
      }

      if (time_delta >= SECS_PER_REAL_MIN) {
        int minutes = time_delta / SECS_PER_REAL_MIN;
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%d minute%s", printed ? ", " : "", minutes, minutes != 1 ? "s" : "");
        time_delta %= SECS_PER_REAL_MIN;
        printed = TRUE;
      }

      if (time_delta > 0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%ld second%s", printed ? ", " : "", time_delta, time_delta != 1 ? "s" : "");
      }

      long nuyen_earned = 0, nuyen_spent = 0;
      strlcpy(buf2, " FAUCETS:", sizeof(buf2));
      strlcpy(buf3, " SINKS:", sizeof(buf3));
      for (int i = 0; i < NUM_OF_TRACKED_NUYEN_INCOME_SOURCES; i++) {
        if (d->nuyen_income_this_play_session[i] == 0)
          continue;

        if (nuyen_faucets_and_sinks[i].type == NI_IS_FAUCET) {
          // log_vfprintf("'%s' is a faucet, so adding %ld to earned.", nuyen_faucets_and_sinks[i].name, d->nuyen_income_this_play_session[i]);
          nuyen_earned += d->nuyen_income_this_play_session[i];
          snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "\r\n - %20s: %.2f / hr",
                   nuyen_faucets_and_sinks[i].name,
                   d->nuyen_income_this_play_session[i] * per_hour_multiplier
                  );
        } else {
          // log_vfprintf("'%s' is a sink, so adding %ld to spent.", nuyen_faucets_and_sinks[i].name, d->nuyen_income_this_play_session[i]);
          nuyen_spent += d->nuyen_income_this_play_session[i];
          snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "\r\n - %20s: -%.2f / hr",
                   nuyen_faucets_and_sinks[i].name,
                   d->nuyen_income_this_play_session[i] * per_hour_multiplier
                  );
        }
      }
      long nuyen_net = nuyen_earned - nuyen_spent;
      long nuyen_per_hour = nuyen_net * per_hour_multiplier;

      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " of playtime, %s earned %ld and spent %ld for a net total of %ld nuyen. That's approximately %ld per hour.\r\nBreakdown:\r\n\r\n%s\r\n\r\n%s",
               GET_CHAR_NAME(d->character),
               nuyen_earned,
               nuyen_spent,
               nuyen_net,
               nuyen_per_hour,
               buf2,  // faucets
               buf3   // sinks
             );

      mudlog(buf, d->character, LOG_GRIDLOG, TRUE);
    }

    /* added to Free up temporary editing constructs */
    if (d->connected == CON_PLAYING
        || d->connected == CON_PART_CREATE
        || (d->connected >= CON_SPELL_CREATE && d->connected <= CON_HELPEDIT && d->connected != CON_ASKNAME))
    {
      if (d->connected == CON_VEHCUST)
        d->edit_veh = NULL;
      if (d->connected == CON_POCKETSEC) {
        d->edit_obj = NULL;
        d->edit_mob = NULL;
      }
      playerDB.SaveChar(d->character);
      act("^L[OOC]: $n has lost $s link.^n", TRUE, d->character, 0, 0, TO_ROOM);
      snprintf(buf, sizeof(buf), "Closing link to: %s. (%s)", GET_CHAR_NAME(d->character), connected_types[d->connected]);
      mudlog(buf, d->character, LOG_CONNLOG, TRUE);
      if (d->character->persona) {
        extract_icon(d->character->persona);
        d->character->persona = NULL;
        PLR_FLAGS(d->character).RemoveBit(PLR_MATRIX);
      }
      free_editing_structs(d, STATE(d));
      d->character->desc = NULL;
    } else {
      snprintf(buf, sizeof(buf), "Cleaning up data structures from %s's disconnection (state %d).",
              GET_CHAR_NAME(d->character) ? GET_CHAR_NAME(d->character) : "<null>",
              d->connected);
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
  global_descriptor_list_invalidated = TRUE;

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

    if (++d->idle_ticks >= 12 ) {
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
    exit(ERROR_FNCTL_ENCOUNTERED_ERROR);
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
  restrict_mud = 0;
}

void free_up_memory(int Empty)
{
  std::cerr << "SYSERR: Out of memory, saving houses and shutting down...\n";
  // Blow away characters to try and free up enough to let us write houses.
  for (struct char_data *next_ch, *ch = character_list; ch; ch = next_ch) {
    next_ch = ch->next;
    delete ch;
  }
  // Write houses.
  House_save_all();
  // Die.
  exit(ERROR_UNABLE_TO_FREE_MEMORY_IN_CLEARSTACKS);
}

void hupsig(int Empty)
{
  mudlog("Received SIGHUP.  Shutting down...", NULL, LOG_SYSLOG, TRUE);
  House_save_all();
  exit(EXIT_CODE_ZERO_ALL_IS_WELL);
}

void intsig(int Empty)
{
  mudlog("Received SIGINT.  Shutting down...", NULL, LOG_SYSLOG, TRUE);
  House_save_all();
  exit(EXIT_CODE_ZERO_ALL_IS_WELL);
}

void termsig(int Empty)
{
  mudlog("Received SIGTERM.  Shutting down...", NULL, LOG_SYSLOG, TRUE);
  House_save_all();
  exit(EXIT_CODE_ZERO_ALL_IS_WELL);
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

 // This code is ANCIENT and does not work on Ubuntu. -- LS.


#if (defined(WIN32) && !defined(__CYGWIN__))
#define my_signal(signo, func) signal(signo, func)
#else
sigfunc *my_signal(int signo, sigfunc * func)
{
  struct sigaction act, oact;

  act.sa_handler = func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;

  if (sigaction(signo, &act, &oact) < 0)
    return SIG_ERR;

  return oact.sa_handler;
}
#endif

void signal_setup(void)
{
  my_signal(SIGINT, hupsig);
  my_signal(SIGTERM, hupsig);

  signal(SIGPIPE, SIG_IGN); // If this is causing you compilation errors, comment it out and uncomment the below version.
#if !defined(WIN32) || defined(__CYGWIN__)

  // my_signal(SIGPIPE, SIG_IGN);
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
  strlcpy(buffer, str, sizeof(buffer));
  return &buffer[0];
}

void send_to_char(struct char_data * ch, const char * const messg, ...)
{
  if (!ch->desc || !messg)
    return;

  char internal_buffer[MAX_STRING_LENGTH];
  memset(internal_buffer, 0, sizeof(internal_buffer));

  va_list argptr;
  va_start(argptr, messg);
  vsnprintf(internal_buffer, sizeof(internal_buffer), messg, argptr); // Flawfinder: ignore
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
  if (!icon || !icon->decker || !icon->decker->ch || !icon->decker->ch->desc || !messg) {
#ifdef DEBUG_SEND_TO_ICON
    log_vfprintf("send_to_icon '%s' failing: no valid target or messg", messg);
#endif
    return;
  }

  char internal_buffer[MAX_STRING_LENGTH];

  va_list argptr;
  va_start(argptr, messg);
  vsnprintf(internal_buffer, sizeof(internal_buffer), messg, argptr); // Flawfinder: ignore
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

void send_to_outdoor(const char *messg, bool is_weather)
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

      if (is_weather &&
          (PRF_FLAGGED(i->character, PRF_NO_WEATHER) ||
           (i->original && PRF_FLAGGED(i->original, PRF_NO_WEATHER))))
      {
        continue;
      }

      SEND_TO_Q(messg, i);
    }
}

void send_to_driver(char *messg, struct veh_data *veh)
{
  struct char_data *i;

  if (messg && veh)
    for (i = veh->people; i; i = i->next_in_veh)
      if (AFF_FLAGGED(i, AFF_PILOT) && i->desc)
        SEND_TO_Q(messg, i->desc);
}

void send_to_host(vnum_t room, const char *messg, struct matrix_icon *icon, bool needsee) {
  // Bail out on no or empty message.
  if (!messg || !*messg)
    return;

  for (struct matrix_icon *i = matrix[room].icons; i; i = i->next_in_host) {
    // Skip icons that don't have connections to send to.
    if (!i->decker)
      continue;

    // If we pass visibility checks (or don't need to run them at all), send the message.
    if (!icon || (icon != i && (!needsee || has_spotted(i, icon))))
      send_to_icon(i, messg);
  }
}

void send_to_veh(const char *messg, struct veh_data *veh, struct char_data *ch, bool torig, ...)
{
  struct char_data *i;

  if (!veh) {
    char errbuf[MAX_STRING_LENGTH];
    snprintf(errbuf, sizeof(errbuf), "SYSERR: Received null vehicle in send_to_veh type one! (Message: %s)", messg);
    mudlog(errbuf, NULL, LOG_SYSLOG, TRUE);
    return;
  }

  if (messg)
  {
    for (i = veh->people; i; i = i->next_in_veh) {
      if (i != ch && i->desc) {
        SEND_TO_Q(messg, i->desc);
      }
    }
    if (torig && veh->rigger && veh->rigger->desc)
      SEND_TO_Q(messg, veh->rigger->desc);
  }
}

void send_to_veh(const char *messg, struct veh_data *veh, struct char_data *ch, struct char_data *cha, bool torig)
{
  struct char_data *i;

  if (!veh) {
    char errbuf[MAX_STRING_LENGTH];
    snprintf(errbuf, sizeof(errbuf), "SYSERR: Received null vehicle in send_to_veh type two! (Message: %s)", messg);
    mudlog(errbuf, NULL, LOG_SYSLOG, TRUE);
    return;
  }

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

void send_to_room(const char *messg, struct room_data *room, struct veh_data *exclude_veh)
{
  struct char_data *i;
  struct veh_data *v;

  if (messg && room) {
    for (i = room->people; i; i = i->next_in_room) {
      if (i->desc)
        if (!(PLR_FLAGGED(i, PLR_REMOTE) || PLR_FLAGGED(i, PLR_MATRIX)) && AWAKE(i))
          SEND_TO_Q(messg, i->desc);

      if (i == i->next_in_room) {
        mudlog("SYSERR: I = I->next_in_room in send_to_room! Truncating list. We will likely crash soon.", NULL, LOG_SYSLOG, TRUE);
        i->next_in_room = NULL;
      }
    }
    for (v = room->vehicles; v; v = v->next_veh) {
      if (v != exclude_veh)
        send_to_veh(messg, v, NULL, TRUE);

      if (v == v->next_veh) {
        mudlog("SYSERR: V = V->next_veh in send_to_room! Truncating list. We may crash soon.", NULL, LOG_SYSLOG, TRUE);
        v->next_veh = NULL;
      }
    }
  }
}

const char *ACTNULL = "<NULL>";

#define CHECK_NULL(pointer, expression)  (((pointer) == NULL) ? ACTNULL : (expression));

// Uses NEW-- make sure you delete!
char* strip_ending_punctuation_new(const char* orig) {
  int len = strlen(orig);
  char* stripped = new char[len + 1];

  strlcpy(stripped, orig, len + 1);

  char* c = stripped + len - 1;

  if (*c == '.' || *c == '!' || *c == '?')
    *c = '\0';

  return stripped;
}

const char *get_voice_perceived_by(struct char_data *speaker, struct char_data *listener, bool invis_staff_should_be_identified) {
  static char voice_buf[500];
  struct remem *mem = NULL;

  if (IS_NPC(speaker))
    return GET_NAME(speaker);
  else {
    bool voice_masked = is_voice_masked(speaker);

    // $z mode: Identify staff members.
    if (invis_staff_should_be_identified && IS_SENATOR(speaker) && !CAN_SEE(listener, speaker))
      strlcpy(voice_buf, "an invisible staff member", sizeof(voice_buf));

    // Otherwise, compose a voice as usual-- start off with their normal voice, then replace with mask if modulated.
    else {
      if (voice_masked) {
        strlcpy(voice_buf, "a masked voice", sizeof(voice_buf));
      } else {
        strlcpy(voice_buf, speaker->player.physical_text.room_desc, sizeof(voice_buf));
      }
    }

    // No radio names mode means just give the voice desc. Otherwise, staff see speaker's name.
    if (IS_SENATOR(listener) && !PRF_FLAGGED(listener, PRF_NO_RADIO_NAMES))
      snprintf(ENDOF(voice_buf), sizeof(voice_buf) - strlen(voice_buf), "(%s)", GET_CHAR_NAME(speaker));

    // Non-staff, but remembered the speaker? You see their remembered name.
    else if (!voice_masked && (mem = safe_found_mem(listener, speaker)))
      snprintf(ENDOF(voice_buf), sizeof(voice_buf) - strlen(voice_buf), "(%s)", CAP(mem->mem));

    // Return our string. If no checks were passed, it just gives their voice desc with no special frills.
    return voice_buf;
  }
}

/* higher-level communication: the act() function */
// now returns the composed line, in case you need to capture it for some reason
const char *perform_act(const char *orig, struct char_data * ch, struct obj_data * obj,
                 void *vict_obj, struct char_data * to)
{
  extern char *make_desc(char_data *ch, char_data *i, char *buf, int act, bool dont_capitalize_a_an, size_t buf_size);
  const char *i = NULL;
  char *buf;
  struct char_data *vict;
  static char lbuf[MAX_STRING_LENGTH];
  char temp_buf[MAX_STRING_LENGTH];
  buf = lbuf;
  vict = (struct char_data *) vict_obj;

  // No need to go through all this junk if the to-char has no descriptor.
  if (!to || !to->desc)
    return NULL;

  // We can also skip if the to-char is fully ignoring the actor.
  if (ch && IS_IGNORING(to, is_blocking_ic_interaction_from, ch)) {
    return NULL;
  }

  // Prep for error case catching.
  strlcpy(lbuf, "An erroneous voice (alert staff!)", sizeof(lbuf));
  i = lbuf;

  for (;;)
  {
    if (*orig == '$') {
      switch (*(++orig)) {
        case 'a':
          i = CHECK_NULL(obj, SANA(obj));
          break;
        case 'A':
          i = CHECK_NULL(vict_obj, SANA((struct obj_data *) vict_obj));
          break;
        case 'e':
          if (to == ch)
            i = "you";
          else if (CAN_SEE(to, ch))
            i = HSSH(ch);
          else
            i = "it";
          break;
        case 'E':
          if (vict_obj) {
            if (to == vict)
              i = "you";
            else if (CAN_SEE(to, vict))
              i = HSSH(vict);
            else
              i = "it";
          }
          break;
        case 'F':
          i = CHECK_NULL(vict_obj, fname((char *) vict_obj));
          break;
        case 'm':
          if (to == ch)
            i = "you";
          else if (CAN_SEE(to, ch))
            i = HMHR(ch);
          else
            i = "them";
          break;
        case 'M':
          if (!vict)
            i = "someone";
          else if (to == vict)
            i = "you";
          else if (CAN_SEE(to, vict))
            i = HMHR(vict);
          else
            i = "them";
          break;
        case 'n':
          if (to == ch)
            i = "you";
          else if (!IS_NPC(ch) && (IS_SENATOR(to) || IS_SENATOR(ch)))
            i = GET_CHAR_NAME(ch);
          else if (CAN_SEE(to, ch)) {
            if (AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE)) {
              struct veh_data *veh;
              RIG_VEH(ch, veh);
              i = GET_VEH_NAME(veh);
            } else {
              i = make_desc(to, ch, temp_buf, TRUE, TRUE, sizeof(temp_buf));
            }
          }
          else
            i = "someone";
          break;
        case 'N':
          if (!vict)
            i = "someone";
          else if (to == vict)
            i = "you";
          else if (!IS_NPC(vict) && (IS_SENATOR(to) || IS_SENATOR(vict)))
            i = GET_CHAR_NAME(vict);
          else if (CAN_SEE(to, vict)) {
            if (AFF_FLAGGED(vict, AFF_RIG) || PLR_FLAGGED(vict, PLR_REMOTE)) {
              struct veh_data *veh;
              RIG_VEH(vict, veh);
              i = GET_VEH_NAME(veh);
            } else {
              i = make_desc(to, vict, temp_buf, TRUE, TRUE, sizeof(temp_buf));
            }
          }
          else
            i = "someone";
          break;
        case 'o':
          i = CHECK_NULL(obj, OBJN(obj, to));
          break;
        case 'O':
          i = CHECK_NULL(vict_obj, OBJN((struct obj_data *) vict_obj, to));
          break;
        case 'p':
          i = CHECK_NULL(obj, OBJS(obj, to));
          break;
        case 'P':
          i = CHECK_NULL(vict_obj, OBJS((struct obj_data *) vict_obj, to));
          break;
        case 's':
          if (to == ch)
            i = "your";
          else if (CAN_SEE(to, ch))
            i = HSHR(ch);
          else
            i = "their";
          break;
        case 'S':
          if (!vict_obj)
            i = "someone's";
          else if (to == vict)
            i = "your";
          else if (CAN_SEE(to, vict))
            i = HSHR(vict);
          else
            i = "their";
          break;
        case 'T':
          i = CHECK_NULL(vict_obj, (char *) vict_obj);
          break;
        case 'v': /* Just voice */
          i = get_voice_perceived_by(ch, to, FALSE);
          break;
        case 'z': /* Desc if visible, voice if not */
          // You always know if it's you.
          if (to == ch) {
            i = "you";
          }

          // Staff ignore visibility checks.
          else if (IS_SENATOR(to)) {
            if (!IS_NPC(ch)) {
              i = GET_CHAR_NAME(ch);
            } else {
              i = make_desc(to, ch, temp_buf, TRUE, TRUE, sizeof(temp_buf));
            }
          }

          // If they're visible, it's simple.
          else if (CAN_SEE(to, ch))
            i = make_desc(to, ch, temp_buf, TRUE, TRUE, sizeof(temp_buf));

          // If we've gotten here, the speaker is an invisible player or staff member.
          else {
            // Since $z is only used for speech, and since NPCs can't be remembered, we display their name when NPCs speak.
            if (IS_NPC(ch))
              i = GET_NAME(ch);
            else {
              i = get_voice_perceived_by(ch, to, TRUE);
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

bool can_send_act_to_target(struct char_data *ch, bool hide_invisible, struct obj_data * obj, void *vict_obj, struct char_data *to, int type) {
#define SENDOK(ch) ((ch)->desc && AWAKE(ch) && !(PLR_FLAGGED((ch), PLR_WRITING) || PLR_FLAGGED((ch), PLR_EDITING) || PLR_FLAGGED((ch), PLR_MAILING) || PLR_FLAGGED((ch), PLR_CUSTOMIZE)) && (STATE(ch->desc) != CON_SPELL_CREATE))
  bool remote;

  if ((remote = (type & TO_REMOTE)))
    type &= ~TO_REMOTE;

  // Nobody unique to send to? Fail.
  if (!to || !SENDOK(to) || to == ch)
    return FALSE;

  // Can't see them and it's an action-based message? Fail.
  if (hide_invisible && ch && !CAN_SEE(to, ch))
    return FALSE;

  // Only some things go through to remote users.
  if (!remote && PLR_FLAGGED(to, PLR_REMOTE))
    return FALSE;

  // Matrix users skip almost everything-- staff sees rolls there though.
  if (PLR_FLAGGED(to, PLR_MATRIX) && !(IS_SENATOR(to) && type == TO_ROLLS))
    return FALSE;

  return type == TO_ROOM || type == TO_ROLLS || (to != vict_obj);

#undef SENDOK
}

/* Condition checking for perform_act(). Returns the string generated by p_a() provided that
    the type is TO_CHAR, TO_VICT, or TO_DECK. If no string is generated or type is other, returns NULL. */
const char *act(const char *str, int hide_invisible, struct char_data * ch,
         struct obj_data * obj, void *vict_obj, int type)
{
  struct char_data *to, *next, *tch;
  int sleep, staff_only;
  bool remote;
  struct veh_data *rigger_check;

#define SENDOK(ch) ((ch)->desc && (AWAKE(ch) || sleep) && (!staff_only || IS_SENATOR(ch))  && !(PLR_FLAGGED((ch), PLR_WRITING) || PLR_FLAGGED((ch), PLR_EDITING) || PLR_FLAGGED((ch), PLR_MAILING) || PLR_FLAGGED((ch), PLR_CUSTOMIZE)) && (STATE(ch->desc) != CON_SPELL_CREATE))

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

  if ((remote = (type & TO_REMOTE)))
    type &= ~TO_REMOTE;

  if ((staff_only = (type & TO_STAFF_ONLY)))
    type &= ~TO_STAFF_ONLY;

  if ( type == TO_ROLLS )
    sleep = 1;

  if (type == TO_CHAR)
  {
    if (ch && SENDOK(ch) && (remote || !PLR_FLAGGED(ch, PLR_REMOTE)) && !PLR_FLAGGED(ch, PLR_MATRIX))
      return perform_act(str, ch, obj, vict_obj, ch);
    return NULL;
  }
  if (type == TO_VICT)
  {
    to = (struct char_data *) vict_obj;

    if (!to || !SENDOK(to))
      return NULL;

    if (((!remote && PLR_FLAGGED(to, PLR_REMOTE)) || PLR_FLAGGED(to, PLR_MATRIX)) && !sleep)
      return NULL;

    if (hide_invisible && ch && !CAN_SEE(to, ch))
      return NULL;

    return perform_act(str, ch, obj, vict_obj, to);
  }
  if (type == TO_DECK)
  {
    if ((to = (struct char_data *) vict_obj) && SENDOK(to) && PLR_FLAGGED(to, PLR_MATRIX))
      return perform_act(str, ch, obj, vict_obj, to);
    return NULL;
  }
  /* ASSUMPTION: at this point we know type must be TO_NOTVICT
   or TO_ROOM or TO_ROLLS */

  if (type == TO_VEH_ROOM) {
    if (ch && ch->in_veh && !ch->in_veh->in_veh) {
      to = ch->in_veh->in_room->people;
      rigger_check = ch->in_veh->in_room->vehicles;
    } else
      return NULL;
  } else if (ch && ch->in_room) {
    to = ch->in_room->people;
    rigger_check = ch->in_room->vehicles;
  } else if (obj && obj->in_room) {
    to = obj->in_room->people;
    rigger_check = obj->in_room->vehicles;
  } else if (obj && obj->in_veh) {
    to = obj->in_veh->people;
    rigger_check = obj->in_veh->carriedvehs;
  } else if (ch && ch->in_veh) {
    to = ch->in_veh->people;
    rigger_check = ch->in_veh->carriedvehs;
  } else
  {
    mudlog("SYSERR: no valid target to act()!", NULL, LOG_SYSLOG, TRUE);
    snprintf(buf, sizeof(buf), "Invocation: act('%s', '%d', char_data, obj_data, vict_obj, '%d').", str, hide_invisible, type);
    if (ch) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nch: %s, in_room %s, in_veh %s",
              GET_CHAR_NAME(ch), ch->in_room ? GET_ROOM_NAME(ch->in_room) : "n/a",
              ch->in_veh ? GET_VEH_NAME(ch->in_veh) : "n/a");
    } else {
      strlcat(buf, " ...No character.", sizeof(buf));
    }
    if (obj) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nobj: %s, in_room %s, in_veh %s",
              GET_OBJ_NAME(obj), obj->in_room ? GET_ROOM_NAME(obj->in_room) : "n/a",
              obj->in_veh ? GET_VEH_NAME(obj->in_veh) : "n/a");
    } else {
      strlcat(buf, " ...No obj.", sizeof(buf));
    }
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }

  // Astral beings can't leave physical traces, so all invocations with astrals must be hidden from invisible chars.
  if (ch && IS_ASTRAL(ch))
    hide_invisible = TRUE;

  if ( type == TO_ROLLS )
  {
    for (; to; to = to->next_in_room) {
      if (IS_NPC(to) || !PRF_FLAGGED(to, PRF_ROLLS))
        continue;

      if (!IS_SENATOR(to)) {
        if (!_OVERRIDE_ALLOW_PLAYERS_TO_USE_ROLLS_ && !PLR_FLAGGED(to, PLR_PAID_FOR_ROLLS))
          continue;

        if (staff_only)
          continue;
      }

      if (SENDOK(to)
          && !(hide_invisible
               && ch
               && !CAN_SEE(to, ch)))
        perform_act(str, ch, obj, vict_obj, to);
    }

    // Send rolls to riggers.
    for (; rigger_check; rigger_check = rigger_check->next_veh) {
      if ((tch = rigger_check->rigger) && tch->desc && PRF_FLAGGED(tch, PRF_ROLLS)) {
        // We currently treat all vehicles as having ultrasonic sensors.
        // Since the check is done to the rigger, we have to apply det-invis to them directly, then remove it when done.
        set_vision_bit(tch, VISION_ULTRASONIC, VISION_BIT_OVERRIDE);
        if (SENDOK(tch)
            && !(hide_invisible
                 && ch
                 && !CAN_SEE(tch, ch)))
          perform_act(str, ch, obj, vict_obj, tch);
        remove_vision_bit(tch, VISION_ULTRASONIC, VISION_BIT_OVERRIDE);
      }
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
      snprintf(buf, sizeof(buf), "SYSERR: Encountered to=next infinite loop while looping over act string ''%s' for %s. Setting their next to NULL, this may break someone. Debug info: %s, type %d",
              str,
              GET_CHAR_NAME(ch),
              to->in_veh ? "in veh" : "in room",
              type);
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      if (to->in_veh)
        to->next_in_veh = NULL;
      else
        to->next_in_room = NULL;

      return NULL;
    }
    if (can_send_act_to_target(ch, hide_invisible, obj, vict_obj, to, type))
      perform_act(str, ch, obj, vict_obj, to);
  }

  // Send to riggers.
  for (; rigger_check; rigger_check = rigger_check->next_veh) {
    if ((tch = rigger_check->rigger) && tch->desc) {
      // We currently treat all vehicles as having ultrasonic sensors.
      // Since the check is done to the rigger, we have to apply det-invis to them directly, then remove it when done.
      set_vision_bit(tch, VISION_ULTRASONIC, VISION_BIT_OVERRIDE);
      if (can_send_act_to_target(ch, hide_invisible, obj, vict_obj, tch, type | TO_REMOTE))
        perform_act(str, ch, obj, vict_obj, tch);
      remove_vision_bit(tch, VISION_ULTRASONIC, VISION_BIT_OVERRIDE);
    }
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

bool ch_is_eligible_to_receive_socialization_bonus(struct char_data *ch) {
  // NPCs don't get social bonuses.
  if (IS_NPC(ch))
    return FALSE;

  // Not in a social room? Can't receive the bonus.
  if (!char_is_in_social_room(ch))
    return FALSE;

  // You're invisible? You can't get the bonus. Too easy to just idle in a bar watching Netflix for karma.
  // And to anyone reading the code and thinking "Oh, it'd be a great prank to make my rival invis while they RP so they don't get the bonus!" -- no, that's abusing the system and will be punished.
  if (IS_AFFECTED(ch, AFF_IMP_INVIS)
       || IS_AFFECTED(ch, AFF_SPELLIMPINVIS)
       || IS_AFFECTED(ch, AFF_RUTHENIUM)
       || IS_AFFECTED(ch, AFF_SPELLINVIS))
    return FALSE;

  // You must be present.
  if (!ch->desc || ch->char_specials.timer >= 10)
    return FALSE;

  return TRUE;
}

void increase_congregation_bonus_pools() {
  // Apply congregation / socializing bonuses. We specifically don't want to tell them when it's full,
  // because that encourages breaking off from RP to go burn it down again. Let them chill.
  for (struct char_data *i = character_list; i; i = i->next) {
    if (!ch_is_eligible_to_receive_socialization_bonus(i))
      continue;

    // You can't be maxed out.
    if (GET_CONGREGATION_BONUS(i) == MAX_CONGREGATION_BONUS) {
      /*
      snprintf(buf, sizeof(buf), "Socialization: Skipping %s's opportunity to earn social points-- already maximized.", GET_CHAR_NAME(i));
      act(buf, FALSE, i, 0, 0, TO_ROLLS);
      */
      continue;
    }

    int occupants = 1;
    // int total_occupants = 1;

    // Two iterations over the room. First, if you're not an NPC and not idle, you proceed to second check.
    for (struct char_data *tempch = i->in_room->people; tempch; tempch = tempch->next_in_room) {
      if (tempch == i || !ch_is_eligible_to_receive_socialization_bonus(tempch))
        continue;

      // total_occupants++;

      // Second, if your host does not match anyone else's host, you count as an occupant.
      // Disabled this code for now since grapevine is a thing. It's still against the rules, we just don't check it here.
      /*
      bool is_multichar = FALSE;
      for (struct char_data *descch = i->in_room->people; descch; descch = descch->next_in_room) {
        if (descch == tempch || IS_NPC(descch) || !descch->desc || descch->char_specials.timer >= 10)
          continue;

        if (!strcmp(descch->desc->host, tempch->desc->host)) {
          snprintf(buf, sizeof(buf), "Socialization: Skipping %s in occupant count-- multichar of %s.", GET_CHAR_NAME(descch), GET_CHAR_NAME(tempch));
          act(buf, FALSE, i, 0, 0, TO_ROLLS);
          is_multichar = TRUE;
          break;
        }
      }
      if (is_multichar)
        continue;
      */

      /*
      snprintf(buf, sizeof(buf), "Socialization: Counting %s as an occupant.", GET_CHAR_NAME(tempch));
      act(buf, FALSE, i, 0, 0, TO_ROLLS);
      */
      occupants++;
    }

    // Skip people who haven't emoted in a while, but only if they're not alone there.
    if (occupants > 1 && i->char_specials.last_social_action > LAST_SOCIAL_ACTION_REQUIREMENT_FOR_CONGREGATION_BONUS) {
      /*
      snprintf(buf, sizeof(buf), "Socialization: Skipping %s's opportunity to earn social points-- they've not emoted to their partner.", GET_CHAR_NAME(i));
      act(buf, FALSE, i, 0, 0, TO_ROLLS);
      */
      continue;
    }

    int point_gain = 1;
    if (occupants >= 10)
      point_gain = 4;
    else if (occupants >= 6)
      point_gain = 3;
    else if (occupants >= 3)
      point_gain = 2;

    /*
    snprintf(buf, sizeof(buf), "Socialization: %s gets %d (valid PC occupants: %d of %d).",
             GET_CHAR_NAME(i),
             MIN(GET_CONGREGATION_BONUS(i) + point_gain, MAX_CONGREGATION_BONUS) - GET_CONGREGATION_BONUS(i),
             occupants,
             total_occupants);
    act(buf, FALSE, i, 0, 0, TO_ROLLS);
    */
    GET_CONGREGATION_BONUS(i) = MIN(GET_CONGREGATION_BONUS(i) + point_gain, MAX_CONGREGATION_BONUS);
  }
}

void send_nuyen_rewards_to_pcs() {
  struct char_data *ch;
  bool already_printed = FALSE;
  snprintf(buf, sizeof(buf), "Standard idle bonus of %d nuyen awarded to: ", IDLE_NUYEN_REWARD_AMOUNT);

  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    if (d->connected == CON_PLAYING
        || d->connected == CON_PART_CREATE
        || (d->connected >= CON_SPELL_CREATE
            && d->connected <= CON_HELPEDIT
            && d->connected != CON_ASKNAME))
    {
      ch = (d->original ? d->original : d->character);
      // Staff and chargen characters don't get the idle bonus.
      if (IS_SENATOR(ch) || PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED))
        continue;

      if (ch->char_specials.timer > IDLE_NUYEN_REWARD_THRESHOLD_IN_MINUTES) {
        if (!PRF_FLAGGED(ch, PRF_NO_IDLE_NUYEN_REWARD_MESSAGE))
          send_to_char(ch, "[OOC message: You have been awarded the standard idling bonus of %d nuyen. You can TOGGLE NOIDLE to hide these messages while still getting the reward.]\r\n", IDLE_NUYEN_REWARD_AMOUNT);
        gain_nuyen(ch, IDLE_NUYEN_REWARD_AMOUNT, NUYEN_INCOME_IDLE_REWARD);

        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%s", already_printed ? ", " : "", GET_CHAR_NAME(ch));
        already_printed = TRUE;
      }
    }
  }

  if (already_printed)
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
}

void process_wheres_my_car() {
  PERF_PROF_SCOPE( pr_wheres_my_car_, "where's my car");
  struct room_data *ch_in_room = NULL, *veh_in_room = NULL;

  // Iterate through all connected PCs in the game, looking for ones who are searching for cars.
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    // Skip anyone not in the playing state.
    if (d->connected)
      continue;

    // Skip anyone who's projecting or doesn't have a character.
    if (!d->character || d->original)
      continue;

    // Skip anyone who hasn't paid for a search.
    if (GET_NUYEN_PAID_FOR_WHERES_MY_CAR(d->character) <= 0)
      continue;

    // Skip anyone not in a room.
    if (!(ch_in_room = get_ch_in_room(d->character)))
      continue;

    int num_vehicles_found = 0, total_vehicles = 0;
    int reward_amount = GET_NUYEN_PAID_FOR_WHERES_MY_CAR(d->character) - (GET_NUYEN_PAID_FOR_WHERES_MY_CAR(d->character) / 2);
    int max_distance_searched = GET_NUYEN_PAID_FOR_WHERES_MY_CAR(d->character) / WHERES_MY_CAR_NUYEN_PER_ROOM;
    int idnum_canary = GET_IDNUM(d->character);

    // Compose the beginning of our log string.
    snprintf(buf, sizeof(buf), "%s paid %ld",
             GET_CHAR_NAME(d->character),
             GET_NUYEN_PAID_FOR_WHERES_MY_CAR(d->character));

    // Clear paid amount.
    GET_NUYEN_PAID_FOR_WHERES_MY_CAR(d->character) = 0;
    assert(idnum_canary == GET_IDNUM(d->character));

    send_to_char("After a bit of waiting, your searchers straggle back in. They"
                 " confer amongst themselves, then give you a report:\r\n", d->character);

    // Iterate through all the vehicles in the game, looking for ones owned by this character.
    for (struct veh_data *veh = veh_list; veh; veh = veh->next) {
      if (veh->owner == GET_IDNUM(d->character) && (veh_in_room = get_veh_in_room(veh))) {
        total_vehicles++;

        // Hard limit of 5 vehicles-- otherwise BFS lags us out.
        if (total_vehicles > 5)
          break;

        // If they didn't pay enough to find this one, skip it.
        int distance = calculate_distance_between_rooms(veh_in_room->number, ch_in_room->number, TRUE);
        if (distance < 0 || distance > max_distance_searched) {
          log_vfprintf("Refusing to find vehicle %s: Distance is %d, vs max %d.", GET_VEH_NAME(veh), distance, max_distance_searched);
          continue;
        }

        struct room_data *room = get_veh_in_room(veh);
        send_to_char(d->character, "%2d) %-30s (At %s^n) [%2d/10] Damage\r\n",
                     ++num_vehicles_found,
                     GET_VEH_NAME(veh),
                     room ? room->name : "someone's tow rig, probably",
                     veh->damage);
      }
    }
    assert(idnum_canary == GET_IDNUM(d->character));

    // Either note to the player that they've paid the full amount, or refund the unpaid half.
    if (num_vehicles_found > 0) {
      send_to_char(d->character, "\r\nYou %s pay out the promised reward of %d.\r\n",
                   num_vehicles_found == total_vehicles ? "nod and" : "begrudgingly",
                   reward_amount);
    } else {
      send_to_char(d->character, "...It's just a blank piece of paper. You stare them down until they leave, then pocket the reward of %d.\r\n",
                   reward_amount);
      // Credit it back.
      GET_NUYEN_RAW(d->character) += reward_amount;
      GET_NUYEN_INCOME_THIS_PLAY_SESSION(d->character, NUYEN_OUTFLOW_WHERESMYCAR) -= reward_amount;

      // Tell them if they don't own a car.
      if (total_vehicles <= 0) {
        send_to_char("[OOC: You don't actually own any vehicles!]\r\n", d->character);
      } else {
        send_to_char(d->character, "[OOC: Your %s too far away or are otherwise inaccessible to you at the moment.]\r\n", total_vehicles == 1 ? "vehicle is": "vehicles are");
      }
    }
    assert(idnum_canary == GET_IDNUM(d->character));

    // Finish up our log string and write it to logs.
    snprintf(buf2, sizeof(buf2), "%s to find %d/%d vehicles (max dist %d).",
             buf,
             num_vehicles_found,
             total_vehicles,
             max_distance_searched);
    mudlog(buf2, d->character, LOG_SYSLOG, TRUE);
  }
}

#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
void verify_vehicle_validity(struct veh_data *veh, bool go_deep=FALSE);
void verify_room_validity(struct room_data *room, bool go_deep=FALSE);
void verify_obj_validity(struct obj_data *obj, bool go_deep=FALSE);
void verify_character_validity(struct char_data *ch, bool go_deep=FALSE);

void verify_vehicle_validity(struct veh_data *veh, bool go_deep) {
  if (veh == NULL)
    return;

  assert(veh->canary == CANARY_VALUE);

  if (!go_deep)
    return;

  verify_room_validity(veh->in_room);
  verify_room_validity(veh->dest);

  for (int i = 0; i < 3; i++)
    verify_room_validity(veh->lastin[i]);

  verify_character_validity(veh->followch);

  for (struct char_data *ch = veh->people; ch; ch = ch->next_in_veh) {
    verify_character_validity(ch);
    assert(ch->in_veh == veh);
  }
  verify_character_validity(veh->rigger);
  verify_character_validity(veh->fighting);

  verify_obj_validity(veh->mount);

  for (struct obj_data *obj = veh->contents; obj; obj = obj->next_content) {
    verify_obj_validity(obj);
    assert(obj->in_veh == veh);
  }

  for (int i = 0; i < NUM_MODS; i++)
    verify_obj_validity(veh->mod[i]);

  verify_vehicle_validity(veh->following);
  verify_vehicle_validity(veh->fight_veh);
  verify_vehicle_validity(veh->next_veh);
  verify_vehicle_validity(veh->next_sub);
  verify_vehicle_validity(veh->prev_sub);
  verify_vehicle_validity(veh->carriedvehs);
  verify_vehicle_validity(veh->in_veh);
  verify_vehicle_validity(veh->towing);
}

void verify_room_validity(struct room_data *room, bool go_deep) {
  if (room == NULL)
    return;

  assert(room->canary == CANARY_VALUE);

  if (!go_deep)
    return;

  for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
    if ((room)->dir_option[dir]) {
      assert((room)->dir_option[dir]->canary == CANARY_VALUE);
    }
    if ((room)->temporary_stored_exit[dir]) {
      assert((room)->temporary_stored_exit[dir]->canary == CANARY_VALUE);
    }
  }

  for (struct obj_data *obj = room->contents; obj; obj = obj->next_content) {
    verify_obj_validity(obj);
    assert(obj->in_room == room);
  }
  for (struct char_data *ch = room->people; ch; ch = ch->next_in_room) {
    verify_character_validity(ch);
    assert(ch->in_room == room);
  }
  for (struct char_data *ch = room->watching; ch; ch = ch->next_watching) {
    verify_character_validity(ch);
  }
  for (struct veh_data *veh = room->vehicles; veh; veh = veh->next_veh) {
    verify_vehicle_validity(veh);
    assert(veh->in_room == room);
  }

  for (int i = 0; i < NUM_WORKSHOP_TYPES; i++)
    verify_obj_validity(room->best_workshop[i]);
}

void verify_obj_validity(struct obj_data *obj, bool go_deep) {
  if (obj == NULL)
    return;

  assert(obj->canary == CANARY_VALUE);

  if (!go_deep)
    return;

  verify_room_validity(obj->in_room);
  verify_vehicle_validity(obj->in_veh);

  verify_character_validity(obj->carried_by);
  verify_character_validity(obj->worn_by);
  verify_character_validity(obj->targ);

  assert(obj->in_obj != obj);
  verify_obj_validity(obj->in_obj);
  verify_obj_validity(obj->contains);      /* Contains objects                 */
  verify_obj_validity(obj->next_content);  /* For 'contains' lists             */

  // verify_host_validity(obj->in_host);

  verify_vehicle_validity(obj->tveh);
}

void verify_character_validity(struct char_data *ch, bool go_deep) {
  if (ch == NULL)
    return;

  assert(ch->canary == CANARY_VALUE);

  if (!go_deep)
    return;

  verify_room_validity(ch->in_room);
  verify_room_validity(ch->was_in_room);

  verify_vehicle_validity(ch->in_veh);

  verify_obj_validity(ch->carrying);
  verify_obj_validity(ch->cyberware);
  verify_obj_validity(ch->bioware);
  for (int i = 0; i < NUM_WEARS; i++)
    verify_obj_validity(ch->equipment[i]);

  verify_character_validity(ch->next_in_room);
  verify_character_validity(ch->next);
  verify_character_validity(ch->next_fighting);
  verify_character_validity(ch->next_in_zone);
  verify_character_validity(ch->next_in_veh);
  verify_character_validity(ch->next_watching);
  verify_character_validity(ch->master);

  /*
  struct char_player_data player;
  struct char_ability_data real_abils;
  struct char_ability_data aff_abils;
  struct char_point_data points;
  struct char_special_data char_specials;
  struct player_special_data *player_specials;
  struct mob_special_data mob_specials;
  struct matrix_icon *persona;
  struct spell_queue *squeue;
  struct sustain_data *sustained;
  struct spirit_sustained *ssust;
  struct descriptor_data *desc;
  struct follow_type *followers;
  struct spell_data *spells;
  IgnoreData *ignore_data;
  Pgroup_data *pgroup;
  Pgroup_invitation *pgroup_invitations;
  */
}

void verify_every_pointer_we_can_think_of() {
  /* Huh, looks like you want to hate your life. Welcome to the shitshow!
     We will now:
       - Iterate over everything I can think of to check for pointer validity
       - Check all the canaries I can think of
       - Probably bog down the machine and cause hella problems
  */
  extern class objList ObjList;

  // First, objects: Are their location pointers valid?
  ObjList.CheckPointers();

  // Next, characters.
  for (struct char_data *ptr = character_list; ptr; ptr = ptr->next) {
    verify_character_validity(ptr, TRUE);
  }

  // Next, vehicles.
  for (struct veh_data *veh = veh_list; veh; veh = veh->next) {
    verify_vehicle_validity(veh, TRUE);
  }

  // Finally, rooms.
  for (int i = 0; i < top_of_world; i++) {
    verify_room_validity(&world[i], TRUE);
  }
}
#endif
