/* hextrail_web_main.c - Minimal Web Entry Point for HexTrail
 *
 * This provides the minimal web-specific code needed to run hextrail
 * using the generic xscreensaver_web wrapper.
 */

#include <emscripten.h>
#include "xscreensaver_web.c"
#include "hextrail.c"

// Web-specific main function
EMSCRIPTEN_KEEPALIVE
int main() {
    // Initialize the web wrapper with hextrail's functions
    return xscreensaver_web_init(
        init_hextrail,      // init function
        draw_hextrail,      // draw function  
        reshape_hextrail,   // reshape function
        free_hextrail       // free function
    );
} 