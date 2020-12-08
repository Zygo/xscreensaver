/* xscreensaver-systemd, Copyright (c) 2019-2020
 * Martin Lucina <martin@lucina.net> and Jamie Zawinski <jwz@jwz.org>
 *
 * ISC License
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * This utility provides systemd integration for XScreenSaver.
 * It does two things:
 *
 *   - When the system is about to go to sleep (e.g., laptop lid closing)
 *     it locks the screen *before* the system goes to sleep, by running
 *     "xscreensaver-command -suspend".  And then when the system wakes
 *     up again, it runs "xscreensaver-command -deactivate" to force the
 *     unlock dialog to appear immediately.
 *
 *   - When another process on the system makes asks for the screen saver
 *     to be inhibited (e.g. because a video is playing) this program
 *     periodically runs "xscreensaver-command -deactivate" to keep the
 *     display un-blanked.  It does this until the other program asks for
 *     it to stop.
 *
 *
 * Background:
 *
 *     For decades, the traditional way for a video player to temporarily
 *     inhibit the screen saver was to have a heartbeat command that ran
 *     "xscreensaver-command -deactivate" once a minute while the video was
 *     playing, and ceased when the video was paused or stopped.  The reason
 *     to do it as a heartbeat rather than a toggle is so that the player
 *     fails SAFE -- if the player exits abnormally, the heart stops beating,
 *     and screen saving and locking resumes.
 *
 *     These days, the popular apps do this by using systemd.  The design of
 *     the systemd method easily and trivially allows an app to inhibit the
 *     screen saver, crash, and then never un-inhibit it, so now your screen
 *     will never blank again.
 *
 *     Furthermore, since the systemd method uses cookies to ensure that only
 *     the app that sent "inhibit" can send the matching "uninhibit", simply
 *     re-launching the crashed video player does not fix the problem.
 *
 *         "Did IQs just drop sharply while I was away?" -- Ellen Ripley
 *
 *     So here's what we're dealing with now, with the various apps you might
 *     use to play video on Linux at the end of 2020:
 *
 *
 *****************************************************************************
 *
 * Firefox (version 78.5)
 *
 *     When playing media, Firefox will send "inhibit" to one of these
 *     targets: "org.freedesktop.ScreenSaver" or "org.gnome.SessionManager".
 *
 *     However, Firefox decides which, if any, of those to use at launch time,
 *     and does not revisit that decision.  So if xscreensaver-systemd has not
 *     been launched before Firefox, it won't work.  Fortunately, in most use
 *     cases, xscreensaver will have been launched earlier in the startup
 *     sequence than the web browser.
 *
 *     If you close the tab or exit while playing, Firefox sends "uninhibit".
 *
 * Critical Firefox Bug:
 *
 *     If Firefox crashes or is killed while playing, it never sends
 *     "uninhibit", leaving the screen saver permanently inhibited.  Once
 *     that happens, the only way to un-fuck things is to kill and restart
 *     the "xscreensaver-systemd" program.
 *
 * Annoying Firefox Bug:
 *
 *     Firefox sends an "inhibit" message when it is merely playing audio.
 *     That's horrible.  Playing audio should prevent your machine from going
 *     to sleep, but it should NOT prevent your screen from blanking or
 *     locking.
 *
 *     However at least it sends it with the reason "audio-playing" instead
 *     of "video-playing", meaning we can (and do) special-case Firefox and
 *     ignore that one.
 *
 *
 *****************************************************************************
 *
 * Chrome (version 87)
 *
 *     Sends "inhibit" to "org.freedesktop.ScreenSaver" (though it uses a
 *     a different object path than Firefox does).  Unlike Firefox, Chrome
 *     does not send an "inhibit" message when only audio is playing.
 *
 * Critical Chrome Bug:
 *
 *     If Chrome crashes or is killed while playing, it never sends
 *     "uninhibit", leaving the screen saver permanently inhibited.
 *
 *
 *****************************************************************************
 *
 * Chromium (version 78, Raspbian 10.4)
 *
 *     Does not use "org.freedesktop.ScreenSaver" or "xdg-screensaver".
 *     It appears to make no attempt to inhibit the screen saver while
 *     video is playing.
 *
 *
 *****************************************************************************
 *
 * Chromium (version 84.0.4147.141, Raspbian 10.6)
 *
 *     Sends "inhibit" to "org.freedesktop.ScreenSaver" (though it uses a
 *     a different object path than Firefox does).  Unlike Firefox, Chrome
 *     does not send an "inhibit" message when only audio is playing.
 *
 *     If you close the tab or exit while playing, Chromium sends "uninhibit".
 *
 * Critical Chromium Bug:
 *
 *     If Chromium crashes or is killed while playing, it never sends
 *     "uninhibit", leaving the screen saver permanently inhibited.
 *
 * Annoying Chromium Bug:
 *
 *     Like Firefox, Chromium sends an "inhibit" message when it is merely
 *     playing audio.  Unlike Firefox, it sends exactly the same "reason"
 *     string as it does when playing video, so we can't tell them apart.
 *
 *
 *****************************************************************************
 *
 * MPV (version 0.29.1)
 *
 *     While playing, it runs "xdg-screensaver reset" every 10 seconds as a
 *     heartbeat.  That program is a super-complicated shell script that will
 *     eventually run "xscreensaver-command -reset".  So MPV talks to the
 *     xscreensaver daemon directly rather than going through systemd.
 *     That's fine.
 *
 *     On Debian 10.4 and 10.6, MPV does not have a dependency on the
 *     "xdg-utils" package, so "xdg-screensaver" might not be installed.
 *     Oddly, Chromium *does* have a dependency on "xdg-utils", even though
 *     Chromium doesn't run "xdg-screensaver".
 *
 *     The source code suggests that MPlayer and MPV call XResetScreenSaver()
 *     as well, but only affects the X11 server's built-in screen saver, not
 *     a userspace screen locker like xscreensaver.
 *
 *     They also call XScreenSaverSuspend() which is part of the MIT
 *     SCREEN-SAVER server extension.  XScreenSaver does make use of that
 *     extension because it is worse than useless.  See the commentary at
 *     the top of xscreensaver.c for details.
 *
 * Annoying MPV Bug:
 *
 *     Like Firefox and Chromium, MPV inhibits screen blanking when only
 *     audio is playing.
 *
 *
 *****************************************************************************
 *
 * MPlayer (version mplayer-gui 2:1.3.0)
 *
 *     I can't get this thing to play video at all.  It only plays the audio
 *     of MP4 files, so I can't guess what it might or might not do with video.
 *     It appears to make no attempt to inhibit the screen saver.
 *
 *
 *****************************************************************************
 *
 * VLC (version 3.0.11-0+deb10u1+rpt3)
 *
 *     VLC sends "inhibit" to "org.freedesktop.ScreenSaver" when playing
 *     video.  It does not send "inhibit" when playing audio only, and it
 *     sends "uninhibit" under all the right circumstances.
 *
 *     NOTE: that's what I saw when I tested it on Raspbian 10.6. However,
 *     the version that came with Raspbian 10.4 -- which also called itself
 *     "VLC 3.0.11" -- did not send "uninhibit" when using the window
 *     manager's "close" button!  Or when killed with "kill".
 *
 *     NOTE ALSO: The VLC source code suggests that under some circumstances
 *     it might be talking to these instead: "org.freedesktop.ScreenSaver",
 *     "org.freedesktop.PowerManagement.Inhibit", "org.mate.SessionManager",
 *     and/or "org.gnome.SessionManager".  It also contains code to run
 *     "xdg-screensaver reset" as a heartbeat.  I can't tell how it decides
 *     which system to use.  I have never seen it run "xdg-screensaver".
 *
 *
 *****************************************************************************
 *
 * TO DO:
 *
 *   - What does the standalone Zoom executable do on Linux?  There doesn't
 *     seem to be a Raspbian build, so I can't test it.
 *
 *   - Since the systemd misdesign allows a program to call "inhibit" and
 *     then crash without un-inhibiting, it would be sensible for us to
 *     auto-uninhibit if the inhibiting process's pid goes away, but it
 *     seems that sd_bus_creds_get_pid() never works, so we can't do that.
 *     This is going to be a constant problem!
 *
 *   - xscreensaver_method_uninhibit() does not actually send a reply, are
 *     we doing the right thing when registering it?
 *
 *   - Currently this code is only listening to "org.freedesktop.ScreenSaver".
 *     Perhaps it should listen to "org.mate.SessionManager" and
 *     "org.gnome.SessionManager"?  Where are those documented?
 *
 *   - Do we need to call sd_bus_release_name() explicitly on exit?
 *
 *   - Run under valgrind to check for any memory leaks.
 *
 *
 * TESTING:
 *
 *   To call the D-BUS methods manually, you can use "busctl":
 *
 *   busctl --user call org.freedesktop.ScreenSaver \
 *     /ScreenSaver org.freedesktop.ScreenSaver \
 *     Inhibit ss test-application test-reason
 *
 *   This will hand out a cookie, which you can pass back to UnInhibit:
 *
 *   u 1792821391
 *
 *   busctl --user call org.freedesktop.ScreenSaver \
 *     /ScreenSaver org.freedesktop.ScreenSaver \
 *     UnInhibit u 1792821391
 *
 * https://github.com/mato/xscreensaver-systemd
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <poll.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>

#include "yarandom.h"

#ifdef HAVE_LIBSYSTEMD
# include <systemd/sd-bus.h>

#else   /* !HAVE_LIBSYSTEMD */

 /* This is a testing shim so that I can somewhat test this even on
    machines that only have libsystemd < 221, such as CentOS 7.7...
  */
 typedef struct sd_bus sd_bus;
 typedef struct sd_bus_message sd_bus_message;
 typedef struct sd_bus_slot sd_bus_slot;
 typedef struct sd_bus_creds sd_bus_creds;
 typedef struct { char *message; } sd_bus_error;
 typedef int (*sd_bus_message_handler_t)
   (sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
 #define SD_BUS_ERROR_NULL { 0 }
 static int sd_bus_message_read (sd_bus_message *m, char *types, ...)
   { return -1; }
 static sd_bus_message *sd_bus_message_unref(sd_bus_message *m) { return 0; }
 static void sd_bus_error_free(sd_bus_error *e) { }
 static int sd_bus_call_method(sd_bus *bus, const char *destination,
                               const char *path, const char *interface,
                               const char *member, sd_bus_error *ret_error,
                               sd_bus_message **reply, const char *types, ...)
   { return -1; }
 static int sd_bus_open_user(sd_bus **ret) { return -1; }
 static int sd_bus_request_name(sd_bus *bus, const char *name, uint64_t flags)
   { return -1; }
 static int sd_bus_open_system(sd_bus **ret) { return -1; }
 static int sd_bus_add_match(sd_bus *bus, sd_bus_slot **slot,
                             const char *match,
                             sd_bus_message_handler_t callback, void *userdata)
   { return -1; }
 static int sd_bus_process(sd_bus *bus, sd_bus_message **r) { return -1; }
 static sd_bus *sd_bus_flush_close_unref(sd_bus *bus) { return 0; }
 static void sd_bus_message_ref(sd_bus_message *r) { }
 static int sd_bus_reply_method_return (sd_bus_message *call,
                                        const char *types, ...) { return -1; }
 typedef int (*sd_bus_message_handler_t) (sd_bus_message *m, void *userdata,
                                          sd_bus_error *ret_error);
 struct sd_bus_vtable { const char *a; const char *b; const char *c;
                        sd_bus_message_handler_t d; int e; };
 typedef struct sd_bus_vtable sd_bus_vtable;
# define SD_BUS_VTABLE_START(_flags) { 0 }
# define SD_BUS_VTABLE_END /**/
# define SD_BUS_VTABLE_UNPRIVILEGED -1
# define SD_BUS_METHOD(_member, _signature, _result, _handler, _flags) { \
   _member, _signature, _result, _handler, _flags }
 static int sd_bus_add_object_vtable(sd_bus *bus, sd_bus_slot **slot,
                                     const char *path, const char *interface,
                                     const sd_bus_vtable *vtable,
                                     void *userdata) { return -1; }
 static int sd_bus_get_fd(sd_bus *bus) { return -1; }
 static int sd_bus_get_events(sd_bus *bus) { return -1; }
 static int sd_bus_get_timeout (sd_bus *bus, uint64_t *u) { return -1; }
 static sd_bus_creds *sd_bus_message_get_creds (sd_bus_message *m) {return 0;}
 static int sd_bus_creds_get_pid(sd_bus_creds *c, pid_t *p) { return -1; }
