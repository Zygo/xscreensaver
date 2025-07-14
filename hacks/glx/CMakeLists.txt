cmake_minimum_required(VERSION 3.16)
project(hextrail_web)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Emscripten-specific settings
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_SDL -s USE_SDL=3 -s USE_WEBGL2=1 -s FULL_ES3=1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s EXPORTED_FUNCTIONS=['_main','_init_hextrail','_draw_hextrail','_reshape_hextrail']")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_WEBGL2=1 -s FULL_ES3=1 -s MIN_WEBGL_VERSION=2")
    
    # Enable optimizations for web
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g -DDEBUG")
endif()

# Find SDL3
find_package(SDL3 REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../../utils
    ${CMAKE_SOURCE_DIR}/../../jwxyz
    ${CMAKE_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/../..
    ${SDL3_INCLUDE_DIRS}
)

# Source files
set(HEXTRAIL_SOURCES
    hextrail.c
    ../../utils/colors.c
    ../../utils/yarandom.c
    ../../utils/usleep.c
    ../../utils/visual-gl.c
    ../screenhack.c
    ../xlockmore.c
    ../fps.c
    ../rotator.c
    ../gltrackball.c
    ../normals.c
)

# Create executable
add_executable(hextrail_web ${HEXTRAIL_SOURCES})

# Link libraries
target_link_libraries(hextrail_web ${SDL3_LIBRARIES})

# Emscripten-specific linking
if(EMSCRIPTEN)
    target_link_options(hextrail_web PRIVATE
        -s USE_SDL=3
        -s USE_WEBGL2=1
        -s FULL_ES3=1
        -s ALLOW_MEMORY_GROWTH=1
        -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']
        -s EXPORTED_FUNCTIONS=['_main','_init_hextrail','_draw_hextrail','_reshape_hextrail']
        -s MIN_WEBGL_VERSION=2
        --preload-file ${CMAKE_SOURCE_DIR}/web/index.html@index.html
        --preload-file ${CMAKE_SOURCE_DIR}/web/style.css@style.css
        --preload-file ${CMAKE_SOURCE_DIR}/web/script.js@script.js
    )
endif() 