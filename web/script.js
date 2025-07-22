// HexTrail Web Interface
console.log('HexTrailWeb class definition loaded');

class HexTrailWeb {
    constructor() {
        console.log('HexTrailWeb constructor starting...');
        this.module = null;
        this.canvas = document.getElementById('canvas');
        this.ctx = this.canvas.getContext('2d');
        this.isRunning = false;
        this.isFullscreen = false;
        this.debugLog = document.getElementById('debug-log');
        
        this.log('HexTrailWeb constructor called');
        this.init();
    }
    
    log(message) {
        const timestamp = new Date().toLocaleTimeString();
        const logEntry = `[${timestamp}] ${message}`;
        console.log(logEntry);
        
        if (this.debugLog) {
            this.debugLog.innerHTML += logEntry + '<br>';
            this.debugLog.scrollTop = this.debugLog.scrollHeight;
        }
    }
    
    async init() {
        try {
            this.log('Initializing HexTrail Web...');
            
            // Load the emscripten module
            this.module = await this.loadModule();
            this.log('Module loaded successfully');
            this.log('Available functions: ' + Object.keys(this.module).filter(k => k.startsWith('_')).join(', '));
            
            // Set up the canvas first
            this.setupCanvas();
            
            // The main() function should have been called automatically by Emscripten
            // and it should have set up the main loop. We just need to wait for it to be ready.
            this.log('Waiting for Emscripten main loop to be ready...');
            
            // Try calling main() if it hasn't been called yet
            if (this.module._main && !this.module._xscreensaver_web_init) {
                this.log('Calling main() function...');
                this.module._main();
            }
            
            // Set up controls
            this.setupControls();
            
            // Set up event listeners
            this.setupEventListeners();
            
            // Hide loading screen
            document.getElementById('loading').style.display = 'none';
            this.log('HexTrail Web initialized successfully');
            
        } catch (error) {
            this.log('ERROR: Failed to initialize HexTrail: ' + error.message);
            console.error('Failed to initialize HexTrail:', error);
            document.getElementById('loading').textContent = 'Failed to load HexTrail: ' + error.message;
        }
    }
    
    async loadModule() {
        return new Promise((resolve, reject) => {
            this.log('Checking for Module object...');
            // The Module object should already be set up by the HTML
            if (typeof Module !== 'undefined') {
                this.log('Module already configured');
                this.log('Module keys: ' + Object.keys(Module).join(', '));
                resolve(Module);
            } else {
                this.log('ERROR: Module not configured');
                reject(new Error('Module not configured'));
            }
        });
    }
    
    setupCanvas() {
        // Emscripten will create its own canvas and replace our canvas element
        // We just need to set up the initial size and let Emscripten handle the rest
        
        // Set initial canvas size
        this.canvas.width = 800;
        this.canvas.height = 600;
        
        // Handle canvas resize
        const resizeCanvas = () => {
            const container = this.canvas.parentElement;
            const maxWidth = container.clientWidth;
            const maxHeight = window.innerHeight * 0.6;
            
            let width = 800;
            let height = 600;
            
            if (width > maxWidth) {
                const ratio = maxWidth / width;
                width = maxWidth;
                height = height * ratio;
            }
            
            if (height > maxHeight) {
                const ratio = maxHeight / height;
                height = maxHeight;
                width = width * ratio;
            }
            
            this.canvas.style.width = width + 'px';
            this.canvas.style.height = height + 'px';
            
            // Update emscripten canvas size if the module is ready
            if (this.module && this.module._reshape_hextrail) {
                this.module._reshape_hextrail(width, height);
            }
        };
        
        resizeCanvas();
        window.addEventListener('resize', resizeCanvas);
    }
    
    // Animation is handled by Emscripten's main loop, so we don't need this method
    // The main() function sets up emscripten_set_main_loop(main_loop, 60, 1)
    