#endif /* !HAVE_LIBSYSTEMD */

#include "queue.h"
#include "version.h"

static char *progname;
static char *screensaver_version;
static int verbose_p = 0;

#define DBUS_CLIENT_NAME     "org.jwz.XScreenSaver"
#define DBUS_SD_SERVICE_NAME "org.freedesktop.login1"
#define DBUS_SD_OBJECT_PATH  "/org/freedesktop/login1"
#define DBUS_SD_INTERFACE    "org.freedesktop.login1.Manager"
#define DBUS_SD_METHOD       "Inhibit"
#define DBUS_SD_METHOD_ARGS  "ssss"
#define DBUS_SD_METHOD_WHAT  "sleep"
#define DBUS_SD_METHOD_WHO   "xscreensaver"
#define DBUS_SD_METHOD_WHY   "lock screen on suspend"
#define DBUS_SD_METHOD_MODE  "delay"

#define DBUS_SD_MATCH "type='signal'," \
                      "interface='" DBUS_SD_INTERFACE "'," \
                      "member='PrepareForSleep'"

#define DBUS_FDO_NAME          "org.freedesktop.ScreenSaver"
#define DBUS_FDO_OBJECT_PATH   "/ScreenSaver"			/* Firefox */
#define DBUS_FDO_OBJECT_PATH_2 "/org/freedesktop/ScreenSaver"	/* Chrome  */
#define DBUS_FDO_INTERFACE     "org.freedesktop.ScreenSaver"

