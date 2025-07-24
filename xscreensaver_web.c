/* xscreensaver_web.c - Generic Web Wrapper for XScreenSaver Hacks
 *
 * This provides a generic web interface for any xscreensaver hack,
 * similar to how jwxyz.h provides Android compatibility.
 */

#include <emscripten.h>
#include <GLES3/gl3.h>
#include <emscripten/html5.h>
#include "jwxyz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <emscripten/emscripten.h>

// Random number generation for color functions
static unsigned int random_seed = 1;
static unsigned int webgl_random() {
    random_seed = random_seed * 1103515245 + 12345;
    return (random_seed >> 16) & 0x7fff;
}

static double frand(double max) {
    return ((double)webgl_random() / 32767.0) * max;
}

// WebGL 2.0 function declarations (since we're using WebGL 2.0)
#define GLAPI extern
#define GLAPIENTRY

// OpenGL constants that might be missing
#ifndef GL_MODELVIEW
#define GL_MODELVIEW 0x1700
#endif
#ifndef GL_PROJECTION
#define GL_PROJECTION 0x1701
#endif
#ifndef GL_TEXTURE
#define GL_TEXTURE 0x1702
#endif
#ifndef GL_FRONT
#define GL_FRONT 0x0404
#endif
#ifndef GL_AMBIENT_AND_DIFFUSE
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#endif
#ifndef GL_SMOOTH
#define GL_SMOOTH 0x1D01
#endif
#ifndef GL_NORMALIZE
#define GL_NORMALIZE 0x0BA1
#endif

// X11 color constants (already defined in jwxyz.h, so we don't redefine them)

// Color generation constants
#define MAXPOINTS 10

#define Bool int
#define True 1
#define False 0

// WebGL state management
#define MAX_MATRIX_STACK_DEPTH 32
#define MAX_VERTICES 100000

typedef struct {
    GLfloat m[16];
} Matrix4f;

typedef struct {
    Matrix4f stack[MAX_MATRIX_STACK_DEPTH];
    int top;
} MatrixStack;

typedef struct {
    GLfloat x, y, z;
} Vertex3f;

typedef struct {
    GLfloat r, g, b, a;
} Color4f;

typedef struct {
    GLfloat x, y, z;
} Normal3f;

typedef struct {
    Vertex3f vertices[MAX_VERTICES];
    Color4f colors[MAX_VERTICES];
    Normal3f normals[MAX_VERTICES];
    int vertex_count;
    GLenum primitive_type;
    Bool in_begin_end;
} ImmediateMode;

// Global state
static MatrixStack modelview_stack;
static MatrixStack projection_stack;
static MatrixStack texture_stack;
static GLenum current_matrix_mode = GL_MODELVIEW;
static ImmediateMode immediate;
static Color4f current_color = {1.0f, 1.0f, 1.0f, 1.0f};
static int total_vertices_this_frame = 0;
static Bool rendering_enabled = True;
static Normal3f current_normal = {0.0f, 0.0f, 1.0f};
static Bool lighting_enabled = False;
static Bool depth_test_enabled = True;

// WebGL shader program
static GLuint shader_program = 0;
static GLuint vertex_shader = 0;
static GLuint fragment_shader = 0;

// Forward declarations
static void init_opengl_state(void);
static void matrix_identity(Matrix4f *m);
static void matrix_multiply(Matrix4f *result, const Matrix4f *a, const Matrix4f *b);
static void matrix_translate(Matrix4f *m, GLfloat x, GLfloat y, GLfloat z);
static void matrix_rotate(Matrix4f *m, GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
static void matrix_scale(Matrix4f *m, GLfloat x, GLfloat y, GLfloat z);
static GLuint compile_shader(const char *source, GLenum type);
static void init_shaders(void);
static void make_color_path_webgl(int npoints, int *h, double *s, double *v, XColor *colors, int *ncolorsP);
static MatrixStack* get_current_matrix_stack(void);

// OpenGL function forward declarations
void glFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val, GLfloat far_val);
void glMultMatrixd(const GLfloat *m);
void glTranslated(GLfloat x, GLfloat y, GLfloat z);

// Matrix utility functions
static void matrix_identity(Matrix4f *m) {
    for (int i = 0; i < 16; i++) {
        m->m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

static void matrix_multiply(Matrix4f *result, const Matrix4f *a, const Matrix4f *b) {
    Matrix4f temp;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp.m[i * 4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                temp.m[i * 4 + j] += a->m[i * 4 + k] * b->m[k * 4 + j];
            }
        }
    }
    *result = temp;
}

static void matrix_translate(Matrix4f *m, GLfloat x, GLfloat y, GLfloat z) {
    Matrix4f translate;
    matrix_identity(&translate);
    translate.m[12] = x;
    translate.m[13] = y;
    translate.m[14] = z;
    matrix_multiply(m, m, &translate);
}

static void matrix_rotate(Matrix4f *m, GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    // Simplified rotation - only around Z axis for now
    if (z != 0) {
        GLfloat rad = angle * M_PI / 180.0f;
        GLfloat c = cos(rad);
        GLfloat s = sin(rad);
        Matrix4f rotate;
        matrix_identity(&rotate);
        rotate.m[0] = c;
        rotate.m[1] = s;
        rotate.m[4] = -s;
        rotate.m[5] = c;
        matrix_multiply(m, m, &rotate);
    }
}

static void matrix_scale(Matrix4f *m, GLfloat x, GLfloat y, GLfloat z) {
    Matrix4f scale;
    matrix_identity(&scale);
    scale.m[0] = x;
    scale.m[5] = y;
    scale.m[10] = z;
    matrix_multiply(m, m, &scale);
}

