/* xscreensaver-systemd, Copyright (c) 2019-2021
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
 * For this to work at all, you must prevent Gnome and KDE from usurping
 * the "org.freedesktop.ScreenSaver" messages, or else this program can't
 * receive them.  The "xscreensaver" man page contains the (complicated)
 * installation instructions.
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
 *     We can sometimes detect that the inhibiting app has exited abnormally
 *     by using "tracking peers" but I'm not sure how reliable that is.
 *
 *     Furthermore, we can't listen for these "inhibit blanking" requests
 *     if some other program is already listening for them -- which Gnome and
 *     KDE do by default, even if their screen savers are otherwise disabled.
 *     That makes it far more complicated for the user to install XScreenSaver
 *     in such a way that "xscreensaver-systemd" can even launch at all.
 *
 *     To recap: because the existing video players decided to delete the
 *     single line of code that they already had -- the heartbeat call to
 *     "xscreensaver-command -deactivate" -- we had to respond by adding a
 *     THOUSAND LINES of complicated code that talks to a server that may
 *     not be running, and that may not allow us to connect, and that may
 *     not work properly anyway, and that makes installation hellaciously
 *     difficult and confusing for the end user.
 *
 *     This is what is laughingly referred to as "progress".
 *
 *     So here's what we're dealing with now, with the various apps that
 *     you might use to play video on Linux at the end of 2020:
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
 * Another Annoying Chromium Bug:
 *
 *     Twitter (and many other sites) auto-convert GIFs to looping MP4s to
 *     save bandwidth.  Chromium inhibits the screen saver any time a Twitter
 *     GIF is on screen (either in the browser or in Tweetdeck).
 *
 *     The proper way for Chrome to fix this would be to stop inhibiting once
 *     the video loops.  That way your multi-hour movie inhibits properly, but
 *     your looping GIF only inhibits for the first few seconds.
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
 * Zoom
 *
 *    I'm told that the proprietary Zoom executable for Linux sends "inhibit"
 *    to "org.freedesktop.ScreenSaver", but I don't have any further details.
 *
 *
 *****************************************************************************
 *
 * Steam:
 *
 *     Inhibits as "My SDL application" (ooh, "Baby's First Hello World",
 *     nice!  You get a gold star sticker) and then 30 seconds later,
 *     uninhibits and immediately re-inhibits, forever.  This works, but
 *     is dumb.
 *
 *****************************************************************************
 *
 *
 * TO DO:
 *
 *   - What precisely does the standalone Zoom executable do on Linux?
 *     There doesn't seem to be a Raspbian build, so I can't test it.
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
 *   - Apparently the two different desktops have managed to come up with
 *     *three* different ways for dbus clients to ask the question, "is the
 *     screen currently blanked?"  We should probably also respond to these:
 *
 *     qdbus org.freedesktop.ScreenSaver /ScreenSaver org.freedesktop.ScreenSaver.GetActive
 *     qdbus org.kde.screensaver         /ScreenSaver org.freedesktop.ScreenSaver.GetActive
 *     qdbus org.gnome.ScreenSaver       /ScreenSaver org.gnome.ScreenSaver.GetActive
 *
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
#include <X11/Xlib.h>

#if defined (HAVE_LIBSYSTEMD)
# include <systemd/sd-bus.h>
#elif defined (HAVE_LIBELOGIND)
# include <elogind/sd-bus.h>
#else
# error Neither HAVE_LIBSYSTEMD nor HAVE_LIBELOGIND is defined.
#endif

#include "version.h"
#include "blurb.h"
#include "yarandom.h"
#include "queue.h"

static char *screensaver_version;

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

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


struct handler_ctx {
  sd_bus *system_bus;
  sd_bus_message *lock_message;
  int lock_fd;
  int is_inhibited;
  sd_bus_track *track;
};

static struct handler_ctx global_ctx = { NULL, NULL, -1, 0, NULL };

SLIST_HEAD(inhibit_head, inhibit_entry) inhibit_head =
  SLIST_HEAD_INITIALIZER(inhibit_head);

struct inhibit_entry {
  uint32_t cookie;
  time_t start_time;
  char *appname;
  char *peer;
  SLIST_ENTRY(inhibit_entry) entries;
};


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
  const char *s;
  const char *sender;

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

  sender = sd_bus_message_get_sender (m);

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
      fprintf (stderr, "%s: inhibited by \"%s\" (%s) with \"%s\", ignored\n",
               blurb(), application_name, sender, inhibit_reason);
    return -1;
  }
  
  /* Tell the global tracker object to monitor when this peer exits. */
  rc = sd_bus_track_add_name(ctx->track, sender);
  if (rc < 0) {
    fprintf (stderr, "%s: failed to track peer \"%s\": %s\n",
             blurb(), sender, strerror(-rc));
    sender = NULL;
  }

  entry = malloc(sizeof (struct inhibit_entry));
  entry->cookie = ya_random();
  entry->appname = strdup(application_name);
  entry->peer = sender ? strdup(sender) : NULL;
  entry->start_time = time ((time_t *)0);
  SLIST_INSERT_HEAD(&inhibit_head, entry, entries);
  ctx->is_inhibited++;
  if (verbose_p)
    fprintf (stderr, "%s: inhibited by \"%s\" (%s) with \"%s\""
             " -> cookie %08X\n",
             blurb(), application_name, sender, inhibit_reason, entry->cookie);

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
  struct inhibit_entry *entry, *entry_next;
  int found = 0;
  const char *sender;

  int rc = sd_bus_message_read (m, "u", &cookie);
  if (rc < 0) {
    fprintf (stderr, "%s: failed to parse method call: %s\n",
             blurb(), strerror(-rc));
    return rc;
  }

  sender = sd_bus_message_get_sender (m);

  SLIST_FOREACH_SAFE(entry, &inhibit_head, entries, entry_next) {
    if (entry->cookie == cookie) {
      if (verbose_p)
        fprintf (stderr, "%s: uninhibited by \"%s\" (%s) with cookie %08X\n",
                 blurb(), entry->appname, sender, cookie);
      SLIST_REMOVE (&inhibit_head, entry, inhibit_entry, entries);
      if (entry->appname) free (entry->appname);
      if (entry->peer) {
        rc = sd_bus_track_remove_name(ctx->track, entry->peer);
        if (rc < 0) {
          fprintf (stderr, "%s: failed to stop tracking peer \"%s\": %s\n",
                   blurb(), entry->peer, strerror(-rc));
        }
        free(entry->peer);
      }
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


/* The only reason this program connects to X at all is so that it dies
   right away when the X server shuts down.  Otherwise the process might
   linger, still connected to systemd but unable to connect to xscreensaver.
 */
static Display *
open_dpy (void)
{
  Display *d;
  const char *s = getenv("DISPLAY");
  if (!s || !*s) {
    fprintf (stderr, "%s: $DISPLAY unset\n", progname);
    exit (1);
  }

  d = XOpenDisplay (s);
  if (!d) {
    fprintf (stderr, "%s: can't open display %s\n", progname, s);
    exit (1);
  }

  return d;
}


static pid_t
get_bus_name_pid (sd_bus *bus, const char *name)
{
  int rc;
  sd_bus_creds *creds;
  pid_t pid;

  rc = sd_bus_get_name_creds (bus, name, SD_BUS_CREDS_PID, &creds);
  if (rc == 0) {
    rc = sd_bus_creds_get_pid (creds, &pid);
    sd_bus_creds_unref (creds);
    if (rc == 0)
      return pid;
  }

  return -1;
}


/* This only works on Linux, but it's useful for the error message.
 */
static char *
process_name (pid_t pid)
{
  char fn[100], buf[100], *s;
  FILE *fd = 0;
  if (pid <= 0) goto FAIL;
  /* "comm" truncates at 16 characters. "cmdline" has nulls between args. */
  sprintf (fn, "/proc/%lu/cmdline", (unsigned long) pid);
  fd = fopen (fn, "r");
  if (!fd) goto FAIL;
  if (!fgets (buf, sizeof(buf)-1, fd)) goto FAIL;
  if (fclose (fd) != 0) goto FAIL;
  s = strchr (buf, '\n');
  if (s) *s = 0;
  return strdup (buf);
 FAIL:
  if (fd) fclose (fd);
  return 0;
}


static int
xscreensaver_systemd_loop (void)
{
  sd_bus *system_bus = NULL, *user_bus = NULL;
  struct handler_ctx *ctx = &global_ctx;
  sd_bus_error error = SD_BUS_ERROR_NULL;
  int rc;
  time_t last_deactivate_time = 0;
  Display *dpy = open_dpy();

  /* 'user_bus' is where we receive messages from other programs sending
     inhibit/uninhibit to org.freedesktop.ScreenSaver, etc.
   */

  rc = sd_bus_open_user (&user_bus);
  if (rc < 0) {
    fprintf (stderr, "%s: user bus connection failed: %s\n",
             blurb(), strerror(-rc));
    goto FAIL;
  }

  /* Create a single tracking object so that we can later ask it,
     "is the peer with this name still around?"  This is how we tell
     that Firefox has exited without calling 'uninhibit'.
   */
  rc = sd_bus_track_new (user_bus,
                         &global_ctx.track,
                         NULL,
                         NULL);
  if (rc < 0) {
    fprintf (stderr, "%s: cannot create user bus tracker: %s\n",
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

  {
    const char * const names[] = { DBUS_FDO_NAME, DBUS_CLIENT_NAME };
    int i = 0;
    for (i = 0; i < countof(names); i++) {
      rc = sd_bus_request_name (user_bus, names[i], 0);
      if (rc < 0) {
        pid_t pid = get_bus_name_pid (user_bus, names[i]);
        if (pid != -1) {
          char *pname = process_name (pid);
          if (pname) {
            fprintf (stderr,
                     "%s: connection failed: \"%s\" in use by pid %lu (%s)\n",
                     blurb(), names[i], (unsigned long) pid, pname);
            free (pname);
          } else {
            fprintf (stderr,
                     "%s: connection failed: \"%s\" in use by pid %lu\n",
                     blurb(), names[i], (unsigned long) pid);
          }
        } else if (-rc == EEXIST || -rc == EALREADY) {
          fprintf (stderr, "%s: connection failed: \"%s\" already in use\n",
                   blurb(), names[i]);
        } else {
          fprintf (stderr, "%s: connection failed for \"%s\": %s\n",
                   blurb(), names[i], strerror(-rc));
        }
        goto FAIL;
      }
    }
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
    struct pollfd fds[3];
    uint64_t poll_timeout_msec, system_timeout_usec, user_timeout_usec;
    struct inhibit_entry *entry, *entry_next;

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

    /* Prune any entries whose original sender has gone away: this happens
       if a program inhibits, then exits without having called uninhibit.
       That would have left us inhibited forever, even if the inhibiting
       program was re-launched, since the new instance won't have the
       same cookie. */
    SLIST_FOREACH_SAFE (entry, &inhibit_head, entries, entry_next) {
      if (entry->peer &&
          !sd_bus_track_count_name (ctx->track, entry->peer)) {
        if (verbose_p)
          fprintf (stderr,
                   "%s: peer %s for inhibiting app \"%s\" has died:"
                   " uninhibiting %08X\n",
                   blurb(),
                   entry->peer,
                   entry->appname,
                   entry->cookie);
        SLIST_REMOVE (&inhibit_head, entry, inhibit_entry, entries);
        if (entry->appname) free (entry->appname);
        free(entry->peer);
        free (entry);
        ctx->is_inhibited--;
        if (ctx->is_inhibited < 0)
          ctx->is_inhibited = 0;
      }
    }

    /* If we are inhibited and HEARTBEAT_INTERVAL has passed, de-activate the
       screensaver. */
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

    /* The remainder of the code that follows is concerned solely with
       determining how long we should wait until the next iteration of the
       event loop.
    */
    rc = sd_bus_get_fd (system_bus);
    if (rc < 0) {
      fprintf (stderr, "%s: sd_bus_get_fd failed for system bus: %s\n",
               blurb(), strerror(-rc));
      goto FAIL;
    }
    fds[0].fd = rc;
    rc = sd_bus_get_events (system_bus);
    if (rc < 0) {
      fprintf (stderr, "%s: sd_bus_get_events failed for system bus: %s\n",
               blurb(), strerror(-rc));
      goto FAIL;
    }
    fds[0].events = rc;
    fds[0].revents = 0;

    rc = sd_bus_get_fd (user_bus);
    if (rc < 0) {
      fprintf (stderr, "%s: sd_bus_get_fd failed for user bus: %s\n",
               blurb(), strerror(-rc));
      goto FAIL;
    }
    fds[1].fd = rc;
    rc = sd_bus_get_events (user_bus);
    if (rc < 0) {
      fprintf (stderr, "%s: sd_bus_get_events failed for user bus: %s\n",
               blurb(), strerror(-rc));
      goto FAIL;
    }
    fds[1].events = rc;
    fds[1].revents = 0;

    /* Activity on the X server connection will wake us from the poll(). */
    fds[2].fd = XConnectionNumber (dpy);
    fds[2].events = POLLIN;
    fds[2].revents = 0;

    rc = sd_bus_get_timeout (system_bus, &system_timeout_usec);
    if (rc < 0) {
      fprintf (stderr, "%s: sd_bus_get_timeout failed for system bus: %s\n",
               blurb(), strerror(-rc));
      goto FAIL;
    }
    sd_bus_get_timeout (user_bus, &user_timeout_usec);
    if (rc < 0) {
      fprintf (stderr, "%s: sd_bus_get_timeout failed for user bus: %s\n",
               blurb(), strerror(-rc));
      goto FAIL;
    }

    /* Pick the smaller of the two bus timeouts and convert from microseconds
       to milliseconds expected by poll(). */
    poll_timeout_msec = ((system_timeout_usec < user_timeout_usec
                          ? system_timeout_usec : user_timeout_usec)
                         / 1000);

    /* If we have been inhibited, we want to wake up at least once every N
       seconds to de-activate the screensaver.
     */
    if (ctx->is_inhibited &&
        poll_timeout_msec > HEARTBEAT_INTERVAL * 1000)
      poll_timeout_msec = HEARTBEAT_INTERVAL * 1000;

    if (poll_timeout_msec < 1000)
      poll_timeout_msec = 1000;

    rc = poll (fds, 3, poll_timeout_msec);
    if (rc < 0) {
      fprintf (stderr, "%s: poll failed: %s\n", blurb(), strerror(errno));
      goto FAIL;
    }

    if (fds[2].revents & (POLLERR | POLLHUP | POLLNVAL)) {
      fprintf (stderr, "%s: X connection closed\n", blurb());
      dpy = 0;
      goto FAIL;
    } else if (fds[2].revents & POLLIN) {
      /* Even though we have requested no events, there are some events that
         the X server sends anyway, e.g. MappingNotify, and if we don't flush
         the fd, it will constantly be ready for reading, and we busy-loop. */
      char buf[1024];
      while (read (fds[2].fd, buf, sizeof(buf)) > 0)
        ;
    }

  } /* Event loop end */

 FAIL:

  if (system_bus)
    sd_bus_flush_close_unref (system_bus);

  if (ctx->track)
    sd_bus_track_unref (ctx->track);

  if (user_bus)
    sd_bus_flush_close_unref (user_bus);

  sd_bus_error_free (&error);

  if (dpy)
    XCloseDisplay (dpy);

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
  char *version = strdup (screensaver_id + 17);
  char *year = strchr (version, '-');
  char *s = strchr (version, ' ');
  *s = 0;
  year = strchr (year+1, '-') + 1;
  s = strchr (year, ')');
  *s = 0;

  screensaver_version = version;

  progname = argv[0];
  s = strrchr (progname, '/');
  if (s) progname = s+1;

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
