/* xscreensaver-systemd, Copyright (c) 2019-2023
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
 *****************************************************************************
 *
 * This utility provides systemd integration for XScreenSaver.
 * It does two things:
 *
 *   - When the system is about to go to sleep (e.g., laptop lid closing)
 *     it locks the screen *before* the system goes to sleep, by running
 *     "xscreensaver-command -suspend".  And then when the system wakes
 *     up again, it runs "xscreensaver-command --deactivate" to force the
 *     unlock dialog to appear immediately.
 *
 *   - When another process on the system asks for the screen saver to be
 *     inhibited (e.g. because a video is playing) this program periodically
 *     runs "xscreensaver-command --deactivate" to keep the display un-blanked.
 *     It does this until the other program asks for it to stop.
 *
 * For this to work at all, you must either:
 *
 *     A: Be running GNOME's "org.gnome.SessionManager" D-Bus service; or
 *     B: Be running KDE's "org.kde.Solid.PowerManagement.PolicyAgent" svc; or
 *     C: Prevent your desktop environment from running the
 *        "org.freedesktop.ScreenSaver" service.
 *
 *
 *****************************************************************************
 *
 * Background:
 *
 *     For decades, the traditional way for a video player to temporarily
 *     inhibit the screen saver was to have a heartbeat command that ran
 *     "xscreensaver-command --deactivate" once a minute while the video was
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
 *     by using "tracking peers" but that seems to not work on all systems.
 *
 *     Furthermore, we can't listen for these "inhibit blanking" requests
 *     if some other program is already listening for them -- which GNOME and
 *     KDE do by default, even if their screen savers are otherwise disabled.
 *
 *     Both GNOME and KDE bind to "org.freedesktop.ScreenSaver" by default,
 *     meaning that we can't listen for events sent there.  However, after
 *     receiving events at "org.freedesktop.ScreenSaver" they both *also*
 *     sends out a secondary set of notifications that we *are* able to
 *     receive.  Naturally GNOME and KDE do this in differently idiosyncratic
 *     ways.
 *
 *     If you use some third desktop system that registers itself as
 *     "org.freedesktop.ScreenSaver", you will need to convince it to stop
 *     doing that.
 *
 *     To recap: because the existing video players decided to delete the
 *     single line of code that they already had -- the heartbeat call to
 *     "xscreensaver-command --deactivate" -- we had to respond by adding
 *     TWELVE HUNDRED LINES of complicated code that talks to a server that
 *     may not be running, and that may not allow us to connect, and that may
 *     not work properly anyway.
 *
 *     This is what is laughingly referred to as "progress".
 *
 *     So here's what we're dealing with now, with the various apps that
 *     you might use to play video on Linux at the end of 2020:
 *
 *
 *****************************************************************************
 *
 * Firefox 91.9.0esr, Raspbian 11.1:
 *
 *     Sends "Inhibit" to the first of these targets that exists at launch:
 *     "org.freedesktop.ScreenSaver /ScreenSaver" or
 *     "org.gnome.SessionManager /org/gnome/SessionManager".
 *     In the source, "WakeLockListener.cpp".
 *
 *   Inhibit:	Kind:				Reason:
 *
 *      Yes	MP4 URL				"video-playing"
 *      Yes	MP3 URL				"audio-playing"
 *      Yes	<VIDEO>				"video-playing"
 *      No	<VIDEO AUTOPLAY>		-
 *      No	<VIDEO AUTOPLAY LOOP>		-
 *      Yes	<AUDIO>				"audio-playing"
 *      No	<AUDIO AUTOPLAY>		-
 *      Yes	YouTube IFRAME			"video-playing"
 *      Yes	YouTube IFRAME, autoplay	"video-playing"
 *      Yes	Uninhibit on quit
 *      No	Uninhibit on kill
 *
 *
 *****************************************************************************
 *
 * Chromium 98.0.4758.106-rpt1, Raspbian 11.1:
 *
 *     Sends "Inhibit" to the first of these targets that exists at launch:
 *     "org.gnome.SessionManager /org/gnome/SessionManager" or
 *     "org.freedesktop.ScreenSaver /org/freedesktop/ScreenSaver".
 *     In the source, "power_save_blocker_linux.cc".
 *
 *   Inhibit:	Kind:				Reason:
 *
 *      Yes	MP4 URL				"chromium-browser-v7"
 *      Yes	MP3 URL				"chromium-browser-v7" <-- BAD
 *      Yes	<VIDEO>				"chromium-browser-v7"
 *      Yes	<VIDEO AUTOPLAY>		"chromium-browser-v7"
 *      Yes	<VIDEO AUTOPLAY LOOP>		"chromium-browser-v7" <-- BAD
 *      No	<AUDIO>				-
 *      No	<AUDIO AUTOPLAY>		-
 *      Yes	YouTube IFRAME			"chromium-browser-v7"
 *      Yes	YouTube IFRAME, autoplay	"chromium-browser-v7"
 *      Yes	Uninhibit on quit
 *      No	Uninhibit on kill
 *
 *  Bad Chromium bug #1:
 *
 *     It inhibits when only audio is playing, and does so with the same
 *     message as for video, so we can't tell them apart.  This means that,
 *     unlike Firefox, playing audio-only in Chromium will prevent your
 *     screen from blanking.
 *
 *  Bad Chromium bug #2:
 *
 *     It inhibits when short looping videos are playing.  Many sites
 *     auto-convert GIFs to looping MP4s to save bandwidth, so Chromium will
 *     inhibit any time such a GIF is visible.
 *
 *     The proper way for Chrome to fix this would be to stop inhibiting once
 *     the video loops.  That way your multi-hour movie inhibits properly, but
 *     your looping GIF only inhibits for the first few seconds.  Other
 *     reasonable choices might include: do not inhibit for silent or muted
 *     videos; only inhibit for full-screen videos.
 *
 *
 *****************************************************************************
 *
 * Chromium 101.0.4951.64, Debian 11.3:
 *
 *     Same as the above, except that "chromium-browser-v7" has changed to
 *     "Video Wake Lock".
 *
 *     When playing videos, it sends two "Inhibit" requests: "Video Wake Lock"
 *     and "Playing audio".  When playing audio, it also sends those same two
 *     requests.  Closer!  Still wrong.
 *
 *
 *****************************************************************************
 *
 * Chrome 101.0.4951.64 (Official Google Build), Debian 11.3:
 *
 *     Same.
 *
 *****************************************************************************
 *
 * MPV 0.32.0-3, Raspbian 11.1:
 *
 *     While playing, it runs "xdg-screensaver reset" every 10 seconds as a
 *     heartbeat.  That program is a super-complicated shell script that will
 *     eventually run "xscreensaver-command -reset".  So MPV talks to the
 *     xscreensaver daemon directly rather than going through systemd.
 *     That's fine.
 *
 *     On Debian 10.4 through 11.1, MPV does not have a dependency on the
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
 *
 *****************************************************************************
 *
 * MPV 0.33+, I think:
 *
 *     The developer had a hissy fit and removed "xdg-screensaver" support:
 *
 *     https://github.com/mpv-player/mpv/commit/c498b2846af0ee8835b9144c9f6893568a4e49c6
 *
 *     So now I guess you're back to figuring out how to add a "heartbeat"
 *     command to have MPV periodically call "xscreensaver-command -reset".
 *     Good luck with that.  Maybe you should just use VLC instead.
 *
 *
 *****************************************************************************
 *
 * VLC 3.0.16-1, Raspbian 11.1 & Debian 11.3:
 *
 *     Sends "Inhibit" to the first of these targets that exists at launch:
 *     "org.freedesktop.ScreenSaver /ScreenSaver" or
 *     "org.mate.SessionManager /org/mate/SessionManager" or
 *     "org.gnome.SessionManager /org/gnome/SessionManager".
 *
 *     For video, it sends the reason "Playing some media."  It does not send
 *     "Inhibit" when playing audio only.  Also it is the only program to
 *     properly send "Uninhibit" when killed.
 *
 *     It also contains code to run "xdg-screensaver reset" as a heartbeat,
 *     but I have never seen that happen.
 *     In the source, "vlc/modules/misc/inhibit/dbus.c" and "xdg.c".
 *
 *
 *****************************************************************************
 *
 * MPlayer 2:1.4, Raspbian 11.1:
 *
 *     Apparently makes no attempt to inhibit the screen saver.
 *
 *     Update, Sep 2023: rumor has it that this works with some versions.
 *     I have not verified this:
 *
 *       ~/.mplayer/config:
 *       heartbeat-cmd="xscreensaver-command --deactivate >&- 2>&- &"
 *
 *
 *****************************************************************************
 *
 * Zoom 5.10.4.2845, Debian 11.3:
 *
 *    Sends "Inhibit" to "org.freedesktop.ScreenSaver" with "in a meeting".
 *    I think it does this for both video and audio-only meetings, but that
 *    seems reasonable.
 *
 *
 *****************************************************************************
 *
 * Steam 1:1.0.0.74, Debian 11.3:
 *
 *     Inhibits, then uninhibits and immediately reinhibits every 30 seconds
 *     forever.  Sometimes it identifies as "Steam", sometimes as "My SDL
 *     application", AKA "Baby's First Hello World".  Perfect, no notes.
 *
 *
 *****************************************************************************
 *
 * XFCE Power Manager Presentation Mode:
 *
 *     This setting prevents the screen from blanking, and has a long history
 *     of becoming turned on accidentally. Tries org.freedesktop.ScreenSaver
 *     and others before falling back to "xscreensaver-command --deactivate"
 *     as a heartbeat.
 *
 *
 *****************************************************************************
 *
 * Kodi / XMBC:
 *
 *     Apparently makes no attempt to inhibit the screen saver.
 *     There are some third party extensions that fix that, e.g.
 *     "kodi-prevent-xscreensaver".
 *
 *
 *****************************************************************************
 *
 *
 * TO DO:
 *
 *   - What is "org.mate.SessionManager"?  If it is just a re-branded
 *     "org.gnome.SessionManager" with the same behavior, we should probably
 *     listen to "InhibitorAdded" on that as well.
 *
 *   - Apparently the two different desktops have managed to come up with
 *     *three* different ways for dbus clients to ask the question, "is the
 *     screen currently blanked?"  We should probably also respond to these:
 *
 *     qdbus org.freedesktop.ScreenSaver /ScreenSaver
 *           org.freedesktop.ScreenSaver.GetActive
 *     qdbus org.kde.screensaver         /ScreenSaver
 *           org.freedesktop.ScreenSaver.GetActive
 *     qdbus org.gnome.ScreenSaver       /ScreenSaver
 *           org.gnome.ScreenSaver.GetActive
 *
 *
 *****************************************************************************
 *
 * TESTING:
 *
 *   To snoop the bus, "dbus-monitor" or "dbus-monitor --system".
 *
 *   To call the D-BUS methods manually, you can use "busctl":
 *
 *     busctl --user call org.freedesktop.ScreenSaver \
 *       /ScreenSaver org.freedesktop.ScreenSaver \
 *       Inhibit ss test-application test-reason
 *
 *   This will hand out a cookie, which you can pass back to UnInhibit:
 *
 *     u 1792821391
 *
 *     busctl --user call org.freedesktop.ScreenSaver \
 *       /ScreenSaver org.freedesktop.ScreenSaver \
 *       UnInhibit u 1792821391
 *
 *
 *****************************************************************************
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

/* This is for power management, on the system bus.
 */
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
#define DBUS_SD_MATCH        "type='signal'," \
                             "interface='" DBUS_SD_INTERFACE "'," \
                             "member='PrepareForSleep'"

