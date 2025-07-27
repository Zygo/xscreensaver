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
#include <stdarg.h>
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
#ifndef GL_FLAT
#define GL_FLAT 0x1D00
#endif
#ifndef GL_NORMALIZE
#define GL_NORMALIZE 0x0BA1
#endif

// Legacy OpenGL constants not available in WebGL 2.0
#ifndef GL_QUADS
#define GL_QUADS 0x0007
#endif
#ifndef GL_QUAD_STRIP
#define GL_QUAD_STRIP 0x0008
#endif
#ifndef GL_POLYGON
#define GL_POLYGON 0x0009
#endif

// Fixed-function OpenGL constants not available in WebGL 2.0
#ifndef GL_LIGHTING
#define GL_LIGHTING 0x0B50
#endif
#ifndef GL_LIGHT0
#define GL_LIGHT0 0x4000
#endif
#ifndef GL_LIGHT1
#define GL_LIGHT1 0x4001
#endif
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D 0x0DE1
#endif
#ifndef GL_FOG
#define GL_FOG 0x0B60
#endif
#ifndef GL_COLOR_MATERIAL
#define GL_COLOR_MATERIAL 0x0B57
#endif
#ifndef GL_CULL_FACE
#define GL_CULL_FACE 0x0B44
#endif
#ifndef GL_CCW
#define GL_CCW 0x0901
#endif
#ifndef GL_CW
#define GL_CW 0x0900
#endif

// Color generation constants
#define MAXPOINTS 10

#define Bool int
#define True 1
#define False 0

// GLdouble is missing in WebGL, provide it
#ifndef GLdouble
typedef double GLdouble;
#endif

// These constants should be available, but define them if missing
#ifndef GL_MODELVIEW_MATRIX
#define GL_MODELVIEW_MATRIX  0x0BA6
#endif
#ifndef GL_PROJECTION_MATRIX
#define GL_PROJECTION_MATRIX 0x0BA7
#endif

// Only provide gluProject if it's not working with the expected signature
// We'll name it differently to avoid conflicts
int gluProject_web(GLdouble objx, GLdouble objy, GLdouble objz,
                   const GLdouble modelMatrix[16], const GLdouble projMatrix[16],
                   const GLint viewport[4],
                   GLdouble *winx, GLdouble *winy, GLdouble *winz) {
    GLdouble in[4], out[4];

    // Transform by model matrix
    in[0] = objx; in[1] = objy; in[2] = objz; in[3] = 1.0;

    // Model transformation
    out[0] = modelMatrix[0]*in[0] + modelMatrix[4]*in[1] + modelMatrix[8]*in[2] + modelMatrix[12]*in[3];
    out[1] = modelMatrix[1]*in[0] + modelMatrix[5]*in[1] + modelMatrix[9]*in[2] + modelMatrix[13]*in[3];
    out[2] = modelMatrix[2]*in[0] + modelMatrix[6]*in[1] + modelMatrix[10]*in[2] + modelMatrix[14]*in[3];
    out[3] = modelMatrix[3]*in[0] + modelMatrix[7]*in[1] + modelMatrix[11]*in[2] + modelMatrix[15]*in[3];

    // Projection transformation
    in[0] = out[0]; in[1] = out[1]; in[2] = out[2]; in[3] = out[3];
    out[0] = projMatrix[0]*in[0] + projMatrix[4]*in[1] + projMatrix[8]*in[2] + projMatrix[12]*in[3];
    out[1] = projMatrix[1]*in[0] + projMatrix[5]*in[1] + projMatrix[9]*in[2] + projMatrix[13]*in[3];
    out[2] = projMatrix[2]*in[0] + projMatrix[6]*in[1] + projMatrix[10]*in[2] + projMatrix[14]*in[3];
    out[3] = projMatrix[3]*in[0] + projMatrix[7]*in[1] + projMatrix[11]*in[2] + projMatrix[15]*in[3];

    if (out[3] == 0.0) return 0; // GL_FALSE

    // Perspective divide and viewport transform
    out[0] /= out[3]; out[1] /= out[3]; out[2] /= out[3];
    *winx = viewport[0] + (1.0 + out[0]) * viewport[2] / 2.0;
    *winy = viewport[1] + (1.0 + out[1]) * viewport[3] / 2.0;
    *winz = (1.0 + out[2]) / 2.0;

    return 1; // GL_TRUE
}

// Debug logging control
static Bool debug_logging_enabled = True;
static int debug_level = 0; // Control debug output levels

// Function to re-enable debug logging
EMSCRIPTEN_KEEPALIVE
void re_enable_debug_logging() {
    debug_logging_enabled = True;
    printf("Debug logging re-enabled\n");
}

// Function to toggle global debug output (affects all printf output)
EMSCRIPTEN_KEEPALIVE
void set_global_debug_enabled(int enabled) {
    // Call JavaScript function to toggle global debug
    EM_ASM({
        if (window.setGlobalDebugEnabled) {
            window.setGlobalDebugEnabled($0);
        }
    }, enabled);

    // Also update our local debug flag
    debug_logging_enabled = enabled;

    printf("Global debug %s\n", enabled ? "enabled" : "disabled");
}

// Function to set debug level (0=errors only, 1=warnings+info, 2+=detailed debug)
EMSCRIPTEN_KEEPALIVE
void set_debug_level(int level) {
    debug_level = level;
    printf("Debug level set to %d\n", level);
}

// Debug function - level 0 goes to stderr, level 1+ goes to stdout
void DL(int level, const char* format, ...) {
    if (!debug_logging_enabled) return;
    if (level <= debug_level) {
        va_list args;
        va_start(args, format);
        if (level == 0) vfprintf(stderr, format, args);
        else vfprintf(stdout, format, args);
        va_end(args);
    }
}

