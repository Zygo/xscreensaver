/* xscreensaver_web.c - Generic Web Wrapper for XScreenSaver Hacks
 *
 * This provides a generic web interface for any xscreensaver hack,
 * similar to how jwxyz.h provides Android compatibility.
 */

#include <emscripten.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <emscripten/html5.h>
#include "jwxyz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define Bool int
#define True 1
#define False 0

// Generic ModeInfo for web builds
typedef struct {
    int screen;
    int window;
    void *data;
    int width;
    int height;
    int count;
    int fps_p;
    void *display;  // Placeholder for Display*
    void *visual;   // Placeholder for Visual*
    void *colormap; // Placeholder for Colormap*
} ModeInfo;

#define MI_SCREEN(mi) ((mi)->screen)
#define MI_WIDTH(mi) ((mi)->width)
#define MI_HEIGHT(mi) ((mi)->height)
#define MI_COUNT(mi) ((mi)->count)
#define MI_DISPLAY(mi) ((mi)->display)
#define MI_VISUAL(mi) ((mi)->visual)
#define MI_COLORMAP(mi) ((mi)->colormap)

// Generic initialization macro
#define MI_INIT(mi, bps) do { \
    if (!bps) { \
        bps = calloc(1, sizeof(*bps)); \
    } \
} while(0)

// WebGL context handle
static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl_context = -1;

// Global ModeInfo for web
static ModeInfo web_mi;

// Function pointers for the hack's functions
typedef void (*init_func)(ModeInfo *);
typedef void (*draw_func)(ModeInfo *);
typedef void (*reshape_func)(ModeInfo *, int, int);
typedef void (*free_func)(ModeInfo *);

static init_func hack_init = NULL;
static draw_func hack_draw = NULL;
static reshape_func hack_reshape = NULL;
static free_func hack_free = NULL;

// Main loop callback
void main_loop(void* arg) {
    if (hack_draw) {
        hack_draw(&web_mi);
    }
}

// Initialize WebGL context
static int init_webgl() {
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.alpha = 0;
    attrs.depth = 1;
    attrs.stencil = 0;
    attrs.antialias = 1;
    attrs.premultipliedAlpha = 0;
    attrs.preserveDrawingBuffer = 0;
    attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_DEFAULT;
    attrs.failIfMajorPerformanceCaveat = 0;
    attrs.enableExtensionsByDefault = 1;
    attrs.explicitSwapControl = 0;
    attrs.proxyContextToMainThread = EMSCRIPTEN_WEBGL_CONTEXT_PROXY_DISALLOW;
    attrs.renderViaOffscreenBackBuffer = 0;
    attrs.majorVersion = 2;
    attrs.minorVersion = 0;

    webgl_context = emscripten_webgl_create_context("#canvas", &attrs);
    if (webgl_context < 0) {
        printf("Failed to create WebGL context! Error: %d\n", webgl_context);
        return 0;
    }

    if (emscripten_webgl_make_context_current(webgl_context) != EMSCRIPTEN_RESULT_SUCCESS) {
        printf("Failed to make WebGL context current!\n");
        return 0;
    }

    return 1;
}

// Generic web initialization
EMSCRIPTEN_KEEPALIVE
int xscreensaver_web_init(init_func init, draw_func draw, reshape_func reshape, free_func free) {
    hack_init = init;
    hack_draw = draw;
    hack_reshape = reshape;
    hack_free = free;

    // Initialize ModeInfo
    web_mi.screen = 0;
    web_mi.window = 0;
    web_mi.data = NULL;
    web_mi.width = 800;
    web_mi.height = 600;
    web_mi.count = 20;
    web_mi.fps_p = 0;
    web_mi.display = NULL;
    web_mi.visual = NULL;
    web_mi.colormap = NULL;

    // Initialize WebGL
    if (!init_webgl()) {
        return 0;
    }

    // Call the hack's init function
    if (hack_init) {
        hack_init(&web_mi);
    }

    // Set up reshape
    if (hack_reshape) {
        hack_reshape(&web_mi, web_mi.width, web_mi.height);
    }

    // Set up the main loop (60 FPS)
    emscripten_set_main_loop(main_loop, 60, 1);

    return 1;
}

// Web-specific function exports for UI controls
EMSCRIPTEN_KEEPALIVE
void set_speed(GLfloat new_speed) {
    // This would need to be implemented per-hack
    printf("Speed set to: %f\n", new_speed);
}

EMSCRIPTEN_KEEPALIVE
void set_thickness(GLfloat new_thickness) {
    // This would need to be implemented per-hack
    printf("Thickness set to: %f\n", new_thickness);
}

EMSCRIPTEN_KEEPALIVE
void set_spin(int spin_enabled) {
    // This would need to be implemented per-hack
    printf("Spin %s\n", spin_enabled ? "enabled" : "disabled");
}

EMSCRIPTEN_KEEPALIVE
void set_wander(int wander_enabled) {
    // This would need to be implemented per-hack
    printf("Wander %s\n", wander_enabled ? "enabled" : "disabled");
}

EMSCRIPTEN_KEEPALIVE
void handle_mouse_drag(int delta_x, int delta_y) {
    // This would need to be implemented per-hack
    printf("Mouse drag: %d, %d\n", delta_x, delta_y);
}

EMSCRIPTEN_KEEPALIVE
void handle_mouse_wheel(int delta) {
    // This would need to be implemented per-hack
    printf("Mouse wheel: %d\n", delta);
}

// Cleanup function
EMSCRIPTEN_KEEPALIVE
void xscreensaver_web_cleanup() {
    if (hack_free) {
        hack_free(&web_mi);
    }
    
    if (webgl_context >= 0) {
        emscripten_webgl_destroy_context(webgl_context);
        webgl_context = -1;
    }
} 