/* This is for handling requests to lock or unlock, on the system bus.
   This is sent by "loginctl lock-session", which in turn is run if someone
   has added "HandleLidSwitch=lock" to /etc/systemd/logind.conf in order to
   make closing the lid lock the screen *without* suspending.  Note that it
   is sent on login1 *Session*, not *Manager*.
 */
#define DBUS_SD_LOCK_INTERFACE "org.freedesktop.login1.Session"
#define DBUS_SD_LOCK_MATCH     "type='signal'," \
                               "interface='" DBUS_SD_LOCK_INTERFACE "'," \
                               "member='Lock'"
#define DBUS_SD_UNLOCK_MATCH   "type='signal'," \
                               "interface='" DBUS_SD_LOCK_INTERFACE "'," \
                               "member='Unlock'"

/* This is for blanking inhibition, on the user bus.
   See the large comment above for the absolute mess that apps make of this.
 */
#define DBUS_FDO_NAME          "org.freedesktop.ScreenSaver"
#define DBUS_FDO_INTERFACE     DBUS_FDO_NAME
#define DBUS_FDO_OBJECT_PATH_1 "/ScreenSaver"		   /* Firefox, VLC */
#define DBUS_FDO_OBJECT_PATH_2 "/org/freedesktop/ScreenSaver"	/* Chrome  */

