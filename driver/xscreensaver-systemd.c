/* xscreensaver-systemd, Copyright (c) 2019 Martin Lucina <martin@lucina.net>
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
 * This is a small utility providing systemd integration for XScreenSaver.
 *
 * When run from ~/.xsession or equivalent, this will:
 *
 *   - Lock the screen before the system goes to sleep (using
 *     xscreensaver-command -suspend).
 *
 *   - Ensure the XScreenSaver password dialog is shown after the system
 *     is resumed (using xscreensaver-command -deactivate).
 *
 * This is implemented using the recommended way to do these things
 * nowadays, namely inhibitor locks. sd-bus is used for DBUS communication,
 * so the only dependency is libsystemd (which you already have if you
 * want this).
 *
 * https://github.com/mato/xscreensaver-systemd
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <systemd/sd-bus.h>

struct handler_ctx {
    sd_bus *bus;
    sd_bus_message *lock;
};
static struct handler_ctx global_ctx = { NULL, NULL };

static int handler(sd_bus_message *m, void *arg,
        sd_bus_error *ret_error)
{
    struct handler_ctx *ctx = arg;
    int before_sleep;
    int rc;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int fd;

    rc = sd_bus_message_read(m, "b", &before_sleep);
    if (rc < 0) {
        warnx("Failed to read message: %s", strerror(-rc));
        return 0;
    }

    /* Use the scheme described at
     * https://www.freedesktop.org/wiki/Software/systemd/inhibit/
     * under "Taking Delay Locks".
     */
    if (before_sleep) {
        rc = system("xscreensaver-command -suspend");
        if (rc == -1) {
            warnx("Failed to run xscreensaver-command");
        }
        else if (WEXITSTATUS(rc) != 0) {
            warnx("xscreensaver-command failed with %d", WEXITSTATUS(rc));
        }

        if (ctx->lock) {
            /*
             * This will release the lock, since we hold the only ref to the
             * message, and sd_bus_message_unref() will close the underlying
             * fd.
             */
            sd_bus_message_unref(ctx->lock);
            ctx->lock = NULL;
        }
        else {
            warnx("Warning: ctx->lock is NULL, this should not happen?");
        }
    }
    else {
        rc = system("xscreensaver-command -deactivate");
        if (rc == -1) {
            warnx("Failed to run xscreensaver-command");
        }
        else if (WEXITSTATUS(rc) != 0) {
            warnx("xscreensaver-command exited with %d", WEXITSTATUS(rc));
        }

        rc = sd_bus_call_method(ctx->bus,
                "org.freedesktop.login1",
                "/org/freedesktop/login1",
                "org.freedesktop.login1.Manager",
                "Inhibit",
                &error,
                &reply,
                "ssss",
                "sleep",
                "xscreensaver",
                "lock screen on suspend",
                "delay");
        if (rc < 0) {
            warnx("Failed to call Inhibit(): %s", error.message);
            goto out;
        }
        /*
         * Verify that the reply actually contains a lock fd.
         */
        rc = sd_bus_message_read(reply, "h", &fd);
        if (rc < 0) {
            warnx("Failed to read message: %s", strerror(-rc));
            goto out;
        }
        assert(fd >= 0);
        ctx->lock = reply;

out:
        sd_bus_error_free(&error);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    sd_bus *bus = NULL, *user_bus = NULL;
    sd_bus_slot *slot = NULL;
    struct handler_ctx *ctx = &global_ctx;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int rc;
    int fd;
    const char *match =
        "type='signal',interface='org.freedesktop.login1.Manager'"
        ",member='PrepareForSleep'";

    rc = sd_bus_open_user(&user_bus);
    if (rc < 0) {
        warnx("Failed to connect to user bus: %s", strerror(-rc));
        goto out;
    }
    rc = sd_bus_request_name(user_bus, "org.jwz.XScreenSaver", 0);
    if (rc < 0) {
        warnx("Failed to acquire well-known name: %s", strerror(-rc));
        warnx("Is another copy of xscreensaver-systemd running?");
        goto out;
    }

    rc = sd_bus_open_system(&bus);
    if (rc < 0) {
        warnx("Failed to connect to system bus: %s", strerror(-rc));
        goto out;
    }
    ctx->bus = bus;

    rc = sd_bus_call_method(bus,
            "org.freedesktop.login1",
            "/org/freedesktop/login1",
            "org.freedesktop.login1.Manager",
            "Inhibit",
            &error,
            &reply,
            "ssss",
            "sleep",
            "xscreensaver",
            "lock screen on suspend",
            "delay");
    if (rc < 0) {
        warnx("Failed to call Inhibit(): %s", error.message);
        goto out;
    }
    /*
     * Verify that the reply actually contains a lock fd.
     */
    rc = sd_bus_message_read(reply, "h", &fd);
    if (rc < 0) {
        warnx("Failed to read message: %s", strerror(-rc));
        goto out;
    }
    assert(fd >= 0);
    ctx->lock = reply;

    rc = sd_bus_add_match(bus, &slot, match, handler, &global_ctx);
    if (rc < 0) {
        warnx("Failed to add match: %s", strerror(-rc));
        goto out;
    }

    for (;;) {
        rc = sd_bus_process(bus, NULL);
        if (rc < 0) {
            warnx("Failed to process bus: %s", strerror(-rc));
            goto out;
        }
        if (rc > 0)
            /* we processed a request, try to process another one, right-away */
            continue;

        /* Wait for the next request to process */
        rc = sd_bus_wait(bus, (uint64_t) -1);
        if (rc < 0) {
            warnx("Failed to wait on bus: %s", strerror(-rc));
            goto out;
        }
    }

out:
    if (reply)
        sd_bus_message_unref(reply);
    if (slot)
        sd_bus_slot_unref(slot);
    if (bus)
        sd_bus_flush_close_unref(bus);
    if (user_bus)
        sd_bus_flush_close_unref(user_bus);
    sd_bus_error_free(&error);

    return EXIT_FAILURE;
}
