/* Generated by wayland-scanner 1.23.1 */

/*
 * Copyright © 2015 Martin Gräßlin
 * Copyright © 2022 Simon Ser
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"

#ifndef __has_attribute
# define __has_attribute(x) 0  /* Compatibility with non-clang compilers. */
#endif

#if (__has_attribute(visibility) || defined(__GNUC__) && __GNUC__ >= 4)
#define WL_PRIVATE __attribute__ ((visibility("hidden")))
#else
#define WL_PRIVATE
#endif

extern const struct wl_interface ext_idle_notification_v1_interface;
extern const struct wl_interface wl_seat_interface;

static const struct wl_interface *ext_idle_notify_v1_types[] = {
	&ext_idle_notification_v1_interface,
	NULL,
	&wl_seat_interface,
	&ext_idle_notification_v1_interface,
	NULL,
	&wl_seat_interface,
};

static const struct wl_message ext_idle_notifier_v1_requests[] = {
	{ "destroy", "", ext_idle_notify_v1_types + 0 },
	{ "get_idle_notification", "nuo", ext_idle_notify_v1_types + 0 },
	{ "get_input_idle_notification", "2nuo", ext_idle_notify_v1_types + 3 },
};

WL_PRIVATE const struct wl_interface ext_idle_notifier_v1_interface = {
	"ext_idle_notifier_v1", 2,
	3, ext_idle_notifier_v1_requests,
	0, NULL,
};

static const struct wl_message ext_idle_notification_v1_requests[] = {
	{ "destroy", "", ext_idle_notify_v1_types + 0 },
};

static const struct wl_message ext_idle_notification_v1_events[] = {
	{ "idled", "", ext_idle_notify_v1_types + 0 },
	{ "resumed", "", ext_idle_notify_v1_types + 0 },
};

WL_PRIVATE const struct wl_interface ext_idle_notification_v1_interface = {
	"ext_idle_notification_v1", 2,
	1, ext_idle_notification_v1_requests,
	2, ext_idle_notification_v1_events,
};