// WebGL 2.0 shader compilation
static GLuint compile_shader(const char *source, GLenum type) {
    GLuint shader = glCreateShader(type);
    printf("Created shader %u of type %d\n", shader, type);

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        printf("Shader compilation error: %s\n", info_log);
        printf("Shader source:\n%s\n", source);
    } else {
        printf("Shader %u compiled successfully\n", shader);
    }

    return shader;
}

static void init_shaders() {
    printf("Initializing WebGL 2.0 shaders...\n");

    const char *vertex_source =
        "#version 300 es\n"
        "in vec3 position;\n"
        "in vec3 color;\n"
        "in vec3 normal;\n"
        "uniform mat4 modelview;\n"
        "uniform mat4 projection;\n"
        "out vec3 frag_color;\n"
        "out vec3 frag_normal;\n"
        "void main() {\n"
        "    gl_Position = projection * modelview * vec4(position, 1.0);\n"
        "    frag_color = color;\n"
        "    frag_normal = normal;\n"
        "}\n";

    const char *fragment_source =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec3 frag_color;\n"
        "in vec3 frag_normal;\n"
        "out vec4 out_color;\n"
        "void main() {\n"
        "    out_color = vec4(frag_color, 1.0);\n"
        "}\n";

    vertex_shader = compile_shader(vertex_source, GL_VERTEX_SHADER);
    fragment_shader = compile_shader(fragment_source, GL_FRAGMENT_SHADER);

    printf("Vertex shader: %u, Fragment shader: %u\n", vertex_shader, fragment_shader);

    shader_program = glCreateProgram();
    printf("Created shader program: %u\n", shader_program);

    if (shader_program == 0) {
        printf("ERROR: glCreateProgram failed! Returned 0\n");
        return;
    }

    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);

    printf("Attached shaders to program %u\n", shader_program);

    glLinkProgram(shader_program);

    GLint success;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetProgramInfoLog(shader_program, 512, NULL, info_log);
        printf("Shader linking error: %s\n", info_log);
        printf("Shader program %u linking failed!\n", shader_program);
    } else {
        printf("Shader program %u linked successfully\n", shader_program);

        // Validate the program
        glValidateProgram(shader_program);
        GLint valid;
        glGetProgramiv(shader_program, GL_VALIDATE_STATUS, &valid);
        if (!valid) {
            GLchar info_log[512];
            glGetProgramInfoLog(shader_program, 512, NULL, info_log);
            printf("Shader validation error: %s\n", info_log);
        } else {
            printf("Shader program %u validated successfully\n", shader_program);
        }
    }
}

// Generic ModeInfo for web builds
typedef struct {
    int screen;
    Window window;
    Display *display;
    Visual *visual;
    Colormap colormap;
    int width;
    int height;
    int count;
    int fps_p;
    int polygon_count; // For polygon counting
    void *data;
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

// Animation state (controlled by HexTrail's rotator system)
static int spin_enabled = 1;
static int wander_enabled = 1;
static float animation_speed = 1.0f;

// Main loop callback
void main_loop(void) {
    static int frame_count = 0;
    frame_count++;
    total_vertices_this_frame = 0; // Reset vertex counter each frame

    // Check if rendering is disabled
    if (!rendering_enabled) {
        return; // Skip rendering entirely
    }

    // Stop debug output after frame 240 (4 seconds)
    if (frame_count <= 240) {
        if (frame_count % 60 == 0) { // Log every 60 frames (once per second)
            printf("Main loop frame %d\n", frame_count);
        }
    }

    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (hack_draw) {
        if (frame_count <= 240 && frame_count % 60 == 0) {
            printf("Calling hack_draw...\n");
        }
        hack_draw(&web_mi);
        if (frame_count <= 240 && frame_count % 60 == 0) {
            printf("hack_draw completed\n");
        }
    } else {
        if (frame_count <= 240 && frame_count % 60 == 0) {
            printf("hack_draw is NULL!\n");
        }
    }
}

void glMatrixMode(GLenum mode) {
    current_matrix_mode = mode;
    printf("glMatrixMode: %d\n", mode);
}



void glOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val, GLfloat far_val) {
    printf("glOrtho: %.3f, %.3f, %.3f, %.3f, %.3f, %.3f\n", left, right, bottom, top, near_val, far_val);

    MatrixStack *stack = get_current_matrix_stack();
    if (stack && stack->top >= 0) {
        // Create orthographic projection matrix
        Matrix4f ortho;
        matrix_identity(&ortho);

        GLfloat tx = -(right + left) / (right - left);
        GLfloat ty = -(top + bottom) / (top - bottom);
        GLfloat tz = -(far_val + near_val) / (far_val - near_val);

        ortho.m[0] = 2.0f / (right - left);
        ortho.m[5] = 2.0f / (top - bottom);
        ortho.m[10] = -2.0f / (far_val - near_val);
        ortho.m[12] = tx;
        ortho.m[13] = ty;
        ortho.m[14] = tz;

        matrix_multiply(&stack->stack[stack->top], &ortho, &stack->stack[stack->top]);
        printf("Orthographic matrix applied\n");
    }
}



void glLoadIdentity(void) {
    MatrixStack *stack;
    switch (current_matrix_mode) {
        case GL_MODELVIEW:
            stack = &modelview_stack;
            break;
        case GL_PROJECTION:
            stack = &projection_stack;
            break;
        case GL_TEXTURE:
            stack = &texture_stack;
            break;
        default:
            return;
    }

    matrix_identity(&stack->stack[stack->top]);
}

