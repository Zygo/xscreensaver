#!/bin/bash

# HexTrail Web Build Script
# This script compiles hextrail for the web using emscripten

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🚀 Building HexTrail for Web with Emscripten${NC}"

# Check if emscripten is available
if ! command -v emcc &> /dev/null; then
    echo -e "${RED}❌ Emscripten not found. Please install emscripten first.${NC}"
    echo -e "${YELLOW}💡 You can install it with: git clone https://github.com/emscripten-core/emsdk.git && ./emsdk install latest && ./emsdk activate latest${NC}"
    exit 1
fi

# Check if we're in the right directory
if [ ! -f "hextrail.c" ]; then
    echo -e "${RED}❌ hextrail.c not found. Please run this script from the hacks/glx directory.${NC}"
    exit 1
fi

# Create build directory
mkdir -p build_web
cd build_web

echo -e "${YELLOW}📦 Compiling HexTrail...${NC}"

# Compile with emscripten
emcc \
    -DUSE_SDL \
    -DSTANDALONE \
    -DUSE_GL \
    -DHAVE_CONFIG_H \
    -s USE_SDL=3 \
    -s USE_WEBGL2=1 \
    -s FULL_ES3=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap'] \
    -s EXPORTED_FUNCTIONS=['_main','_init_hextrail','_draw_hextrail','_reshape_hextrail','_free_hextrail'] \
    -s MIN_WEBGL_VERSION=2 \
    -O3 \
    -I. \
    -I../../utils \
    -I../../jwxyz \
    -I.. \
    -I../.. \
    ../hextrail_web.c \
    ../../utils/colors.c \
    ../../utils/yarandom.c \
    ../../utils/usleep.c \
    ../../utils/visual-gl.c \
    ../screenhack.c \
    ../xlockmore.c \
    ../fps.c \
    ../rotator.c \
    ../gltrackball.c \
    ../normals.c \
    -o hextrail_web.html \
    --preload-file ../web/index.html@index.html \
    --preload-file ../web/style.css@style.css \
    --preload-file ../web/script.js@script.js

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Build successful!${NC}"
    echo -e "${BLUE}📁 Output files:${NC}"
    echo -e "   - hextrail_web.html (main HTML file)"
    echo -e "   - hextrail_web.js (JavaScript module)"
    echo -e "   - hextrail_web.wasm (WebAssembly binary)"
    
    echo -e "${YELLOW}🌐 To run locally:${NC}"
    echo -e "   python3 -m http.server 8000"
    echo -e "   Then open http://localhost:8000/hextrail_web.html"
    
    # Copy files to web directory for easy access
    cp hextrail_web.* ../web/
    echo -e "${GREEN}📋 Files copied to web/ directory${NC}"
    
else
    echo -e "${RED}❌ Build failed!${NC}"
    exit 1
fi 