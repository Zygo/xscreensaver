/* sonar, Copyright (c) 1998-2008 Jamie Zawinski and Stephen Martin
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * This implements the "simulation" sensor for sonar.
 */

#include "screenhackI.h"
#include "sonar.h"

typedef struct {
  const char *team_a_name;
  const char *team_b_name;
  int team_a_count;
  int team_b_count;
  sonar_bogie *targets;
  Bool debug_p;
} sim_data;


static void
sim_free_data (sonar_sensor_data *ssd, void *closure)
{
  sim_data *sd = (sim_data *) closure;
  free (sd);
}

static void
sim_free_bogie_data (sonar_sensor_data *ssd, void *closure)
{
  free (closure);
}


/* Return an updated (moved) copy of the bogies.
 */
static sonar_bogie *
sim_scan (sonar_sensor_data *ssd)
{
  sim_data *sd = (sim_data *) ssd->closure;
  sonar_bogie *b, *b2, *list = 0;
  double scale = 0.01;
  for (b = sd->targets; b; b = b->next)
    {
      b->r  += scale * (0.5 - frand(1.0));
      b->th += scale * (0.5 - frand(1.0));
      while (b->r < 0.2) b->r += scale * 0.1;
      while (b->r > 0.9) b->r -= scale * 0.1;

      b2 = copy_bogie (ssd, b);
      b2->next = list;
      list = b2;
    }
  return list;
}


static void
make_bogies (sonar_sensor_data *ssd)
{
  sim_data *sd = (sim_data *) ssd->closure;
  int i, j;

  for (j = 0; j <= 1; j++)
    for (i = 0; i < (j ? sd->team_a_count : sd->team_b_count); i++)
      {
        sonar_bogie *b = (sonar_bogie *) calloc (1, sizeof(*b));
        const char *name = (j ? sd->team_a_name : sd->team_b_name);
        b->name = (char *) malloc (strlen(name) + 10);
        sprintf (b->name, "%s%03d", name, i+1);
        b->r = 0.3 + frand(0.5);
        b->th = frand (M_PI*2);
        b->next = sd->targets;
        sd->targets = b;
        if (sd->debug_p)
          fprintf (stderr, "%s: %s: %5.2f %5.2f\n", progname,
                   b->name, b->r, b->th);
      }
}


sonar_sensor_data *
init_simulation (Display *dpy, char **error_ret,
                 const char *team_a_name, const char *team_b_name,
                 int team_a_count, int team_b_count,
                 Bool debug_p)
{
  sonar_sensor_data *ssd = (sonar_sensor_data *) calloc (1, sizeof(*ssd));
  sim_data *sd = (sim_data *) calloc (1, sizeof(*sd));

  sd->team_a_name  = team_a_name;
  sd->team_b_name  = team_b_name;
  sd->team_a_count = team_a_count;
  sd->team_b_count = team_b_count;
  sd->debug_p      = debug_p;

  ssd->closure       = sd;
  ssd->scan_cb       = sim_scan;
  ssd->free_data_cb  = sim_free_data;
  ssd->free_bogie_cb = sim_free_bogie_data;

  make_bogies (ssd);

  return ssd;
}