// Initialize WebGL context and OpenGL state
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
        printf("Failed to create WebGL context! Error: %lu\n", webgl_context);
        return 0;
    }

    if (emscripten_webgl_make_context_current(webgl_context) != EMSCRIPTEN_RESULT_SUCCESS) {
        printf("Failed to make WebGL context current!\n");
        return 0;
    }

    // Initialize OpenGL state
    init_opengl_state();

    return 1;
}

static MatrixStack* get_current_matrix_stack() {
    switch (current_matrix_mode) {
        case GL_MODELVIEW:
            return &modelview_stack;
        case GL_PROJECTION:
            return &projection_stack;
        case GL_TEXTURE:
            return &texture_stack;
        default:
            return &modelview_stack;
    }
}

static void init_opengl_state() {
    // Initialize matrix stacks
    matrix_identity(&modelview_stack.stack[0]);
    matrix_identity(&projection_stack.stack[0]);
    matrix_identity(&texture_stack.stack[0]);
    modelview_stack.top = 0;
    projection_stack.top = 0;
    texture_stack.top = 0;

    // Initialize immediate mode
    immediate.in_begin_end = False;
    immediate.vertex_count = 0;

    // Initialize shaders
    init_shaders();

    // Set up basic OpenGL state
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// Generic web initialization
EMSCRIPTEN_KEEPALIVE
int xscreensaver_web_init(init_func init, draw_func draw, reshape_func reshape, free_func free) {
    printf("xscreensaver_web_init called\n");

    hack_init = init;
    hack_draw = draw;
    hack_reshape = reshape;
    hack_free = free;

    printf("Function pointers set: init=%p, draw=%p, reshape=%p, free=%p\n",
           (void*)init, (void*)draw, (void*)reshape, (void*)free);

    // Initialize ModeInfo
    web_mi.screen = 0;
    web_mi.window = (Window)1;
    web_mi.display = (Display*)1;
    web_mi.visual = (Visual*)1;
    web_mi.colormap = (Colormap)1;
    web_mi.width = 800;
    web_mi.height = 600;
    web_mi.count = 20;
    web_mi.fps_p = 0;
    web_mi.data = NULL;

    printf("ModeInfo initialized: width=%d, height=%d\n", web_mi.width, web_mi.height);

    // Check canvas size
    int canvas_width, canvas_height;
    emscripten_get_canvas_element_size("#canvas", &canvas_width, &canvas_height);
    printf("Canvas size: %dx%d\n", canvas_width, canvas_height);

    // Initialize WebGL
    printf("Initializing WebGL...\n");
    if (!init_webgl()) {
        printf("WebGL initialization failed!\n");
        return 0;
    }
    printf("WebGL initialized successfully\n");

    // Call the hack's init function
    if (hack_init) {
        printf("Calling hack_init...\n");
        hack_init(&web_mi);
        printf("hack_init completed\n");
    } else {
        printf("hack_init is NULL!\n");
    }

    // Set up reshape
    if (hack_reshape) {
        printf("Calling hack_reshape...\n");
        hack_reshape(&web_mi, web_mi.width, web_mi.height);
        printf("hack_reshape completed\n");
    } else {
        printf("hack_reshape is NULL!\n");
    }

    // Set up the main loop (60 FPS)
    printf("Setting up main loop...\n");
    emscripten_set_main_loop(main_loop, 60, 1);
    printf("Main loop set up successfully\n");

    return 1;
}

// Web-specific function exports for UI controls
EMSCRIPTEN_KEEPALIVE
void set_speed(GLfloat new_speed) {
    animation_speed = new_speed;
    printf("Animation speed set to: %f\n", animation_speed);
}

EMSCRIPTEN_KEEPALIVE
void set_thickness(GLfloat new_thickness) {
    // This would need to be implemented per-hack
    printf("Thickness set to: %f\n", new_thickness);
}

EMSCRIPTEN_KEEPALIVE
void set_spin(int new_spin_enabled) {
    spin_enabled = new_spin_enabled;
    printf("Spin %s\n", spin_enabled ? "enabled" : "disabled");
}

EMSCRIPTEN_KEEPALIVE
void set_wander(int new_wander_enabled) {
    wander_enabled = new_wander_enabled;
    printf("Wander %s\n", wander_enabled ? "enabled" : "disabled");
}

EMSCRIPTEN_KEEPALIVE
void stop_rendering() {
    rendering_enabled = False;
    printf("Rendering stopped\n");
}

EMSCRIPTEN_KEEPALIVE
void start_rendering() {
    rendering_enabled = True;
    printf("Rendering started\n");
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

// Dummy init_GL function for web builds
GLXContext *init_GL(ModeInfo *mi) {
    // Return a dummy context pointer
    static GLXContext dummy_context = (GLXContext)1;
    return &dummy_context;
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

Bool screenhack_event_helper(void *display, void *window, void *event) {
    return False;
}

// Missing jwxyz_abort function
void jwxyz_abort(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    // In a real implementation, this would terminate the program
    // For web, we'll just exit the program
    exit(1);
}

// Missing GLU functions for WebGL
void gluPerspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar) {
    GLfloat xmin, xmax, ymin, ymax;

    ymax = zNear * tan(fovy * M_PI / 360.0f);
    ymin = -ymax;
    xmin = ymin * aspect;
    xmax = ymax * aspect;

    glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

void gluLookAt(GLfloat eyex, GLfloat eyey, GLfloat eyez,
               GLfloat centerx, GLfloat centery, GLfloat centerz,
               GLfloat upx, GLfloat upy, GLfloat upz) {
    GLfloat m[16];
    GLfloat x[3], y[3], z[3];
    GLfloat mag;

    /* Make rotation matrix */

    /* Z vector */
    z[0] = eyex - centerx;
    z[1] = eyey - centery;
    z[2] = eyez - centerz;
    mag = sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
    if (mag) {
        z[0] /= mag;
        z[1] /= mag;
        z[2] /= mag;
    }

    /* Y vector */
    y[0] = upx;
    y[1] = upy;
    y[2] = upz;

    /* X vector = Y cross Z */
    x[0] = y[1] * z[2] - y[2] * z[1];
    x[1] = -y[0] * z[2] + y[2] * z[0];
    x[2] = y[0] * z[1] - y[1] * z[0];

    /* Recompute Y = Z cross X */
    y[0] = z[1] * x[2] - z[2] * x[1];
    y[1] = -z[0] * x[2] + z[2] * x[0];
    y[2] = z[0] * x[1] - z[1] * x[0];

    /* cross product gives area of parallelogram, which is < 1.0 for
     * non-perpendicular unit vectors; so normalize x, y here
     */

    mag = sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    if (mag) {
        x[0] /= mag;
        x[1] /= mag;
        x[2] /= mag;
    }

    mag = sqrt(y[0] * y[0] + y[1] * y[1] + y[2] * y[2]);
    if (mag) {
        y[0] /= mag;
        y[1] /= mag;
        y[2] /= mag;
    }

#define M(row,col)  m[col*4+row]
    M(0,0) = x[0];  M(0,1) = x[1];  M(0,2) = x[2];  M(0,3) = 0.0;
    M(1,0) = y[0];  M(1,1) = y[1];  M(1,2) = y[2];  M(1,3) = 0.0;
    M(2,0) = z[0];  M(2,1) = z[1];  M(2,2) = z[2];  M(2,3) = 0.0;
    M(3,0) = 0.0;   M(3,1) = 0.0;   M(3,2) = 0.0;   M(3,3) = 1.0;
#undef M
    glMultMatrixd(m);

    /* Translate Eye to Origin */
    glTranslated(-eyex, -eyey, -eyez);
}

// Real OpenGL implementations for WebGL
void glShadeModel(GLenum mode) {
    // Store shading mode for shader selection
    // For now, just ignore - we'll use flat shading
}

void glPushMatrix(void) {
    MatrixStack *stack;
    switch (current_matrix_mode) {
        case GL_MODELVIEW:
            stack = &modelview_stack;
            break;
        case GL_PROJECTION:
            stack = &projection_stack;
            break;
        case GL_TEXTURE:
            stack = &texture_stack;
            break;
        default:
            return;
    }

    if (stack->top < MAX_MATRIX_STACK_DEPTH - 1) {
        stack->top++;
        stack->stack[stack->top] = stack->stack[stack->top - 1];
    }
}

void glPopMatrix(void) {
    MatrixStack *stack;
    switch (current_matrix_mode) {
        case GL_MODELVIEW:
            stack = &modelview_stack;
            break;
        case GL_PROJECTION:
            stack = &projection_stack;
            break;
        case GL_TEXTURE:
            stack = &texture_stack;
            break;
        default:
            return;
    }

    if (stack->top > 0) {
        stack->top--;
    }
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    MatrixStack *stack;
    switch (current_matrix_mode) {
        case GL_MODELVIEW:
            stack = &modelview_stack;
            break;
        case GL_PROJECTION:
            stack = &projection_stack;
            break;
        case GL_TEXTURE:
            stack = &texture_stack;
            break;
        default:
            return;
    }

    matrix_translate(&stack->stack[stack->top], x, y, z);
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    MatrixStack *stack;
    switch (current_matrix_mode) {
        case GL_MODELVIEW:
            stack = &modelview_stack;
            break;
        case GL_PROJECTION:
            stack = &projection_stack;
            break;
        case GL_TEXTURE:
            stack = &texture_stack;
            break;
        default:
            return;
    }

    matrix_rotate(&stack->stack[stack->top], angle, x, y, z);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z) {
    MatrixStack *stack;
    switch (current_matrix_mode) {
        case GL_MODELVIEW:
            stack = &modelview_stack;
            break;
        case GL_PROJECTION:
            stack = &projection_stack;
            break;
        case GL_TEXTURE:
            stack = &texture_stack;
            break;
        default:
            return;
    }

    matrix_scale(&stack->stack[stack->top], x, y, z);
}

void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
    current_normal.x = nx;
    current_normal.y = ny;
    current_normal.z = nz;
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    current_color.r = r;
    current_color.g = g;
    current_color.b = b;
    current_color.a = 1.0f;

    static int color_count = 0;
    color_count++;
    if (color_count <= 10) { // Only log first 10 color changes
        printf("glColor3f: RGB(%.3f, %.3f, %.3f)\n", r, g, b);
    }
}

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    current_color.r = r;
    current_color.g = g;
    current_color.b = b;
    current_color.a = a;

    static int color_count = 0;
    color_count++;
    if (color_count <= 10) { // Only log first 10 color changes
        printf("glColor4f: RGBA(%.3f, %.3f, %.3f, %.3f)\n", r, g, b, a);
    }
}

void glColor4fv(const GLfloat *v) {
    current_color.r = v[0];
    current_color.g = v[1];
    current_color.b = v[2];
    current_color.a = v[3];

    static int color_count = 0;
    color_count++;
    if (color_count <= 10) { // Only log first 10 color changes
        printf("glColor4fv: RGBA(%.3f, %.3f, %.3f, %.3f)\n", v[0], v[1], v[2], v[3]);
    }
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
    // Store material properties for shader uniforms
    // For now, just ignore - we'll implement basic lighting later
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    if (!immediate.in_begin_end) {
        return;
    }

    if (immediate.vertex_count >= MAX_VERTICES) {
        printf("WARNING: Vertex limit reached (%d), dropping vertex!\n", MAX_VERTICES);
        return;
    }

    immediate.vertices[immediate.vertex_count].x = x;
    immediate.vertices[immediate.vertex_count].y = y;
    immediate.vertices[immediate.vertex_count].z = z;
    immediate.colors[immediate.vertex_count] = current_color;
    immediate.normals[immediate.vertex_count] = current_normal;
    immediate.vertex_count++;
}

void glBegin(GLenum mode) {
    immediate.in_begin_end = True;
    immediate.primitive_type = mode;
    immediate.vertex_count = 0;
}

void glEnd(void) {
    if (!immediate.in_begin_end || immediate.vertex_count == 0) {
        immediate.in_begin_end = False;
        return;
    }

    static int glEnd_count = 0;
    glEnd_count++;
    if (glEnd_count <= 5) { // Only log first 5 glEnd calls
        printf("glEnd #%d: Drawing %d vertices with primitive type %d\n", glEnd_count, immediate.vertex_count, immediate.primitive_type);
    }

    // Draw a test triangle on the first frame to verify rendering works
    static int test_triangle_drawn = 0;
    if (glEnd_count == 1 && !test_triangle_drawn) {
        printf("Drawing test triangle in bright red...\n");
        test_triangle_drawn = 1;

        // Temporarily override the current geometry with a bright red triangle
        immediate.vertex_count = 3;
        immediate.primitive_type = GL_TRIANGLES;

        // Set bright red color
        for (int i = 0; i < 3; i++) {
            immediate.colors[i].r = 1.0f;
            immediate.colors[i].g = 0.0f;
            immediate.colors[i].b = 0.0f;
            immediate.colors[i].a = 1.0f;
        }

        // Set triangle vertices (centered, visible)
        immediate.vertices[0].x = -0.3f; immediate.vertices[0].y = -0.3f; immediate.vertices[0].z = 0.0f;
        immediate.vertices[1].x =  0.3f; immediate.vertices[1].y = -0.3f; immediate.vertices[1].z = 0.0f;
        immediate.vertices[2].x =  0.0f; immediate.vertices[2].y =  0.3f; immediate.vertices[2].z = 0.0f;

        printf("Test triangle vertices set\n");
    }

    // Create VBOs and render
    GLuint vbo_vertices, vbo_colors, vbo_normals;
    glGenBuffers(1, &vbo_vertices);
    glGenBuffers(1, &vbo_colors);
    glGenBuffers(1, &vbo_normals);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
    glBufferData(GL_ARRAY_BUFFER, immediate.vertex_count * sizeof(Vertex3f),
                 immediate.vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_colors);
    glBufferData(GL_ARRAY_BUFFER, immediate.vertex_count * sizeof(Color4f),
                 immediate.colors, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_normals);
    glBufferData(GL_ARRAY_BUFFER, immediate.vertex_count * sizeof(Normal3f),
                 immediate.normals, GL_STATIC_DRAW);

    // Use our WebGL 2.0 wrapper
    printf("Using WebGL 2.0 wrapper...\n");

    // Use the pre-compiled shader program
    glUseProgram(shader_program);

    // Set up projection matrix (simple orthographic for now)
    GLint projection_loc = glGetUniformLocation(shader_program, "projection");
    if (projection_loc != -1) {
        // Simple orthographic projection
        GLfloat projection[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        glUniformMatrix4fv(projection_loc, 1, GL_FALSE, projection);
    }

    // Set up modelview matrix (identity for now)
    GLint modelview_loc = glGetUniformLocation(shader_program, "modelview");
    if (modelview_loc != -1) {
        GLfloat modelview[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        glUniformMatrix4fv(modelview_loc, 1, GL_FALSE, modelview);
    }

    // Create and bind VBOs
    glGenBuffers(1, &vbo_vertices);
    glGenBuffers(1, &vbo_colors);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
    glBufferData(GL_ARRAY_BUFFER, immediate.vertex_count * sizeof(Vertex3f), immediate.vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_colors);
    glBufferData(GL_ARRAY_BUFFER, immediate.vertex_count * sizeof(Color4f), immediate.colors, GL_STATIC_DRAW);

    // Set up vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
    GLint pos_attrib = glGetAttribLocation(shader_program, "position");
    if (pos_attrib != -1) {
        glEnableVertexAttribArray(pos_attrib);
        glVertexAttribPointer(pos_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_colors);
    GLint color_attrib = glGetAttribLocation(shader_program, "color");
    if (color_attrib != -1) {
        glEnableVertexAttribArray(color_attrib);
        glVertexAttribPointer(color_attrib, 4, GL_FLOAT, GL_FALSE, 0, 0);
    }

    // Draw
    glDrawArrays(immediate.primitive_type, 0, immediate.vertex_count);

    // Cleanup
    glDeleteBuffers(1, &vbo_vertices);
    glDeleteBuffers(1, &vbo_colors);

    printf("WebGL 2.0 wrapper rendering completed\n");

    // Check for WebGL errors after drawing
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("WebGL error after glDrawArrays: %d\n", error);
    }

    // Cleanup
    glDeleteBuffers(1, &vbo_vertices);
    glDeleteBuffers(1, &vbo_colors);
    glDeleteBuffers(1, &vbo_normals);

    immediate.in_begin_end = False;
}

// Missing GLX function
void glXMakeCurrent(Display *display, Window window, GLXContext context) {
    // This is handled by our WebGL context management
    // No-op for web builds
}

// Missing OpenGL functions for WebGL compatibility
void glFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val, GLfloat far_val) {
    // Create perspective projection matrix
    Matrix4f frustum;
    matrix_identity(&frustum);

    GLfloat a = (right + left) / (right - left);
    GLfloat b = (top + bottom) / (top - bottom);
    GLfloat c = -(far_val + near_val) / (far_val - near_val);
    GLfloat d = -(2 * far_val * near_val) / (far_val - near_val);

    frustum.m[0] = 2 * near_val / (right - left);
    frustum.m[5] = 2 * near_val / (top - bottom);
    frustum.m[8] = a;
    frustum.m[9] = b;
    frustum.m[10] = c;
    frustum.m[11] = -1;
    frustum.m[14] = d;
    frustum.m[15] = 0;

    matrix_multiply(&projection_stack.stack[projection_stack.top],
                   &projection_stack.stack[projection_stack.top], &frustum);
}

void glMultMatrixd(const GLfloat *m) {
    Matrix4f matrix;
    for (int i = 0; i < 16; i++) {
        matrix.m[i] = m[i];
    }

    MatrixStack *stack;
    switch (current_matrix_mode) {
        case GL_MODELVIEW:
            stack = &modelview_stack;
            break;
        case GL_PROJECTION:
            stack = &projection_stack;
            break;
        case GL_TEXTURE:
            stack = &texture_stack;
            break;
        default:
            return;
    }

    matrix_multiply(&stack->stack[stack->top], &stack->stack[stack->top], &matrix);
}

void glTranslated(GLfloat x, GLfloat y, GLfloat z) {
    glTranslatef(x, y, z);
}

// Real trackball functions from trackball.c
#define TRACKBALLSIZE (0.8f)

// Vector utility functions
static void vzero(float *v) {
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

static void vset(float *v, float x, float y, float z) {
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

static void vsub(const float *src1, const float *src2, float *dst) {
    dst[0] = src1[0] - src2[0];
    dst[1] = src1[1] - src2[1];
    dst[2] = src1[2] - src2[2];
}

static void vcopy(const float *v1, float *v2) {
    for (int i = 0; i < 3; i++)
        v2[i] = v1[i];
}

static void vcross(const float *v1, const float *v2, float *cross) {
    float temp[3];
    temp[0] = (v1[1] * v2[2]) - (v1[2] * v2[1]);
    temp[1] = (v1[2] * v2[0]) - (v1[0] * v2[2]);
    temp[2] = (v1[0] * v2[1]) - (v1[1] * v2[0]);
    vcopy(temp, cross);
}

static float vlength(const float *v) {
    return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static void vscale(float *v, float div) {
    v[0] *= div;
    v[1] *= div;
    v[2] *= div;
}

static void vnormal(float *v) {
    vscale(v, 1.0f / vlength(v));
}

static float vdot(const float *v1, const float *v2) {
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

static void vadd(const float *src1, const float *src2, float *dst) {
    dst[0] = src1[0] + src2[0];
    dst[1] = src1[1] + src2[1];
    dst[2] = src1[2] + src2[2];
}

// Project an x,y pair onto a sphere of radius r OR a hyperbolic sheet
static float tb_project_to_sphere(float r, float x, float y) {
    float d, t, z;
    d = sqrt(x * x + y * y);
    if (d < r * 0.70710678118654752440f) {    /* Inside sphere */
        z = sqrt(r * r - d * d);
    } else {           /* On hyperbola */
        t = r / 1.41421356237309504880f;
        z = t * t / d;
    }
    return z;
}

// Given an axis and angle, compute quaternion
static void axis_to_quat(float a[3], float phi, float q[4]) {
    vnormal(a);
    vcopy(a, q);
    vscale(q, sin(phi / 2.0f));
    q[3] = cos(phi / 2.0f);
}

// Normalize quaternion
static void normalize_quat(float q[4]) {
    float mag = (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    for (int i = 0; i < 4; i++) q[i] /= mag;
}

// Real trackball implementation
void trackball(float q[4], float p1x, float p1y, float p2x, float p2y) {
    float a[3]; /* Axis of rotation */
    float phi;  /* how much to rotate about axis */
    float p1[3], p2[3], d[3];
    float t;

    if (p1x == p2x && p1y == p2y) {
        /* Zero rotation */
        vzero(q);
        q[3] = 1.0f;
        return;
    }

    /*
     * First, figure out z-coordinates for projection of P1 and P2 to
     * deformed sphere
     */
    vset(p1, p1x, p1y, tb_project_to_sphere(TRACKBALLSIZE, p1x, p1y));
    vset(p2, p2x, p2y, tb_project_to_sphere(TRACKBALLSIZE, p2x, p2y));

    /*
     *  Now, we want the cross product of P1 and P2
     */
    vcross(p2, p1, a);

    /*
     *  Figure out how much to rotate around that axis.
     */
    vsub(p1, p2, d);
    t = vlength(d) / (2.0f * TRACKBALLSIZE);

    /*
     * Avoid problems with out-of-control values...
     */
    if (t > 1.0f) t = 1.0f;
    if (t < -1.0f) t = -1.0f;
    phi = 2.0f * asin(t);

    axis_to_quat(a, phi, q);
}

// Real quaternion addition
#define RENORMCOUNT 97

void add_quats(float q1[4], float q2[4], float dest[4]) {
    static int count = 0;
    float t1[4], t2[4], t3[4];
    float tf[4];

    vcopy(q1, t1);
    vscale(t1, q2[3]);

    vcopy(q2, t2);
    vscale(t2, q1[3]);

    vcross(q2, q1, t3);
    vadd(t1, t2, tf);
    vadd(t3, tf, tf);
    tf[3] = q1[3] * q2[3] - vdot(q1, q2);

    dest[0] = tf[0];
    dest[1] = tf[1];
    dest[2] = tf[2];
    dest[3] = tf[3];

    if (++count > RENORMCOUNT) {
        count = 0;
        normalize_quat(dest);
    }
}

// Real rotation matrix builder
void build_rotmatrix(float m[4][4], float q[4]) {
    m[0][0] = 1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]);
    m[0][1] = 2.0f * (q[0] * q[1] - q[2] * q[3]);
    m[0][2] = 2.0f * (q[2] * q[0] + q[1] * q[3]);
    m[0][3] = 0.0f;

    m[1][0] = 2.0f * (q[0] * q[1] + q[2] * q[3]);
    m[1][1] = 1.0f - 2.0f * (q[2] * q[2] + q[0] * q[0]);
    m[1][2] = 2.0f * (q[1] * q[2] - q[0] * q[3]);
    m[1][3] = 0.0f;

    m[2][0] = 2.0f * (q[2] * q[0] - q[1] * q[3]);
    m[2][1] = 2.0f * (q[1] * q[2] + q[0] * q[3]);
    m[2][2] = 1.0f - 2.0f * (q[1] * q[1] + q[0] * q[0]);
    m[2][3] = 0.0f;

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}

void glMultMatrixf(const float *m) {
    // Convert float matrix to GLfloat and multiply
    GLfloat gl_matrix[16];
    for (int i = 0; i < 16; i++) {
        gl_matrix[i] = m[i];
    }
    glMultMatrixd(gl_matrix);
}

// Missing GLX function
void glXSwapBuffers(Display *display, Window window) {
    // This is handled by our WebGL context management
    // No-op for web builds
}

// Real X11 color functions for WebGL
Status XAllocColorCells(Display *display, Colormap colormap, Bool contig, unsigned long plane_masks_return[], unsigned int nplanes, unsigned long pixels_return[], unsigned int npixels) {
    // For WebGL, we don't need to allocate X11 color cells
    // Just assign sequential pixel values
    for (unsigned int i = 0; i < npixels; i++) {
        pixels_return[i] = i;
    }
    return 1; // Success
}

Status XStoreColors(Display *display, Colormap colormap, XColor *defs_in_out, int ncolors) {
    // For WebGL, we don't need to store colors in X11 colormap
    // Colors are handled directly by OpenGL
    return 1; // Success
}

Status XAllocColor(Display *display, Colormap colormap, XColor *screen_in_out) {
    // For WebGL, we don't need to allocate X11 colors
    // Just return success
    return 1; // Success
}

Status XFreeColors(Display *display, Colormap colormap, unsigned long pixels[], int npixels, unsigned long planes) {
    // For WebGL, we don't need to free X11 colors
    return 1; // Success
}

// Missing utility functions
Bool has_writable_cells(Screen *screen, Visual *visual) {
    // Dummy implementation for web builds
    return True;
}

// Color utility functions for WebGL
void hsv_to_rgb(int h, double s, double v, double *r, double *g, double *b) {
    // Simple HSV to RGB conversion
    double c = v * s;
    double x = c * (1 - fabs(fmod(h / 60.0, 2) - 1));
    double m = v - c;

    if (h >= 0 && h < 60) {
        *r = c; *g = x; *b = 0;
    } else if (h >= 60 && h < 120) {
        *r = x; *g = c; *b = 0;
    } else if (h >= 120 && h < 180) {
        *r = 0; *g = c; *b = x;
    } else if (h >= 180 && h < 240) {
        *r = 0; *g = x; *b = c;
    } else if (h >= 240 && h < 300) {
        *r = x; *g = 0; *b = c;
    } else {
        *r = c; *g = 0; *b = x;
    }

    *r += m; *g += m; *b += m;
}

void rgb_to_hsv(unsigned short r, unsigned short g, unsigned short b, int *h, double *s, double *v) {
    double rf = r / 65535.0;
    double gf = g / 65535.0;
    double bf = b / 65535.0;

    double max = fmax(fmax(rf, gf), bf);
    double min = fmin(fmin(rf, gf), bf);
    double delta = max - min;

    *v = max;

    if (max == 0) {
        *s = 0;
        *h = 0;
        return;
    }

    *s = delta / max;

    if (delta == 0) {
        *h = 0;
    } else if (max == rf) {
        *h = (int)(60 * fmod(((gf - bf) / delta), 6));
    } else if (max == gf) {
        *h = (int)(60 * (((bf - rf) / delta) + 2));
    } else {
        *h = (int)(60 * (((rf - gf) / delta) + 4));
    }

    if (*h < 0) *h += 360;
}

// Convert XColor to GLfloat array for OpenGL
void xcolor_to_glfloat(const XColor *xcolor, GLfloat *rgba) {
    rgba[0] = xcolor->red / 65535.0f;   // Red
    rgba[1] = xcolor->green / 65535.0f; // Green
    rgba[2] = xcolor->blue / 65535.0f;  // Blue
    rgba[3] = 1.0f;                     // Alpha
    
    static int xcolor_count = 0;
    xcolor_count++;
    if (xcolor_count <= 5) { // Log first 5 color conversions
        printf("[%ld] xcolor_to_glfloat: XColor(%d,%d,%d) -> GLfloat(%.3f,%.3f,%.3f,%.3f)\n",
               (long)(emscripten_get_now()), xcolor->red, xcolor->green, xcolor->blue, rgba[0], rgba[1], rgba[2], rgba[3]);
    }
}

// Generate smooth color map for WebGL
void make_smooth_colormap_webgl(XColor *colors, int *ncolorsP, Bool allocate_p, Bool *writable_pP, Bool verbose_p) {
    printf("make_smooth_colormap_webgl called: allocate_p=%d, verbose_p=%d\n", allocate_p, verbose_p);
    printf("make_smooth_colormap_webgl: ncolorsP=%d before call\n", *ncolorsP);
    int npoints;
    int ncolors = *ncolorsP;
    int i;
    int h[MAXPOINTS];
    double s[MAXPOINTS];
    double v[MAXPOINTS];
    double total_s = 0;
    double total_v = 0;
    int loop = 0;

    if (*ncolorsP <= 0) return;

    // Randomly choose number of color points
    {
        int n = webgl_random() % 20;
        if      (n <= 5)  npoints = 2;  /* 30% of the time */
        else if (n <= 15) npoints = 3;  /* 50% of the time */
        else if (n <= 18) npoints = 4;  /* 15% of the time */
        else             npoints = 5;   /*  5% of the time */
    }

REPICK_ALL_COLORS:
    for (i = 0; i < npoints; i++) {
    REPICK_THIS_COLOR:
        if (++loop > 10000) abort();
        h[i] = webgl_random() % 360;
        s[i] = ((double)(webgl_random() % 1000)) / 1000.0; // 0.0 to 1.0
        v[i] = ((double)(webgl_random() % 800)) / 1000.0 + 0.2; // 0.2 to 1.0

        // Make sure adjacent colors aren't too close
        if (i > 0) {
            int j = (i+1 == npoints) ? 0 : (i-1);
            double hi = ((double) h[i]) / 360;
            double hj = ((double) h[j]) / 360;
            double dh = hj - hi;
            double distance;
            if (dh < 0) dh = -dh;
            if (dh > 0.5) dh = 0.5 - (dh - 0.5);
            distance = sqrt((dh * dh) +
                          ((s[j] - s[i]) * (s[j] - s[i])) +
                          ((v[j] - v[i]) * (v[j] - v[i])));
            if (distance < 0.2)
                goto REPICK_THIS_COLOR;
        }
        total_s += s[i];
        total_v += v[i];
    }

    // Ensure minimum saturation and value
    if (total_s / npoints < 0.2)
        goto REPICK_ALL_COLORS;
    if (total_v / npoints < 0.3)
        goto REPICK_ALL_COLORS;

    // Generate smooth color path
    make_color_path_webgl(npoints, h, s, v, colors, &ncolors);

    *ncolorsP = ncolors;
}

// Generate smooth color path for WebGL
static void make_color_path_webgl(int npoints, int *h, double *s, double *v, XColor *colors, int *ncolorsP) {
    int ncolors = *ncolorsP;
    int i, j;

    for (i = 0; i < ncolors; i++) {
        double t = (double)i / (ncolors - 1);
        int segment = (int)(t * (npoints - 1));
        double local_t = t * (npoints - 1) - segment;

        if (segment >= npoints - 1) {
            segment = npoints - 2;
            local_t = 1.0;
        }

        // Interpolate between two color points
        double h1 = h[segment], h2 = h[segment + 1];
        double s1 = s[segment], s2 = s[segment + 1];
        double v1 = v[segment], v2 = v[segment + 1];

        // Handle hue wrapping
        double dh = h2 - h1;
        if (dh > 180) dh -= 360;
        if (dh < -180) dh += 360;

        double interp_h = h1 + dh * local_t;
        double interp_s = s1 + (s2 - s1) * local_t;
        double interp_v = v1 + (v2 - v1) * local_t;

        // Convert to RGB
        double r, g, b;
        hsv_to_rgb(interp_h, interp_s, interp_v, &r, &g, &b);

        colors[i].red = (unsigned short)(r * 65535);
        colors[i].green = (unsigned short)(g * 65535);
        colors[i].blue = (unsigned short)(b * 65535);
        colors[i].flags = DoRed | DoGreen | DoBlue;
        
        if (i < 3) { // Log first 3 colors
            printf("[%ld] make_color_path_webgl: Color[%d]: RGB(%d, %d, %d) from HSV(%.1f, %.2f, %.2f)\n", 
                   (long)(emscripten_get_now()), i, colors[i].red, colors[i].green, colors[i].blue, interp_h, interp_s, interp_v);
        }
    }
}

// Missing fps function
void do_fps(ModeInfo *mi) {
    // Dummy implementation for web builds
    // FPS is handled by the web main loop
}

// Missing X11 function stubs for web build
int XSendEvent(Display *display, Window window, Bool propagate, long event_mask, XEvent *event) {
    // Web stub - events are handled differently in web environment
    (void)display;
    (void)window;
    (void)propagate;
    (void)event_mask;
    (void)event;
    return 1; // Success
}

// Missing function that screenhack.c uses
char *XGetAtomName(Display *display, Atom atom) {
    // Web stub - return a default name
    (void)display;
    (void)atom;
    return strdup("WEB_ATOM");
}

// Stub for get_float_resource
float get_float_resource(Display *dpy, char *res_name, char *res_class) {
    // Return default values for common resources
    (void)dpy;
    (void)res_class;

    if (strcmp(res_name, "speed") == 0) {
        return 1.0; // Default speed
    }
    if (strcmp(res_name, "thickness") == 0) {
        return 0.1; // Default thickness
    }
    if (strcmp(res_name, "spin") == 0) {
        return 1.0; // Default spin
    }
    if (strcmp(res_name, "wander") == 0) {
        return 1.0; // Default wander
    }
    return 1.0; // Default for unknown resources
}
