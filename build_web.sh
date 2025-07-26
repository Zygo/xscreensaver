#!/bin/bash

# HexTrail Web Build Script
# This script compiles hextrail for the web using emscripten

set -e

# Get the repository root directory (same directory as this script)
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source emsdk environment (adjust path as needed)
if [ -f "$HOME/src/emsdk/emsdk_env.sh" ]; then
    source "$HOME/src/emsdk/emsdk_env.sh"
elif [ -f "$REPO_ROOT/emsdk/emsdk_env.sh" ]; then
    source "$REPO_ROOT/emsdk/emsdk_env.sh"
else
    echo "Warning: emsdk_env.sh not found. Make sure emscripten is in your PATH."
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

UTILS_DIR="$REPO_ROOT/utils"
GLX_DIR="$REPO_ROOT/hacks/glx"
HACKS_DIR="$REPO_ROOT/hacks"
JWXYZ_DIR="$REPO_ROOT/jwxyz"


echo -e "${BLUE}🚀 Building HexTrail for Web with Emscripten${NC}"
echo -e "${YELLOW}📁 Repository root: $REPO_ROOT${NC}"
echo -e "${YELLOW}📁 Utils directory: $UTILS_DIR${NC}"
echo -e "${YELLOW}📁 GLX directory: $GLX_DIR${NC}"
echo -e "${YELLOW}📁 JWXYZ directory: $JWXYZ_DIR${NC}"

# Check if emscripten is available
if ! command -v emcc &> /dev/null; then
    echo -e "${RED}❌ Emscripten not found. Please install emscripten first.${NC}"
    echo -e "${YELLOW}💡 You can install it with: git clone https://github.com/emscripten-core/emsdk.git && ./emsdk install latest && ./emsdk activate latest${NC}"
    exit 1
fi

# Check if we're in the right directory
if [ ! -f "hacks/glx/hextrail_web_main.c" ]; then
    echo -e "${RED}❌ hextrail_web_main.c not found. Please run this script from the project root directory.${NC}"
    exit 1
fi

# Create build directory
mkdir -p build_web
cd build_web

echo -e "${YELLOW}📦 Compiling HexTrail...${NC}"

# Compile with emscripten using custom HTML template
emcc \
    -DSTANDALONE \
    -DUSE_GL \
    -DHAVE_CONFIG_H \
    -DWEB_BUILD \
    -DHAVE_JWXYZ \
    -s USE_WEBGL2=1 \
    -s FULL_ES3=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap'] \
    -s EXPORTED_FUNCTIONS=['_main','_init_hextrail','_draw_hextrail','_reshape_hextrail_wrapper','_free_hextrail','_set_speed','_set_thickness','_set_spin','_set_wander','_stop_rendering','_start_rendering','_handle_mouse_drag','_handle_mouse_wheel','_handle_keypress'] \
    -s MIN_WEBGL_VERSION=2 \
    -O3 \
    -I$JWXYZ_DIR \
    -I. \
    -I$REPO_ROOT \
    -I$HACKS_DIR \
    -I$UTILS_DIR \
    -I$GLX_DIR \
    $GLX_DIR/hextrail_web_main.c \
    $UTILS_DIR/colors.c \
    $UTILS_DIR/yarandom.c \
    $UTILS_DIR/usleep.c \
    $HACKS_DIR/screenhack.c \
    $GLX_DIR/rotator.c \
    $GLX_DIR/gltrackball.c \
    $GLX_DIR/normals.c \
    $JWXYZ_DIR/jwxyz-timers.c \
    -o index.html \
    --shell-file $REPO_ROOT/web/template.html

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Build successful!${NC}"
    echo -e "${BLUE}📁 Output files:${NC}"
    echo -e "   - index.html (main HTML file)"
    echo -e "   - hextrail_web.js (JavaScript module)"
    echo -e "   - hextrail_web.wasm (WebAssembly binary)"

    echo -e "${YELLOW}🌐 To run locally:${NC}"
    echo -e "   cd build_web"
    echo -e "   python3 -m http.server 8000"
    echo -e "   Then open http://localhost:8000"

    echo -e "${GREEN}📋 Files in build_web directory${NC}"

else
    echo -e "${RED}❌ Build failed!${NC}"
    exit 1
fi