#define HEARTBEAT_INTERVAL 50  /* seconds */


struct handler_ctx {
  sd_bus *system_bus;
  sd_bus_message *lock_message;
  int lock_fd;
  int is_inhibited;
};

static struct handler_ctx global_ctx = { NULL, NULL, -1 };

SLIST_HEAD(inhibit_head, inhibit_entry) inhibit_head =
  SLIST_HEAD_INITIALIZER(inhibit_head);

struct inhibit_entry {
  uint32_t cookie;
  pid_t pid;
  time_t start_time;
  char *appname;
  SLIST_ENTRY(inhibit_entry) entries;
};


static const char *
blurb (void)
{
  static char buf[255];
  time_t now = time ((time_t *) 0);
  char ct[100];
  int n = strlen(progname);
  if (n > 100) n = 99;
  strncpy(buf, progname, n);
  buf[n++] = ':';
  buf[n++] = ' ';
  ctime_r (&now, ct);
  strncpy(buf+n, ct+11, 8);
  strcpy(buf+n+9, ": ");
  return buf;
}


static void
xscreensaver_command (const char *cmd)
{
  char buf[1024];
  int rc;
  sprintf (buf, "xscreensaver-command %.100s -%.100s",
           (verbose_p ? "-verbose" : "-quiet"),
           cmd);
  if (verbose_p)
    fprintf (stderr, "%s: exec: %s\n", blurb(), buf);
  rc = system (buf);
  if (rc == -1)
    fprintf (stderr, "%s: exec failed: %s\n", blurb(), buf);
  else if (WEXITSTATUS(rc) != 0)
    fprintf (stderr, "%s: exec: \"%s\" exited with status %d\n", 
             blurb(), buf, WEXITSTATUS(rc));
}