// Helper function to handle GL_INVALID_ENUM errors
static void handle_1280_error(const char *location) {
    if (debug_logging_enabled) {
        printf("GL_INVALID_ENUM (1280) detected at %s - PAUSING debug logging\n", location);
        debug_logging_enabled = False;

        // Also disable global debug to stop all printf output
        set_global_debug_enabled(0);
    }
}

// Helper function to check for OpenGL errors and handle them consistently
static void check_gl_error_wrapper_internal(const char *location, int line) {
#ifdef FINDBUG_MODE
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error at %s (line %d): %d\n", location, line, error);
        if (error == 1280) {
            handle_1280_error(location);
        }
    }
#endif
}

// Macro that automatically includes the line number
#define check_gl_error_wrapper(location) check_gl_error_wrapper_internal(location, __LINE__)

// Provide glGetDoublev since WebGL only has glGetFloatv
void glGetDoublev_web(GLenum pname, GLdouble *params) {
    // Handle matrix parameters specially to avoid GL_INVALID_ENUM
    if (pname == GL_MODELVIEW_MATRIX || pname == GL_PROJECTION_MATRIX) {
        // Return identity matrix for now - this avoids the GL_INVALID_ENUM error
        // TODO: Later we can implement proper matrix stack retrieval
        for (int i = 0; i < 16; i++) {
            params[i] = (i % 5 == 0) ? 1.0 : 0.0; // Identity matrix
        }
        return;
    }

    // For non-matrix parameters, use glGetFloatv as before
    check_gl_error_wrapper("before glGetFloatv");
    GLfloat float_params[16];
    glGetFloatv(pname, float_params);
    check_gl_error_wrapper("after glGetFloatv");

    // Convert float to double
    int count = 1; // Default to 1 parameter for non-matrix values
    for (int i = 0; i < count; i++) {
        params[i] = (GLdouble)float_params[i];
    }
}

// Override the function names for hextrail.c when building for web
#define gluProject gluProject_web
#define glGetDoublev glGetDoublev_web

// Matrix stack management
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

static MatrixStack modelview_stack;
static MatrixStack projection_stack;
static MatrixStack texture_stack;
static GLenum current_matrix_mode = GL_MODELVIEW;
static ImmediateMode immediate;
static Color4f current_color = {1.0f, 1.0f, 1.0f, 1.0f};
static int total_vertices_this_frame = 0;
static Bool rendering_enabled = True;
static Normal3f current_normal = {0.0f, 0.0f, 1.0f};

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
static void init_gl_function_pointers(void);

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
    DL(1, "Created shader %u of type %d\n", shader, type);

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        DL(1, "Shader compilation error: %s\n", info_log);
        DL(1, "Shader source:\n%s\n", source);
    } else {
        DL(1, "Shader %u compiled successfully\n", shader);
    }

    return shader;
}

static void init_shaders() {
    DL(1, "Initializing WebGL 2.0 shaders...\n");

    const char *vertex_source =
        "#version 300 es\n"
        "in vec3 position;\n"
        "in vec4 color;\n"
        "uniform mat4 modelview;\n"
        "uniform mat4 projection;\n"
        "out vec4 frag_color;\n"
        "void main() {\n"
        "    gl_Position = projection * modelview * vec4(position, 1.0);\n"
        "    frag_color = color;\n"
        "}\n";

    const char *fragment_source =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec4 frag_color;\n"
        "out vec4 out_color;\n"
        "void main() {\n"
        "    out_color = frag_color;\n"
        "}\n";

    vertex_shader = compile_shader(vertex_source, GL_VERTEX_SHADER);
    fragment_shader = compile_shader(fragment_source, GL_FRAGMENT_SHADER);

    DL(1, "Vertex shader: %u, Fragment shader: %u\n", vertex_shader, fragment_shader);

    shader_program = glCreateProgram();
    DL(1, "Created shader program: %u\n", shader_program);

    if (shader_program == 0) {
        DL(0, "ERROR: glCreateProgram failed! Returned 0\n");
        return;
    }

    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);

    DL(1, "Attached shaders to program %u\n", shader_program);

    glLinkProgram(shader_program);

    GLint success;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetProgramInfoLog(shader_program, 512, NULL, info_log);
        DL(1, "Shader linking error: %s\n", info_log);
        DL(1, "Shader program %u linking failed!\n", shader_program);
    } else {
        DL(1, "Shader program %u linked successfully\n", shader_program);

        // Validate the program
        glValidateProgram(shader_program);
        GLint valid;
        glGetProgramiv(shader_program, GL_VALIDATE_STATUS, &valid);
        if (!valid) {
            GLchar info_log[512];
            glGetProgramInfoLog(shader_program, 512, NULL, info_log);
            DL(1, "Shader validation error: %s\n", info_log);
        } else {
            DL(1, "Shader program %u validated successfully\n", shader_program);
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
    long count;
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
typedef Bool (*handle_event_func)(ModeInfo *, XEvent *);

static init_func hack_init = NULL;
static draw_func hack_draw = NULL;
static reshape_func hack_reshape = NULL;
static free_func hack_free = NULL;
static handle_event_func hack_handle_event = NULL;

// Animation state (controlled by HexTrail's rotator system)
static int spin_enabled = 1;
static int wander_enabled = 1;

// Keypress state for web events
static char web_keypress_char = 0;
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

    check_gl_error_wrapper("start of main loop");

    // Don't reset the matrix stack - let it accumulate transformations from glTranslatef/glRotatef
    // The native code will call glLoadIdentity() when needed

    // Stop debug output after frame 240 (4 seconds)
    if (frame_count <= 240) {
        if (frame_count % 30 == 0) { // Log every 30 frames (once per second at 30 FPS)
            DL(1, "Main loop frame %d\n", frame_count);
        }
    }

    // Don't reset the matrix - let it persist like the native version
    // The native hextrail code uses glPushMatrix/glPopMatrix to manage transformations

    check_gl_error_wrapper("before glClear in main loop");
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    check_gl_error_wrapper("after glClear in main loop");

    if (hack_draw) {
        if (frame_count <= 240 && frame_count % 60 == 0) {
            DL(1, "Calling hack_draw...\n");
        }

        check_gl_error_wrapper("before hack_draw");
        hack_draw(&web_mi);
        check_gl_error_wrapper("after hack_draw");

        if (frame_count <= 240 && frame_count % 60 == 0) {
            DL(1, "hack_draw completed\n");
        }
    } else {
        if (frame_count <= 240 && frame_count % 60 == 0) {
            DL(1, "hack_draw is NULL!\n");
        }
    }
}

void glMatrixMode(GLenum mode) {
    // Check for errors before glMatrixMode
    GLenum before_matrix_error = glGetError();
    if (before_matrix_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error before glMatrixMode: %d\n", before_matrix_error);
    }

    current_matrix_mode = mode;
    DL(1, "glMatrixMode: %d\n", mode);

    // Check for errors after glMatrixMode
    GLenum after_matrix_error = glGetError();
    if (after_matrix_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error after glMatrixMode: %d\n", after_matrix_error);
        if (after_matrix_error == 1280) {
            handle_1280_error("after glMatrixMode");
        }
    }
}

void glOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val, GLfloat far_val) {
    // Check for errors before glOrtho
    GLenum before_ortho_error = glGetError();
    if (before_ortho_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error before glOrtho: %d\n", before_ortho_error);
        if (before_ortho_error == 1280) {
            handle_1280_error("before glOrtho");
        }
    }

    DL(1, "glOrtho: %.3f, %.3f, %.3f, %.3f, %.3f, %.3f\n", left, right, bottom, top, near_val, far_val);

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
        DL(1, "Orthographic matrix applied\n");
    }

    // Check for errors after glOrtho
    GLenum after_ortho_error = glGetError();
    if (after_ortho_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error after glOrtho: %d\n", after_ortho_error);
        if (after_ortho_error == 1280) {
            handle_1280_error("after glOrtho");
        }
    }
}

