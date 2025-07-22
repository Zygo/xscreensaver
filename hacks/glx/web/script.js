// HexTrail Web Interface
class HexTrailWeb {
    constructor() {
        this.module = null;
        this.canvas = document.getElementById('canvas');
        this.ctx = this.canvas.getContext('2d');
        this.isRunning = false;
        this.isFullscreen = false;
        
        this.init();
    }
    
    async init() {
        try {
            // Load the emscripten module
            this.module = await this.loadModule();
            
            // Initialize the module
            this.module._main();
            
            // Set up the canvas
            this.setupCanvas();
            
            // Initialize hextrail
            this.module._init_hextrail();
            
            // Start the animation loop
            this.startAnimation();
            
            // Set up controls
            this.setupControls();
            
            // Set up event listeners
            this.setupEventListeners();
            
            // Hide loading screen
            document.getElementById('loading').style.display = 'none';
            
        } catch (error) {
            console.error('Failed to initialize HexTrail:', error);
            document.getElementById('loading').textContent = 'Failed to load HexTrail';
        }
    }
    
    async loadModule() {
        return new Promise((resolve, reject) => {
            // Create a script tag to load the emscripten module
            const script = document.createElement('script');
            script.src = 'hextrail_web.js';
            script.onload = () => {
                // The module should be available globally
                if (typeof Module !== 'undefined') {
                    Module.onRuntimeInitialized = () => {
                        resolve(Module);
                    };
                } else {
                    reject(new Error('Module not found'));
                }
            };
            script.onerror = () => {
                reject(new Error('Failed to load module'));
            };
            document.head.appendChild(script);
        });
    }
    
    setupCanvas() {
        // Set canvas size
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
            
            // Update emscripten canvas size
            if (this.module) {
                this.module._reshape_hextrail(width, height);
            }
        };
        
        resizeCanvas();
        window.addEventListener('resize', resizeCanvas);
    }
    
    startAnimation() {
        if (this.isRunning) return;
        
        this.isRunning = true;
        
        const animate = () => {
            if (!this.isRunning) return;
            
            // Call the draw function
            if (this.module) {
                this.module._draw_hextrail();
            }
            
            requestAnimationFrame(animate);
        };
        
        animate();
    }
    
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