static int
xscreensaver_register_sleep_lock (struct handler_ctx *ctx)
{
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = NULL;
  int fd = -1;
  int rc = sd_bus_call_method (ctx->system_bus,
                               DBUS_SD_SERVICE_NAME, DBUS_SD_OBJECT_PATH,
                               DBUS_SD_INTERFACE, DBUS_SD_METHOD,
                               &error, &reply,
                               DBUS_SD_METHOD_ARGS,
                               DBUS_SD_METHOD_WHAT, DBUS_SD_METHOD_WHO,
                               DBUS_SD_METHOD_WHY, DBUS_SD_METHOD_MODE);
  if (rc < 0) {
    fprintf (stderr, "%s: inhibit sleep failed: %s\n", 
             blurb(), error.message);
    goto DONE;
  }

  /* Save the lock fd and explicitly take a ref to the lock message. */
  rc = sd_bus_message_read (reply, "h", &fd);
  if (rc < 0 || fd < 0) {
    fprintf (stderr, "%s: inhibit sleep failed: no lock fd: %s\n",
             blurb(), strerror(-rc));
    goto DONE;
  }
  sd_bus_message_ref(reply);
  ctx->lock_message = reply;
  ctx->lock_fd = fd;

 DONE:
  sd_bus_error_free (&error);

  return rc;
}