void glLoadIdentity(void) {
    // Check for errors before glLoadIdentity
    GLenum before_identity_error = glGetError();
    if (before_identity_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error before glLoadIdentity: %d\n", before_identity_error);
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

    matrix_identity(&stack->stack[stack->top]);

    // Debug: Log when modelview matrix is reset
    if (current_matrix_mode == GL_MODELVIEW) {
        static int reset_count = 0;
        reset_count++;
        if (reset_count <= 3) {
            DL(2, "DEBUG: ModelView matrix reset to identity (count: %d)\n", reset_count);
        }
    }

    // Check for errors after glLoadIdentity
    GLenum after_identity_error = glGetError();
    if (after_identity_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error after glLoadIdentity: %d\n", after_identity_error);
    }
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
        DL(1, "Failed to create WebGL context! Error: %lu\n", webgl_context);
        return 0;
    }

    if (emscripten_webgl_make_context_current(webgl_context) != EMSCRIPTEN_RESULT_SUCCESS) {
        DL(1, "Failed to make WebGL context current!\n");
        return 0;
    }

    // Initialize function pointers to real WebGL functions
    init_gl_function_pointers();

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
    // Check for any existing errors at the start of init_opengl_state
    GLenum init_start_error = glGetError();
    if (init_start_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error at start of init_opengl_state: %d\n", init_start_error);
        if (init_start_error == 1280) {
            handle_1280_error("start of init_opengl_state");
        }
    }

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

    // Check for errors after glEnable
    GLenum state_error = glGetError();
    if (state_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error after glEnable(GL_DEPTH_TEST): %d\n", state_error);
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Check for errors after glClearColor
    state_error = glGetError();
    if (state_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error after glClearColor: %d\n", state_error);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Check for errors after glClear
    state_error = glGetError();
    if (state_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error after glClear: %d\n", state_error);
    }
}

// Generic web initialization
EMSCRIPTEN_KEEPALIVE
int xscreensaver_web_init(init_func init, draw_func draw, reshape_func reshape, free_func free, handle_event_func handle_event) {
    DL(1, "xscreensaver_web_init called\n");

    // Initialize random seed for consistent but varied colors
    extern void ya_rand_init(unsigned int seed);
    ya_rand_init(0);
    DL(1, "Random seed initialized\n");

    hack_init = init;
    hack_draw = draw;
    hack_reshape = reshape;
    hack_free = free;
    hack_handle_event = handle_event;

    DL(1, "Function pointers set: init=%p, draw=%p, reshape=%p, free=%p, handle_event=%p\n",
           (void*)init, (void*)draw, (void*)reshape, (void*)free, (void*)handle_event);

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

    DL(1, "ModeInfo initialized: width=%d, height=%d\n", web_mi.width, web_mi.height);

    // Check canvas size
    int canvas_width, canvas_height;
    emscripten_get_canvas_element_size("#canvas", &canvas_width, &canvas_height);
    DL(1, "Canvas size: %dx%d\n", canvas_width, canvas_height);

    // Initialize WebGL
    DL(1, "Initializing WebGL...\n");
    if (!init_webgl()) {
        DL(1, "WebGL initialization failed!\n");
        return 0;
    }
    DL(1, "WebGL initialized successfully\n");

    // Call the hack's init function
    if (hack_init) {
        DL(1, "Calling hack_init...\n");
        hack_init(&web_mi);
        DL(1, "hack_init completed\n");
    } else {
        DL(1, "hack_init is NULL!\n");
    }

    // Rotator will be initialized by webpage with checkbox values
    DL(2, "DEBUG: Web rotator will be initialized by webpage\n");

    // Set up reshape
    if (hack_reshape) {
        DL(1, "Calling hack_reshape...\n");
        hack_reshape(&web_mi, web_mi.width, web_mi.height);
        DL(1, "hack_reshape completed\n");
    } else {
        DL(1, "hack_reshape is NULL!\n");
    }

    // Set up the main loop (30 FPS - matches hextrail.c timing better than 60 FPS)
    DL(1, "Setting up main loop...\n");
    emscripten_set_main_loop(main_loop, 30, 1);
    DL(1, "Main loop set up successfully\n");

    return 1;
}

// Web-specific function exports for UI controls
EMSCRIPTEN_KEEPALIVE
void set_speed(GLfloat new_speed) {
    extern GLfloat speed;
    speed = new_speed;
    DL(1, "Animation speed set to: %f\n", speed);
}

EMSCRIPTEN_KEEPALIVE
void set_thickness(GLfloat new_thickness) {
    extern GLfloat thickness;
    thickness = new_thickness;
    DL(1, "Thickness set to: %f\n", thickness);

    // Send event to hack to update corners
    if (hack_handle_event) {
        // Create a dummy event to trigger corner recalculation
        // The hack can check if thickness changed and call scale_corners()
        hack_handle_event(&web_mi, NULL);  // NULL event means "parameter changed"
    }
}

EMSCRIPTEN_KEEPALIVE
void set_spin(int new_spin_enabled) {
    extern Bool do_spin;
    extern void update_hextrail_rotator(void);
    DL(2, "DEBUG: set_spin called with %d, current do_spin=%d\n", new_spin_enabled, do_spin);
    do_spin = new_spin_enabled;
    DL(1, "Spin %s (do_spin now=%d)\n", do_spin ? "enabled" : "disabled", do_spin);
    update_hextrail_rotator();
}

EMSCRIPTEN_KEEPALIVE
void set_wander(int new_wander_enabled) {
    extern Bool do_wander;
    extern void update_hextrail_rotator(void);
    DL(2, "DEBUG: set_wander called with %d, current do_wander=%d\n", new_wander_enabled, do_wander);
    do_wander = new_wander_enabled;
    DL(1, "Wander %s (do_wander now=%d)\n", do_wander ? "enabled" : "disabled", do_wander);
    update_hextrail_rotator();
}

EMSCRIPTEN_KEEPALIVE
void stop_rendering() {
    rendering_enabled = False;
    DL(1, "Rendering stopped\n");
}

EMSCRIPTEN_KEEPALIVE
void start_rendering() {
    rendering_enabled = True;
    DL(1, "Rendering started\n");
}

EMSCRIPTEN_KEEPALIVE
void handle_mouse_drag(int delta_x, int delta_y) {
    // This would need to be implemented per-hack
    DL(1, "Mouse drag: %d, %d\n", delta_x, delta_y);
}

EMSCRIPTEN_KEEPALIVE
void handle_mouse_wheel(int delta) {
    // This would need to be implemented per-hack
    DL(1, "Mouse wheel: %d\n", delta);
}

EMSCRIPTEN_KEEPALIVE
void handle_keypress(int keycode, int charcode) {
    if (hack_handle_event) {
        // Create a dummy XEvent structure for keyboard events
        XEvent event;
        memset(&event, 0, sizeof(event));
        event.type = KeyPress;
        event.xkey.keycode = keycode;
        event.xkey.state = 0;  // No modifiers for now

        // Store the character for XLookupString to return
        web_keypress_char = (char)charcode;

        // Pass the event to the hack
        hack_handle_event(&web_mi, &event);
        DL(1, "Keypress: keycode=%d, char=%c\n", keycode, (char)charcode);
    }
}

EMSCRIPTEN_KEEPALIVE
void reshape_hextrail_wrapper(int width, int height) {
    if (hack_reshape) {
        web_mi.width = width;
        web_mi.height = height;
        hack_reshape(&web_mi, width, height);
        DL(1, "Reshaped to %dx%d\n", width, height);
    }
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

// Missing X11 function for web builds
int XLookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer, KeySym *keysym_return, XComposeStatus *status_in_out) {
    // Return the character stored by handle_keypress
    if (buffer_return && bytes_buffer > 0) {
        buffer_return[0] = web_keypress_char;
        if (bytes_buffer > 1) buffer_return[1] = '\0';
        return web_keypress_char ? 1 : 0;
    }
    if (keysym_return) *keysym_return = 0;
    return 0;
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

    check_gl_error_wrapper("before glMultMatrixd in gluLookAt");
    glMultMatrixd(m);
    check_gl_error_wrapper("after glMultMatrixd in gluLookAt");

    /* Translate Eye to Origin */
    glTranslated(-eyex, -eyey, -eyez);
    check_gl_error_wrapper("after glTranslated in gluLookAt");
}


// Function pointers to the real OpenGL functions
void (*glEnable_real)(GLenum) = NULL;
void (*glDisable_real)(GLenum) = NULL;
void (*glClear_real)(GLbitfield) = NULL;
// Note: glShadeModel and glFrontFace don't exist in WebGL 2.0, so no function pointers needed

// Initialize function pointers to real WebGL functions
static void init_gl_function_pointers(void) {
    // Get function pointers to the real WebGL functions
    glEnable_real = (void (*)(GLenum))emscripten_webgl_get_proc_address("glEnable");
    glDisable_real = (void (*)(GLenum))emscripten_webgl_get_proc_address("glDisable");
    glClear_real = (void (*)(GLbitfield))emscripten_webgl_get_proc_address("glClear");
    // Note: glShadeModel and glFrontFace don't exist in WebGL 2.0, so we don't try to get them

    if (!glEnable_real) {
        DL(1, "WARNING: Could not get glEnable function pointer\n");
    }
    if (!glDisable_real) {
        DL(1, "WARNING: Could not get glDisable function pointer\n");
    }
    if (!glClear_real) {
        DL(1, "WARNING: Could not get glClear function pointer\n");
    }
}

// OpenGL state tracking
static Bool normalize_enabled = False;
static Bool lighting_enabled = False;

// Add glEnable wrapper that handles unsupported capabilities in WebGL 2.0
void glEnable(GLenum cap) {
    // Check for errors before glEnable
    GLenum before_error = glGetError();
    if (before_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error before glEnable(%d): %d\n", cap, before_error);
        if (before_error == 1280) {
            handle_1280_error("before glEnable");
        }
    }

    // Check for unsupported capabilities in WebGL 2.0
    switch (cap) {
        case GL_NORMALIZE:
            // Store normalize state for shader use
            normalize_enabled = True;
            DL(2, "DEBUG: glEnable(GL_NORMALIZE=%d 0x%x) - will normalize normals in shader\n", cap, cap);
            return;
        case GL_LIGHTING:
            // Store lighting state for shader use
            lighting_enabled = True;
            DL(2, "DEBUG: glEnable(GL_LIGHTING) - will apply lighting in shader\n");
            return;
        case GL_LIGHT0:
        case GL_LIGHT1:
            // Fixed-function lighting is not supported in WebGL 2.0, ignore it
            DL(1, "WARNING: glEnable(GL_LIGHT%d) ignored - not supported in WebGL 2.0\n", cap - GL_LIGHT0);
            return;
        case GL_TEXTURE_2D:
            // Fixed-function texturing is not supported in WebGL 2.0, ignore it
            DL(1, "WARNING: glEnable(GL_TEXTURE_2D) ignored - not supported in WebGL 2.0\n");
            return;
        case GL_FOG:
            // Fixed-function fog is not supported in WebGL 2.0, ignore it
            DL(1, "WARNING: glEnable(GL_FOG) ignored - not supported in WebGL 2.0\n");
            return;
        case GL_COLOR_MATERIAL:
            // Fixed-function materials are not supported in WebGL 2.0, ignore it
            DL(1, "WARNING: glEnable(GL_COLOR_MATERIAL) ignored - not supported in WebGL 2.0\n");
            return;
        default:
            DL(2, "DEBUG: glEnable default case for cap=%d (0x%x), GL_NORMALIZE=%d (0x%x)\n", cap, cap, GL_NORMALIZE, GL_NORMALIZE);
            // For supported capabilities, call the real glEnable
            if (glEnable_real) {
                glEnable_real(cap);
            } else {
                DL(1, "WARNING: glEnable(%d) ignored - real function not available\n", cap);
            }
            break;
    }

    check_gl_error_wrapper("after glEnable");
}

// Add glDisable wrapper that handles unsupported capabilities in WebGL 2.0
void glDisable(GLenum cap) {
    check_gl_error_wrapper("start of glDisable");

    // Check for unsupported capabilities in WebGL 2.0
    switch (cap) {
        case GL_NORMALIZE:
            // Store normalize state for shader use
            normalize_enabled = False;
            DL(2, "DEBUG: glDisable(GL_NORMALIZE) - will not normalize normals in shader\n");
            return;
        case GL_LIGHTING:
            // Store lighting state for shader use
            lighting_enabled = False;
            DL(2, "DEBUG: glDisable(GL_LIGHTING) - will not apply lighting in shader\n");
            return;
        case GL_LIGHT0:
        case GL_LIGHT1:
            // Fixed-function lighting is not supported in WebGL 2.0, ignore it
            DL(1, "WARNING: glDisable(GL_LIGHT%d) ignored - not supported in WebGL 2.0\n", cap - GL_LIGHT0);
            return;
        case GL_TEXTURE_2D:
            // Fixed-function texturing is not supported in WebGL 2.0, ignore it
            DL(1, "WARNING: glDisable(GL_TEXTURE_2D) ignored - not supported in WebGL 2.0\n");
            return;
        case GL_FOG:
            // Fixed-function fog is not supported in WebGL 2.0, ignore it
            DL(1, "WARNING: glDisable(GL_FOG) ignored - not supported in WebGL 2.0\n");
            return;
        case GL_COLOR_MATERIAL:
            // Fixed-function materials are not supported in WebGL 2.0, ignore it
            DL(1, "WARNING: glDisable(GL_COLOR_MATERIAL) ignored - not supported in WebGL 2.0\n");
            return;
        case GL_DEPTH_TEST:
            DL(2, "DEBUG: glDisable(GL_DEPTH_TEST), glDisable_real=%p\n", glDisable_real);
            // fallthrough
        case GL_CULL_FACE:
            DL(2, "DEBUG: glDisable(GL_CULL_FACE), glDisable_real=%p\n", glDisable_real);
            // fallthrough
        default:
            // For supported capabilities, call the real glDisable
            if (glDisable_real) {
                DL(2, "DEBUG: glDisable(%d) - calling real function\n", cap);
                glDisable_real(cap);
            } else {
                DL(1, "WARNING: glDisable(%d) ignored - real function not available\n", cap);
            }
            break;
    }

    check_gl_error_wrapper("end of glDisable");
}

void glClear(GLbitfield mask) {
    check_gl_error_wrapper("before glClear wrapper");

    // Call the real glClear function
    if (glClear_real) {
        glClear_real(mask);
        check_gl_error_wrapper("after glClear_real");
    } else {
        DL(1, "WARNING: glClear(%d) ignored - real function not available\n", mask);
    }
}

// Shading model state
static GLenum current_shade_model = GL_SMOOTH; // Default to smooth

void glShadeModel(GLenum mode) {
    check_gl_error_wrapper("before glShadeModel");

    // Store the shading mode for shader use
    current_shade_model = mode;
    DL(2, "DEBUG: glShadeModel set to %d (GL_SMOOTH=%d, GL_FLAT=%d)\n", mode, GL_SMOOTH, GL_FLAT);

    // For WebGL 2.0, we'll handle this in our shaders
    // GL_SMOOTH = interpolate colors between vertices
    // GL_FLAT = use the last vertex color for the entire primitive
}

// Front face winding order state
static GLenum current_front_face = GL_CCW; // Default to CCW

// Add glFrontFace wrapper
void glFrontFace(GLenum mode) {
    check_gl_error_wrapper("before glFrontFace");

    // Store the front face mode for shader use
    current_front_face = mode;
    DL(2, "DEBUG: glFrontFace set to %d (GL_CCW=%d, GL_CW=%d)\n", mode, GL_CCW, GL_CW);

    // For WebGL 2.0, we'll handle this in our shaders using gl_FrontFacing
    // The real glFrontFace function is not available in WebGL 2.0
}

void glPushMatrix(void) {
    check_gl_error_wrapper("before glPushMatrix");

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
        DL(1, "glColor3f: RGB(%.3f, %.3f, %.3f)\n", r, g, b);
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
        DL(1, "glColor4f: RGBA(%.3f, %.3f, %.3f, %.3f)\n", r, g, b, a);
    }
}

void glColor4fv(const GLfloat *v) {
    check_gl_error_wrapper("before glColor4fv");

    current_color.r = v[0];
    current_color.g = v[1];
    current_color.b = v[2];
    current_color.a = v[3];

    static int color_count = 0;
    color_count++;
    if (color_count <= 10) { // Only log first 10 color changes
        DL(1, "glColor4fv: RGBA(%.3f, %.3f, %.3f, %.3f)\n", v[0], v[1], v[2], v[3]);
    }
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
    // Store material properties for shader uniforms
    // For now, just ignore - we'll implement basic lighting later
}

void glLightfv(GLenum light, GLenum pname, const GLfloat *params) {
    // Store lighting properties for shader uniforms
    // For now, just ignore - we'll implement basic lighting later
    DL(2, "DEBUG: glLightfv(light=%d, pname=%d) - lighting not implemented yet\n", light, pname);
}

void glLightf(GLenum light, GLenum pname, GLfloat param) {
    // Store lighting properties for shader uniforms
    // For now, just ignore - we'll implement basic lighting later
    DL(2, "DEBUG: glLightf(light=%d, pname=%d, param=%f) - lighting not implemented yet\n", light, pname, param);
}

void glLightModelfv(GLenum pname, const GLfloat *params) {
    // Store lighting model properties for shader uniforms
    // For now, just ignore - we'll implement basic lighting later
    DL(2, "DEBUG: glLightModelfv(pname=%d) - lighting model not implemented yet\n", pname);
}

void glFinish(void) {
    // In WebGL 2.0, glFinish() is not needed as the browser handles synchronization
    // But we'll call it for compatibility
    DL(2, "DEBUG: glFinish() called - WebGL handles synchronization automatically\n");
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    if (!immediate.in_begin_end) {
        return;
    }

    if (immediate.vertex_count >= MAX_VERTICES) {
        DL(1, "WARNING: Vertex limit reached (%d), dropping vertex!\n", MAX_VERTICES);
        return;
    }

    // Check for errors before glVertex3f (only on first few vertices to avoid spam)
    static int vertex_error_check_count = 0;
    if (vertex_error_check_count < 5) {
        check_gl_error_wrapper("before glVertex3f");
        vertex_error_check_count++;
    }

    immediate.vertices[immediate.vertex_count].x = x;
    immediate.vertices[immediate.vertex_count].y = y;
    immediate.vertices[immediate.vertex_count].z = z;
    immediate.colors[immediate.vertex_count] = current_color;
    immediate.normals[immediate.vertex_count] = current_normal;
    immediate.vertex_count++;
}

void glBegin(GLenum mode) {
    check_gl_error_wrapper("before glBegin");

    immediate.in_begin_end = True;
    immediate.primitive_type = mode;
    immediate.vertex_count = 0;

    static int glBegin_count = 0;
    glBegin_count++;
    if (glBegin_count <= 5) {
        DL(2, "DEBUG: glBegin called with mode=%d (GL_TRIANGLES=%d, GL_LINES=%d)\n",
               mode, GL_TRIANGLES, GL_LINES);
    }
}

void glEnd(void) {
    if (!immediate.in_begin_end || immediate.vertex_count == 0) {
        immediate.in_begin_end = False;
        return;
    }

    static int glEnd_count = 0;
    glEnd_count++;
    if (glEnd_count <= 5) { // Only log first 5 glEnd calls
        DL(1, "glEnd #%d: Drawing %d vertices with primitive type %d\n", glEnd_count, immediate.vertex_count, immediate.primitive_type);
    }

    check_gl_error_wrapper("start of glEnd");

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

    // Use our WebGL 2.0 wrapper (limit messages to first 5 frames)
    static int webgl_wrapper_count = 0;
    if (webgl_wrapper_count < 5) {
        DL(1, "Using WebGL 2.0 wrapper...\n");
        webgl_wrapper_count++;
    }

    // Use the pre-compiled shader program
    if (shader_program == 0) {
        DL(0, "ERROR: shader_program is 0, cannot render!\n");
        return;
    }

    // Check WebGL context state
    GLenum context_error = glGetError();
    if (context_error != GL_NO_ERROR) {
        DL(1, "WARNING: WebGL context error before glUseProgram: %d\n", context_error);
    }

    glUseProgram(shader_program);

    // Set up shader uniforms for legacy OpenGL state
    GLint normalize_loc = glGetUniformLocation(shader_program, "normalize_enabled");
    if (normalize_loc != -1) {
        glUniform1i(normalize_loc, normalize_enabled ? 1 : 0);
    }

    GLint smooth_shading_loc = glGetUniformLocation(shader_program, "smooth_shading");
    if (smooth_shading_loc != -1) {
        glUniform1i(smooth_shading_loc, (current_shade_model == GL_SMOOTH) ? 1 : 0);
    }

    GLint front_face_ccw_loc = glGetUniformLocation(shader_program, "front_face_ccw");
    if (front_face_ccw_loc != -1) {
        glUniform1i(front_face_ccw_loc, (current_front_face == GL_CCW) ? 1 : 0);
    }

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

        // Check for errors after projection matrix
        GLenum uniform_error = glGetError();
        if (uniform_error != GL_NO_ERROR) {
            DL(0, "ERROR: WebGL error after projection matrix: %d\n", uniform_error);
        }
    }

    // Set up modelview matrix - apply transformations directly
    GLint modelview_loc = glGetUniformLocation(shader_program, "modelview");
    if (modelview_loc != -1) {
        // Start with identity matrix
        GLfloat modelview[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        // Apply scale factor of 3 (native version uses 18)
        GLfloat scale = 3.0f;
        modelview[0] = scale;
        modelview[5] = scale;
        modelview[10] = scale;

        // Apply current frame's transformations from the rotator
        // We'll get these values from the debug output in hextrail.c
        // Note: We can't access hextrail internals from here due to circular dependencies
        // For now, use a simple approach without direct rotator access

        // Try to use the matrix stack, but fall back to hardcoded if it's broken
        GLfloat stack_matrix[16];
        memcpy(stack_matrix, modelview_stack.stack[modelview_stack.top].m, 16 * sizeof(GLfloat));

        // Debug: Print raw matrix values to see what's in the stack
        static int raw_matrix_debug_count = 0;
        if (raw_matrix_debug_count < 2) {
            DL(2, "DEBUG: Raw matrix stack (frame %d): trans=(%.3f,%.3f,%.3f), scale=(%.3f,%.3f,%.3f)\n",
                   raw_matrix_debug_count, stack_matrix[12], stack_matrix[13], stack_matrix[14],
                   stack_matrix[0], stack_matrix[5], stack_matrix[10]);
            raw_matrix_debug_count++;
        }

        // Scale down massive translations while keeping rotations and scales
        float translation_scale = 0.1f;
        stack_matrix[12] *= translation_scale;  // X translation
        stack_matrix[13] *= translation_scale;  // Y translation
        stack_matrix[14] *= translation_scale;  // Z translation

        // Check if the scaled matrix has reasonable values
        if (fabs(stack_matrix[12]) < 10.0f && fabs(stack_matrix[13]) < 10.0f && fabs(stack_matrix[14]) < 10.0f) {
            // Use the scaled matrix stack (real transformations)
            glUniformMatrix4fv(modelview_loc, 1, GL_FALSE, stack_matrix);

            static int stack_debug_count = 0;
            if (stack_debug_count < 3) {
                DL(2, "DEBUG: Using scaled matrix stack (frame %d): trans=(%.3f,%.3f,%.3f)\n",
                       stack_debug_count, stack_matrix[12], stack_matrix[13], stack_matrix[14]);
                stack_debug_count++;
            }
        } else {
            // Fall back to hardcoded matrix (safety net)
            glUniformMatrix4fv(modelview_loc, 1, GL_FALSE, modelview);

            static int fallback_debug_count = 0;
            if (fallback_debug_count < 3) {
                DL(2, "DEBUG: Using fallback matrix (frame %d): scale=%.1f\n", fallback_debug_count, scale);
                fallback_debug_count++;
            }
        }

        // Check for errors after modelview matrix
        GLenum matrix_error = glGetError();
        if (matrix_error != GL_NO_ERROR) {
            DL(0, "ERROR: WebGL error after modelview matrix: %d\n", matrix_error);
        }

        // Debug: Print the applied transformations
        static int transform_debug_count = 0;
        if (transform_debug_count < 3) {
            DL(2, "DEBUG: Direct Transformations (frame %d): scale=%.1f\n",
                   transform_debug_count, scale);
            transform_debug_count++;
        }
    }

    // Create and bind VBOs
    glGenBuffers(1, &vbo_vertices);
    glGenBuffers(1, &vbo_colors);

    DL(2, "DEBUG: Created VBOs: vertices=%u, colors=%u\n", vbo_vertices, vbo_colors);

    // Check for errors after VBO creation
    GLenum vbo_error = glGetError();
    if (vbo_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error after VBO creation: %d\n", vbo_error);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
    glBufferData(GL_ARRAY_BUFFER, immediate.vertex_count * sizeof(Vertex3f), immediate.vertices, GL_STATIC_DRAW);

    // Check for errors after vertex VBO setup
    vbo_error = glGetError();
    if (vbo_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error after vertex VBO setup: %d\n", vbo_error);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_colors);
    glBufferData(GL_ARRAY_BUFFER, immediate.vertex_count * sizeof(Color4f), immediate.colors, GL_STATIC_DRAW);

    // Check for errors after color VBO setup
    vbo_error = glGetError();
    if (vbo_error != GL_NO_ERROR) {
        DL(0, "ERROR: WebGL error after color VBO setup: %d\n", vbo_error);
    }

    // Set up vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
    GLint pos_attrib = glGetAttribLocation(shader_program, "position");
    if (pos_attrib != -1) {
        glEnableVertexAttribArray(pos_attrib);
        glVertexAttribPointer(pos_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
        DL(2, "DEBUG: Position attribute: location=%d, VBO=%u\n", pos_attrib, vbo_vertices);
    } else {
        DL(0, "ERROR: Could not find 'position' attribute in shader!\n");
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_colors);
    GLint color_attrib = glGetAttribLocation(shader_program, "color");
    if (color_attrib != -1) {
        glEnableVertexAttribArray(color_attrib);
        glVertexAttribPointer(color_attrib, 4, GL_FLOAT, GL_FALSE, 0, 0);
        DL(2, "DEBUG: Color attribute: location=%d, VBO=%u\n", color_attrib, vbo_colors);
    } else {
        DL(0, "ERROR: Could not find 'color' attribute in shader!\n");
    }

    // Note: Normal attribute not used in current shader - removed to eliminate error
    // If lighting is needed in the future, add "in vec3 normal;" to vertex shader

    // Draw
    static int draw_debug_count = 0;
    if (draw_debug_count < 5) {
        DL(2, "DEBUG: glDrawArrays called with primitive_type=%d, vertex_count=%d\n",
               immediate.primitive_type, immediate.vertex_count);
        draw_debug_count++;
    }

    // Validate primitive type for WebGL 2.0
    GLenum valid_primitive = immediate.primitive_type;

    // Check if primitive type is 0 (invalid)
    if (immediate.primitive_type == 0) {
        DL(0, "ERROR: Primitive type is 0 (invalid), using GL_TRIANGLES\n");
        valid_primitive = GL_TRIANGLES;
    } else {
        switch (immediate.primitive_type) {
            case GL_POINTS:
            case GL_LINE_STRIP:
            case GL_LINE_LOOP:
            case GL_LINES:
            case GL_TRIANGLE_STRIP:
            case GL_TRIANGLE_FAN:
            case GL_TRIANGLES:
                // These are valid in WebGL 2.0
                break;
            case GL_QUADS:
            case GL_QUAD_STRIP:
            case GL_POLYGON:
                // These are not supported in WebGL 2.0, convert to triangles
                DL(1, "WARNING: Converting unsupported primitive type %d to GL_TRIANGLES\n", immediate.primitive_type);
                valid_primitive = GL_TRIANGLES;
                break;
            default:
                DL(0, "ERROR: Unknown primitive type %d, using GL_TRIANGLES\n", immediate.primitive_type);
                valid_primitive = GL_TRIANGLES;
                break;
        }
    }

    // Check for zero vertex count
    if (immediate.vertex_count == 0) {
        DL(1, "WARNING: glDrawArrays called with 0 vertices, skipping draw\n");
    } else {
        // Ensure we have a valid VBO bound for drawing
        glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
        glDrawArrays(valid_primitive, 0, immediate.vertex_count);
    }

    // Limit completion message to first 5 frames
    static int webgl_complete_count = 0;
    if (webgl_complete_count < 5) {
        DL(1, "WebGL 2.0 wrapper rendering completed\n");
        webgl_complete_count++;
    }

    // Check for WebGL errors after drawing (limit to first 5 frames)
    static int webgl_error_count = 0;
    GLenum error = glGetError();
    if (error != GL_NO_ERROR && webgl_error_count < 5) {
        DL(1, "WebGL error after glDrawArrays: %d (0x%x) - IGNORING FOR NOW\n", error, error);
        if (error == 1280) {
            DL(1, "  GL_INVALID_ENUM - but rendering might still work\n");
        }
        webgl_error_count++;
    }

    // Disable vertex attributes before cleanup
    if (pos_attrib != -1) {
        glDisableVertexAttribArray(pos_attrib);
    }
    if (color_attrib != -1) {
        glDisableVertexAttribArray(color_attrib);
    }
    // Normal attribute cleanup removed - not used in current shader

    // Cleanup
    glDeleteBuffers(1, &vbo_vertices);
    glDeleteBuffers(1, &vbo_colors);
    // vbo_normals cleanup removed - not used in current shader

    immediate.in_begin_end = False;
}

// Missing GLX function
void glXMakeCurrent(Display *display, Window window, GLXContext context) {
    // This is handled by our WebGL context management
    // No-op for web builds
}

// Missing OpenGL functions for WebGL compatibility
void glFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val, GLfloat far_val) {
    check_gl_error_wrapper("before glFrustum");

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

    check_gl_error_wrapper("after glFrustum");
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

    check_gl_error_wrapper("after glMultMatrixd");
}

void glTranslated(GLfloat x, GLfloat y, GLfloat z) {
    glTranslatef(x, y, z);

    check_gl_error_wrapper("after glTranslated");
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

    check_gl_error_wrapper("after glMultMatrixf");
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
        DL(1, "[%ld] xcolor_to_glfloat: XColor(%d,%d,%d) -> GLfloat(%.3f,%.3f,%.3f,%.3f)\n",
               (long)(emscripten_get_now()), xcolor->red, xcolor->green, xcolor->blue, rgba[0], rgba[1], rgba[2], rgba[3]);
    }
}

// Generate smooth color map for WebGL
void make_smooth_colormap_webgl(XColor *colors, int *ncolorsP, Bool allocate_p, Bool *writable_pP, Bool verbose_p) {
    DL(1, "make_smooth_colormap_webgl called: allocate_p=%d, verbose_p=%d\n", allocate_p, verbose_p);
    DL(1, "make_smooth_colormap_webgl: ncolorsP=%d before call\n", *ncolorsP);
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
            DL(1, "[%ld] make_color_path_webgl: Color[%d]: RGB(%d, %d, %d) from HSV(%.1f, %.2f, %.2f)\n",
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
