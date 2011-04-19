/* sonar, Copyright (c) 1998-2008 Jamie Zawinski and Stephen Martin
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __SONAR_XSCREENSAVER_H__
#define __SONAR_XSCREENSAVER_H__

typedef struct sonar_sensor_data sonar_sensor_data;
typedef struct sonar_bogie sonar_bogie;

struct sonar_bogie {
  void *closure;
  char *name;		/* bogie name, e.g., host name */
  char *desc;		/* second line of text, e.g., ping time */
  double r;		/* distance, 0 - 1.0 */
  double th;		/* heading, 0 - 2 pi */
  double ttl;		/* time to live, 0 - 2 pi */
  sonar_bogie *next;	/* next one in the list */
};

struct sonar_sensor_data {
  void *closure;

  /* Called frequently (every time the sweep moves).
     Returns a list of new bogies to be added to the display list 
     once the sweep comes around to their position.
   */
  sonar_bogie *(*scan_cb) (sonar_sensor_data *);

  /* Called when a bogie is freed, to free bogie->closure */
  void (*free_bogie_cb) (sonar_sensor_data *, void *closure);

  /* Called at exit, to free ssd->closure */
  void (*free_data_cb) (sonar_sensor_data *, void *closure);
};

/* frees bogie and its contents, including calling the free_bogie_cb. */
extern void free_bogie (sonar_sensor_data *ssd, sonar_bogie *b);

/* makes a copy of the bogie, not including the 'closure' data. */
extern sonar_bogie *copy_bogie (sonar_sensor_data *ssd, const sonar_bogie *b);


/* Set up and return sensor state for ICMP pings. */
extern sonar_sensor_data *init_ping (Display *dpy, 
                                     char **error_ret,
                                     const char *subnets,
                                     int ping_timeout,
                                     Bool resolve_p, Bool times_p,
                                     Bool debug_p);

/* Set up and return sensor state for the simulation. */
extern sonar_sensor_data *init_simulation (Display *dpy,
                                           char **error_ret,
                                           const char *team_a_name, 
                                           const char *team_b_name,
                                           int team_a_count, 
                                           int team_b_count,
                                           Bool debug_p);

#endif /* __SONAR_XSCREENSAVER_H__ */