/* Called when DBUS_SD_INTERFACE sends a "PrepareForSleep" signal.
   The event is sent twice: before sleep, and after.
 */
static int
xscreensaver_systemd_handler (sd_bus_message *m, void *arg,
                              sd_bus_error *ret_error)
{
  struct handler_ctx *ctx = arg;
  int before_sleep;
  int rc;

  rc = sd_bus_message_read (m, "b", &before_sleep);
  if (rc < 0) {
    fprintf (stderr, "%s: message read failed: %s\n",
             blurb(), strerror(-rc));
    return 1;  /* >= 0 means success */
  }

  /* Use the scheme described at
     https://www.freedesktop.org/wiki/Software/systemd/inhibit/
     under "Taking Delay Locks".
   */

  if (before_sleep) {
    /* Tell xscreensaver that we are suspending, and to lock if desired. */
    xscreensaver_command ("suspend");

    if (ctx->lock_message) {
      /* Release the lock, meaning we are done and it's ok to sleep now.
         Don't rely on unref'ing the message to close the fd, do that
         explicitly here.
      */
      close(ctx->lock_fd);
      sd_bus_message_unref (ctx->lock_message);
      ctx->lock_message = NULL;
      ctx->lock_fd = -1;
    } else {
      fprintf (stderr, "%s: no context lock\n", blurb());
    }
  } else {
    /* Tell xscreensaver to present the unlock dialog right now. */
    xscreensaver_command ("deactivate");

    /* We woke from sleep, so we need to re-register for the next sleep. */
    rc = xscreensaver_register_sleep_lock (ctx);
    if (rc < 0)
      fprintf (stderr, "%s: could not re-register sleep lock\n", blurb());
  }

  return 1;  /* >= 0 means success */
}


/* Called from the vtable when another process sends a request to systemd
   to inhibit the screen saver.  We return to them a cookie which they must
   present with their "uninhibit" request.
 */
static int
xscreensaver_method_inhibit (sd_bus_message *m, void *arg,
                             sd_bus_error *ret_error)
{
  struct handler_ctx *ctx = arg;
  const char *application_name = 0, *inhibit_reason = 0;
  struct inhibit_entry *entry = 0;
  sd_bus_creds *creds = 0;
  pid_t pid = 0;
  const char *s;

  int rc = sd_bus_message_read(m, "ss", &application_name, &inhibit_reason);
  if (rc < 0) {
    fprintf (stderr, "%s: failed to parse method call: %s\n",
             blurb(), strerror(-rc));
    return rc;
  }

  if (!application_name || !*application_name) {
    fprintf (stderr, "%s: no app name in method call\n", blurb());
    return -1;
  }

  if (!inhibit_reason || !*inhibit_reason) {
    fprintf (stderr, "%s: no reason in method call from \"%s\"\n",
             blurb(), application_name);
    return -1;
  }

  /* Omit directory (Chrome does this shit) */
  s = strrchr (application_name, '/');
  if (s && s[1]) application_name = s+1;

  if (strcasestr (inhibit_reason, "audio") &&
      !strcasestr (inhibit_reason, "video")) {
    /* Firefox 78 sends an inhibit when playing audio only, with reason
       "audio-playing".  This is horrible.  Ignore it.  (But perhaps it
       would be better to accept it, issue them a cookie, and then just
       ignore that entry?) */
    if (verbose_p)
      fprintf (stderr, "%s: inhibited by \"%s\" with \"%s\", ignored\n",
               blurb(), application_name, inhibit_reason);
    return -1;
  }

  /* Get the pid of the process that called inhibit, so that we can
     auto-uninhibit if that process dies.  We need to do this because so
     many apps do not fail safe, and systemd's design doesn't enforce that.

     Welp, this doesn't work.  We get ENODATA any time we try to get the
     pid of the message sender.  FFFFFFFfffffffuuuuuuu......

     Other things that do not work include: sd_bus_creds_get_exe,
     sd_bus_creds_get_cmdline and sd_bus_creds_get_description.  So maybe
     we could go through the process table and find a pid whose command
     line matches application_name, but there's no guarantee that those are
     the same.
  */
  creds = sd_bus_message_get_creds (m);
  if (!creds) {
    if (verbose_p)
      fprintf (stderr, "%s: inhibit: unable to get creds of \"%s\"\n",
               blurb(), application_name);
  } else {
    rc = sd_bus_creds_get_pid (creds, &pid);
    if (rc < 0 || pid <= 0) {
      pid = 0;
      if (verbose_p)
        fprintf (stderr, "%s: inhibit: unable to get pid of \"%s\": %s\n",
                 blurb(), application_name, strerror(-rc));
    }
  }

  entry = malloc(sizeof (struct inhibit_entry));
  entry->cookie = ya_random();
  entry->appname = strdup(application_name);
  entry->pid = pid;
  entry->start_time = time ((time_t *)0);
  SLIST_INSERT_HEAD(&inhibit_head, entry, entries);
  ctx->is_inhibited++;
  if (verbose_p)
    fprintf (stderr, "%s: inhibited by \"%s\" with \"%s\" -> cookie %08X\n",
             blurb(), application_name, inhibit_reason, entry->cookie);

  return sd_bus_reply_method_return (m, "u", entry->cookie);
}