#define DBUS_GSN_INTERFACE     "org.gnome.SessionManager"
#define DBUS_GSN_PATH          "/org/gnome/SessionManager"
#define DBUS_GSN_MATCH_1       "type='signal'," \
                               "interface='" DBUS_GSN_INTERFACE "'," \
                               "member='InhibitorAdded'"
#define DBUS_GSN_MATCH_2       "type='signal'," \
                               "interface='" DBUS_GSN_INTERFACE "'," \
                               "member='InhibitorRemoved'"

#define DBUS_KDE_INTERFACE     "org.kde.Solid.PowerManagement.PolicyAgent"
#define DBUS_KDE_PATH         "/org/kde/Solid/PowerManagement/PolicyAgent"
#define DBUS_KDE_MATCH         "type='signal'," \
                               "interface='" DBUS_KDE_INTERFACE "'," \
                               "member='InhibitionsChanged'"
#define INTERNAL_KDE_COOKIE "_KDE_"

/* This is for metadata about D-Bus itself.
 */
#define DBUS_SERVICE_DBUS    "org.freedesktop.DBus"
#define DBUS_PATH_DBUS      "/org/freedesktop/DBus"
#define DBUS_INTERFACE_DBUS  DBUS_SERVICE_DBUS


#define HEARTBEAT_INTERVAL 50  /* seconds */

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


struct handler_ctx {
  sd_bus *system_bus;
  sd_bus_message *lock_message;
  int lock_fd;
  int inhibit_count;
  sd_bus_track *track;
};

static struct handler_ctx global_ctx = { NULL, NULL, -1, 0, NULL };

SLIST_HEAD(inhibit_head, inhibit_entry) inhibit_head =
  SLIST_HEAD_INITIALIZER(inhibit_head);

struct inhibit_entry {
  char *cookie;
  time_t start_time;
  char *appname;
  char *peer;
  char *reason;
  Bool ignored_p;
  Bool seen_p;
  SLIST_ENTRY(inhibit_entry) entries;
};


