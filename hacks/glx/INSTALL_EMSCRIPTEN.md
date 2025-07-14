# Installing Emscripten for HexTrail Web Build

## Quick Installation

### Option 1: Using the Emscripten SDK (Recommended)

1. **Clone the Emscripten SDK:**
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ```

2. **Install the latest version:**
   ```bash
   ./emsdk install latest
   ```

3. **Activate the latest version:**
   ```bash
   ./emsdk activate latest
   ```

4. **Set up environment variables:**
   ```bash
   source ./emsdk_env.sh
   ```

5. **Verify installation:**
   ```bash
   emcc --version
   ```

### Option 2: Using Package Manager (Ubuntu/Debian)

```bash
# Add the emscripten repository
wget -qO- https://deb.nodesource.com/setup_lts.x | sudo -E bash -
sudo apt-get install -y nodejs

# Install emscripten
sudo apt-get install -y emscripten
```

### Option 3: Using Docker

```bash
# Pull the emscripten Docker image
docker pull emscripten/emsdk

# Run a container with the current directory mounted
docker run --rm -v $(pwd):/src -w /src emscripten/emsdk emcc --version
```

## Building HexTrail for Web

Once emscripten is installed:

1. **Navigate to the glx directory:**
   ```bash
   cd hacks/glx
   ```

2. **Make the build script executable:**
   ```bash
   chmod +x build_web.sh
   ```

3. **Run the build:**
   ```bash
   ./build_web.sh
   ```

4. **Start a local server:**
   ```bash
   python3 -m http.server 8000
   ```

5. **Open in browser:**
   ```
   http://localhost:8000/web/hextrail_web.html
   ```

## Troubleshooting

### Common Issues

1. **"emcc: command not found"**
   - Make sure you've sourced the emsdk environment: `source ./emsdk_env.sh`
   - Or add emscripten to your PATH permanently

2. **SDL3 not found**
   - Emscripten should handle SDL3 automatically
   - Make sure you're using the latest emscripten version

3. **Compilation errors**
   - Check that all source files exist
   - Make sure you're in the correct directory
   - Try building with verbose output: `emcc -v ...`

4. **WebGL errors in browser**
   - Make sure your browser supports WebGL 2.0
   - Try a different browser (Chrome/Edge recommended)
   - Check browser console for specific error messages

### Environment Setup

Add to your `~/.bashrc` or `~/.zshrc`:

```bash
# Emscripten environment
export EMSDK="$HOME/emsdk"
export PATH="$EMSDK:$PATH"
export PATH="$EMSDK/upstream/emscripten:$PATH"
```

Then reload your shell:
```bash
source ~/.bashrc
```

### Alternative Build Commands

If the build script doesn't work, you can try building manually:

```bash
emcc \
  -DUSE_SDL \
  -DSTANDALONE \
  -DUSE_GL \
  -s USE_SDL=3 \
  -s USE_WEBGL2=1 \
  -s FULL_ES3=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -O3 \
  -I. -I../../utils -I.. -I../.. \
  hextrail_web.c \
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
  -o hextrail_web.html
```

## System Requirements

- **Linux/macOS/Windows** (WSL recommended for Windows)
- **Node.js** 14+ (for emscripten)
- **Python** 3.6+ (for emscripten)
- **Git** (for cloning emsdk)
- **Modern web browser** with WebGL 2.0 support

## Performance Tips

1. **Optimization levels:**
   - `-O0` for debugging
   - `-O2` for development
   - `-O3` for production (default)

2. **Memory settings:**
   - `-s ALLOW_MEMORY_GROWTH=1` allows dynamic memory allocation
   - `-s INITIAL_MEMORY=16MB` sets initial memory size

3. **WebGL settings:**
   - `-s USE_WEBGL2=1` enables WebGL 2.0
   - `-s FULL_ES3=1` enables full OpenGL ES 3.0 features

## Next Steps

After successful installation and build:

1. **Test the web version** in different browsers
2. **Modify the UI** by editing files in the `web/` directory
3. **Add new features** by modifying `hextrail_web.c`
4. **Deploy to a web server** for public access

## Support

If you encounter issues:

1. Check the [Emscripten documentation](https://emscripten.org/docs/)
2. Look at the [Emscripten GitHub issues](https://github.com/emscripten-core/emscripten/issues)
3. Check browser console for specific error messages
4. Try building with verbose output for more details 