/* Called from the vtable when another process sends a request to systemd
   to uninhibit the screen saver.  The cookie must match an earlier "inhibit"
   request.
 */
static int
xscreensaver_method_uninhibit (sd_bus_message *m, void *arg,
                               sd_bus_error *ret_error)
{
  struct handler_ctx *ctx = arg;
  uint32_t cookie;
  struct inhibit_entry *entry;
  int found = 0;

  int rc = sd_bus_message_read (m, "u", &cookie);
  if (rc < 0) {
    fprintf (stderr, "%s: failed to parse method call: %s\n",
             blurb(), strerror(-rc));
    return rc;
  }

  SLIST_FOREACH(entry, &inhibit_head, entries) {
    if (entry->cookie == cookie) {
      if (verbose_p)
        fprintf (stderr, "%s: uninhibited by \"%s\" with cookie %08X\n",
                 blurb(), entry->appname, cookie);
      SLIST_REMOVE (&inhibit_head, entry, inhibit_entry, entries);
      if (entry->appname) free (entry->appname);
      free(entry);
      ctx->is_inhibited--;
      if (ctx->is_inhibited < 0)
        ctx->is_inhibited = 0;
      found = 1;
      break;
    }
  }

  if (! found)
    fprintf (stderr, "%s: uninhibit: no match for cookie %08X\n",
             blurb(), cookie);

  return sd_bus_reply_method_return (m, "");
}

/*
 * This vtable defines the service interface we implement.
 */
static const sd_bus_vtable
xscreensaver_dbus_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Inhibit", "ss", "u", xscreensaver_method_inhibit,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("UnInhibit", "u", "", xscreensaver_method_uninhibit,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};


static int
pid_dead (pid_t pid)
{
  int rc = kill (pid, 0);
  if (rc == 0) return 0;	/* Process exists. */
  if (errno == EPERM) return 0;	/* Process exists but is not owned by us. */
  return 1;			/* No such process, ESRCH. */
}