    setupControls() {
        // Speed control
        const speedSlider = document.getElementById('speed');
        const speedValue = document.getElementById('speed-value');
        
        speedSlider.addEventListener('input', (e) => {
            const value = parseFloat(e.target.value);
            speedValue.textContent = value.toFixed(1);
            
            // Update speed in the module
            if (this.module && this.module._set_speed) {
                this.module._set_speed(value);
            }
        });
        
        // Thickness control
        const thicknessSlider = document.getElementById('thickness');
        const thicknessValue = document.getElementById('thickness-value');
        
        thicknessSlider.addEventListener('input', (e) => {
            const value = parseFloat(e.target.value);
            thicknessValue.textContent = value.toFixed(2);
            
            // Update thickness in the module
            if (this.module && this.module._set_thickness) {
                this.module._set_thickness(value);
            }
        });
        
        // Spin control
        const spinCheckbox = document.getElementById('spin');
        spinCheckbox.addEventListener('change', (e) => {
            if (this.module && this.module._set_spin) {
                this.module._set_spin(e.target.checked ? 1 : 0);
            }
        });
        
        // Wander control
        const wanderCheckbox = document.getElementById('wander');
        wanderCheckbox.addEventListener('change', (e) => {
            if (this.module && this.module._set_wander) {
                this.module._set_wander(e.target.checked ? 1 : 0);
            }
        });
        
        // Reset button
        const resetButton = document.getElementById('reset');
        resetButton.addEventListener('click', () => {
            if (this.module) {
                this.module._init_hextrail();
            }
        });
        
        // Fullscreen button
        const fullscreenButton = document.getElementById('fullscreen');
        fullscreenButton.addEventListener('click', () => {
            this.toggleFullscreen();
        });
    }
    
    setupEventListeners() {
        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            switch (e.key.toLowerCase()) {
                case 'f':
                    this.toggleFullscreen();
                    break;
                case 'r':
                    if (this.module) {
                        this.module._init_hextrail();
                    }
                    break;
                case 's':
                    const spinCheckbox = document.getElementById('spin');
                    spinCheckbox.checked = !spinCheckbox.checked;
                    spinCheckbox.dispatchEvent(new Event('change'));
                    break;
                case 'w':
                    const wanderCheckbox = document.getElementById('wander');
                    wanderCheckbox.checked = !wanderCheckbox.checked;
                    wanderCheckbox.dispatchEvent(new Event('change'));
                    break;
                case 'escape':
                    if (this.isFullscreen) {
                        this.exitFullscreen();
                    }
                    break;
            }
        });
        
        // Mouse controls for canvas
        let isDragging = false;
        let lastX = 0;
        let lastY = 0;
        
        this.canvas.addEventListener('mousedown', (e) => {
            isDragging = true;
            lastX = e.clientX;
            lastY = e.clientY;
            this.canvas.style.cursor = 'grabbing';
        });
        
        this.canvas.addEventListener('mousemove', (e) => {
            if (isDragging && this.module) {
                const deltaX = e.clientX - lastX;
                const deltaY = e.clientY - lastY;
                
                // Convert mouse movement to rotation
                if (this.module._handle_mouse_drag) {
                    this.module._handle_mouse_drag(deltaX, deltaY);
                }
                
                lastX = e.clientX;
                lastY = e.clientY;
            }
        });
        
        this.canvas.addEventListener('mouseup', () => {
            isDragging = false;
            this.canvas.style.cursor = 'grab';
        });
        
        this.canvas.addEventListener('mouseleave', () => {
            isDragging = false;
            this.canvas.style.cursor = 'grab';
        });
        
        // Mouse wheel for zoom
        this.canvas.addEventListener('wheel', (e) => {
            e.preventDefault();
            if (this.module && this.module._handle_mouse_wheel) {
                const delta = e.deltaY > 0 ? -1 : 1;
                this.module._handle_mouse_wheel(delta);
            }
        });
        
        // Set initial cursor
        this.canvas.style.cursor = 'grab';
    }
    
    toggleFullscreen() {
        if (this.isFullscreen) {
            this.exitFullscreen();
        } else {
            this.enterFullscreen();
        }
    }
    
    enterFullscreen() {
        const container = document.getElementById('container');
        container.classList.add('fullscreen');
        this.isFullscreen = true;
        
        // Resize canvas to fullscreen
        this.canvas.width = window.innerWidth;
        this.canvas.height = window.innerHeight;
        this.canvas.style.width = '100vw';
        this.canvas.style.height = '100vh';
        
        if (this.module) {
            this.module._reshape_hextrail(window.innerWidth, window.innerHeight);
        }
    }
    
    exitFullscreen() {
        const container = document.getElementById('container');
        container.classList.remove('fullscreen');
        this.isFullscreen = false;
        
        // Reset canvas size
        this.setupCanvas();
    }
    
    destroy() {
        this.isRunning = false;
        
        if (this.module) {
            this.module._free_hextrail();
        }
    }
}

// Initialize the application when the page loads
document.addEventListener('DOMContentLoaded', () => {
    window.hextrailApp = new HexTrailWeb();
});

// Clean up when the page unloads
window.addEventListener('beforeunload', () => {
    if (window.hextrailApp) {
        window.hextrailApp.destroy();
    }
}); 