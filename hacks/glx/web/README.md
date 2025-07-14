# HexTrail Web Version

A web-based version of the HexTrail screensaver from XScreenSaver, compiled with Emscripten.

## What is HexTrail?

HexTrail is a mesmerizing hexagonal cellular automaton that creates beautiful, organic-looking patterns. It simulates the growth of hexagonal cells that spread across a grid, creating intricate trails and patterns.

## Features

- **Real-time animation** with smooth 60fps rendering
- **Interactive controls** for speed, thickness, spin, and wander
- **Mouse controls** - click and drag to rotate, scroll to zoom
- **Keyboard shortcuts** - F for fullscreen, R for reset, S/W to toggle effects
- **Responsive design** that works on desktop and mobile
- **Fullscreen mode** for immersive viewing

## Prerequisites

To build the web version, you need:

1. **Emscripten SDK** - Download and install from [emscripten.org](https://emscripten.org/docs/getting_started/downloads.html)
2. **SDL3** - The build script will handle this via emscripten
3. **A modern web browser** with WebGL 2.0 support

## Building

1. Navigate to the `hacks/glx` directory:
   ```bash
   cd hacks/glx
   ```

2. Run the build script:
   ```bash
   ./build_web.sh
   ```

3. The build will create:
   - `hextrail_web.html` - Main HTML file
   - `hextrail_web.js` - JavaScript module
   - `hextrail_web.wasm` - WebAssembly binary

## Running

### Local Development Server

1. Start a local web server:
   ```bash
   python3 -m http.server 8000
   ```

2. Open your browser and go to:
   ```
   http://localhost:8000/web/hextrail_web.html
   ```

### Using a Different Server

You can use any web server that supports serving static files. Some options:

- **Node.js**: `npx serve web/`
- **PHP**: `php -S localhost:8000 -t web/`
- **Python**: `python3 -m http.server 8000`

## Controls

### Mouse
- **Click and drag** - Rotate the view
- **Mouse wheel** - Zoom in/out

### Keyboard
- **F** - Toggle fullscreen
- **R** - Reset the animation
- **S** - Toggle spin effect
- **W** - Toggle wander effect
- **Escape** - Exit fullscreen

### UI Controls
- **Speed slider** - Control animation speed (0.1x to 3.0x)
- **Thickness slider** - Control line thickness (0.05 to 0.5)
- **Spin checkbox** - Enable/disable rotation
- **Wander checkbox** - Enable/disable camera movement
- **Reset button** - Restart the animation
- **Fullscreen button** - Enter fullscreen mode

## Technical Details

### Architecture
- **C/C++ core** - Original XScreenSaver code compiled to WebAssembly
- **SDL3** - Cross-platform multimedia library (via emscripten)
- **WebGL 2.0** - Hardware-accelerated graphics
- **JavaScript interface** - Modern web UI with controls

### Performance
- **WebAssembly** provides near-native performance
- **WebGL 2.0** enables hardware acceleration
- **Optimized rendering** with efficient OpenGL calls
- **Memory management** with automatic garbage collection

### Browser Compatibility
- **Chrome/Edge** 67+ (WebGL 2.0 support)
- **Firefox** 51+ (WebGL 2.0 support)
- **Safari** 15+ (WebGL 2.0 support)
- **Mobile browsers** with WebGL 2.0 support

## Troubleshooting

### Build Issues
- **Emscripten not found**: Install emscripten SDK
- **SDL3 errors**: Make sure emscripten is properly configured
- **Compilation errors**: Check that all source files are present

### Runtime Issues
- **Black screen**: Check WebGL 2.0 support in your browser
- **Slow performance**: Try reducing the grid size or disabling effects
- **No controls**: Check browser console for JavaScript errors

### Browser Console Errors
- **WebGL context lost**: Refresh the page
- **Memory errors**: The app will automatically handle memory growth
- **Module loading errors**: Check that all files are served correctly

## Development

### Modifying the C Code
1. Edit `hextrail.c` in the parent directory
2. Rebuild with `./build_web.sh`
3. Refresh the browser

### Modifying the Web Interface
1. Edit files in the `web/` directory
2. Refresh the browser (no rebuild needed for HTML/CSS/JS changes)

### Adding New Features
- **C side**: Add functions and export them in the build script
- **JavaScript side**: Add UI controls and call the exported functions
- **Styling**: Modify `style.css` for visual changes

## License

This project is based on XScreenSaver, which is licensed under the same terms as the original XScreenSaver project.

## Credits

- **Original HexTrail**: Jamie Zawinski (jwz@jwz.org)
- **Web Port**: Compiled with Emscripten
- **Web Interface**: Modern responsive design with WebGL 2.0

## Contributing

Feel free to submit issues and enhancement requests! The web version is designed to be easily extensible and modifiable. 