static int
xscreensaver_systemd_loop (void)
{
  sd_bus *system_bus = NULL, *user_bus = NULL;
  struct handler_ctx *ctx = &global_ctx;
  sd_bus_error error = SD_BUS_ERROR_NULL;
  int rc;
  time_t last_deactivate_time = 0;

  /* 'user_bus' is where we receive messages from other programs sending
     inhibit/uninhibit to org.freedesktop.ScreenSaver, etc.
   */

  rc = sd_bus_open_user (&user_bus);
  if (rc < 0) {
    fprintf (stderr, "%s: user bus connection failed: %s\n",
             blurb(), strerror(-rc));
    goto FAIL;
  }

  rc = sd_bus_add_object_vtable (user_bus,
                                 NULL,
                                 DBUS_FDO_OBJECT_PATH,
                                 DBUS_FDO_INTERFACE,
                                 xscreensaver_dbus_vtable,
                                 &global_ctx);
  if (rc < 0) {
    fprintf (stderr, "%s: vtable registration failed: %s\n",
             blurb(), strerror(-rc));
    goto FAIL;
  }

  rc = sd_bus_add_object_vtable (user_bus,
                                 NULL,
                                 DBUS_FDO_OBJECT_PATH_2,
                                 DBUS_FDO_INTERFACE,
                                 xscreensaver_dbus_vtable,
                                 &global_ctx);
  if (rc < 0) {
    fprintf (stderr, "%s: vtable registration failed: %s\n",
             blurb(), strerror(-rc));
    goto FAIL;
  }

  rc = sd_bus_request_name (user_bus, DBUS_FDO_NAME, 0);
  if (rc < 0) {
    fprintf (stderr, "%s: failed to connect as %s: %s\n",
             blurb(), DBUS_FDO_NAME, strerror(-rc));
    goto FAIL;
  }

  rc = sd_bus_request_name (user_bus, DBUS_CLIENT_NAME, 0);
  if (rc < 0) {
    fprintf (stderr, "%s: failed to connect as %s: %s\n",
             blurb(), DBUS_CLIENT_NAME, strerror(-rc));
    goto FAIL;
  }


  /* 'system_bus' is where we hold a lock on org.freedesktop.login1, meaning
     that the system will send us a PrepareForSleep message when the system is
     about to suspend.
  */

  rc = sd_bus_open_system (&system_bus);
  if (rc < 0) {
    fprintf (stderr, "%s: system bus connection failed: %s\n",
             blurb(), strerror(-rc));
    goto FAIL;
  }

  /* Obtain a lock fd from the "Inhibit" method, so that we can delay
     sleep when a "PrepareForSleep" signal is posted. */

  ctx->system_bus = system_bus;
  rc = xscreensaver_register_sleep_lock (ctx);
  if (rc < 0)
    goto FAIL;

  /* This is basically an event mask, saying that we are interested in
     "PrepareForSleep", and to run our callback when that signal is thrown.
  */
  rc = sd_bus_add_match (system_bus, NULL, DBUS_SD_MATCH,
                         xscreensaver_systemd_handler,
                         &global_ctx);
  if (rc < 0) {
    fprintf (stderr, "%s: add match failed: %s\n", blurb(), strerror(-rc));
    goto FAIL;
  }

  if (verbose_p)
    fprintf (stderr, "%s: connected\n", blurb());


  /* Run an event loop forever, and wait for our callback to run.
   */
  while (1) {
    struct pollfd fds[2];
    uint64_t poll_timeout, system_timeout, user_timeout;
    struct inhibit_entry *entry;

    /* We MUST call sd_bus_process() on each bus at least once before calling
       sd_bus_get_events(), so just always start the event loop by processing
       all outstanding requests on both busses. */
    do {
      rc = sd_bus_process (system_bus, NULL);
      if (rc < 0) {
        fprintf (stderr, "%s: failed to process system bus: %s\n",
                 blurb(), strerror(-rc));
        goto FAIL;
      }
    } while (rc > 0);

    do {
      rc = sd_bus_process (user_bus, NULL);
      if (rc < 0) {
        fprintf (stderr, "%s: failed to process user bus: %s\n",
                 blurb(), strerror(-rc));
        goto FAIL;
      }
    } while (rc > 0);

    fds[0].fd = sd_bus_get_fd (system_bus);
    fds[0].events = sd_bus_get_events (system_bus);
    fds[0].revents = 0;

    fds[1].fd = sd_bus_get_fd (user_bus);
    fds[1].events = sd_bus_get_events (user_bus);
    fds[1].revents = 0;

    sd_bus_get_timeout (system_bus, &system_timeout);
    sd_bus_get_timeout (user_bus, &user_timeout);

    if (system_timeout == 0 && user_timeout == 0)
      poll_timeout = 0;
    else if (system_timeout == UINT64_MAX && user_timeout == UINT64_MAX)
      poll_timeout = -1;
    else {
      poll_timeout = (system_timeout < user_timeout
                      ? system_timeout : user_timeout);
      poll_timeout /= 1000000;
    }

    /* Prune any entries whose process has gone away: this happens if
       a program inhibits, then exits without having called uninhibit.
       That would have left us inhibited forever, even if the inhibiting
       program was re-launched, since the new instance won't have the
       same cookie. */
    SLIST_FOREACH (entry, &inhibit_head, entries) {
      if (entry->pid &&   /* Might not know this entry's pid, sigh... */
          pid_dead (entry->pid)) {
        if (verbose_p)
          fprintf (stderr,
                   "%s: pid %lu for inhibiting app \"%s\" has died:"
                   " uninhibiting %08X\n",
                   blurb(),
                   (unsigned long) entry->pid,
                   entry->appname,
                   entry->cookie);
        SLIST_REMOVE (&inhibit_head, entry, inhibit_entry, entries);
        if (entry->appname) free (entry->appname);
        free (entry);
        ctx->is_inhibited--;
        if (ctx->is_inhibited < 0)
          ctx->is_inhibited = 0;
      }
    }


    /* We want to wake up at least once every N seconds to de-activate
       the screensaver if we have been inhibited.
     */
    if (poll_timeout > HEARTBEAT_INTERVAL * 1000)
      poll_timeout = HEARTBEAT_INTERVAL * 1000;

    rc = poll (fds, 2, poll_timeout);
    if (rc < 0) {
      fprintf (stderr, "%s: poll failed: %s\n", blurb(), strerror(-rc));
      exit (EXIT_FAILURE);
    }

    if (ctx->is_inhibited) {
      time_t now = time ((time_t *) 0);
      if (now - last_deactivate_time >= HEARTBEAT_INTERVAL) {
        if (verbose_p) {
          SLIST_FOREACH (entry, &inhibit_head, entries) {
            char ct[100];
            ctime_r (&entry->start_time, ct);
            fprintf (stderr, "%s: inhibited by \"%s\" since %s",
                     blurb(), entry->appname, ct);
          }
        }
        xscreensaver_command ("deactivate");
        last_deactivate_time = now;
      }
    }
  }

 FAIL:
  if (system_bus)
    sd_bus_flush_close_unref (system_bus);

  if (user_bus)
    sd_bus_flush_close_unref (user_bus);

  sd_bus_error_free (&error);

  return EXIT_FAILURE;
}