static void
xscreensaver_command (const char *cmd)
{
  char buf[1024];
  int rc;
  sprintf (buf, "xscreensaver-command %.100s --%.100s",
           (verbose_p ? "--verbose" : "--quiet"),
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


/* Make a method call and read a single string reply.
   Like "dbus-send --print-reply --dest=$dest $path $interface.$msg"
 */
static const char *
dbus_send (sd_bus *bus, const char *dest, const char *path,
           const char *interface, const char *msg)
{
  int rc;
  sd_bus_message *m = 0;
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = 0;
  const char *ret = 0;

  rc = sd_bus_message_new_method_call (bus, &m, dest, path, interface, msg);
  if (rc < 0) {
    fprintf (stderr, "%s: dbus_send: failed to create message: %s%s: %s\n",
             blurb(), interface, msg, strerror(-rc));
    return 0;
  }

  sd_bus_message_set_auto_start (m, 1);
  sd_bus_message_set_expect_reply (m, 1);

  rc = sd_bus_call (bus, m, -1, &error, &reply);
  if (rc < 0) {
    fprintf (stderr, "%s: dbus_send: call failed: %s.%s: %s\n", 
             blurb(), interface, msg, strerror(-rc));
    return 0;
  }

  rc = sd_bus_message_read (reply, "s", &ret);
  if (rc < 0) {
    fprintf (stderr, "%s: dbus_send: failed to read reply: %s.%s: %s\n", 
             blurb(), interface, msg, strerror(-rc));
    return 0;
  }

  return ret;
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


/* Called when "org.freedesktop.login1.Manager" sends a "PrepareForSleep"
   signal.  The event is sent twice: before sleep, and after.
 */
static int
xscreensaver_prepare_for_sleep_cb (sd_bus_message *m, void *arg,
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


/* Called when "org.freedesktop.login1.Session" sends a "Lock" signal.
  The event is sent several times in quick succession.
  https://www.freedesktop.org/software/systemd/man/org.freedesktop.login1.html
 */
static int
xscreensaver_lock_cb (sd_bus_message *m, void *arg, sd_bus_error *ret_error)
{
  /* Tell xscreensaver that we are suspending, and to lock if desired.
     Maybe sending "lock" here would be more appropriate, but that might
     do a screen-fade first. */
  xscreensaver_command ("suspend");
  return 1;  /* >= 0 means success */
}

/* Called when "org.freedesktop.login1.Session" sends an "Unlock" signal.
   I'm not sure if anything ever sends this.
 */
static int
xscreensaver_unlock_cb (sd_bus_message *m, void *arg, sd_bus_error *ret_error)
{
  /* Tell xscreensaver to present the unlock dialog right now. */
  xscreensaver_command ("deactivate");
  return 1;  /* >= 0 means success */
}


/* Is this "reason" string one that means we should inhibit blanking?
   If the string is e.g. "audio-playing", (Firefox) the answer is no.
   If the string is "Download in progress", (Chromium) the answer is no.
   Likewise, GEdit sends "There are unsaved documents", which is apparently
   an attempt for it to inhibit *logout*, which is reasonable, rather than
   an attempt to inhibit *blanking*, which is not.  I don't know how to tell
   them apart.
 */
static Bool
good_reason_p (const char *s)
{
  if (!s || !*s) return True;
  if (strcasestr (s, "video")) return True;
  if (strcasestr (s, "audio")) return False;
  if (strcasestr (s, "download")) return False;
  if (strcasestr (s, "unsaved")) return False;
  return True;
}


static const char *
remove_dir (const char *s)
{
  if (s) {
    const char *s2 = strrchr (s, '/');
    if (s2 && s2[1]) return s2+1;
  }
  return s;
}


static struct inhibit_entry *
add_new_entry (struct handler_ctx *ctx,
               const char *cookie,
               const char *application_name,
               const char *sender,
               const char *via,
               const char *inhibit_reason)
{
  struct inhibit_entry *entry = calloc (1, sizeof (*entry));
  entry->cookie     = strdup(cookie);
  entry->appname    = strdup(application_name);
  entry->peer       = sender ? strdup(sender) : NULL;
  entry->reason     = strdup(inhibit_reason ? inhibit_reason : "");
  entry->start_time = time ((time_t *)0);
  entry->ignored_p  = ! good_reason_p (inhibit_reason);
  SLIST_INSERT_HEAD (&inhibit_head, entry, entries);

  if (! entry->ignored_p)
    ctx->inhibit_count++;

  if (verbose_p) {
    char *c2 = (char *)
      malloc ((entry->cookie ? strlen(entry->cookie) : 0) + 20);
    if (cookie && !!strcmp (cookie, INTERNAL_KDE_COOKIE))
      sprintf (c2, " cookie \"%s\"", remove_dir (entry->cookie));
    else
      *c2 = 0;
    fprintf (stderr,
             "%s: inhibited by \"%s\" (%s%s%s) with \"%s\"%s%s\n",
             blurb(), remove_dir (application_name),
             (via ? "via " : ""),
             (via ? via : ""),
             (sender ? sender : ""),
             inhibit_reason,
             c2,
             (entry->ignored_p ? " (ignored)" : ""));
  }

  return entry;
}


static void
free_entry (struct handler_ctx *ctx, struct inhibit_entry *entry)
{
  if (entry->peer) {
    int rc = sd_bus_track_remove_name (ctx->track, entry->peer);
    if (rc < 0) {
      fprintf (stderr, "%s: failed to stop tracking peer \"%s\": %s\n",
               blurb(), entry->peer, strerror(-rc));
    }
    free (entry->peer);
  }

  if (entry->appname) free (entry->appname);
  if (entry->cookie)  free (entry->cookie);
  if (entry->reason)  free (entry->reason);

  if (! entry->ignored_p)
    ctx->inhibit_count--;
  if (ctx->inhibit_count < 0)
    ctx->inhibit_count = 0;

  free (entry);
}


/* Remove any entries with the given cookie.
   If cookie is NULL, instead remove any entries whose peer is dead.
 */
static Bool
remove_matching_entry (struct handler_ctx *ctx,
                       const char *matching_cookie,
                       const char *sender,
                       const char *via)
{
  struct inhibit_entry *entry, *entry_next;
  Bool found = False;

  SLIST_FOREACH_SAFE (entry, &inhibit_head, entries, entry_next) {
    if (matching_cookie
        ? !strcmp (entry->cookie, matching_cookie)
        : !sd_bus_track_count_name (ctx->track, entry->peer)) {
      if (verbose_p) {
        if (matching_cookie)
          fprintf (stderr,
                   "%s: uninhibited by \"%s\" (%s%s%s) with \"%s\""
                   " cookie \"%s\"%s\n",
                   blurb(), remove_dir (entry->appname),
                   (via ? "via " : ""),
                   (via ? via : ""),
                   (sender ? sender : ""),
                   entry->reason,
                   remove_dir (entry->cookie),
                   (entry->ignored_p ? " (ignored)" : ""));
        else
          fprintf (stderr, "%s: peer %s for inhibiting app \"%s\" has died:"
                           " uninhibiting %s%s\n",
                   blurb(), entry->peer, entry->appname,
                   remove_dir (entry->cookie),
                   (entry->ignored_p ? " (ignored)" : ""));
      }
      SLIST_REMOVE (&inhibit_head, entry, inhibit_entry, entries);
      free_entry (ctx, entry);
      found = True;
      break;
    }
  }

  if (!found && matching_cookie && verbose_p)
    fprintf (stderr, "%s: uninhibit: no match for cookie \"%s\"\n",
             blurb(), remove_dir (matching_cookie));

  return found;
}


/* Called from the vtable when another process sends a request to
   "org.freedesktop.ScreenSaver" (on which we are authoritative) to
   inhibit the screen saver.  We return to them a cookie which they must
   present with their "uninhibit" request.
 */
static int
xscreensaver_inhibit_cb (sd_bus_message *m, void *arg,
                         sd_bus_error *ret_error)
{
  struct handler_ctx *ctx = arg;
  const char *application_name = 0, *inhibit_reason = 0;
  const char *sender;
  uint32_t cookie;
  char cookie_str[20];

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

  application_name = remove_dir (application_name);

  /* Tell the global tracker object to monitor when this peer exits. */
  rc = sd_bus_track_add_name(ctx->track, sender);
  if (rc < 0) {
    fprintf (stderr, "%s: failed to track peer \"%s\": %s\n",
             blurb(), sender, strerror(-rc));
    sender = NULL;
  }

  cookie = ya_random();
  sprintf (cookie_str, "%08X", cookie);

  add_new_entry (ctx, cookie_str, application_name, sender, 0, inhibit_reason);
  return sd_bus_reply_method_return (m, "u", cookie);
}


/* Called from the vtable when another process sends a request to
   "org.freedesktop.ScreenSaver" (on which we are authoritative) to
   uninhibit the screen saver.  The cookie must match an earlier "inhibit"
   request.
 */
static int
xscreensaver_uninhibit_cb (sd_bus_message *m, void *arg,
                           sd_bus_error *ret_error)
{
  struct handler_ctx *ctx = arg;
  uint32_t cookie;
  char cookie_str[20];
  const char *sender;

  int rc = sd_bus_message_read (m, "u", &cookie);
  if (rc < 0) {
    fprintf (stderr, "%s: failed to parse method call: %s\n",
             blurb(), strerror(-rc));
    return rc;
  }

  sprintf (cookie_str, "%08X", cookie);
  sender = sd_bus_message_get_sender (m);
  remove_matching_entry (ctx, cookie_str, sender, 0);

  return sd_bus_reply_method_return (m, "");
}


/* Called when "org.gnome.SessionManager" (gnome-shell) sends out a broadcast
   announcement that some other process has received an inhibitor lock.
   Anyone can receive this signal: it is non-exclusive.
 */
static int
xscreensaver_gnome_inhibitor_added_cb (sd_bus_message *m, void *arg,
                                       sd_bus_error *err)
{
  struct handler_ctx *ctx = arg;
  sd_bus *bus = sd_bus_message_get_bus (m);
  const char *path = 0;
  const char *iface = 0;
  const char *sender = 0;
  const char *appid = 0;
  const char *reason = 0;
  char *via;
  int rc;

  rc = sd_bus_message_read (m, "o", &path);
  if (rc < 0) {
    fprintf (stderr, "%s: failed to parse method call: %s\n",
             blurb(), strerror(-rc));
    return rc;
  }

  sender = sd_bus_message_get_sender (m);
  iface = sd_bus_message_get_interface (m);

  appid = dbus_send (bus, DBUS_GSN_INTERFACE, path,
                     DBUS_GSN_INTERFACE ".Inhibitor", "GetAppId");
  if (!appid) return 0;

  appid = remove_dir (appid);
  reason = dbus_send (bus, DBUS_GSN_INTERFACE, path,
                      DBUS_GSN_INTERFACE ".Inhibitor", "GetReason");
  if (!reason) return 0;

  /* We can't get the original peer sender of this message: this is
     a rebroadcast from gnome-shell. */

  via = (char *) malloc (strlen(iface) + strlen(sender) + 10);
  sprintf (via, "%s%s", iface, sender);
  add_new_entry (ctx, path, appid, 0, via, reason);
  free (via);

  return 0;
}


/* Called when "org.gnome.SessionManager" (gnome-shell) sends out a broadcast
   announcement that some other process has relinquished an inhibitor lock.
   Anyone can receive this signal: it is non-exclusive.
 */
static int
xscreensaver_gnome_inhibitor_removed_cb (sd_bus_message *m, void *arg,
                                         sd_bus_error *err)
{
  struct handler_ctx *ctx = arg;
  const char *path = 0;
  const char *sender = 0;
  const char *iface = 0;
  int rc;

  rc = sd_bus_message_read (m, "o", &path);
  if (rc < 0) {
    fprintf (stderr, "%s: failed to parse method call: %s\n",
             blurb(), strerror(-rc));
    return rc;
  }

  iface = sd_bus_message_get_interface (m);
  sender = sd_bus_message_get_sender (m);
  remove_matching_entry (ctx, path, sender, iface);
  return 0;
}


/* Called when "org.kde.Solid.PowerManagement.PolicyAgent" (powerdevil)
   sends out a broadcast announcement that some other process has received
   or relinquished an inhibitor lock.  Anyone can receive this signal: it
   is non-exclusive.
 */
static int
xscreensaver_kde_inhibitor_changed_cb (sd_bus_message *m, void *arg,
                                       sd_bus_error *err)
{
  /* When an inhibitor is being added, this message contains the appname
     and reason as an array of two strings.  When one is being removed, the
     message contains an empty array, followed by an array of one element,
     the appname.  So one can tell "inhibit" and "uninhibit" apart, but
     can't tell *which* inhibition was being removed, since the uninhibit
     message doesn't contain the reason or a cookie.  So that's pretty
     worthless.

     Instead of parsing the contents of this message, any time we get
     "InhibitionsChanged", we send a "ListInhibitions" message and parse
     the result of that instead: the current list of inhibitions.
   */
  struct handler_ctx *ctx = arg;
  sd_bus *bus = sd_bus_message_get_bus (m);
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = 0;
  const char *dest = DBUS_KDE_INTERFACE;
  const char *path = DBUS_KDE_PATH;
  const char *interface = dest;
  const char *msg = "ListInhibitions";
  struct inhibit_entry *entry, *entry_next;
  int rc;

  rc = sd_bus_message_new_method_call (bus, &m, dest, path, interface, msg);
  if (rc < 0) {
    fprintf (stderr, "%s: KDE: failed to create message: %s%s: %s\n",
             blurb(), interface, msg, strerror(-rc));
    return rc;
  }

  sd_bus_message_set_auto_start (m, 1);
  sd_bus_message_set_expect_reply (m, 1);

  rc = sd_bus_call (bus, m, -1, &error, &reply);
  if (rc < 0) {
    fprintf (stderr, "%s: KDE: call failed: %s.%s: %s\n", 
             blurb(), interface, msg, strerror(-rc));
    return rc;
  }

  m = reply;

  /* It's an array of an arbitrary number of structs of 2 strings each. */
  rc = sd_bus_message_enter_container (m, 'a', "(ss)");
  if (rc < 0) {
    fprintf (stderr, "%s: KDE: enter container failed: %s.%s: %s\n", 
             blurb(), interface, msg, strerror(-rc));
    return rc;
  }

  /* Since they don't give us the original inhibiting cookie, we have to
     assume that the appname/reason pair is unique: that means that if
     someone sends two identical inhibits, that counts as one.  No nesting.
   */
  interface = "KDE";  /* That string is so long */

  /* Clear the "seen" flags for our internal tracking. */
  SLIST_FOREACH_SAFE (entry, &inhibit_head, entries, entry_next) {
    if (!strcmp (entry->cookie, INTERNAL_KDE_COOKIE))
      entry->seen_p = False;
  }

  /* Iterate over each entry in this message reply.
   */
  while (1) {
    const char *appname = 0, *reason = 0;
    Bool seen_p = False;

    rc = sd_bus_message_read (m, "(ss)", &appname, &reason);
    if (rc < 0) {
      fprintf (stderr, "%s: KDE: message read failed: %s.%s: %s\n", 
               blurb(), interface, msg, strerror(-rc));
      return rc;
    }

    if (rc == 0) break;

    /* Tag any existing entries that match an entry in the new list.
     */
    SLIST_FOREACH_SAFE (entry, &inhibit_head, entries, entry_next) {
      if (!strcmp (entry->cookie, INTERNAL_KDE_COOKIE) &&
          !strcmp (entry->appname, appname) &&
          !strcmp (entry->reason, reason)) {
        entry->seen_p = True;
        seen_p = True;
      }
    }

    /* Add a new entry if this one is not already in the list.
     */
    if (! seen_p) {
      entry = add_new_entry (ctx, INTERNAL_KDE_COOKIE, appname, 0,
                             interface, reason);
      entry->seen_p = True;
    }
  }

  /* Remove any existing entries that were not mentioned.
   */
  SLIST_FOREACH_SAFE (entry, &inhibit_head, entries, entry_next) {
    if (!strcmp (entry->cookie, INTERNAL_KDE_COOKIE) &&
        !entry->seen_p) {
      if (verbose_p)
        fprintf (stderr,
                 "%s: uninhibited by \"%s\" (via %s) with \"%s\"%s\n",
                 blurb(), remove_dir (entry->appname),
                 interface,
                 entry->reason,
                 (entry->ignored_p ? " (ignored)" : ""));
      SLIST_REMOVE (&inhibit_head, entry, inhibit_entry, entries);
      free_entry (ctx, entry);
    }
  }

  return 0;
}


/* This vtable defines the services we implement on the
   "org.freedesktop.ScreenSaver" endpoint.
 */
static const sd_bus_vtable xscreensaver_dbus_vtable[] = {
  SD_BUS_VTABLE_START(0),
  SD_BUS_METHOD ("Inhibit", "ss", "u", xscreensaver_inhibit_cb,
                 SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD ("UnInhibit", "u", "", xscreensaver_uninhibit_cb,
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


/* Whether the given service name is currently registered on the bus.
 */
static Bool
service_exists_p (sd_bus *bus, const char *name)
{
  int rc;
  sd_bus_message *m = 0;
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = 0;
  int ret = 0;
  const char *dest = DBUS_SERVICE_DBUS;
  const char *path = DBUS_PATH_DBUS;
  const char *interface = DBUS_INTERFACE_DBUS;
  const char *msg = "NameHasOwner";

  rc = sd_bus_message_new_method_call (bus, &m, dest, path, interface, msg);
  if (rc < 0) {
    fprintf (stderr, "%s: dbus_send: failed to create message: %s%s: %s\n",
             blurb(), interface, msg, strerror(-rc));
    return 0;
  }

  rc = sd_bus_message_append (m, "s", name);
  if (rc < 0) {
    fprintf (stderr, "%s: dbus_send: failed append arg: %s%s: %s\n",
             blurb(), interface, msg, strerror(-rc));
    return 0;
  }

  sd_bus_message_set_auto_start (m, 1);

  rc = sd_bus_call (bus, m, -1, &error, &reply);
  if (rc < 0) {
    fprintf (stderr, "%s: dbus_send: call failed: %s.%s: %s\n", 
             blurb(), interface, msg, strerror(-rc));
    return 0;
  }

  rc = sd_bus_message_read (reply, "b", &ret);
  if (rc < 0) {
    fprintf (stderr, "%s: dbus_send: failed to read reply: %s.%s: %s\n", 
             blurb(), interface, msg, strerror(-rc));
    return 0;
  }

  return ret;
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

  /* 'system_bus' is where we hold a lock on "org.freedesktop.login1", meaning
     that we will receive a "PrepareForSleep" message when the system is about
     to suspend.

     'user_bus' is where we receive messages from other programs sending
     "Inhibit" and "Uninhibit" requests to "org.freedesktop.ScreenSaver", and
     notifications of same from "org.gnome.SessionManager" and
     "org.kde.Solid.PowerManagement.PolicyAgent".
   */

  rc = sd_bus_open_system (&system_bus);
  if (rc < 0) {
    fprintf (stderr, "%s: system bus connection failed: %s\n",
             blurb(), strerror(-rc));
    goto FAIL;
  }

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

  /* Exit if "org.jwz.XScreenSaver" is already registered on the user bus.
     We don't receive any events on that endpoint, but this is a good mutex
     to prevent more than one copy of "xscreensaver-systemd" from running.
   */
  {
    const char *name = DBUS_CLIENT_NAME;
    rc = sd_bus_request_name (user_bus, name, 0);
    if (rc < 0) {
      pid_t pid = get_bus_name_pid (user_bus, name);
      if (pid != -1) {
        char *pname = process_name (pid);
        if (pname) {
          fprintf (stderr, "%s: \"%s\" in use by pid %lu (%s)\n",
                   blurb(), name, (unsigned long) pid, remove_dir (pname));
          free (pname);
        } else {
          fprintf (stderr, "%s: \"%s\" in use by pid %lu\n",
                   blurb(), name, (unsigned long) pid);
        }
      } else if (-rc == EEXIST || -rc == EALREADY) {
        fprintf (stderr, "%s: \"%s\" already in use\n", blurb(), name);
      } else {
        fprintf (stderr, "%s: unknown error: \"%s\": %s\n",
                 blurb(), name, strerror(-rc));
      }
      goto FAIL;
    } else if (verbose_p) {
      fprintf (stderr, "%s: registered as \"%s\"\n", blurb(), name);
    }
  }

    
  /* Register ourselves as "org.freedesktop.ScreenSaver" if possible.
     If "org.gnome.SessionManager" or "org.kde.Solid.PowerManagement.
     PolicyAgent" are registered, this is optional; otherwise it is
     mandatory.
   */
  {
    const char *gname = DBUS_GSN_INTERFACE;
    const char *kname = DBUS_KDE_INTERFACE;
    const char *name  = DBUS_FDO_NAME;
    Bool fd_p    = False;
    Bool gnome_p = False;
    Bool kde_p   = False;
    time_t start = time ((time_t *) 0);
    time_t now   = start;
    int timeout  = 30;
    int retries  = 0;

    rc = sd_bus_request_name (user_bus, name, 0);
    if (rc >= 0) {
      fd_p = True;
      if (verbose_p)
        fprintf (stderr, "%s: registered as \"%s\"\n", blurb(), name);
    } else {
      pid_t pid = get_bus_name_pid (user_bus, name);
      if (pid != -1) {
        char *pname = process_name (pid);
        if (verbose_p) {
          fprintf (stderr, "%s: \"%s\" in use by pid %lu (%s)\n",
                   blurb(), name, (unsigned long) pid, remove_dir (pname));
          free (pname);
        } else {
          if (verbose_p)
            fprintf (stderr, "%s: \"%s\" in use by pid %lu\n",
                     blurb(), name, (unsigned long) pid);
        }
      } else if (-rc == EEXIST || -rc == EALREADY) {
        if (verbose_p)
          fprintf (stderr, "%s: \"%s\" already in use\n", blurb(), name);
      } else {
        fprintf (stderr, "%s: unknown error for \"%s\": %s\n",
                 blurb(), name, strerror(-rc));
      }
    }

    /* If XScreenSaver was launched at login, it's possible that
       "org.freedesktop.ScreenSaver" has been registered by "ksmserver" but
       "org.kde.Solid.PowerManagement.PolicyAgent" hasn't yet been registered
       by "org_kde_powerdevil".  So give it 30 seconds to see if things
       settle down.
     */
    while (1) {
      gnome_p = service_exists_p (user_bus, gname);
      kde_p   = service_exists_p (user_bus, kname);
      if (fd_p || gnome_p || kde_p)
        break;
      now = time ((time_t *) 0);
      if (now >= start + timeout)
        break;
      
      retries++;
      sleep (3);
    }

    if (verbose_p) {
      int i = 0;
      for (i = 0; i < 2; i++) {
        Bool exists_p    = (i == 0 ? gnome_p : kde_p);
        const char *name = (i == 0 ? gname   : kname);
        char rr[20];
        if (now == start)
          *rr = 0;
        else
          sprintf (rr, " after %lu seconds", now - start);

        if (exists_p) {
          pid_t pid = get_bus_name_pid (user_bus, name);
          if (pid != -1) {
            char *pname = process_name (pid);
            fprintf (stderr, "%s: \"%s\" in use by pid %lu (%s)%s\n",
                     blurb(), name, (unsigned long) pid, remove_dir (pname),
                     rr);
            free (pname);
          } else {
            fprintf (stderr, "%s: \"%s\" in use%s\n", blurb(), name, rr);
          }
        } else {
          fprintf (stderr, "%s: \"%s\" not in use%s\n", blurb(), name, rr);
        }
      }
    }

    if (! (fd_p || gnome_p || kde_p))  /* Must have at least one */
      goto FAIL;

    /* Register our callbacks for things sent to
       "org.freedesktop.ScreenSaver /ScreenSaver" and
       "org.freedesktop.ScreenSaver /org/freedesktop/ScreenSaver"
     */
    if (fd_p) {
      const char * const names[] = { DBUS_FDO_OBJECT_PATH_1,
                                     DBUS_FDO_OBJECT_PATH_2 };
      int i = 0;
      for (i = 0; i < countof(names); i++) {
        rc = sd_bus_add_object_vtable (user_bus, NULL, names[i],
                                       DBUS_FDO_INTERFACE,
                                       xscreensaver_dbus_vtable,
                                       &global_ctx);
        if (rc < 0) {
          fprintf (stderr, "%s: vtable registration failed for %s: %s\n",
                   blurb(), names[i], strerror(-rc));
          goto FAIL;
        }
      }
    }
  }


  /* Obtain a lock fd from the "Inhibit" method, so that we can delay
     sleep when a "PrepareForSleep" signal is posted.
   */
  ctx->system_bus = system_bus;
  rc = xscreensaver_register_sleep_lock (ctx);
  if (rc < 0)
    goto FAIL;

  /* Register a callback for "org.freedesktop.login1.Manager.PrepareForSleep".
     System bus.
   */
  rc = sd_bus_add_match (system_bus, NULL, DBUS_SD_MATCH,
                         xscreensaver_prepare_for_sleep_cb, &global_ctx);
  if (rc < 0) {
    fprintf (stderr, "%s: registering sleep callback failed: %s\n",
             blurb(), strerror(-rc));
    goto FAIL;
  }

  /* Register a callback for "org.freedesktop.login1.Session.Lock".
     System bus.
   */
  rc = sd_bus_add_match (system_bus, NULL, DBUS_SD_LOCK_MATCH,
                         xscreensaver_lock_cb, &global_ctx);
  if (rc < 0) {
    fprintf (stderr, "%s: registering lock-session callback failed: %s\n",
             blurb(), strerror(-rc));
    goto FAIL;
  }

  /* And for Unlock. */
  rc = sd_bus_add_match (system_bus, NULL, DBUS_SD_UNLOCK_MATCH,
                         xscreensaver_unlock_cb, &global_ctx);
  if (rc < 0) {
    fprintf (stderr, "%s: registering lock-session callback failed: %s\n",
             blurb(), strerror(-rc));
    goto FAIL;
  }


  /* Register a callback for "org.gnome.SessionManager.InhibitorAdded".
     User bus.
   */
  rc = sd_bus_add_match (user_bus, NULL, DBUS_GSN_MATCH_1,
                         xscreensaver_gnome_inhibitor_added_cb, &global_ctx);
  if (rc < 0) {
    fprintf (stderr, "%s: registering GNOME inhibitor callback failed: %s\n",
             blurb(), strerror(-rc));
    goto FAIL;
  }

  /* Register a callback for "org.gnome.SessionManager.InhibitorRemoved".
     User bus.
   */
  rc = sd_bus_add_match (user_bus, NULL, DBUS_GSN_MATCH_2,
                         xscreensaver_gnome_inhibitor_removed_cb, &global_ctx);
  if (rc < 0) {
    fprintf (stderr, "%s: registering GNOME de-inhibitor callback failed: %s\n",
             blurb(), strerror(-rc));
    goto FAIL;
  }

  /* Register a callback for "org.kde.Solid.PowerManagement.PolicyAgent.
     InhibitionsChanged".  User bus.
   */
  rc = sd_bus_add_match (user_bus, NULL, DBUS_KDE_MATCH,
                         xscreensaver_kde_inhibitor_changed_cb, &global_ctx);
  if (rc < 0) {
    fprintf (stderr, "%s: registering KDE inhibitor callback failed: %s\n",
             blurb(), strerror(-rc));
    goto FAIL;
  }


  /* Run an event loop forever, and wait for our callbacks to run.
   */
  while (1) {
    struct pollfd fds[3];
    uint64_t poll_timeout_msec, system_timeout_usec, user_timeout_usec;

    /* We MUST call sd_bus_process() on each bus at least once before calling
       sd_bus_get_events(), so just always start the event loop by processing
       all outstanding requests on both busses.
     */
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
       same cookie.
     */
    remove_matching_entry (ctx, NULL, 0, 0);

    /* If we are inhibited and HEARTBEAT_INTERVAL has passed, de-activate the
       screensaver.
     */
    if (ctx->inhibit_count > 0) {
      time_t now = time ((time_t *) 0);
      if (now - last_deactivate_time >= HEARTBEAT_INTERVAL) {
        if (verbose_p) {
          struct inhibit_entry *entry;
          SLIST_FOREACH (entry, &inhibit_head, entries) {
            char ct[100];
            ctime_r (&entry->start_time, ct);
            fprintf (stderr, "%s: inhibited by \"%s\" since %s",
                     blurb(), remove_dir (entry->appname), ct);
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
       to milliseconds expected by poll().
     */
    poll_timeout_msec = ((system_timeout_usec < user_timeout_usec
                          ? system_timeout_usec : user_timeout_usec)
                         / 1000);

    /* If we have been inhibited, we want to wake up at least once every N
       seconds to de-activate the screensaver.
     */
    if (ctx->inhibit_count > 0 &&
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

  progname = remove_dir (argv[0]);

  for (i = 1; i < argc; i++)
    {
      const char *s = argv [i];
      int L;
      if (s[0] == '-' && s[1] == '-') s++;
      L = strlen (s);
      if (L < 2) USAGE ();
      else if (!strncmp (s, "-q",       L)) verbose_p = 0;
      else if (!strncmp (s, "-quiet",   L)) verbose_p = 0;
      else if (!strncmp (s, "-verbose", L)) verbose_p++;
      else if (!strncmp (s, "-v",       L)) verbose_p++;
      else if (!strncmp (s, "-vv",      L)) verbose_p += 2;
      else if (!strncmp (s, "-vvv",     L)) verbose_p += 3;
      else if (!strncmp (s, "-vvvv",    L)) verbose_p += 4;
      else if (!strcmp (s, "-ver") ||
               !strcmp (s, "-vers") ||
               !strcmp (s, "-version"))
        {
          fprintf (stderr, "%s\n", screensaver_id+4);
          exit (1);
        }

      else USAGE ();
    }

# undef ya_rand_init
  ya_rand_init (0);

  exit (xscreensaver_systemd_loop());
}