static char *usage = "\n\
usage: %s [-verbose]\n\
\n\
This program is launched by the xscreensaver daemon to monitor DBus.\n\
It invokes 'xscreensaver-command' to tell the xscreensaver daemon to lock\n\
the screen before the system suspends, e.g., when a laptop's lid is closed.\n\
\n\
It also responds to certain messages sent by media players allowing them to\n\
request that the screen not be blanked during playback.\n\
\n\
From XScreenSaver %s, (c) 1991-%s Jamie Zawinski <jwz@jwz.org>.\n";


#define USAGE() do { \
 fprintf (stderr, usage, progname, screensaver_version, year); exit (1); \
 } while(0)


int
main (int argc, char **argv)
{
  int i;
  char *s;
  char year[5];

  progname = argv[0];
  s = strrchr (progname, '/');
  if (s) progname = s+1;

  screensaver_version = (char *) malloc (5);
  memcpy (screensaver_version, screensaver_id + 17, 4);
  screensaver_version [4] = 0;

  s = strchr (screensaver_id, '-');
  s = strrchr (s, '-');
  s++;
  strncpy (year, s, 4);
  year[4] = 0;

  for (i = 1; i < argc; i++)
    {
      const char *s = argv [i];
      int L;
      if (s[0] == '-' && s[1] == '-') s++;
      L = strlen (s);
      if (L < 2) USAGE ();
      else if (!strncmp (s, "-verbose", L)) verbose_p = 1;
      else if (!strncmp (s, "-quiet",   L)) verbose_p = 0;
      else USAGE ();
    }

# undef ya_rand_init
  ya_rand_init (0);

  exit (xscreensaver_systemd_loop());
}
